#!/usr/bin/env python3
"""Validate saved/reloaded R2 symbol placement and record measured pin evidence."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build/live-20260928"
PLAN_PATH = ROOT / "source/r2-vin-peripherals-placement-plan.json"
PARTS_PATH = ROOT / "source/parts-lock.json"
OUT = ROOT / "r2-vin-peripherals-checkpoint.json"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    plan = json.loads(PLAN_PATH.read_text())
    parts = json.loads(PARTS_PATH.read_text())
    snapshots = {}
    pages = {}
    for page in (1, 2, 3):
        path = BUILD / f"p{page}-r2-readback.json"
        response = json.loads(path.read_text())
        assert response["ok"], path
        result = response["result"]
        assert result["pinsAvailable"] if "pinsAvailable" in result else all(x.get("pinsAvailable", True) for x in result["components"])
        comps = [x for x in result["components"] if x.get("componentType") == "part"]
        refs = [x.get("designator") for x in comps]
        assert len(refs) == len(set(refs)), page
        snapshots[page] = {x["designator"]: x for x in comps}
        pages[str(page)] = {"pageUuid": next(x["pageUuid"] for x in plan["placements"] if x["page"] == page),
                            "partCount": len(comps), "wires": len(result.get("wires", [])),
                            "snapshotSha256": sha(path)}
        assert pages[str(page)]["wires"] == 0, page
    measured = []
    by_code = {}
    for item in plan["placements"]:
        component = snapshots[item["page"]][item["ref"]]
        assert component["supplierId"] == item["lcsc"], item
        assert component["x"] == item["x"] and component["y"] == item["y"], item
        assert component.get("pinsAvailable") and component["pins"], item
        actual_pins = [{"number": p["pinNumber"], "name": p["pinName"]} for p in component["pins"]]
        expected = parts[item["lcsc"]]["pins"]
        assert {x["number"]: x["name"] for x in actual_pins} == {x["number"]: x["name"] for x in expected}, item
        if item["lcsc"] in by_code:
            assert by_code[item["lcsc"]] == actual_pins, item
        by_code[item["lcsc"]] = actual_pins
        measured.append({"ref": item["ref"], "page": item["page"], "lcsc": item["lcsc"],
                         "primitiveId": component["primitiveId"], "pins": actual_pins})
    for code, pins in by_code.items():
        parts[code]["pins"] = pins
        parts[code]["pinEvidence"] = "official-placed-symbol-readback"
    PARTS_PATH.write_text(json.dumps(dict(sorted(parts.items())), ensure_ascii=False, indent=2) + "\n")
    summary = {"status": "live-placed-saved-reloaded-unwired", "projectUuid": plan["projectUuid"],
               "sourcePlan": str(PLAN_PATH.relative_to(ROOT)), "sourcePlanSha256": sha(PLAN_PATH),
               "pages": pages, "placedCount": len(measured), "placed": measured,
               "limitations": ["All R2 VIN additions remain at off-sheet measurement coordinates.",
                               "No page has VIN wires or NC markers; no final layout or DRC has passed."]}
    OUT.write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
    print(f"verified {len(measured)} saved/reloaded VIN peripheral symbols across 3 pages")


if __name__ == "__main__":
    main()
