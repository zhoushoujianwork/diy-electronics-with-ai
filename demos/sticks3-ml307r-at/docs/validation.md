# Validation

## Build

- ESP-IDF 5.5.2 build: passed on 2026-09-27 with `78/esp-ml307` 3.7.5 and
  `78/uart-uhci` 0.4.0. The initial application binary was `0x80870` bytes;
  the first raw-UART diagnostic build was `0x84210` bytes, and the eight-rate
  diagnostic build was `0x84220` bytes, leaving 48% of the 1 MiB app partition.
  These builds were hash-verified when flashed to the StickS3.
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
  about 5 V, EN unconnected and its indicator blinking. They found the original
  TX/RX wiring reversed, corrected it, and subsequently connected the grounds.
  A fresh 120-second capture with Demo app version `eb20f6b` after both changes
  still showed five AT-detection timeouts; the raw UART probe received zero
  bytes at 115200, 921600, 460800, 57600 and 9600 bps. There was no panic,
  unexpected reset or USB reconnect. With Tiny UART disconnected and StickS3
  Hat2 G5 shorted to G6, the same build received all four transmitted `AT\r\n`
  bytes at every tested baud rate; `OK` was absent as expected for a wire loop.
  This verifies the StickS3 UART1 pins and firmware TX/RX path, but does not
  verify either Tiny pin or its AT firmware. With Tiny alone on its independent
  supply and UART disconnected, the user measured EN at about 5 V and TXD idle
  at about 3.6 V relative to Tiny GND. The latter exceeds the seller table's
  3.3 V claim; it prompted a pause in direct Tiny TXD → StickS3 G6 wiring.
  The user later questioned this meter reading and requested the direct retest
  recorded below. The exact carrier PCB revision, power-source current rating
  and measured peak current have not been independently recorded. Real modem
  detection, SIM registration and TCP
  through StickS3 remain pending.
- A separate short DAPLink UART test used `/dev/cu.usbmodem212402` at 115200 bps.
  The user wired Tiny directly to LCKFB DAPLink and used its 5 V pin for this
  bench check. `AT` replied `OK`; `AT+CGMM` returned `ML307R`; `AT+CPIN?`
  returned `READY`; `AT+CEREG?` reported registration state 1 and `AT+COPS?`
  reported CHINA MOBILE. Three signal samples about five seconds apart all
  returned `+CSQ: 31,99` for `AT+CSQ` and `AT+CESQ` RSRQ/RSRP codes
  29/67. `AT+MIPCALL?`
  reported context 1 active with a nonzero IP; the IP was not recorded in Git.
  This confirms the module's AT interface, SIM registration and reported signal
  in that short setup. DAPLink 5 V output current and RX input tolerance were
  not verified, so the run does not establish a suitable long-term modem power
  or logic interface, cellular TCP, or StickS3 interoperability.
- With StickS3 powered by USB-C, the user reported reconnecting G5 to Tiny RXD,
  Tiny TXD directly to G6, and common GND. Tiny's power source and whether its
  DAPLink RX/TX wires were removed were not reconfirmed for this run. Demo app
  `0cf41d4` was flashed and hash-verified. A deliberate reset reported reason
  11; AT detection timed out. The raw probe transmitted four bytes per attempt
  but received **zero bytes** in two attempts at each of 115200, 921600, 460800,
  230400, 57600, 38400, 19200 and 9600 bps. A 120-second post-flash capture
  and 65-second reset capture showed no panic, stack overflow, task-start
  failure or unexpected USB reconnect. Lowest observed `bringup` free stack
  was 8940/12288 B (73%); modem receive/event margins remain unmeasured. This
  rules out a simple baud-rate mismatch for the observed zero-byte path but
  does not yet locate the open or contended connection. No StickS3-side modem
  detection, registration or TCP was observed. Raw logs remain outside Git.
- To check another Hat2 UART pair, Demo `802bba9` routed UART1 TX to GPIO4
  (Hat2 pin 4) and RX to GPIO7 (pin 8). It built under ESP-IDF 5.5.2 as a
  `0x84280`-byte application, was flashed with hash verification, and logged
  `UART_PINS uart=1 tx=4 rx=7` after reset. With the user reporting the wires
  moved to those pins, the 120-second capture still showed AT timeouts and
  zero received bytes in two attempts at every tested baud rate from 9600 to
  921600 bps. `bringup` free stack remained 8940/12288 B; no panic, stack
  overflow or unexpected reset was observed. A physical G4/G7 loopback was
  requested but not completed before the next direction-swap trial. This test
  does not establish whether the alternate pair reaches the Tiny UART.
- After swapping physical UART wires, Demo `1c8dc1a` assigned UART1 TX to
  GPIO7 (Hat2 pin 8) and RX to GPIO4 (pin 4). It built as a `0x84280`-byte
  application and was flashed with hash verification. The reset log confirmed
  `UART_PINS uart=1 tx=7 rx=4`. With the user reporting the swapped Tiny wiring,
  a 120-second serial capture again showed AT timeouts and zero received bytes
  at all eight probe rates. `bringup` free stack remained 8940/12288 B; no
  panic, stack overflow or unexpected reset was observed. This direction swap
  did not establish UART communication; the exact inter-board path is still
  unverified. A DAPLink check of the physical StickS3 TX/RX paths is next.
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
