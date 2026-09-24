#!/usr/bin/env python3
"""Bounded UART capture; raw logs belong in a local ignored/private directory."""
import argparse
import json
import re
import time
from pathlib import Path
import serial


def summarize(text, require_streaming=False):
    fatal = re.findall(r".*(?:Guru Meditation|panic'ed|stack overflow|TASK_START_FAILED|Brownout|assert failed).*", text, re.I)
    heartbeats = [dict(re.findall(r"(\w+)=(\S+)", line)) for line in text.splitlines() if "HEARTBEAT ms=" in line]
    stats = [dict(re.findall(r"(\w+)=(\d+)", line)) for line in text.splitlines() if "AUDIO_STATS " in line]
    stacks = [dict(re.findall(r"(\w+)=(\d+)", line)) for line in text.splitlines() if "STACK synth=" in line]
    failures = []
    if fatal:
        failures.append("fatal/reset symptoms in log")
    if len(heartbeats) < 10:
        failures.append("fewer than 10 heartbeats")
    for a, b in zip(heartbeats, heartbeats[1:]):
        delta = int(b["ms"]) - int(a["ms"])
        if delta <= 0 or delta > 2500:
            failures.append("heartbeat reset or gap")
        if int(b["frames"]) <= int(a["frames"]):
            failures.append("synthesis frame counter stalled")
    if any(int(s["errors"]) or int(s["dropped"]) for s in stats):
        failures.append("API error or event queue overflow")
    minimum = {k: min(int(s[k]) for s in stacks) for k in ("synth", "manager", "console", "heartbeat")} if stacks else {}
    if not minimum or any(v < 1024 for v in minimum.values()):
        failures.append("missing stack measurement or less than 1024 bytes margin")
    streamed = [h for h in heartbeats if h["connected"] == "1" and h["streaming"] == "1"]
    if require_streaming and len(streamed) < 30:
        failures.append("fewer than 30 streaming heartbeats")
    if require_streaming and len(stats)>1 and int(stats[-1]["underrun_bytes"]) > int(stats[0]["underrun_bytes"]):
        failures.append("PCM underrun during capture")
    return {"pass": not failures, "failures": sorted(set(failures)), "heartbeats": len(heartbeats),
            "streaming_heartbeats": len(streamed), "stack_min_bytes": minimum,
            "render_max_us": max((int(s["render_max_us"]) for s in stats), default=0),
            "heap_min_bytes": min((int(s["heap"]) for s in stats), default=0),
            "peak_max": max((int(h["peak"]) for h in heartbeats), default=0),
            "boot_presses": text.count("BOOT_BUTTON PRESS"), "boot_releases": text.count("BOOT_BUTTON RELEASE"),
            "final": heartbeats[-1] if heartbeats else {}, "fatal": fatal[:3]}


def main():
    p = argparse.ArgumentParser()
    p.add_argument("port")
    p.add_argument("--log", type=Path, required=True)
    p.add_argument("--seconds", type=float, default=120)
    p.add_argument("--exercise", action="store_true", help="run serial throttle/profile sequence")
    p.add_argument("--reset", action="store_true", help="pulse EN before capture; release physical BOOT first")
    p.add_argument("--require-streaming", action="store_true")
    args = p.parse_args()
    args.log.parent.mkdir(parents=True, exist_ok=True)
    port = serial.Serial(port=None, baudrate=115200, timeout=.05, write_timeout=1)
    port.dtr = False
    port.rts = False
    port.port = args.port
    port.open()
    schedule = [(1, "tasks")]
    if args.exercise:
        schedule += [(3, "profile twin270\nvolume 20\nstart\nthrottle 100"), (10, "throttle 0"),
                     (17, "profile v12\nredline 16000\nstart\nthrottle 100"), (28, "throttle 0"),
                     (35, "exhaust tin_can\nstart\nthrottle 60"), (43, "exhaust straight"),
                     (50, "stop"), (54, "profile invalid\nthrottle 101\nvolume nan"),
                     (57, "profile twin270\nexhaust stock\nvolume 20\nstop\ntasks")]
    collected = bytearray()
    try:
        if args.reset:
            port.rts = True
            time.sleep(.1)
            port.rts = False
        start = time.monotonic()
        with args.log.open("wb") as f:
            while time.monotonic() - start < args.seconds:
                elapsed = time.monotonic() - start
                while schedule and elapsed >= schedule[0][0]:
                    _, cmd = schedule.pop(0)
                    port.write((cmd + "\n").encode())
                data = port.read(port.in_waiting or 1)
                if data:
                    f.write(data)
                    f.flush()
                    collected.extend(data)
    finally:
        port.write(b"stop\n")
        port.close()
    summary = summarize(collected.decode(errors="replace"), args.require_streaming)
    args.log.with_suffix(".summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2))
    print(json.dumps(summary, ensure_ascii=False, indent=2))
    raise SystemExit(0 if summary["pass"] else 1)


if __name__ == "__main__":
    main()
