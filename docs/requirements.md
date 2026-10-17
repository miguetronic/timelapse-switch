# Requirements

What Timelapse Switch must do, each item with an ID so it can be traced to code, tests and releases (see `docs/quality.md`). Interface and installation requirements (`UX-`, `INS-`) live in `docs/design/ux-spec.md`.

**How to read the table**

- **Priority:** *Must* (no release without it), *Should* (expected in 1.0), *Could* (nice to have).
- **Verified by:** *Test* (automatic), *Demo* (a person follows a written procedure), *Inspection* (review of code or docs), *Measure* (a recorded measurement).
- **Status:** *Draft* until the first release that implements it, then the version number.

Wording follows the usual convention: **must** is mandatory, **should** is recommended.

## Recording behavior

| ID | Requirement | Priority | Verified by | Status |
|---|---|---|---|---|
| REQ-001 | During a recording, the user must be able to switch between real time and timelapse at any moment, without pausing or stopping. | Must | Demo | Draft |
| REQ-002 | Timelapse speeds must include x2, x4 and x8, and should include x16. The speed must be selectable before and during a recording. | Must | Test, Demo | Draft |
| REQ-003 | When the user stops, the file must be complete and playable, with the timelapse sections already sped up. No post-processing step is allowed. | Must | Demo | Draft |
| REQ-004 | In a timelapse section at speed N, exactly 1 of every N frames must be kept, at a regular cadence. | Must | Test | Draft |
| REQ-005 | Audio and video must stay in sync across any number of switches, without drift: the audio never more than half an audio block from its place (10.7 ms at 48 kHz), the video never more than half a frame (16.7 ms at 30 fps), so they are never more than about 27 ms apart, well under the roughly 45 ms at which viewers notice. | Must | Test, Measure | Draft |
| REQ-006 | During timelapse sections, the audio must be silence by default. | Must | Test | Draft |
| REQ-007 | Each switch should add a chapter marker when the file format supports it (Hybrid MP4). | Should | Demo | Draft |
| REQ-020 | While recording, the dock must stay silent about the recording's health unless something needs attention (disk running out, frames lost, encoder falling behind), and must offer the session's details on request (waiting skipped, compression, file size and rate, disk left, time per mode, switches, pauses, speed map) at no measurable cost. | Should | Test, Demo | Draft |
| REQ-019 | During a recording, in real time or in timelapse, the user must be able to pause and resume as often as they like. Nothing recorded while paused reaches the file, and the file continues without a gap or a jump. While paused, the user can choose the mode and speed to resume in (by default the ones before the pause), and the dock always shows which. Stopping while paused must save the file normally. Many quick pauses must never freeze OBS, lose sync or damage the file. | Must | Test, Demo | Draft |

## Compatibility

| ID | Requirement | Priority | Verified by | Status |
|---|---|---|---|---|
| REQ-008 | The plugin must use only the public OBS API, and must work with OBS 31.1 or newer on Windows x64, macOS (Apple Silicon and Intel) and Linux x86_64. | Must | Inspection, Demo | Draft |
| REQ-009 | Recordings must use OBS's own encoders and file formats. Any encoder that has a variant accepting frames from system memory must work. | Must | Demo | Draft |
| REQ-010 | The plugin must reuse the recording settings of the current OBS profile (folder, format, encoder) without modifying them. | Should | Demo | Draft |

## Reliability and feedback

| ID | Requirement | Priority | Verified by | Status |
|---|---|---|---|---|
| REQ-011 | The plugin must never crash or freeze OBS. On any failure during a recording, it must stop its own recording, keep the file playable, and report what happened. | Must | Test, Inspection | Draft |
| REQ-012 | Every failure must have an error code (`TSW-Exxx`), an English log line and a translated message in the dock that says what happened, what is safe, why, and what to do. | Must | Inspection | Draft |
| REQ-013 | The real-time callbacks must not allocate memory, write logs, wait on locks or perform I/O. | Must | Inspection, Test | Draft |

## Interface

| ID | Requirement | Priority | Verified by | Status |
|---|---|---|---|---|
| REQ-015 | The dock must show the current state, the real time recorded and the length of the final video. | Must | Demo | Draft |
| REQ-016 | Record, stop, switch and speed must be available as OBS hotkeys. | Must | Demo | Draft |
| REQ-017 | The interface must be available in English and Spanish from the first release. | Must | Inspection | Draft |

## Performance

| ID | Requirement | Priority | Verified by | Status |
|---|---|---|---|---|
| REQ-014 | On the reference PC (Xeon E5-2689, Radeon RX 580, OBS 32.2, Linux), a 4K30 recording must not lose frames because of the plugin. Load tests need the maintainer's explicit approval before they run. | Must | Measure | Draft |
| REQ-018 | The documentation must say plainly what recording with the plugin costs compared with OBS's own recording: CPU, RAM, graphics card and the data that crosses to and from the card, per resolution, with numbers measured on the reference PC. The README must carry the short version. It must be updated whenever the way frames travel changes. | Must | Inspection, Measure | Draft |

## Open questions

These come from `docs/design/ux-spec.md`, section 7, and will become requirements once decided:

- Non-integer speeds (x1.5)?
- Allow OBS's recording and ours at the same time, or warn only?
- Sign the installers (Apple Developer ID, Windows signing) before 1.0?
- A visible "x4" mark in the video, sped-up audio, sound cues?
- Speed chips clicked in real time: only pre-select the next speed, or start timelapse like the speed hotkeys?
