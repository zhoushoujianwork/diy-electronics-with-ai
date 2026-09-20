#!/usr/bin/env python3
"""Validate the hardware catalog without requiring a JSON Schema runtime.

JSON-formatted YAML works with the standard library. Normal YAML additionally requires PyYAML.
The JSON schemas remain the canonical interchange contract; this tool enforces repository-specific
cross-file and evidence rules that JSON Schema alone cannot express conveniently.
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import Any, Iterable
from urllib.parse import urlparse


ID_RE = re.compile(r"^[a-z0-9]+(?:-[a-z0-9]+)*$")
CATEGORIES = {
    "development-board", "system-on-module", "single-board-computer", "microcontroller-module",
    "sensor-module", "communication-module", "display-module", "audio-module",
    "positioning-module", "power-module", "actuator-module", "accessory",
}
LIFECYCLES = {"active", "not-recommended-for-new-designs", "discontinued", "unknown"}
LEVELS = {"cataloged", "build-verified", "hardware-verified"}
SOURCE_TYPES = {"product-page", "documentation", "datasheet", "schematic", "repository", "certification"}
SOURCED_GROUPS = {"processor", "memory", "display", "power", "mechanical"}
EVIDENCE_KINDS = {"build-log", "test-report", "serial-log", "photo", "video", "measurement", "note"}
VENDOR_FIELDS = {"schema_version", "id", "name", "website", "documentation", "aliases", "last_reviewed", "notes"}
PRODUCT_FIELDS = {
    "schema_version", "id", "vendor_id", "name", "family", "model", "sku", "revision",
    "category", "lifecycle", "summary", "board_profile", "specifications", "software",
    "sources", "validation", "notes",
}


@dataclass(frozen=True)
class Problem:
    path: Path
    location: str
    message: str

    def render(self, root: Path) -> str:
        try:
            filename = self.path.relative_to(root)
        except ValueError:
            filename = self.path
        where = f":{self.location}" if self.location else ""
        return f"{filename}{where}: {self.message}"


def _load_yaml(path: Path) -> Any:
    text = path.read_text(encoding="utf-8")
    try:
        return json.loads(text)
    except json.JSONDecodeError:
        try:
            import yaml  # type: ignore[import-not-found]
        except ImportError as exc:
            raise ValueError(
                "normal YAML requires PyYAML; install it or use JSON syntax, which is valid YAML"
            ) from exc
        return yaml.safe_load(text)


def _is_date(value: Any) -> bool:
    if isinstance(value, dt.date):
        return True
    if not isinstance(value, str):
        return False
    try:
        dt.date.fromisoformat(value)
    except ValueError:
        return False
    return bool(re.fullmatch(r"\d{4}-\d{2}-\d{2}", value))


def _is_https(value: Any) -> bool:
    if not isinstance(value, str):
        return False
    parsed = urlparse(value)
    return parsed.scheme == "https" and bool(parsed.netloc)


def _is_repo_relative(value: Any) -> bool:
    if not isinstance(value, str) or not value or "\\" in value:
        return False
    path = PurePosixPath(value)
    return not path.is_absolute() and ".." not in path.parts


class Checker:
    def __init__(self, root: Path) -> None:
        self.root = root
        self.problems: list[Problem] = []
        self.vendor_ids: dict[str, Path] = {}
        self.product_ids: dict[tuple[str, str], Path] = {}

    def error(self, path: Path, location: str, message: str) -> None:
        self.problems.append(Problem(path, location, message))

    def require(self, obj: dict[str, Any], keys: Iterable[str], path: Path, location: str = "") -> None:
        for key in keys:
            if key not in obj:
                self.error(path, f"{location}.{key}".strip("."), "required field is missing")

    def check_id(self, value: Any, path: Path, location: str) -> bool:
        if not isinstance(value, str) or not ID_RE.fullmatch(value):
            self.error(path, location, "must be a lowercase ASCII kebab-case ID")
            return False
        return True

    def check_vendor(self, path: Path, expected_id: str) -> None:
        try:
            data = _load_yaml(path)
        except Exception as exc:
            self.error(path, "", f"cannot parse YAML: {exc}")
            return
        if not isinstance(data, dict):
            self.error(path, "", "top level must be a mapping")
            return
        self.require(data, ("schema_version", "id", "name", "website", "last_reviewed"), path)
        for field in data.keys() - VENDOR_FIELDS:
            self.error(path, field, "unknown vendor field")
        if data.get("schema_version") != 1:
            self.error(path, "schema_version", "must equal 1")
        vendor_id = data.get("id")
        if self.check_id(vendor_id, path, "id"):
            if vendor_id != expected_id:
                self.error(path, "id", f"must match vendor directory {expected_id!r}")
            if vendor_id in self.vendor_ids:
                self.error(path, "id", f"duplicates {self.vendor_ids[vendor_id]}")
            else:
                self.vendor_ids[vendor_id] = path
        if not isinstance(data.get("name"), str) or not data.get("name", "").strip():
            self.error(path, "name", "must be a non-empty string")
        if not _is_https(data.get("website")):
            self.error(path, "website", "must be an absolute HTTPS URL")
        if "documentation" in data and not _is_https(data.get("documentation")):
            self.error(path, "documentation", "must be an absolute HTTPS URL")
        if not _is_date(data.get("last_reviewed")):
            self.error(path, "last_reviewed", "must be an ISO 8601 calendar date (YYYY-MM-DD)")

    def _check_source_refs(self, value: Any, known: set[str], path: Path, location: str) -> None:
        if not isinstance(value, list) or not value:
            self.error(path, location, "must be a non-empty list of source IDs")
            return
        for index, ref in enumerate(value):
            if ref not in known:
                self.error(path, f"{location}[{index}]", f"unknown source ID {ref!r}")

    def _walk_sourced_groups(self, data: dict[str, Any], source_ids: set[str], path: Path) -> None:
        specs = data.get("specifications")
        if specs is not None and not isinstance(specs, dict):
            self.error(path, "specifications", "must be a mapping")
            return
        if isinstance(specs, dict):
            for name, value in specs.items():
                if name in SOURCED_GROUPS:
                    if not isinstance(value, dict):
                        self.error(path, f"specifications.{name}", "must be a mapping")
                    else:
                        self._check_source_refs(value.get("source_refs"), source_ids, path, f"specifications.{name}.source_refs")
                elif name in {"wireless", "interfaces", "connectors"}:
                    if not isinstance(value, list):
                        self.error(path, f"specifications.{name}", "must be a list")
                    else:
                        for index, item in enumerate(value):
                            if not isinstance(item, dict):
                                self.error(path, f"specifications.{name}[{index}]", "must be a mapping")
                            else:
                                self._check_source_refs(item.get("source_refs"), source_ids, path, f"specifications.{name}[{index}].source_refs")
                else:
                    self.error(path, f"specifications.{name}", "unknown specification group")
        software = data.get("software")
        if software is not None:
            if not isinstance(software, dict):
                self.error(path, "software", "must be a mapping")
            else:
                self._check_source_refs(software.get("source_refs"), source_ids, path, "software.source_refs")

    def _check_numbers(self, data: dict[str, Any], path: Path) -> None:
        specs = data.get("specifications")
        if not isinstance(specs, dict):
            return
        for group, fields in {
            "processor": ("cores", "max_frequency_hz"),
            "memory": ("flash_bytes", "ram_bytes", "psram_bytes"),
            "display": ("width_px", "height_px"),
        }.items():
            obj = specs.get(group)
            if not isinstance(obj, dict):
                continue
            for field in fields:
                if field in obj and (isinstance(obj[field], bool) or not isinstance(obj[field], int) or obj[field] < (1 if group != "memory" else 0)):
                    self.error(path, f"specifications.{group}.{field}", "must be a non-negative integer in the documented base unit")
        power = specs.get("power")
        if isinstance(power, dict):
            for field in ("input_voltage_min_v", "input_voltage_max_v", "input_voltage_nominal_v", "logic_voltage_v", "peak_current_a"):
                if field in power and (isinstance(power[field], bool) or not isinstance(power[field], (int, float)) or power[field] < 0):
                    self.error(path, f"specifications.power.{field}", "must be a non-negative number")
            low, high = power.get("input_voltage_min_v"), power.get("input_voltage_max_v")
            if isinstance(low, (int, float)) and isinstance(high, (int, float)) and low > high:
                self.error(path, "specifications.power", "minimum input voltage exceeds maximum")

    def _check_validation(self, value: Any, path: Path) -> None:
        if not isinstance(value, dict):
            self.error(path, "validation", "must be a mapping")
            return
        level = value.get("level")
        if level not in LEVELS:
            self.error(path, "validation.level", f"must be one of {sorted(LEVELS)}")
            return
        if level == "cataloged":
            return
        for field in ("checked_on", "firmware_commit", "build_target", "evidence"):
            if field not in value:
                self.error(path, f"validation.{field}", f"required for {level}")
        if "checked_on" in value and not _is_date(value["checked_on"]):
            self.error(path, "validation.checked_on", "must be an ISO 8601 calendar date")
        commit = value.get("firmware_commit")
        if commit is not None and (not isinstance(commit, str) or len(commit) < 7):
            self.error(path, "validation.firmware_commit", "must identify a commit with at least 7 characters")
        evidence = value.get("evidence")
        if evidence is not None:
            if not isinstance(evidence, list) or not evidence:
                self.error(path, "validation.evidence", "must be a non-empty list")
            else:
                for index, item in enumerate(evidence):
                    if not isinstance(item, dict):
                        self.error(path, f"validation.evidence[{index}]", "must be a mapping")
                    else:
                        for field in ("kind", "path", "summary"):
                            if field not in item:
                                self.error(path, f"validation.evidence[{index}].{field}", "required evidence field is missing")
                        if item.get("kind") not in EVIDENCE_KINDS:
                            self.error(path, f"validation.evidence[{index}].kind", f"must be one of {sorted(EVIDENCE_KINDS)}")
                        if not _is_repo_relative(item.get("path")):
                            self.error(path, f"validation.evidence[{index}].path", "must be a safe repository-relative path")
                        if not isinstance(item.get("summary"), str) or not item.get("summary", "").strip():
                            self.error(path, f"validation.evidence[{index}].summary", "must be a non-empty string")
        if level == "hardware-verified":
            for field in ("exact_revision", "power_arrangement", "duration_seconds", "pass_criteria"):
                if field not in value:
                    self.error(path, f"validation.{field}", "required for hardware-verified")
            duration = value.get("duration_seconds")
            if duration is not None and (isinstance(duration, bool) or not isinstance(duration, int) or duration < 1):
                self.error(path, "validation.duration_seconds", "must be a positive integer")
            criteria = value.get("pass_criteria")
            if criteria is not None and (not isinstance(criteria, list) or not criteria):
                self.error(path, "validation.pass_criteria", "must be a non-empty list")

    def check_product(self, path: Path, expected_vendor: str) -> None:
        try:
            data = _load_yaml(path)
        except Exception as exc:
            self.error(path, "", f"cannot parse YAML: {exc}")
            return
        if not isinstance(data, dict):
            self.error(path, "", "top level must be a mapping")
            return
        self.require(data, (
            "schema_version", "id", "vendor_id", "name", "family", "category", "lifecycle",
            "summary", "sources", "validation",
        ), path)
        for field in data.keys() - PRODUCT_FIELDS:
            self.error(path, field, "unknown product field")
        if data.get("schema_version") != 1:
            self.error(path, "schema_version", "must equal 1")
        product_id = data.get("id")
        if self.check_id(product_id, path, "id"):
            if product_id != path.stem:
                self.error(path, "id", f"must match filename stem {path.stem!r}")
            product_key = (expected_vendor, product_id)
            if product_key in self.product_ids:
                self.error(path, "id", f"duplicates {self.product_ids[product_key]}")
            else:
                self.product_ids[product_key] = path
        vendor_id = data.get("vendor_id")
        if self.check_id(vendor_id, path, "vendor_id") and vendor_id != expected_vendor:
            self.error(path, "vendor_id", f"must match vendor directory {expected_vendor!r}")
        if data.get("category") not in CATEGORIES:
            self.error(path, "category", f"must be one of {sorted(CATEGORIES)}")
        if data.get("lifecycle") not in LIFECYCLES:
            self.error(path, "lifecycle", f"must be one of {sorted(LIFECYCLES)}")
        for field in ("name", "family", "summary"):
            if not isinstance(data.get(field), str) or not data.get(field, "").strip():
                self.error(path, field, "must be a non-empty string")
        board_profile = data.get("board_profile")
        if board_profile is not None and (
            not _is_repo_relative(board_profile) or not board_profile.startswith("boards/")
        ):
            self.error(path, "board_profile", "must be a safe repository-relative path below boards/")
        if "pinout" in data or "pins" in data:
            self.error(path, "pinout", "pin maps are not accepted here; link a revision-specific boards/ profile")

        sources = data.get("sources")
        source_ids: set[str] = set()
        if not isinstance(sources, list) or not sources:
            self.error(path, "sources", "must be a non-empty list of official sources")
        else:
            for index, source in enumerate(sources):
                location = f"sources[{index}]"
                if not isinstance(source, dict):
                    self.error(path, location, "must be a mapping")
                    continue
                self.require(source, ("id", "type", "title", "publisher", "url", "accessed_on", "official"), path, location)
                source_id = source.get("id")
                if self.check_id(source_id, path, f"{location}.id"):
                    if source_id in source_ids:
                        self.error(path, f"{location}.id", "duplicate source ID")
                    source_ids.add(source_id)
                if source.get("type") not in SOURCE_TYPES:
                    self.error(path, f"{location}.type", f"must be one of {sorted(SOURCE_TYPES)}")
                if source.get("official") is not True:
                    self.error(path, f"{location}.official", "must be true; catalog facts require an official source")
                if not _is_https(source.get("url")):
                    self.error(path, f"{location}.url", "must be an absolute HTTPS URL")
                if not _is_date(source.get("accessed_on")):
                    self.error(path, f"{location}.accessed_on", "must be an ISO 8601 calendar date")
                for field in ("title", "publisher"):
                    if not isinstance(source.get(field), str) or not source.get(field, "").strip():
                        self.error(path, f"{location}.{field}", "must be a non-empty string")

        self._walk_sourced_groups(data, source_ids, path)
        self._check_numbers(data, path)
        self._check_validation(data.get("validation"), path)

    def run(self) -> list[Problem]:
        vendors_root = self.root / "catalog" / "vendors"
        if not vendors_root.exists():
            return self.problems
        for vendor_dir in sorted(p for p in vendors_root.iterdir() if p.is_dir()):
            expected_id = vendor_dir.name
            if not ID_RE.fullmatch(expected_id):
                self.error(vendor_dir, "", "vendor directory must use lowercase ASCII kebab-case")
            vendor_file = vendor_dir / "vendor.yaml"
            if not vendor_file.is_file():
                self.error(vendor_file, "", "vendor.yaml is required")
            else:
                self.check_vendor(vendor_file, expected_id)
            products_dir = vendor_dir / "products"
            if products_dir.exists():
                for product_file in sorted(products_dir.glob("*.yaml")):
                    self.check_product(product_file, expected_id)
        return self.problems


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root", type=Path, default=Path(__file__).resolve().parents[2],
        help="repository root (default: inferred from this script)",
    )
    parser.add_argument("--json", action="store_true", help="emit machine-readable JSON")
    args = parser.parse_args(argv)
    root = args.root.resolve()
    checker = Checker(root)
    problems = checker.run()
    if args.json:
        print(json.dumps([
            {"path": str(p.path.relative_to(root)), "location": p.location, "message": p.message}
            for p in problems
        ], ensure_ascii=False, indent=2))
    elif problems:
        for problem in problems:
            print(problem.render(root), file=sys.stderr)
        print(f"catalog validation failed: {len(problems)} problem(s)", file=sys.stderr)
    else:
        print(
            f"catalog validation passed: {len(checker.vendor_ids)} vendor(s), "
            f"{len(checker.product_ids)} product record(s)"
        )
    return 1 if problems else 0


if __name__ == "__main__":
    raise SystemExit(main())
