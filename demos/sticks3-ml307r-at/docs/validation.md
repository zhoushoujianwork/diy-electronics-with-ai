# Validation

## Build

- ESP-IDF 5.5.2 build: passed on 2026-09-27 with `78/esp-ml307` 3.7.5 and
  `78/uart-uhci` 0.4.0. The revised application binary was `0x80870` bytes, leaving
  50% of the 1 MiB app partition. Rebuilt successfully after the wiring review;
  firmware commit will be recorded when flashed.
- Static driver review found that its network wait can return registration-ready
  without PDP/IP readiness. The Demo now logs `CELL_REGISTERED` separately from
  `TCP_CONNECTED` and backs off when TCP fails; this behavior is build-verified
  but awaits real-device serial evidence.
- Real ML307R-DL Tiny carrier detection, SIM registration and TCP: pending.
- A user-supplied interface table identifies VIN as 5–16 V, TXD/RXD as 3.3 V,
  EN as pulled up to VIN, and BAT as a separate 3.4–4.2 V input that must not
  be powered with VIN. Its pin numbers start at VIN=1, opposite the top-to-bottom
  hole count in the seller image. These supplied documents are not a measured
  electrical check of the user's exact PCB; independent modem supply and
  physical label inspection remain pending. EN stays disconnected for bring-up.
  StickS3 Hat2 BAT is not accepted as a Tiny BAT supply for network testing
  because its 4G transmit-current capacity is undocumented.

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
