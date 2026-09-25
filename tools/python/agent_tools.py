#!/usr/bin/env python3
"""为 Agent 提供只读查询、验证规划和白名单检查执行接口。"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path
from typing import Any

from classify_ci_changes import changed_paths, classify_paths
from validate_ai_development import validate_repo
from verification_plan import load_policy, validate_policy, verification_plan


AI_ROOT = Path("docs/developer/ai-development")
CAPABILITY_PATH = AI_ROOT / "eda-capabilities.json"
FIXTURE_PATH = AI_ROOT / "fixture-provenance.json"


def emit(payload: dict[str, Any], output: str = "") -> int:
    """以稳定 JSON 输出结果，必要时写入明确指定的文件。"""
    text = json.dumps(payload, ensure_ascii=False, indent=2) + "\n"
    if output:
        Path(output).write_text(text, encoding="utf-8")
    else:
        sys.stdout.write(text)
    return 0 if payload.get("ok", False) else 1


def run_git(repository: Path, arguments: list[str]) -> tuple[int, str, str]:
    """执行固定的只读 Git 查询，不接受 shell 字符串。"""
    try:
        result = subprocess.run(
            ["git", "-C", str(repository), *arguments],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            check=False,
        )
    except OSError as exc:
        return 127, "", str(exc)
    return result.returncode, result.stdout, result.stderr


def parse_status_paths(status: str) -> list[str]:
    """从 git status --short 输出提取工作区路径，并处理重命名目标。"""
    paths: list[str] = []
    for line in status.splitlines():
        if not line or line.startswith("##"):
            continue
        path = line[3:] if len(line) >= 3 else ""
        if " -> " in path:
            path = path.rsplit(" -> ", 1)[1]
        if path:
            paths.append(path)
    return paths


def inspect_changes(repository: Path, base: str, head: str) -> dict[str, Any]:
    """返回工作区状态、完整变更文件和 CI 范围分类。"""
    status_code, status, status_error = run_git(repository, ["status", "--short", "--branch"])
    paths, fallback, reason = changed_paths(base, head, repository)
    working_tree_paths = parse_status_paths(status)
    effective_paths = sorted(set(paths) | set(working_tree_paths))
    classification = classify_paths(effective_paths)
    if fallback:
        classification.update(
            classification="full-fallback",
            run_full=True,
            run_ui=False,
            run_resources=False,
            safe_fallback=True,
        )
    return {
        "ok": status_code == 0,
        "tool": "inspect_changes",
        "repository": str(repository),
        "base": base,
        "head": head,
        "worktree": {"status": status, "stderr": status_error, "exit_code": status_code},
        "base_head_files": paths,
        "working_tree_files": working_tree_paths,
        "changed_files": effective_paths,
        "classification": classification,
        "fallback_reason": reason,
        "side_effects": "read-only",
    }


def plan_verification(repository: Path, classification: str) -> dict[str, Any]:
    """根据统一策略生成验证步骤、来源和未覆盖风险。"""
    try:
        policy = load_policy(repository)
        errors = validate_policy(policy)
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        return {"ok": False, "tool": "plan_verification", "errors": [str(exc)]}
    if errors:
        return {"ok": False, "tool": "plan_verification", "errors": errors}
    lines = verification_plan(policy, classification)
    profile_names = [line.split(" ", 1)[0].split("=", 1)[1] for line in lines]
    steps = []
    for profile in profile_names:
        for step in policy["minimum_policy"][profile]:
            steps.append({"id": step, "description": policy["commands"][step]})
    risks = [
        "CI 范围分类不等于模块级语义验证计划",
        "商业 EDA 打开、保存和回读不会由该工具自动验证",
    ]
    if classification in {"full", "mixed", "full-fallback", "unknown"}:
        risks.append("完整验证命令可能耗时，且仍需根据实际改动补充格式专用测试")
    return {
        "ok": True,
        "tool": "plan_verification",
        "classification": classification,
        "profiles": profile_names,
        "steps": steps,
        "reasons": [f"分类 {classification} 映射到策略 profile {', '.join(profile_names)}"],
        "risks": risks,
        "policy": str(AI_ROOT / "verification-policy.json"),
        "side_effects": "planning-only",
    }


def validate_project_docs(repository: Path) -> dict[str, Any]:
    """执行仓库 AI 文档、Skill、工作流、台账和 fixture 的只读校验。"""
    errors = validate_repo(repository)
    return {
        "ok": not errors,
        "tool": "validate_project_docs",
        "errors": errors,
        "policy": str(AI_ROOT / "policy/AI_POLICY.md"),
        "side_effects": "read-only",
    }


def load_json_file(repository: Path, relative: Path) -> dict[str, Any]:
    """读取控制面 JSON 文件并确保其顶层为对象。"""
    with (repository / relative).open(encoding="utf-8") as stream:
        value = json.load(stream)
    if not isinstance(value, dict):
        raise ValueError(f"{relative}: 顶层必须是对象")
    return value


def query_capability(repository: Path, format_id: str, artifact: str) -> dict[str, Any]:
    """查询单个 EDA 格式 artifact 的实现和验证证据。"""
    try:
        data = load_json_file(repository, CAPABILITY_PATH)
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        return {"ok": False, "tool": "query_capability", "errors": [str(exc)]}
    for entry in data.get("formats", []):
        if entry.get("id") == format_id:
            details = entry.get("artifacts", {}).get(artifact)
            if details is None:
                break
            return {
                "ok": True,
                "tool": "query_capability",
                "format": format_id,
                "artifact": artifact,
                "record": details,
                "known_loss": entry.get("known_loss", "unknown"),
                "evidence": entry.get("evidence", []),
                "side_effects": "read-only",
            }
    return {"ok": False, "tool": "query_capability", "errors": [f"未知格式或 artifact：{format_id}/{artifact}"]}


def query_fixture(repository: Path, fixture_path: str) -> dict[str, Any]:
    """查询单个 fixture 的来源、哈希、测试和不变量记录。"""
    try:
        data = load_json_file(repository, FIXTURE_PATH)
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        return {"ok": False, "tool": "query_fixture", "errors": [str(exc)]}
    for entry in data.get("fixtures", []):
        if entry.get("path") == fixture_path:
            return {
                "ok": True,
                "tool": "query_fixture",
                "fixture": entry,
                "side_effects": "read-only",
            }
    return {"ok": False, "tool": "query_fixture", "errors": [f"fixture 未登记：{fixture_path}"]}


def allowed_checks(repository: Path) -> dict[str, list[str]]:
    """返回固定的检查白名单；不接受任意 shell 或任意测试路径。"""
    python = repository / ".venv/bin/python"
    python_command = str(python if python.exists() else Path(sys.executable))
    tools = repository / "tools/python"
    return {
        "ai_consistency": [python_command, str(tools / "validate_ai_development.py")],
        "verification_plan": [python_command, str(tools / "verification_plan.py"), "--check"],
        "python_tests": [python_command, "-m", "unittest", "discover", "-s", "tests/python", "-p", "test_*.py"],
        "docs_build": [python_command, str(tools / "build_docs.py"), "--mkdocs"],
        "cpp_format": [python_command, str(tools / "format_code.py"), "--cpp", "--check"],
        "qml_format": [python_command, str(tools / "format_code.py"), "--qml", "--check"],
        "build": [python_command, str(tools / "build_project.py"), "--test"],
        "ctest": ["ctest", "--test-dir", "build", "--output-on-failure"],
    }


def run_check(repository: Path, check_name: str) -> dict[str, Any]:
    """执行白名单中的检查并返回命令、退出状态和输出摘要。"""
    checks = allowed_checks(repository)
    command = checks.get(check_name)
    if command is None:
        return {"ok": False, "tool": "run_check", "errors": [f"不允许的检查：{check_name}"], "allowed": sorted(checks)}
    environment = os.environ.copy()
    environment["QT_QPA_PLATFORM"] = "offscreen"
    try:
        result = subprocess.run(command, cwd=repository, capture_output=True, text=True, env=environment, check=False)
    except OSError as exc:
        return {"ok": False, "tool": "run_check", "check": check_name, "command": command, "errors": [str(exc)]}
    return {
        "ok": result.returncode == 0,
        "tool": "run_check",
        "check": check_name,
        "command": command,
        "exit_code": result.returncode,
        "stdout_tail": result.stdout[-4000:],
        "stderr_tail": result.stderr[-4000:],
        "side_effects": "whitelisted-check; build artifacts may be created",
    }


def generate_evidence_report(input_path: str, output: str) -> dict[str, Any]:
    """根据明确提供的检查结果生成结构化证据报告，不猜测未提供的结果。"""
    try:
        source = Path(input_path).read_text(encoding="utf-8") if input_path else sys.stdin.read()
        data = json.loads(source) if source.strip() else {}
    except (OSError, json.JSONDecodeError) as exc:
        return {"ok": False, "tool": "generate_evidence_report", "errors": [str(exc)]}
    report = {
        "baseline": data.get("baseline", {"ref": "unknown", "commit": "unknown"}),
        "scope": data.get("scope", "unknown"),
        "read_files": data.get("read_files", []),
        "modified_files": data.get("modified_files", []),
        "checks": data.get("checks", []),
        "unexecuted": data.get("unexecuted", []),
        "blockers": data.get("blockers", []),
        "commercial_eda_validation": data.get("commercial_eda_validation", "unknown"),
        "risks": data.get("risks", []),
        "generated_by": "agent_tools.generate_evidence_report",
    }
    return {"ok": True, "tool": "generate_evidence_report", "report": report, "output": output or "stdout", "side_effects": "stdout-only unless output is explicitly provided"}


def main() -> int:
    """解析 JSON CLI 子命令并保持所有输出结构化。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repository", type=Path, default=Path(__file__).resolve().parents[2])
    subparsers = parser.add_subparsers(dest="tool", required=True)

    inspect = subparsers.add_parser("inspect_changes")
    inspect.add_argument("--base", required=True)
    inspect.add_argument("--head", required=True)

    plan = subparsers.add_parser("plan_verification")
    plan.add_argument("--classification", required=True)

    subparsers.add_parser("validate_project_docs")

    capability = subparsers.add_parser("query_capability")
    capability.add_argument("--format", required=True, dest="format_id")
    capability.add_argument("--artifact", required=True)

    fixture = subparsers.add_parser("query_fixture")
    fixture.add_argument("--path", required=True)

    check = subparsers.add_parser("run_check")
    check.add_argument("--name", required=True, dest="check_name")

    report = subparsers.add_parser("generate_evidence_report")
    report.add_argument("--input", default="")
    report.add_argument("--output", default="")

    args = parser.parse_args()
    repository = args.repository.resolve()
    if args.tool == "inspect_changes":
        payload = inspect_changes(repository, args.base, args.head)
    elif args.tool == "plan_verification":
        payload = plan_verification(repository, args.classification)
    elif args.tool == "validate_project_docs":
        payload = validate_project_docs(repository)
    elif args.tool == "query_capability":
        payload = query_capability(repository, args.format_id, args.artifact)
    elif args.tool == "query_fixture":
        payload = query_fixture(repository, args.path)
    elif args.tool == "run_check":
        payload = run_check(repository, args.check_name)
    else:
        payload = generate_evidence_report(args.input, args.output)
    return emit(payload, args.output if args.tool == "generate_evidence_report" else "")


if __name__ == "__main__":
    raise SystemExit(main())
