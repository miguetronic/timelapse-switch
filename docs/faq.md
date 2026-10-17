# Questions and answers

Short answers to questions users ask, with the reason behind each one.

## Sound and picture

### Why is the picture one frame later than in OBS's own recording?

Because OBS's own recording with a GPU encoder (one that encodes on the graphics card, such as `hevc_ffmpeg_vaapi_tex` on Linux) labels each picture with the time of the picture before it. So in those recordings every picture shows up one frame early. The plugin's recordings, like OBS's own x264 recordings, put each picture at its true time.

Think of a photo album where each photo carries the caption of the one before it: nothing is missing, but everything is shifted by one photo.

```
  true time            0 ms     33 ms    67 ms    100 ms
  plugin, x264         [ 1 ]    [ 2 ]    [ 3 ]    [ 4 ]
  OBS with GPU encoder [ 2 ]    [ 3 ]    [ 4 ]    [ 5 ]    each picture one slot early
```

One frame is 33 ms at 30 fps and 17 ms at 60 fps. Most people start to notice sound and picture apart at about 45 ms when the sound comes first, and later when the sound comes last, so usually nobody sees the difference.

The plugin keeps the true time on purpose: copying the shift would make its recordings wrong in order to match a mistake. If OBS corrects it one day, both recordings will agree. The details are in `docs/architecture.md`, "Timing" (found in `libobs/obs-video.c`, OBS 32.2.2, and measured on Linux with VAAPI; the code is shared by every GPU encoder).

### I tuned my microphone's Sync Offset while recording with a GPU encoder. Should I change it?

Only if, in the plugin's recordings, your voice seems to come slightly before your lips move. Your offset was tuned on recordings whose picture is one frame early, so for true timing it is one frame short.

To fix it, add one frame to the microphone's offset: **33 ms at 30 fps, 17 ms at 60 fps** (OBS: Edit > Advanced Audio Properties > Sync Offset). After that, OBS's own GPU recordings will have the opposite one-frame difference: the sound slightly late, which is much harder to notice than early sound.

### I record with OBS's Record button and with the plugin at the same time. Do the two files line up?

Within one frame. If you edit them together, slide one of them by one frame (the OBS file one frame later) and they match exactly.

## Performance

### Why does OBS's preview look less smooth while Timelapse Switch records?

To choose which frames go into the file, the plugin needs each frame in the computer's memory, and OBS copies it there on the same thread that draws the preview (3 to 8 ms per 4K frame). On a busy computer that copy takes a different time on every frame, so the preview can look uneven. **The recording is not affected**: it is as smooth as OBS's own. If the preview bothers you, lighten the computer's load, or turn the preview off during long recordings (right click on it, untick **Enable Preview**). The details and the measurements are in `docs/performance.md`, section 9.

### Can I record with OBS's Record button and with Timelapse Switch at the same time?

Yes, but in 4K it is best not to. Two recordings ask the graphics card's video encoder for twice the work; on a modest card both recordings then lose or repeat frames. The dock warns while both are running (TSW-E302). At lower resolutions the load is smaller; the dock's health line tells you if it is still too much.

