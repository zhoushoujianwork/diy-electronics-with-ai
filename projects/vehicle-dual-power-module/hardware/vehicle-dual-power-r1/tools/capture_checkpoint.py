#!/usr/bin/env python3
"""Save/reload/read the actual measurement-stage project, never claim wiring.

Uses only serial typed CLI calls. A failed/partial operation stops the capture.
Raw snapshots and native archive are local artifacts excluded from Git.
"""
import hashlib
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    project = json.loads((ROOT / 'eda-project.json').read_text())
    design = json.loads((ROOT / 'source/design.json').read_text())
    raw = ROOT / 'raw/checkpoint-20260928'
    raw.mkdir(parents=True, exist_ok=True)
    print(json.dumps(project), flush=True)
    results = []
    for page in project['pages']:
        pn, uuid = page['number'], page['uuid']
        summary = dict(page=pn, documentId=uuid, state='measurement-stage-unwired')
        steps = [
            ('save', ['sch', 'save', '--doc', uuid]),
            ('reload', ['doc', 'reload', uuid, '--json']),
            ('read', ['sch', 'list', '--page', uuid, '--stay', '--include-device-identity',
                      '--include-pins', '--include-bbox', '--include-wires', '--include-page-primitives']),
        ]
        for label, args in steps:
            output = raw / f'p{pn}-{label}.json'
            assert not output.exists(), f'Refusing overwrite: {output}'
            proc = subprocess.run(['rtk', 'easyeda', *args, '--project', project['projectUuid']],
                                  capture_output=True, text=True, timeout=100)
            output.write_text(proc.stdout)
            (raw / f'p{pn}-{label}.stderr.txt').write_text(proc.stderr)
            assert proc.returncode == 0, (pn, label, proc.stderr[-1000:])
            data = json.loads(proc.stdout)
            assert data.get('ok', True), (pn, label, data)
            summary[label + 'Sha256'] = hashlib.sha256(output.read_bytes()).hexdigest()
            if label == 'read':
                actual = {c['designator']: c for c in data['result']['components'] if c['componentType'] == 'part'}
                expected = {c['ref']: c for c in design['components'] if c['page'] == pn}
                assert actual.keys() == expected.keys()
                for ref, c in actual.items():
                    assert c['supplierId'] == expected[ref]['lcsc'], ref
                    assert c['pinsAvailable'] and c['netlistAvailable'], ref
                    assert {p['pinNumber'] for p in c['pins']} == set(expected[ref]['pins']), ref
                summary['parts'] = len(actual)
                summary['unconnectedPins'] = sum(p['net'] == '' for c in actual.values() for p in c['pins'])
                summary['namedPins'] = sum(bool(p['net']) for c in actual.values() for p in c['pins'])
                summary['ncPins'] = sum(bool(p['noConnected']) for c in actual.values() for p in c['pins'])
        results.append(summary)
        print(json.dumps(summary), flush=True)
    report = dict(status='live-measurements-persisted-wiring-incomplete', pages=results)
    (ROOT / 'checkpoint-evidence.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
