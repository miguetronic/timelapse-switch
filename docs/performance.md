# Performance and memory

What recording with Timelapse Switch costs your computer, compared with recording with OBS's own Record button, and why. The numbers were measured on the reference PC; section 8 shows how to measure on yours. Sizes are in MB, where 1 MB = 1,000,000 bytes.

## 1. The short answer

| | With Timelapse Switch, compared with OBS's own recording |
|---|---|
| Picture quality and file size | **the same**: same encoder, same settings, same file format |
| What is on screen (scene, sources, filters) | **does not matter**: the extra cost depends only on resolution and frames per second |
| CPU | **a little more**: about 0.4 of one CPU core in 4K30, 3 % of the reference PC's CPU |
| RAM while recording | **more**: 286 MB in 4K30, about 70 MB in 1080p30; most of it is given back when the recording stops (section 4) |
| Graphics card | **more work, less memory**: its 3D engine 21 % busy instead of 8 %, 5 W more, and 373 MB per second crossing in each direction between the card and RAM; but about 130 MB less of the card's own memory |
| When the plugin is not recording | **almost nothing**: the dock refreshes 10 times per second; after a recording, OBS keeps 75 MB of its own (section 4) |

Why the "more"? Because frames take a longer road. The rest of this page explains that road, step by step.

## 2. Two roads for a frame

**Analogy.** Think of the graphics card as a kitchen, and of the encoder as the packing station inside it. With OBS's own recording on a GPU encoder, each dish goes from the stove to the packing station without leaving the kitchen. Timelapse Switch has to decide, dish by dish, whether it goes into the box: in real time all of them do, in timelapse x4 only 1 of every 4. The plugin makes that decision at a table outside the kitchen, in the computer's memory (RAM). So every dish walks out of the kitchen and back in, through a corridor (the PCIe bus), and is copied at each of the three tables it passes on the way.

```
 Road A: OBS's own recording with a GPU encoder (for example hevc_ffmpeg_vaapi_tex)

   card   finished picture --> encoder --> compressed video --> file
          (the frame never leaves the card; a queue of 10 frames waits there)


 Road B: Timelapse Switch, with the encoder's memory variant (hevc_ffmpeg_vaapi)

   card   finished picture
                |
                | (1) down the bus
                | (2) OBS copies it into its memory feed
                v
   RAM    OBS's memory feed       6 frames
                |
                | (3) the valve copies the frames it keeps
                v
   RAM    plugin's feed          16 frames
                |
                | (4) the encoder copies the frame into its own buffer
                v
   RAM    encoder's frame         1 frame
                |
                | (5) up the bus
                v
   card   encoder --> compressed video --> file
```

| Step | Who does it | Happens for |
|---|---|---|
| (1) The card copies the finished picture into RAM ("download") | OBS, on its graphics thread (`download_frame`) | every frame OBS draws |
| (2) OBS copies it into its memory feed, which hands it to everyone who asked for frames in memory | OBS, on its graphics thread (`output_video_data`) | every frame OBS draws |
| (3) The valve copies the frames it keeps into the plugin's own feed | Timelapse Switch (`src/recorder-rt.c`) | the frames that go into the file: all of them in real time, 1 of N in timelapse xN |
| (4) The encoder copies the frame into its own buffer | OBS's encoder, memory variant | the frames that go into the file |
| (5) The encoder sends the frame up to the card | OBS's encoder, memory variant | the frames that go into the file |

So in timelapse the plugin costs a little less than in real time: steps (3) to (5) happen only for the frames that are kept. Steps (1) and (2) happen for every frame while the plugin records.

**Why not stay inside the card?** On road A the encoder takes every frame OBS draws, in order, and OBS offers no way to hold one back or send one twice. Holding frames back is exactly what a timelapse is. The alternative we studied (two encoders on the card, one of them at 1 of N frames) cannot change speed while recording, waits up to 2 seconds for each switch, and cannot write Hybrid MP4; see `docs/decisions.md`, ADR-003.

**Road B is not new to OBS.** It is the road every recording with x264 (or another CPU encoder) takes, and the road OBS uses whenever a plugin asks for frames in memory. With x264, OBS already does steps (1) and (2), and x264 does its own step (4); the plugin adds step (3) and its feed.

## 3. How much data moves

OBS's default color format, NV12, uses 1.5 bytes per pixel. A frame of 1920 x 1080 pixels is therefore 1920 x 1080 x 1.5 = 3.1 MB.

| Recording | One frame | Each step above, per second |
|---|---|---|
| 1080p, 30 fps | 3.1 MB | 93 MB/s |
| 1080p, 60 fps | 3.1 MB | 187 MB/s |
| 1440p, 30 fps | 5.5 MB | 166 MB/s |
| 4K (2160p), 30 fps | 12.4 MB | 373 MB/s |
| 4K (2160p), 60 fps | 12.4 MB | 746 MB/s |

With OBS set to I444 or to a 10-bit format (P010), frames are twice as big, and so is everything in this section and the next.

**Is that a lot?** Steps (1) and (5) cross the PCIe bus, one in each direction. The reference PC's bus (PCIe 3.0, 16 lanes) carries about 15,750 MB/s each way, so 4K30 uses about 2.4 % of it. Steps (2) to (4) are copies inside RAM, which on any recent computer moves several thousand MB per second. The cost is real, but small next to what the hardware can do; it matters mostly on a computer that is already near its limit.

## 4. RAM while recording

Each waiting area of road B holds whole frames, so the RAM it takes is easy to count:

| Part | Frames | 1080p | 4K | Why it exists |
|---|---|---|---|---|
| OBS's memory feed | 6 | 18.7 MB | 74.6 MB | OBS's own; it takes RAM only once someone asks for frames in memory |
| The plugin's feed | 16 | 49.8 MB | 199.1 MB | lets the encoder fall behind for about half a second (at 30 fps) without losing a frame |
| The encoder's frame | 1 | 3.1 MB | 12.4 MB | the encoder's own buffer before sending the frame up to the card |
| **Total** | **23** | **71.5 MB** | **286.2 MB** | |
| **Measured** (more than OBS's own recording) | | 65 MB (a 20-second trial) | 286.3 MB | section 5 |

**When it is given back.** When the recording stops, the plugin closes its feed and the encoder, and gives their 17 frames back (211 MB in 4K). OBS keeps its own memory feed (6 frames, 75 MB in 4K) ready until you close OBS, exactly as it does after any recording with x264. Measured in 4K, once the file was saved, OBS held 74 MB more than after its own recording: those 6 frames.

```
  RAM that recording added to OBS, 4K30, measured (MB)

  OBS's own recording   while recording  #                                    7
                        once saved       #                                    6

  Timelapse Switch      while recording  ###################################  293
                        once saved       ##########                           80
                                         ^ OBS's memory feed, kept until OBS closes
```

## 5. Measured on the reference PC

Recording 4K at 30 frames per second for 5 minutes, each road in a fresh OBS with the same scene and the same encoder settings, each compared with its own idle minute. The numbers are what recording **added** to OBS while idle, except the busy shares, which are the values while recording.

| | OBS's own recording | Timelapse Switch | What it means |
|---|---|---|---|
| CPU | 0.05 cores | 0.47 cores | less than half a core; the reference CPU has 16 threads |
| RAM | 7 MB | 293 MB | the 23 frames of section 4, almost to the byte |
| Graphics card memory | 222 MB | 90 MB | **less** with the plugin: road A keeps a queue of 10 frames inside the card (124 MB), road B keeps its queue in RAM instead |
| Graphics card 3D engine, busy | 7.6 % | 21.1 % | most likely moving frames down and up (steps 1 and 5) |
| Graphics card video encoder, busy | 61 % | 76 % | the same video takes the encoder longer on road B; the reason is not confirmed yet |
| Graphics card power | 16 W | 21 W | |
| PCIe packets sent by the card | almost none | 11.4 million per second | the frames going down to RAM (step 1) |
| OBS's graphics thread, per frame (median) | 0.44 ms | 3.01 ms | out of the 33.3 ms each frame has at 30 fps; mostly step (2) |
| Lost frames | 0 of 9,000 | 0 of 8,999 | |

Measured on 2026-10-16. The full report, with the method, the idle values and OBS's own timings: `docs/test-reports/2026-10-16-recording-cost.md`.

The scene in this measurement is light (a small video scaled to 4K). That is on purpose: the extra cost does not depend on the scene (section 6), and a light scene keeps it from hiding behind the scene's own cost.

## 6. What does not change

- **Quality and file size.** The plugin uses the encoder chosen in your OBS profile with the same settings; for a GPU encoder it picks its memory variant (`hevc_ffmpeg_vaapi` for `hevc_ffmpeg_vaapi_tex`), which encodes the same frames with the same settings.
- **Timing.** On road B each frame keeps its own time, as in OBS's own x264 recordings. OBS's road A labels each frame with the time of the frame before it, so the plugin's picture sits about one frame later than in an OBS recording made with a GPU encoder: the plugin's is the true time (`docs/architecture.md`, "Timing").
- **The scene.** Sources, filters and transitions are drawn by OBS before the frame reaches either road, so a heavy scene costs the same with or without the plugin.

## 7. When should you care?

- **Your computer records 4K with OBS today with room to spare:** the plugin fits too. The reference PC, built around a 2012 server processor, had plenty left (section 5).
- **Your computer is near its limit** (OBS's Stats dock already shows frames missed): the plugin's extra copies may be what tips it over. Try 1080p, or 30 fps instead of 60.
- **Your computer has little RAM** (8 GB or less) and you record in 4K: count on about 290 MB more while recording, on top of OBS itself.
- **You record with x264:** the extra cost is smaller, because OBS already takes road B (section 2).

## 8. Measure it on your computer

- **OBS's Stats dock** (Docks > Stats) shows OBS's CPU and memory, and frames missed while rendering or encoding. Compare it while idle and while the plugin records.
- **The plugin's log line** when a recording stops (Help > Log Files > View Current Log) gives the frames it received, sent and lost, and how many frames, at most, waited in its feed for the encoder (section 9).
- **For contributors:** `tests/integration/measure.sh` repeats the measurement of section 5 on a Linux computer with an AMD graphics card; see `tests/integration/README.md`, "Measuring the cost".

## 9. Could it cost less?

Perhaps, and only with measurements to show it is worth it:

- **A smaller plugin feed.** The plugin's feed is a waiting room with 16 seats between the valve and the encoder. If the encoder stalls for a moment, frames wait there; if all 16 seats are taken, the next frame is lost (nothing crashes, but that picture is missing, and the log says so). 16 is the most OBS allows: about half a second at 30 fps, a quarter of a second at 60 fps. OBS's own memory feed has 6.

  Fewer seats would save RAM (12.4 MB per seat in 4K) but leave less room for stalls. So the feed stays at 16, and it is **not a setting**: stability comes first, and the right number depends on encoder stalls that a user cannot see, so a wrong choice would show up later as lost frames, far from its cause. Instead, the plugin writes in its log how many frames, at most, waited:

  ```
  [timelapse-switch] recording stopped at ... | feed: at most 1 of 16 frames waiting
  ```

  "1 of 16" means each frame found the encoder free. That is what the reference PC showed in 1080p30 and in 4K30 (5 minutes, 80 speed switches), even with a virtual machine and software builds running at the same time. So on this PC fewer seats would do; but a slower encoder or a busier computer needs more room, and 199 MB of RAM is cheap next to a lost picture, so the feed stays at 16. If recordings on many computers show that it never fills beyond a few seats, the default can shrink for everyone, still with no setting.
- **Road A for real time only.** Not possible within one file today: a file cannot switch roads halfway (ADR-003).
