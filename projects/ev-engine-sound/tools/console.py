#!/usr/bin/env python3
"""Interactive USB serial controls with keepalive; never resets or flashes."""
import argparse
import queue
import sys
import threading
import time

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port")
    parser.add_argument("--log", default="engine-voice.log")
    args = parser.parse_args()
    import serial
    commands = queue.Queue()
    def input_loop():
        for line in sys.stdin:
            commands.put(line.strip())
        commands.put("quit")
    threading.Thread(target=input_loop, daemon=True).start()
    device = serial.Serial()
    device.port = args.port
    device.baudrate = 115200
    device.timeout = .05
    device.write_timeout = .5
    device.dtr = False
    device.rts = False
    device.open()
    print("Commands: profile single|twin270|triple270|inline5|flat6|crossplane8|v10|v12, start, stop, throttle 0..100, rpm N, volume 0..100, status, quit")
    try:
        with open(args.log, "ab") as log:
            keepalive = 0
            while True:
                # Capture before sending controls; log includes raw device output.
                data = device.read(4096)
                if data:
                    log.write(data)
                    log.flush()
                    sys.stdout.write(data.decode(errors="replace"))
                    sys.stdout.flush()
                try:
                    cmd = commands.get_nowait()
                except queue.Empty:
                    cmd = None
                if cmd == "quit":
                    break
                if cmd:
                    device.write((cmd+"\n").encode("ascii"))
                if time.monotonic() >= keepalive:
                    device.write(b"ping\n")
                    keepalive = time.monotonic()+.5
    except KeyboardInterrupt:
        pass
    finally:
        try:
            device.write(b"stop\n")
        finally:
            device.close()

if __name__ == "__main__":
    main()
