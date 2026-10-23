#!/usr/bin/env python3
# Timelapse Switch
# SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
# SPDX-License-Identifier: GPL-2.0-or-later
"""guide-art.py: the pictures of the install guide (site/src/content/docs/guide/install.mdx).

    guide-art.py --out site/src/assets/install [--installer <capture.png>] [--finish <capture.png>]
                 [--docks <capture.png>]

Draws the windows a person meets while installing: Windows SmartScreen, Windows Settings, macOS's
security messages, a Linux file manager. They are illustrations, not screenshots: plain and the same
on every system version, with what to click outlined in the site's orange and numbered. The
real windows look a little different on each version; the words and the places are the same.

With the optional captures, it also marks up real screenshots: the plugin's Windows installer
(tools/package/make-msi.sh, run in Wine and captured without the pointer: the first window with
"I accept" ticked, and the last one) and OBS's Docks menu with Timelapse Switch (the sandbox OBS of tools/make-demo.sh, menu
open). Needs Pillow for those.
"""

import argparse
import pathlib
import sys
from xml.sax.saxutils import escape

ACCENT = "#d2603a"
FONT = "Segoe UI, system-ui, -apple-system, Ubuntu, Cantarell, sans-serif"
INK = "#1f1f1f"
MUTED = "#5f5f5f"


class Svg:
    """An illustration, in the order things are added; text sizes in px of the illustration."""

    def __init__(self, width: int, height: int, title: str):
        self.width, self.height, self.title, self.parts = width, height, title, []

    def add(self, part: str) -> None:
        self.parts.append(part)

    def rect(self, x, y, w, h, fill, stroke="none", rx=0, width=1) -> None:
        self.add(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" fill="{fill}" '
                 f'stroke="{stroke}" stroke-width="{width}"/>')

    def text(self, x, y, words, size=15, fill=INK, weight=400, anchor="start", underline=False) -> None:
        deco = ' text-decoration="underline"' if underline else ""
        self.add(f'<text x="{x}" y="{y}" font-size="{size}" fill="{fill}" font-weight="{weight}" '
                 f'text-anchor="{anchor}"{deco}>{escape(words)}</text>')

    def lines(self, x, y, rows, size=15, fill=INK, gap=1.4, weight=400) -> None:
        for i, row in enumerate(rows):
            self.text(x, y + i * size * gap, row, size, fill, weight)

    def button(self, x, y, w, h, label, fill="#ffffff", ink=INK, stroke="#8a8a8a", rx=4, size=15) -> None:
        self.rect(x, y, w, h, fill, stroke, rx)
        self.text(x + w / 2, y + h / 2 + size * 0.35, label, size, ink, 500, "middle")

    def mark(self, x, y, w, h, number: int, side="right") -> None:
        """What to click: an orange outline and its step number."""
        self.rect(x - 5, y - 5, w + 10, h + 10, "none", ACCENT, 8, 3)
        cx = x + w + 22 if side == "right" else x - 22
        cy = y + h / 2
        if side == "top":
            cx, cy = x + w / 2, y - 22
        self.add(f'<circle cx="{cx}" cy="{cy}" r="14" fill="{ACCENT}"/>')
        self.text(cx, cy + 5.5, str(number), 16, "#ffffff", 700, "middle")

    def window(self, x, y, w, h, title="", bar=34, fill="#ffffff", bar_fill="#f3f3f3", rx=10) -> None:
        self.rect(x, y, w, h, fill, "#c4c4c4", rx)
        if bar:
            self.add(f'<path d="M{x} {y + bar} V{y + rx} a{rx} {rx} 0 0 1 {rx} -{rx} H{x + w - rx} '
                     f'a{rx} {rx} 0 0 1 {rx} {rx} V{y + bar} Z" fill="{bar_fill}"/>')
            self.add(f'<line x1="{x}" y1="{y + bar}" x2="{x + w}" y2="{y + bar}" stroke="#d6d6d6"/>')
            if title:
                self.text(x + 14, y + bar / 2 + 5, title, 14, MUTED)

    def folder(self, x, y, label, size=44, color="#e8b04b") -> None:
        s = size / 44
        self.add(f'<path transform="translate({x} {y}) scale({s})" d="M0 6 a4 4 0 0 1 4 -4 h12 l5 5 h19 '
                 f'a4 4 0 0 1 4 4 v25 a4 4 0 0 1 -4 4 h-36 a4 4 0 0 1 -4 -4 Z" fill="{color}"/>')
        self.text(x + size / 2, y + size + 14, label, 13, INK, 400, "middle")

    def save(self, path: pathlib.Path) -> None:
        body = "\n".join(self.parts)
        path.write_text(
            f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {self.width} {self.height}" '
            f'width="{self.width}" height="{self.height}" font-family="{FONT}" role="img">\n'
            f'<title>{escape(self.title)}</title>\n{body}\n</svg>\n', encoding="utf-8")


def smartscreen(out: pathlib.Path, step: int) -> None:
    """Windows SmartScreen: 1, "More info"; 2, "Run anyway"."""
    blue, white = "#1f5fa8", "#ffffff"
    name = "smartscreen-more-info" if step == 1 else "smartscreen-run-anyway"
    svg = Svg(560, 330, "Windows protected your PC: click More info, then Run anyway")
    svg.rect(0, 0, 560, 330, blue, rx=8)
    svg.text(32, 62, "Windows protected your PC", 28, white, 300)
    svg.lines(32, 104, ["Microsoft Defender SmartScreen prevented an unrecognized",
                        "app from starting. Running this app might put your PC at risk."], 15, white)
    if step == 1:
        svg.text(32, 162, "More info", 15, white, 600, underline=True)
        svg.mark(32, 148, 70, 20, 1)
        svg.button(428, 272, 104, 34, "Don't run", blue, white, white, 2)
    else:
        svg.lines(32, 162, ["App:", "Publisher:"], 15, white)
        svg.lines(118, 162, ["timelapse-switch-windows-x64.msi", "Unknown publisher"], 15, white)
        svg.button(306, 272, 110, 34, "Run anyway", blue, white, white, 2)
        svg.button(428, 272, 104, 34, "Don't run", blue, white, white, 2)
        svg.mark(306, 272, 110, 34, 2, "top")
    svg.save(out / f"windows-{name}.svg")


def windows_uninstall(out: pathlib.Path) -> None:
    svg = Svg(620, 300, "Windows Settings, Apps, Installed apps: Timelapse Switch, the three dots, Uninstall")
    svg.window(0, 0, 620, 300, "Settings")
    svg.text(28, 76, "Apps  ›  Installed apps", 22, INK, 600)
    svg.rect(28, 98, 564, 64, "#f7f7f7", "#e2e2e2", 6)
    svg.rect(44, 114, 32, 32, "#1f1a17", rx=6)
    svg.add('<path d="M50 125 h7 v6 l4 -6 l4 6 v-6 h5 v10 h-5 l-4 -6 l-4 6 v-4 h-7 Z" fill="#d2603a"/>')
    svg.text(90, 126, "Timelapse Switch for OBS Studio", 15, INK, 600)
    svg.text(90, 146, "Miguetronic", 13, MUTED)
    svg.text(530, 136, "•••", 16, INK, 700, "middle")
    svg.mark(512, 120, 36, 22, 1)
    svg.rect(400, 176, 180, 44, "#ffffff", "#d0d0d0", 6)
    svg.text(420, 204, "Uninstall", 15, INK)
    svg.mark(408, 182, 164, 32, 2)
    svg.lines(28, 258, ["Windows Settings  ›  Apps  ›  Installed apps"], 13, MUTED)
    svg.save(out / "windows-uninstall.svg")


def macos_blocked(out: pathlib.Path) -> None:
    svg = Svg(420, 360, "macOS: timelapse-switch-macos.pkg Not Opened, click Done")
    svg.rect(0, 0, 420, 360, "#ececec", "#c4c4c4", 14)
    svg.rect(178, 26, 64, 64, "#ffffff", "#cfcfcf", 12)
    svg.add('<path d="M196 50 h12 v8 l7 -8 l7 8 v-8 h10 v18 h-10 l-7 -8 l-7 8 v-6 h-12 Z" fill="#d2603a"/>')
    svg.text(210, 122, "“timelapse-switch-macos.pkg”", 16, INK, 700, "middle")
    svg.text(210, 144, "Not Opened", 16, INK, 700, "middle")
    for i, row in enumerate(["Apple could not verify “timelapse-switch-", "macos.pkg” is free of malware that may harm",
                             "your Mac or compromise your privacy."]):
        svg.text(210, 176 + i * 19, row, 13, INK, 400, "middle")
    svg.button(28, 276, 364, 30, "Done", "#2f7cf6", "#ffffff", "#2f7cf6", 7, 14)
    svg.button(28, 314, 364, 30, "Move to Trash", "#ffffff", INK, "#cfcfcf", 7, 14)
    svg.mark(28, 276, 364, 30, 1, "top")
    svg.save(out / "macos-not-opened.svg")


def macos_open_anyway(out: pathlib.Path) -> None:
    svg = Svg(640, 330, "macOS System Settings, Privacy & Security: click Open Anyway")
    svg.window(0, 0, 640, 330, "", bar=0, fill="#ffffff")
    svg.add('<path d="M10 0 H190 V330 H10 a10 10 0 0 1 -10 -10 V10 a10 10 0 0 1 10 -10 Z" fill="#f0f0f0"/>')
    for i, c in enumerate(["#ff5f57", "#febc2e", "#28c840"]):
        svg.add(f'<circle cx="{20 + i * 20}" cy="20" r="6" fill="{c}"/>')
    for i, item in enumerate(["Wi-Fi", "Bluetooth", "Network", "General", "Appearance", "Privacy & Security",
                              "Desktop & Dock", "Displays"]):
        y = 64 + i * 32
        if item == "Privacy & Security":
            svg.rect(10, y - 21, 170, 30, "#2f7cf6", rx=6)
        svg.text(22, y, item, 14, "#ffffff" if item == "Privacy & Security" else INK)
    svg.text(214, 44, "Privacy & Security", 19, INK, 700)
    svg.text(214, 100, "Security", 15, INK, 700)
    svg.rect(214, 114, 400, 120, "#f6f6f6", "#e2e2e2", 8)
    svg.lines(232, 146, ["“timelapse-switch-macos.pkg” was blocked to", "protect your Mac."], 14)
    svg.button(474, 190, 122, 28, "Open Anyway", "#ffffff", INK, "#bdbdbd", 6, 14)
    svg.mark(474, 190, 122, 28, 2, "top")
    svg.lines(214, 276, ["Scroll down to Security. The message stays for about an hour", "after you tried to open the file."],
              13, MUTED)
    svg.save(out / "macos-open-anyway.svg")


def linux_address(out: pathlib.Path) -> None:
    svg = Svg(620, 330, "A file manager: press Ctrl+L, type ~/.config/obs-studio, then open plugins")
    svg.window(0, 0, 620, 330, "", bar=52)
    svg.rect(120, 11, 380, 30, "#ffffff", "#9a9a9a", 6)
    svg.text(132, 31, "~/.config/obs-studio", 15, INK)
    svg.add(f'<line x1="279" y1="17" x2="279" y2="36" stroke="{INK}" stroke-width="1.5"/>')
    svg.mark(120, 11, 380, 30, 1)
    svg.text(26, 31, "Ctrl + L", 13, MUTED, 600)
    for i, name in enumerate(["basic", "logs", "plugin_config", "plugins"]):
        x = 40 + i * 140
        svg.folder(x, 104, name, 56)
        if name == "plugins":
            svg.mark(x - 18, 96, 92, 92, 2, "top")
    svg.lines(26, 262, ["Your OBS settings live here. If there is no “plugins” folder,",
                        "create it: right click, New Folder, plugins."], 14, MUTED)
    svg.save(out / "linux-address.svg")


def linux_copy(out: pathlib.Path) -> None:
    svg = Svg(660, 300, "Drag the timelapse-switch folder from the zip into the plugins folder")
    svg.window(0, 20, 290, 250, "timelapse-switch-linux-x86_64.zip")
    svg.folder(50, 92, "timelapse-switch", 56)
    svg.add('<g transform="translate(176 92)"><path d="M0 0 h30 l12 12 v44 h-42 Z" fill="#ffffff" stroke="#9a9a9a"/>'
            '<path d="M30 0 v12 h12" fill="none" stroke="#9a9a9a"/></g>')
    svg.text(197, 168, "How to install.txt", 13, INK, 400, "middle")
    svg.mark(24, 86, 108, 92, 3, "top")
    svg.window(370, 20, 290, 250, "plugins")
    svg.rect(440, 84, 150, 110, "#fbe9e2", ACCENT, 10, 2)
    svg.text(515, 216, "drop it here", 14, ACCENT, 600, "middle")
    svg.add(f'<path d="M140 130 C 240 60, 330 60, 432 128" fill="none" stroke="{ACCENT}" stroke-width="4"/>'
            f'<path d="M420 116 L436 132 L414 138 Z" fill="{ACCENT}"/>')
    svg.save(out / "linux-copy.svg")


def mark_png(source: str, out: pathlib.Path, crop: tuple, marks: list, scale: float = 1.0) -> None:
    """A real screenshot, cropped, with orange numbered outlines: marks are (x, y, w, h, number, side), the
    number on the right of the outline, or "top" above it."""
    from PIL import Image, ImageDraw, ImageFont

    picture = Image.open(source).convert("RGB").crop(crop)
    if scale != 1.0:
        picture = picture.resize((round(picture.width * scale), round(picture.height * scale)),
                                 Image.Resampling.LANCZOS)
    draw = ImageDraw.Draw(picture)
    try:
        font = ImageFont.truetype("DejaVuSans-Bold.ttf", 17)
    except OSError:
        font = ImageFont.load_default()
    for x, y, w, h, number, side in marks:
        x, y, w, h = ((v - o) * scale for v, o in zip((x, y, x + w, y + h), (crop[0], crop[1], crop[0], crop[1])))
        draw.rounded_rectangle((x - 5, y - 5, w + 5, h + 5), 8, outline=ACCENT, width=3)
        cx, cy = w + 24, (y + h) / 2
        if side == "top":
            cx, cy = (x + w) / 2, y - 24
        elif cx + 15 > picture.width:
            cx = x - 24
        draw.ellipse((cx - 14, cy - 14, cx + 14, cy + 14), fill=ACCENT)
        draw.text((cx, cy), str(number), fill="white", font=font, anchor="mm")
    picture.save(out, optimize=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--out", required=True)
    parser.add_argument("--installer", help="Wine capture of the installer's first window, 'I accept' ticked")
    parser.add_argument("--finish", help="Wine capture of the installer's last window")
    parser.add_argument("--docks", help="capture of the sandbox OBS with the Docks menu open")
    args = parser.parse_args()
    out = pathlib.Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    smartscreen(out, 1)
    smartscreen(out, 2)
    windows_uninstall(out)
    macos_blocked(out)
    macos_open_anyway(out)
    linux_address(out)
    linux_copy(out)
    # Captures of a 900 x 600 screen with the installer at (204, 120), and of the 1600 x 900 one
    # of tools/make-demo.sh with OBS's window at (0, 25).
    if args.installer:
        mark_png(args.installer, out / "windows-installer.png", (204, 120, 697, 480),
                 [(377, 400, 256, 16, 1, "right"), (488, 445, 106, 22, 2, "top")])
    if args.finish:
        mark_png(args.finish, out / "windows-finish.png", (204, 120, 697, 480), [(522, 445, 72, 22, 3, "top")])
    if args.docks:
        mark_png(args.docks, out / "obs-docks.png", (0, 25, 1281, 520),
                 [(124, 28, 48, 22, 1, "right"), (126, 314, 156, 24, 2, "right"), (972, 54, 308, 440, 3, "right")],
                 0.75)
    return 0


if __name__ == "__main__":
    sys.exit(main())
