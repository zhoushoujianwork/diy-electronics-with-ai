#!/usr/bin/env python3
"""Build deterministic, screen-sized image assets for the SquareLine project."""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont, ImageOps


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "docs/third-party/engine-sim/official-engine-reference.png"
OUTPUT = ROOT / "ui/squareline/assets/engine-cutaway.png"
ROTOR_OUTPUT = ROOT / "ui/squareline/assets/engine-rotor.png"
FONT = Path("/System/Library/Fonts/SFNSMono.ttf")

SCALE = 4
WIDTH = 172
HEIGHT = 108
BACKGROUND = "#0e1012"
FOREGROUND = "#f7f7f7"
GRID = "#777b7e"
ROTOR_PIVOT = (88, 86)
ROTOR_SIZE = 48


def split_rotor(source: Image.Image) -> tuple[Image.Image, Image.Image]:
    """Separate the official crank web from the static cutaway.

    Engine Simulator publishes this view as a raster reference, so the
    moving crank web has to be isolated by its neutral-grey palette.  The
    two geometric guards keep grey antialiasing from the white con-rods out
    of the rotating layer.
    """
    background = source.getpixel((0, 0))
    mask = Image.new("L", source.size, 0)
    pixels = source.load()
    mask_pixels = mask.load()
    for y in range(330, 520):
        for x in range(250, 470):
            left_web = ((x - 330) / 72) ** 2 + ((y - 425) / 80) ** 2 <= 1
            right_web = ((x - 388) / 62) ** 2 + ((y - 410) / 52) ** 2 <= 1
            if not (left_web or right_web):
                continue
            red, green, blue = pixels[x, y]
            luminance = (red + green + blue) / 3
            if max(red, green, blue) - min(red, green, blue) <= 8 and 25 <= luminance <= 210:
                mask_pixels[x, y] = 255

    rotor = source.convert("RGBA")
    rotor.putalpha(mask)
    base = source.copy()
    base_pixels = base.load()
    for y in range(330, 520):
        for x in range(250, 470):
            if mask_pixels[x, y]:
                base_pixels[x, y] = background
    return base, rotor


def fit_reference(source: Image.Image) -> Image.Image:
    return ImageOps.fit(
        source,
        (160 * SCALE, 88 * SCALE),
        method=Image.Resampling.LANCZOS,
        centering=(0.5, 0.54),
    )


def scaled_font(points: float) -> ImageFont.FreeTypeFont:
    return ImageFont.truetype(FONT, round(points * SCALE))


def main() -> None:
    if not SOURCE.is_file():
        raise SystemExit(f"Missing Engine Simulator reference: {SOURCE}")

    canvas = Image.new("RGB", (WIDTH * SCALE, HEIGHT * SCALE), BACKGROUND)
    source = Image.open(SOURCE).convert("RGB")
    base, rotor = split_rotor(source)

    # Keep the official geometry intact. Only near-black breathing room is
    # cropped before a high-quality downsample into the physical panel.
    engine = fit_reference(base)
    canvas.paste(engine, (6 * SCALE, 16 * SCALE))

    draw = ImageDraw.Draw(canvas)
    draw.rectangle(
        (0, 0, WIDTH * SCALE - 1, HEIGHT * SCALE - 1),
        outline=GRID,
        width=SCALE,
    )
    draw.rectangle((SCALE, SCALE, 102 * SCALE, 14 * SCALE), fill=BACKGROUND)
    draw.text(
        (5 * SCALE, 3 * SCALE),
        "ENGINE CUTAWAY",
        font=scaled_font(6.5),
        fill=FOREGROUND,
    )

    # SquareLine converts this RGB image to the project's RGB565 format. The
    # Lanczos reduction happens first so the final 172x108 asset retains
    # antialiased mechanical edges instead of the old integer-pixel geometry.
    canvas = canvas.resize((WIDTH, HEIGHT), Image.Resampling.LANCZOS)
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(OUTPUT, optimize=True)
    print(f"rendered {OUTPUT} ({WIDTH}x{HEIGHT}, RGB)")

    rotor_canvas = Image.new("RGBA", (WIDTH * SCALE, HEIGHT * SCALE), (0, 0, 0, 0))
    rotor_canvas.alpha_composite(fit_reference(rotor), (6 * SCALE, 16 * SCALE))
    rotor_canvas = rotor_canvas.resize((WIDTH, HEIGHT), Image.Resampling.LANCZOS)
    half = ROTOR_SIZE // 2
    left = ROTOR_PIVOT[0] - half
    top = ROTOR_PIVOT[1] - half
    rotor_asset = Image.new("RGBA", (ROTOR_SIZE, ROTOR_SIZE), (0, 0, 0, 0))
    rotor_asset.alpha_composite(rotor_canvas, (-left, -top))
    rotor_asset.save(ROTOR_OUTPUT, optimize=True)
    print(f"rendered {ROTOR_OUTPUT} ({ROTOR_SIZE}x{ROTOR_SIZE}, RGBA)")


if __name__ == "__main__":
    main()
