#!/usr/bin/env python3
"""根据 PR 的完整变更范围选择需要执行的 CI 检查。"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


DOC_SUFFIXES = {".md", ".mdx", ".rst"}
DOCUMENT_CONFIG_FILES = {"mkdocs.yml", "mkdocs.yaml"}
STATIC_SUFFIXES = {".gif", ".ico", ".jpeg", ".jpg", ".png", ".svg", ".webp"}
WORKFLOW_PREFIXES = (".github/workflows/", ".github/actions/")
FULL_PREFIXES = ("src/", "tests/", "tools/", "deploy/")
CI_CLASSIFIER_FILES = {
    "tools/python/classify_ci_changes.py",
    "tests/python/test_classify_ci_changes.py",
}
BUILD_FILE_NAMES = {
    "CMakeLists.txt",
    "CMakePresets.json",
    "CMakeSettings.json",
    "vcpkg.json",
    "vcpkg-configuration.json",
    "conanfile.py",
    "conanfile.txt",
    "pyproject.toml",
    "requirements.txt",
}


def parse_name_status_z(raw: bytes) -> list[str]:
    """解析 git 的 NUL 分隔变更列表，并保留重命名的旧路径和新路径。"""
    fields = raw.decode("utf-8", errors="surrogateescape").split("\0")
    paths: list[str] = []
    index = 0
    while index < len(fields) - 1:
        status = fields[index]
        index += 1
        if not status:
            continue
        path_count = 2 if status[0] in {"C", "R"} else 1
        for _ in range(path_count):
            if index >= len(fields) - 1:
                return []
            path = fields[index]
            index += 1
            if not path or "\x00" in path:
                return []
            paths.append(path)
    return paths


def is_document(path: str) -> bool:
    """判断路径是否只属于文档内容，不把构建或工作流配置当作文档。"""
    normalized = path.replace("\\", "/")
    name = Path(normalized).name
    if normalized.startswith(WORKFLOW_PREFIXES):
        return False
    if normalized in DOCUMENT_CONFIG_FILES:
        return True
    if normalized.startswith("docs/") or normalized.startswith(".github/ISSUE_TEMPLATE/"):
        return normalized.endswith(tuple(DOC_SUFFIXES)) or name in {"LICENSE", "NOTICE"}
    return Path(name).suffix in DOC_SUFFIXES


def is_qml(path: str) -> bool:
    """判断路径是否为不涉及 C++ 接口的 QML/UI 源文件。"""
    normalized = path.replace("\\", "/")
    return normalized.startswith("src/ui/qml/") and Path(normalized).suffix in {".qml", ".qmltypes"}


def is_static_resource(path: str) -> bool:
    """判断路径是否为可以独立校验的静态资源。"""
    normalized = path.replace("\\", "/")
    if normalized.startswith(("src/", "deploy/", ".github/")):
        return False
    return normalized.startswith(("assets/", "resources/")) or Path(normalized).suffix.lower() in STATIC_SUFFIXES


def is_full_validation(path: str) -> bool:
    """判断路径是否可能影响编译、测试、打包或 CI 本身。"""
    normalized = path.replace("\\", "/")
    name = Path(normalized).name
    suffix = Path(normalized).suffix.lower()
    if normalized.startswith(WORKFLOW_PREFIXES) or normalized.startswith(FULL_PREFIXES):
        return True
    if name in BUILD_FILE_NAMES or name.startswith(("CMake", "Dockerfile")):
        return True
    if suffix in {
        ".c",
        ".cc",
        ".cpp",
        ".h",
        ".hpp",
        ".cmake",
        ".json",
        ".lock",
        ".qrc",
        ".rc",
        ".py",
        ".sh",
        ".bat",
        ".ps1",
        ".yml",
        ".yaml",
    }:
        return True
    return False


def is_ci_classifier_file(path: str) -> bool:
    """判断路径是否只影响变更范围分类器及其回归测试。"""
    return path.replace("\\", "/") in CI_CLASSIFIER_FILES


def classify_paths(paths: list[str]) -> dict[str, str | bool | int]:
    """按最保守规则生成 CI 范围分类，无法判断时选择完整验证。"""
    if not paths:
        return {
            "classification": "unknown",
            "run_full": True,
            "run_ui": False,
            "run_resources": False,
            "run_classifier_tests": False,
            "run_workflow": True,
            "docs_changed": False,
            "safe_fallback": True,
            "changed_files": 0,
        }

    categories: set[str] = set()
    for path in paths:
        if not path or "\x00" in path:
            categories.add("full")
        elif is_ci_classifier_file(path):
            categories.add("classifier")
        elif is_document(path):
            categories.add("docs")
        elif is_qml(path):
            categories.add("qml")
        elif is_static_resource(path):
            categories.add("resources")
        elif is_full_validation(path):
            categories.add("full")
        else:
            categories.add("full")

    if categories == {"docs"}:
        classification = "docs-only"
    elif categories == {"classifier"}:
        classification = "classifier-only"
    elif categories <= {"docs", "classifier"}:
        classification = "docs-and-classifier-only"
    elif categories == {"resources"}:
        classification = "resources-only"
    elif categories == {"qml"}:
        classification = "qml-only"
    elif "full" in categories:
        classification = "full"
    else:
        classification = "mixed"

    return {
        "classification": classification,
        "run_full": "full" in categories,
        "run_ui": "qml" in categories,
        "run_resources": "resources" in categories,
        "run_classifier_tests": "classifier" in categories,
        "run_workflow": any(path.replace("\\", "/").startswith(WORKFLOW_PREFIXES) for path in paths),
        "docs_changed": "docs" in categories,
        "safe_fallback": False,
        "changed_files": len(paths),
    }


def changed_paths(base: str, head: str, repository: Path) -> tuple[list[str], bool, str]:
    """读取 base...head 的完整变更，异常时返回安全回退标志。"""
    if not base or not head:
        return [], True, "base 或 head SHA 缺失"
    command = [
        "git",
        "-C",
        str(repository),
        "diff",
        "--name-status",
        "-z",
        "--find-renames",
        "--find-copies",
        f"{base}...{head}",
    ]
    try:
        result = subprocess.run(command, check=True, capture_output=True)
    except (OSError, subprocess.CalledProcessError) as error:
        return [], True, f"无法读取完整 diff：{error}"
    paths = parse_name_status_z(result.stdout)
    if not paths and result.stdout:
        return [], True, "无法解析 git diff 的文件列表"
    return paths, False, ""


def write_outputs(values: dict[str, str | bool | int], output_path: str) -> None:
    """写入 GitHub Actions 输出，同时在本地打印同样的键值。"""
    lines = [f"{key}={str(value).lower() if isinstance(value, bool) else value}" for key, value in values.items()]
    if output_path:
        with open(output_path, "a", encoding="utf-8") as output:
            output.write("\n".join(lines) + "\n")
    print("\n".join(lines))


def main() -> int:
    """执行分类并在异常输入时选择完整验证。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", default="")
    parser.add_argument("--head", default="")
    parser.add_argument("--repository", type=Path, default=Path.cwd())
    parser.add_argument("--github-output", default="")
    args = parser.parse_args()

    paths, fallback, reason = changed_paths(args.base, args.head, args.repository)
    values = classify_paths(paths)
    if fallback:
        values.update(
            classification="full-fallback",
            run_full=True,
            run_ui=False,
            run_resources=False,
            run_classifier_tests=False,
            run_workflow=True,
            docs_changed=False,
            safe_fallback=True,
            changed_files=0,
        )
    write_outputs(values, args.github_output)
    if reason:
        print(f"分类回退原因：{reason}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
