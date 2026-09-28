#!/usr/bin/env python3
"""Bind electrical source to genuine typed symbol/text measurements.

Usage: python3 tools/prepare_layout.py <directory-with-pN-symbols-response.json> --version v8
Outputs measured zone inputs, not hand-positioned routing or an Apply queue.
"""
import hashlib
import json
import argparse
from collections import defaultdict
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]


def prepare(raw, version, pages=None):
    design=json.loads((ROOT/"source/design.json").read_text())
    parts=json.loads((ROOT/"source/parts-lock.json").read_text())
    project=json.loads((ROOT/"eda-project.json").read_text())
    zone_of={c["ref"]:c["zone"] for c in design["components"]}
    netzones=defaultdict(set)
    for c in design["components"]:
        for n in c["pins"].values():
            if n:netzones[n].add(c["zone"])
    evidence=[]
    for pg in project["pages"]:
        pn=pg["number"]
        if pages is not None and pn not in pages:
            continue
        source=raw/f'p{pn}-symbols-response.json'
        body=json.loads(source.read_text())
        assert body["ok"] and body["context"]["documentUuid"]==pg["uuid"]
        actual={c["designator"]:c for c in body["result"]["components"] if c["componentType"]=="part"}
        txtsource=raw/f'p{pn}-texts.json';txt=json.loads(txtsource.read_text())
        assert txt["documentId"]==pg["uuid"]
        boxes={c["parentId"]:c for c in txt["designators"]}
        expected={c["ref"]:c for c in design["components"] if c["page"]==pn}
        assert set(actual)==set(expected),(pn,set(actual)^set(expected))
        data=dict(schemaVersion=1,spacing=10,maxCandidates=200000,
                  routing=dict(maxExpandedNodes=1000000,maxReroutes=8),
                  components=[],attachments=[],netPolicies={},zones=[])
        for ref,c in expected.items():
            obs=actual[ref];p=parts[c["lcsc"]]
            assert obs["device"]["uuid"]==p["deviceUuid"],ref
            assert obs["supplierId"]==c["lcsc"],ref
            assert obs["pinsAvailable"] and obs["netlistAvailable"],ref
            assert {v["pinNumber"] for v in obs["pins"]}==set(c["pins"]),ref
            assert obs["primitiveId"] in boxes,ref
            text=boxes[obs["primitiveId"]];assert text["value"]==ref and text["visible"],ref
            p["pins"]=[dict(number=x["pinNumber"],name=x["pinName"]) for x in obs["pins"]]
            p["pinEvidence"]="official-placed-symbol-readback"
            p["edaName"]=obs["device"]["name"]
            pins=[]
            for pin in obs["pins"]:
                net=c["pins"][pin["pinNumber"]]
                pins.append(dict(number=pin["pinNumber"],name=pin["pinName"],net=net or "",
                                 x=pin["x"],y=pin["y"],rotation=pin["rotation"]))
                if net:
                    data["netPolicies"][net]="local_ground" if net=="GND" else (
                        "module_port" if len(netzones[net])>1 else "direct")
            m={k:obs[k] for k in ["x","y","rotation","mirror","bbox"]}
            m.update(designator=ref,value=p["value"],pins=pins,textBboxes=[text["bbox"]])
            data["components"].append(dict(id=c["id"],measurement=m,
                pinStates={k:"nc" for k,v in c["pins"].items() if v is None}))
            if "attachment" in c:
                a=c["attachment"]
                data["attachments"].append(dict(componentId=c["id"],pinNumber=a["pin"],
                    attachTo=dict(componentId="cmp-"+a["owner"],pinNumber=a["ownerPin"])))
        for z in design["zones"]:
            if z["page"]==pn:
                data["zones"].append(dict(id=z["id"],title=z["title"],coreComponentId="cmp-"+z["core"],
                                         componentIds=["cmp-"+v for v in z["members"]]))
        out=ROOT/"source"/f'p{pn}-zones-{version}.json'
        encoded=json.dumps(data,indent=2)+"\n"
        if out.exists():
            assert out.read_text()==encoded, 'Never overwrite a retained layout input'
        else:
            out.write_text(encoded)
        evidence.append(dict(page=pn,documentId=pg["uuid"],parts=len(actual),
            symbolSnapshotSha256=hashlib.sha256(source.read_bytes()).hexdigest(),
            designatorSnapshotSha256=hashlib.sha256(txtsource.read_bytes()).hexdigest(),
            layoutInputSha256=hashlib.sha256(out.read_bytes()).hexdigest(),
            coverage="device UUID, C number, physical symbol pin inventory, measured body/pins/designator geometry",
            electricalConnections="not-yet-applied"))
    (ROOT/"source/parts-lock.json").write_text(json.dumps(parts,ensure_ascii=False,indent=2)+"\n")
    (ROOT/f"measurement-evidence-{version}.json").write_text(json.dumps(evidence,indent=2)+"\n")
    print(f'Bound {sum(x["parts"] for x in evidence)} measured component geometries; electrical wiring still pending.')


if __name__=="__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument('raw',type=Path)
    parser.add_argument('--version',required=True)
    parser.add_argument('--pages',help='Comma-separated page numbers; default all pages')
    args=parser.parse_args()
    assert args.version.isalnum()
    pages={int(x) for x in args.pages.split(',')} if args.pages else None
    prepare(args.raw,args.version,pages)
