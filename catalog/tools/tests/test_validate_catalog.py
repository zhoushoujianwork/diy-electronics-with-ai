from __future__ import annotations

import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).resolve().parents[1] / "validate_catalog.py"
SPEC = importlib.util.spec_from_file_location("validate_catalog", MODULE_PATH)
assert SPEC and SPEC.loader
validate_catalog = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = validate_catalog
SPEC.loader.exec_module(validate_catalog)


def write_json_yaml(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2), encoding="utf-8")


def vendor_record() -> dict[str, object]:
    return {
        "schema_version": 1,
        "id": "example-vendor",
        "name": "Example Vendor",
        "website": "https://example.com/",
        "last_reviewed": "2026-09-20",
    }


def product_record() -> dict[str, object]:
    return {
        "schema_version": 1,
        "id": "example-board",
        "vendor_id": "example-vendor",
        "name": "Example Board",
        "family": "Example",
        "model": "EX-1",
        "category": "development-board",
        "lifecycle": "active",
        "summary": "A fixture used only to test catalog validation.",
        "specifications": {
            "processor": {"model": "EX32", "cores": 2, "source_refs": ["product-page"]},
            "memory": {"flash_bytes": 8388608, "source_refs": ["product-page"]},
        },
        "sources": [{
            "id": "product-page",
            "type": "product-page",
            "title": "Example product page",
            "publisher": "Example Vendor",
            "url": "https://example.com/products/example-board",
            "accessed_on": "2026-09-20",
            "official": True,
        }],
        "validation": {"level": "cataloged"},
    }


class CatalogValidationTests(unittest.TestCase):
    def make_catalog(self, root: Path, product: dict[str, object] | None = None) -> None:
        base = root / "catalog" / "vendors" / "example-vendor"
        write_json_yaml(base / "vendor.yaml", vendor_record())
        write_json_yaml(base / "products" / "example-board.yaml", product or product_record())

    def test_valid_catalog_passes_without_yaml_dependency(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_catalog(root)
            self.assertEqual([], validate_catalog.Checker(root).run())

    def test_unknown_source_reference_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            product = product_record()
            product["specifications"]["processor"]["source_refs"] = ["missing"]  # type: ignore[index]
            self.make_catalog(root, product)
            messages = [problem.message for problem in validate_catalog.Checker(root).run()]
            self.assertTrue(any("unknown source ID" in message for message in messages))

    def test_hardware_verified_requires_real_test_context(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            product = product_record()
            product["validation"] = {
                "level": "hardware-verified",
                "checked_on": "2026-09-20",
                "firmware_commit": "abcdef1",
                "build_target": "example-board",
                "evidence": [{"path": "docs/test.md"}],
            }
            self.make_catalog(root, product)
            locations = {problem.location for problem in validate_catalog.Checker(root).run()}
            self.assertIn("validation.exact_revision", locations)
            self.assertIn("validation.power_arrangement", locations)
            self.assertIn("validation.duration_seconds", locations)
            self.assertIn("validation.pass_criteria", locations)

    def test_vendor_directory_must_match_record(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_catalog(root)
            record_path = root / "catalog" / "vendors" / "example-vendor" / "vendor.yaml"
            record = vendor_record()
            record["id"] = "different-vendor"
            write_json_yaml(record_path, record)
            messages = [problem.message for problem in validate_catalog.Checker(root).run()]
            self.assertTrue(any("must match vendor directory" in message for message in messages))


if __name__ == "__main__":
    unittest.main()
