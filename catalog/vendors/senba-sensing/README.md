# Senba Sensing supplier knowledge

This page records reusable selection boundaries for Senba PIR components. Exact product facts and sources remain
in [`products/`](products/). Purchase intent, orders, and stock counts remain outside the public catalog.

## AS312 identity

[`AS312`](products/as312.yaml) is a low-voltage digital pyroelectric infrared sensing component. A marketplace
listing described as an "AS312 module" may add a Fresnel lens, regulator, capacitors, connector, transistor, or
indicator. Treat the finished board as a separate product until its manufacturer, schematic, and pin order are
known; the sensor name alone does not establish module supply voltage, range, or connector orientation.

LCSC identifies the Senba part as `C90465` and links a manufacturer family datasheet. The family sheet establishes
a 2.7-3.3 V three-pin VDD/REL/VSS component, but it does not establish a universal breakout-board range or field
of view. Those depend strongly on the Fresnel lens and mechanical installation.

## Integration comparison

| Choice | What is verified | Selection boundary |
| --- | --- | --- |
| Senba AS312 component | Low-voltage digital PIR sensing component | Requires a documented lens and surrounding circuit; do not infer the pin order or supply range of an unknown seller board |
| M5Stack HAT PIR U054 | Finished M5StickC accessory containing AS312; nominal 500 cm, less than 100 degrees, 2 second hold | Easiest documented M5StickC integration; fixed behavior and no schematic indicator LED |
| HC-SR501-style breakout | Common adjustable PIR breakout class | Not the same product as AS312 or U054; seller revisions, regulator, timing range, and pin details must be verified before cataloging |

For a compact fixed-behavior wake trigger, a documented AS312-based module is a reasonable candidate. In an
access-control system, use PIR only to wake the camera or face-recognition process. PIR is not identity evidence
and must never authorize the lock by itself. Confirm that the selected MCU input is wake-capable, wait for the PIR
output to return inactive before sleeping again, and keep lock or relay power isolated from the sensor/MCU path.

## Official and authorized routes

- [Senba Sensing official site](https://www.senbasensor.com/)
- [Senba PIR product family](https://www.senbasensor.com/pyroelectric-infrared-sensor/)
- [AS312 / LCSC C90465](https://www.lcsc.com/product-detail/C90465.html)
- [M5Stack U054 AS312 integration](https://shop.m5stack.com/products/m5stickccompatible-hat-pir-sensor)
