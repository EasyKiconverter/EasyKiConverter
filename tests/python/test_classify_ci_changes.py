#!/usr/bin/env python3
"""验证 CI 变更范围分类器的安全边界。"""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "python"))

from classify_ci_changes import changed_paths, classify_paths, parse_name_status_z


class ClassifyCiChangesTest(unittest.TestCase):
    """覆盖常见变更、异常路径以及删除和重命名场景。"""

    def test_document_only(self):
        result = classify_paths(["README.md", "docs/user/FAQ_en.md", "mkdocs.yml"])
        self.assertEqual(result["classification"], "docs-only")
        self.assertFalse(result["run_full"])

    def test_resource_only(self):
        result = classify_paths(["assets/logo.svg", "resources/icons/app.png"])
        self.assertEqual(result["classification"], "resources-only")
        self.assertFalse(result["run_full"])

    def test_qml_only(self):
        result = classify_paths(["src/ui/qml/MainWindow.qml"])
        self.assertEqual(result["classification"], "qml-only")
        self.assertTrue(result["run_ui"])
        self.assertFalse(result["run_full"])

    def test_code_cmake_workflow_and_mixed_changes_are_full(self):
        for paths in (
            ["src/core/ir/Footprint.cpp"],
            ["CMakeLists.txt"],
            [".github/workflows/build.yml"],
            ["README.md", "src/main.cpp"],
        ):
            self.assertTrue(classify_paths(paths)["run_full"], paths)

        mixed_ui_resources = classify_paths(["src/ui/qml/Main.qml", "resources/icons/app.svg"])
        self.assertFalse(mixed_ui_resources["run_full"])
        self.assertTrue(mixed_ui_resources["run_ui"])
        self.assertTrue(mixed_ui_resources["run_resources"])

    def test_deleted_and_renamed_files_are_kept_for_classification(self):
        raw = b"D\0docs/old.md\0R100\0README.md\0README_en.md\0"
        self.assertEqual(parse_name_status_z(raw), ["docs/old.md", "README.md", "README_en.md"])
        self.assertEqual(classify_paths(parse_name_status_z(raw))["classification"], "docs-only")

    def test_workflow_changes_request_workflow_validation(self):
        result = classify_paths([".github/workflows/build.yml"])
        self.assertTrue(result["run_workflow"])
        self.assertTrue(classify_paths([".github/workflows/WORKFLOW_DEPENDENCIES.md"])["run_full"])

    def test_unknown_path_and_empty_diff_fail_safe(self):
        self.assertTrue(classify_paths(["component.without.known.extension"])["run_full"])
        self.assertTrue(classify_paths([])["safe_fallback"])
        self.assertTrue(classify_paths([])["run_full"])

    def test_missing_diff_revision_requests_full_validation(self):
        paths, fallback, reason = changed_paths("missing-base", "missing-head", Path.cwd())
        self.assertEqual(paths, [])
        self.assertTrue(fallback)
        self.assertIn("无法读取完整 diff", reason)


if __name__ == "__main__":
    unittest.main()
