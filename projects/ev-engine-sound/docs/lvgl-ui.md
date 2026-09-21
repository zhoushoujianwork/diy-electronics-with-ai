# LCKFB LVGL dashboard

The 320x240 landscape dashboard is intentionally operable with one hand and
keeps the live engine state visible while a control is being touched.

## Main page

- Top controls: `START` / `STOP`, live RPM/state and a `SET` page button.
- `ENGINE · TAP NEXT` and `EXHAUST · TAP NEXT` are large, independent cycle
  buttons. Each click advances exactly one item and wraps at the end; there are
  no swipe gestures, momentum, dropdowns or hidden lists.
- The centre is a retained RGB565 powertrain workbench with no vehicle body.
  The left side shows a profile-sensitive engine assembly with up to six visible
  cylinder barrels, plug leads, intake trumpets, cooling fins, cases and a rotating
  crank pulley. The firing cylinder glows in time with live engine state.
- A separate twist-throttle assembly occupies the upper right. It includes a
  housing, cable, ribbed rubber grip, end cap, twist index and live opening bar.
- The selected exhaust is enlarged on the lower right. Stock uses a long
  grey silencer, Akrapovič style a dark tapered can with a red tip, Yoshimura
  style a brushed-gold can with contrasting bands, `TIN CAN` an intentionally
  oversized red drinks can with silver rolled rims, pull tab, bubbles and a
  generic `COLA` wordmark, and `NO MUFFLER` a narrow heat-tinted open pipe.
  Small `AKRA`, `YOSH`, `OEM` and `OPEN` pixel marks make every selection readable;
  they are descriptive non-official artwork rather than copied brand assets.
- `REDLINE` is a main-page slider from idle + 500 RPM through 16000 RPM.
- `HOLD THROTTLE`: the only throttle control on screen. Pressing starts the
  engine and applies 100% throttle; releasing always returns to zero throttle.
  The button changes to `RELEASE TO COAST` while held. The load strip and phase
  caption reflect the audio engine's smoothed state, including `STARTING`,
  `ACCEL`, `HOLD`, `COAST`, `IDLE` and `STOPPING`.

## Rendering and touch timing

- Touch and display timers use 16 ms; the powertrain update target is 33 ms.
  These are scheduling targets, not claims of measured 60/30 FPS.
- Cycle-button callbacks log the old and new selection before dispatching the
  action. Text, styles and slider values are updated only when changed. The powertrain
  stops invalidating when parked and does not redraw behind the settings page.
- A 69120-byte static RGB565 canvas avoids heap allocation in the render loop.
  Two 40-line DMA buffers transfer the result without putting frame data on a stack.
- `UI_READBACK` includes render count, maximum render/update times, raster frame
  count (`draw_passes`) and the `cycle_buttons` selector mode for verification.

## Sound transitions

Startup has a 550 ms smooth ignition ramp and a short procedural starter whirr.
Throttle load follows 85 ms attack / 190 ms release; flywheel response is 240 ms
up / 480 ms down. A quiet intake layer rises with load; on release, a decaying
exhaust/noise emphasis adds a restrained overrun tail. Gain uses 45 ms attack /
75 ms release. All these time constants are sample-rate independent and tested
at both 16 kHz and 32 kHz. The sounds remain procedural, not recorded samples.

The engine profile and exhaust voicing are separate controls. Five exhaust
presets change resonance frequency/decay, pulse and noise balance, low-pass
response, drive and overrun emphasis without adding samples or heap work to the
audio loop. `AKRAPOVIC STYLE` and `YOSHIMURA STYLE` are unofficial descriptive
labels, not measured replicas or manufacturer-endorsed sound maps.

## Settings page

- `OUTPUT VOLUME -/+`: software output gain in 5% steps, from 0% to 100%.
- Exhaust selection lives on the main page so it remains visible beside the
  engine selector and can be changed while the engine is running.
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
change also emits `UI_SYNC mode=cycle_buttons` with both selected values.

## Hardware path

- ST7789: SPI2, GPIO41 SCLK, GPIO40 MOSI, GPIO39 DC, 40 MHz, mode 2.
- LCD chip-select: PCA9557 address 0x19 bit 0, active low.
- Backlight: GPIO42, LEDC 5 kHz at 50% duty (a static GPIO level does not light
  this board's boost circuit reliably).
- FT6336/FT5x06 touch: shared I2C0 on GPIO1/GPIO2, address 0x38, transformed for
  landscape with swap-XY and mirror-X.
- LVGL task: core 0, priority 4, explicit 10240-byte stack. The heartbeat reports
  its measured high-water mark as `stack_lvgl`.
