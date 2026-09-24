# Validation record

## Current state

- The user placed the powered unit outdoors on 2026-09-24. A read-only production SQL check at 14:17 UTC
  found consecutive five-second telemetry frames through sequence 228 but no valid location. The running
  firmware does not yet report satellite diagnostics over MQTT, so this observation establishes only that
  uplink continued without a fix; it does not establish outdoor satellite visibility. A new diagnostic
  payload records `ext.gnss_online`, `gnss_rmc_status`, `gnss_gga_quality`, `gnss_satellites`, `gnss_hdop`
  and `position_source` (`GNSS` or `NONE`). Host tests and an ESP-IDF build passed, but this version has not
  been flashed because the unit is still outside. Outdoor satellite and task-stack verification remain pending.
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
- GPS outdoor fix, moving track and mini-program location sharing: pending. The authenticated server-code
  request, on-screen display, speaker playback and mini-program real-device binding are hardware-validated.
- Mini-program host tests and JavaScript syntax checks: passed. WeChat DevTools recognized the production
  AppID, but preview compilation was blocked because the local DevTools session requires a fresh login.
- Status remains `prototype` until the real-device checks below are complete.

## Observed task margins

The lowest observed high-water marks during Wi-Fi + WSS/TLS + LCD + GNSS UART operation were:

| Task | Configured stack | Lowest free stack | Result |
| --- | ---: | ---: | --- |
| GPS UART/parser | 4096 bytes | 1516 bytes | pass |
| Telemetry/MQTT queue | 6144 bytes | 3072 bytes | pass |
| LVGL UI | 8192 bytes | 4172 bytes | pass |
| Binding-code voice | 4096 bytes | 2092 bytes | pass |
| Heartbeat/diagnostics | 4096 bytes | 1832 bytes | pass |

Each retained at least 25% and at least 1024 bytes. The longer Wi-Fi/MQTT/GNSS run used the earlier firmware
recorded in repository commit `c8085cb`; the speaker playback and voice-task measurement use commit `12f0348`.

## Remaining acceptance work

The indoor run does not prove GNSS positioning or the mini-program location views. Move the powered unit outdoors
with an open sky view and complete the movement and sharing checks below. Do not change the manifest to
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
