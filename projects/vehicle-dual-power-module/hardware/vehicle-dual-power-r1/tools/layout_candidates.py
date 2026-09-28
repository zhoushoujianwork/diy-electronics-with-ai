#!/usr/bin/env python3
"""Prepare immutable v3 layout experiments; no live EDA writes.

Supply rails are power symbols; their explicitly owned peripherals still need
physical wires. Allow passive rotation, subject to fresh real-text measurement
before any Apply. Retain every failed report and never combine partial output.
"""
import hashlib
import json
import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RAILS = set('VEH_IN VEH_FUSED VEH_CD VEH_PROT VEH_AON_5V CAR_5V PACK_IN PACK_FUSED BAT_CS BAT_SW BAT_SYS AON_RAW 3V0_AON HOST_SRC MODEM_SRC HOST_REG MODEM_REG HOST_SW HOST_5V TINY_BAT HOST_3V3 MODEM_VIO CHG_REGN CHG_PMID'.split())


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--version', required=True, help='New immutable experiment name, e.g. v6')
    args = parser.parse_args()
    assert args.version.isalnum(), 'Use an alphanumeric version'
    target = ROOT / ('build/layout-' + args.version)
    target.mkdir(parents=True, exist_ok=True)
    manifest = []
    for pn in range(1, 5):
        old = ROOT / f'source/p{pn}-zones-v1.json'
        data = json.loads(old.read_text())
        data['maxCandidates'] = 40000
        data['routing'] = dict(maxExpandedNodes=2000000, maxReroutes=8)
        for name in data['netPolicies']:
            if name in RAILS:
                data['netPolicies'][name] = 'local_power'
        core_ids = {z['coreComponentId'] for z in data['zones']}
        for c in data['components']:
            if c['id'] not in core_ids and len(c['measurement']['pins']) == 2:
                c['allowedRotations'] = [0, 90, 180, 270]
        full = ROOT / f'source/p{pn}-zones-{args.version}.json'
        encoded = json.dumps(data, indent=2) + '\n'
        if full.exists():
            assert full.read_text() == encoded, 'Never overwrite an experiment'
        else:
            full.write_text(encoded)
        for zone in data['zones']:
            ids = set(zone['componentIds'])
            one = dict(data, zones=[zone],
                       components=[c for c in data['components'] if c['id'] in ids],
                       attachments=[a for a in data['attachments'] if a['componentId'] in ids])
            used_nets = {p['net'] for c in one['components'] for p in c['measurement']['pins'] if p['net']}
            one['netPolicies'] = {n: policy for n, policy in data['netPolicies'].items() if n in used_nets}
            source = target / (zone['id'] + '-input.json')
            payload = json.dumps(one, indent=2) + '\n'
            if source.exists():
                assert source.read_text() == payload
            else:
                source.write_text(payload)
            manifest.append(dict(page=pn, zone=zone['id'], source=str(source.relative_to(ROOT)),
                                 sha256=hashlib.sha256(source.read_bytes()).hexdigest()))
    (target / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'Prepared {len(manifest)} independent offline zones; no live change.')


if __name__ == '__main__':
    main()
