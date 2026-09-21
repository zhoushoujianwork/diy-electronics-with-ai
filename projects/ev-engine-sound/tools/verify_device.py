#!/usr/bin/env python3
"""Bounded serial capture and transition/load exercise. Does not reset the board."""
import argparse
import pathlib
import re
import time
import serial

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("port")
parser.add_argument("--log", required=True)
args = parser.parse_args()
schedule = [
    (0.2, "stop"), (0.5, "status"), (1, "profile inline4"),
    (1.3, "volume 40"), (1.6, "exhaust stock"), (2, "start"),
    (3, "throttle 100"), (5, "exhaust akrapovic"),
    (7, "exhaust yoshimura"), (9, "exhaust tin_can"),
    (11, "exhaust straight"), (12, "throttle 0"),
    (14, "status"), (16, "stop"),
    (19, "profile v12"), (20, "redline 16000"), (21, "start"),
    (23, "throttle 100"), (29, "status"), (42, "status"),
    (44, "throttle 0"), (49, "profile flat6"), (50, "throttle 60"),
    (54, "throttle 0"), (57, "stop"), (58, "volume 60"),
    (59, "exhaust stock"),
    (60, "profile inline4"), (63, "status"),
]
device = serial.Serial()
device.port, device.baudrate = args.port, 115200
device.timeout, device.write_timeout = .05, .5
device.dtr = device.rts = False
device.open()
start, next_ping, step = time.monotonic(), 0, 0
capture = bytearray()
try:
    with open(args.log, "wb") as log:
        while time.monotonic() - start < 67:
            data = device.read(4096)
            if data:
                capture.extend(data)
                log.write(data)
                log.flush()
            elapsed = time.monotonic() - start
            if step < len(schedule) and elapsed >= schedule[step][0]:
                cmd = schedule[step][1]
                device.write((cmd + "\n").encode("ascii"))
                print(f"{elapsed:5.1f}s > {cmd}", flush=True)
                step += 1
            if elapsed >= next_ping:
                device.write(b"ping\n")
                next_ping = elapsed + .45
finally:
    try:
        device.write(b"stop\n")
    finally:
        device.close()

text = capture.decode(errors="replace")
fatal = re.findall(r"stack overflow|Guru Meditation|panic|AUDIO_FAULT|STACK_MARGIN_LOW|fault=1|write_errors=[1-9]|result=INVALID|task_start.*failed", text, re.I)
beats = re.findall(r"HEARTBEAT[^\r\n]*", text)
assert len(beats) >= 30, f"Only {len(beats)} heartbeats"
assert not fatal, fatal
frames = [int(re.search(r"frames=(\d+)", line)[1]) for line in beats]
assert all(b > a for a, b in zip(frames, frames[1:])), "audio stalled or restarted"
for line in beats:
    render = int(re.search(r"render_max_us=(\d+)", line)[1])
    budget = int(re.search(r"block_budget_us=(\d+)", line)[1])
    assert render < budget, f"render exceeded budget: {line}"
for phase in ("STARTING", "ACCEL", "COAST", "IDLE", "STOPPING", "OFF"):
    assert re.search(r"STATE_TRANSITION: AUDIO \w+ -> " + phase, text), phase
assert re.search(r"rpm=1[56][0-9]{3}", text), "high RPM not reached"
assert re.search(r"STATUS[^\n]*version=0\.3\.0", text), "wrong firmware version"
assert "animation=powertrain_rig" in text, "wrong UI renderer"
assert "selector=cycle_buttons" in text, "wrong selector mode"
for exhaust in ("stock", "akrapovic", "yoshimura", "tin_can", "straight"):
    assert re.search(r"UI_SYNC[^\n]*mode=cycle_buttons[^\n]*exhaust=" + exhaust, text), exhaust
assert "AUDIO_READBACK" in text
assert re.search(r"STATUS[^\n]*running=0 rpm=0 throttle=0 volume=60", text), "safe final state missing"
for name in ("audio", "console", "heartbeat", "lvgl"):
    values = [int(x) for x in re.findall(r"stack_" + name + r"=(\d+)", text)]
    assert values and min(values) >= 1024, (name, values)
    print(f"stack_{name}_min={min(values)} B")
print(f"PASS: {len(beats)} heartbeats, transitions, 16000 RPM, zero faults")
for line in text.splitlines():
    if "UI_READBACK" in line or "STATE_TRANSITION: AUDIO" in line:
        print(line)
print(f"Evidence: {pathlib.Path(args.log).resolve()}")
