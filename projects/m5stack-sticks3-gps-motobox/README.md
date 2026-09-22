# StickS3 GPS → MotoBox

This open project turns an M5Stack StickS3 into a Wi-Fi GNSS tracker. It reads an M5Stack Unit GPS v1.1,
uploads MotoBox-compatible telemetry over MQTT TLS, and displays and speaks a server-generated one-time code
that the MotoBox mini program uses to bind the device.

The project remains a `prototype`: indoor connectivity and offline recovery have been validated, while outdoor
positioning, moving tracks and mini-program real-device acceptance are still pending. See
[validation](docs/validation.md) for the evidence and remaining checks.

The first public demonstration uses a platform-managed device account. MotoBox broker access is issued
separately; cloning this repository does not automatically create a cloud device or MQTT credential.

## Hardware

| Item | Exact model/revision | Quantity | Notes |
| --- | --- | ---: | --- |
| Controller | M5Stack StickS3 K150 | 1 | ESP32-S3-PICO-1-N8R8, 3.3 V GPIO |
| GNSS | M5Stack Unit GPS v1.1 / U032-V11 | 1 | ATGM336H-6N / AT6668, 115200 8N1 |
| Cable | HY2.0-4P Grove cable | 1 | Included with the GNSS Unit |
| Power | USB Type-C 5 V source | 1 | Use USB for first bring-up |

The Unit documentation lists 31.64 mA at 5 V as typical consumption. This is not a measured startup peak;
confirm the real module current before relying on battery operation.

## Wiring and power

Connect the keyed Grove cable directly. Looking at the documented pin order:

| Grove wire | Unit GPS v1.1 | StickS3 |
| --- | --- | --- |
| Black | GND | GND |
| Red | 5 V | 5 V output |
| Yellow | UART RX | GPIO9 / host TX |
| White | UART TX | GPIO10 / host RX |

The firmware enables the StickS3 M5PM1 5 V boost. Once enabled, do not also feed 5 V into the Grove red
wire. Confirm the connector key and labels before power-on; ESP32-S3 GPIO is not 5 V tolerant.

## Configure, build and flash

Install ESP-IDF 5.5.2, then create the ignored local configuration:

```bash
cd projects/m5stack-sticks3-gps-motobox/firmware
cp sdkconfig.local.defaults.example sdkconfig.local.defaults
```

Fill in the Wi-Fi/hotspot and the device-specific MotoBox MQTT TLS values. Do not commit that file. Build and
flash with both defaults files:

```bash
source ~/esp/esp-idf/export.sh
idf.py -B build \
  -DSDKCONFIG_DEFAULTS='sdkconfig.defaults;sdkconfig.local.defaults' \
  set-target esp32s3 build
idf.py -B build -p /dev/cu.usbmodemXXXX flash monitor
```

If you previously built this project under `demos/`, use a fresh build directory after the move to `projects/`;
the old CMake cache contains absolute paths. Keep your ignored local configuration when rebuilding.

The firmware rejects empty credentials and a broker URI that does not begin with `mqtts://` or `wss://`. It validates the
server hostname and public certificate chain through the ESP-IDF certificate bundle. Do not disable TLS
verification to work around a broker certificate error.

The Mandarin binding prompt and digit clips are checked in as 16 kHz mono PCM, so normal firmware builds do
not require a speech service. `firmware/tools/generate_binding_voice.py` reproduces them on macOS with the
Tingting system voice and `ffmpeg` when the wording or audio processing needs to change.

## Bind with the MotoBox mini program

1. Wait for `Wi-Fi UP` and `MQTT UP`. After the authenticated MQTT session is ready, the StickS3 requests a
   six-digit one-time code from MotoBox, opens the binding page, and says “绑定验证码” followed by the six
   digits. The announcement plays once for each new code; restart the device to request and hear the current
   code again if it was missed.
2. Open the MotoBox mini program, select **添加设备**, and enter the code spoken or shown by the StickS3. The
   code is valid for ten minutes and can be used once. Press A to switch between the code and status pages.
3. Open the MotoBox WeChat mini program and sign in. Its availability may be limited to the current official or
   invited experience release; use the access route supplied with the demonstration. The latest repository
   evidence only confirms an uploaded development build. It does not claim that every WeChat user can open a
   formal or experience release.
4. Optionally name the device and finish binding.
5. Move outdoors for the first fix. The device page distinguishes an online device waiting for a first fix from
   an offline device. After valid points arrive, open **实时位置**, **历史轨迹**, or **分享位置**.

MotoBox is the hosted companion service for this project. The firmware, protocol example and reproduction steps
are open under the repository MIT license; the hosted service and its device credentials are operated
separately.

- MotoBox website: [motobox.daboluo.cc](https://motobox.daboluo.cc/)
- MotoBox source and API contract: [github.com/zhoushoujianwork/motobox](https://github.com/zhoushoujianwork/motobox)
- This hardware project: `projects/m5stack-sticks3-gps-motobox/` in DIY Electronics with AI

## Data and offline behavior

- Topic: `vehicle/v1/{device_id}/telemetry`
- Model: `m5stack-sticks3-gps`; capabilities: `gps`, `wifi`
- Coordinates: WGS-84; speed: km/h; device time: UTC milliseconds
- Interval: five seconds; MQTT QoS 1; retained flag off
- Valid fix: matched RMC/GGA time, at least four satellites, `0 < HDOP < 20`, age at most five seconds
- Offline queue: 120 RAM frames, oldest first; a frame is removed only after its PUBACK
- Full queue: preserve an in-flight frame, drop the oldest frame not in flight, and expose the drop count
- Restart: RAM backlog is intentionally lost in this prototype

The firmware initializes the K150's 8 MiB Octal PSRAM. The offline queue and TLS allocations use external RAM
so that display DMA buffers and FreeRTOS task stacks retain sufficient internal memory.

## Expected serial result

```text
POWER_READY lcd=on grove_5v=on pm1=0x6e
AUDIO_READY amp=off format=0x0c volume=0xbf
BINDING_VOICE_READY sample_rate=16000 task_stack=4096
GNSS_READY uart=1 baud=115200 rx=10 tx=9
WIFI_CONNECTED
MQTT_CONNECTED uri=wss://...
BINDING_REQUESTED msg_id=...
BINDING_CODE_READY expires_ms=...
BINDING_VOICE_START digits=6
BINDING_VOICE_DONE stack_free=...
GNSS_FIX ... sat=8 hdop=1.1 ...
MQTT_ACK seq=1 ...
HEARTBEAT gps=1 ... queue=0 dropped=0 ...
```

The binding request uses `vehicle/v1/{device_id}/binding/request`; the server responds only to the authenticated
device on `vehicle/v1/{device_id}/binding/response`. Run host tests with `./tests/run.sh`. See
[validation](docs/validation.md) for the required real-device run and
the distinction between build and hardware verification.

## Sources

- [M5Stack StickS3 documentation](https://docs.m5stack.com/en/core/StickS3), accessed 2026-09-22.
- [M5Stack Unit GPS v1.1 documentation](https://docs.m5stack.com/en/unit/Unit-GPS%20v1.1), accessed 2026-09-22.
- [MotoBox protocol summary](https://github.com/zhoushoujianwork/motobox), accessed 2026-09-22.
