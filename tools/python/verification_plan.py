#!/usr/bin/env python3
"""读取统一验证策略并生成可审计的最小验证计划。"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any


AI_ROOT = Path("docs/developer/ai-development")
POLICY_PATH = AI_ROOT / "verification-policy.json"
REQUIRED_PROFILES = {
    "cpp",
    "parser_or_importer",
    "exporter_or_ir",
    "qml",
    "python_tool",
    "cmake_dependency_workflow",
    "documentation",
    "unknown",
}


def load_policy(repository: Path) -> dict[str, Any]:
    """读取仓库内的统一验证策略。"""
    path = repository / POLICY_PATH
    with path.open(encoding="utf-8") as stream:
        data = json.load(stream)
    if not isinstance(data, dict):
        raise ValueError(f"{path}: 顶层必须是对象")
    return data


def validate_policy(data: dict[str, Any]) -> list[str]:
    """确保最低验证策略中的每个步骤都能解析到命令或明确选择器。"""
    errors: list[str] = []
    minimum_policy = data.get("minimum_policy")
    commands = data.get("commands")
    if not isinstance(minimum_policy, dict):
        return ["minimum_policy 必须是对象"]
    if not isinstance(commands, dict):
        return ["commands 必须是对象"]
    missing_profiles = sorted(REQUIRED_PROFILES - minimum_policy.keys())
    errors.extend(f"minimum_policy 缺少配置：{profile}" for profile in missing_profiles)
    referenced = {item for steps in minimum_policy.values() if isinstance(steps, list) for item in steps}
    for item in sorted(referenced):
        description = commands.get(item)
        if not isinstance(description, str) or not description.strip():
            errors.append(f"验证步骤 {item} 没有可执行命令或可解释选择器")
    return errors


def verification_plan(data: dict[str, Any], classification: str) -> list[str]:
    """根据 CI 分类映射到策略 profile，供日志和 Agent 证据报告使用。"""
    mapping = {
        "docs-only": ["documentation"],
        "classifier-only": ["python_tool"],
        "docs-and-classifier-only": ["documentation", "python_tool"],
        "qml-only": ["qml"],
        "resources-only": ["documentation"],
        "full": ["unknown"],
        "mixed": ["unknown"],
        "full-fallback": ["unknown"],
        "unknown": ["unknown"],
    }
    profiles = mapping.get(classification, ["unknown"])
    return [f"profile={profile} steps={','.join(data['minimum_policy'][profile])}" for profile in profiles]


def main() -> int:
    """执行策略校验或打印指定分类的验证计划。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repository", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--classification", default="unknown")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    try:
        data = load_policy(args.repository.resolve())
        errors = validate_policy(data)
    except (OSError, json.JSONDecodeError, ValueError) as exc:
        print(f"verification policy failed: {exc}", file=sys.stderr)
        return 1
    if errors:
        print("verification policy failed:", file=sys.stderr)
        print("\n".join(f"- {item}" for item in errors), file=sys.stderr)
        return 1
    if not args.check:
        print("\n".join(verification_plan(data, args.classification)))
    else:
        print("Verification policy plan passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
