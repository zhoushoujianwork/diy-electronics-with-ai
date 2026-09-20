# LCKFB LVGL dashboard

The 320x240 landscape dashboard is intentionally operable with one hand and
keeps the live engine state visible while a control is being touched.

## Main page

- Top controls: `START` / `STOP`, live RPM/state and a `SET` page button.
- A horizontal, centre-snapping carousel switches directly among all 18 engine
  profiles. Cards can be swiped or tapped; no dropdown is used.
- The centre is an Engine Simulator-inspired mechanical cutaway rather than an
  arc gauge. A retained RGB565 LVGL canvas renders cylinders, valves, pistons, connecting
  rods, crank pins and ignition for inline, V and flat layouts. Cylinder phase
  comes from each profile's actual firing order; inline four therefore moves as
  symmetric 1/4 and 2/3 pairs. Mechanical speed is RPM-linked slow motion to
  avoid aliasing; the top lamp uses real firing activity. The slow-motion chamber
  flash follows compression TDC, so the flame and piston stay in phase.
  A 3.5:1 rod/crank ratio preserves connecting-rod length throughout the stroke;
  dual piston rings, moving intake/exhaust valves, cylinder numbers and chamber
  colours make power/exhaust/intake/compression visible. V banks use 45, 60, 72
  or 90 degrees and expand into adjacent pairs instead of overlapping cylinders.
  These are schematic cutaways, not manufacturer-specific CAD geometry.
- `REDLINE` is a main-page slider from idle + 500 RPM through 16000 RPM.
- `HOLD THROTTLE`: the only throttle control on screen. Pressing starts the
  engine and applies 100% throttle; releasing always returns to zero throttle.
  The button changes to `RELEASE TO COAST` while held. The load strip and phase
  caption reflect the audio engine's smoothed state, including `STARTING`,
  `ACCEL`, `HOLD`, `COAST`, `IDLE` and `STOPPING`.

## Rendering and touch timing

- Touch and display timers use 16 ms; the mechanical update target is 33 ms.
  These are scheduling targets, not claims of measured 60/30 FPS.
- The carousel uses 6 px drag recognition, momentum and a 240 ms ease-out snap.
  While scrolling, mechanical raster updates pause to prioritize the moving cards;
  audio continues and the mechanism resumes at the current simulated phase.
- Text, styles and slider values are updated only when changed. The mechanism
  stops invalidating when parked and does not redraw behind the settings page.
- A 74240-byte static RGB565 canvas replaces hundreds of per-frame draw tasks.
  Two 40-line DMA buffers transfer the result without putting frame data on a stack.
- `UI_READBACK` includes render count, maximum render/update times, raster frame
  count (`draw_passes`) and scrolling state for device-side verification.

## Sound transitions

Startup has a 550 ms smooth ignition ramp and a short procedural starter whirr.
Throttle load follows 85 ms attack / 190 ms release; flywheel response is 240 ms
up / 480 ms down. A quiet intake layer rises with load; on release, a decaying
exhaust/noise emphasis adds a restrained overrun tail. Gain uses 45 ms attack /
75 ms release. All these time constants are sample-rate independent and tested
at both 16 kHz and 32 kHz. The sounds remain procedural, not recorded samples.

## Settings page

- `OUTPUT VOLUME -/+`: software output gain in 5% steps, from 0% to 100%.
- Values update live and apply immediately. Changing engine type restores that
  type's default redline. `BACK` returns to the main page.

| Cylinders | Dependent crank/layout choices |
| --- | --- |
| 1 | `360 CRANK` |
| 2 | `180 P-TWIN`, `270 P-TWIN`, `360 P-TWIN`, `45 V-TWIN`, `90 V-TWIN` |
| 3 | `120 EVEN`, `270 T-PLANE` |
| 4 | `EVEN INLINE`, `CROSSPLANE` |
| 5 | `INLINE FIVE` |
| 6 | `INLINE SIX`, `60 V-SIX`, `FLAT SIX` |
| 8 | `FLATPLANE V8`, `CROSSPLANE V8` |
| 10 | `72 V-TEN` |
| 12 | `60 V-TWELVE` |

All touch callbacks emit a `TOUCH` log followed by an `UI_ACTION` result. UI
control disables the remote-link timeout, while a later serial command returns
control to the two-second serial watchdog. The LVGL refresh timer only reads a
locked state snapshot; it never touches the audio engine directly. A profile
change also emits `UI_SYNC` with the selected profile and mechanical layout.

## Hardware path

- ST7789: SPI2, GPIO41 SCLK, GPIO40 MOSI, GPIO39 DC, 40 MHz, mode 2.
- LCD chip-select: PCA9557 address 0x19 bit 0, active low.
- Backlight: GPIO42, LEDC 5 kHz at 50% duty (a static GPIO level does not light
  this board's boost circuit reliably).
- FT6336/FT5x06 touch: shared I2C0 on GPIO1/GPIO2, address 0x38, transformed for
  landscape with swap-XY and mirror-X.
- LVGL task: core 0, priority 4, explicit 10240-byte stack. The heartbeat reports
  its measured high-water mark as `stack_lvgl`.
