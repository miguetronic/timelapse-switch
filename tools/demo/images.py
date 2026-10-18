#!/usr/bin/env python3
# Timelapse Switch
# SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
# SPDX-License-Identifier: GPL-2.0-or-later
"""images.py: the dock pictures and the social preview for the README and the website.

    images.py --screenshots build_x86_64/ui-screenshots --logo <logo-dark PNG> --font <ttf>
              --out site/src/assets/dock --social docs/images/social-preview.png

The dock pictures come from tests/ui/obs-themes.sh (the dock as OBS draws it, in OBS's own themes),
cropped to the dock's height. The social preview (1280 x 640, the size GitHub asks for) puts the
logo, the value line and the dock in timelapse side by side. Needs Pillow.
"""

import argparse
import os
import sys

from PIL import Image, ImageDraw, ImageFont

INK = (31, 26, 23)
CANVAS = (245, 239, 230)
MUTED = (190, 178, 166)

# (theme, state) pairs shown on the website, in OBS's default dark theme and in its light theme.
STATES = ("idle", "real-time", "timelapse", "paused-choice", "summary", "details")
THEMES = {"Yami.Original": "dark", "Yami.Light": "light"}


def crop_to_content(image: Image.Image, margin: int = 12) -> Image.Image:
    """The picture without the empty rows of background under the dock."""
    background = image.getpixel((image.width - 1, image.height - 1))
    rgb = image.convert("RGB")
    for y in range(rgb.height - 1, -1, -1):
        if any(rgb.getpixel((x, y)) != background for x in range(0, rgb.width, 2)):
            return image.crop((0, 0, image.width, min(image.height, y + 1 + margin)))
    return image


def dock_pictures(screenshots: str, out: str) -> Image.Image:
    """Crop and copy the chosen states; return the dark timelapse picture for the social preview."""
    os.makedirs(out, exist_ok=True)
    chosen = None
    for theme, tone in THEMES.items():
        for state in STATES:
            source = os.path.join(screenshots, theme, f"en-US-300-{state}.png")
            picture = crop_to_content(Image.open(source))
            picture.save(os.path.join(out, f"dock-{state}-{tone}.png"), optimize=True)
            if theme == "Yami.Original" and state == "timelapse":
                chosen = picture
    return chosen


def social_preview(logo_path: str, font_path: str, dock: Image.Image, out: str) -> None:
    image = Image.new("RGB", (1280, 640), INK)
    logo = Image.open(logo_path).convert("RGBA")
    logo = logo.crop(logo.getbbox())  # the PNG has a transparent margin around the logo
    logo = logo.resize((560, int(560 * logo.height / logo.width)), Image.LANCZOS)
    image.paste(logo, (80, 130), logo)
    draw = ImageDraw.Draw(image)
    title = ImageFont.truetype(font_path, 50)
    small = ImageFont.truetype(font_path, 26)
    draw.text((80, 130 + logo.height + 70), "Speed up the slow parts.\nWhile you record.", font=title,
              fill=CANVAS, spacing=12)
    draw.text((80, 548), "Free plugin for OBS Studio  ·  GPL-2.0-or-later", font=small, fill=MUTED)
    dock = dock.convert("RGBA")
    dock = dock.resize((int(dock.width * 1.25), int(dock.height * 1.25)), Image.LANCZOS)
    top = max(40, (640 - dock.height) // 2)
    image.paste(dock.crop((0, 0, dock.width, min(dock.height, 640 - top - 40))), (1280 - dock.width - 90, top))
    image.save(out, optimize=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    for name in ("screenshots", "logo", "font", "out", "social"):
        parser.add_argument(f"--{name}", required=True)
    args = parser.parse_args()
    dock = dock_pictures(args.screenshots, args.out)
    social_preview(args.logo, args.font, dock, args.social)
    print(f"dock pictures in {args.out}, social preview in {args.social}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
