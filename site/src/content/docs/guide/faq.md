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

## Does it send anything over the internet?

No. No analytics, no update checks; it only opens a web page when you click a link in the dock.

## Something went wrong. What do I do?

Every message in the dock has a code such as TSW-E103 and a "Learn more" link to its page in [Messages](/timelapse-switch/errors/). If it is a bug, [report it](https://github.com/miguetronic/timelapse-switch/issues/new?template=bug.yml) with your OBS log (Help > Log Files > Upload Current Log File).
