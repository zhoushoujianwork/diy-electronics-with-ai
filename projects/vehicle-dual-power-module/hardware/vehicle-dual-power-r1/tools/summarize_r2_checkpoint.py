#!/usr/bin/env python3
"""Check saved/reloaded R2 VIN symbol placement and retain a small evidence summary."""
import hashlib
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLAN = json.loads((ROOT / "source/r2-vin-placement-plan.json").read_text())
EXPECTED_PINS = {
    "U105": 9, "U206": 21, "U308": 12, "U309": 10,
    "SW301": 6, "J309": 2, "J310": 2, "J311": 2,
}
PAGE_FILES = {1: "p1-after-reload.json", 2: "p2-after-reload.json", 3: "p3-after-reload.json"}


def main(snapshot_dir: Path):
    pages = {}
    actual = {}
    for page, filename in PAGE_FILES.items():
        source = snapshot_dir / filename
        raw = source.read_bytes()
        response = json.loads(raw)
        assert response["ok"], (page, response)
        result = response["result"]
        assert not result["wires"], f"Page {page} is no longer the unwired checkpoint"
        pages[page] = {
            "pageUuid": next(p["uuid"] for p in json.loads((ROOT / "eda-project.json").read_text())["pages"] if p["number"] == page),
            "partCount": sum(c.get("componentType") == "part" for c in result["components"]),
            "wires": len(result["wires"]),
            "snapshotSha256": hashlib.sha256(raw).hexdigest(),
        }
        actual.update({c["designator"]: (page, c) for c in result["components"] if c.get("componentType") == "part"})

    placed = []
    for target in PLAN["placements"]:
        page, component = actual[target["ref"]]
        assert pages[page]["pageUuid"] == target["pageUuid"], target["ref"]
        assert component["supplierId"] == target["lcsc"], target["ref"]
        assert (component["x"], component["y"]) == (target["x"], target["y"]), target["ref"]
        assert component["pinsAvailable"] and component["netlistAvailable"], target["ref"]
        assert len(component["pins"]) == EXPECTED_PINS[target["ref"]], target["ref"]
        assert all(p["net"] == "" and not p["noConnected"] for p in component["pins"]), target["ref"]
        placed.append({
            "ref": target["ref"], "page": page, "lcsc": target["lcsc"],
            "primitiveId": component["primitiveId"],
            "pins": [{"number": p["pinNumber"], "name": p["pinName"]} for p in component["pins"]],
        })

    summary = {
        "status": "live-placed-saved-reloaded-unwired",
        "projectUuid": PLAN["projectUuid"],
        "sourcePlan": "source/r2-vin-placement-plan.json",
        "sourcePlanSha256": hashlib.sha256((ROOT / "source/r2-vin-placement-plan.json").read_bytes()).hexdigest(),
        "pages": pages,
        "placed": placed,
        "limitations": ["All eight symbols remain outside the drawing sheet for measurement.", "No VIN wire, NC marker, final layout or DRC is present."],
    }
    destination = ROOT / "r2-vin-checkpoint.json"
    destination.write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
    print(f"Verified {len(placed)} saved/reloaded library parts; wrote {destination.name}")


if __name__ == "__main__":
    assert len(sys.argv) == 2, "usage: summarize_r2_checkpoint.py <snapshot-dir>"
    main(Path(sys.argv[1]))
