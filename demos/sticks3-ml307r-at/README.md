# StickS3 + ML307R-DL Tiny AT/TCP bring-up

This focused Demo checks whether an ML307R-DL Tiny **AT** carrier can be detected over
StickS3 UART1, register on a cellular network and open a TCP socket. It does not use
Wi-Fi, GPS, MQTT or MotoBox credentials. It is build-verified only; the Tiny carrier
voltage levels, power input and 4G operation still need real-device checks.
The `78/esp-ml307` 3.7.5 dependency is licensed under
[Apache-2.0](https://github.com/78/esp-ml307/blob/main/LICENSE).

## Hardware and wiring

- StickS3 K150, ML307R-DL Tiny AT carrier, activated mainland-China IoT SIM and
  matching LTE antenna.
- In the supplied seller image, hold the modem's printed text upright with the
  six-hole header on the right. Its labels run top to bottom **BAT, EN, RX, TX,
  GND, VIN**. StickS3 Hat2 pin 1 (GND) → Tiny hole 5 **GND**; pin 2 (GPIO5/TX)
  → hole 3 **RX**; pin 6 (GPIO6/RX) ← hole 4 **TX**. Confirm the same labels on
  the actual carrier before attaching wires.
- Supply StickS3 by USB. Supply Tiny separately and connect the grounds. Confirm
  the Tiny carrier's marked power input, accepted voltage, UART high level and
  startup method against its exact board revision before connecting. A verified
  5 V input may use an independent 5 V/2 A bench source; never power the modem
  from StickS3 Grove or Hat2 EXT_5V.
- Measure Tiny TXD idle high level before attaching GPIO6. StickS3 GPIO is 3.3 V;
  add bidirectional logic-level adaptation when the carrier's UART levels require it.

[StickS3 Hat2 numbering](https://docs.m5stack.com/en/core/StickS3) is official.
The Tiny hole order is read from the supplied product image and is not yet
verified on the physical board. BAT and EN are outside this three-wire UART link;
the VIN label alone does not establish its allowed supply voltage or startup method.

## Build and run

Install ESP-IDF 5.5.2. From `firmware/`:

```bash
source ~/esp/esp-idf/export.sh
idf.py set-target esp32s3 build
idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

The default TCP probe opens `example.com:80`; set `CONFIG_BRINGUP_TCP_HOST` and
`CONFIG_BRINGUP_TCP_PORT` in menuconfig if a different allowed endpoint is needed.
The probe sends no personal data. A successful serial run should contain
`MODEM_DETECTED`, `NETWORK_READY` and `TCP_CONNECTED`, followed by stack margins.
`MODEM_DETECT_FAILED` means the UART, level, power or AT firmware needs checking;
`NETWORK_WAIT_FAILED` points to SIM, antenna, registration or data service.

## Limits and acceptance

Compilation does not establish wiring or cellular compatibility. Before promoting
this Demo, record the exact Tiny PCB revision and silkscreen, UART voltage, power
source, cold-start and transmit current/voltage, SIM/APN, 30 minutes of serial
logs, and all task stack high-water marks. Each task must retain at least 25% and
1024 bytes. The full serial log and SIM/account details stay outside Git.

See [validation](docs/validation.md). The completed tracker is in
[StickS3 GPS → MotoBox](../../projects/m5stack-sticks3-gps-motobox/).
