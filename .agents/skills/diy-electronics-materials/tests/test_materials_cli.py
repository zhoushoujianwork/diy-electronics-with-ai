#!/usr/bin/env python3
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "scripts/materials.py"


class MaterialsCliTest(unittest.TestCase):
    def run_cli(self, *args, env=None):
        completed = subprocess.run(
            [sys.executable, str(SCRIPT), *map(str, args)],
            check=True,
            capture_output=True,
            text=True,
            env=env,
        )
        return json.loads(completed.stdout)

    def test_private_default_and_inventory_workflow(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            private_home = root / "private-materials"
            env = os.environ.copy()
            env["ELECTRONIC_MATERIALS_HOME"] = str(private_home)
            env.pop("XDG_DATA_HOME", None)

            initial = self.run_cli("status", env=env)
            self.assertEqual(initial["database"], str(private_home / "materials.sqlite3"))
            self.assertEqual(initial["purchase_lines"], 0)

            fixture = root / "orders.json"
            fixture.write_text(json.dumps([{
                "source": "lcsc",
                "account": "test",
                "order_id": "fixture-order",
                "line_id": "001",
                "title": "ESP32 test fixture",
                "quantity": 2,
                "unit": "件",
                "status": "completed",
                "evidence": "fixture-only",
                "observed_at": "2026-01-01T00:00:00+00:00",
                "purchased_at": "2025-12-31",
                "model": "ESP32",
                "category": "模块与开发板",
                "aliases": ["ESP32"],
            }], ensure_ascii=False), encoding="utf-8")

            imported = self.run_cli("import", fixture, env=env)
            self.assertEqual(imported["changed_rows"], 1)
            found = self.run_cli("search", "ESP32", env=env)
            self.assertEqual(found["matched"], 1)
            key = found["results"][0]["key"]

            stock = self.run_cli(
                "stocktake", key, "1", "--unit", "件", "--note", "test fixture", env=env
            )
            self.assertTrue(stock["recorded"])
            coverage = self.run_cli(
                "coverage", "lcsc", "--status", "partial", "--scope", "test fixture", env=env
            )
            self.assertEqual(coverage[1]["status"], "partial")

            export = root / "export.json"
            exported = self.run_cli("export", export, env=env)
            self.assertEqual(exported["records"], 1)
            self.assertTrue(export.is_file())


if __name__ == "__main__":
    unittest.main()
