#!/usr/bin/env python3
"""Local electronics purchase registry; no browser credentials or network access."""
import argparse
import hashlib
import json
import math
import os
import sqlite3
import sys
from datetime import datetime, timezone
from pathlib import Path


def default_database():
    """Resolve the private database outside the repository."""
    explicit_home = os.environ.get("ELECTRONIC_MATERIALS_HOME")
    if explicit_home:
        return Path(explicit_home).expanduser() / "materials.sqlite3"
    data_home = os.environ.get("XDG_DATA_HOME")
    if data_home:
        return Path(data_home).expanduser() / "electronic-materials/materials.sqlite3"
    return Path.home() / ".local/share/electronic-materials/materials.sqlite3"


DEFAULT = default_database()
SOURCES = ('taobao', 'lcsc')
STATES = ('completed', 'shipped', 'paid', 'unpaid', 'cancelled', 'refunded', 'partial_refund', 'unknown')
UNACQUIRED_STATES = ('unpaid', 'cancelled', 'refunded')
STRINGS = ('source', 'account', 'order_id', 'line_id', 'title', 'unit', 'status', 'evidence', 'observed_at')
OPTIONAL = ('status_raw', 'purchased_at', 'seller', 'model', 'lcsc_code', 'spec', 'package', 'category', 'product_url', 'notes')

def now():
    return datetime.now(timezone.utc).isoformat()

def out(obj):
    print(json.dumps(obj, ensure_ascii=False, indent=2))

def valid_time(value):
    d = datetime.fromisoformat(value.replace('Z', '+00:00'))
    if d.tzinfo is None:
        raise ValueError('observed_at must include timezone')
    return d

def validate(row):
    if not isinstance(row, dict):
        raise ValueError('Every row must be an object')
    unknown = set(row) - set(STRINGS + OPTIONAL + ('quantity', 'aliases'))
    if unknown:
        raise ValueError('Unknown fields: ' + ', '.join(sorted(unknown)))
    for field in STRINGS:
        if not isinstance(row.get(field), str) or not row[field].strip():
            raise ValueError('Missing/non-string field: ' + field)
    for field in OPTIONAL:
        if field in row and not isinstance(row[field], str):
            raise ValueError('Optional field must be a string: ' + field)
    if row['source'] not in SOURCES or row['status'] not in STATES:
        raise ValueError('Invalid source/status')
    valid_time(row['observed_at'])
    if 'quantity' not in row:
        raise ValueError('quantity required (null allowed)')
    q = row['quantity']
    if q is not None and (type(q) not in (int, float) or not math.isfinite(q) or q < 0):
        raise ValueError('quantity must be null or a finite nonnegative number')
    aliases = row.get('aliases', [])
    if not isinstance(aliases, list) or any(not isinstance(v, str) for v in aliases):
        raise ValueError('aliases must be a string array')
    identity = [row[f] for f in ('source', 'account', 'order_id', 'line_id')]
    key = hashlib.sha256(json.dumps(identity, ensure_ascii=False).encode()).hexdigest()[:24]
    return key, json.dumps(row, ensure_ascii=False, sort_keys=True)

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--db', type=Path, default=DEFAULT)
    sub = ap.add_subparsers(dest='cmd', required=True)
    sub.add_parser('status')
    s = sub.add_parser('search'); s.add_argument('query', nargs='?', default='')
    s.add_argument('--source', choices=SOURCES); s.add_argument('--category'); s.add_argument('--status', choices=STATES)
    s.add_argument('--limit', type=int, default=100); s.add_argument('--offset', type=int, default=0)
    i = sub.add_parser('import'); i.add_argument('file', type=Path)
    e = sub.add_parser('export'); e.add_argument('file', type=Path)
    c = sub.add_parser('coverage'); c.add_argument('source', choices=SOURCES)
    c.add_argument('--status', required=True, choices=('not_started', 'blocked', 'partial', 'complete'))
    c.add_argument('--scope', required=True); c.add_argument('--note', default='')
    t = sub.add_parser('stocktake'); t.add_argument('key'); t.add_argument('quantity', type=float)
    t.add_argument('--unit', required=True); t.add_argument('--note', required=True)
    a = ap.parse_args()
    os.umask(0o077)
    a.db.parent.mkdir(parents=True, exist_ok=True)
    db = sqlite3.connect(a.db)
    db.executescript("""
    CREATE TABLE IF NOT EXISTS purchases(key TEXT PRIMARY KEY, data TEXT NOT NULL, updated_at TEXT NOT NULL);
    CREATE TABLE IF NOT EXISTS purchase_history(id INTEGER PRIMARY KEY, key TEXT NOT NULL, data TEXT NOT NULL, imported_at TEXT NOT NULL);
    CREATE TABLE IF NOT EXISTS coverage(source TEXT PRIMARY KEY, status TEXT, scope TEXT, note TEXT, updated_at TEXT);
    CREATE TABLE IF NOT EXISTS stocktakes(id INTEGER PRIMARY KEY, key TEXT NOT NULL, quantity REAL NOT NULL, unit TEXT NOT NULL, note TEXT NOT NULL, observed_at TEXT NOT NULL);
    """)
    def coverage():
        values = {r[0]: dict(zip(('source','status','scope','note','updated_at'), r)) for r in db.execute('SELECT * FROM coverage')}
        return [values.get(source, {'source':source, 'status':'not_started'}) for source in SOURCES]
    def rows():
        result = []
        for key, raw, updated in db.execute('SELECT * FROM purchases ORDER BY key'):
            record = json.loads(raw)
            record.update(key=key, imported_at=updated)
            stock = db.execute('SELECT quantity, unit, note, observed_at FROM stocktakes WHERE key=? ORDER BY id DESC LIMIT 1', (key,)).fetchone()
            record['stocktake'] = dict(zip(('quantity','unit','note','observed_at'), stock)) if stock else None
            result.append(record)
        return result
    if a.cmd == 'import':
        data = json.loads(a.file.read_text(encoding='utf-8-sig'))
        if not isinstance(data, list):
            raise ValueError('Input must be a JSON array')
        validated = [validate(r) for r in data]
        if len({k for k, _ in validated}) != len(validated):
            raise ValueError('Duplicate order-line identities in this batch; inspect the source')
        changed = 0
        excluded = 0
        removed = 0
        with db:
            for (key, raw), row in zip(validated, data):
                if row['status'] in UNACQUIRED_STATES:
                    excluded += 1
                    if db.execute('DELETE FROM purchases WHERE key=?', (key,)).rowcount:
                        removed += 1
                    db.execute('DELETE FROM purchase_history WHERE key=?', (key,))
                    db.execute('DELETE FROM stocktakes WHERE key=?', (key,))
                    continue
                old = db.execute('SELECT data FROM purchases WHERE key=?', (key,)).fetchone()
                if old and old[0] == raw:
                    continue
                if old and valid_time(json.loads(raw)['observed_at']) < valid_time(json.loads(old[0])['observed_at']):
                    raise ValueError('Refusing older observation for ' + key)
                stamp = now()
                db.execute('INSERT INTO purchase_history(key,data,imported_at) VALUES(?,?,?)', (key,raw,stamp))
                db.execute('INSERT INTO purchases VALUES(?,?,?) ON CONFLICT(key) DO UPDATE SET data=excluded.data,updated_at=excluded.updated_at', (key,raw,stamp))
                changed += 1
        out({'input_rows':len(data), 'excluded_unacquired_rows':excluded,
             'removed_unacquired_rows':removed, 'changed_rows':changed,
             'total_rows':db.execute('SELECT COUNT(*) FROM purchases').fetchone()[0]})
    elif a.cmd == 'coverage':
        with db:
            db.execute('INSERT INTO coverage VALUES(?,?,?,?,?) ON CONFLICT(source) DO UPDATE SET status=excluded.status,scope=excluded.scope,note=excluded.note,updated_at=excluded.updated_at', (a.source,a.status,a.scope,a.note,now()))
        out(coverage())
    elif a.cmd == 'status':
        records = rows()
        latest = {}
        for source in SOURCES:
            source_rows = [r for r in records if r['source'] == source and r.get('purchased_at')]
            if source_rows:
                newest = max(source_rows, key=lambda r: r['purchased_at'])
                latest[source] = {'purchased_at':newest['purchased_at'], 'order_id':newest['order_id']}
            else:
                latest[source] = None
        out({'database':str(a.db), 'purchase_lines':len(records), 'orders':len({(r['source'],r['account'],r['order_id']) for r in records}), 'lines_with_stocktake':sum(r['stocktake'] is not None for r in records), 'latest_purchase':latest, 'coverage':coverage(), 'notice':'购买记录不等于现存库存；零记录不表示没有购买。'})
    elif a.cmd == 'search':
        if a.limit < 1 or a.offset < 0:
            raise ValueError('limit must be positive and offset nonnegative')
        fields = ('title','model','lcsc_code','spec','package','category','seller','notes','order_id')
        results=[]
        for r in rows():
            haystack = ' '.join(str(r.get(f,'')) for f in fields) + ' ' + ' '.join(r.get('aliases',[]))
            if a.source and r['source'] != a.source: continue
            if a.status and r['status'] != a.status: continue
            if a.category and a.category.casefold() not in r.get('category','').casefold(): continue
            if all(term in haystack.casefold() for term in a.query.casefold().split()): results.append(r)
        results.sort(key=lambda r:r.get('purchased_at',''), reverse=True)
        out({'matched':len(results), 'offset':a.offset, 'coverage':coverage(), 'results':results[a.offset:a.offset+a.limit], 'notice':'quantity为原订单量；stocktake为该批次最近盘点，null表示未盘点。'})
    elif a.cmd == 'stocktake':
        if not math.isfinite(a.quantity) or a.quantity < 0 or not a.note.strip():
            raise ValueError('Nonnegative finite quantity and explanatory note required')
        record=db.execute('SELECT data FROM purchases WHERE key=?',(a.key,)).fetchone()
        if not record: raise ValueError('Unknown purchase-line key')
        if json.loads(record[0])['unit'] != a.unit: raise ValueError('Unit differs from order; clarify conversion first')
        with db:
            db.execute('INSERT INTO stocktakes(key,quantity,unit,note,observed_at) VALUES(?,?,?,?,?)', (a.key,a.quantity,a.unit,a.note,now()))
        out({'key':a.key,'quantity':a.quantity,'unit':a.unit,'recorded':True})
    elif a.cmd == 'export':
        payload={'exported_at':now(),'coverage':coverage(),'records':rows()}
        with a.file.open('x',encoding='utf-8') as f:
            json.dump(payload,f,ensure_ascii=False,indent=2)
        out({'export':str(a.file),'records':len(payload['records'])})
    db.close()

if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, sqlite3.Error) as exc:
        print('Error: ' + str(exc), file=sys.stderr)
        sys.exit(1)
