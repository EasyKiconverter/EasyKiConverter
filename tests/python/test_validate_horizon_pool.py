#!/usr/bin/env python3
"""验证 Horizon Pool 官方索引检查器的正反向边界。"""

import json
import importlib.util
import sqlite3
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "integration" / "validate_horizon_pool.py"
SPEC = importlib.util.spec_from_file_location("validate_horizon_pool", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def create_database(path: Path, *, include_package: bool = True, valid_reference: bool = True) -> None:
    """创建仅用于验证器测试的最小 SQLite 索引。"""
    with sqlite3.connect(path) as connection:
        connection.executescript(
            """
            CREATE TABLE units (uuid TEXT);
            CREATE TABLE symbols (uuid TEXT);
            CREATE TABLE entities (uuid TEXT);
            CREATE TABLE packages (uuid TEXT);
            CREATE TABLE padstacks (uuid TEXT);
            CREATE TABLE parts (uuid TEXT, entity TEXT, package TEXT);
            CREATE VIEW all_items_view AS
              SELECT 'unit' AS type, uuid FROM units
              UNION ALL SELECT 'symbol', uuid FROM symbols
              UNION ALL SELECT 'entity', uuid FROM entities
              UNION ALL SELECT 'package', uuid FROM packages
              UNION ALL SELECT 'padstack', uuid FROM padstacks
              UNION ALL SELECT 'part', uuid FROM parts;
            """
        )
        connection.execute("INSERT INTO units VALUES ('unit')")
        connection.execute("INSERT INTO symbols VALUES ('symbol')")
        connection.execute("INSERT INTO entities VALUES ('entity')")
        if include_package:
            connection.execute("INSERT INTO packages VALUES ('package')")
        connection.execute("INSERT INTO padstacks VALUES ('padstack')")
        package = "package" if valid_reference else "missing-package"
        connection.execute("INSERT INTO parts VALUES ('part', 'entity', ?)", (package,))


class ValidateHorizonPoolTest(unittest.TestCase):
    """确保验证器拒绝缺失对象和断裂引用。"""

    def test_valid_index_passes(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            database = Path(directory) / "pool.db"
            create_database(database)
            summary = MODULE.validate_index(Path(directory))
            self.assertEqual(summary["parts"], 1)

    def test_missing_index_type_fails(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            database = Path(directory) / "pool.db"
            create_database(database, include_package=False)
            with self.assertRaisesRegex(RuntimeError, "package"):
                MODULE.validate_index(Path(directory))

    def test_broken_part_reference_fails(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            database = Path(directory) / "pool.db"
            create_database(database, valid_reference=False)
            with self.assertRaisesRegex(RuntimeError, "Package"):
                MODULE.validate_index(Path(directory))

    def test_source_reference_checker_rejects_model_path_escape(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            pool = Path(directory)
            for folder in ("units", "symbols", "entities", "padstacks", "packages", "parts"):
                (pool / folder).mkdir()
            (pool / "units/unit.json").write_text(json.dumps({"uuid": "unit", "pins": {"pin": {}}}))
            (pool / "symbols/symbol.json").write_text(json.dumps({"uuid": "symbol", "unit": "unit"}))
            (pool / "entities/entity.json").write_text(
                json.dumps({"uuid": "entity", "gates": {"gate": {"unit": "unit"}}})
            )
            (pool / "padstacks/padstack.json").write_text(json.dumps({"uuid": "padstack"}))
            (pool / "packages/pkg/package.json").parent.mkdir()
            (pool / "models").mkdir()
            (pool / "models/model.step").write_text("STEP")
            (pool / "packages/pkg/package.json").write_text(
                json.dumps(
                    {
                        "uuid": "package",
                        "pads": {"pad": {"padstack": "padstack"}},
                        "models": {"model": {"filename": "models/model.step"}},
                        "default_model": "model",
                    }
                )
            )
            (pool / "parts/part.json").write_text(
                json.dumps(
                    {
                        "uuid": "part",
                        "entity": "entity",
                        "package": "package",
                        "pad_map": {"pad": {"gate": "gate", "pin": "pin"}},
                    }
                )
            )
            summary = MODULE.validate_source_references(pool)
            self.assertEqual(summary["parts"], 1)
            package_path = pool / "packages/pkg/package.json"
            package = json.loads(package_path.read_text())
            package["models"]["model"]["filename"] = "../outside.step"
            package_path.write_text(json.dumps(package))
            with self.assertRaisesRegex(RuntimeError, "越过根目录"):
                MODULE.validate_source_references(pool)


if __name__ == "__main__":
    unittest.main()
