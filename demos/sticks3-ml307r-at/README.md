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
  GND, VIN**. The supplied interface table numbers these pins in reverse,
  starting with VIN as pin 1: GND=2, TXD=3, RXD=4, EN=5, BAT=6. StickS3 Hat2
  pin 1 (GND) → Tiny **GND** (table pin 2, fifth hole from the top); pin 2
  (GPIO5/TX) → **RXD** (table pin 4, third hole); pin 6 (GPIO6/RX) ← **TXD**
  (table pin 3, fourth hole). Confirm the same labels on the actual carrier.
- Supply StickS3 by USB. Supply Tiny separately and connect the grounds. Confirm
  the Tiny carrier's exact revision and marked power input before connecting.
  The supplied interface table specifies VIN 5–16 V and TXD/RXD 3.3 V. Use an
  independent 5 V supply branch rated at least 2 A, not StickS3 Grove or Hat2
  EXT_5V. Measure Tiny TXD idle high level before attaching GPIO6.
- The supplied table says EN is pulled up to VIN. Leave EN unconnected for this
  bring-up; at 5 V VIN it must not connect directly to a 3.3 V StickS3 GPIO.
  BAT is a separate 3.4–4.2 V battery input and must not be powered with VIN.
  StickS3 Hat2 pin 11 is labelled BAT and its onboard battery is 250 mAh; its
  allowable cellular transmit load is undocumented, so do not use it to power
  Tiny BAT.

[StickS3 Hat2 numbering](https://docs.m5stack.com/en/core/StickS3) is official.
The carrier's hole order and electrical values come from user-supplied seller
material, not an independently verified public source or physical measurement.
The table's pin numbers must not be confused with counting holes from the top.

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
