#!/usr/bin/env python3
"""Check captured serial evidence. A passing log does not prove audible quality."""
import argparse
import re

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("log")
    p.add_argument("--min-heartbeats",type=int,default=60)
    args=p.parse_args()
    with open(args.log,encoding="utf-8",errors="replace") as f:
        text=f.read()
    errors=[]
    if re.search(r"stack overflow|Guru Meditation|panic|AUDIO_FAULT|STACK_MARGIN_LOW|task_start .*failed",text,re.I):
        errors.append("fatal / audio / stack error present")
    if len(re.findall(r"BOOT version=",text))>1:
        errors.append("multiple boots")
    rows=[]
    for line in text.splitlines():
        if "HEARTBEAT" not in line: continue
        fields=dict(re.findall(r"(\w+)=(\d+)",line))
        required=("frames","stack_audio","stack_console","stack_heartbeat","write_errors","render_max_us","fault")
        if not all(k in fields for k in required):
            errors.append("incomplete heartbeat")
            continue
        rows.append(fields)
    if len(rows)<args.min_heartbeats:
        errors.append(f"only {len(rows)} heartbeats; require {args.min_heartbeats}")
    for i,row in enumerate(rows):
        if int(row["write_errors"]) or int(row["fault"]): errors.append("audio error/fault")
        if int(row["render_max_us"])>=8000: errors.append("render exceeds block deadline")
        if min(int(row[k]) for k in ("stack_audio","stack_console","stack_heartbeat"))<1024:
            errors.append("stack margin below 1024 bytes")
        if i and int(row["frames"])<=int(rows[i-1]["frames"]): errors.append("frame continuity failed")
    # Device log timestamps validate duration and reveal heartbeat gaps.
    times=[int(t) for t in re.findall(r"[IW] \((\d+)\).*HEARTBEAT",text)]
    if len(times)!=len(rows): errors.append("missing device heartbeat timestamps")
    if any(b-a>2500 or b<=a for a,b in zip(times,times[1:])): errors.append("heartbeat gap / time reset")
    if len(times)>1 and times[-1]-times[0]<(args.min_heartbeats-1)*900: errors.append("capture duration too short")
    if errors:
        print("FAIL: "+"; ".join(sorted(set(errors))))
        return 1
    print(f"PASS: {len(rows)} heartbeats, increasing frames, no faults, stack margins >=1024 bytes")
    return 0

if __name__=="__main__":
    raise SystemExit(main())
