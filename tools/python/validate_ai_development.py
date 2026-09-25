#!/usr/bin/env python3
"""校验 AI 协作文档、Skill 元数据、能力台账和 fixture provenance。"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path
from typing import Any

from verification_plan import validate_policy


SKILLS = ("development", "testing", "code-review", "documentation")
AI_ROOT = Path("docs/developer/ai-development")
PAIR_RE = re.compile(r"\[[^\]]+\]\(([^)]+)\)")
MUTABLE_RULE_LINK_RE = re.compile(r"github\.com/[^/]+/[^/]+/blob/(?:master|main)/(?:AGENTS|CLAUDE|PROJECT_INSTRUCTIONS)\.md")


def error(path: Path, message: str, line: int | None = None) -> str:
    """生成包含文件和行号的诊断。"""
    location = f"{path}:{line}" if line else str(path)
    return f"{location}: {message}"


def load_json(path: Path, errors: list[str]) -> Any | None:
    """读取 JSON，并将语法错误转换为定位明确的诊断。"""
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        errors.append(error(path, "文件不存在"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        line = getattr(exc, "lineno", None)
        errors.append(error(path, f"JSON 无法读取：{exc}", line))
    return None


def validate_bilingual_pair(repo: Path, chinese: Path, english: Path) -> list[str]:
    """检查中英文文件存在，并在正文前部互相链接。"""
    errors: list[str] = []
    if not chinese.exists():
        errors.append(error(chinese, "中文文档不存在"))
        return errors
    if not english.exists():
        errors.append(error(english, "英文对应文档不存在"))
        return errors
    chinese_text = chinese.read_text(encoding="utf-8")
    english_text = english.read_text(encoding="utf-8")
    if english.name not in chinese_text:
        errors.append(error(chinese, f"未链接英文对应文档 {english.name}"))
    if chinese.name not in english_text:
        errors.append(error(english, f"未链接中文对应文档 {chinese.name}"))
    return errors


def validate_local_links(repo: Path, document: Path) -> list[str]:
    """检查 Markdown 中的相对文件链接，不访问网络 URL。"""
    errors: list[str] = []
    lines = document.read_text(encoding="utf-8").splitlines()
    for line_number, line in enumerate(lines, 1):
        for target in PAIR_RE.findall(line):
            target = target.split("#", 1)[0].strip()
            if not target or target.startswith(("http://", "https://", "mailto:", "#")):
                continue
            candidate = (document.parent / target).resolve()
            try:
                candidate.relative_to(repo.resolve())
            except ValueError:
                errors.append(error(document, f"相对链接越出仓库：{target}", line_number))
                continue
            if not candidate.exists():
                errors.append(error(document, f"链接目标不存在：{target}", line_number))
    return errors


def validate_skill_metadata(repo: Path, skill: str) -> list[str]:
    """验证一个 Skill 的机器可读元数据及其必读资料。"""
    errors: list[str] = []
    skill_dir = repo / AI_ROOT / "skills" / skill
    metadata_path = skill_dir / "SKILL.meta.json"
    data = load_json(metadata_path, errors)
    if not isinstance(data, dict):
        return errors
    schema_path = repo / AI_ROOT / "schemas/skill-metadata.schema.json"
    schema = load_json(schema_path, errors)
    if isinstance(schema, dict):
        errors.extend(validate_json_schema(data, schema, metadata_path))
    required = {"id", "purpose", "triggers", "required_sources", "side_effects", "verification", "stop_conditions", "evidence_outputs"}
    missing = sorted(required - data.keys())
    if missing:
        errors.append(error(metadata_path, f"缺少字段：{', '.join(missing)}"))
    if data.get("id") != skill:
        errors.append(error(metadata_path, f"id 必须为 {skill}"))
    for field in required - {"id", "purpose"}:
        if not isinstance(data.get(field), list) or (field != "side_effects" and not data[field]):
            errors.append(error(metadata_path, f"字段 {field} 必须是数组且不能为空（side_effects 可以为空）"))
    for source in data.get("required_sources", []):
        source_path = repo / source
        if not source_path.exists():
            errors.append(error(metadata_path, f"必读资料不存在：{source}"))
    if not (skill_dir / "SKILL.md").exists():
        errors.append(error(skill_dir / "SKILL.md", "Skill 正文不存在"))
    return errors


def validate_json_schema(value: Any, schema: dict[str, Any], path: Path, location: str = "$",) -> list[str]:
    """校验 Skill 元数据使用的 JSON Schema 子集，并保留字段定位。"""
    errors: list[str] = []
    expected_type = schema.get("type")
    type_matches = {
        "object": isinstance(value, dict),
        "array": isinstance(value, list),
        "string": isinstance(value, str),
    }
    if expected_type in type_matches and not type_matches[expected_type]:
        return [error(path, f"{location} 类型错误，期望 {expected_type}")]
    if isinstance(value, dict):
        for required in schema.get("required", []):
            if required not in value:
                errors.append(error(path, f"{location}.{required} 缺少必填字段"))
        properties = schema.get("properties", {})
        if schema.get("additionalProperties") is False:
            for key in value:
                if key not in properties:
                    errors.append(error(path, f"{location}.{key} 是未声明字段"))
        for key, child_schema in properties.items():
            if key in value and isinstance(child_schema, dict):
                errors.extend(validate_json_schema(value[key], child_schema, path, f"{location}.{key}"))
    if isinstance(value, list):
        minimum = schema.get("minItems")
        if isinstance(minimum, int) and len(value) < minimum:
            errors.append(error(path, f"{location} 至少需要 {minimum} 项"))
        item_schema = schema.get("items")
        if isinstance(item_schema, dict):
            for index, item in enumerate(value):
                errors.extend(validate_json_schema(item, item_schema, path, f"{location}[{index}]"))
    if isinstance(value, str):
        minimum = schema.get("minLength")
        if isinstance(minimum, int) and len(value) < minimum:
            errors.append(error(path, f"{location} 不能为空"))
        pattern = schema.get("pattern")
        if isinstance(pattern, str) and re.fullmatch(pattern, value) is None:
            errors.append(error(path, f"{location} 不符合模式 {pattern}"))
    return errors


def validate_workflow_manifest(repo: Path) -> list[str]:
    """确保工作流依赖文档只引用真实工作流并覆盖当前工作流。"""
    path = repo / ".github/workflows/WORKFLOW_DEPENDENCIES.md"
    errors: list[str] = []
    if not path.exists():
        return [error(path, "工作流依赖文档不存在")]
    text = path.read_text(encoding="utf-8")
    listed = set(re.findall(r"`([^`]+\.ya?ml)`", text))
    actual = {item.name for item in (repo / ".github/workflows").glob("*.y*ml")}
    for name in sorted(listed - actual):
        errors.append(error(path, f"引用了不存在的 workflow：{name}"))
    for name in sorted(actual - listed):
        errors.append(error(path, f"未记录实际 workflow：{name}"))
    for action in sorted(set(re.findall(r"`(\.github/actions/[^`]+)`", text))):
        if "*" in action:
            continue
        if not (repo / action).exists():
            errors.append(error(path, f"引用了不存在的 composite action：{action}"))
    errors.extend(validate_workflow_trigger_table(repo, path, text))
    return errors


def workflow_events(workflow: Path) -> set[str]:
    """从工作流的顶层 on 区块读取事件，避免依赖额外 YAML 库。"""
    events: set[str] = set()
    in_on = False
    for line in workflow.read_text(encoding="utf-8").splitlines():
        if line == "on:" or line.startswith("on: "):
            in_on = True
            continue
        if in_on and line and not line.startswith((" ", "\t", "#")):
            break
        if in_on:
            match = re.match(r"^  (push|pull_request|schedule|workflow_dispatch|workflow_call):", line)
            if match:
                events.add(match.group(1))
            if re.match(r"^    tags(?:-ignore)?:", line):
                events.add("tag")
    return events


def validate_workflow_trigger_table(repo: Path, document: Path, text: str) -> list[str]:
    """对照真实 workflow 的 on 事件检查依赖文档触发器表。"""
    errors: list[str] = []
    rows: dict[str, tuple[list[str], int]] = {}
    lines = text.splitlines()
    row_re = re.compile(r"^\|\s*([^|]+?)\s*\|\s*([✓✗])\s*\|\s*([✓✗])\s*\|\s*([✓✗])\s*\|\s*([✓✗])\s*\|\s*([✓✗])\s*\|")
    for line_number, line in enumerate(lines, 1):
        match = row_re.match(line)
        if match:
            rows[match.group(1).strip()] = ([match.group(index) for index in range(2, 7)], line_number)
    columns = ("push", "pull_request", "tag", "schedule", "workflow_dispatch")
    workflows = repo / ".github/workflows"
    for workflow in sorted(workflows.glob("*.y*ml")):
        name = workflow.name
        if name not in rows:
            errors.append(error(document, f"触发器表缺少 workflow：{name}"))
            continue
        values, line_number = rows[name]
        events = workflow_events(workflow)
        expected = {
            "push": "push" in events,
            "pull_request": "pull_request" in events,
            "tag": "tag" in events,
            "schedule": "schedule" in events,
            "workflow_dispatch": "workflow_dispatch" in events,
        }
        for column, value in zip(columns, values):
            actual = value == "✓"
            if actual != expected[column]:
                errors.append(error(document, f"{name} 的 {column} 触发器与 workflow 不一致", line_number))
    return errors


def validate_verification_policy(repo: Path) -> list[str]:
    """检查验证策略中的命令引用和策略文件自身。"""
    policy_path = repo / AI_ROOT / "verification-policy.json"
    errors: list[str] = []
    data = load_json(policy_path, errors)
    if not isinstance(data, dict):
        return errors
    if data.get("source_of_truth") != str(AI_ROOT / "verification-policy.json"):
        errors.append(error(policy_path, "source_of_truth 必须指向当前验证策略"))
    commands = data.get("commands")
    if not isinstance(commands, dict) or not commands:
        errors.append(error(policy_path, "commands 必须是非空对象"))
        return errors
    for command_name, command in commands.items():
        if not isinstance(command, str) or not command.strip():
            errors.append(error(policy_path, f"命令 {command_name} 不能为空"))
            continue
        for relative in re.findall(r"(?:tools|tests)/[A-Za-z0-9_./-]+", command):
            if not (repo / relative).exists():
                errors.append(error(policy_path, f"命令 {command_name} 引用不存在路径：{relative}"))
    for message in validate_policy(data):
        errors.append(error(policy_path, message))
    return errors


def validate_fixture_manifest(repo: Path) -> list[str]:
    """校验 fixture 路径、哈希、测试引用和未知来源标记。"""
    manifest_path = repo / AI_ROOT / "fixture-provenance.json"
    errors: list[str] = []
    data = load_json(manifest_path, errors)
    if not isinstance(data, dict) or not isinstance(data.get("fixtures"), list):
        errors.append(error(manifest_path, "fixtures 必须是数组"))
        return errors
    entries = {item.get("path"): item for item in data["fixtures"] if isinstance(item, dict)}
    fixture_root = repo / "tests/fixtures"
    actual = {str(item.relative_to(repo)) for item in fixture_root.rglob("*") if item.is_file() and item.name not in {"README.md", ".gitkeep"}}
    for relative in sorted(actual - entries.keys()):
        errors.append(error(manifest_path, f"fixture 未登记：{relative}"))
    for relative, item in entries.items():
        fixture = repo / relative
        if not fixture.is_file() and item.get("availability") == "optional-local":
            continue
        if not fixture.is_file():
            errors.append(error(manifest_path, f"登记的 fixture 不存在：{relative}"))
            continue
        if item.get("availability") != "optional-local":
            actual_hash = hashlib.sha256(fixture.read_bytes()).hexdigest()
            if item.get("sha256") != actual_hash:
                errors.append(error(manifest_path, f"fixture SHA-256 不匹配：{relative}"))
        for field in ("source", "acquisition", "software_version", "license", "distribution_status"):
            if not item.get(field):
                errors.append(error(manifest_path, f"fixture {relative} 缺少字段：{field}"))
        for test in item.get("tests", []):
            if not (repo / test).exists():
                errors.append(error(manifest_path, f"fixture 测试不存在：{test}"))
    return errors


def validate_capability_ledger(repo: Path) -> list[str]:
    """检查 EDA 能力台账引用的代码、测试和证据文件。"""
    ledger_path = repo / AI_ROOT / "eda-capabilities.json"
    errors: list[str] = []
    data = load_json(ledger_path, errors)
    if not isinstance(data, dict) or not isinstance(data.get("formats"), list):
        errors.append(error(ledger_path, "formats 必须是数组"))
        return errors
    dimensions = data.get("status_dimensions")
    required_dimensions = {
        "implementation",
        "automated_testing",
        "structural_validation",
        "commercial_eda_validation",
    }
    if not isinstance(dimensions, dict):
        errors.append(error(ledger_path, "status_dimensions 必须是对象"))
    else:
        missing_dimensions = sorted(required_dimensions - dimensions.keys())
        if missing_dimensions:
            errors.append(error(ledger_path, f"status_dimensions 缺少字段：{', '.join(missing_dimensions)}"))
        for dimension, description in dimensions.items():
            if not isinstance(description, dict) or not isinstance(description.get("field"), str):
                errors.append(error(ledger_path, f"status_dimensions.{dimension} 必须声明 field"))
    statuses = set(data.get("status_vocabulary", []))
    for format_entry in data["formats"]:
        for artifact, details in format_entry.get("artifacts", {}).items():
            if not isinstance(details, dict):
                errors.append(error(ledger_path, f"{format_entry.get('id')}/{artifact} 必须是对象"))
                continue
            if "code_entry" not in details:
                errors.append(error(ledger_path, f"缺少实现入口字段：{format_entry.get('id')}/{artifact}"))
            if "automated_tests" not in details:
                errors.append(error(ledger_path, f"缺少自动测试字段：{format_entry.get('id')}/{artifact}"))
            code_entry = details.get("code_entry")
            if code_entry is not None and not isinstance(code_entry, str):
                errors.append(error(ledger_path, f"实现入口必须是字符串或 null：{format_entry.get('id')}/{artifact}"))
            if code_entry and not (repo / code_entry).exists():
                errors.append(error(ledger_path, f"代码入口不存在：{code_entry}"))
            automated_tests = details.get("automated_tests")
            if not isinstance(automated_tests, list):
                errors.append(error(ledger_path, f"自动测试必须是数组：{format_entry.get('id')}/{artifact}"))
                automated_tests = []
            for test in automated_tests:
                if not (repo / test).exists():
                    errors.append(error(ledger_path, f"测试证据不存在：{test}"))
            for field in ("structural_validation", "commercial_eda_validation"):
                value = details.get(field)
                if value not in statuses and value not in {"unknown", "not-applicable"}:
                    errors.append(error(ledger_path, f"未知验证状态 {value!r}：{format_entry.get('id')}/{artifact}"))
        for evidence in format_entry.get("evidence", []):
            if not (repo / evidence).exists():
                errors.append(error(ledger_path, f"证据文件不存在：{evidence}"))
    return errors


def validate_repo(repo: Path) -> list[str]:
    """执行全部仓库级 AI 控制面检查。"""
    errors: list[str] = []
    policy = repo / AI_ROOT / "policy/AI_POLICY.md"
    policy_en = repo / AI_ROOT / "policy/AI_POLICY_en.md"
    errors.extend(validate_bilingual_pair(repo, policy, policy_en))
    for document in (repo / AI_ROOT).rglob("*.md"):
        errors.extend(validate_local_links(repo, document))
        for line_number, line in enumerate(document.read_text(encoding="utf-8").splitlines(), 1):
            if MUTABLE_RULE_LINK_RE.search(line):
                errors.append(error(document, "AI 文档不得链接到可变分支中的规则文件", line_number))
    for skill in SKILLS:
        errors.extend(validate_skill_metadata(repo, skill))
    errors.extend(validate_verification_policy(repo))
    errors.extend(validate_workflow_manifest(repo))
    errors.extend(validate_fixture_manifest(repo))
    errors.extend(validate_capability_ledger(repo))
    return errors


def main() -> int:
    """打印全部诊断并返回适用于 CI 的状态码。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repository", type=Path, default=Path(__file__).resolve().parents[2])
    args = parser.parse_args()
    errors = validate_repo(args.repository.resolve())
    if errors:
        print("AI development consistency check failed:", file=sys.stderr)
        print("\n".join(f"- {item}" for item in errors), file=sys.stderr)
        return 1
    print("AI development consistency check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
