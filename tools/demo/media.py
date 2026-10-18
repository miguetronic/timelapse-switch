#!/usr/bin/env python3
# Timelapse Switch
# SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
# SPDX-License-Identifier: GPL-2.0-or-later
"""media.py: the demo's files, from a screen capture of the sandbox OBS and the times of its clicks.

    media.py --take <capture.mkv> --marks <marks.txt> --font <ttf> --gif <out.gif> --webm <out.webm>
             --poster <out.png>

The capture shows OBS's preview and the dock while the plugin records. OBS's preview always runs in
real time, so the timelapse stretch of the capture is sped up x8 here, the same way the plugin
speeds up the file: the viewer sees what the finished video holds. marks.txt has one "name seconds"
line per click (record, timelapse, realtime, stop, end), in seconds since the capture started.
Each stretch gets a label in the corner ("Real time", "Timelapse x8"). Needs ffmpeg.
"""

import argparse
import subprocess
import sys

# The part of the 1280 x 875 capture to keep: the preview and the dock.
CROP = "crop=990:508:290:27"
# OBS's zoom bar under the preview, painted over with the preview's background.
ZOOM_BAR = "drawbox=x=0:y=478:w=668:h=30:color=0x13141a:t=fill"
# A click shows up in the capture this much earlier than its mark (measured on the frames).
LAG_S = 0.2
SPEED = 8


def marks(path: str) -> dict[str, float]:
    with open(path, encoding="utf-8") as lines:
        return {name: float(seconds) - LAG_S for name, seconds in (line.split() for line in lines)}


def label(text: str, font: str) -> str:
    color = "0xF5EFE6" if text == "Real time" else "0xFFB067"
    return (f"drawtext=fontfile='{font}':text='{text}':x=26:y=104:fontsize=28:fontcolor={color}:"
            "box=1:boxcolor=0x1F1A17@0.85:boxborderw=10")


def edit(take: str, t: dict[str, float], font: str, out: str) -> None:
    """One video: 0.8 s before the first click to 4 s after Stop, the timelapse stretch x8."""
    start, switch, back, end = 0.8, t["timelapse"], t["realtime"], t["stop"] + 4.2
    graph = (f"[0:v]{CROP},{ZOOM_BAR},split=3[a][b][c];"
             f"[a]trim={start}:{switch},setpts=PTS-STARTPTS,{label('Real time', font)}[A];"
             f"[b]trim={switch}:{back},setpts=(PTS-STARTPTS)/{SPEED},{label(f'Timelapse ×{SPEED}', font)}[B];"
             f"[c]trim={back}:{end},setpts=PTS-STARTPTS,{label('Real time', font)}[C];"
             "[A][B][C]concat=n=3:v=1,fps=30[v]")
    run("-i", take, "-filter_complex", graph, "-map", "[v]", "-c:v", "libx264", "-preset", "slow", "-crf",
        "16", "-pix_fmt", "yuv420p", out)


def run(*command: str) -> None:
    subprocess.run(["ffmpeg", "-v", "error", "-y", *command], check=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    for name in ("take", "marks", "font", "gif", "webm", "poster"):
        parser.add_argument(f"--{name}", required=True)
    args = parser.parse_args()
    edited = args.take + ".edited.mp4"
    edit(args.take, marks(args.marks), args.font, edited)
    run("-i", edited, "-vf", "fps=15,scale=820:-1:flags=lanczos,split[a][b];[a]palettegen=max_colors=128:"
        "stats_mode=diff[p];[b][p]paletteuse=dither=bayer:bayer_scale=4:diff_mode=rectangle", args.gif)
    run("-i", edited, "-an", "-c:v", "libvpx-vp9", "-b:v", "0", "-crf", "34", "-row-mt", "1", args.webm)
    run("-ss", "3", "-i", edited, "-frames:v", "1", args.poster)
    return 0


if __name__ == "__main__":
    sys.exit(main())
