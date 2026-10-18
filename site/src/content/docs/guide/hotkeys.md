---
title: Hotkeys
description: Every action of Timelapse Switch, on a key of your choice.
---

Bind keys in **Settings > Hotkeys**. Every action starts with "Timelapse Switch:", and none is bound at first, so nothing clashes with your other shortcuts.

| Action | What it does |
|---|---|
| Record | Starts a recording, in the mode the dock shows |
| Stop and save | Stops and finishes the file |
| Switch to timelapse | Goes to timelapse at the chosen speed |
| Back to real time | Returns to normal speed |
| Speed x2, x4, x8, x16 | Goes to timelapse at that speed directly |
| Faster, Slower | Steps the timelapse speed up or down |
| Pause or resume | Pauses, or resumes in the mode chosen while paused |
| Add chapter marker | Adds a chapter at this moment (Hybrid MP4) |

A second press of a mode switch (to timelapse or back to real time) within 0.4 seconds of the last one counts as a double press and is ignored, so a bouncing key never switches twice; Pause has the same guard, with its own clock. Changing the speed while already in timelapse always applies.

**Linux with Wayland:** hotkeys work only while OBS has the focus; this is a limit of Wayland, not of the plugin. On X11 they work from any window.

**macOS:** for hotkeys from other apps, OBS needs **Input Monitoring** permission (System Settings > Privacy & Security).
