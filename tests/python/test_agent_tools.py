#!/usr/bin/env python3
"""验证 Agent JSON 工具的查询边界、白名单和结构化输出。"""

import json
import tempfile
import unittest
from pathlib import Path

import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "python"))

from agent_tools import (
    allowed_checks,
    generate_evidence_report,
    parse_status_paths,
    plan_verification,
    query_capability,
    query_fixture,
    resolve_report_output,
    run_check,
)


class AgentToolsTest(unittest.TestCase):
    """确保 Agent 工具只暴露仓库内、可复核的能力。"""

    @classmethod
    def setUpClass(cls):
        cls.repository = Path(__file__).resolve().parents[2]

    def test_plan_verification_full_contains_build_and_tests(self):
        result = plan_verification(self.repository, "full")
        self.assertTrue(result["ok"])
        step_ids = {step["id"] for step in result["steps"]}
        self.assertIn("build", step_ids)
        self.assertIn("full_validation", step_ids)

    def test_status_parser_keeps_untracked_and_renamed_targets(self):
        status = "## topic\n M docs/README.md\n?? tools/new_tool.py\nR  old.md -> docs/new.md\n"
        self.assertEqual(parse_status_paths(status), ["docs/README.md", "tools/new_tool.py", "docs/new.md"])

    def test_capability_query_returns_evidence(self):
        result = query_capability(self.repository, "kicad", "symbol")
        self.assertTrue(result["ok"])
        self.assertIn("automated_tests", result["record"])
        self.assertIn("commercial_eda_validation", result["record"])

    def test_unknown_capability_is_rejected(self):
        result = query_capability(self.repository, "not-a-format", "symbol")
        self.assertFalse(result["ok"])

    def test_fixture_query_returns_provenance(self):
        result = query_fixture(self.repository, "tests/fixtures/easyeda/symbol_basic.json")
        self.assertTrue(result["ok"])
        self.assertIn("sha256", result["fixture"])
        self.assertIn("tests", result["fixture"])

    def test_unknown_check_is_rejected_without_execution(self):
        result = run_check(self.repository, "shell:rm -rf")
        self.assertFalse(result["ok"])
        self.assertIn("python_tests", result["allowed"])

    def test_allowed_checks_are_fixed_commands(self):
        checks = allowed_checks(self.repository)
        self.assertIn("ai_consistency", checks)
        self.assertNotIn("shell", checks)
        self.assertTrue(all(isinstance(command, list) for command in checks.values()))

    def test_evidence_report_preserves_missing_results_as_unknown(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "evidence.json"
            source.write_text(json.dumps({"baseline": {"ref": "v3.1.13"}, "checks": []}), encoding="utf-8")
            result = generate_evidence_report(str(source), "")
            self.assertTrue(result["ok"])
            self.assertEqual(result["report"]["commercial_eda_validation"], "unknown")
            self.assertEqual(result["report"]["risks"], [])

    def test_report_output_rejects_existing_file_without_force(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            existing = root / "evidence.json"
            existing.write_text("existing", encoding="utf-8")
            output, error = resolve_report_output(root, "evidence.json", False)
            self.assertEqual(output, "")
            self.assertIn("--force", error)

    def test_report_output_rejects_paths_outside_repository(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            output, error = resolve_report_output(root, "../evidence.json", True)
            self.assertEqual(output, "")
            self.assertIn("仓库目录内", error)

    def test_report_output_allows_new_repository_file_and_explicit_force(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            output, error = resolve_report_output(root, "evidence.json", False)
            self.assertTrue(output.endswith("evidence.json"))
            self.assertEqual(error, "")
            Path(output).write_text("old", encoding="utf-8")
            forced_output, force_error = resolve_report_output(root, "evidence.json", True)
            self.assertEqual(forced_output, output)
            self.assertEqual(force_error, "")


if __name__ == "__main__":
    unittest.main()
