#!/usr/bin/env python3
"""Render the high-resolution desktop design master for the LCKFB UI."""

from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs/assets/engine-sim-official-concept.png"
SOURCE = ROOT / "docs/third-party/engine-sim/official-engine-reference.png"

BG = "#0e1012"
FG = "#ffffff"
GRID = "#777b7e"
DIM = "#303336"
PINK = "#f394be"
RED = "#ee4445"
ORANGE = "#f4802a"
YELLOW = "#fdbd2e"
BLUE = "#77cee0"

FONT_PATH = "/System/Library/Fonts/SFNSMono.ttf"


def font(size: int):
    return ImageFont.truetype(FONT_PATH, size)


def label(draw, xy, text, size=24, color=FG, anchor=None):
    draw.text(xy, text, font=font(size), fill=color, anchor=anchor)


def frame(draw, box, title):
    draw.rectangle(box, outline=GRID, width=2)
    label(draw, (box[0] + 18, box[1] + 12), title, 25)


def exhaust(draw, kind, origin, scale=1.0):
    ox, oy = origin
    p = lambda x, y: (int(ox + x * scale), int(oy + y * scale))
    w = max(2, int(8 * scale))
    draw.line([p(0, 80), p(55, 125), p(110, 135)], fill=PINK, width=w)
    draw.line([p(0, 120), p(55, 135), p(110, 135)], fill=PINK, width=w)
    draw.line([p(55, 125), p(115, 165)], fill="#d0d2d3", width=max(3, int(13 * scale)))
    if kind == "tin_can":
        x0, y0 = p(105, 100)
        x1, y1 = p(385, 235)
        draw.rectangle((x0, y0, x1, y1), fill="#bc1830", outline="#e44b59", width=max(2, int(5 * scale)))
        draw.rectangle((*p(115, 88), *p(375, 105)), fill="#d9dddf")
        draw.rectangle((*p(115, 230), *p(375, 245)), fill="#aeb4b7")
        draw.line([p(130, 225), p(350, 110)], fill=FG, width=max(3, int(8 * scale)))
        draw.line([p(175, 240), p(390, 125)], fill=FG, width=max(2, int(5 * scale)))
        draw.ellipse((*p(135, 93), *p(175, 113)), outline=GRID, width=max(2, int(4 * scale)))
        label(draw, p(245, 165), "COLA", int(38 * scale), FG, "mm")
        draw.rectangle((*p(385, 128), *p(425, 205)), fill="#383b3e")
    elif kind == "stock":
        draw.rounded_rectangle((*p(105, 105), *p(405, 215)), radius=int(18 * scale), fill="#64686c", outline="#b8bbbd", width=max(2, int(5 * scale)))
        draw.rectangle((*p(385, 125), *p(430, 195)), fill="#c9cbcc")
    elif kind == "carbon":
        draw.polygon([p(105, 100), p(400, 120), p(365, 225), p(115, 215)], fill="#292c2f", outline="#64686c")
        for x in range(135, 360, 35):
            draw.line([p(x, 108), p(x + 65, 215)], fill="#4c5155", width=max(1, int(2 * scale)))
        draw.rectangle((*p(365, 125), *p(410, 210)), fill=RED)
    elif kind == "titanium":
        draw.polygon([p(105, 105), p(400, 85), p(375, 225), p(115, 215)], fill="#c79a59", outline="#eadaba")
        draw.rectangle((*p(120, 105), *p(145, 215)), fill="#765431")
        draw.rectangle((*p(365, 95), *p(395, 218)), fill="#eee5ce")
    else:
        draw.line([p(105, 160), p(420, 160)], fill="#777b7e", width=max(4, int(36 * scale)))
        draw.line([p(110, 147), p(415, 147)], fill="#d2d4d5", width=max(2, int(8 * scale)))
        for i, color in enumerate((BLUE, PINK, ORANGE)):
            draw.rectangle((*p(190 + i * 22, 130), *p(205 + i * 22, 190)), fill=color)


def main():
    image = Image.new("RGB", (1920, 1080), BG)
    draw = ImageDraw.Draw(image)

    draw.rectangle((0, 0, 1919, 104), outline=GRID, width=2)
    draw.polygon([(24, 18), (62, 18), (42, 88)], fill=FG)
    draw.polygon([(58, 18), (90, 18), (73, 88)], fill=FG)
    draw.rectangle((43, 61, 31 + 42, 70), fill=BG)
    label(draw, (112, 18), "ENGINE SIM / EV LAB", 35)
    label(draw, (114, 62), "OFFICIAL VISUAL STUDY  ·  LCKFB ESP32-S3", 18, GRID)
    label(draw, (1110, 24), "4200 RPM", 42, FG)
    draw.ellipse((1365, 43, 1381, 59), fill=RED)
    label(draw, (1400, 36), "ACCEL", 25, ORANGE)
    draw.rectangle((1670, 18, 1774, 86), outline=GRID, width=2)
    label(draw, (1722, 52), "START", 22, anchor="mm")
    draw.rectangle((1792, 18, 1895, 86), outline=GRID, width=2)
    label(draw, (1844, 52), "SET", 22, anchor="mm")

    engine = Image.open(SOURCE).convert("RGB")
    engine = engine.resize((750, 618), Image.Resampling.LANCZOS)
    image.paste(engine, (105, 125))
    frame(draw, (0, 104, 960, 750), "ENGINE CUTAWAY")
    label(draw, (930, 716), "V2 / LIVE FIRING", 18, GRID, "ra")

    frame(draw, (960, 104, 1285, 427), "IGNITION")
    for i, x in enumerate((1040, 1205)):
        draw.ellipse((x - 58, 220, x + 58, 336), outline=DIM, width=10)
        draw.ellipse((x - 47, 231, x + 47, 325), fill=FG if i else "#44484b")
        if i:
            draw.ellipse((x - 18, 260, x + 18, 296), fill=ORANGE)
        label(draw, (x, 360), f"CYL {i + 1}", 17, GRID, "mm")

    frame(draw, (960, 427, 1285, 750), "THROTTLE")
    draw.line((1020, 520, 1020, 675), fill=FG, width=7)
    draw.line((1225, 520, 1225, 675), fill=FG, width=7)
    draw.line((1060, 620, 1180, 585), fill=FG, width=8)
    draw.ellipse((1106, 585, 1138, 617), fill=FG)
    draw.rectangle((1055, 683, 1195, 710), fill="#2e3134")
    for x in range(1063, 1188, 18):
        draw.rectangle((x, 683, x + 7, 710), fill=BG)
    draw.rectangle((1195, 678, 1224, 715), fill=ORANGE)
    label(draw, (1120, 475), "72%", 27, BLUE, "mm")

    frame(draw, (1285, 104, 1919, 750), "EXHAUST / TIN CAN")
    exhaust(draw, "tin_can", (1325, 205), 1.3)
    label(draw, (1315, 590), "TOTAL EXHAUST FLOW", 22)
    points = []
    for x in range(1318, 1885, 8):
        amplitude = (x - 1318) / 567 * 42
        y = 680 - amplitude * __import__("math").sin((x - 1318) * 0.07)
        points.append((x, int(y)))
    draw.line(points, fill=ORANGE, width=4)
    draw.line((1318, 680, 1885, 680), fill=DIM, width=2)

    frame(draw, (0, 750, 960, 950), "ENGINE  ·  TAP NEXT")
    label(draw, (40, 840), "2C", 38, BLUE)
    label(draw, (150, 840), "270 P-TWIN", 38)
    label(draw, (905, 850), ">", 42, BLUE, "mm")
    frame(draw, (960, 750, 1919, 950), "EXHAUST  ·  TAP NEXT")
    names = [("OEM", "stock"), ("CF", "carbon"), ("TI", "titanium"), ("COLA", "tin_can"), ("OPEN", "open")]
    for i, (name, kind) in enumerate(names):
        x = 980 + i * 182
        selected = kind == "tin_can"
        draw.rectangle((x, 800, x + 165, 925), outline=ORANGE if selected else GRID, width=4 if selected else 2)
        exhaust(draw, kind, (x + 18, 807), 0.32)
        label(draw, (x + 82, 902), name, 17, ORANGE if selected else FG, "mm")

    label(draw, (24, 980), "REDLINE", 19, GRID)
    draw.rectangle((145, 990, 1420, 1004), fill=DIM)
    draw.rectangle((145, 990, 1080, 1004), fill=ORANGE)
    draw.rectangle((1070, 980, 1090, 1015), fill=YELLOW)
    label(draw, (1450, 981), "9000 RPM", 24)
    draw.rectangle((1640, 970, 1895, 1035), outline=RED, width=3)
    label(draw, (1768, 1002), "HOLD THROTTLE", 20, anchor="mm")
    label(draw, (24, 1045), "VISUAL SOURCE: ANGE YAGHI / ENGINE-SIM · MIT · COMMIT 85F7C3B", 14, GRID)

    OUT.parent.mkdir(parents=True, exist_ok=True)
    image.save(OUT, optimize=True)
    print(OUT)


if __name__ == "__main__":
    main()
