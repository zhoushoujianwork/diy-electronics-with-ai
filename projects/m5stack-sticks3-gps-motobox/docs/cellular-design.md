# ML307R-DL Tiny cellular uplink design

## Scope and data path

The existing GPS Unit stays on Grove GPIO9/10 (UART2). ML307R-DL Tiny AT uses
Hat2 GPIO5/6 (UART1) through `78/esp-ml307` 3.7.5 (Apache-2.0). The modem
provides TCP and UDP data sockets. ESP32 mbedTLS handles the MQTT TLS handshake,
public certificate chain and hostname verification. The application retains the
device identity, `vehicle/v1/{device_id}/telemetry`, binding request/response
topics, QoS 1 PUBACK and 120-frame RAM queue. Wi-Fi is not started in cellular mode.

The cellular UDP SNTP request includes an eight-byte random transmit timestamp.
Its reply must match that nonce, be a server reply with valid leap/stratum fields
and yield a plausible UTC date before it sets the clock. GNSS time remains an
alternative. Telemetry sampling begins after either source establishes trusted
time, including when GPS has no indoor fix.

The sampler writes a five-second frame into the PSRAM-backed RAM queue without
calling modem or MQTT APIs. A separate uplink task queues the head for ESP-MQTT;
the MQTT task performs network I/O. The head is reserved before enqueue so a
full queue does not discard it. A PUBACK removes the matching head; disconnect
clears its in-flight mark so it can be retried. A short early-PUBACK cache covers
the race between enqueue returning its message ID and event delivery. QoS 1 is
at-least-once, so a reconnect may duplicate a sequence; MotoBox should dedupe by
device and `ext.seq`. A restart loses the RAM backlog.

`system.signal` is raw CSQ 0–31 in cellular mode, 99 when unknown. Wi-Fi mode
keeps its RSSI dBm semantics. Cellular frames advertise `caps: ["gps", "gsm"]`,
set `modules.gsm` from data readiness, and add `ext.cell_model`,
`cell_registered`, `cell_data`, `cell_reconnects` and `cell_sampled_ms`.

## Task stack sizing and observability

Stack sizes are in ESP-IDF bytes. The configured size covers the largest known
path and local buffers; real margins remain unverified until a loaded 4G run.

| Task | Stack | Largest relevant path |
| --- | ---: | --- |
| GPS | 4096 | UART read, NMEA parser, fix snapshot |
| Telemetry sampler | 6144 | 1024-byte payload, status/fix structs, JSON formatting and queue push |
| Uplink | 8192 | 1024-byte payload copy, binding status publish and MQTT enqueue |
| Cellular orchestration | 12288 | modem detection, registration, CSQ and UDP SNTP |
| ESP-MQTT | 12288 | custom TCP transport, mbedTLS handshake/read/write and MQTT events |
| Modem receive | 2048 | upstream `78/esp-ml307` UHCI DMA receive task |
| Modem event | 6144 | upstream AT parse, TCP URC and UDP SNTP callback |
| UI | 8192 | LVGL screen refresh and button handling |
| Binding voice | 4096 | PCM clip playback |
| Heartbeat | 4096 | status snapshot and task high-water queries |

The heartbeat logs each task's free stack, queue depth, drops and modem state.
Acceptance requires at least 25% and 1024 B free for **each** task under
registration, transmit, GPS and LCD load. The two upstream AT task stack sizes
are fixed by dependency 3.7.5; if their measured margins fail, adjust the
dependency or replace its task configuration before claiming hardware support.

## Hardware and rollback

The [functional wiring diagram](assets/ml307r-tiny-wiring.svg) includes the
seller image's right-side order: BAT, EN, RX, TX, GND, VIN from top to bottom
with the shield text upright. Confirm the same order on the actual carrier.
Keep modem power separate from StickS3
Grove/Hat2 output, share ground, confirm 3.3 V-compatible UART or add level
adaptation, attach antenna before power, and verify Tiny startup method. The
prior Wi-Fi firmware commits and hardware records remain available as rollback;
the current 4G firmware has not been flashed.
