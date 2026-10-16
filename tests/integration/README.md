# Integration test

A real OBS records with the plugin while a script presses its hotkeys, and then the file is checked frame by frame and sample by sample: length, rhythm, sync, silence, lost frames and memory leaks.

## How it works

```
  make-test-media.sh        run.sh                                    analyze.py
  +----------------+   +-------------------------------------+   +--------------------------+
  | 5 min of video |   | a separate OBS (its own settings,   |   | reads the recording and  |
  | that shows its |-->| port 4466) plays the media;         |-->| compares it with what    |
  | frame number,  |   | drive_obs.py presses the hotkeys;   |   | the plugin and the probe |
  | a beep every s |   | sync-probe logs what OBS delivered  |   | wrote to the OBS log     |
  +----------------+   +-------------------------------------+   +--------------------------+
```

**The test media** (`make-test-media.sh`, made once into `build_x86_64/integration/`): every frame shows its own number as 16 black or white bars (bit 0 on the left), and a 1 kHz beep of 50 ms starts on every frame whose number is a multiple of 30. Lossless (FFV1 and PCM), so what the analysis reads is exactly what was recorded.

**The sandbox:** OBS runs with `XDG_CONFIG_HOME` pointing to a new temporary folder, so your own OBS settings, scenes and plugins are never read or changed. Its obs-websocket listens on port 4466 instead of the usual 4455.

**The sync probe** (`sync-probe.c`): a tiny OBS module installed only in the sandbox, never shipped. It logs when OBS delivers each beep and each frame whose number is a multiple of 30, exactly as the plugin receives them. To do that it asks OBS for every frame in memory, so while it is installed OBS always takes the memory road (`docs/performance.md`). That is why `measure.sh` leaves it out.

## Running it

Build first (`tools/check.sh`, or `cmake --build build_x86_64`), with the `.venv` of `tools/requirements-dev.txt`. Then:

```
tests/integration/run.sh                                     1080p, about 100 s, HEVC on VAAPI
tests/integration/run.sh --encoder obs_x264                  the same with x264
tests/integration/run.sh --scenario real                     25 s in real time, no switch
tests/integration/run.sh --resolution 2160 --scenario long   4K, about 300 s: a load test
```

`--keep` keeps the sandbox folder (it is always kept when a check fails). It holds the OBS log (`config/obs-studio/logs/`) and the recording (`recordings/`).

> The 4K long scenario is a **load test**: CPU and GPU work hard for several minutes. Run it only when the computer can take it, and on the reference PC only with the maintainer's approval (`docs/quality.md`, section 5).

**Needs:** Linux, OBS 31.1 or later (obs-websocket comes with it), `ffmpeg` and `ffprobe`. The default encoder is VAAPI HEVC; on other hardware use `--encoder obs_x264`.

## The scenarios

| Scenario | Length | What it does |
|---|---|---|
| `standard` | about 100 s | x4, x8, 20 quick x2 switches one second apart, x16, real time in between; ends with 4 s at x4 |
| `long` | about 300 s | the standard pattern three times; ends with 4 s at x4 |
| `real` | 25 s | real time only |

Ending in timelapse matters: when you stop, the encoder still holds frames (x264 about 40), and the plugin must keep it moving until every one of them is in the file.

## The checks

| Check | Passes when | Why this limit |
|---|---|---|
| length | the video lasts the sum of each section's length divided by its speed, within 0.15 s | OBS writes no video until the first audio arrives; measured: 0.02 to 0.09 s short. A wrong speed is off by seconds |
| same length | audio and video end within 0.05 s of each other | |
| rhythm | in each section the frame number advances by the section's speed per frame | |
| sync | in every real-time section, each beep sits within 30 ms of where OBS itself had it, relative to its frame | REQ-005 allows 27.4 ms (half an audio block plus half a frame); the rest is measuring margin |
| silence | timelapse sections are silent | REQ-006 |
| lost frames | no frame was lost | |
| memory leaks | OBS reports "Number of memory leaks: 0" when it closes | rule B2 |

## Why sync is measured against OBS, not against the clock

OBS itself does not play the test media in perfect sync: its sound reaches OBS 35 to 65 ms ahead of its picture, by a different amount each time OBS starts (measured on OBS 32.2.2). Any recording of that media carries this offset, with or without the plugin. So the probe records what OBS delivered, and the analysis subtracts it, leaving only what the plugin adds. Measured on 2026-10-16: 0.0 ms in the first section, at most 13 ms after switches.

**Compared with OBS's own recording:** a recording made by OBS with x264 has the same timing as the plugin's. A recording made by OBS with a GPU encoder (the `_tex` encoders) shows each frame about one frame earlier, because libobs labels each frame on that path with the time of the frame before it (`output_gpu_encoders` in `libobs/obs-video.c`). See `docs/architecture.md`, "Timing".

## Measuring the cost

`measure.sh` answers a different question: not "is the file right?" but "what does recording cost the computer?". It records the same scene twice, each time in a fresh sandbox OBS:

```
  OBS alone      +------------+-------------------------+------------------+
                 | 1 min idle | 5 min OBS's own Record  | 30 s, file saved |
                 +------------+-------------------------+------------------+

  the plugin     +------------+-------------------------+------------------+
                 | 1 min idle | 5 min plugin, real time | 30 s, file saved |
                 +------------+-------------------------+------------------+

  each recording is compared with its own idle minute, so whatever else the
  computer is doing cancels out; the last 30 s show what is given back
```

```
tests/integration/measure.sh                         4K30, HEVC on VAAPI, about 7 min per mode
tests/integration/measure.sh --resolution 1080       the same in 1080p
tests/integration/measure.sh --seconds 20 --idle 15  a quick try of the tool itself
```

Once per second, `measure.py` writes down:

| What | Where it comes from |
|---|---|
| CPU time of OBS, as cores kept busy | `/proc/<pid>/stat` |
| CPU use of the whole computer | `/proc/stat` |
| RAM held by OBS (resident memory) | `/proc/<pid>/status` |
| The graphics card's work for OBS, engine by engine (3D, compute, copy, video encoder), and the card memory OBS holds | `/proc/<pid>/fdinfo`, kept by the amdgpu driver for each program |
| The card's clock, power and PCIe packets | amdgpu files in `/sys/class/drm/card*/device` |
| OBS's own counters: render time, frames missed | obs-websocket `GetStats` |

When OBS closes it also writes how long each step of its graphics thread took (its profiler), and the report copies the steps that differ between the two roads: `download_frame` and `output_video_data` (memory road), `output_gpu_encoders` (graphics card road).

The report, `build_x86_64/measure/<date and time>/report.md`, puts both modes side by side. Its numbers are summarized in `docs/performance.md`.

Two counters are kept in the samples but read with care:

- **PCIe packets** are counted, not bytes: a packet can be smaller than the largest size the card reports, so packets times that size would overstate the traffic. The count is still a clear sign of which road the frames take.
- **The card's overall busy share** (`gpu_busy_percent`) jumped between 0 and 80 % for the same work on the reference card, so the report uses the per-program engine times instead.

> Like the 4K long scenario, this is a **load test**: on the reference PC, run it only with the maintainer's approval (`docs/quality.md`, section 5).

## Reading a failure

- `OBS did not start`: see `obs-output.txt` in the sandbox folder.
- The OBS log has the plugin's own lines (`[timelapse-switch]`), among them a summary when the recording stops: frames in, sent and lost, audio blocks sent, ring full and ring empty counts, and how late frames and audio arrive.
- `sync` lists the offset per real-time section, so a jump after a given switch shows where to look.

## Files

| File | Job |
|---|---|
| `run.sh` | builds, makes the media once, starts the sandbox OBS, runs the scenario, closes OBS, runs the analysis |
| `sandbox.sh` | shared by `run.sh` and `measure.sh`: writes the sandbox configuration, starts and closes the sandbox OBS |
| `measure.sh`, `measure.py` | the cost measurement: samples once per second, then the side by side report |
| `make-test-media.sh` | the test media |
| `drive_obs.py` | presses the hotkeys on schedule through obs-websocket, waits for the file to be saved |
| `analyze.py` | the checks above |
| `sync-probe.c`, `CMakeLists.txt` | the sync probe (CMake option `ENABLE_INTEGRATION_TESTS`, on by default) |
