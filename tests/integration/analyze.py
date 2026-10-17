#!/usr/bin/env python3
# Timelapse Switch
# SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
# SPDX-License-Identifier: GPL-2.0-or-later
"""analyze.py: check a recording made from the test media (make-test-media.sh).

    analyze.py --recording <file> --log <obs log file> [--allow-lost-frames 0.0]

Every frame of the test media spells its own number in 16 bars, and a beep starts on
every frame whose number is a multiple of 30. So the recording can be checked against
what really happened, as logged by the plugin (start, each switch, stop) and by the sync
probe (sync-probe.c: when OBS delivered each beep and each frame):

    length        the final video is as long as the sections say (sum of length / speed)
    same length   audio and video end together
    rhythm        in each section the picture advances by the section's speed per frame
    sync          in every real-time section each beep sits where OBS itself had it,
                  relative to its frame (REQ-005): what the plugin adds, not what the
                  test media or OBS bring
    silence       timelapse sections are silent (REQ-006)
    chapters      one chapter per section, where the section starts, named after its speed
                  (REQ-007; Hybrid MP4 only)
    pause         across each pause the picture jumps by exactly the paused time: nothing
                  recorded while paused is in the file, nothing around it is lost (REQ-019)
    lost frames   frames lost because the computer could not keep up
    feed          the plugin reports how many frames, at most, waited in its feed for the
                  encoder (docs/performance.md, section 9)

Prints one PASS or FAIL line per check; the exit status is the number of failures.
Needs ffmpeg and ffprobe. Standard library only.
"""

import argparse
import array
import json
import re
import statistics
import subprocess
import sys

FPS = 30
RATE = 48000
BEEP_PERIOD = 30  # frames between beeps in the test media
MEDIA_FRAMES = 300 * FPS  # the test media loops after this many frames
LEVEL_BEEP = 3000  # 16-bit sample level that counts as "beep"
LEVEL_SILENT = 200  # anything quieter counts as silence
# The file starts a few frames short of the plugin's own count: OBS writes no video until the first audio
# arrives, so both start together. Measured: 0.02 to 0.09 s. A wrong speed in a section is off by seconds,
# so this margin still catches real mistakes.
LENGTH_MARGIN_S = 0.15
# REQ-005 allows the audio half a block (10.7 ms) and the video half a frame (16.7 ms) from their places,
# so up to 27.4 ms between them; the rest is margin for the measurement.
SYNC_LIMIT_S = 0.030


def run(*command: str) -> bytes:
    return subprocess.run(command, check=True, capture_output=True).stdout


def frame_numbers(path: str) -> list[int]:
    """The number each frame of the recording shows, unwrapped across loops of the media."""
    raw = run("ffmpeg", "-v", "error", "-i", path, "-vf", "scale=16:1:flags=area,format=gray",
              "-fps_mode", "passthrough", "-f", "rawvideo", "-")
    numbers, loops, previous = [], 0, None
    for i in range(0, len(raw) - 15, 16):
        row = raw[i:i + 16]
        n = sum(1 << bit for bit in range(16) if row[bit] > 125)
        if previous is not None and n + MEDIA_FRAMES // 2 < previous % MEDIA_FRAMES:
            loops += 1  # the media looped back to frame 0
        previous = n + loops * MEDIA_FRAMES
        numbers.append(previous)
    return numbers


def audio_samples(path: str) -> array.array:
    samples = array.array("h")
    samples.frombytes(run("ffmpeg", "-v", "error", "-i", path, "-vn", "-ac", "1", "-ar", str(RATE),
                          "-f", "s16le", "-"))
    return samples


def beep_onsets(samples: array.array) -> list[float]:
    """Times (s) where a beep starts: loud after at least 0.1 s of quiet."""
    onsets, quiet = [], RATE  # start as if after a long quiet, so a beep at 0 counts
    for i, value in enumerate(samples):
        if abs(value) > LEVEL_BEEP:
            if quiet > RATE // 10:
                onsets.append(i / RATE)
            quiet = 0
        else:
            quiet += 1
    return onsets


def plugin_events(log_path: str) -> tuple[list[tuple[float, int]], float, float]:
    """From the plugin's log: [(section start s, speed)], stop time s, share of lost frames."""
    sections, stop_s, lost = [(0.0, 1)], None, 0.0
    with open(log_path, encoding="utf-8", errors="replace") as log:
        for line in log:
            if m := re.search(r"\[timelapse-switch\] recording started .*speed x(\d+)", line):
                sections = [(0.0, int(m.group(1)))]
            elif m := re.search(r"\[timelapse-switch\] (?:switch|resume): speed x(\d+) at ([\d.]+) s", line):
                sections.append((float(m.group(2)), int(m.group(1))))
            elif m := re.search(r"\[timelapse-switch\] pause at ([\d.]+) s", line):
                sections.append((float(m.group(1)), 0))  # speed 0: paused
            elif m := re.search(r"\[timelapse-switch\] recording stopped at ([\d.]+) s.*lost ([\d.]+)%", line):
                stop_s, lost = float(m.group(1)), float(m.group(2)) / 100
    if stop_s is None:
        raise RuntimeError("the log has no 'recording stopped' line from the plugin")
    return sections, stop_s, lost


def feed_waiting(log_path: str) -> tuple[int, int] | None:
    """From the plugin's summary line: (most frames that waited in its feed for the encoder, feed size)."""
    with open(log_path, encoding="utf-8", errors="replace") as log:
        for line in log:
            if m := re.search(r"\[timelapse-switch\] recording stopped .*feed: at most (\d+) of (\d+) frames waiting", line):
                return int(m.group(1)), int(m.group(2))
    return None


def probe_offsets(log_path: str) -> list[tuple[int, float]]:
    """OBS's own sync, from the sync probe: [(frame number, its beep onset minus the frame's time, s)]."""
    frames, beeps = [], []
    with open(log_path, encoding="utf-8", errors="replace") as log:
        for line in log:
            if m := re.search(r"\[tsw-sync-probe\] frame (\d+) at (\d+)", line):
                frames.append((int(m.group(1)), int(m.group(2))))
            elif m := re.search(r"\[tsw-sync-probe\] beep at (\d+)", line):
                beeps.append(int(m.group(1)))
    offsets = []
    for number, at_ns in frames:
        nearest = min(beeps, key=lambda beep_ns: abs(beep_ns - at_ns), default=None)
        if nearest is not None and abs(nearest - at_ns) < 300_000_000:  # beeps are 1 s apart
            offsets.append((number, (nearest - at_ns) / 1e9))
    return offsets


def output_sections(sections: list[tuple[float, int]], stop_s: float) -> list[dict]:
    """Each section with where it starts and ends in the recording and in the final video. A paused
    section (speed 0) takes no time in the final video."""
    result, out = [], 0.0
    for i, (start, speed) in enumerate(sections):
        end = sections[i + 1][0] if i + 1 < len(sections) else stop_s
        length = (end - start) / speed if speed else 0.0
        result.append({"speed": speed, "start": start, "end": end, "out_start": out, "out_end": out + length})
        out = result[-1]["out_end"]
    return result


class Report:
    def __init__(self):
        self.failures = 0

    def check(self, name: str, ok: bool, details: str) -> None:
        print(f"{'PASS' if ok else 'FAIL'}  {name}: {details}")
        self.failures += not ok


def check_rhythm(report: Report, sections: list[dict], numbers: list[int]) -> None:
    worst = []
    for s in sections:
        first, last = int((s["out_start"] + 0.1) * FPS), int((s["out_end"] - 0.1) * FPS)
        steps = [numbers[i + 1] - numbers[i] for i in range(first, min(last, len(numbers) - 1))]
        if len(steps) >= 5:
            median = statistics.median(steps)
            if median != s["speed"]:
                worst.append(f"x{s['speed']} at {s['out_start']:.1f} s advanced {median} per frame")
    report.check("rhythm", not worst, "; ".join(worst) or f"all {len(sections)} sections advance by their speed")


def check_sync(report: Report, sections: list[dict], numbers: list[int], onsets: list[float],
               obs_offsets: list[tuple[int, float]]) -> None:
    """Each beep of the real-time sections against OBS's own offset for the same beep."""
    if not obs_offsets:
        report.check("sync", False, "no sync probe lines in the OBS log (see tests/integration/README.md)")
        return
    added, per_section, cursor, unmatched = [], [], 0, 0
    for s in (s for s in sections if s["speed"] == 1):
        section = []
        for onset in (t for t in onsets if s["out_start"] + 0.2 < t < s["out_end"] - 0.2):
            i = min(int(onset * FPS), len(numbers) - 1)
            beep_frame = round(numbers[i] / BEEP_PERIOD) * BEEP_PERIOD
            expected = i / FPS + (beep_frame - numbers[i]) / FPS  # when that frame is on screen
            # The same beep in the probe's list: the next one with that frame number (the media loops).
            match = next((k for k in range(cursor, len(obs_offsets))
                          if obs_offsets[k][0] == beep_frame % MEDIA_FRAMES), None)
            if match is None:
                unmatched += 1
                continue
            cursor = match
            section.append(onset - expected - obs_offsets[match][1])
        if section:
            added += section
            per_section.append((s["out_start"], statistics.median(section)))
    if not added:
        report.check("sync", False, "no beep of the recording was found in the probe's list")
        return
    worst = max(added, key=abs)
    obs_own = statistics.median(offset for _, offset in obs_offsets)
    details = ", ".join(f"{m * 1000:+.1f} ms at {t:.0f} s" for t, m in per_section)
    report.check("sync", abs(worst) <= SYNC_LIMIT_S and unmatched == 0,
                 f"worst {worst * 1000:+.1f} ms from OBS's own sync (limit {SYNC_LIMIT_S * 1000:.0f}), "
                 f"{len(added)} beeps, {unmatched} not in the probe's list; OBS's own: {obs_own * 1000:+.1f} ms; "
                 f"per section {details}")


def chaptered(sections: list[dict]) -> list[dict]:
    """The sections that start a chapter: each change of speed. A pause adds none, and resuming at the
    speed it had before the pause adds none either."""
    result = []
    for s in sections:
        if s["speed"] and (not result or result[-1]["speed"] != s["speed"]):
            result.append(s)
    return result


def check_chapters(report: Report, sections: list[dict], path: str) -> None:
    """Hybrid MP4 gets a chapter per change of speed, placed within 0.1 s (3 frames) of where it starts."""
    probe = json.loads(run("ffprobe", "-v", "error", "-show_chapters", "-of", "json", path))
    chapters = [(float(c["start_time"]), c.get("tags", {}).get("title", "")) for c in probe.get("chapters", [])]
    sections = chaptered(sections)
    problems = []
    if len(chapters) != len(sections):
        problems.append(f"{len(chapters)} chapters for {len(sections)} changes of speed")
    for s, (start, title) in zip(sections, chapters):
        name = "Real time" if s["speed"] == 1 else f"Timelapse \u00d7{s['speed']}"
        if abs(start - s["out_start"]) > 0.1 or title != name:
            problems.append(f"\"{title}\" at {start:.3f} s, expected \"{name}\" at {s['out_start']:.3f} s")
    report.check("chapters", not problems,
                 "; ".join(problems[:3]) or f"{len(chapters)} chapters, one per change of speed")


def check_pauses(report: Report, sections: list[dict], numbers: list[int]) -> None:
    """Across each pause the media's frame number jumps by the paused time: what OBS showed while paused
    is not in the file, and the frames on both sides are. Scenarios without pauses skip this check."""
    pauses = [k for k, s in enumerate(sections) if s["speed"] == 0]
    if not pauses:
        return
    problems = []
    for k in pauses:
        p = sections[k]
        if k == len(sections) - 1:
            # Paused until Stop: nothing may follow it in the file (0.15 s allowed, as for the length).
            extra_s = len(numbers) / FPS - p["out_start"]
            if extra_s > LENGTH_MARGIN_S:
                problems.append(f"{extra_s:.2f} s recorded after the pause that lasted until Stop")
            continue
        # The file starts a few hundredths of a second off the log's clock, so the jump is looked for
        # within 0.2 s (6 frames) of where the log places the pause: the largest step there.
        i = round(p["out_start"] * FPS)
        window = range(max(1, i - 6), min(len(numbers), i + 7))
        if not window:
            problems.append(f"pause at {p['start']:.1f} s is not inside the video")
            continue
        jump = max(numbers[j] - numbers[j - 1] for j in window)
        expected = (p["end"] - p["start"]) * FPS
        # Margin: each frame next to the pause may sit up to one step of its section off (1 media frame in
        # real time, N in timelapse xN), plus 3 frames (0.1 s) for the measuring itself.
        before = sections[k - 1]["speed"] if k > 0 else 1
        after = sections[k + 1]["speed"] if k + 1 < len(sections) else 1
        if abs(jump - expected) > before + after + 3:
            problems.append(f"pause of {p['end'] - p['start']:.1f} s at {p['start']:.1f} s: "
                            f"the picture jumps {jump} frames, expected about {expected:.0f}")
    report.check("pause", not problems,
                 "; ".join(problems) or f"{len(pauses)} pauses, each left out of the file exactly")


def check_silence(report: Report, sections: list[dict], samples: array.array) -> None:
    timelapse = [s for s in sections if s["speed"] > 1 and s["out_end"] - s["out_start"] > 0.2]
    loud = []
    for s in timelapse:
        part = samples[int((s["out_start"] + 0.05) * RATE):int((s["out_end"] - 0.05) * RATE)]
        if part and max(abs(v) for v in part) >= LEVEL_SILENT:
            loud.append(f"x{s['speed']} at {s['out_start']:.1f} s")
    details = "sound in " + ", ".join(loud) if loud else f"all {len(timelapse)} timelapse sections are silent"
    report.check("silence", not loud, details)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--recording", required=True)
    parser.add_argument("--log", required=True)
    parser.add_argument("--allow-lost-frames", type=float, default=0.0, help="share, 0.01 = 1%%")
    args = parser.parse_args()

    sections, stop_s, lost = plugin_events(args.log)
    sections = output_sections(sections, stop_s)
    numbers = frame_numbers(args.recording)
    samples = audio_samples(args.recording)
    video_s, audio_s, expected_s = len(numbers) / FPS, len(samples) / RATE, sections[-1]["out_end"]

    report = Report()
    report.check("length", abs(video_s - expected_s) <= LENGTH_MARGIN_S,
                 f"video {video_s:.3f} s, expected {expected_s:.3f} s from {len(sections)} sections")
    report.check("same length", abs(audio_s - video_s) <= 0.05, f"audio {audio_s:.3f} s, video {video_s:.3f} s")
    check_rhythm(report, sections, numbers)
    check_sync(report, sections, numbers, beep_onsets(samples), probe_offsets(args.log))
    check_silence(report, sections, samples)
    check_chapters(report, sections, args.recording)
    check_pauses(report, sections, numbers)
    report.check("lost frames", lost <= args.allow_lost_frames, f"{lost * 100:.2f}% lost")
    feed = feed_waiting(args.log)
    report.check("feed", feed is not None and 1 <= feed[0] <= feed[1],
                 f"at most {feed[0]} of {feed[1]} frames waited for the encoder" if feed
                 else "the plugin's summary line does not say how full its feed got")
    return report.failures


if __name__ == "__main__":
    sys.exit(main())
