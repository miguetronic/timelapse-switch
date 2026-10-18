---
title: How to record a timelapse in OBS Studio
description: Three ways to make a timelapse with OBS Studio, lowering the frame rate, speeding up in an editor, or switching speed while you record, and when each one fits.
---

OBS Studio has no timelapse button, but there are three ways to get one. Which one fits depends on a single question: **is the whole recording a timelapse, or only some parts of it?**

| | Whole recording sped up | Some parts sped up | Editing afterwards |
|---|---|---|---|
| 1. Lower the frame rate | Yes | No | Yes, to speed it up |
| 2. Record normally, speed up later | Yes | Yes | Yes |
| 3. Timelapse Switch | Yes | Yes | No |

## 1. Lower the frame rate

OBS records fewer frames per second, and an editor plays them back faster.

1. Open **Settings > Video**.
2. In **FPS Type**, choose **Fractional FPS Value** and set, for example, **Numerator 1** and **Denominator 2**: one frame every 2 seconds.
3. Record as usual. Then, in an editor or with ffmpeg, play the file back at 30 frames per second.

Good to know:

- **It changes all of OBS,** not just the recording: the preview, a stream or a virtual camera run at that frame rate too.
- **It cannot change during a recording.** The whole file is a timelapse; to record a part in real time you stop, change the setting and start a new file.
- **Stopping can be slow** at very low frame rates, while OBS finishes the last frames.

Fits: a recording that is a timelapse from start to end, such as a sunset or a long render, when nothing else uses OBS at the same time.

## 2. Record normally, speed up later

Record at your usual frame rate, then speed up the slow parts in a video editor, or speed up a whole file with ffmpeg:

```sh
# Plays the recording 8 times faster and drops the sound.
ffmpeg -i recording.mkv -vf "setpts=PTS/8" -an -r 30 timelapse.mp4
```

Good to know:

- **It is the most flexible**: any part, any speed, decided after recording.
- **It costs time and disk space**: every slow minute is recorded in full, then cut and sped up by hand. For a long session, that is often more work than the recording itself.

Fits: short recordings, or when you edit the video anyway.

## 3. Switch speed while you record

[Timelapse Switch](/timelapse-switch/) is a free plugin for OBS Studio that adds a dock with one button. While you record, you switch between real time and timelapse x2, x4, x8 or x16, as many times as you want. OBS keeps its normal frame rate; the plugin decides, frame by frame, which frames go into the file.

When you press **Stop**, the file is finished: the slow parts are already sped up, the rest plays at normal speed with its sound, and in Hybrid MP4 there is a chapter at each change of speed.

Good to know:

- **It records a file of its own**, beside OBS's **Start Recording**, with the encoder and settings of your OBS profile.
- **In timelapse the sound is silent**: sped-up sound is just noise.
- **It asks more of your computer** than OBS's own recording: see [Performance](/timelapse-switch/guide/performance/).

Fits: long sessions that mix moments worth watching with long waits, such as crafts, building models, cooking or coding, when you want the video finished as soon as you stop.

[Install Timelapse Switch](/timelapse-switch/guide/install/), then follow [First steps](/timelapse-switch/guide/first-steps/).
