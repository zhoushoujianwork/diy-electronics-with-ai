#!/usr/bin/env python3
"""Place only the authored R2 measurement symbols through the typed EasyEDA CLI.

This is a temporary symbol-pin measurement checkpoint, not schematic wiring.
It checks the live page before each batch and stops on any uncertain response.
"""
import argparse
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLAN = ROOT / "source/r2-vin-peripherals-placement-plan.json"
JOURNAL = ROOT / "build/live-20260928/r2-placement-journal.jsonl"


def call(*args):
    proc = subprocess.run(["rtk", "easyeda", *args], capture_output=True, text=True)
    try:
        data = json.loads(proc.stdout)
    except json.JSONDecodeError as exc:
        raise RuntimeError(f"unreadable typed response for {args}: {proc.stdout[:300]} {proc.stderr[:300]}") from exc
    if proc.returncode or not data.get("ok"):
        raise RuntimeError(f"typed command failed for {args}: {proc.stdout[:800]} {proc.stderr[:300]}")
    return data


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--apply", action="store_true", help="write to the connected EasyEDA project")
    args = parser.parse_args()
    plan = json.loads(PLAN.read_text())
    if plan["coordinateState"] != "temporary-off-sheet-measurement":
        raise RuntimeError("unexpected placement plan state")
    if not args.apply:
        print(f"dry plan: {len(plan['placements'])} symbol measurements on pages 1-3")
        return
    JOURNAL.parent.mkdir(parents=True, exist_ok=True)
    for page in (1, 2, 3):
        page_items = [x for x in plan["placements"] if x["page"] == page]
        page_uuid = page_items[0]["pageUuid"]
        current = call("sch", "list", "--project", plan["projectUuid"], "--page", page_uuid,
                       "--stay", "--include-pins", "--include-device-identity")
        components = current["result"]["components"]
        refs = {x.get("designator"): x for x in components if x.get("componentType") == "part"}
        for item in page_items:
            present = refs.get(item["ref"])
            if present:
                actual_device = present.get("device", {}).get("uuid")
                if actual_device != item["deviceUuid"]:
                    raise RuntimeError(f"existing {item['ref']} has unexpected device {actual_device}")
                continue
            response = call("sch", "place", "--project", plan["projectUuid"], "--doc", page_uuid,
                            "--lib", item["libraryUuid"], "--uuid", item["deviceUuid"],
                            "--designator", item["ref"], "--x", str(item["x"]), "--y", str(item["y"]))
            placed = response["result"]
            if placed.get("component", {}).get("designator") != item["ref"]:
                raise RuntimeError(f"placement response lacks confirmed designator for {item['ref']}")
            with JOURNAL.open("a") as out:
                out.write(json.dumps({"ref": item["ref"], "page": page, "response": placed}, ensure_ascii=False) + "\n")
        save = call("sch", "save", "--project", plan["projectUuid"], "--doc", page_uuid)
        if not save.get("result", {}).get("saved"):
            raise RuntimeError(f"page {page} save was not confirmed")
        print(f"page {page}: {len(page_items)} planned symbols, saved")


if __name__ == "__main__":
    main()
