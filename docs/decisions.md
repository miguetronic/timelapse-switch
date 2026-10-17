# Decisions

Each important decision is recorded here with its context, the options considered, and its consequences. This is what lets someone ask, years later, "why is it like this?" and get an answer. Records are never deleted; a decision that changes gets a new record that supersedes the old one.

Format of each record: **Context** (the problem), **Decision**, **Alternatives** (and why not), **Consequences** (what we accept).

---

## ADR-001. Speed up while recording, not after

*Date: 2026-10-14. Status: accepted.*

**Context.** Creators who paint, build, cook or code want the slow parts of their recordings sped up. Today they record everything and speed it up later in an editor, or with a script that runs FFmpeg over the whole file, which takes time and disk space. On the OBS forum this feature was requested in 2023 ("switch back and forth from normal time to timelapse") with no answer.

**Decision.** The file is sped up **during** the recording. When the user presses Stop, the file is final. There is no post-processing step and no external tool.

**Alternatives.**
- *Mark sections while recording, process with FFmpeg afterwards.* Keeps the original footage, but the user waits for the processing, and the plugin would depend on an FFmpeg program that OBS does not ship on Windows. Rejected by the maintainer.

**Consequences.** The frames skipped in timelapse sections are not saved: the full-speed version of those sections cannot be recovered. Recording both versions at once would mean encoding twice.

---

## ADR-002. Our own recorder, not OBS's pause button

*Date: 2026-10-14. Status: accepted.*

**Context.** OBS does not let a plugin drop frames from its own recording. One idea was to pause and resume OBS's recording very quickly (for x4: let 1 frame through, pause for 3, and so on).

**Decision.** The plugin runs **its own recorder**, built from OBS's own encoders and file writers, with its own Record and Stop buttons in the dock. This is the same pattern well-known plugins use (Aitum Vertical, Source Record).

**Alternatives.**
- *Pulse OBS's pause.* Rejected after reading the OBS 32 source (`libobs/obs-output.c`, `obs-encoder.c`):
  - each pause writes a line to the OBS log, so about 15 lines per second at x4;
  - a new pause can only start after the audio has caught up with the previous one, so the speed would be irregular;
  - the end of a pause is detected by an exact timestamp match, so if OBS skips that one frame under load, the recording can stay paused until the end.

**Consequences.**
- There are two record buttons in OBS (OBS's and ours). The dock must make this crystal clear (`docs/design/ux-spec.md`, section 3).
- OBS's own Record button keeps recording at normal speed.

---

## ADR-003. Frames pass through system memory (CPU path)

*Date: 2026-10-15. Status: accepted. Cost measured on 2026-10-16 (`docs/performance.md`); REQ-014 still to be confirmed with a real scene before release.*

**Context.** To choose which frames reach the encoder, the plugin needs the frames outside the GPU. Today, with GPU encoders such as `hevc_ffmpeg_vaapi_tex`, a frame can go from the scene to the encoder without ever leaving the graphics card.

**Decision.** The plugin receives each finished frame in system memory (`obs_add_raw_video_callback`), passes the selected ones into its own video feed (`video_output_open`), and encodes them with the CPU-frame variant of the user's encoder (for example `hevc_ffmpeg_vaapi` instead of `hevc_ffmpeg_vaapi_tex`). Audio follows the same idea with `obs_add_raw_audio_callback` and its own audio feed (`audio_output_open`), whose input can skip turns. This works because OBS encoders count frames and samples instead of reading the clock.

**Alternatives.**
- *GPU only, two encoders.* One encoder at full rate and one at 1 of N frames (`obs_encoder_set_frame_rate_divisor`), and the plugin switches between their outputs. Kept as **plan B**. Its costs:
  - the divisor cannot change while recording, so the speed is fixed per recording;
  - switching must wait for a keyframe, which takes up to 1 to 2 seconds;
  - the GPU encodes about 25% more all the time at x4;
  - the plugin would have to write the file itself, so no Hybrid MP4;
  - joining two streams depends on each encoder's settings;
  - frames would take OBS's GPU road, and with it OBS's one-frame shift (`docs/architecture.md`, "Timing"): plan B would have to compensate it, and the sync check of the integration test would show whether it does.

**Consequences.**
- In 4K30, 373 MB/s go from the card to memory and 373 MB/s back, and each kept frame is copied three times in memory. The download is the same OBS already does for every x264 recording.
- Measured on the reference PC on 2026-10-16, 4K30 for 5 minutes, against OBS's own recording with a GPU encoder: 0.42 CPU cores more; 286 MB more RAM while recording, of which OBS keeps 75 MB (its own memory feed) until it closes; about 130 MB less memory on the card; 5 W more on the card; no lost frames (`docs/performance.md`, `docs/test-reports/2026-10-16-recording-cost.md`).
- The cost is stated plainly to users, in the README and in `docs/performance.md` (REQ-018).
- On OBS's graphics thread, steps (1) and (2) take 3 to 8 ms per 4K frame, which on a busy computer makes the preview less smooth while the plugin records (the recording is not affected; measured on 2026-10-17, `docs/performance.md`, section 9). A road that copies the frame off that thread (road C, section 10) is studied as an experiment.
- REQ-014 holds with the test scene. If a real scene shows lost frames because of this road, plan B is implemented behind the same dock.
- On this path each frame keeps its own time, as in OBS's own x264 recordings. OBS's GPU path labels each frame with the time of the one before, so the plugin's picture sits about one frame later than in OBS's own GPU-encoder recordings (measured 2026-10-16; `docs/architecture.md`, "Timing").

---

## ADR-004. Name: Timelapse Switch

*Date: 2026-10-15. Status: accepted.*

**Context.** The OBS Project's resource policy does not allow "OBS" in the names or logos of third-party tools, and recommends against the `obs-` prefix in new repository names.

**Decision.** The plugin is called **Timelapse Switch**: it says what it does. Repository `miguetronic/timelapse-switch`.

**Consequences.** "Plugin for OBS Studio" appears only as a description, never as part of the name or the logo.

---

## ADR-005. Written with AI assistance, disclosed openly

*Date: 2026-10-15. Status: accepted.*

**Context.** Most of the code is written by an AI coding assistant (Claude, by Anthropic) and reviewed by the maintainer.

**Decision.** The README says so plainly. The same rules and checks (`docs/coding-rules.md`, `tools/check.sh`) apply to every line, whoever writes it. Releases are published on GitHub.

**Consequences.** The OBS forum's resources section does not accept resources written largely with AI tools, so the plugin is not listed there.

---

## ADR-006. C17 core, C++17 and Qt for the dock, OBS 31.1 or newer

*Date: 2026-10-15. Status: accepted.*

**Context.** OBS itself is written in C (libobs) and C++ with Qt (the interface). The official plugin template builds against OBS 31.1.1.

**Decision.** The core logic is C17, like libobs, and the most analyzable language for rules such as P10. The dock is C++17 with Qt 6, which OBS docks require. The minimum supported OBS version is 31.1.

**Consequences.** Two languages, with the rules adapted for each in `docs/coding-rules.md`. Only the public OBS API is used, never `obs-internal.h`.

---

## ADR-007. Coding rules: "The Power of Ten", adapted

*Date: 2026-10-15. Status: accepted.*

**Context.** A plugin runs inside OBS: if it crashes or freezes, OBS and the user's recording go with it. The maintainer asked for practices worthy of industrial audit.

**Decision.** Adopt G. J. Holzmann's ten rules for safety critical code (NASA/JPL), adapted to a plugin, with machine checks wherever possible (`tools/check.sh`: strict warnings, clang-tidy, the GCC analyzer and text checks).

**Consequences.** Some OBS idioms are not used in our code (`goto` cleanup, macros with variable arguments). Exceptions are allowed only with a written reason (`docs/coding-rules.md`, Part C).

---

## ADR-008. Conventional Commits and release-please

*Date: 2026-10-16. Status: accepted; release-please is switched on when the repository is on GitHub.*

**Context.** Version numbers and the changelog should follow from the work done, without manual bookkeeping, and nothing should reach users without the maintainer's review.

**Decision.** Every commit follows Conventional Commits, checked by a Git hook and by `tools/check.sh`. Releases are automated with release-please: it keeps a release pull request with the next version and changelog, and publishes when the maintainer merges it. The first public release is 1.0.0.

**Alternatives.**
- *semantic-release.* It publishes as soon as a qualifying commit reaches `main`, and it updates files by pushing a commit to `main` from CI, outside review. Updating `buildspec.json` would also need extra plugins and a script. The release pull request of release-please gives a preview and keeps every change to `main` reviewed.

**Consequences.** The first line of each commit is written for users, because it becomes the changelog. Tags have no "v" prefix, as the OBS template's build workflow expects.

---

## ADR-009. Release security: checksums and SBOM, no per-release vulnerability report

*Date: 2026-10-16. Status: accepted.*

**Context.** Professional releases should let users verify what they download and what it contains, without adding work that brings no information.

**Decision.** Every release includes SHA-256 checksums (from the OBS template) and an SBOM in CycloneDX 1.6, generated from `buildspec.json` by `tools/make-sbom.sh`. Security reports go through GitHub's private reporting (`SECURITY.md`).

**Alternatives.**
- *A bill of vulnerabilities (BOV) with every release.* The plugin ships only its own code, so it would almost always say "none known". If a vulnerability in OBS or Qt ever affects users, a statement is published at that moment, in the same CycloneDX format.

**Consequences.** Kept deliberately small. Build provenance attestation and automatic vulnerability scanning can be added later if the project grows.


---

## ADR-010. User documentation site on GitHub Pages

*Date: 2026-10-16. Status: accepted; the site is published together with version 1.0.0.*

**Context.** Every error message in the dock has a "Learn more" link (`docs/design/ux-spec.md`, UX-085), and users need an install and usage guide in English and Spanish. Those links are built into the plugin, so their address must never change.

**Decision.** The user documentation lives at `https://miguetronic.github.io/timelapse-switch/`, served by GitHub Pages. Error pages follow the pattern `https://miguetronic.github.io/timelapse-switch/errors/TSW-Exxx`. No cookies, no analytics.

The site is built with **Astro** and its documentation theme **Starlight** (amended on 2026-10-16), and GitHub Actions publishes it to GitHub Pages with Astro's official action. Pages are written in Markdown; Astro turns them into static HTML and CSS. Visitors get almost no JavaScript: only the search box, the light and dark switch, and small interactive "islands" where a page needs one (for example a demo of the dock), never a framework running the whole page. Starlight brings what a documentation site needs: English and Spanish, search that runs in the browser with no outside service (Pagefind), accessible navigation, and light and dark themes.

```
  Markdown pages (English, Spanish)                                   GitHub Pages
  + Starlight theme   --> Astro (GitHub Actions) --> static HTML, CSS --> visitors
  + islands (demo)                                   and a little JS
```

**Alternatives.**
- *A custom domain from the start.* It is not needed: if one is added later, GitHub redirects the `github.io` addresses to it, so links in installed versions keep working.
- *Plain HTML written by hand.* Nothing to install, but about 45 pages in two languages would repeat the same header, menu and footer, and sooner or later one copy would differ.
- *Zola or Hugo* (one program each, written in Rust and Go). Fewer moving parts, but search, components and the documentation layout would have to be built by hand.
- *Jekyll*, built into GitHub Pages: slow, ageing, and it needs Ruby to preview the site locally.
- *Docusaurus*: sends React to every visitor and is built for much larger documentation.
- *WordPress*: needs a server and a database, constant updates, and keeps the content away from the code it documents.

**Consequences.**
- The site is published with 1.0.0, when there is a working plugin and a real demo to show.
- Building it needs Node.js and npm packages. They run only in GitHub Actions and on the maintainer's machine, never reach visitors and are not part of the plugin, so they do not enter its SBOM. They are pinned with a lock file and watched by Dependabot.
- By default Astro turns file names into lowercase addresses. The error pages must keep the exact pattern above (or the plugin's link changes before 1.0.0), and a check opens every error link the plugin builds, in both languages.

---

## ADR-011. Pause as a section of the plugin's own timeline

*Date: 2026-10-17. Status: accepted (REQ-019).*

**Context.** Users pause recordings, sometimes many times in a row, both in real time and in timelapse, and may want to resume in a different mode. A pause must never freeze OBS, lose sync or damage the file.

**Decision.** A pause is one more section of the plugin's timeline, in which the final video does not move (`tsw_timeline_pause`). Frames and audio of a paused section never pass the gates, so nothing reaches the encoders; resuming appends a normal section, at the mode and speed chosen while paused. The output and the encoders never know a pause happened.

```
  OBS time:     0s ......... 10s ............ 25s ......... 30s
  section:      x1           PAUSED            x1
  final video:  0s ......... 10s              10s ......... 15s     (the pause takes no time)
```

Details that keep it robust:
- A pause is refused unless one more section stays free after it, so its resume always fits (the timeline holds 4096 sections: about 2000 pauses per recording).
- A stop while paused first resumes at the very moment of the stop. Emptying the encoder needs frames and audio flowing, because OBS's file writer interleaves them; with the audio held back by the pause the file was never finished (found by the "pauses" integration scenario before release).
- No chapter for a pause; resuming at another speed than before adds the chapter of that speed.
- A second press within 400 ms of the previous pause or resume is a double press (UX-046), with its own clock: a pause right after a speed switch is a real wish.

**Alternatives.**
- *OBS's own output pause* (`obs_output_pause`). Rejected: it compares exact timestamps, and with frames that arrive at the irregular pace of timelapse a skipped frame can leave the recording paused for ever (the same reason ADR-002 rejected pause-based timelapse); and its timestamps would fight the plugin's own timeline.
- *Timelapse at a very high speed instead of a pause.* Rejected: it still records, slowly, and it needs a chapter.

**Consequences.**
- Verified by unit tests (timeline, session, a 10-minute simulation with 410 pauses), and in a real OBS by the integration scenarios `pause` and `pauses` (40 quick pauses, each choosing how to resume, ending with Stop while paused): every pause is left out of the file exactly, sync holds, nothing is lost.


---

## ADR-012. Road C: the plugin copies the frame itself, off OBS's graphics thread

*Date: 2026-10-17. Status: experimental, built and measured; road B (ADR-003) stays the default until road C is tried in a real session.*

**Context.** On road B (ADR-003), OBS waits for each frame to come down from the card and copies it into its memory feed on its graphics thread, the thread that then draws the preview: 2.8 ms per 4K frame as a median, 5.9 ms for the worst 1 %, up to 13 ms, measured on 2026-10-17 with a phone camera and a busy computer. On a busy computer the preview then looks less smooth while the plugin records (`docs/performance.md`, section 9). The frame is then copied a second time, into the plugin's feed, and in timelapse OBS copies every frame even though most are not kept.

**Decision.** Where it is supported, the plugin takes the frame from the card itself:

```
  graphics thread, every frame (OBS's "rendered" callback):
     scene drawn  ->  convert to the output format, with OBS's own shader  ->  ask the card to copy it
                      into a staging slot (it does not wait)  ->  hand the slot of the previous frame,
                      now copied, to our copier thread  ->  OBS draws the preview

  our copier thread:
     the frame, straight from the slot into the plugin's feed (one copy, only the kept frames)
```

- **Same picture.** The conversion uses OBS's own `format_conversion.effect`, with the same technique, color matrix and parameters OBS uses for its memory feed; the input is OBS's main texture, as it is for OBS. A verification mode records through both roads at once and compares a hash of every whole frame: road C is adopted only if every frame is identical.
- **Same timing.** Each frame keeps the time OBS drew it at, and covers as many intervals as OBS's own feed would give it (a late draw covers more); repeats are handed to the recorder exactly as OBS's feed hands them (`src/core/stage-ring.h`).
- **Never a wait on the graphics thread.** Four staging slots, used in turn. If every slot is busy because the copier is behind, the frame is not staged, it is counted, and the frame before it covers its interval: a repeated picture, as when OBS's own feed is full.
- **OBS's rendering state is left as it was:** render target, viewport, projection and blending are saved and restored around the plugin's work.
- **When.** Only for the main canvas, unscaled output (base resolution equal to output resolution) and the SDR formats NV12, I420 and I444, and at first only on Linux, where it is tested. Anywhere else, road B, and the log says which road is used and why.

**Alternatives.**
- *Road B as it is* (ADR-003). It stays, as the fallback.
- *Plan B of ADR-003, GPU only with two encoders.* Its costs are unchanged (fixed speed per recording, waits for keyframes, no Hybrid MP4).
- *Mapping on the graphics thread and copying there.* No gain: the copy is the cost.

**Consequences.**
- Measured on 2026-10-17, 4K30 (`docs/test-reports/2026-10-17-road-c.md`): OBS's graphics thread took 0.80 ms per frame as a median on road C, against 3.31 ms on road B and 0.66 ms for OBS's own recording (worst 1 %: 1.57, 5.29 and 1.15 ms); road C's own step, 0.22 ms. Against road B, 0.37 CPU cores instead of 0.46 and 71 MB less RAM, all given back when the file is saved. In the verification mode every frame was identical to road B's, and every integration scenario passes on road C.
- Frames reach the plugin about one frame later than on road B; the file is not affected, because every frame and audio block carries its own time.
- More card memory: four staging slots and the conversion planes (about 62 MB in 4K NV12; measured: 47 MB more in RAM (GTT) and 13 MB more video memory than road B).
- The checks before adoption: the unit tests of the slot ring, the verification mode (every frame identical), and every integration scenario (sync, rhythm, pauses, totals, lost frames, leaks) on road C, then the cost measured in 4K like ADR-003.
