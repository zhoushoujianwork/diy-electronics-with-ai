#!/usr/bin/env python3
"""Bounded, read-only serial evidence capture; status queries do not feed watchdog.

Optionally wait for an app USB PID at a previously identified physical location.
Never resets, flashes, starts the engine or silently reconnects after a drop.
"""
import argparse
import time
import serial
from serial.tools import list_ports

p = argparse.ArgumentParser(description=__doc__)
target = p.add_mutually_exclusive_group(required=True)
target.add_argument("--port")
target.add_argument("--usb-location")
p.add_argument("--usb-pid", type=lambda s: int(s, 0), default=0x4001)
p.add_argument("--wait", type=float, default=30)
p.add_argument("--seconds", type=float, default=30)
p.add_argument("--log", required=True)
args = p.parse_args()
deadline = time.monotonic() + args.wait
port = args.port
while not port and time.monotonic() < deadline:
    matches = [x.device for x in list_ports.comports()
               if x.vid == 0x303A and x.pid == args.usb_pid
               and x.location == args.usb_location]
    if len(matches) == 1:
        port = matches[0]
        break
    time.sleep(.1)
if not port:
    raise SystemExit("App port not found at the confirmed USB location")
d = serial.Serial()
d.port, d.baudrate = port, 115200
d.timeout, d.write_timeout = .1, .5
d.dtr = d.rts = False
d.open()
print(f"CAPTURE port={port} seconds={args.seconds}", flush=True)
start, next_status = time.monotonic(), 1
try:
    with open(args.log, "wb") as log:
        while time.monotonic() - start < args.seconds:
            data = d.read(4096)
            if data:
                log.write(data)
                log.flush()
                print(data.decode(errors="replace"), end="", flush=True)
            elapsed = time.monotonic() - start
            if elapsed >= next_status:
                d.write(b"status\n")
                next_status = elapsed + 5
finally:
    d.close()
print(f"\nCAPTURE_COMPLETE log={args.log}", flush=True)
