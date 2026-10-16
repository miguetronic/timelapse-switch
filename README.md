# Timelapse Switch

**Speed up the slow parts. While you record.**

Timelapse Switch is a plugin for OBS Studio (Windows, macOS and Linux) that lets you switch between real time and timelapse **during** a recording, without pausing. When you press Stop, the file is already finished: the parts you sped up play at x2, x4 or x8, the rest plays at normal speed, and there is nothing left to edit.

> **Status: early development (0.1.0).** It already records at mixed speeds, with chapters and its dock, and it is tested inside a real OBS on Linux. Still missing before the first release: the settings dialog, the welcome card, the checks before recording, and testing on Windows and macOS. Watch the repository to know when the first version is ready.

## Who it is for

Anyone whose recordings have long, slow stretches:

- a painter waiting for layers to dry;
- a model maker, a crafter, a cook;
- a developer coding live.

Today you record everything and speed it up later in an editor. With Timelapse Switch, you press a button while recording.

## How it works, in one picture

```
   OBS picture  ---->  [ Timelapse Switch ]  ---->  OBS encoder  ---->  one file
                       real time: every frame passes                    mixed speeds,
                       timelapse x4: 1 frame out of 4 passes            ready to upload
```

Think of a tape recorder with a fast-forward pedal: while you hold the pedal, the tape records only 1 of every 4 moments, so that part plays 4 times faster. OBS encoders count frames instead of looking at the clock, so a section with fewer frames simply plays faster.

## What it costs your computer

In short: **the same picture, with some extra work for your computer.**

- **Quality and file size are the same** as with OBS's own Record button: the plugin uses the encoder and the settings of your OBS profile.
- **The extra work comes from the road each frame takes.** To choose which frames go into the file, the plugin needs each frame in the computer's memory (RAM). OBS's own recording with a GPU encoder never takes the frame out of the graphics card. With the plugin, each frame goes down from the card to RAM, waits in three places there (each one a copy), and goes back up to the card's encoder.

```
  OBS alone, GPU encoder           With Timelapse Switch

  card   picture                   card   picture
            |                                |  down the bus
            |                      RAM    OBS's memory feed    6 frames
            |                                |  the valve
            |                      RAM    plugin's feed       16 frames
            |                                |
            |                      RAM    encoder's frame      1 frame
            v                                v  up the bus
  card   encoder                   card   encoder
```

Think of the graphics card as a kitchen and the encoder as its packing station. OBS alone passes each dish from the stove to the packing station without leaving the kitchen. The plugin must look at each dish to decide whether it goes into the box (in timelapse x4, only 1 of every 4 does), and its table is outside the kitchen. So every dish walks out and back in.

Measured on the reference PC (2012 Xeon, Radeon RX 580, Linux), recording 4K at 30 frames per second for 5 minutes:

| What recording added | OBS alone | With the plugin |
|---|---|---|
| CPU | 0.05 cores | 0.47 cores (3 % of that 16-thread CPU) |
| RAM | 7 MB | 293 MB |
| Graphics card memory | 222 MB | 90 MB (less: the plugin keeps its frames in RAM instead) |
| Graphics card power | 16 W | 21 W |
| Lost frames | 0 | 0 |

- **The cost depends on resolution and frame rate, not on your scene.** OBS draws the scene before the frame reaches either road.
- **Most of that RAM is given back when the recording stops.** The plugin returns its part (211 MB in 4K). OBS keeps its own 75 MB ready until you close it, as it does after any recording with x264.
- **With x264** (or another CPU encoder), OBS itself already takes frames out of the card, so the plugin adds less.

Every step, the numbers for each resolution and how to measure on your own computer: `docs/performance.md`.

## Documentation

- `docs/architecture.md`: how it works inside OBS
- `docs/performance.md`: what it costs your computer, and why
- `docs/faq.md`: questions and answers, such as sound and picture timing
- `docs/design/ux-spec.md`: the dock and installation
- `docs/requirements.md`: what it must do
- `docs/decisions.md`: why it is built this way
- `docs/building.md`: how to build it
- `CONTRIBUTING.md`: how we work, including the coding rules (based on NASA/JPL's "The Power of Ten")

## How this project is made

Timelapse Switch is written largely with the help of an AI coding assistant (Claude, by Anthropic), and every change is reviewed by the maintainer. The code follows strict, machine-checked rules (`docs/coding-rules.md`) whoever writes it. Releases are published here on GitHub.

## License

GPL-2.0-or-later, see `LICENSE`.

Timelapse Switch is an independent project by Miguel Parra (Miguetronic). It is not affiliated with or endorsed by the OBS Project. OBS and OBS Studio are trademarks of their respective owners.
