#!/usr/bin/env python3
"""Split the VIN output capacitor bank into its own named schematic zone.

This changes schematic drawing ownership only. Pin/net and NC data stay in the
canonical connectivity source, and no EasyEDA document is edited here.
"""

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "source/p3-zones-v9.json"
TARGET = ROOT / "source/p3-zones-v10.json"
CORE = {"cmp-U309", "cmp-R331", "cmp-R332", "cmp-C336"}
BANK = {"cmp-C337", "cmp-C338"}


def main():
    data = json.loads(SOURCE.read_text())
    zones = data["zones"]
    original = next(z for z in zones if z["id"] == "vin-load-switch")
    assert set(original["componentIds"]) == CORE | BANK
    index = zones.index(original)
    zones[index:index + 1] = [
        {
            **original,
            "id": "vin-load-core",
            "componentIds": [c for c in original["componentIds"] if c in CORE],
        },
        {
            "id": "vin-load-output-bank",
            "title": "VIN 5V OUTPUT CAPACITOR BANK",
            "coreComponentId": "cmp-C338",
            "componentIds": [c for c in original["componentIds"] if c in BANK],
            "placement": {"samePageAs": "vin-load-core", "preferAdjacent": True},
        },
    ]
    data["attachments"] = [
        a for a in data["attachments"] if a["componentId"] not in BANK
    ]
    data["attachments"].append({
        "componentId": "cmp-C337", "pinNumber": "1",
        "attachTo": {"componentId": "cmp-C338", "pinNumber": "1"},
    })
    before = {c["id"]: [(p["number"], p["net"]) for p in c["measurement"]["pins"]]
              for c in json.loads(SOURCE.read_text())["components"]}
    after = {c["id"]: [(p["number"], p["net"]) for p in c["measurement"]["pins"]]
             for c in data["components"]}
    assert before == after
    members = [i for z in zones for i in z["componentIds"]]
    assert len(members) == len(set(members)) == len(data["components"])
    encoded = json.dumps(data, indent=2) + "\n"
    if TARGET.exists():
        assert TARGET.read_text() == encoded, "Refuse to overwrite a changed candidate"
    else:
        TARGET.write_text(encoded)
    print(f"Prepared {TARGET.name}; {len(zones)} zones; pin/net assignments unchanged")


if __name__ == "__main__":
    main()
