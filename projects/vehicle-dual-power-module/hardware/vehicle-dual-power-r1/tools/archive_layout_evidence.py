#!/usr/bin/env python3
"""Retain exact layout inputs and concise honest reports; never mark partial success."""
import hashlib
import json
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    attempts = []
    successful = set()
    all_zones = {z['id'] for z in json.loads((ROOT / 'source/design.json').read_text())['zones']}
    for version in ['v1', 'v3', 'v4', 'v5', 'v6', 'v7']:
        folder = ROOT / ('build/layout-' + version)
        for report in sorted(folder.glob('*-report.json')):
            stem = report.name.removesuffix('-report.json')
            source = (ROOT / f'source/{stem}-zones-v1.json') if version == 'v1' else folder / (stem + '-input.json')
            result = json.loads(report.read_text())
            assert result['sourceSha256'] == sha(source), report
            target = source if version == 'v1' else ROOT / 'source/layout-attempt-inputs' / version / source.name
            target.parent.mkdir(parents=True, exist_ok=True)
            if target.exists():
                assert target.read_bytes() == source.read_bytes(), target
            else:
                shutil.copyfile(source, target)
            item = dict(version=version, name=stem, status=result['status'],
                        input=str(target.relative_to(ROOT)), inputSha256=sha(source),
                        report=str(report.relative_to(ROOT)), reportSha256=sha(report),
                        failureClass=result.get('failureClass'),
                        error=result.get('error', '').split('; feasibilityReport=')[0],
                        globalInfeasibilityProven=result.get('globalInfeasibilityProven', False))
            geometry = folder / (stem + '-geometry.json')
            if result['status'] == 'planned':
                assert geometry.exists(), geometry
                item['geometrySha256'] = sha(geometry)
                successful.add(stem)
            attempts.append(item)
        # An externally stopped process has no tool-issued failure report.
        # Preserve its input and say so rather than inventing budget exhaustion.
        for source in sorted(folder.glob('*-input.json')):
            stem = source.name.removesuffix('-input.json')
            if (folder / (stem + '-report.json')).exists():
                continue
            target = ROOT / 'source/layout-attempt-inputs' / version / source.name
            target.parent.mkdir(parents=True, exist_ok=True)
            if target.exists():
                assert target.read_bytes() == source.read_bytes()
            else:
                shutil.copyfile(source, target)
            attempts.append(dict(version=version, name=stem, status='interrupted-no-result',
                                 input=str(target.relative_to(ROOT)), inputSha256=sha(source),
                                 reason='Stopped remaining duplicate experiments after completed required-zone failures; no tool report or usable geometry was returned.',
                                 globalInfeasibilityProven=False))
    output = dict(status='incomplete', toolVersion='1.8.0',
                  scope='offline bounded zone layout; no final schematic Apply',
                  completeZones=sorted(successful), unresolvedZones=sorted(all_zones - successful),
                  attempts=attempts)
    (ROOT / 'layout-attempts.json').write_text(json.dumps(output, indent=2, ensure_ascii=False) + '\n')
    print(json.dumps(dict(attempts=len(attempts), completeZones=len(successful), unresolvedZones=sorted(all_zones-successful))))


if __name__ == '__main__':
    main()
