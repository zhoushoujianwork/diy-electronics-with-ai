from __future__ import annotations

import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path
from typing import Any


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

    def source_problems(self, url: str, notes: object = ...) -> list[Any]:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            product = product_record()
            source = product["sources"][0]  # type: ignore[index]
            source["url"] = url
            if notes is not ...:
                source["notes"] = notes
            self.make_catalog(root, product)
            return validate_catalog.Checker(root).run()

    def test_official_http_source_with_explanation_passes(self) -> None:
        self.assertEqual([], self.source_problems(
            "http://example.com/docs/example-board",
            "The manufacturer's official documentation uses this HTTP URL; HTTPS is unavailable.",
        ))

    def test_http_source_without_notes_is_rejected(self) -> None:
        locations = {problem.location for problem in self.source_problems("http://example.com/docs")}
        self.assertIn("sources[0].notes", locations)

    def test_http_source_with_blank_or_non_string_notes_is_rejected(self) -> None:
        for notes in ("", " \t\n ", None, 123):
            with self.subTest(notes=notes):
                locations = {problem.location for problem in self.source_problems("http://example.com/docs", notes)}
                self.assertIn("sources[0].notes", locations)

    def test_non_http_source_url_is_rejected(self) -> None:
        for url in ("ftp://example.com/docs", "file:///docs/example-board", "//example.com/docs"):
            with self.subTest(url=url):
                locations = {problem.location for problem in self.source_problems(url, "Official source.")}
                self.assertIn("sources[0].url", locations)

    def test_source_url_without_host_is_rejected(self) -> None:
        for url in ("http://", "https://", "http:///docs", "https:///docs", "http://:80/docs", "http://user@/docs"):
            with self.subTest(url=url):
                locations = {problem.location for problem in self.source_problems(url, "Official HTTP source; HTTPS unavailable.")}
                self.assertIn("sources[0].url", locations)

    def test_source_url_with_uppercase_scheme_or_whitespace_is_rejected(self) -> None:
        for url in ("HTTP://example.com/docs", "HTTPS://example.com/docs", "https://example.com/a b", "https://example.com/docs\n"):
            with self.subTest(url=url):
                locations = {problem.location for problem in self.source_problems(url, "Official source.")}
                self.assertIn("sources[0].url", locations)

    def test_vendor_urls_still_require_https(self) -> None:
        for field in ("website", "documentation"):
            with self.subTest(field=field), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                self.make_catalog(root)
                record = vendor_record()
                record[field] = "http://example.com/"
                write_json_yaml(root / "catalog/vendors/example-vendor/vendor.yaml", record)
                locations = {problem.location for problem in validate_catalog.Checker(root).run()}
                self.assertIn(field, locations)

    def test_source_url_schema_matches_validator(self) -> None:
        try:
            from jsonschema import Draft202012Validator, FormatChecker
        except ImportError:
            self.skipTest("JSON Schema parity check requires optional jsonschema")
        schema = json.loads((MODULE_PATH.parents[1] / "schema/product.schema.json").read_text())
        validator = Draft202012Validator(schema, format_checker=FormatChecker())
        cases = (
            ("https://example.com/docs", ..., True),
            ("https://example.com:8443/docs?lang=en#overview", ..., True),
            ("http://example.com", "Official HTTP source; HTTPS is unavailable.", True),
            ("http://example.com:8080/docs?lang=en#overview", "Official HTTP source; HTTPS is unavailable.", True),
            ("http://example.com/docs", "Official HTTP source; HTTPS is unavailable.", True),
            ("http://example.com/docs", ..., False),
            ("http://example.com/docs", "", False),
            ("http://example.com/docs", " \t\n ", False),
            ("http://example.com/docs", None, False),
            ("http://example.com/docs", 123, False),
            ("ftp://example.com/docs", "Official source.", False),
            ("file:///docs/example-board", "Official source.", False),
            ("docs/example-board", "Official source.", False),
            ("//example.com/docs", "Official source.", False),
            ("http://", "Official HTTP source; HTTPS unavailable.", False),
            ("https://", ..., False),
            ("http:///docs", "Official HTTP source; HTTPS unavailable.", False),
            ("https:///docs", ..., False),
            ("http://:80/docs", "Official HTTP source; HTTPS unavailable.", False),
            ("http://user@/docs", "Official HTTP source; HTTPS unavailable.", False),
            ("HTTP://example.com/docs", "Official HTTP source; HTTPS unavailable.", False),
            ("HTTPS://example.com/docs", ..., False),
            ("https://example.com/a b", ..., False),
            ("https://example.com/docs\n", ..., False),
        )
        for url, notes, expected in cases:
            with self.subTest(url=url, notes=notes):
                product = product_record()
                source = product["sources"][0]  # type: ignore[index]
                source["url"] = url
                if notes is not ...:
                    source["notes"] = notes
                self.assertEqual(expected, validator.is_valid(product))
                self.assertEqual(expected, not self.source_problems(url, notes))

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
