---
title: Performance
description: What Timelapse Switch costs your computer, and why.
---

In short: **the same picture, with some extra work for your computer.**

- **Quality and file size are the same** as with OBS's own Record button: the plugin uses the encoder and settings of your OBS profile.
- **The extra work comes from the road each frame takes.** To choose which frames go into the file, the plugin needs each frame in the computer's memory. OBS's own recording with a GPU encoder never takes the frame out of the graphics card; with the plugin, each frame comes down from the card and the encoder takes it back up.

Measured on the reference PC (a 2012 Xeon, a Radeon RX 580, Linux), recording 4K at 30 frames per second:

| What recording added | OBS's Record button | Timelapse Switch |
|---|---|---|
| CPU | 0.05 cores | 0.37 cores |
| RAM | 7 MB | about 200 MB, given back when the file is saved |
| Lost frames | 0 | 0 |

The cost depends on resolution and frame rate, not on your scene. With x264 or another CPU encoder, OBS itself already takes frames out of the card, so the plugin adds less.

## The preview while recording

While the plugin records, the graphics card has more to do, so on some computers **OBS's preview can look a little less smooth**, as if it skipped now and then. The recording is not affected, and neither is the rest of your desktop. On Linux the plugin already takes the frames in the lightest way OBS allows plugins to.

What helps:

- Close heavy programs while you record.
- During long recordings, turn the preview off: right-click it and untick **Enable Preview**. The recording does not need it.
- Do not record with OBS's own Record button and the plugin at the same time: two recordings give the video encoder twice the work, and the dock warns about it ([TSW-E302](/timelapse-switch/errors/tsw-e302/)).

The full measurements and how to repeat them are in the repository: [docs/performance.md](https://github.com/miguetronic/timelapse-switch/blob/main/docs/performance.md).
