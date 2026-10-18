#!/usr/bin/env python3
# Timelapse Switch
# SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
# SPDX-License-Identifier: GPL-2.0-or-later
"""media.py: the demo's files, from the plugin's recording and its OBS log.

    media.py --recording <file> --log <obs log> --font <ttf> --gif <out.gif> --webm <out.webm>
             --poster <out.png>

Each section of the recording gets a label in the corner ("Real time", "Timelapse x8"), placed at
the times the plugin logged, so the label always matches what the clock shows. Needs ffmpeg.
"""

import argparse
import re
import subprocess
import sys


def sections(log_path: str) -> list[tuple[float, float, int]]:
    """(start, end, speed) of each section in the final video, in seconds, from the plugin's log."""
    events, stop = [(0.0, 1)], None
    with open(log_path, encoding="utf-8", errors="replace") as log:
        for line in log:
            if m := re.search(r"\[timelapse-switch\] switch: speed x(\d+) at ([\d.]+) s", line):
                events.append((float(m.group(2)), int(m.group(1))))
            elif m := re.search(r"\[timelapse-switch\] recording stopped at ([\d.]+) s", line):
                stop = float(m.group(1))
    if stop is None:
        raise SystemExit("media.py: the log has no 'recording stopped' line")
    out, position = [], 0.0
    for (start, speed), (end, _) in zip(events, events[1:] + [(stop, 0)]):
        length = (end - start) / speed
        out.append((position, position + length, speed))
        position += length
    return out


def labels(parts: list[tuple[float, float, int]], font: str) -> str:
    """An ffmpeg drawtext chain: one label per section, shown only during it."""
    filters = []
    for start, end, speed in parts:
        text = "Real time" if speed == 1 else f"Timelapse x{speed}"
        color = "0xF5EFE6" if speed == 1 else "0xFFB067"
        filters.append(f"drawtext=fontfile='{font}':text='{text}':x=48:y=40:fontsize=64:fontcolor={color}:"
                       f"box=1:boxcolor=0x1F1A17@0.85:boxborderw=24:enable='between(t,{start:.3f},{end:.3f})'")
    return ",".join(filters)


def run(*command: str) -> None:
    subprocess.run(["ffmpeg", "-v", "error", "-y", *command], check=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    for name in ("recording", "log", "font", "gif", "webm", "poster"):
        parser.add_argument(f"--{name}", required=True)
    args = parser.parse_args()
    parts = sections(args.log)
    text = labels(parts, args.font)
    run("-i", args.recording, "-an", "-vf", f"{text},scale=1280:-2", "-c:v", "libvpx-vp9", "-b:v", "0",
        "-crf", "34", "-row-mt", "1", args.webm)
    run("-i", args.recording, "-vf", f"{text},fps=12,scale=720:-2:flags=lanczos,split[a][b];"
        "[a]palettegen=max_colors=48[p];[b][p]paletteuse=dither=bayer:bayer_scale=4", args.gif)
    # The seek goes after the input: the labels' times are the recording's own.
    run("-i", args.recording, "-ss", f"{parts[1][0] + 1.0:.3f}", "-frames:v", "1", "-vf",
        f"{text},scale=1280:-2", args.poster)
    for start, end, speed in parts:
        print(f"  {start:6.2f} s to {end:6.2f} s: x{speed}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
