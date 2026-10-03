#!/usr/bin/env python3
"""使用固定版本的 Horizon 官方工具验证生成的 Pool。"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import sqlite3
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any


REQUIRED_INDEX_TYPES = {"entity", "package", "padstack", "part", "symbol", "unit"}


def _run(command: list[str], cwd: Path) -> subprocess.CompletedProcess[str]:
    """运行验证步骤并保留官方工具的标准输出。"""
    return subprocess.run(command, cwd=cwd, text=True, capture_output=True, check=False)


def _git_snapshot(pool: Path) -> None:
    """为官方审查工具准备隔离的、不会污染源 Pool 的 Git 快照。"""
    for command in (
        ["git", "init", "--quiet"],
        ["git", "add", "."],
        [
            "git",
            "-c",
            "user.name=EasyKiConverter validation",
            "-c",
            "user.email=validation@invalid",
            "commit",
            "--quiet",
            "-m",
            "validation snapshot",
        ],
    ):
        result = _run(command, pool)
        if result.returncode != 0:
            detail = (result.stderr or result.stdout).strip()
            raise RuntimeError(f"Git 快照失败：{' '.join(command)}：{detail}")


def _index_types(database: Path) -> tuple[set[str], dict[str, tuple[str, str]]]:
    """读取官方索引中的对象类型及 Part 的实体/封装引用。"""
    with sqlite3.connect(database) as connection:
        rows = connection.execute("SELECT type, uuid FROM all_items_view").fetchall()
        parts = {
            uuid: (entity, package)
            for uuid, entity, package in connection.execute("SELECT uuid, entity, package FROM parts")
        }
    return {str(item[0]) for item in rows}, parts


def validate_index(pool: Path) -> dict[str, Any]:
    """验证 pool.db 的最低对象集合和 Part 外键完整性。"""
    database = pool / "pool.db"
    if not database.is_file() or database.stat().st_size == 0:
        raise RuntimeError(f"官方 PoolUpdater 未生成有效 pool.db：{database}")
    try:
        types, parts = _index_types(database)
        indexed_uuids = {uuid for _, uuid in _all_index_rows(database)}
    except sqlite3.Error as exc:
        raise RuntimeError(f"无法读取官方 pool.db：{exc}") from exc
    missing_types = sorted(REQUIRED_INDEX_TYPES - types)
    if missing_types:
        raise RuntimeError(f"pool.db 缺少对象索引：{', '.join(missing_types)}")
    missing_references = {
        part_uuid: [reference for reference in references if reference not in indexed_uuids]
        for part_uuid, references in parts.items()
        if any(reference not in indexed_uuids for reference in references)
    }
    if missing_references:
        raise RuntimeError(f"Part 引用不存在的 Entity/Package：{json.dumps(missing_references, ensure_ascii=False)}")
    return {"index_types": sorted(types), "parts": len(parts)}


def _all_index_rows(database: Path) -> list[tuple[str, str]]:
    """返回官方索引中的类型和 UUID，用于引用校验。"""
    with sqlite3.connect(database) as connection:
        return [(str(item_type), str(uuid)) for item_type, uuid in connection.execute("SELECT type, uuid FROM all_items_view")]


def _read_json_object(path: Path) -> dict[str, Any]:
    """读取一个 Pool source JSON 对象，并将文件错误定位到具体路径。"""
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise RuntimeError(f"Pool source JSON 无法读取：{path}：{exc}") from exc
    if not isinstance(value, dict):
        raise RuntimeError(f"Pool source JSON 不是对象：{path}")
    return value


def _source_objects(directory: Path, *, package_files: bool = False) -> dict[str, tuple[Path, dict[str, Any]]]:
    """读取一个 Pool 对象目录，按 UUID 建立可诊断索引。"""
    if not directory.is_dir():
        raise RuntimeError(f"Pool 缺少对象目录：{directory}")
    paths = directory.rglob("package.json") if package_files else directory.glob("*.json")
    objects: dict[str, tuple[Path, dict[str, Any]]] = {}
    for path in sorted(paths):
        value = _read_json_object(path)
        uuid = value.get("uuid")
        if not isinstance(uuid, str) or not uuid:
            raise RuntimeError(f"Pool source 缺少有效 uuid：{path}")
        if uuid in objects:
            raise RuntimeError(f"Pool source 存在重复 uuid {uuid}：{path} 与 {objects[uuid][0]}")
        objects[uuid] = (path, value)
    return objects


def _pool_relative_file(pool: Path, filename: Any, context: Path) -> Path:
    """解析 Pool 内部文件引用，拒绝绝对路径和目录穿越。"""
    if not isinstance(filename, str) or not filename:
        raise RuntimeError(f"3D 模型引用缺少 filename：{context}")
    candidate = (pool / filename).resolve()
    try:
        candidate.relative_to(pool.resolve())
    except ValueError as exc:
        raise RuntimeError(f"Pool 文件引用越过根目录：{context} -> {filename}") from exc
    return candidate


def validate_source_references(pool: Path) -> dict[str, int]:
    """校验 source JSON 的跨对象引用和 3D 文件引用。"""
    units = _source_objects(pool / "units")
    symbols = _source_objects(pool / "symbols")
    entities = _source_objects(pool / "entities")
    padstacks = _source_objects(pool / "padstacks")
    packages = _source_objects(pool / "packages", package_files=True)
    parts = _source_objects(pool / "parts")

    for path, symbol in symbols.values():
        unit_uuid = symbol.get("unit")
        if unit_uuid not in units:
            raise RuntimeError(f"Symbol 引用不存在的 Unit：{path} -> {unit_uuid}")

    for path, entity in entities.values():
        for gate_uuid, gate in entity.get("gates", {}).items():
            if not isinstance(gate, dict) or gate.get("unit") not in units:
                raise RuntimeError(f"Entity Gate 引用不存在的 Unit：{path} -> {gate_uuid}")

    for path, package in packages.values():
        pads = package.get("pads", {})
        if not isinstance(pads, dict):
            raise RuntimeError(f"Package pads 不是对象：{path}")
        models = package.get("models", {})
        if not isinstance(models, dict):
            raise RuntimeError(f"Package models 不是对象：{path}")
        default_model = package.get("default_model")
        if default_model != "00000000-0000-0000-0000-000000000000" and default_model not in models:
            raise RuntimeError(f"Package default_model 引用不存在的模型：{path} -> {default_model}")
        for pad_uuid, pad in pads.items():
            if not isinstance(pad, dict) or pad.get("padstack") not in padstacks:
                raise RuntimeError(f"Package Pad 引用不存在的 Padstack：{path} -> {pad_uuid}")
        for model_uuid, model in models.items():
            if not isinstance(model, dict):
                raise RuntimeError(f"Package 模型对象无效：{path} -> {model_uuid}")
            model_path = _pool_relative_file(pool, model.get("filename"), path)
            if not model_path.is_file():
                raise RuntimeError(f"Package 模型文件不存在：{path} -> {model_path}")

    for path, part in parts.values():
        entity_uuid = part.get("entity")
        package_uuid = part.get("package")
        if entity_uuid not in entities:
            raise RuntimeError(f"Part 引用不存在的 Entity：{path} -> {entity_uuid}")
        if package_uuid not in packages:
            raise RuntimeError(f"Part 引用不存在的 Package：{path} -> {package_uuid}")
        entity = entities[entity_uuid][1]
        package = packages[package_uuid][1]
        unit_by_gate = {
            gate_uuid: gate.get("unit")
            for gate_uuid, gate in entity.get("gates", {}).items()
            if isinstance(gate, dict)
        }
        for pad_uuid, mapping in part.get("pad_map", {}).items():
            if pad_uuid not in package.get("pads", {}):
                raise RuntimeError(f"Part pad_map 引用不存在的 Package Pad：{path} -> {pad_uuid}")
            if not isinstance(mapping, dict) or mapping.get("gate") not in unit_by_gate:
                raise RuntimeError(f"Part pad_map 引用不存在的 Gate：{path} -> {pad_uuid}")
            unit_uuid = unit_by_gate[mapping["gate"]]
            if unit_uuid not in units or mapping.get("pin") not in units[unit_uuid][1].get("pins", {}):
                raise RuntimeError(f"Part pad_map 引用不存在的 Unit Pin：{path} -> {pad_uuid}")

    return {
        "units": len(units),
        "symbols": len(symbols),
        "entities": len(entities),
        "packages": len(packages),
        "padstacks": len(padstacks),
        "parts": len(parts),
    }


def _validate_with_python(staged_pool: Path, horizon_python: Path, temporary: Path) -> None:
    """通过固定版本的官方 Python binding 更新并注册隔离 Pool。"""
    script = (
        "import os, sys\n"
        "import horizon\n"
        "pool = os.path.abspath(sys.argv[1])\n"
        "horizon.Pool.update(pool)\n"
        "database = os.path.join(pool, 'pool.db')\n"
        "if not os.path.isfile(database) or os.path.getsize(database) == 0:\n"
        "    raise RuntimeError('Pool.update did not create a usable pool.db')\n"
        "horizon.PoolManager.add_pool(pool)\n"
        "registered = horizon.PoolManager.get_pools()\n"
        "if pool not in {os.path.abspath(path) for path in registered}:\n"
        "    raise RuntimeError('PoolManager.add_pool did not register the Pool')\n"
    )
    environment = os.environ.copy()
    environment["XDG_CONFIG_HOME"] = str(temporary / "config")
    configured_module_path = environment.get("EASYKICONVERTER_HORIZON_PYTHONPATH")
    module_path = str(Path(configured_module_path).expanduser().resolve()) if configured_module_path else str(horizon_python.parent)
    current_python_path = environment.get("PYTHONPATH")
    environment["PYTHONPATH"] = (
        module_path if not current_python_path else module_path + os.pathsep + current_python_path
    )
    result = subprocess.run(
        [str(horizon_python), "-c", script, str(staged_pool)],
        cwd=staged_pool.parent,
        env=environment,
        text=True,
        capture_output=True,
        check=False,
    )
    if result.returncode != 0:
        detail = (result.stderr or result.stdout).strip()
        raise RuntimeError(f"Horizon Python binding 失败（退出码 {result.returncode}）：{detail}")


def validate_pool(
    pool: Path, horizon_review: Path | None = None, horizon_python: Path | None = None
) -> dict[str, Any]:
    """在隔离副本中运行官方 PoolUpdater 并验证索引。"""
    pool = pool.expanduser().resolve()
    if horizon_review is not None:
        horizon_review = horizon_review.expanduser().resolve()
    if horizon_python is not None:
        horizon_python = horizon_python.expanduser().resolve()
    pool_info = pool / "pool.json"
    if not pool.is_dir():
        raise RuntimeError(f"Pool 目录不存在：{pool}")
    if not pool_info.is_file():
        raise RuntimeError(f"Pool 缺少 pool.json：{pool_info}")
    try:
        json.loads(pool_info.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise RuntimeError(f"pool.json 无法读取：{exc}") from exc
    if horizon_review is not None and (not horizon_review.is_file() or not os.access(horizon_review, os.X_OK)):
        raise RuntimeError(f"Horizon 官方验证工具不可执行：{horizon_review}")
    if horizon_python is not None and (not horizon_python.is_file() or not os.access(horizon_python, os.X_OK)):
        raise RuntimeError(f"Horizon Python 解释器不可执行：{horizon_python}")
    if horizon_review is None and horizon_python is None:
        raise RuntimeError("必须提供 --horizon-review 或 --horizon-python")

    with tempfile.TemporaryDirectory(prefix=".horizon-review-", dir=pool.parent) as temporary:
        staged_pool = Path(temporary) / pool.name
        shutil.copytree(pool, staged_pool)
        if horizon_python is not None:
            _validate_with_python(staged_pool, horizon_python, Path(temporary))
        else:
            _git_snapshot(staged_pool)
            report = Path(temporary) / "review.md"
            images = Path(temporary) / "images"
            result = _run(
                [str(horizon_review), "--pool-update", "-o", str(report), "-i", str(images), str(staged_pool)],
                pool.parent,
            )
            if result.returncode != 0:
                detail = (result.stderr or result.stdout).strip()
                raise RuntimeError(f"Horizon 官方工具失败（退出码 {result.returncode}）：{detail}")
        source_summary = validate_source_references(staged_pool)
        summary = {**validate_index(staged_pool), "source_objects": source_summary}
    return summary


def main() -> int:
    """解析参数并输出机器可读的验证摘要。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pool", type=Path, required=True, help="EasyKiConverter 生成的 Pool 目录")
    tools = parser.add_mutually_exclusive_group(required=True)
    tools.add_argument("--horizon-review", type=Path, help="固定版本的 horizon-pr-review 可执行文件")
    tools.add_argument("--horizon-python", type=Path, help="固定版本的官方 Python 解释器")
    arguments = parser.parse_args()
    try:
        summary = validate_pool(arguments.pool, arguments.horizon_review, arguments.horizon_python)
    except (OSError, RuntimeError) as exc:
        print(json.dumps({"ok": False, "error": str(exc)}, ensure_ascii=False))
        return 1
    print(json.dumps({"ok": True, **summary}, ensure_ascii=False, sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main())
