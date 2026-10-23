#!/usr/bin/env python3
# Timelapse Switch
# SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
# SPDX-License-Identifier: GPL-2.0-or-later
"""make-installer-art.py: the pictures of the Windows installer's windows, from the plugin's icon.

    tools/package/make-installer-art.py

Writes, next to this script, the two pictures make-msi.sh puts into the installer instead of the
toolkit's own: installer-dialog.bmp (493 x 312, the welcome and last windows: a dark band on the
left with the icon, white where the text goes) and installer-banner.bmp (493 x 58, the top of the
other windows: white, the icon on the right). Run it again only if the icon changes. Needs Pillow.
"""

import pathlib
import sys

from PIL import Image

HERE = pathlib.Path(__file__).resolve().parent
DARK = (31, 26, 23)  # the icon's own background
WHITE = (255, 255, 255)


def icon(size: int) -> Image.Image:
    with Image.open(HERE / "timelapse-switch.ico") as ico:
        ico.size = (256, 256)
        return ico.convert("RGBA").resize((size, size), Image.Resampling.LANCZOS)


def save(picture: Image.Image, name: str) -> None:
    # 8-bit palette: the installer stays small, and the colors are few.
    picture.convert("RGB").quantize(colors=256, method=Image.Quantize.MEDIANCUT).save(HERE / name)


def main() -> int:
    # The welcome and last windows: the text starts 164 px from the left (130 dialog units).
    dialog = Image.new("RGB", (493, 312), WHITE)
    dialog.paste(DARK, (0, 0, 164, 312))
    mark = icon(112)
    dialog.paste(mark, ((164 - 112) // 2, 56), mark)
    save(dialog, "installer-dialog.bmp")

    banner = Image.new("RGB", (493, 58), WHITE)
    mark = icon(44)
    banner.paste(mark, (493 - 44 - 8, 7), mark)
    save(banner, "installer-banner.bmp")
    return 0


if __name__ == "__main__":
    sys.exit(main())
