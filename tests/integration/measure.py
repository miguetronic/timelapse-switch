#!/usr/bin/env python3
# Timelapse Switch
# SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
# SPDX-License-Identifier: GPL-2.0-or-later
"""measure.py: what a recording costs the computer, measured on a running sandbox OBS.

    measure.py run --port 4466 --pid <obs pid> --log <obs log> --mode obs|plugin|plugin-c
                   --idle 60 --seconds 300 --after 30 --out <folder>
    measure.py report <folder>

run: one sample per second, first with OBS idle, then while recording, either with OBS's own
Record button (mode obs) or with the plugin in real time (mode plugin), and last once the file
is saved, to see what is given back. Each sample holds:
  - OBS's CPU time and resident memory (/proc/<pid>);
  - the whole computer's CPU use (/proc/stat);
  - the graphics card as a whole: clocks, power and PCIe packets (amdgpu files in /sys,
    when the card has them; its overall busy share is kept in the samples but left out of
    the report, because on the reference card it jumps between 0 and 80 % for the same work);
  - the graphics card's work for OBS alone, engine by engine (3D, copy, video encoder and
    decoder), and the card memory OBS holds (/proc/<pid>/fdinfo, amdgpu);
  - OBS's own frame counters (obs-websocket GetStats).
It writes <folder>/<mode>.json.

report: reads obs.json, plugin.json and the OBS logs copied next to them (obs.log,
plugin.log), and writes <folder>/report.md: the two recordings side by side, each against
its own idle minute, so whatever else the computer was doing cancels out.
"""

import argparse
import glob
import json
import os
import re
import statistics
import sys
import threading
import time

from drive_obs import HOTKEY, Obs, connect, wait_for_log

SETTLE_S = 10  # the first seconds of each phase are left out: OBS and the encoder warm up
CLOCK_TICKS = os.sysconf("SC_CLK_TCK")


# ---------------------------------------------------------------------------------------
# Reading the computer.


def read_text(path: str) -> str | None:
    try:
        with open(path, encoding="ascii") as file:
            return file.read()
    except OSError:
        return None


def process_cpu_s(pid: int) -> float:
    """CPU time used so far by every thread of the process, user plus system, in seconds."""
    fields = read_text(f"/proc/{pid}/stat").rsplit(")", 1)[1].split()
    return (int(fields[11]) + int(fields[12])) / CLOCK_TICKS  # utime and stime


def process_memory_mib(pid: int) -> dict:
    """Resident memory now (VmRSS) and its highest value so far (VmHWM)."""
    values = {}
    for line in read_text(f"/proc/{pid}/status").splitlines():
        key, _, rest = line.partition(":")
        if key in ("VmRSS", "VmHWM"):
            values[key] = int(rest.split()[0]) / 1024
    return values


def system_cpu_ticks() -> tuple[int, int]:
    """(busy, total) clock ticks of all CPUs since boot; waiting for the disk counts as idle."""
    numbers = [int(n) for n in read_text("/proc/stat").splitlines()[0].split()[1:]]
    idle = numbers[3] + numbers[4]
    return sum(numbers) - idle, sum(numbers)


def find_gpu() -> str | None:
    """The sysfs folder of the first amdgpu card, the only kind these files exist for."""
    for folder in sorted(glob.glob("/sys/class/drm/card*/device")):
        if os.path.exists(os.path.join(folder, "gpu_busy_percent")):
            return folder
    return None


def gpu_sample(folder: str) -> dict:
    sample = {}
    for name in ("gpu_busy_percent", "mem_busy_percent", "mem_info_vram_used", "mem_info_gtt_used"):
        text = read_text(os.path.join(folder, name))
        if text is not None:
            sample[name] = int(text)
    # hwmon: clocks in Hz, power in microwatts.
    for name, key, scale in (("freq1_input", "core_mhz", 1e6), ("freq2_input", "memory_mhz", 1e6),
                             ("power1_input", "power_w", 1e6), ("power1_average", "power_w", 1e6)):
        for path in glob.glob(os.path.join(folder, "hwmon", "hwmon*", name)):
            text = read_text(path)
            if text is not None:
                sample[key] = int(text) / scale
    return sample


def obs_gpu_use(pid: int) -> dict:
    """The graphics card's work for this process only, from the kernel's per-client counters:
    busy nanoseconds per engine (drm-engine-gfx, -dma, -enc, ...) and the card memory the
    process holds. A process may open the card several times; each client counts once."""
    clients: dict[str, dict] = {}
    for path in glob.glob(f"/proc/{pid}/fdinfo/*"):
        text = read_text(path)
        if not text or "drm-driver:\tamdgpu" not in text:
            continue
        fields = dict(line.split(":", 1) for line in text.splitlines() if ":" in line)
        clients[fields.get("drm-client-id", path).strip()] = fields
    use: dict[str, float] = {}
    for fields in clients.values():
        for key, value in fields.items():
            if key.startswith("drm-engine-") and value.strip().endswith("ns"):
                use[key[len("drm-engine-"):]] = use.get(key[len("drm-engine-"):], 0) + int(value.split()[0])
            elif key in ("drm-memory-vram", "drm-memory-gtt"):
                use[key] = use.get(key, 0) + int(value.split()[0]) / 1024  # KiB to MiB
    return use


class PcieCounter(threading.Thread):
    """Reads amdgpu's pcie_bw again and again. Each read takes the kernel one second: it counts
    the PCIe packets the card received and sent during that second, and gives the largest size
    a packet may have. Packets times that size is an upper bound of the bytes moved."""

    def __init__(self, folder: str):
        super().__init__(daemon=True)
        self.path = os.path.join(folder, "pcie_bw")
        self.readings: list[tuple[float, int, int, int]] = []
        self.running = True

    def run(self) -> None:
        while self.running:
            text = read_text(self.path)
            if text is None:
                return
            received, sent, payload = (int(n) for n in text.split())
            self.readings.append((time.monotonic(), received, sent, payload))


# ---------------------------------------------------------------------------------------
# One run.


class Sampler:
    def __init__(self, obs: Obs, pid: int, gpu: str | None):
        self.obs, self.pid, self.gpu = obs, pid, gpu
        self.samples: list[dict] = []
        self.last = (time.monotonic(), process_cpu_s(pid), *system_cpu_ticks())
        self.last_engines = obs_gpu_use(pid)

    def sample(self, phase: str, phase_start: float) -> None:
        now, cpu_s = time.monotonic(), process_cpu_s(self.pid)
        busy, total = system_cpu_ticks()
        then, last_cpu_s, last_busy, last_total = self.last
        self.last = (now, cpu_s, busy, total)
        stats = self.obs.request("GetStats")
        sample = {
            "phase": phase,
            "t": now - phase_start,
            "obs_cores": (cpu_s - last_cpu_s) / (now - then),
            "system_cpu_percent": 100.0 * (busy - last_busy) / max(1, total - last_total),
            **process_memory_mib(self.pid),
            "fps": stats["activeFps"],
            "render_ms": stats["averageFrameRenderTime"],
            "render_skipped": stats["renderSkippedFrames"],
            "output_skipped": stats["outputSkippedFrames"],
        }
        if self.gpu:
            sample.update(gpu_sample(self.gpu))
        use = obs_gpu_use(self.pid)
        for key, value in use.items():
            if key.startswith("drm-memory-"):
                sample["obs_" + key[len("drm-memory-"):] + "_mib"] = value
            else:  # busy share of this engine over the last second, for OBS alone
                sample["obs_engine_" + key] = 100.0 * (value - self.last_engines.get(key, 0)) / ((now - then) * 1e9)
        self.last_engines = use
        self.samples.append(sample)

    def phase(self, name: str, seconds: float) -> None:
        start = time.monotonic()
        for second in range(1, int(seconds) + 1):
            time.sleep(max(0.0, start + second - time.monotonic()))
            self.sample(name, start)


def start_recording(obs: Obs, mode: str) -> None:
    if mode == "obs":
        obs.request("StartRecord")
    else:
        obs.press("Record")  # idle and nothing chosen: the plugin starts in real time


def stop_recording(obs: Obs, mode: str, log: str) -> None:
    if mode == "obs":
        obs.request("StopRecord")
        deadline = time.monotonic() + 120
        while obs.request("GetRecordStatus")["outputActive"]:
            if time.monotonic() > deadline:
                raise TimeoutError("OBS did not finish its recording in 120 s")
            time.sleep(0.5)
    else:
        obs.press("Stop")
        wait_for_log(log, "[timelapse-switch] recording saved", timeout_s=120)


def run(args: argparse.Namespace) -> int:
    obs = connect(args.port, wait_s=60)
    if args.mode.startswith("plugin"):
        names = {hotkey for hotkey in obs.request("GetHotkeyList")["hotkeys"] if hotkey.startswith(HOTKEY)}
        if HOTKEY + "Record" not in names:
            print("FAIL  the plugin's Record hotkey is missing")
            return 1
    gpu = find_gpu()
    pcie = PcieCounter(gpu) if gpu else None
    if pcie:
        pcie.start()
    sampler = Sampler(obs, args.pid, gpu)

    print(f"{args.mode}: {args.idle} s idle, then {args.seconds} s recording")
    sampler.phase("idle", args.idle)
    recording_start = time.monotonic()
    start_recording(obs, args.mode)
    sampler.phase("recording", args.seconds)
    stop_recording(obs, args.mode, args.log)
    recording_end = time.monotonic()
    sampler.phase("after", args.after)
    if pcie:
        pcie.running = False
        pcie.join(timeout=3)

    # PCIe readings, placed in the phase they fell in (by the end of their second).
    idle_end = recording_start
    readings = []
    for at, received, sent, payload in pcie.readings if pcie else []:
        phase = "idle" if at <= idle_end else ("recording" if at <= recording_end else "after")
        readings.append({"phase": phase, "received": received, "sent": sent, "payload": payload})
    result = {"mode": args.mode, "gpu": gpu, "samples": sampler.samples, "pcie": readings}
    with open(os.path.join(args.out, f"{args.mode}.json"), "w", encoding="utf-8") as file:
        json.dump(result, file, indent=1)
    return 0


# ---------------------------------------------------------------------------------------
# The report.

# OBS writes a no-break space between the number and "ms"; \s matches it.
# A name starts at its first letter (after the time and the tree drawn in front of it), and may hold
# spaces, no-break ones too.
PROFILER_LINE = re.compile(
    r"([A-Za-z_][\w()>./\s:-]*?): min=([\d.]+)\sms, median=([\d.]+)\sms, max=([\d.]+)\sms, "
    r"99th percentile=([\d.]+)\sms"
)
# The whole graphics thread per frame, what OBS does before the preview, and road C's own step (ADR-012).
PROFILER_NAMES = ("obs_graphics_thread", "output_frame", "render_video", "render_main_texture",
                  "timelapse-switch: road C", "download_frame", "output_video_data", "render_displays")
MODES = {"obs": "OBS", "plugin": "plugin, road B", "plugin-c": "plugin, road C"}
LOG_EVIDENCE = re.compile(
    r"Output '.*': (Total|Number)|skipped frames due to encoding lag|\[timelapse-switch\] (encoders|recording stopped|frames \||road C:)"
)


def settled(samples: list[dict], phase: str) -> list[dict]:
    return [s for s in samples if s["phase"] == phase and s["t"] > SETTLE_S]


def mean(samples: list[dict], key: str) -> float:
    if key.startswith("obs_engine_"):  # an engine OBS has not used yet has no counter: 0 % busy
        return statistics.fmean(s.get(key, 0.0) for s in samples) if samples else float("nan")
    values = [s[key] for s in samples if key in s]
    return statistics.fmean(values) if values else float("nan")


def p95(samples: list[dict], key: str) -> float:
    values = sorted(s[key] for s in samples if key in s)
    return values[int(0.95 * (len(values) - 1))] if values else float("nan")


def pcie_thousands(readings: list[dict], phase: str, key: str) -> float:
    """Thousands of PCIe packets per second. Not bytes: packets are often smaller than the
    largest size the counter reports, so packets times that size overstates the traffic."""
    values = [r[key] / 1000 for r in readings if r["phase"] == phase]
    return statistics.fmean(values[1:]) if len(values) > 1 else float("nan")  # the first may straddle


def profiler(log_text: str) -> dict:
    """The per-call timings OBS prints when it closes (the first, "Profiler Results", block)."""
    block = log_text.split("== Profiler Results", 1)[-1].split("== Profiler Time Between Calls", 1)[0]
    found = {}
    for line in block.splitlines():
        match = PROFILER_LINE.search(line)
        name = re.sub(r"\(\d[^)]*\)$", "", match.group(1).strip()) if match else ""  # "(33.3333 ms)" goes
        if name in PROFILER_NAMES and name not in found:
            found[name] = [float(match.group(n)) for n in (2, 3, 4, 5)]
    return found


def summary(run_data: dict, log_text: str) -> dict:
    idle, rec = settled(run_data["samples"], "idle"), settled(run_data["samples"], "recording")
    after = settled(run_data["samples"], "after")
    rows = {
        "OBS CPU, cores busy (mean)": (mean(idle, "obs_cores"), mean(rec, "obs_cores")),
        "OBS CPU, cores busy (95th percentile)": (p95(idle, "obs_cores"), p95(rec, "obs_cores")),
        "whole computer CPU, % (mean)": (mean(idle, "system_cpu_percent"), mean(rec, "system_cpu_percent")),
        "OBS memory in RAM, MiB (mean)": (mean(idle, "VmRSS"), mean(rec, "VmRSS")),
        "OBS card memory, video memory, MiB (mean)": (mean(idle, "obs_vram_mib"), mean(rec, "obs_vram_mib")),
        "OBS card memory, in RAM (GTT), MiB (mean)": (mean(idle, "obs_gtt_mib"), mean(rec, "obs_gtt_mib")),
    }
    engines = sorted({key for s in idle + rec for key in s if key.startswith("obs_engine_")})
    for key in engines:
        rows[f"OBS work on the card, engine {key[len('obs_engine_'):]}, % busy (mean)"] = (mean(idle, key), mean(rec, key))
    rows.update({
        "whole card, core clock, MHz (mean)": (mean(idle, "core_mhz"), mean(rec, "core_mhz")),
        "whole card, power, W (mean)": (mean(idle, "power_w"), mean(rec, "power_w")),
        "whole card, PCIe packets sent, thousands/s": (
            pcie_thousands(run_data["pcie"], "idle", "sent"),
            pcie_thousands(run_data["pcie"], "recording", "sent"),
        ),
        "whole card, PCIe packets received, thousands/s": (
            pcie_thousands(run_data["pcie"], "idle", "received"),
            pcie_thousands(run_data["pcie"], "recording", "received"),
        ),
        "OBS frame render time, ms (mean)": (mean(idle, "render_ms"), mean(rec, "render_ms")),
    })
    last = run_data["samples"][-1]
    counters = {
        "frames OBS skipped while rendering": last["render_skipped"],
        "frames skipped in OBS's memory feed": last["output_skipped"],
        "highest OBS memory (VmHWM), MiB": last.get("VmHWM", float("nan")),
        "OBS memory in RAM once the file is saved, MiB (mean)": mean(after, "VmRSS"),
        "seconds measured (idle, recording, after)": f"{len(idle)}, {len(rec)}, {len(after)}",
    }
    evidence = [line.split(": ", 1)[-1] for line in log_text.splitlines() if LOG_EVIDENCE.search(line)]
    return {"rows": rows, "counters": counters, "profiler": profiler(log_text), "evidence": evidence}


def number(value) -> str:
    if isinstance(value, float):
        return "n/a" if value != value else f"{value:.2f}"  # NaN: the computer has no such counter
    return str(value)


def report(args: argparse.Namespace) -> int:
    runs = {}
    for mode in MODES:  # the modes measured in this folder; OBS's own is the reference
        if not os.path.exists(os.path.join(args.folder, f"{mode}.json")):
            continue
        with open(os.path.join(args.folder, f"{mode}.json"), encoding="utf-8") as file:
            data = json.load(file)
        with open(os.path.join(args.folder, f"{mode}.log"), encoding="utf-8", errors="replace") as file:
            runs[mode] = summary(data, file.read())
    if "obs" not in runs:
        print("FAIL  OBS's own run is missing: it is the reference")
        return 1
    plugins = [mode for mode in runs if mode != "obs"]

    out = ["# Recording cost: OBS's own recording against the plugin in real time", ""]
    out += ["Each run: a fresh sandbox OBS, the same scene, " f"{SETTLE_S} s of warm-up left out of each phase.", ""]
    header = ["Measure"] + [f"{MODES[mode]} {phase}" for mode in runs for phase in ("idle", "recording")]
    header += [f"difference ({MODES[mode]} minus OBS)" for mode in plugins]
    out += ["| " + " | ".join(header) + " |", "|" + "---|" * len(header)]
    names = list(dict.fromkeys(name for mode in runs for name in runs[mode]["rows"]))
    for name in names:
        values = {mode: runs[mode]["rows"].get(name, (0.0, 0.0)) for mode in runs}  # an engine never used: 0
        added = {mode: values[mode][1] - values[mode][0] for mode in runs}
        cells = [number(v) for mode in runs for v in values[mode]]
        cells += [number(added[mode] - added["obs"]) for mode in plugins]
        out.append(f"| {name} | " + " | ".join(cells) + " |")
    out += ["", "The difference compares what each recording added to its own idle minute.", ""]
    out += ["| Counter | " + " | ".join(MODES[mode] for mode in runs) + " |", "|---|" + "---|" * len(runs)]
    for name in runs["obs"]["counters"]:
        out.append(f"| {name} | " + " | ".join(number(runs[mode]["counters"][name]) for mode in runs) + " |")
    out += ["", "## OBS's own timings, per frame (from its log when it closes)", ""]
    out += ["| Step | " + " | ".join(f"{MODES[mode]}: min / median / max / 99th, ms" for mode in runs) + " |",
            "|---|" + "---|" * len(runs)]
    for name in PROFILER_NAMES:
        cells = [" / ".join(number(v) for v in runs[mode]["profiler"].get(name, [])) or "not run" for mode in runs]
        out.append(f"| {name} | " + " | ".join(cells) + " |")
    for mode in runs:
        out += ["", f"## Log lines, {mode} run", "", "```", *runs[mode]["evidence"], "```"]
    with open(os.path.join(args.folder, "report.md"), "w", encoding="utf-8") as file:
        file.write("\n".join(out) + "\n")
    print("\n".join(out))
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    commands = parser.add_subparsers(dest="command", required=True)
    one = commands.add_parser("run", help="measure one sandbox OBS")
    one.add_argument("--port", type=int, required=True)
    one.add_argument("--pid", type=int, required=True, help="process id of the sandbox OBS")
    one.add_argument("--log", required=True, help="OBS log file of this run")
    one.add_argument("--mode", choices=tuple(MODES), required=True)
    one.add_argument("--idle", type=int, default=60, help="seconds idle before recording")
    one.add_argument("--seconds", type=int, default=300, help="seconds of recording")
    one.add_argument("--after", type=int, default=30, help="seconds measured once the file is saved")
    one.add_argument("--out", required=True, help="folder for <mode>.json")
    both = commands.add_parser("report", help="compare the two runs in a folder")
    both.add_argument("folder")
    args = parser.parse_args()
    return run(args) if args.command == "run" else report(args)


if __name__ == "__main__":
    sys.exit(main())
