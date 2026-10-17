# Test report: what recording costs, OBS's own recording against the plugin

*Date: 2026-10-16. Requirement: REQ-018 (and input for REQ-014). Run with the maintainer's approval.*

## What was run

| Run | Command | Purpose |
|---|---|---|
| Main | `tests/integration/measure.sh --resolution 2160 --seconds 300 --idle 60` | 5 minutes of 4K30 recording with each road |
| Follow-up | `tests/integration/measure.sh --resolution 2160 --seconds 30 --idle 15 --after 40` | the RAM given back once the file is saved (the main run's tool did not yet measure after the recording) |

How the tool works, and what each counter means: `tests/integration/README.md`, "Measuring the cost".

## On what

| | |
|---|---|
| Computer | the reference PC: Intel Xeon E5-2689 (8 cores, 16 threads, 2012), 64 GB DDR3, AMD Radeon RX 580 8 GB on PCIe 3.0 x16 |
| Software | Linux 6.18 (LTS), Mesa 26.2.4, OBS 32.2.2 |
| OBS settings | 3840 x 2160 at 30 fps, NV12, Rec. 709 partial range; HEVC on VAAPI, CQP 22, keyframe every 2 s, no B-frames; Hybrid MP4; AAC 192 kb/s |
| Encoder per road | OBS's own recording: `hevc_ffmpeg_vaapi_tex`; the plugin: `hevc_ffmpeg_vaapi`, same settings |
| Scene | the integration test media (a 640 x 360 video scaled to fill the canvas), in a fresh sandbox OBS for each road |

## Main run, verbatim

Each run: a fresh sandbox OBS, the same scene, 10 s of warm-up left out of each phase.

| Measure | OBS idle | OBS recording | plugin idle | plugin recording | difference (plugin minus OBS) |
|---|---|---|---|---|---|
| OBS CPU, cores busy (mean) | 0.06 | 0.11 | 0.06 | 0.53 | 0.42 |
| OBS CPU, cores busy (95th percentile) | 0.07 | 0.13 | 0.07 | 0.56 | 0.43 |
| whole computer CPU, % (mean) | 3.56 | 3.42 | 3.37 | 6.30 | 3.07 |
| OBS memory in RAM, MiB (mean) | 453.91 | 460.38 | 453.54 | 733.00 | 272.99 |
| OBS card memory, video memory, MiB (mean) | 109.54 | 320.81 | 109.54 | 195.37 | -125.45 |
| OBS card memory, in RAM (GTT), MiB (mean) | 47.75 | 101.30 | 47.75 | 101.30 | 0.00 |
| OBS work on the card, engine compute, % busy (mean) | 0.00 | 0.00 | 0.00 | 4.17 | 4.17 |
| OBS work on the card, engine enc_1, % busy (mean) | 0.00 | 61.13 | 0.00 | 76.42 | 15.29 |
| OBS work on the card, engine gfx, % busy (mean) | 1.89 | 7.62 | 1.97 | 21.08 | 13.39 |
| whole card, core clock, MHz (mean) | 837.35 | 1321.24 | 767.86 | 1340.00 | 88.25 |
| whole card, power, W (mean) | 8.95 | 24.98 | 7.99 | 28.96 | 4.94 |
| whole card, PCIe packets sent, thousands/s | 1.00 | 1.74 | 0.41 | 11422.05 | 11420.91 |
| whole card, PCIe packets received, thousands/s | 4.17 | 3.50 | 1.66 | 2.32 | 1.33 |
| OBS frame render time, ms (mean) | 0.54 | 0.58 | 0.54 | 3.33 | 2.75 |

The difference compares what each recording added to its own idle minute.

| Counter | OBS | plugin |
|---|---|---|
| frames OBS skipped while rendering | 0 | 0 |
| frames skipped in OBS's memory feed | 0 | 0 |
| highest OBS memory (VmHWM), MiB | 461.57 | 734.12 |
| seconds measured (idle, recording) | 51, 291 | 51, 291 |

### OBS's own timings, per frame (from its log when it closes)

| Step | OBS: min / median / max / 99th, ms | plugin: min / median / max / 99th, ms |
|---|---|---|
| output_frame | 0.12 / 0.44 / 5.44 / 0.96 | 0.14 / 3.01 / 8.63 / 5.55 |
| render_video | 0.02 / 0.26 / 4.69 / 0.59 | 0.02 / 0.38 / 4.66 / 0.69 |
| download_frame | not run | 0.00 / 0.10 / 0.33 / 0.24 |
| output_video_data | not run | 2.29 / 2.38 / 7.94 / 4.66 |
| output_gpu_encoders | 0.00 / 0.01 / 0.04 / 0.01 | not run |

### Log lines, obs run

```
Output 'adv_file_output': Total frames output: 9000
Output 'adv_file_output': Total drawn frames: 9003
```

### Log lines, plugin run

```
[timelapse-switch] encoders | video=hevc_ffmpeg_vaapi audio=ffmpeg_aac track=1
[timelapse-switch] recording stopped at 299.980798 s: frames in 8999, sent 8998, lost 0.00% | audio blocks sent 14060, ring full 0, ring empty 0
Output 'timelapse-switch output': Total frames output: 8998
Output 'timelapse-switch output': Total drawn frames: 9002
```

## Follow-up, the rows about RAM

| Counter | OBS | plugin |
|---|---|---|
| highest OBS memory (VmHWM), MiB | 459.49 | 730.22 |
| OBS memory in RAM once the file is saved, MiB (mean) | 459.31 | 530.01 |
| seconds measured (idle, recording, after) | 6, 21, 31 | 6, 21, 31 |
| OBS memory in RAM while idle, MiB (mean) | 453.66 | 453.44 |

## Reading

- **CPU:** recording added 0.05 cores to OBS alone and 0.47 cores with the plugin.
- **RAM:** recording added 6.5 MiB to OBS alone and 279.5 MiB with the plugin. The difference, 273.0 MiB (286.3 MB), is what 23 frames of 4K NV12 take: 6 in OBS's memory feed, 16 in the plugin's feed, 1 in the encoder (286.2 MB).
- **RAM given back:** once the file was saved, OBS with the plugin held 70.9 MiB (74.4 MB) more than OBS after its own recording: OBS's memory feed, 6 frames (74.6 MB), which OBS keeps until it closes. The plugin's 17 frames were given back.
- **Graphics card memory:** recording added 211.3 MiB to OBS alone and 85.8 MiB with the plugin. OBS's own road keeps a queue of 10 frames inside the card (124.4 MB); the plugin keeps its queue in RAM instead.
- **Graphics card work:** the 3D engine went from 7.6 % to 21.1 % busy for OBS, the video encoder engine from 61.1 % to 76.4 %, and the compute engine from 0 to 4.2 %; the card drew 5 W more. The encoder engine's extra share is not explained yet.
- **Bus:** only the plugin's road made the card send PCIe packets in large numbers (11.4 million per second): the frames going down to RAM.
- **OBS's graphics thread:** its work per frame went from a median of 0.44 ms to 3.01 ms (99th percentile: 0.96 ms to 5.55 ms), out of the 33.3 ms each frame has at 30 fps. Most of it is step (2), the copy into OBS's memory feed: 2.38 ms median.
- **Lost frames:** none on either road.

## Later the same day: how full the plugin's feed gets

The plugin's summary line now says how many frames, at most, waited in its feed for the encoder (`docs/performance.md`, section 10). Integration test, HEVC on VAAPI, same reference PC, with a virtual machine and software builds running at the same time:

| Run | Result |
|---|---|
| `tests/integration/run.sh --scenario real` (1080p30, 25 s) | all checks pass; feed: at most 1 of 16 frames waiting |
| `tests/integration/run.sh` (1080p30, about 100 s, 28 sections) | all checks pass; feed: at most 1 of 16 frames waiting |
| `tests/integration/run.sh --resolution 2160 --scenario long` (4K30, 300 s, 80 sections) | all checks pass, 0 lost frames, worst sync 16.0 ms; feed: at most 1 of 16 frames waiting |
