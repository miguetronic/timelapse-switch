# Architecture

How Timelapse Switch works inside OBS, in plain words first and in detail after.

> **Status:** this is the agreed design (see `docs/decisions.md`). The code is still the template skeleton; each part below is marked **planned** until it exists.

## The idea in one paragraph

OBS renders your scene 30 times per second (or whatever frame rate you set) and hands each picture, called a frame, to an encoder, which compresses it into the video file. OBS encoders don't look at the clock: they **count frames**. So if, in timelapse x4, only 1 of every 4 frames reaches the encoder, the file simply plays that part 4 times faster. Timelapse Switch is the gatekeeper that decides which frames, and how much audio, get through.

**Analogy:** a tape recorder with a fast-forward pedal. OBS's own Record button is the main recorder. Timelapse Switch is a second recorder that sees the same picture; while you press its pedal, it records only 1 of every N frames.

## The big picture

```
                       OBS (unchanged)                               Timelapse Switch
  +----------------------------------------------------+
  |  scenes, sources, filters                          |
  |            |                                       |
  |            v                                       |
  |  program picture (the main mix)  ---- every frame ------> [ video valve ]  passes 1 of N in timelapse
  |  program sound  (the audio mix)  ---- every chunk ------> [ audio valve ]  passes the matching share
  |                                                    |             |
  |  OBS encoders (x264, NVENC, VAAPI, ...)  <---------------- own video and audio "feeds"
  |  OBS file writer (Hybrid MP4, MKV, ...)            |             |
  |            |                                       |      [ recorder ]   start, stop, chapters
  |            v                                       |             |
  |  one file, mixed speeds, ready to upload           |      [ dock (Qt) ]  buttons, counters, errors
  +----------------------------------------------------+
```

Only the two valves are new logic. The encoding and the file writing are OBS's own, which keeps the plugin small and compatible with every encoder and format OBS supports.

## Modules (planned)

| Module | File(s) | Language | Job |
|---|---|---|---|
| Plugin entry | `src/plugin-main.c` | C | Load and unload; register every OBS callback (the only place where that happens). |
| Logging | `src/plugin-support.*` | C | `tsw_log`: OBS log lines with the plugin prefix. **Exists.** |
| Compiler differences | `src/tsw-compiler.h` | C | The only conditional compilation. **Exists.** |
| Timeline | `src/core/timeline.c` | C | Pure math, no OBS: given the mode history, how many frames and audio samples pass. Fully unit tested. |
| Video valve | `src/core/video-valve-rt.c` | C | Real-time: decides frame by frame, copies the frames that pass into our own video feed. |
| Audio valve | `src/core/audio-valve-rt.c` | C | Real-time: passes the share of audio that matches the frames (silence in timelapse by default). |
| Recorder | `src/core/recorder.c` | C | Creates the encoders, feeds and file writer from the user's OBS settings; starts, stops, adds chapters. |
| Fault log | `src/core/fault.c` | C | `TSW_CHECK` failures and error codes, safe to fill from real-time threads, emptied by the UI thread. |
| Platform | `src/platform/{linux,windows,macos}.c` | C | Small OS differences, picked by CMake (no `#ifdef`). |
| Dock | `src/ui/dock.cpp` | C++ / Qt | The panel described in `docs/design/ux-spec.md`; reads counters and faults with a timer. |

## Threads

See `docs/coding-rules.md`, B1, for the rules. In short: the two valves run on OBS's real-time threads and must never wait, allocate or log; everything else runs on the UI thread.

## Callback table (rule P10-9)

Function pointers are allowed only where OBS or Qt require a callback. Every one of them is listed here, so the flow of control can be followed without guessing. A pull request that adds or removes a callback updates this table.

| Callback | Registered in | Called by | Thread | Status |
|---|---|---|---|---|
| `obs_module_load` / `obs_module_unload` | `plugin-main.c` (`OBS_DECLARE_MODULE`) | OBS at startup and exit | UI | exists |
| Raw video callback (frame selection) | `plugin-main.c` via `obs_add_raw_video_callback` | OBS video thread, once per frame while recording | video | planned |
| Raw audio callback (audio capture) | `plugin-main.c` via `obs_add_raw_audio_callback` | OBS audio thread, once per audio chunk | audio | planned |
| Audio feed input | `recorder.c` via `audio_output_open` | our audio feed's thread | audio | planned |
| Output signals (`start`, `stop`) | `recorder.c` via the output's signal handler | OBS output thread | output | planned |
| Frontend events (exit, profile change, OBS recording) | `plugin-main.c` via `obs_frontend_add_event_callback` | OBS UI | UI | planned |
| Hotkeys (record, stop, switch, speed) | `plugin-main.c` via `obs_hotkey_register_frontend` | OBS hotkey thread | hotkey | planned |
| Qt signals (buttons, timer) | `src/ui/dock.cpp` via `connect` | Qt event loop | UI | planned |

## Why it is built this way

The reasons, with the options we rejected, are in `docs/decisions.md`: speeding up live instead of after recording (ADR-001), using our own recorder instead of OBS's pause button (ADR-002), and letting frames pass through system memory (ADR-003).
