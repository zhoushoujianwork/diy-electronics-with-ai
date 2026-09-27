# Validation record

## ML307R-DL Tiny 4G revision

- ESP-IDF 5.5.2 firmware build and host GNSS, telemetry queue/payload and binding tests passed on
  2026-09-27. This build uses ML307R-DL Tiny AT/TCP, ESP32-side TLS hostname and public-chain
  verification, cellular UDP SNTP, GPS on UART2, and separate five-second sampling and MQTT uplink
  tasks. Static driver review found that registration-ready can be returned before PDP receives an IP;
  the application now requires an active `MIPCALL` context with a nonzero IP before reporting
  `cell_data` or opening sockets. This change builds, but has no real-modem serial evidence.
  Build output and host tests do not prove electrical or network operation.
- A user-supplied seller image of the ML307R-DL Tiny shows six right-side holes labelled, top to bottom
  with the shield text upright, BAT/EN/RX/TX/GND/VIN. A later user-supplied interface table numbers
  these in reverse from VIN=1 to BAT=6, specifies VIN 5–16 V and 3.3 V TXD/RXD, and says EN is pulled
  up to VIN; BAT takes 3.4–4.2 V and must not be powered with VIN. The screenshot includes device
  identifiers and stays outside Git. These are supplied carrier documents, not physical measurements
  or independently sourced catalog facts. The user first reported StickS3 Hat2 EXT_5V to Tiny VIN,
  then moved Tiny BAT to StickS3 Hat2 BAT pin 11 with Tiny VIN disconnected. Neither StickS3 rail
  has a documented 4G transmit-current allowance; the latter connection was flagged for removal.
  The user later disconnected Tiny BAT and reported an independent roughly 5 V VIN supply,
  a blinking indicator and EN unconnected. The exact PCB markings, UART idle voltage and
  transmit current remain unverified. The independent AT/TCP Demo was flashed on 2026-09-27;
  StickS3 booted, but no valid AT response was detected in the initial serial windows.
  The user corrected reversed TX/RX wiring and connected a shared GND, but a new
  120-second serial capture with Demo `eb20f6b` still received zero raw UART
  bytes and timed out on AT detection. With Tiny UART disconnected, a Hat2 G5/G6
  loopback returned all four sent bytes at every tested baud rate, verifying
  the StickS3-side UART path; Tiny-side AT communication remains unverified.
  With Tiny independently powered and UART disconnected, the user measured
  EN≈5 V and Tiny TXD idle≈3.6 V relative to Tiny GND. Direct Tiny TXD to
  StickS3 GPIO6 was paused pending UART-suitable level shifting or a measured
  divider; the seller's 3.3 V UART claim has not matched this first reading.
  In a separate short DAPLink UART check at 115200 bps, the same Tiny answered
  `AT`, identified as `ML307R`, had SIM READY, registered to CHINA MOBILE, and
  returned CSQ 31/99 plus CESQ RSRQ/RSRP codes 29/67 in three samples. Its data
  context was active with a nonzero IP. The user powered Tiny from DAPLink 5 V
  for that check; neither the DAPLink output current nor RX input tolerance was
  verified. This establishes module-side AT, SIM and signal only for that short
  setup, not StickS3 4G uplink or sustained transmit power.
  The user later questioned the voltage reading and requested a direct UART
  retest. With StickS3 powered by USB-C and direct G5→Tiny RXD, Tiny TXD→G6,
  and common GND reported, Demo `0cf41d4` was flashed and hash-verified.
  Tiny's supply and DAPLink UART isolation were not reconfirmed for this run.
  The new probe transmitted `AT` but received zero bytes at all eight tested
  rates (9600–921600 bps). A 120-second post-flash and 65-second reset capture
  showed repeated AT timeouts without unexpected resets, panic or stack
  overflow; `bringup` free stack was at least 8940/12288 B. This still does not
  establish StickS3-side modem communication or diagnose the exact wiring fault.
  A separate AT Demo build `802bba9` then tried StickS3 Hat2 GPIO4/TX and
  GPIO7/RX with the user reporting moved wires. A 120-second capture again
  received zero bytes across the eight tested baud rates and showed no panic
  or unexpected reset. This alternate-pin test has not verified the Tiny link;
  the integrated project still uses GPIO5/6.
  A second alternate Demo `1c8dc1a` swapped directions to GPIO7/TX and
  GPIO4/RX, with the user reporting the corresponding wire move. It likewise
  received zero AT bytes during a 120-second capture without abnormal resets
  or panic. These tests have not identified a functional StickS3-to-Tiny UART
  connection; the integrated GPS/MotoBox 4G build remains unflashed.
  A following one-way check connected StickS3 G7 to DAPLink RX. Tiny UART
  removal was not reconfirmed. DAPLink received 22 exact `AT\r\n` sequences
  at 115200 bps over 30 seconds, verifying the Demo's physical G7 transmit
  path. Both USB devices were on the same computer without a direct GND jumper;
  the return
  path was not traced. At that point G4 receive and the Tiny interconnect
  remained unverified.
  A following DAPLink TX→StickS3 G4 injection sent 144 bytes of synthetic
  `OK` responses after the first modem timeout. The 115200-bps raw probe
  received all 144 bytes and reported `ok=1`, verifying G4 receive separately.
  This is an injected response, not a modem reply. Tiny communication remains
  unverified.
  In a subsequent one-way test, StickS3 G7 drove the Tiny socket formerly
  used by DAPLink TX while DAPLink RX monitored Tiny's reply socket. With
  StickS3 G4 left open, DAPLink read 11 complete `OK` responses in the first
  11 seconds and 22 over 30 seconds at 115200 bps. This proves the modem
  receives StickS3 AT and replies on that Tiny pin; StickS3 receiving the reply
  is the remaining UART check.
  After GPIO4/RX was connected to Tiny TXD, the independent Demo `1c8dc1a`
  completed four AT identification, CHINA MOBILE registration (CSQ 31) and
  TCP connections to its public test endpoint over a 120-second serial run.
  A follow-up Demo `324d9ef` completed two more cycles in 50 seconds and
  measured active free stack at 9580/12288 B for `bringup`, 1096/2048 B for
  `modem_receive` and 3364/6144 B for `modem_event`. No panic, unexpected
  reset or USB reconnect was observed. This is hardware evidence for the
  isolated modem path only; power/current peaks and integrated GPS/MQTT
  behavior remain unmeasured.
  At that point, registration and TCP had been verified only in the isolated
  Demo; the integrated GPS/MotoBox 4G uplink had not been flashed.
  The first integrated cellular build used `mqtts://` on port 8883. In a
  150-second capture it registered and obtained UDP network time, but all 12
  MQTT TCP connection attempts failed; 27 telemetry frames accumulated in RAM.
  A development-computer check also timed out on public port 8883, while the
  MotoBox port 443 `/mqtt` WebSocket upgrade returned HTTP 101. The firmware
  then gained a WSS wrapper around the existing ESP32-side, certificate-verified
  TLS transport; this preserves MQTT QoS 1 and requires no Wi-Fi.
  With that build flashed on StickS3 K150, GPS Unit v1.1 and the same separately
  powered ML307R-DL Tiny, a 150-second follow-up serial capture observed
  sequences 3–32 sampled, published and PUBACKed in order. Fifteen heartbeats
  reported GPS NMEA online, no indoor fix, trusted time, MQTT connected, zero
  queue and zero drops at the end. No panic, stack overflow, task-start failure,
  abnormal reset or USB reconnect appeared. Lowest observed free stacks in
  bytes were GPS 1480, cellular 9552, modem receive 1276, modem event 3336,
  MQTT 8784, telemetry 2400, uplink 4936, UI 4204, voice 3308 and heartbeat
  1264; each exceeded 25% of its configured stack and 1024 B. A read-only
  MotoBox SQL query later found the first 128 frames of the USB-reset run in
  continuous ingestion order with original `ext.ts_ms` preserved and `gsm`
  capability/state present. Raw serial, device ID and SQL details stay outside Git.
  That short run does not establish long-term stability or outage recovery.
  A subsequent 31-minute USB-reset run used firmware source `d4de2aa` and the
  locally configured WSS credentials; the flashed application image SHA-256 was
  `a2c4428dc4cc93bcdf11953c1a3deb74b36e08bc564089480e0ef9630182c0f2`.
  The hardware was StickS3 K150, Unit GPS v1.1 and the same ML307R-DL Tiny,
  with Tiny on a separate 5 V supply, shared ground, GPIO7/4 UART and EN open.
  The supply's current rating, voltage during transmission and UART high level
  were not measured. The requested USB reset reported reason 11; this was not
  a full power-off cold start. Serial recorded 185 heartbeats spanning 1851 s,
  GPS NMEA online without an indoor fix, trusted cellular time, MQTT connected,
  Wi-Fi off, and sequences 1–372 sampled, published and PUBACKed in order.
  Final queue and drop counts were zero. One expected reset at the test start
  was the only reset; there was no panic, Guru Meditation, stack overflow,
  task-start failure, MQTT error or USB reconnect. Minimum free stack in bytes:
  GPS 1476/4096, cellular 9552/12288, modem receive 1260/2048, modem event
  3352/6144, MQTT 8892/12288, telemetry 2412/6144, uplink 4804/8192,
  UI 4172/8192, voice 3308/4096 and heartbeat 1276/4096. Every measured task
  retained at least 25% and 1024 B. A read-only MotoBox SQL check found all
  372 sequences once and in ingestion order, no backwards sampling time,
  `ext.ts_ms` equal to the stored original `ts_ms` in every frame, 4164–5802 ms
  between samples and 344–3116 ms ingestion delay. Every frame advertised
  `gsm`, `modules.gsm=true`, `modules.wifi=false` and CSQ 31; none contained
  an indoor location fix. The raw log and database identifiers remain outside Git.
  Negative checks then used temporary, Git-ignored test configurations. With
  a certificate-invalid host on the same service IP, 100 seconds recorded 14
  `TLS_FAILED code=-9984 verify=0x4` events, no `TLS_VERIFIED`, MQTT connection
  or PUBACK, and continued sampling/heartbeats without panic. With the valid
  WSS host but an invalid password, 100 seconds recorded nine verified TLS
  handshakes followed by nine broker `not authorized` refusals, no MQTT
  connection or PUBACK, and continued sampling/heartbeats without panic.
  Reflashing between these checks discarded their RAM-only queues as expected.
  The valid private configuration was restored from an outside-Git backup and
  rebuilt before the final flash. Its application image SHA-256 was
  `9cd05dc675367ba6fc0bc76d4bac0f5a1c481699d742a61200d9243b593ddf0b`.
  A 150-second post-restore USB-reset capture showed modem registration,
  cellular time, verified TLS, MQTT connection, 29 consecutive samples and
  29 PUBACKs, with 15 heartbeats and no MQTT error, panic or stack overflow.
  A read-only MotoBox query found all 29 restored-run frames in the database.
  Deliberate two-minute link loss/recovery,
  full power-off cold start, outdoor motion, power peaks, UART level and visual
  screen/button checks remain pending.
  The former complete 8 MiB StickS3 flash image was saved privately outside Git before the
  Demo flash and is the immediate device rollback point. The existing Wi-Fi firmware and
  all hardware evidence below remain software rollback references.
- Pending 4G acceptance: 30-minute real-device run with cold start, GNSS/LCD/button concurrency,
  two-minute cellular outage/recovery and outdoor movement; MotoBox sequence and original sample-time
  check; wrong certificate and wrong credential rejection; signal 99 when unknown; voltage/current
  peaks; task margins of at least 25% and 1024 B for GPS, sampler, uplink, MQTT, modem orchestration,
  modem receive/event, UI, voice and heartbeat. Capture full serial and private route outside Git.

## Current state

- On the second outdoor run on 2026-09-24, the `fec6e12` firmware reached its first backend-confirmed
  stationary GNSS fix at 14:51:19 UTC. A read-only SQL check through 15:01:45 UTC found 126 consecutive
  valid `GNSS` location frames, sequences 86–211, with seven to eleven GGA-used satellites and HDOP 1.4–7.1.
  Recent frames reported GGA quality 1. Earlier GSV telemetry had climbed to a six-satellite peak in a
  single constellation report and peak SNR 45, establishing reception before the fix. No coordinates or
  device identifier are included here. The broader telemetry window contains a sequence reset from 78 to 1
  at 14:44:13 UTC; without an outdoor serial capture its cause cannot be determined. This evidence proves
  a stationary outdoor GNSS fix and sustained MQTT ingestion for the observed interval, not a moving track,
  location accuracy against ground truth, or absence of reboot during the whole outdoor session.
- GSV visibility firmware `fec6e12` was built and hash-verified on the same StickS3 K150 + Unit GPS
  v1.1 / U032-V11 with USB-C power and Grove 5 V. Two indoor serial windows spanning about 140 seconds
  recorded 14 heartbeats; after SNTP established trusted time, 13 five-second telemetry frames
  were published and acknowledged with queue depth and drops at zero. GSV sentences passed checksum and
  parser validation, but the indoor peaks were zero satellites in view and zero signal-to-noise ratio;
  GGA also reported zero satellites used. SQL stored `gsv_seen=true`, `gsv_peak_in_view=0` and
  `gsv_peak_snr=0` alongside `position_source=NONE`. No panic, stack overflow, Guru Meditation, reset
  or USB reconnect was observed. Minimum free stack in this run was GPS 1480 B, telemetry 2624 B,
  UI 4164 B, voice 3308 B and heartbeat 1584 B. Raw logs remain private at
  `/tmp/sticks3-gsv-diagnostic-20260924.log` and `/tmp/sticks3-gsv-steady-20260924.log`. The GSV peaks
  are maxima since boot, not current satellite counts. The outdoor result is recorded above.
- Diagnostic firmware from `fd281f6` was built with ESP-IDF 5.5.2 and hash-verified on the StickS3 K150
  with Unit GPS v1.1 / U032-V11, powered over USB-C with Grove 5 V enabled. A 95-second indoor serial
  capture after flashing recorded nine continuous heartbeats, 19 telemetry publications and 19 MQTT ACKs,
  nine `bound=1` replies, no panic, stack overflow, Guru Meditation or reset, and no queue drops.
  Minimum free stack was GPS 1492 B, telemetry 2576 B, UI 4196 B, voice 3308 B and heartbeat 1532 B,
  each above 25% and 1024 B. Production SQL independently stored `gnss_online=true`, `RMC=V`, GGA quality 0,
  satellites 0, HDOP 25.5 and `position_source=NONE` in recent frames. The private full log stays outside
  Git at `/tmp/sticks3-gnss-uplink-20260924.log`. This diagnostic version was superseded by `fec6e12`
  before the second outdoor run.
- The user placed the powered unit outdoors on 2026-09-24. A read-only production SQL check at 14:17 UTC
  found consecutive five-second telemetry frames through sequence 228 but no valid location. The running
  firmware does not yet report satellite diagnostics over MQTT, so this observation establishes only that
  uplink continued without a fix; it does not establish outdoor satellite visibility. A new diagnostic
  payload records `ext.gnss_online`, `gnss_rmc_status`, `gnss_gga_quality`, `gnss_satellites`, `gnss_hdop`
  and `position_source` (`GNSS` or `NONE`). This first outdoor run used the earlier firmware and therefore
  could not expose satellite counts remotely.
- A later read-only SQL check at 14:29 UTC found 179 frames in the preceding 15 minutes, up to
  sequence 367, and zero valid locations. Valid indoor NMEA traffic establishes that the configured UART baud,
  receive pin and sentence parser work; this outdoor result alone cannot distinguish antenna/sky-view issues
  from a module positioning problem. MotoBox phone-assisted fallback code now has an owner-authenticated HTTP
  path and a mini-program foreground switch, with `PHONE` source labels. It remains unproven on a real phone
  and requires the API and mini-program to be deployed before use. Carrier IP was rejected as a tracking
  source because its geolocation is unrelated to the phone's precise position. Nearby AP positioning needs
  provider coverage and credentials; production has neither enabled nor configured the existing AMap resolver.
- 2026-09-24 status-screen firmware `a743599` was built with ESP-IDF 5.5.2 and flashed with hash verification
  to the StickS3 K150 + Unit GPS v1.1, powered by USB-C with Grove 5 V enabled. A 75-second serial capture
  after flashing recorded eight uninterrupted ten-second heartbeats, `gps_online=1` from valid NMEA traffic,
  `gps_fix=0` indoors, Wi-Fi and MQTT connected, an assigned local IP and RSSI around -35 to -37 dBm,
  15 telemetry PUBACKs and queue depth zero. All eight replies from the currently deployed older MotoBox
  backend to status probes were ignored as `BINDING_CODE_IGNORED reason=status_probe`; no code-ready or voice
  event appeared in that capture. No panic, Guru Meditation, stack overflow, task-start failure, reset loop or
  USB reconnect was observed. Lowest free stack: GPS 1504 B, telemetry 3128 B, UI 4212 B, voice 3308 B and
  heartbeat 1560 B, all above the 25%/1024 B acceptance threshold. The full log remains outside Git at
  `/tmp/sticks3-status-final2-20260924.log`.
- MotoBox binding-status backend commit `38ca40e` passed 130 Go tests and `make build`; only the
  `daboluo-telemetry-ingest` production binary was deployed after backing up its prior binary. The running
  binary SHA-256 is `317b7139e1674910dd7aa07de10ae205fa0e3e084d7f4298c51a8abdd5783329`; its health
  endpoint stayed healthy. Before reboot the device received four consecutive `BINDING_STATUS bound=1`
  responses. A controlled reboot then produced `WIFI_CONNECTED`, `MQTT_CONNECTED`, eight more `bound=1`
  responses and seven continuous heartbeats during a 70-second serial capture. No code request, code-ready
  event or voice announcement occurred. GPS NMEA traffic resumed, no indoor fix was claimed, seven telemetry
  frames received PUBACKs, and all task free-stack minima remained above 25%/1024 B. No panic, Guru
  Meditation, stack overflow or task-start failure appeared. The reboot log stays outside Git at
  `/tmp/sticks3-bound-reboot-20260924.log`. The firmware passes `bound=1` to the LCD every second; the
  physical LCD text layout has not been visually inspected.
- Host parser, telemetry payload and queue tests: passed on 2026-09-22, including checksum failures and
  malformed checksum digits, invalid dates and fixes, mismatched sentence times, midnight rollover, protocol
  fields and units, omission of location without a fix, queue overflow with an in-flight head, PUBACK removal
  and reconnect retry.
- ESP-IDF 5.5.2 clean build with the voice-enabled binding firmware: passed. Application size was `0x181b20`;
  the 3 MiB application partition had 50% free.
- StickS3 K150 flash: passed twice on `/dev/cu.usbmodem21101`, including flash hash verification. The exact
  hardware was StickS3 K150 + Unit GPS v1.1, powered from USB-C with Grove 5 V enabled by M5PM1.
- Voice-enabled hardware run: passed on the same StickS3 K150 + Unit GPS v1.1 and USB-C power arrangement.
  The boot log reached `AUDIO_READY`, `BINDING_VOICE_READY` and the codec/PM1 readback checks. A real
  authenticated production request then reached `BINDING_CODE_READY`, `BINDING_VOICE_START` and
  `BINDING_VOICE_DONE`; the user confirmed that the speaker audibly read the six digits. The 4096-byte voice
  task retained 2092 bytes at its lowest observed high-water mark after playback. Serial monitoring continued
  for about 80 seconds without panic, Guru Meditation, stack overflow, task-start failure, reset loop, USB
  reconnect or heartbeat loss. The flashed source is recorded in commit `12f0348`.
- Cold-start binding-voice regression: fixed and revalidated on 2026-09-22 with the same StickS3 K150,
  Unit GPS v1.1 and USB-C power arrangement. PM1 GPIO2, which enables the shared LCD/ES8311 3.3 V rail,
  had retained its open-drain reset state: its output latch read high while its physical input read low, so
  ES8311 did not acknowledge at `0x18`. Configuring GPIO2 as push-pull before making it an output produced
  `mode=0x0c out=0x04 in=0x15 drive=0x13`, followed by `AUDIO_CODEC_FOUND`, `AUDIO_READY`,
  `BINDING_CODE_READY`, `BINDING_VOICE_START` and `BINDING_VOICE_DONE`. The real production response drove
  one six-digit playback; the user confirmed hearing it and completing the mini-program binding. The UI also
  logged `PAGE bind reason=code_ready`, confirming that it switched to the page that renders the same server
  code. The 4096-byte voice task again retained 2092 bytes. Serial monitoring continued for 70 seconds with
  stable ten-second heartbeats and no panic, Guru Meditation, stack overflow, task-start failure, reset loop
  or USB reconnect. The complete serial log remains outside Git at `/tmp/sticks3-binding-gpio2-fixed.log`;
  the flashed source is the firmware in this validation commit.
- Final flashed build serial run: passed for more than 6 minutes through sequence 71. Every queued frame
  received a QoS 1 PUBACK, queue depth returned to zero, `dropped` remained zero, heap stayed near 8.34 MiB,
  and no panic, reset, task-start failure or USB reconnect appeared. Its observed free-stack minima were GPS
  1516 B, telemetry 3092 B, UI 4172 B and heartbeat 1916 B.
- Indoor serial run: passed for more than 50 minutes. ESP32-S3 detected 8 MiB Octal PSRAM at 80 MHz and the
  startup memory test passed. Wi-Fi, SNTP, TLS/WSS MQTT, QoS 1 publish/PUBACK and heartbeat continuity were
  observed without panic, Guru Meditation, stack overflow, task-start failure, reset loop or USB reconnect.
- Offline recovery: passed with a controlled WSS gateway outage longer than two minutes. The RAM queue grew to
  55 frames, then drained in original sequence order after TLS reconnection. Sequence 381 through 435 all
  received PUBACKs, the queue returned to zero and `dropped` remained zero. Production SQL independently
  confirmed all 55 distinct sequence numbers with no gaps; stored `ts_ms` values kept their original five-second
  cadence while `ingested_ms - ts_ms` ranged from about 22 to 272 seconds.
- MotoBox ingestion: passed for status-only frames. Production SQL `device_latest` and `telemetry_events`
  advanced continuously; an authenticated HTTP test read both `/latest` and `/events`, then removed its
  temporary account and binding. The latest snapshot preserved model `m5stack-sticks3-gps`, capabilities
  `gps,wifi`, and Wi-Fi module state. A final post-flash SQL check observed the latest sequence advance from
  1 through 74, and none of the 116 recent Demo frames contained a `location` member while the indoor
  receiver had no fix.
- MQTT consumer recovery: passed. After forcing the ingest client off EMQX, it reconnected, restored its
  wildcard subscription and continued storing subsequent sequence numbers.
- MQTT gateway controls: passed. Public TLS certificate and hostname verification succeeded for
  `emqx.daboluo.cc`; a wrong password received MQTT reason 135, and a valid device credential received reason
  135 when publishing to another device topic. The Mosquitto journal recorded `Denied PUBLISH`. TCP 9001 is
  guarded by a persistent host firewall rule so only the Nginx loopback upstream can reach it. The ACME deploy
  hook now validates the certificate/key pair, restarts the gateway and reloads Nginx; timestamped rollback
  copies were retained on the server.
- GPS outdoor stationary fix: observed as described above. Moving track and mini-program location sharing
  remain pending. The authenticated server-code request, on-screen display, speaker playback and mini-program
  real-device binding are hardware-validated.
- Mini-program host tests and JavaScript syntax checks: passed. WeChat DevTools compiled the v0.2.31
  development preview; the phone-assisted location path still needs API rollout and phone-device acceptance.
- Status remains `prototype` until the real-device checks below are complete.

## Observed task margins

The lowest observed high-water marks during Wi-Fi + WSS/TLS + LCD + GNSS UART operation were:

| Task | Configured stack | Lowest free stack | Result |
| --- | ---: | ---: | --- |
| GPS UART/parser | 4096 bytes | 1480 bytes | pass |
| Telemetry/MQTT queue | 6144 bytes | 2576 bytes | pass |
| LVGL UI | 8192 bytes | 4164 bytes | pass |
| Binding-code voice | 4096 bytes | 2092 bytes | pass |
| Heartbeat/diagnostics | 4096 bytes | 1532 bytes | pass |

Each retained at least 25% and at least 1024 bytes. The longer Wi-Fi/MQTT/GNSS run used the earlier firmware
recorded in repository commit `c8085cb`; the speaker playback and voice-task measurement use commit `12f0348`.
The current diagnostic payload and lower telemetry/heartbeat free-stack marks were observed with `fd281f6`
during the 95-second indoor run; a renewed outdoor load check remains pending.
GSV parsing in `fec6e12` retained at least 1480 B free GPS stack and 2624 B free telemetry stack in the
indoor run.

## Remaining acceptance work

The second outdoor run proved a stationary GNSS fix, but not moving tracks or the mini-program location views.
Complete the movement and sharing checks below. Do not change the manifest to
`hardware-verified` until those checks succeed.
Unbinding and re-binding should return `BIND UNBOUND`, then a single new code, then `BIND BOUND`. The
physical LCD layout and status transitions during a deliberate MQTT outage also remain to be inspected.

## Hardware acceptance procedure

Record the exact board and Unit revision, firmware commit, USB supply, test start/end time and weather/sky
conditions. Run for at least 30 minutes, including movement, a two-minute hotspot outage and recovery.

Pass criteria:

1. A valid RMC/GGA pair produces a fix with at least four satellites and HDOP below 20.
2. The screen, A button and ten-second heartbeat remain responsive throughout the test.
3. No stack overflow, panic, Guru Meditation, reset loop, task-start failure or unexpected USB reconnect occurs.
4. Every task retains at least 25% and at least 1024 bytes of stack at the observed high-water mark.
5. MQTT reconnects, queued frames drain in sequence after PUBACK, and the drop counter remains zero.
6. MotoBox `latest` and `events` contain the device and preserve sample time across the outage.
7. The server returns a six-digit code only after the authenticated MQTT request; the device displays and
   speaks it, then the mini program consumes it once, binds the matching device, and shows the current point
   and historical track. The request, display, speaker and one-time mini-program binding have passed; the
   current-point and historical-track views remain pending until outdoor GNSS validation.

Keep the complete serial log and private route outside Git. Commit only a sanitized summary with coarse or
synthetic coordinates.
