#!/usr/bin/env python3
"""验证 AI 协作控制面检查器的正向和反向边界。"""

import json
import tempfile
import unittest
from pathlib import Path

import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "python"))

from validate_ai_development import (
    validate_bilingual_pair,
    validate_capability_ledger,
    validate_json_schema,
    validate_skill_metadata,
    validate_workflow_manifest,
    workflow_events,
)


class ValidateAiDevelopmentTest(unittest.TestCase):
    """确保元数据错误能定位到具体文件，而有效输入可以通过。"""

    def test_valid_bilingual_pair(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            chinese = root / "README.md"
            english = root / "README_en.md"
            chinese.write_text("[English version](README_en.md)\n", encoding="utf-8")
            english.write_text("[中文版](README.md)\n", encoding="utf-8")
            self.assertEqual(validate_bilingual_pair(root, chinese, english), [])

    def test_missing_bilingual_link_reports_file(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            chinese = root / "README.md"
            english = root / "README_en.md"
            chinese.write_text("# 文档\n", encoding="utf-8")
            english.write_text("[中文版](README.md)\n", encoding="utf-8")
            errors = validate_bilingual_pair(root, chinese, english)
            self.assertTrue(any("README.md" in item for item in errors))

    def test_skill_metadata_requires_existing_sources(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            skill_dir = root / "docs/developer/ai-development/skills/demo"
            skill_dir.mkdir(parents=True)
            (skill_dir / "SKILL.md").write_text("# Demo\n", encoding="utf-8")
            metadata = {
                "id": "demo",
                "purpose": "demo",
                "triggers": ["demo"],
                "required_sources": ["missing.md"],
                "side_effects": [],
                "verification": ["check"],
                "stop_conditions": ["stop"],
                "evidence_outputs": ["evidence"],
            }
            (skill_dir / "SKILL.meta.json").write_text(json.dumps(metadata), encoding="utf-8")
            errors = validate_skill_metadata(root, "demo")
            self.assertTrue(any("missing.md" in item for item in errors))

    def test_skill_metadata_rejects_schema_type_and_extra_field(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            skill_dir = root / "docs/developer/ai-development/skills/demo"
            skill_dir.mkdir(parents=True)
            (skill_dir / "SKILL.md").write_text("# Demo\n", encoding="utf-8")
            metadata = {
                "id": "demo",
                "purpose": ["wrong"],
                "triggers": ["demo"],
                "required_sources": ["docs.md"],
                "side_effects": [],
                "verification": ["check"],
                "stop_conditions": ["stop"],
                "evidence_outputs": ["evidence"],
                "unexpected": True,
            }
            (skill_dir / "SKILL.meta.json").write_text(json.dumps(metadata), encoding="utf-8")
            schema_dir = root / "docs/developer/ai-development/schemas"
            schema_dir.mkdir(parents=True)
            schema = Path(__file__).resolve().parents[2] / "docs/developer/ai-development/schemas/skill-metadata.schema.json"
            (schema_dir / schema.name).write_text(schema.read_text(encoding="utf-8"), encoding="utf-8")
            errors = validate_skill_metadata(root, "demo")
            self.assertTrue(any("类型错误" in item for item in errors))
            self.assertTrue(any("未声明字段" in item for item in errors))

    def test_verification_plan_rejects_unmapped_minimum_step(self):
        from verification_plan import validate_policy

        data = {"minimum_policy": {"unknown": ["missing_step"]}, "commands": {}}
        self.assertTrue(any("missing_step" in item for item in validate_policy(data)))

    def test_optional_local_fixture_does_not_require_local_hash(self):
        from validate_ai_development import validate_fixture_manifest

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fixture = root / "tests/fixtures/local.bin"
            fixture.parent.mkdir(parents=True)
            fixture.write_bytes(b"local variation")
            manifest = root / "docs/developer/ai-development/fixture-provenance.json"
            manifest.parent.mkdir(parents=True)
            manifest.write_text(
                json.dumps({"fixtures": [{
                    "path": "tests/fixtures/local.bin",
                    "availability": "optional-local",
                    "sha256": "not-verified",
                    "source": "unknown",
                    "acquisition": "local",
                    "software_version": "unknown",
                    "license": "unknown",
                    "distribution_status": "unknown",
                    "tests": [],
                }]}),
                encoding="utf-8",
            )
            self.assertEqual(validate_fixture_manifest(root), [])

    def test_real_workflow_manifest_and_events_are_consistent(self):
        repository = Path(__file__).resolve().parents[2]
        self.assertEqual(validate_workflow_manifest(repository), [])
        self.assertIn("pull_request", workflow_events(repository / ".github/workflows/docs-check.yml"))
        self.assertIn("workflow_dispatch", workflow_events(repository / ".github/workflows/docs-check.yml"))

    def test_capability_ledger_requires_status_dimensions(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            ledger = root / "docs/developer/ai-development/eda-capabilities.json"
            ledger.parent.mkdir(parents=True)
            ledger.write_text(json.dumps({"formats": [], "status_vocabulary": []}), encoding="utf-8")
            errors = validate_capability_ledger(root)
            self.assertTrue(any("status_dimensions" in item for item in errors))

    def test_real_capability_ledger_is_machine_readable(self):
        repository = Path(__file__).resolve().parents[2]
        self.assertEqual(validate_capability_ledger(repository), [])


if __name__ == "__main__":
    unittest.main()
