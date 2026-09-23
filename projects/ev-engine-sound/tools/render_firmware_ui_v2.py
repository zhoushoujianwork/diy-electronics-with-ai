#!/usr/bin/env python3
"""Render an exact 320x240 desktop-first UI study before firmware porting."""

from pathlib import Path
import math

from PIL import Image, ImageDraw, ImageFont, ImageOps

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "docs/assets"
ENGINE_SOURCE = ROOT / "docs/third-party/engine-sim/official-engine-reference.png"
FONT_PATH = "/System/Library/Fonts/SFNSMono.ttf"

W, H, S = 320, 240, 4
BG = "#0e1012"
FG = "#f7f7f7"
GRID = "#777b7e"
DIM = "#303336"
PINK = "#f394be"
RED = "#ee4445"
ORANGE = "#f4802a"
YELLOW = "#fdbd2e"
BLUE = "#77cee0"

EXHAUSTS = (
    ("STOCK", "stock"),
    ("CARBON", "carbon"),
    ("TITANIUM", "titanium"),
    ("TIN CAN", "tin_can"),
    ("OPEN", "open"),
)


def sc(value):
    return int(round(value * S))


def box(x0, y0, x1, y1):
    return tuple(sc(v) for v in (x0, y0, x1, y1))


def font(px):
    return ImageFont.truetype(FONT_PATH, sc(px))


def text(draw, xy, value, px=7, color=FG, anchor=None):
    draw.text((sc(xy[0]), sc(xy[1])), value, font=font(px), fill=color, anchor=anchor)


def line(draw, points, color, width=1):
    draw.line([(sc(x), sc(y)) for x, y in points], fill=color, width=max(1, sc(width)), joint="curve")


def frame(draw, bounds, title, right=None):
    draw.rectangle(box(*bounds), outline=GRID, width=sc(1))
    text(draw, (bounds[0] + 5, bounds[1] + 4), title, 7)
    if right:
        text(draw, (bounds[2] - 5, bounds[1] + 4), right, 6, GRID, "ra")


def draw_header(draw, rpm="4200 RPM", state="ACCEL", state_color=ORANGE):
    draw.rectangle(box(0, 0, 319, 27), outline=GRID, width=sc(1))
    # Compact upstream-inspired A mark, separated from the project name.
    draw.polygon([(sc(5), sc(5)), (sc(11), sc(5)), (sc(8), sc(21))], fill=FG)
    draw.polygon([(sc(10), sc(5)), (sc(16), sc(5)), (sc(13), sc(21))], fill=FG)
    draw.rectangle(box(7, 15, 14, 17), fill=BG)
    text(draw, (21, 5), "EV ENGINE", 7)
    text(draw, (21, 15), "SIMULATOR", 4.5, GRID)
    draw.ellipse(box(91, 10, 97, 16), fill=RED)
    text(draw, (102, 6), rpm, 7)
    text(draw, (174, 7), state, 6, state_color)
    # The whole header is the gesture target. This is an affordance, not a menu button.
    text(draw, (313, 5), "SETTINGS", 5.5, FG, "ra")
    text(draw, (313, 15), "PULL DOWN  V", 4.2, GRID, "ra")


def draw_engine_panel(image, draw):
    frame(draw, (0, 28, 171, 136), "ENGINE CUTAWAY", "2C / 270")
    source = Image.open(ENGINE_SOURCE).convert("RGB")
    # Keep the official geometry intact; crop only excess near-black breathing room.
    source = ImageOps.fit(source, (sc(159), sc(88)), method=Image.Resampling.LANCZOS,
                          centering=(0.5, 0.54))
    image.paste(source, (sc(6), sc(45)))
    # Put ignition into the engine panel instead of spending a separate column.
    text(draw, (8, 124), "IGN", 4.5, GRID)
    for i, cx in enumerate((30, 43)):
        draw.ellipse(box(cx - 4, 122, cx + 4, 130), outline=DIM, width=sc(1))
        draw.ellipse(box(cx - 2.5, 123.5, cx + 2.5, 128.5), fill=ORANGE if i else "#484b4e")
        if i:
            draw.ellipse(box(cx - 1, 125, cx + 1, 127), fill=FG)
    text(draw, (164, 124), "LIVE", 4.5, ORANGE, "ra")


def draw_exhaust(draw, kind):
    label_value = dict((key, title) for title, key in EXHAUSTS)[kind]
    frame(draw, (172, 28, 319, 136), "EXHAUST", label_value)

    # Two exhaust primaries meet on a shared datum before the selected can.
    line(draw, [(178, 54), (190, 66), (207, 73)], PINK, 3)
    line(draw, [(178, 66), (193, 70), (207, 73)], PINK, 3)
    line(draw, [(191, 65), (209, 78)], "#d2d4d5", 5)

    if kind == "stock":
        draw.rounded_rectangle(box(207, 58, 303, 88), radius=sc(5), fill="#63676b",
                               outline="#b7bbbe", width=sc(2))
        draw.rectangle(box(216, 61, 292, 65), fill="#9ca0a3")
        draw.rectangle(box(298, 64, 309, 82), fill="#cdd0d2")
        draw.rectangle(box(306, 68, 315, 78), fill="#292c2f")
        text(draw, (255, 74), "OEM", 7, FG, "mm")
    elif kind == "carbon":
        draw.polygon([*map(sc, (206, 57)), *map(sc, (303, 62)), *map(sc, (291, 91)),
                      *map(sc, (211, 88))], fill="#292c2f")
        draw.line([*box(207, 57, 303, 62)], fill=GRID, width=sc(1))
        for x in range(217, 288, 12):
            line(draw, [(x, 60), (x + 20, 88)], "#555a5d", .6)
        draw.rectangle(box(293, 63, 304, 88), fill=RED)
        draw.rectangle(box(302, 69, 315, 82), fill="#222527")
        text(draw, (255, 74), "CF", 7, FG, "mm")
    elif kind == "titanium":
        draw.polygon([*map(sc, (206, 59)), *map(sc, (301, 55)), *map(sc, (293, 91)),
                      *map(sc, (211, 88))], fill="#c89b58")
        line(draw, [(208, 60), (299, 56)], "#f0dfbd", 1.5)
        draw.rectangle(box(211, 59, 218, 88), fill="#795832")
        draw.rectangle(box(287, 57, 297, 90), fill="#eadcbe")
        draw.rectangle(box(296, 66, 315, 82), fill="#352b20")
        text(draw, (254, 74), "TI", 7, BG, "mm")
    elif kind == "tin_can":
        draw.rectangle(box(205, 54, 301, 93), fill="#bd1831", outline="#e54b59", width=sc(2))
        draw.rectangle(box(209, 51, 301, 56), fill="#dce0e2")
        draw.rectangle(box(209, 92, 301, 97), fill="#aeb4b7")
        draw.ellipse(box(216, 52, 232, 58), outline=GRID, width=sc(1))
        line(draw, [(216, 89), (290, 57)], FG, 2)
        line(draw, [(230, 96), (305, 64)], FG, 1.4)
        draw.rectangle(box(300, 61, 307, 88), fill="#e54b59")
        draw.rectangle(box(305, 68, 316, 81), fill="#34373a")
        text(draw, (254, 73), "COLA", 8, FG, "mm")
    else:
        line(draw, [(207, 73), (307, 73)], "#74787b", 11)
        line(draw, [(210, 69), (306, 69)], "#d2d4d5", 2.5)
        for i, color in enumerate((BLUE, PINK, ORANGE)):
            draw.rectangle(box(236 + i * 7, 66, 240 + i * 7, 81), fill=color)
        draw.rectangle(box(304, 65, 315, 82), fill="#292c2f")
        text(draw, (260, 87), "NO CAN", 5.5, FG, "mm")

    text(draw, (178, 104), "FLOW", 4.5, GRID)
    baseline = 124
    points = []
    for x in range(178, 313):
        envelope = (x - 178) / 135
        y = baseline - math.sin((x - 178) * .18) * (1.2 + envelope * (5 if kind == "open" else 3))
        points.append((x, y))
    line(draw, points, RED if kind == "open" else ORANGE, 1)
    line(draw, [(178, baseline), (313, baseline)], DIM, .5)


def draw_selector(draw, x, title, value, accent):
    draw.rectangle(box(x, 140, x + 153, 178), outline=accent, width=sc(1))
    text(draw, (x + 7, 144), title + "  ·  TAP NEXT", 4.5, GRID)
    text(draw, (x + 7, 158), value, 6.5, accent)
    text(draw, (x + 145, 159), ">", 8, accent, "mm")


def draw_throttle(draw, mode="active", remaining=3.0):
    accent = RED if mode == "active" else GRID
    draw.rectangle(box(5, 184, 314, 231), outline=accent, width=sc(1))
    text(draw, (14, 190), "THROTTLE INPUT", 4.5, GRID)
    if mode == "active":
        text(draw, (14, 201), "HOLD TO REV", 7, FG)
        text(draw, (14, 217), "RELEASE  =  AUTO OFF 3.0 s", 4.5, GRID)
    else:
        text(draw, (14, 201), "THROTTLE RELEASED", 6.5, FG)
        text(draw, (14, 216), f"AUTO OFF IN {remaining:.1f} s", 5.5, ORANGE)
        # A quiet countdown line communicates that no separate stop control is required.
        draw.rectangle(box(14, 225, 157, 228), fill=DIM)
        draw.rectangle(box(14, 225, 14 + 143 * remaining / 3.0, 228), fill=ORANGE)

    # Mechanical twist-grip, large enough to remain legible on the physical 320x240 screen.
    line(draw, [(176, 207), (201, 207)], "#bfc3c5", 4)
    draw.rectangle(box(196, 197, 295, 217), fill="#25292c", outline="#4d5154", width=sc(1))
    for x in range(202, 290, 8):
        draw.rectangle(box(x, 198, x + 3, 216), fill=BG)
    draw.rectangle(box(295, 196, 307, 218), fill=ORANGE if mode == "active" else "#6e4b2f")
    line(draw, [(175, 203), (194, 203)], "#e1e3e4", 1)
    text(draw, (252, 222), "PRESS + HOLD", 4.2, FG if mode == "active" else GRID, "ma")


def draw_footer(draw, exhaust_name, mode="active", remaining=3.0):
    draw_selector(draw, 5, "ENGINE", "2C  270 P-TWIN", BLUE)
    draw_selector(draw, 162, "EXHAUST", exhaust_name, ORANGE)
    draw_throttle(draw, mode, remaining)
    text(draw, (8, 234), "DESIGN PREVIEW  ·  ENGINE SIM VISUAL / MIT", 3.5, GRID)


def draw_settings_drawer(draw, volume=60, redline=9000, auto_off=3.0):
    """Draw the fully-open pull-down panel over the mechanical view."""
    draw.rectangle(box(0, 28, 319, 139), fill="#121517", outline=GRID, width=sc(1))
    text(draw, (10, 33), "QUICK SETTINGS", 7, FG)
    text(draw, (309, 34), "AUTO CLOSE 5 s", 4.2, GRID, "ra")
    line(draw, [(9, 47), (310, 47)], DIM, .6)

    def slider(y, label_value, value_text, ratio, accent):
        text(draw, (10, y), label_value, 5.5, FG)
        draw.rectangle(box(91, y + 2, 263, y + 6), fill=DIM)
        knob_x = 91 + 172 * ratio
        draw.rectangle(box(91, y + 2, knob_x, y + 6), fill=accent)
        draw.rectangle(box(knob_x - 2, y - 1, knob_x + 3, y + 9), fill=YELLOW)
        text(draw, (309, y - 1), value_text, 5.5, accent, "ra")

    slider(55, "VOLUME", f"{volume}%", volume / 100.0, BLUE)
    slider(82, "REDLINE", f"{redline} RPM", (redline - 6000) / 10000.0, ORANGE)
    slider(109, "AUTO OFF", f"{auto_off:.1f} s", (auto_off - 1.0) / 7.0, PINK)
    line(draw, [(140, 132), (180, 132)], GRID, 2)
    text(draw, (160, 135), "^", 4, GRID, "mm")


def render(kind, view="active"):
    image = Image.new("RGB", (W * S, H * S), BG)
    draw = ImageDraw.Draw(image)
    if view == "auto_off":
        draw_header(draw, "1200 RPM", "COAST", YELLOW)
    else:
        draw_header(draw)
    draw_engine_panel(image, draw)
    draw_exhaust(draw, kind)
    title = dict((key, name) for name, key in EXHAUSTS)[kind]
    if view == "auto_off":
        draw_footer(draw, title, "auto_off", 2.4)
    else:
        draw_footer(draw, title)
    if view == "drawer":
        draw_settings_drawer(draw)
    # BOX keeps the 4x layout grid crisp while still averaging diagonal geometry.
    exact = image.resize((W, H), Image.Resampling.BOX)
    return image, exact


def main():
    ASSETS.mkdir(parents=True, exist_ok=True)
    screens = []
    masters = {}
    for name, kind in EXHAUSTS:
        master, exact = render(kind)
        exact.save(ASSETS / f"firmware-ui-v2-{kind}.png", optimize=True)
        screens.append((name, exact))
        masters[name] = master

    # Smooth desktop design master plus exact screen pixels enlarged without interpolation.
    main_screen = dict(screens)["TIN CAN"]
    masters["TIN CAN"].save(ASSETS / "firmware-ui-v2-master.png", optimize=True)
    main_screen.resize((W * 4, H * 4), Image.Resampling.NEAREST).save(
        ASSETS / "firmware-ui-v2-review.png", optimize=True)

    drawer_master, drawer_exact = render("tin_can", "drawer")
    drawer_master.save(ASSETS / "firmware-ui-v2-drawer-master.png", optimize=True)
    drawer_exact.save(ASSETS / "firmware-ui-v2-drawer.png", optimize=True)
    drawer_exact.resize((W * 4, H * 4), Image.Resampling.NEAREST).save(
        ASSETS / "firmware-ui-v2-drawer-review.png", optimize=True)

    auto_off_master, auto_off_exact = render("tin_can", "auto_off")
    auto_off_master.save(ASSETS / "firmware-ui-v2-auto-off-master.png", optimize=True)
    auto_off_exact.save(ASSETS / "firmware-ui-v2-auto-off.png", optimize=True)
    auto_off_exact.resize((W * 4, H * 4), Image.Resampling.NEAREST).save(
        ASSETS / "firmware-ui-v2-auto-off-review.png", optimize=True)

    contact = Image.new("RGB", (W * 3, H * 2), BG)
    for i, (_, screen) in enumerate(screens):
        x = (i % 3) * W
        y = (i // 3) * H
        contact.paste(screen, (x, y))
    contact.resize((W * 3 * 2, H * 2 * 2), Image.Resampling.NEAREST).save(
        ASSETS / "firmware-ui-v2-exhausts.png", optimize=True)
    print(ASSETS / "firmware-ui-v2-master.png")
    print(ASSETS / "firmware-ui-v2-review.png")
    print(ASSETS / "firmware-ui-v2-drawer-review.png")
    print(ASSETS / "firmware-ui-v2-auto-off-review.png")
    print(ASSETS / "firmware-ui-v2-exhausts.png")


if __name__ == "__main__":
    main()
