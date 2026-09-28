# R1 design decisions

## User-confirmed scope

2026-09-27: independent automotive power board; main board 5V/1A; Tiny BAT supply about 3.9V/2A; 1S protected Li-polymer pack about 3000mAh, ≥9A continuous discharge; parking motion wake; seven days stationary standby. Two output connector families plus terminal blocks, no USB output. The XH family uses 2.50mm pitch, separately from 2.54mm headers.

Power key: 2s shutdown request, maximum 10s save/stop timeout. Manual off masks onboard and external motion; wake only by key or fresh ACC rising edge. Parking standby after 120s stationary with ACC absent permits motion/external wake. External wake is active-low open drain with 20ms debounce and release-before-rearm.

Master switch: dual pole, low-current control only, independently disables vehicle protection FETs and battery back-to-back FETs. OFF overrides controller firmware; residual semiconductor/protection leakage is specified and measured separately from load off current.

## Electrical changes required by Tiny BAT selection

- Tiny VIN and BAT are mutually exclusive power inputs. This revision powers BAT; VIN remains unconnected.
- One common low-voltage source mux cannot be assigned the combined worst-case current without checking its current and thermal limits. Two independent source mux/converter channels separate the host and modem current.
- Selected rail implementation: one TPS2117 source mux and TPS63020 buck-boost per channel; nominal host regulator output 5.095V and modem 3.901V before load-path losses. Calculated limits are in [engineering.md](engineering.md). Low-battery/hot full-load operation remains a bench acceptance item because headline switch-current ratings are insufficient evidence.
- Converter disable and an ideal diode do not by themselves prove load disconnection. LM66100 OFF retains its forward body-diode path. Its reverse-blocking configuration requires CE tied to VOUT; it cannot also be treated as a normal GPIO-controlled isolating switch. Host output therefore needs a separate true disconnect switch.
- A separate protected-vehicle low-current regulator supplies the always-on domain when no battery is fitted. This removes the circular dependency in which the controller would need to enable the buck before it has power itself.
- ACC sensing must not bypass master OFF. Its optocoupler input path is gated by the vehicle master control. External UART/wake/debug inputs need powered-off behavior specified.
- BQ25606's default thermistor hot shutdown is about 60°C. The selected 9.76k/1.5M network and external C394021 thermistor calculate to cold-stop 1.56–4.99°C and hot-stop 39.29–42.45°C, including manufacturer R/T bounds and resistor tolerances. This conservatively satisfies the 0–45°C charge window; temperature trips still need physical verification.
- Car-only startup first probes the disabled vehicle buck. ACC does not prove the main vehicle input exists, and zero battery ADC does not prevent startup when the car rail is valid. Battery cutoffs only shut down loads that depend on the battery.

## Evidence and status

Engineering data, calculations, source tests and readback summaries are stored alongside the source and BOM. Library existence establishes component identity; it does not establish stock availability or vehicle qualification.

Current status: the four A4 sheets contain all 221 selected parts for measurement. The full pin/net source and 73-type onboard BOM are generated. Eleven electrical-intent tests pass. Automated layout is incomplete; no final wiring/NC Apply, final DRC, PCB, firmware, or hardware validation is claimed. See [validation.md](validation.md) for scope and retained failure evidence.
