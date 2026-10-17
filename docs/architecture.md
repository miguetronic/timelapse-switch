# Architecture

How Timelapse Switch works inside OBS, in plain words first and in detail after.

> **Status:** this is the agreed design (see `docs/decisions.md`). The recorder, its real-time path, the hotkeys, chapters and the dock exist; the settings dialog, the welcome card and the preflight check are **planned**.

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

**The price of the valves.** To choose frames, the video valve needs them in system memory, so frames leave the graphics card, wait in three places in RAM (OBS's memory feed, our feed, the encoder's buffer) and go back up to the card's encoder (ADR-003). In 4K30 that is 373 MB per second each way across the bus and about 286 MB of RAM while recording. Every step, and what it costs measured against OBS's own recording: `docs/performance.md`.

## Modules

| Module | File(s) | Language | Job |
|---|---|---|---|
| Plugin entry | `src/plugin-main.c` | C | Load and unload; hotkeys, settings, OBS closing. **Exists.** |
| Session | `src/core/session.c` | C | Pure logic, no OBS: what each button and hotkey means in each state (also paused), the chosen speed, starting mode and mode to resume in, the double-press guard (UX-046). **Exists**, unit tested. |
| Dock view | `src/core/dock-view.c` | C | Pure logic, no OBS or Qt: what each zone of the dock shows, the summary and error cards. **Exists**, unit tested. |
| Controller | `src/controller.c` | C | The one place where hotkeys and dock buttons act, so both give the same result (UX-053). **Exists.** |
| Logging | `src/plugin-support.*` | C | `tsw_log`: OBS log lines with the plugin prefix. **Exists.** |
| Compiler differences | `src/tsw-compiler.h` | C | The only conditional compilation. **Exists.** |
| Timeline | `src/core/timeline.c` | C | Pure math, no OBS: maps any OBS time to its position in the final video; a pause is a section in which the final video stands still (ADR-011). **Exists**, unit tested. |
| Gates | `src/core/gate.c` | C | Pure math, no OBS: from that position, how many copies of each frame and which audio blocks go to the encoder. **Exists**, unit tested, including a 10-minute sync simulation. |
| Audio ring | `src/core/audio-ring.c` | C | Pure, no OBS: the lock-free queue that carries audio blocks from OBS's audio thread to ours. **Exists**, unit tested. |
| Valves (real-time path) | `src/recorder-rt.c` | C | Real-time: sends each frame 0, 1 or more times into our video feed, and passes, silences or skips each audio block, as the gates say. **Exists.** |
| Recorder | `src/recorder.c` | C | Creates the encoders, feeds and file writer from the user's OBS settings; starts, switches speed, adds chapters (Hybrid MP4), stops (emptying the encoder first); reports its status to the dock. **Exists.** |
| Fault log | `src/fault.c` | C | `TSW_CHECK` failures, safe to note from real-time threads, written to the log by the UI thread. **Exists.** |
| Platform | `src/platform/{linux,windows,macos}.c` | C | Small OS differences, picked by CMake (no `#ifdef`). |
| Dock | `src/ui/dock.cpp` | C++ / Qt | The panel described in `docs/design/ux-spec.md`, drawn as in `docs/design/visual-design.md`: draws a dock view, reports clicks; no OBS, so `tests/ui` draws it alone. **Exists.** |
| Dock theme and painting | `src/ui/dock-theme.cpp`, `dock-status.cpp`, `dock-widgets.cpp`, `dock-glyph.cpp`, `dock-paint.cpp` | C++ / Qt | Reads the OBS theme through hidden probe widgets, mixes the dock's colors with the contrast guard, paints the status card, buttons, rails and glyphs. **Exists.** |
| Color math | `src/core/color.c` | C | Pure, no OBS or Qt: WCAG contrast, mixing, and the contrast guard. **Exists**, unit tested. |
| Dock in OBS | `src/ui/dock-obs.cpp` | C++ / Qt | Adds the dock to OBS's Docks menu, in a scroll area; a timer shows the controller's view 10 times per second. **Exists.** |

## Threads

See `docs/coding-rules.md`, B1, for the rules. In short: the valves run on OBS's real-time threads and must never wait, allocate or log; while stopping, a short check runs on OBS's graphics thread; everything else runs on the UI thread.

## Timing

Three details keep sound and picture together. Each one was found by measuring (`tests/integration/README.md`).

```
  OBS draws frame k at time T ----------> the plugin gets it at about T + 38 ms
  OBS mixes the audio block of time t --> the plugin gets it at about t + 25 ms
```

1. **Stamped time, not arrival.** Frames and audio arrive late, by different amounts. The gates place each one by the time OBS stamped on it, never by when it arrived.
2. **Start.** The timeline starts once the encoders and the file writer are running (starting them takes tens of milliseconds), and the first frame opens the first slot of the video wherever it falls. Before these two fixes the video gate repeated the first frame to fill the gap, and the video trailed the sound by about one frame.
3. **Stop.** Encoders keep some frames (x264 about 40, so 1.3 s at 30 fps) and hand them over only as new frames arrive. After stop, the plugin keeps sending frames until the file writer has every frame sent before the stop (or nothing moves for 1 s, or 5 s pass), and then stops it at its next packet.

**Memory path and GPU path** (road B and road A in `docs/performance.md`). In OBS's own recording with a GPU encoder (the `_tex` encoders), frames never leave the graphics card, and libobs labels each frame on that path with the time of the frame before it (`output_gpu_encoders` in `libobs/obs-video.c`, OBS 32.2.2). On the memory path, used by x264 and by this plugin (ADR-003), each frame keeps its own time. So the plugin's picture sits where it does in OBS's own x264 recordings, and about one frame (33 ms at 30 fps) later than in OBS's own GPU-encoder recordings. The plugin keeps the true time rather than copy that shift. What this means for users, and how to adjust a microphone's sync offset tuned on GPU recordings: `docs/faq.md`.

## Callback table (rule P10-9)

Function pointers are allowed only where OBS or Qt require a callback. Every one of them is listed here, so the flow of control can be followed without guessing. A pull request that adds or removes a callback updates this table.

| Callback | Registered in | Called by | Thread | Status |
|---|---|---|---|---|
| `obs_module_load` / `obs_module_unload` | `plugin-main.c` (`OBS_DECLARE_MODULE`) | OBS at startup and exit | UI | exists |
| Hotkeys (`on_record`, `on_stop`, `on_timelapse`, `on_real_time`, `on_single_key` for speeds, Faster, Slower and chapter marker) | `plugin-main.c` via `obs_hotkey_pair_register_frontend` and `obs_hotkey_register_frontend` | OBS hotkey thread; they only queue an action | hotkey | exists |
| Queued action (`run_action`) | `plugin-main.c` via `obs_queue_task`; it calls the controller | OBS UI | UI | exists |
| Frontend events (`on_frontend_event`: OBS closing) | `plugin-main.c` via `obs_frontend_add_event_callback` | OBS UI | UI | exists; profile change and OBS recording planned |
| Raw video (`tsw_recorder_receive_frame`) | `recorder.c` via `obs_add_raw_video_callback` | OBS video thread, once per frame while recording | video | exists |
| Raw audio (`tsw_recorder_receive_audio`) | `recorder.c` via `obs_add_raw_audio_callback` | OBS audio thread, once per audio block while recording | audio | exists |
| Audio feed input (`tsw_recorder_provide_audio`) | `recorder.c` via `audio_output_open` | our audio feed's thread, every 1024 samples | audio | exists |
| Output signal `stop` (`output_stopped`) | `recorder.c` via the output's signal handler | OBS output thread, once the file is complete | output | exists |
| Stop check (`flush_tick`) | `recorder.c` via `obs_add_tick_callback`, only while stopping | OBS graphics thread, once per frame | graphics | exists |
| End of recording (`finish`) | `recorder.c` via `obs_queue_task` | OBS UI | UI | exists |
| Qt signals of the dock's buttons and chips | `src/ui/dock.cpp` via `connect`; they emit `inputRequested` or `dismissRequested` | Qt event loop | UI | exists |
| Dock signals and refresh timer (100 ms) | `src/ui/dock-obs.cpp` via `connect`; they call the controller | Qt event loop | UI | exists |
| Theme changes (`onFrontendEvent`: `OBS_FRONTEND_EVENT_THEME_CHANGED`) | `src/ui/dock-obs.cpp` via `obs_frontend_add_event_callback`; it queues the dock's `scheduleRestyle` | OBS UI, after every theme, Font Size or Density change | UI | exists |
| Dock animations (thumb slide, card cross-fade, saving sweep) | `src/ui/dock-status.cpp`, `src/ui/dock-widgets.cpp` via `connect` on `QVariantAnimation` | Qt event loop, only while an animation runs (at most 5 s) | UI | exists |

The sync probe of the integration test (`tests/integration/sync-probe.c`) is a separate module that is never shipped, so it is not listed.

## Why it is built this way

The reasons, with the options we rejected, are in `docs/decisions.md`: speeding up live instead of after recording (ADR-001), using our own recorder instead of OBS's pause button (ADR-002), and letting frames pass through system memory (ADR-003).
