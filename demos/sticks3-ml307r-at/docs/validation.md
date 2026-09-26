# Validation

## Build

- ESP-IDF 5.5.2 build: passed on 2026-09-27 with `78/esp-ml307` 3.7.5 and
  `78/uart-uhci` 0.4.0. The initial application binary was `0x80870` bytes;
  the raw-UART diagnostic build was `0x84210` bytes, leaving 48% of the 1 MiB
  app partition. Both were hash-verified when flashed to the StickS3.
- Static driver review found that its network wait can return registration-ready
  without PDP/IP readiness. The Demo now logs `CELL_REGISTERED` separately from
  `TCP_CONNECTED` and backs off when TCP fails; this behavior is build-verified
  but awaits real-device serial evidence.
- StickS3 K150 / ESP32-S3-PICO-1, 8 MiB flash and PSRAM, was identified on
  `/dev/cu.usbmodem212301`. Its previous 8 MiB flash image was backed up outside
  Git before flashing. Initial AT Demo app version `080c78e` booted normally.
  A deliberate serial RTS pulse produced reset reason 11. A 120-second serial
  window showed repeated `MODEM_DETECT_FAILED` timeouts, no registration or TCP,
  no panic, stack overflow, task-start failure or unexpected USB reconnect.
  The UART1/UHCI initialization succeeded. The first raw-UART diagnostic saw
  248 bytes at 921600 bps across two reads but no `OK`; a later deliberate
  StickS3 reset saw zero bytes at all tested rates. This does not prove those
  bytes originated from the modem. Later 70- and 35-second windows still showed
  AT timeouts.
  The `bringup` task's lowest observed free stack was 8956 bytes of 12288
  (73%); modem receive/event task margins remain unmeasured because modem
  detection never succeeded. Raw logs remain outside Git.
- The user reports Tiny BAT disconnected, Tiny VIN supplied independently at
  about 5 V, EN unconnected and its indicator blinking. The exact carrier PCB
  revision, UART idle voltage, power-source current rating and measured peak
  current have not been independently recorded. Physical TX/RX and common-ground
  confirmation is pending. Real modem detection, SIM registration and TCP are
  still pending; the first hardware attempt did not pass them.
- A user-supplied interface table identifies VIN as 5–16 V, TXD/RXD as 3.3 V,
  EN as pulled up to VIN, and BAT as a separate 3.4–4.2 V input that must not
  be powered with VIN. Its pin numbers start at VIN=1, opposite the top-to-bottom
  hole count in the seller image. These supplied documents are not a measured
  electrical check of the user's exact PCB; physical label inspection remains
  pending. EN stays disconnected for bring-up.
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
