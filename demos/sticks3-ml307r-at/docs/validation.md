# Validation

## Build

- ESP-IDF 5.5.2 build: passed on 2026-09-27 with `78/esp-ml307` 3.7.5 and
  `78/uart-uhci` 0.4.0. The application binary was `0x80860` bytes, leaving
  50% of the 1 MiB app partition. Firmware commit will be recorded when flashed.
- Real ML307R-DL Tiny carrier detection, SIM registration and TCP: pending.
- Board pinout, UART level and independent modem supply: pending physical inspection.

## Hardware procedure

With the exact PCB revision and supply arrangement recorded, attach antenna and
SIM, then power the Tiny independently. Capture the complete StickS3 serial log.
Check cold start, AT detection, network registration, TCP connection, CSQ updates
and a 30-minute steady run. Measure supply voltage/current while registering and
transmitting. Check reset reason, panic, Guru Meditation, stack overflow, task
creation failure, unexpected USB reconnect and heartbeat continuity. Record the
lowest free stack for `bringup`, `modem_receive` and `modem_event`; each must have
at least 25% and 1024 bytes free. Preserve raw logs privately and commit a
sanitized summary with firmware commit, start/end time and pass/fail criteria.
