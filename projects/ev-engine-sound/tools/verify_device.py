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
    (0.2, "stop"), (0.5, "volume 0"), (0.8, "status"),
    (1, "profile inline4"), (1.6, "exhaust stock"), (2, "start"),
    (3, "throttle 100"), (5, "exhaust akrapovic"),
    (7, "exhaust yoshimura"), (9, "exhaust tin_can"),
    (11, "exhaust straight"), (12, "throttle 0"),
    (18, "status"), (38, "status"), (54, "status"),
    (55, "profile v12"), (56, "redline 16000"), (57, "start"),
    (58, "throttle 100"), (64, "status"), (67, "throttle 0"),
    (69, "stop"), (70, "exhaust stock"),
    (71, "profile inline4"), (72, "status"), (75, "status"),
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
        while time.monotonic() - start < 77:
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
assert re.search(r"STATUS[^\n]*version=0\.4\.3", text), "wrong firmware version"
assert "animation=sticks3_canvas" in text, "wrong UI renderer"
assert "layout=full_width_engine" in text, "wrong UI layout"
assert "selector=cycle_buttons" in text, "wrong selector mode"
assert "menu=pull_down" in text, "wrong settings menu mode"
assert "drawer_auto_close_ms=5000" in text, "wrong drawer auto-close policy"
assert "font_scale=12_28" in text, "wrong firmware font scale"
armed=[int(value) for value in re.findall(r"AUTO_OFF_ARMED center_ms=30000 target_ms=(\d+) phase=IDLE",text)]
assert armed and all(25000<=value<=35000 for value in armed), "random idle dwell outside 25-35 seconds"
stopped=[int(value) for value in re.findall(r"STATE_TRANSITION: RUN -> STOP reason=idle_auto_off idle_ms=(\d+) randomized=1",text)]
assert any(value in armed for value in stopped), "auto-off transition missing"
for exhaust in ("stock", "akrapovic", "yoshimura", "tin_can", "straight"):
    assert re.search(r"UI_SYNC[^\n]*mode=cycle_buttons[^\n]*exhaust=" + exhaust, text), exhaust
assert "AUDIO_READBACK" in text
assert all("pca_out=0x00" in line for line in re.findall(r"AUDIO_READBACK[^\n]*", text)), "amplifier enabled during silent verification"
statuses=re.findall(r"STATUS[^\n]*",text)
assert statuses and re.search(r"running=0 rpm=0 throttle=0 volume=0",statuses[-1]), "safe final state missing"
for name in ("audio", "console", "heartbeat", "lvgl"):
    values = [int(x) for x in re.findall(r"stack_" + name + r"=(\d+)", text)]
    assert values and min(values) >= 1024, (name, values)
    print(f"stack_{name}_min={min(values)} B")
print(f"PASS: {len(beats)} heartbeats, transitions, 16000 RPM, zero faults")
for line in text.splitlines():
    if "UI_READBACK" in line or "STATE_TRANSITION: AUDIO" in line:
        print(line)
print(f"Evidence: {pathlib.Path(args.log).resolve()}")
