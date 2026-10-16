# Timelapse Switch

**Speed up the slow parts. While you record.**

Timelapse Switch is a plugin for OBS Studio (Windows, macOS and Linux) that lets you switch between real time and timelapse **during** a recording, without pausing. When you press Stop, the file is already finished: the parts you sped up play at x2, x4 or x8, the rest plays at normal speed, and there is nothing left to edit.

> **Status: early development (0.1.0).** The design is done and the project skeleton builds, but the plugin does not do anything useful yet. Watch the repository to know when the first version is ready.

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

## Documentation

- `docs/architecture.md`: how it works inside OBS
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
