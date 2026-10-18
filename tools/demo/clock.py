#!/usr/bin/env python3
# Timelapse Switch
# SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
# SPDX-License-Identifier: GPL-2.0-or-later
"""clock.py: the neutral scene of the demo, an analog clock and a stopwatch, as a video file.

    clock.py --out clock.mp4 [--seconds 90] [--font /path/to/font.ttf]

Recorded through the plugin, the clock shows the speed at a glance: in real time the second hand
turns once a minute; at x8 it races. A short tick each second gives OBS some sound to record.
Needs Pillow and ffmpeg.
"""

import argparse
import math
import shutil
import subprocess
import sys

from PIL import Image, ImageDraw, ImageFont

WIDTH, HEIGHT, FPS = 1920, 1080, 30
INK = (31, 26, 23)          # Umber Ink, the brand's dark neutral
CANVAS = (245, 239, 230)    # Raw Canvas, the light neutral
TERRACOTTA = (210, 96, 58)  # the brand's primary color
MUTED = (120, 110, 102)


def default_font() -> str:
    """A sans-serif font of the system, found with fontconfig."""
    found = subprocess.run(["fc-match", "-f", "%{file}", "sans-serif:bold"], capture_output=True, text=True)
    return found.stdout.strip()


def hand(draw: ImageDraw.ImageDraw, center: tuple[int, int], angle: float, length: float, width: int,
         color: tuple[int, int, int]) -> None:
    """One hand of the clock; angle 0 points to 12, growing clockwise."""
    x = center[0] + length * math.sin(angle)
    y = center[1] - length * math.cos(angle)
    draw.line([center, (x, y)], fill=color, width=width)


def frame(t: float, font: ImageFont.FreeTypeFont) -> bytes:
    """The picture at t seconds."""
    image = Image.new("RGB", (WIDTH, HEIGHT), INK)
    draw = ImageDraw.Draw(image)
    center, radius = (WIDTH // 2, HEIGHT // 2 - 60), 360
    draw.ellipse([center[0] - radius, center[1] - radius, center[0] + radius, center[1] + radius],
                 outline=CANVAS, width=10)
    for mark in range(60):
        angle = mark * math.tau / 60
        outer, inner = radius - 22, radius - (70 if mark % 5 == 0 else 40)
        draw.line([(center[0] + inner * math.sin(angle), center[1] - inner * math.cos(angle)),
                   (center[0] + outer * math.sin(angle), center[1] - outer * math.cos(angle))],
                  fill=CANVAS if mark % 5 == 0 else MUTED, width=8 if mark % 5 == 0 else 4)
    hand(draw, center, (t / 43200) * math.tau, radius * 0.5, 16, CANVAS)  # hour hand: a turn in 12 hours
    hand(draw, center, (t / 3600) * math.tau, radius * 0.75, 12, CANVAS)  # minute hand: a turn per hour
    hand(draw, center, (t / 60) * math.tau, radius * 0.88, 6, TERRACOTTA)  # second hand: a turn per minute
    draw.ellipse([center[0] - 18, center[1] - 18, center[0] + 18, center[1] + 18], fill=TERRACOTTA)
    minutes, seconds = divmod(t, 60)
    text = f"{int(minutes):02d}:{seconds:04.1f}"
    box = draw.textbbox((0, 0), text, font=font)
    draw.text(((WIDTH - (box[2] - box[0])) / 2, center[1] + radius + 40), text, font=font, fill=CANVAS)
    return image.tobytes()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--out", required=True)
    parser.add_argument("--seconds", type=int, default=90)
    parser.add_argument("--font", default="")
    args = parser.parse_args()
    if shutil.which("ffmpeg") is None:
        print("clock.py: ffmpeg is needed", file=sys.stderr)
        return 1
    font = ImageFont.truetype(args.font or default_font(), 96)
    tick = "0.25*sin(2*PI*1200*t)*lt(mod(t\\,1)\\,0.025)"
    ffmpeg = subprocess.Popen(
        ["ffmpeg", "-v", "error", "-y",
         "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", f"{WIDTH}x{HEIGHT}", "-r", str(FPS), "-i", "-",
         "-f", "lavfi", "-i", f"aevalsrc={tick}:s=48000:d={args.seconds}",
         "-c:v", "libx264", "-preset", "veryfast", "-crf", "18", "-pix_fmt", "yuv420p",
         "-c:a", "aac", "-b:a", "128k", "-shortest", args.out],
        stdin=subprocess.PIPE)
    for n in range(args.seconds * FPS):
        ffmpeg.stdin.write(frame(n / FPS, font))
    ffmpeg.stdin.close()
    return ffmpeg.wait()


if __name__ == "__main__":
    sys.exit(main())
