---
title: Questions and answers
description: Short answers to what users ask about Timelapse Switch.
---

## Does it replace OBS's Record button?

No. The dock records its own file; OBS's **Start Recording** keeps working as always, at normal speed. Use one or the other.

## Why is the sound silent in timelapse?

Sped-up sound is noise, and keeping it would make the audio track longer than the picture. Silence keeps both in step; real-time parts keep their sound.

## Why is the picture one frame later than in OBS's own recording?

OBS's own recording with a GPU encoder labels each picture with the time of the picture before it, so its pictures show up one frame early. The plugin's recordings, like OBS's own x264 recordings, put each picture at its true time. One frame is 33 ms at 30 fps: usually nobody notices. If you tuned your microphone's **Sync Offset** on GPU recordings and your voice now seems slightly early, add one frame to it (33 ms at 30 fps, 17 ms at 60 fps).

## Can I record with OBS's Record button and Timelapse Switch at the same time?

Yes, but in 4K it is best not to: two recordings give the graphics card's video encoder twice the work, and on a modest card both lose frames. The dock warns while both run.

## Why does OBS's preview look less smooth while the plugin records?

The graphics card has more to do: see [Performance](/timelapse-switch/guide/performance/#the-preview-while-recording). The recording is not affected.

## Which formats get chapters?

Hybrid MP4 (in OBS: **Settings > Output > Recording Format**). Other formats record normally, without chapters.

## Why does the timelapse part look blurrier than the rest?

In timelapse each picture differs much more from the one before it, so the encoder needs more data to describe it: about three times as much at ×4. How your OBS profile sets the encoder decides what happens then:

- **Quality-based** (CQP, CRF or ICQ, and OBS's simple output mode with a quality preset): the encoder spends what it needs. The timelapse looks as sharp as the rest; the file just grows faster in that part.
- **Fixed bitrate** (CBR, or VBR with a low limit): the encoder cannot spend more, so the timelapse loses detail, right where things move most.

If the timelapse looks soft, open **Settings > Output > Recording** (Output Mode **Advanced**) and set **Rate Control** to CQP or CRF, or raise the bitrate. The plugin uses the same settings as OBS's own recording.

## Does the file say it was made with Timelapse Switch?

No. Inside the file the tracks are called "Video" and "Audio", and the chapters "Real time" and "Timelapse x4", in the language of your OBS. Only the file name ends in "tswitch", so you can tell it apart from OBS's own recordings; rename it as you like.

## Does it send anything over the internet?

No. No analytics, no update checks; it only opens a web page when you click a link in the dock.

## Something went wrong. What do I do?

Every message has a code such as TSW-E103, in the dock or in the OBS log; its page in [Messages](/timelapse-switch/errors/) says what it means and what to do. Error cards link to it with "Learn more". If it is a bug, [report it](https://github.com/miguetronic/timelapse-switch/issues/new?template=bug.yml) with your OBS log (Help > Log Files > Upload Current Log File).
