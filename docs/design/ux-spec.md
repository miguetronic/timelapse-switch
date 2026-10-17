# Timelapse Switch: UX and Installation Specification

A plugin for OBS Studio by **Miguetronic** (Miguel Parra, github.com/miguetronic).
Planned repository: `github.com/miguetronic/timelapse-switch` · License: GPL-2.0-or-later · Minimum OBS: 31.1

| Field | Value |
|---|---|
| Version | 0.2, draft for the author's review |
| Date | 2026-10-16 |
| Status | Design. How the dock looks (theme tokens, type, spacing, three visual directions) is in `docs/design/visual-design.md`. |
| Readers | The author, future contributors, testers and translators |
| Standards | Inspired by ISO 9241-110:2020 and ISO 9241-210:2019. This is not a certification claim. |

**How to read this document**

- **IDs.** `UX-xxx` covers the interface, errors, accessibility and languages. `INS-xxx` covers installation. Numbers are grouped by area and the gaps are reserved, so new items never force a renumbering. Code, tests and issues should quote these IDs.
- **Keywords.** MUST, SHOULD and MAY follow RFC 2119: MUST is a release blocker, SHOULD needs a written reason to skip, MAY is optional.
- **Principle tags.** Each UX requirement names the ISO 9241-110 interaction principle that justifies it: `[TASK]` suitability for the user's tasks, `[SELF]` self-descriptiveness, `[EXPECT]` conformity with user expectations, `[LEARN]` learnability, `[CONTROL]` controllability, `[ROBUST]` use error robustness, `[ENGAGE]` user engagement.
- **Mockups** are plain ASCII so they align in any editor. Legend: `(o)` record dot, `>` real time (play), `>>` timelapse (fast forward), `[]` stop, `(i)` info, `(!)` warning, `(X)` error, `[*]` settings, `[?]` help, `[*x4*]` selected chip. The real UI uses monochrome SVG icons, never emoji (emoji render differently on each OS).
- **Sources** are cited as `[S#]` and listed in section 8. Every one was opened during this research.

**Mini glossary**

| Term | Meaning |
|---|---|
| TS recording | The file made by this plugin's own recorder (the dock's Record button). |
| OBS recording | The usual file from OBS's own Start Recording button. The plugin never changes it. |
| Section | A stretch of a TS recording at one speed (real time, or timelapse at one speed). |
| Recorded time | Clock time since the TS recording started ("how long have I been working"). |
| Final length | How long the resulting video lasts ("how long my viewers will watch"). |

---

## 1. Summary of principles

1. **One glance, one truth.** The state banner always answers "Am I recording, and at what speed?" with words, an icon and a shape, readable from across the room, in every OBS theme and without relying on color. It carries two signals that never merge: a tally (record dot and REC) for "am I recording?" and a mode line (REAL TIME, or TIMELAPSE and the speed) for "at what speed?".
2. **State is a noun, buttons are verbs.** The banner says what is happening ("TIMELAPSE x4"). Buttons say what will happen if pressed ("Back to real time"). This removes the classic toggle doubt: "does this button show the current state or the next one?" [S52]
3. **Two recorders, never confused.** Timelapse Switch is a second recorder next to OBS's own. It has its own words ("Record with Timelapse Switch", "Stop and save"), its own file suffix and a reminder that stays until the user has learned the difference.
4. **Hands busy, eyes elsewhere.** Every action works with hotkeys, a foot pedal or a macro pad. Nothing important needs a mouse or a pop-up during a session. Stopping by hotkey needs a short hold, so a bumped key never ends a two-hour session.
5. **Never lose the take.** If something fails, the plugin stops cleanly, keeps the file, says up to which minute it is safe, and explains what happened, why, and what to do, with a `TSW-Exxx` code for support.
6. **Native to OBS.** The dock inherits the OBS theme, style, font size and density. No hardcoded colors: every color is mixed at runtime from the theme and adjusted in lightness until it stands out from what it sits on. It behaves like OBS where OBS already set an expectation (time format, wording patterns, hotkeys screen).
7. **Minimal dock, deep settings.** The dock is for the session (record, switch, speed, stop). Everything that is configured once lives in a settings dialog.
8. **Warm and honest.** A little delight (time saved, ready-made YouTube chapters), no gimmicks, plain language, and full transparency about how the plugin was built.

**From principles to decisions (ISO 9241-110:2020).** The seven principle names follow the 2020 edition [S55]. The ISO pages themselves were not readable (HTTP 403), so the mapping below uses the principles as design lenses, not as quoted requirements.

| ISO 9241-110 principle | What it means for Timelapse Switch | Main decisions |
|---|---|---|
| Suitability for the user's tasks | Fit a real creative session: talk, then a long quiet stretch, then finishing touches. | Live speed switching, silence by default in timelapse, final-length counter, ready-made chapters (UX-005, UX-042, UX-071). |
| Self-descriptiveness | The state and the next action are obvious at any moment. | Banner plus verb buttons, words next to every icon, output summary (UX-003, UX-004, UX-031). |
| Conformity with user expectations | Behave like OBS and like media players. | OBS time format, OBS hotkeys screen, the universal fast-forward symbol, theme-native look (UX-006, UX-050, UX-016). |
| Learnability | First success in minutes, without a manual. | Three-step welcome card, suggested hotkeys, a docs page per error code (UX-030, UX-051, UX-085). |
| Controllability | The user sets the pace and can change their mind. | Change speed any time, pre-select the starting mode, nothing modal mid-session (UX-009, UX-010, UX-013). |
| Use error robustness | Prevent slips, recover from failures. | Hold-to-stop, repeat-press filter, preflight checks, never lose the take (UX-052, UX-046, UX-083). |
| User engagement | Motivating and trustworthy to come back to. | Session summary with time saved, speed map, polished native look (UX-070, UX-072). |

---

## 2. Personas and key scenarios

### 2.1 Context of use

The typical setting is a workbench, a desk, a kitchen or an easel. A camera (often a phone used as a webcam) films the work from above and feeds OBS. The monitor is 1 to 2 m away, often to the side. Hands are busy and often dirty (paint, glue, flour, gloves), so touching the mouse is a nuisance. Attention is on the work; the screen gets short glances. Sessions last 30 to 120 minutes, with a talking intro, a long quiet stretch (drying, polishing, simmering, compiling) and a talking finish.

Today the slow stretch is sped up afterwards in an editor, or by re-encoding the whole file with a tool such as FFmpeg, which can take as long as the recording itself. OBS users have asked for exactly this switch for years: a tutorial recorder in 2023 [S16], a travel-footage recorder in 2023 [S17], and a 2015 request that got the answer "This is why there is editing software" [S18].

### 2.2 Personas

| | Persona | Setup | Needs most | Watch out for |
|---|---|---|---|---|
| **P1 (primary)** | **Ana**, 34, jewelry maker with a small YouTube channel. Not technical. | Windows, phone overhead as webcam, OBS for recording only, 45 to 90 min sessions. | Speed up the polishing without touching the mouse; know the state from a glance at 1.5 m; upload right after stopping. | Presses OBS's own Start Recording out of habit; forgets whether timelapse is on. |
| P2 | **Diego**, 41, acrylic painter who streams on Twitch. | Windows, NVENC, streams in real time while recording. | Stream at normal speed and get a ready YouTube version from the same session. | Double encoder load; two files in one folder. |
| P3 | **Mei**, 27, miniature model maker. | macOS (Apple Silicon), gloves on, airbrush noise. | Big state, hands-free switching, silence during noisy drying or airbrush parts. | Gatekeeper blocks the install; background hotkeys need permission. |
| P4 | **Sam**, 30, live coder. | Linux, Wayland session, OBS Flatpak. | Fast-forward long builds and test runs. | Hotkeys only work while OBS is focused on Wayland [S20]; installs the wrong package type for a Flatpak OBS. |
| P5 | **Rosa**, 55, home cook with a recipe channel. | Laptop 2 to 3 m away, wet hands. | Very large state, foot pedal, optional sound cue. | Cannot read normal text from the stove. |

### 2.3 Key scenarios

| ID | Scenario | Success looks like |
|---|---|---|
| SC1 | First mixed-speed recording: talk, speed up the slow stage at x4, finish in real time. | One file with the slow stage already 4 times faster and the voice intact. No editor opened. |
| SC2 | Change speed mid-timelapse after realizing the slow stage is long (x4 to x8). | One key or one click, effective on the next frame; the final-length counter shows the gain. |
| SC3 | Hands busy: switch with a pedal without looking. | The banner color block and shape change are visible in peripheral vision; an optional cue confirms. |
| SC4 | Disk fills up 40 minutes in. | Recording stops cleanly, the file is playable, and the dock says up to which minute it is safe and what to do. |
| SC5 | Stream in real time and record mixed speed at once (P2). | Both run; the dock warns once about extra load and two files. |
| SC6 | Upload right after stopping. | The summary card offers "Open folder" and "Copy chapters" in YouTube format. |
| SC7 | Wayland or macOS hotkeys do not fire in the background (P3, P4). | The dock explained the limitation on first run and how to work around it. |

### 2.4 User journey

```
 STAGE        WHAT HAPPENS                         RISK                          OUR ANSWER

 1 DISCOVER   Sees a craft video and asks          "Is this AI junk? Does it     Real footage, honest README,
              "how did you speed up the slow part?" really work?"                real demo
 2 DECIDE     Opens the README.                    "Two record buttons? Hmm."    README explains why
 3 INSTALL    One recommended file per OS.         Blocked by SmartScreen or     "Which file?" table, signing,
                                                   Gatekeeper; wrong Linux file  step-by-step help (INS-003/007/009)
 4 FIRST RUN  Dock appears with a welcome card.    Presses OBS's own Record      Separate-file hint, cross-warnings
                                                                                 (UX-021, UX-022)
 5 SESSION    Talks, x4 on the slow part, ends.      Forgets the state; bumps      Glanceable banner, hold-to-stop,
                                                   the Stop key                  repeat filter (UX-003/052/046)
 6 AFTER      Summary card, copies chapters,       Chapter rules; no audio       YouTube-ready chapters, silent
              uploads.                             track for Instagram           track always there (UX-071/042)
 7 RETURN     Records the next session the         An update breaks it           Opt-in update notice, tested
              same way, tells a friend.                                          OBS matrix (UX-034, INS-001)
```

### 2.5 Human-centred process (ISO 9241-210:2019)

ISO 9241-210 asks for six things, summarized here from NIST's page [S56]: an explicit understanding of users, tasks and environments; users involved throughout; design driven and refined by user-centred evaluation; an iterative process; attention to the whole user experience; multidisciplinary skills. Applied to a one-person open source project, that becomes:

1. **Understand the context:** 3 to 5 short interviews with creators who record long sessions.
2. **Specify:** this document. Every requirement has an ID.
3. **Design:** these mockups first, then a clickable Qt prototype of the dock before the recording engine is finished.
4. **Evaluate early and often** against measurable targets:

| Target | Measure | Pass |
|---|---|---|
| T1 First success | First-time testers go from download to a finished mixed-speed file. | 4 of 5 testers in 10 minutes or less (Windows and macOS). |
| T2 Glance test | Show the dock for 1 second at 1.5 m; the tester says the state. | 95 % correct, in all 7 theme styles and in grayscale. |
| T3 Two-button confusion | After onboarding, ask "record this with timelapse". | At most 1 of 5 testers starts OBS's own recording. |
| T4 Error comprehension | Inject disk-full and encoder-missing failures. | 4 of 5 can say what happened and what to do next. |
| T5 Language fit | Spanish UI at 260 and 220 px wide docks, measured against the visible area (the scroll area's viewport), not the dock's own width. | No truncated or clipped label, no hidden verb. |

5. **Iterate:** fix, re-test the failed targets, then release. Testers are recruited among early adopters, on all three operating systems.
6. **Whole experience:** from the README to the YouTube upload, not just the dock (section 5).

---

## 3. Dock design

### 3.1 Mental model: a second recorder with a fast-forward pedal

Think of OBS's Start Recording as the main tape deck. Timelapse Switch is a second deck sitting next to it. Both see the same picture, but only the second deck has a fast-forward pedal, and whatever it records while the pedal is down goes onto the tape already fast-forwarded.

Why a separate recorder instead of changing OBS's own? Speeding up live means skipping frames before they are encoded, so the recording needs its own encoder. Many OBS setups share one encoder between streaming and recording, and a stream must stay in real time. An experienced OBS forum member pointed out exactly this when the feature was requested in 2023 [S16]. Aitum Vertical sets the precedent of a plugin with its own record controls in its own dock [S23].

### 3.2 Anatomy: six zones that never move

The dock is a vertical stack of six zones in four bands: Status (A and B), Action (C), Below (D and E, or a card) and Context (F). Their positions never change; only their contents do. The eye learns where to look, like the dials of a car.

| Zone | Name | Contents by state |
|---|---|---|
| A | State banner | Two lines. A tally: READY, (o) REC, PAUSED, STARTING, SAVING, SAVED or ERROR. A hero: "Starts in real time", REAL TIME, TIMELAPSE x4, "Resumes in real time" or "Resumes in timelapse x4" (paused), "Finishing the file", "File ready" or the error headline. Words, icon and frame style. |
| B | Counters | RECORDED and FINAL VIDEO side by side, and a status line: the current stretch while recording, or a warning summary. |
| C | Main slot | The biggest button, holding the most likely next action: Record (idle), Switch to timelapse or Back to real time (recording), "Resume in real time" or "Resume in timelapse x4" (paused), Record again (error, summary). |
| D | Speed row | Speed chips x2, x4, x8 and x16 (a custom value later). When idle, also the starting mode. When paused, one "Resume in" rail in the chips' place: Real time, x2, x4, x8 and x16. After a recording, the summary or error card takes zones D to F. |
| E | Stop slot | "Pause" (while paused, "Resume", in the same place) and "Stop and save", side by side, set apart by a gap or a rule. Shown while recording, paused and saving. |
| F | Context line | Separate-file hint, file name, audio note, saving note, and the details of a warning. |

**Fixed positions.** At a given width, language, font and density, zones A, B and C keep their pixel positions in every state, and E keeps its position in every recording state: A and B have fixed heights, the status line of B keeps its height even when empty, and cards and warning details open below C, never above it. How each zone looks, in every OBS theme, is specified in `docs/design/visual-design.md`.

Why does the main slot change from Record to the switch? Because those are the next actions in each phase, and both are safe to press by accident: an unwanted switch is undone with the same button. Stopping is the one action that cannot be undone (it splits the video into two files), so Stop lives in its own place and asks for a hold when triggered by hotkey (UX-052).

### 3.3 States

```
  IDLE ---Record---> STARTING ---ok---> REC: REAL TIME  <---switch--->  REC: TIMELAPSE xN
   ^                    |                    |      ^                      ^      |
   |                  failed                 |      |   Pause    Resume    |      |
   |                    |                    |      v   (in the mode shown) v      |
   |                    |                    |    +------- PAUSED ---------+      |
   |                    |                    |                 |                  |
   |                    |                   Stop              Stop               Stop
   |                    |                    +-----------------+------------------+
   |                    v                                      v
   |                  ERROR  <-------- failed ------------  SAVING  ---ok--->  SUMMARY
   |                    |                                                        |
   +----- dismiss ------+------------------------- dismiss ----------------------+
          (Record pressed in ERROR or SUMMARY goes straight to STARTING)
```

**Paused** (REQ-019): reached with Pause from real time or from timelapse, as often as wanted. Nothing reaches the file while paused, and the counters stand still. While paused, the user chooses the mode and speed to resume in (by default the ones before the pause); the banner and the main button always say which. Resume returns to recording in that mode; Stop saves the file as usual.

A failure during a recording (disk full, encoder error) triggers SAVING automatically, so the file is finalized before the ERROR state appears. Speed chips change N inside the timelapse state without leaving it. Warnings never change the state; they appear under the counters or in zone F.

### 3.4 Mockups by state

Mockups are about 300 px wide, a common dock width. Notes on the right are not part of the UI. They show the structure every visual direction shares; how it looks in each OBS theme (fills, frames, sizes, spacing) is in `docs/design/visual-design.md`.

**Idle (ready).** Neutral block with READY and how the recording will start, dimmed counters, Record in the main slot, starting mode and speed pre-selectable, separate-file hint.

```
+-- Timelapse Switch ------------------ [?][*] +
| +------------------------------------------+ |
| | READY                                    | |   A  tally
| | >  Starts in real time                   | |      quiet hero (no recording look)
| +------------------------------------------+ |
|  RECORDED              FINAL VIDEO           |   B  readout (dimmed)
|  0:00:00               0:00:00               |
|                                              |      status line, height kept
|  +----------------------------------------+  |
|  |  (o)  Record with Timelapse Switch     |  |   C  main slot: Record
|  +----------------------------------------+  |
|  START IN                                    |
|  [    >  Real time   |    >> Timelapse   ]   |   D  start mode: two segments
|  TIMELAPSE SPEED                             |
|  [    x2   |   (x4)  |    x8   |   x16   ]   |      speed rail, armed (outlined)
|                                              |
|  (i) Separate file. OBS's Start Recording    |   F  context line (E is hidden)
|      still records at normal speed.          |
+----------------------------------------------+
```

**Recording, real time.** Outlined block with the tally (record dot and REC) and the words REAL TIME. Both counters advance together. The main slot offers the switch, in the place where Record was.

```
+-- Timelapse Switch ------------------ [?][*] +
| +==========================================+ |
| | (o) REC                                  | |   A  tally: record dot + REC
| | >  REAL TIME                             | |      hero; outlined frame
| +==========================================+ |
|  RECORDED              FINAL VIDEO           |   B  equal while in real time
|  0:12:03               0:12:03               |
|  This stretch 12:03                          |      status line: current stretch
|  +========================================+  |
|  ||  >>  Switch to timelapse  x4         ||  |   C  same place as Record
|  +========================================+  |
|  NEXT SPEED                                  |   D  pre-selects the next speed
|  [    x2   |   (x4)  |    x8   |   x16   ]   |      armed: outlined
|  ------------------------------------------  |
|  [ []  Stop and save                      ]  |   E  full width, set apart
|  File: 2026-10-15 19-30-00 tswitch.mp4       |   F  file name
+----------------------------------------------+
```

**Recording, timelapse.** The block becomes a filled area in the theme's accent color, adjusted in lightness until it stands out from the dock, with a frame and fast-forward chevrons. The tally stays. Even without color, the shape changes from outlined to filled and the words change. The final-length counter now advances 4 times slower, and the status line shows the current stretch.

```
+-- Timelapse Switch ------------------ [?][*] +
| ############################################ |
| # (o) REC                         >  >  >  # |   A  filled with the theme accent (adjusted
| # >> TIMELAPSE  x4                >  >  >  # |      to 3:1), frame, chevrons; tally stays
| ############################################ |
|  RECORDED              FINAL VIDEO           |   B  final grows 4x slower
|  0:31:40               0:16:58               |
|  This stretch x4   19:37 -> 4:54             |      status line: current stretch
|  +========================================+  |
|  ||  >   Back to real time               ||  |   C  verb + destination
|  +========================================+  |
|  SPEED NOW                                   |   D  changes speed on the next frame
|  [    x2   |   *x4*  |    x8   |   x16   ]   |      engaged: filled
|  ------------------------------------------  |
|  [ []  Stop and save                      ]  |   E  same place as before
|  (i) Audio: silence during timelapse         |   F  what happens to audio
+----------------------------------------------+
```

**Paused** (REQ-019). The block keeps no record dot and no REC: a PAUSED tally with two bars, a solid pause sign at the end of the tally line, and a frame held between two thick side bars, so it reads as paused without color. The hero, the main button and the "Resume in" rail all say the mode it will resume in, chosen right here on one rail that takes the speed rail's place and height, so nothing below it moves; Pause has become Resume in the same place. The counters stand still and the status line counts the pause.

```
+-- Timelapse Switch ------------------ [?][*] +
| +==========================================+ |
| |# (||) PAUSED                     (||)   #| |   A  tally: two bars, no dot; pause sign
| |# >> Resumes in timelapse x4             #| |      quiet hero: the mode it resumes in
| |#                                        #| |      side bars: the pause shape
| +==========================================+ |
|  RECORDED              FINAL VIDEO           |   B  standing still
|  0:31:40               0:16:58               |
|  Paused for 0:01:12                          |      status line: how long
|  +========================================+  |
|  ||  >>  Resume in timelapse x4          ||  |   C  names the mode; armed (outlined)
|  +========================================+  |
|  RESUME IN                                   |   D  one rail, as tall as the speed
|  [ >  Real time  | x2 | (x4) | x8 | x16  ]   |      rail: real time or a speed; armed
|                                              |
|  [ >>  Resume      ]  [ []  Stop and save ]  |   E  Resume where Pause was
|  (i) Nothing is recorded until you resume.   |   F
+----------------------------------------------+
```

**Error.** The block shows the icon, the word ERROR and a plain headline. The card below the main slot gives reassurance first, then why, then what to do, with one-click fixes; the code is small, at the end. The main slot stays where it was, ready for a new recording.

```
+-- Timelapse Switch ------------------ [?][*] +
| +::::::::::::::::::::::::::::::::::::::::::+ |
| : (X) ERROR                                : |   A  icon + word, dashed danger frame
| : Recording stopped: disk full             : |      plain headline
| +::::::::::::::::::::::::::::::::::::::::::+ |
|  RECORDED              FINAL VIDEO           |   B  what the take reached
|  0:42:10               0:18:16               |
|                                              |
|  +----------------------------------------+  |
|  |  (o)  Record with Timelapse Switch     |  |   C  ready to try again, same place
|  +----------------------------------------+  |
|  +----------------------------------------+  |
|  | Everything up to 0:42:10 is saved    x |  |   card below C: reassurance first
|  | (0:18:16 of final video).              |  |
|  | Why: drive D:\ has 0 MB free.          |  |      why, with the real value
|  | What to do: free some space or choose  |  |      what to do
|  | another folder, then record again.     |  |
|  | [ Open folder ]  [ Change folder... ]  |  |      one-click fixes
|  | Copy details   Learn more    TSW-E101  |  |      links; the code small, last
|  +----------------------------------------+  |
+----------------------------------------------+
```

**Warning during a recording.** The state stays in the block. The warning's summary takes the status line under the counters, and its details appear below Stop, where they can be dismissed. Nothing above Stop moves.

```
+-- Timelapse Switch ------------------ [?][*] +
| ############################################ |
| # (o) REC                         >  >  >  # |   state stays visible
| # >> TIMELAPSE  x8                >  >  >  # |
| ############################################ |
|  RECORDED              FINAL VIDEO           |
|  1:05:12               0:24:30               |
|  (!) Warning: 4% of frames lost              |   summary takes the status line
|  +========================================+  |
|  ||  >   Back to real time               ||  |   C does not move
|  +========================================+  |
|  SPEED NOW                                   |
|  [    x2   |    x4   |   *x8*  |   x16   ]   |
|  ------------------------------------------  |
|  [ []  Stop and save                      ]  |
|  +----------------------------------------+  |
|  | (!) WARNING  4% of frames lost in the  |  |   details below Stop, dismissible
|  | last minute. The computer is busy.     |  |
|  | Details   TSW-E201                   x |  |
|  +----------------------------------------+  |
+----------------------------------------------+
```

**Compact layout (when the visible area of the dock is narrower than 260 px; it returns at 276 px).** Same zones and order, same verbs: labels wrap instead of getting shorter. Full labels stay in accessible names.

```
+-- Timelapse Sw... ------ [*] +
| ############################ |   same zones, same order
| # (o) REC               x4 # |   the speed may move to the tally line
| # >> TIMELAPSE             # |
| ############################ |
|  RECORDED    FINAL VIDEO     |
|  0:31:40     0:16:58         |
|  This stretch x4             |
|  19:37 -> 4:54               |
|  +========================+  |
|  ||  >  Back to real     ||  |   verbs stay: the label wraps to
|  ||     time             ||  |   two lines instead of becoming
|  +========================+  |   a noun
|  SPEED NOW                   |
|  [  x2 | *x4*|  x8 | x16 ]   |
|  --------------------------  |
|  [ []  Stop and save      ]  |   full names in accessible names
+------------------------------+
```

### 3.5 Labels, icons and accessible names

| Key (locale file) | English | Spanish | Icon | Accessible name (EN) |
|---|---|---|---|---|
| `Dock.Title` | Timelapse Switch | Timelapse Switch | plugin icon | Timelapse Switch |
| `Action.Record` | Record with Timelapse Switch | Grabar con Timelapse Switch | record dot | Start a Timelapse Switch recording |
| `Action.Stop` | Stop and save | Detener y guardar | square | Stop the Timelapse Switch recording and save the file |
| `Action.ToTimelapse` | Switch to timelapse x4 | Pasar a timelapse x4 | fast forward | Switch to timelapse, 4 times faster |
| `Action.ToRealTime` | Back to real time | Volver a tiempo real | play | Go back to real time |
| `Speed.Value` (chips) | x2 / x4 / x8 / x16 | x2 / x4 / x8 / x16 | none | Speed, 4 times faster, next timelapse speed (or: speed now) |
| `State.Tally.Ready` + `State.Hero.StartsRealTime` | READY / Starts in real time | LISTO / Empieza en tiempo real | play | Ready. Starts in real time. |
| `State.Tally.Rec` + `State.Hero.RealTime` | REC / REAL TIME | GRABANDO / TIEMPO REAL | record dot + play | Recording in real time |
| `State.Tally.Rec` + `State.Hero.Timelapse` | REC / TIMELAPSE x4 | GRABANDO / TIMELAPSE x4 | record dot + fast forward | Recording in timelapse, 4 times faster |
| `State.Tally.Starting` / `State.Tally.Saving` | STARTING / SAVING | INICIANDO / GUARDANDO | three dots | Starting / Saving the file |
| `Counter.Recorded.Caption` / `Counter.Final.Caption` | RECORDED / FINAL VIDEO | GRABADO / DURACIÓN FINAL | none | 12 minutes 3 seconds recorded, 12 minutes 3 seconds of final video |
| `Hint.Separate` | Separate file. OBS's Start Recording still records at normal speed. | Archivo aparte. «Iniciar Grabación» de OBS sigue grabando a velocidad normal. | info | (same text) |

The full list of keys, new ones included, is in `docs/design/visual-design.md`, section 3.3. There are no short labels for narrow docks: a label wraps to two lines instead of losing its verb (UX-004).

Icon rules: the fast-forward double triangle and the play triangle are the conventions every media player uses, so they need no learning `[EXPECT]`. The UI shows the multiplication sign (`×4`); this document writes `x4` for ASCII safety. Screen readers get "4 times faster", never "x four".

### 3.6 Sizes and glanceability

How big must the state be? A control-room guideline puts the optimal character height at 20 arcminutes of visual angle and the minimum at 16 arcminutes (citing MIL-PRF-87268D), which works out to character height = distance / 172 for optimal and distance / 215 for minimum [S50]. At 1.5 m that is 8.7 mm (optimal) and 7.0 mm (minimum).

| Glance size | Banner text (relative to OBS base font) | Cap height on a 24" 1080p screen | Comfortable up to | Readable up to |
|---|---|---|---|---|
| Normal (default) | about 2x | about 18 px, 5.0 mm | 0.85 m | 1.1 m |
| Large (setting) | about 3.5x | about 31 px, 8.7 mm | 1.5 m | 1.85 m |

On a 27" 1440p screen, Large needs about 37 px of cap height for the same 8.7 mm. The color block and the outlined-versus-filled shape of the banner are recognizable from much farther than the text, which is why the banner changes shape and not only words.

**Measured baseline (2026-10-16).** In every Yami style the first dock drew its banner at the base size (13 px, 1.0x), readable only to about 0.55 m: Yami's `QWidget { font-size }` rule overrides `QWidget::setFont`. Sizes are therefore applied through the dock's own style sheet, or by painting the text, from the base font read after the theme is applied (UX-105). Each visual direction sets its Normal target between 2x and 2.75x, fitted to the dock's width (`docs/design/visual-design.md`, 3.6).

### 3.7 Theme integration

What OBS does today (32.2.2 source, the latest release on 2026-08-14):

- Settings > Appearance offers a Theme (Yami or System), a Style for Yami (Default, Classic, Grey, Light, Acri, Rachni), a Font Size and a Density [S3, S7]. Themes are QSS with variables and images [S1]. In 32.2.2 the System theme is hidden unless it is already selected or Windows High Contrast is on, and maintainers call it unsupported (OBS issue #11498); the dock still supports it as best effort, mainly for High Contrast.
- Measured with the theme render harness (`tests/ui/obs-themes.sh`): the theme's highlight is only 1.03 to 2.49:1 against the dock, a checked button 1.12 to 1.89:1 against an unchecked one, and `text-danger` 1.90 to 2.93:1 on the dark styles. A theme's colors are therefore not state cues by themselves: the dock keeps their hue and adjusts their lightness until they reach 3:1 for shapes and 4.5:1 for text (`docs/design/visual-design.md`, 3.5).
- Theme files map their variables onto the Qt palette (`--palette_window`, `--palette_highlight: var(--primary)`, `--palette_buttonText`, and so on), so a plugin that reads `QPalette` inherits the theme [S2].
- Themes define global utility classes that any widget can opt into through its `class` property: `text-danger`, `text-warning`, `text-success`, `text-muted`, `text-large`, `text-heading`, `frame-notice`, plus icon classes such as `icon-gear` [S2]. OBS applies them with a helper that sets the property and forces a style refresh [S6].
- OBS's own Record button changes its text from "Start Recording" to "Stop Recording" and gets the `state-active` class, which paints it with the theme's primary color, not red. It shows "Stopping Recording..." while finalizing. The Pause button is icon-only with an accessible name and tooltip that swap between "Pause Recording" and "Unpause Recording" [S4]. The `state-active` style only targets OBS's own object names (`#recordButton` and friends) [S2].
- The status bar shows a red dot icon and `hh:mm:ss`; when paused, the icon alternates with a yellow pause icon every second and "(PAUSED)" is appended [S5].
- Accent colors vary a lot: Rachni's primary is a dusky rose, `rgb(145,76,103)`, close in hue to the danger red; Light's primary is a pale blue, `rgb(140,181,255)`, with black text [S3]. A design that says "red means real time, accent means timelapse" would fail in Rachni.
- Aitum Vertical paints its checked record button with a hardcoded `rgb(255,0,0)` and uses icon-only buttons with tooltips, while it does use OBS's icon classes for small buttons [S23]. We keep the good part and avoid the hardcoded color.

### 3.8 How the dock avoids confusion with OBS's own Record button

The risk goes both ways: pressing OBS's Start Recording and wondering why the switch does nothing, or running both recorders by accident. Seven guards, from strongest to lightest:

1. **Different words.** Never "Start Recording". The dock says "Record with Timelapse Switch" and "Stop and save" (UX-020).
2. **A reminder that fades out.** The separate-file hint stays until the user has finished three TS recordings, then shrinks to an info icon with the same tooltip (UX-021).
3. **Cross-awareness.** If OBS's own recording starts while the dock is idle, the dock says so and explains how to get timelapse (TSW-E301). If both run, it warns about double load and two files (TSW-E302) (UX-022, UX-023).
4. **No silent no-ops.** Pressing the switch while idle pre-selects the starting mode and the banner says "Starts in timelapse x4" (UX-024).
5. **Different files.** TS files end with a configurable suffix, `tswitch` by default (UX-025).
6. **Different names in the hotkeys screen.** Every action starts with "Timelapse Switch:" (UX-026).
7. **Different icon.** A record dot combined with fast-forward chevrons (UX-027).

### 3.9 First-run onboarding

On first start after installation, the dock appears (docked near the Controls dock when OBS allows it) and shows a welcome card inside the dock, below the main slot like every card (UX-002). No pop-up window: the card can be ignored and the dock still works.

```
+-- Timelapse Switch ------------------ [?][*] +
| +------------------------------------------+ |
| | READY                                    | |   the dock as always
| | >  Starts in real time                   | |
| +------------------------------------------+ |
|  RECORDED              FINAL VIDEO           |
|  0:00:00               0:00:00               |
|                                              |
|  +----------------------------------------+  |
|  |  (o)  Record with Timelapse Switch     |  |   C  in its usual place
|  +----------------------------------------+  |
|  +----------------------------------------+  |
|  | Welcome! 3 steps:                    x |  |   non-modal card, below C
|  | 1. Press (o) Record above. This is a   |  |
|  |    second recorder, separate from      |  |
|  |    OBS's Start Recording.              |  |
|  | 2. Slow part? Press >> Timelapse.      |  |
|  | 3. Press > Real time to go back.       |  |
|  |    Your file comes out sped up.        |  |
|  | Using your OBS recording settings:     |  |   self-description of output
|  |   Hybrid MP4, NVENC H.264, D:\Videos   |  |
|  |   Change...                            |  |
|  | [ ] Tell me about new versions         |  |   opt-in, unticked
|  |     (GitHub)                           |  |
|  | [ Set up hotkeys ]      [ Got it ]     |  |   two exits, both safe
|  +----------------------------------------+  |
+----------------------------------------------+
```

At the same time a quiet preflight check runs (folder writable, free space, encoder available, format supports chapters) and reports only what matters, as info lines. Platform notes appear once and only where relevant: on Linux Wayland, global hotkeys only work while OBS is focused [S20]; on macOS, background hotkeys need the Input Monitoring permission, which OBS itself requests in its "Review App Permissions" dialog [S7].

### 3.10 What goes in the dock and what goes in Settings

| Item | Dock | Settings |
|---|---|---|
| Record, Stop and save | yes | |
| Switch real time / timelapse | yes | |
| Speed chips | x2, x4, x8 + custom chip | custom value |
| Counters | yes | show "time saved" on/off |
| Starting mode | pre-select while idle | default starting mode |
| Messages | inline + history list | |
| Output (folder, format, encoder, file suffix) | summary only | yes |
| Audio during timelapse | short note while in timelapse | Silence (default) or sped-up audio |
| Chapter markers | | on/off, names |
| Hotkeys | "Set up hotkeys" link | helper + link to OBS Settings > Hotkeys |
| Glance size, sound cues, reduce motion, status bar indicator | | yes |
| Update notice | | opt-in |
| Docs, log, report a bug, version | `[?]` menu | About tab |

Settings opens from the gear in the dock title and from Tools > Timelapse Switch Settings... (`obs_frontend_add_tools_menu_item` [S8]).

```
+-- Timelapse Switch Settings ---------------- [x] +
| [Output] [Timelapse] [Feedback] [Hotkeys] [?]    |
| ------------------------------------------------ |
| Recording settings                               |
|  (*) Use my OBS recording settings               |
|      Hybrid MP4 . NVENC H.264 . D:\Videos        |
|      (i) OBS shares its stream encoder with      |
|      recordings. Timelapse Switch needs its      |
|      own, so it will use NVENC H.264 too.        |
|  ( ) Custom                                      |
|      Folder   [ D:\Videos          ][...]        |
|      Format   [ Hybrid MP4 (.mp4)     v ]        |
|      Encoder  [ NVIDIA NVENC H.264    v ]        |
|  File name ends with  [ tswitch ]                |
| ------------------------------------------------ |
|  Changes apply to the next recording.            |
|                         [ Cancel ]  [ OK ]       |
+--------------------------------------------------+
```

### 3.11 Hotkeys

Hotkeys use OBS's own frontend hotkey API, so they appear in Settings > Hotkeys, are saved with OBS and support OBS's "pair" behavior, where one key shared by two actions acts as a toggle [S7, S9].

| Action (as shown in OBS Settings > Hotkeys) | Type | Suggested key (keyboard) | Suggested key (pedal or macro pad) |
|---|---|---|---|
| Timelapse Switch: Switch to timelapse / Back to real time | pair (same key toggles) | Num 0 | F13 |
| Timelapse Switch: Speed x2 / x4 / x8 / custom | single | Num 1 / Num 2 / Num 3 / Num 4 | F14 to F17 |
| Timelapse Switch: Faster / Slower | single, two keys (a pair would make one key toggle between them, which is not useful here) | Num + / Num - | |
| Timelapse Switch: Add chapter marker | single | Num . | |
| Timelapse Switch: Record / Stop and save | pair, Stop needs a 1 s hold | Num Enter | F18 |
| Timelapse Switch: Pause or resume | single (one key pauses and resumes; no hold: a pause is undone by the same key) | Num * | F19 |

Rules behind the table:

- **No default bindings.** As with OBS's own recording actions on a fresh install, every hotkey starts unbound; this avoids clashing with whatever the user already has. The hotkey helper offers the suggested set with one click and checks for conflicts first (UX-051).
- **Keys may also reach the app in focus.** Pick keys no other app uses. F13 to F24 are ideal: cheap USB foot pedals and macro pads can send them and almost no application listens to them. macOS keyboards go up to F19.
- **Speed keys also start timelapse** (setting, on by default): pressing "x8" while in real time enters timelapse at x8. One key, one result, for busy hands.
- **Platform limits** are explained, not hidden: on Wayland, OBS hotkeys only fire while OBS is focused; there is an open OBS issue [S20], a draft OBS pull request to add portal-based global hotkeys [S21], and a third-party plugin that already implements the GlobalShortcuts portal [S22]. On macOS, OBS needs Input Monitoring to see keys in the background [S7].

### 3.12 Feedback: seeing and hearing the state

- **Visual.** Every switch updates the banner and counters in the same moment the recorder confirms it. Clicking and hotkeys give identical feedback: the button shows a press, the banner changes.
- **Status bar indicator (optional, on by default if feasible).** A small "TS >> x4 0:31:40" in OBS's status bar keeps the state visible when the dock is hidden or on another monitor. Feasibility must be verified, since the status bar is OBS's own widget.
- **Sound cues (optional, off by default).** Short earcons under 300 ms: rising two notes for entering timelapse, falling two notes for back to real time, one soft tone for start, two for saved, a low tone for errors. Important: a cue played on the speakers can be captured by the microphone or a Desktop Audio source. The "enter timelapse" cue lands in a silent section, so it is harmless; the others may be recorded. The setting says so and offers "Only when entering timelapse".
- **No blinking.** Unlike OBS's blinking pause icon, the dock never blinks. An optional chevron drift in the timelapse banner stops after 5 seconds and obeys "Reduce motion" [S46].
- **Clean audio joins.** Each switch uses a very short audio fade so there is no click at the boundary (engineering tunes the length).

### 3.13 Delight that stays professional

- **Session summary** after Stop: recorded versus final length, "You skipped 52:35 of waiting", a speed map strip, the file name, and buttons for Open folder, Play and Copy chapters.
- **Copy chapters** produces YouTube description timestamps on the final timeline. YouTube needs the first stamp at 00:00, at least three stamps and chapters of 10 seconds or more [S67]; if the session does not meet that, the dialog explains why and offers a merged version.
- **One warm line per session, never in errors.** "Nice session!" appears once in the summary and nowhere else.
- **An icon worth recognizing:** a record dot with fast-forward chevrons.

```
+-- Timelapse Switch ------------------ [?][*] +
| +------------------------------------------+ |
| | (v) SAVED                                | |   A  calm, neutral
| | File ready                               | |
| +------------------------------------------+ |
|  RECORDED              FINAL VIDEO           |   B  the last take
|  1:23:45               0:31:10               |
|                                              |
|  +----------------------------------------+  |
|  |  (o)  Record with Timelapse Switch     |  |   C  same place as always
|  +----------------------------------------+  |
|  +----------------------------------------+  |
|  | 2026-10-15 19-30-00 tswitch.mp4      x |  |   card below C
|  | You skipped 52:35 of waiting.          |  |      one light touch, once
|  | Nice session!                          |  |
|  | Speed map (recorded time):             |  |
|  | [>][>> x4 ///////][>][>> x8 ////][>]   |  |      sections at a glance; timelapse
|  | [ Open folder ] [ Play ]               |  |      hatched, never color alone
|  | [ Copy chapters ]                      |  |      next steps for upload
|  +----------------------------------------+  |
+----------------------------------------------+
```

### 3.14 Requirements: dock, states and theme

| ID | Requirement | Principle |
|---|---|---|
| UX-001 | The plugin MUST add exactly one dock titled "Timelapse Switch" with `obs_frontend_add_dock_by_id`, listed in the Docks menu [S8]. | [EXPECT] |
| UX-002 | The dock MUST keep zones A to F in a fixed order in every state; only zone contents change. At a given width, language, font and density, zones A, B and C MUST keep their pixel positions in every state, and E in every recording state: A and B have fixed heights, and cards and warning details open below C. | [SELF] [LEARN] [ROBUST] |
| UX-003 | The banner MUST carry two separate signals: a tally (record dot and REC, in both recording states and never otherwise) and a mode line (REAL TIME, or TIMELAPSE and the speed). It MUST show the state with words, an icon and a frame style: outlined for real time; filled for timelapse, with a 2 px frame and fast-forward chevrons; a dashed danger frame for error. Color MUST NOT be the only difference. Idle MUST NOT look like a recording (no dot, no REC, sentence case). | [SELF] |
| UX-004 | Buttons MUST be labelled with the action they perform, verb first; they MUST NOT display the current state. This holds at every width: a narrow dock wraps the label to two lines instead of shortening it to a noun such as "Timelapse x4", which is a state name. | [SELF] |
| UX-005 | While recording, the main slot (zone C) MUST hold the mode switch and be the largest control. | [TASK] |
| UX-006 | Counters MUST show recorded time and final length side by side as `h:mm:ss` (the OBS status bar format [S5]) with tabular digits, small captions above them (RECORDED, FINAL VIDEO) and digits larger than body text. Idle shows a dimmed 0:00:00. | [TASK] [EXPECT] |
| UX-007 | While recording, zone B SHOULD show the current stretch in its status line: in timelapse, real time elapsed and its length in the final video; in real time, its length. The line keeps its height in every state. | [TASK] |
| UX-008 | Speed chips MUST offer x2, x4, x8 and x16 (a custom value MAY replace or follow x16 once open question 1 is decided); default x4; exactly one chip selected (radio behavior). The selected chip MUST carry a cue that is not color (bold and a marker). A pre-selected speed (armed: idle and real time) MUST look outlined and the live speed (engaged: timelapse) filled. | [TASK] [CONTROL] |
| UX-009 | A speed change during timelapse MUST apply from the next frame; during real time it MUST pre-select the next timelapse speed. | [CONTROL] |
| UX-010 | While idle, the user MUST be able to choose the starting mode with a two-segment control (Real time, Timelapse), not radio buttons, and the banner MUST state it in sentence case ("Starts in real time", "Starts in timelapse x4"). | [CONTROL] [SELF] |
| UX-011 | "Starting" and "Saving" MUST be shown; controls that cannot act then are disabled, not hidden. While saving, zone F MUST say "Keep OBS open until the file is saved."; any busy animation stops within 5 s (UX-063). | [SELF] [ROBUST] |
| UX-012 | The dock MUST work from 220 px wide, switching to the compact layout when its visible area (the scroll area's viewport, not the dock's own width) is narrower than 260 px, and back at 276 px or wider, without hiding any function. Its minimum width MUST NOT exceed the visible area: rows wrap or stack instead. | [TASK] |
| UX-013 | The plugin MUST NOT open modal dialogs while a TS recording is running. | [CONTROL] [ROBUST] |
| UX-014 | Closing the dock MUST NOT affect the recording; reopening shows the live state. | [ROBUST] |
| UX-016 | Colors MUST come from `QPalette` roles and OBS utility classes (`text-danger`, `text-warning`, `text-success`, `text-muted`, `frame-notice`), read from unstyled probe widgets after the theme is applied, with a fallback when a class is missing (System has no `text-muted` or `frame-notice`); no hardcoded colors. Before use, each color MUST be adjusted in lightness only until it reaches 3:1 (shapes) or 4.5:1 (text) against the surface it sits on. Styles MUST refresh on `OBS_FRONTEND_EVENT_THEME_CHANGED` and on style, palette and font changes, coalesced into one pass, never by calling `setPalette` or reading back a color the dock set itself [S2, S8]. | [EXPECT] |
| UX-017 | The plugin MUST NOT reuse OBS object names such as `#recordButton` to borrow their styles. | [ROBUST] |
| UX-018 | Icons MUST be monochrome vectors (painted with `QPainter`, or SVG tinted at runtime) in a color derived from the surface they sit on, painted at the screen's device pixel ratio and painted again when it changes. The record dot SHOULD use the theme's danger color: it appears in both recording states, so it never encodes the speed. OBS's own icon classes MUST NOT be relied on, because their fixed white or black fills vanish in half the themes. | [EXPECT] |
| UX-019 | Every release MUST be screenshot-checked in Yami Default, Classic, Grey, Light, Acri, Rachni and System, on each OS. The theme render harness (`tests/ui/obs-themes.sh`) MUST draw every state in those 7 themes, in both languages, at 220, 260 and 300 px, plus Font Size 8 and 12 with Classic and Comfortable density, and check fit, fixed positions, hero size and contrast; a sandbox OBS spot check per release completes it. | [EXPECT] |
| UX-015 | The dock SHOULD let theme authors override its derived colors through Qt properties (`TswDock { qproperty-accentColor: ...; }`, also `dangerColor`, `successColor`, `warningColor` and `surfaceColor`). Unset properties come from the theme; set ones still pass the contrast adjustment of UX-016. | [EXPECT] |
| UX-128 | While recording, in real time or in timelapse, zone E MUST offer "Pause" next to "Stop and save"; while paused, the same button, in the same place, MUST read "Resume". Accessible names: "Pause recording" and "Resume recording". The hotkey "Pause or resume" does the same (3.11). | [CONTROL] [LEARN] |
| UX-129 | The paused state MUST be unmistakable without color: a PAUSED tally with a pause glyph (two bars), no record dot and no REC, and a frame style that differs from both recording states and from saving. Its hero says, in sentence case, the mode it will resume in: "Resumes in real time" or "Resumes in timelapse x4". | [SELF] |
| UX-130 | While paused, the main slot MUST read "Resume in real time" or "Resume in timelapse xN": the label itself always names the mode and speed recording will resume in, so the user never has to guess. | [SELF] [CONTROL] |
| UX-131 | While paused, the user MUST be able to choose how to resume on one rail, "Resume in", that takes the speed chips' place and height: Real time, x2, x4, x8 and x16, the choice marked as armed (outlined and bold). Real time means resume in real time; a speed means resume in timelapse at that speed. The mode and speed hotkeys choose too. None of them resumes by itself. The choice starts from the mode and speed before the pause and lasts until that pause ends. At 300 and 260 px the five segments MUST show their words in both languages; where "Real time" cannot fit on the rail's one row, its play glyph stands for it, with the words in its tooltip and accessible name. | [CONTROL] |
| UX-132 | While paused, RECORDED and FINAL VIDEO MUST stand still (paused time never counts in either), and the status line MUST show how long the pause has lasted ("Paused for 0:01:12"). | [TASK] [SELF] |
| UX-133 | Pause and resume MUST act at once and as often as the user wants; only a second press within 400 ms of the previous pause or resume is taken as a double press and ignored (UX-046). Stopping while paused MUST save the file like any other stop. | [CONTROL] [ROBUST] |

### 3.15 Requirements: confusion guards and onboarding

| ID | Requirement | Principle |
|---|---|---|
| UX-020 | The plugin MUST NOT use OBS's strings "Start Recording" or "Stop Recording" (Spanish "Iniciar Grabación", "Detener Grabación") for its own controls [S7]. | [SELF] |
| UX-021 | While idle, zone F MUST show the separate-file hint until the user has completed 3 TS recordings, then MAY collapse it to an info icon with the same tooltip. | [LEARN] |
| UX-022 | If OBS's own recording starts while TS is idle, the dock MUST show TSW-E301 (info). | [ROBUST] |
| UX-023 | If both recordings run, the dock MUST show TSW-E302 (warning) once per session and MUST still allow it. | [CONTROL] |
| UX-024 | Pressing the switch or a speed key while idle MUST pre-select the starting mode and say so in the banner; it MUST NOT silently do nothing. | [ROBUST] |
| UX-025 | TS files MUST carry a distinct, configurable suffix (default `tswitch`) after the OBS filename pattern. | [ROBUST] |
| UX-026 | All hotkey names MUST start with "Timelapse Switch:". | [SELF] |
| UX-027 | The plugin icon SHOULD combine a record dot with fast-forward chevrons and MUST NOT use the OBS logo [S14]. | [SELF] |
| UX-030 | On first run the dock MUST become visible and show the non-modal welcome card with "Set up hotkeys" and "Got it". | [LEARN] |
| UX-031 | The welcome card and the settings MUST show which output settings will be used, in plain words, with a "Change..." link. | [SELF] |
| UX-032 | First run and every Record MUST run a preflight (folder writable, free space, encoder available, chapter support) and report problems before recording starts. | [ROBUST] |
| UX-033 | Platform notes (Wayland focus, macOS Input Monitoring) MUST appear once, only on the affected platform, and stay reachable from Help. | [LEARN] |
| UX-034 | Update notices MUST be opt-in (unticked box in the welcome card); no other network traffic. | [CONTROL] |
| UX-035 | Help MUST offer "Show welcome again". | [LEARN] |

### 3.16 Requirements: settings, hotkeys, feedback and delight

| ID | Requirement | Principle |
|---|---|---|
| UX-040 | The dock MUST contain only session controls; configuration lives in Settings (gear, and Tools menu). The settings dialog MUST be non-modal, so it never blocks a running session (UX-013). | [TASK] [CONTROL] |
| UX-041 | Output MUST default to "Use my OBS recording settings". When OBS records with the stream encoder or a Custom FFmpeg output, which cannot be reused as is, the dialog MUST say what will be used instead and why. | [SELF] [ROBUST] |
| UX-042 | Audio during timelapse MUST default to Silence, with "Sped-up audio" as an option. The file MUST always keep an audio track (real silent samples) and a constant frame rate, because some social platforms may reject files without audio. | [TASK] |
| UX-043 | Settings changes to output MUST apply to the next recording; during a recording those fields are disabled with "Applies to the next recording". Feedback options apply at once. | [ROBUST] [SELF] |
| UX-044 | Chapter markers MUST default to on when the format supports them (Hybrid MP4 or MOV, OBS's default container for new profiles since 32.0 [S11]) and MUST be greyed out with an explanation otherwise (TSW-E006). Names: "Real time", "Timelapse x4", in the UI language. | [SELF] |
| UX-045 | Feedback settings MUST include Glance size (Normal, Large), Sound cues (off by default), Reduce motion, Status bar indicator. | [CONTROL] |
| UX-046 | Repeated presses of the same switch within 400 ms (setting, 0 to 1000 ms) MUST be ignored. | [ROBUST] |
| UX-050 | Hotkeys MUST be registered with OBS's frontend hotkey API, using pairs for Record/Stop and Timelapse/Real time [S9]. | [EXPECT] |
| UX-051 | Hotkeys MUST NOT be bound by default. The helper's "Use suggested keys" MUST check existing OBS bindings for conflicts before applying. | [CONTROL] [ROBUST] |
| UX-052 | Stopping by hotkey MUST need a hold (default 1 s, setting) with a fill animation on the Stop button. Clicking Stop is immediate, unless OBS's "Show confirmation dialog when stopping recording" is on, in which case an inline "Click again to stop" (not a dialog) is used. | [ROBUST] [EXPECT] |
| UX-053 | Hotkey actions MUST produce the same visual feedback as clicks. | [SELF] |
| UX-054 | A manual "Add chapter marker" action MUST exist (hotkey and `[?]` menu). | [TASK] |
| UX-060 | The banner MUST change as soon as the recorder confirms a switch; perceived delay under 100 ms. | [SELF] |
| UX-061 | Sound cues MUST be off by default, under 300 ms, distinct per event, and the setting MUST warn that cues can end up in the recording. | [TASK] [ROBUST] |
| UX-062 | A status bar indicator SHOULD be offered if technically feasible without disturbing OBS's own status bar. | [SELF] |
| UX-063 | The dock MUST NOT blink. Optional motion MUST stop within 5 s and obey "Reduce motion" [S46]. | [ENGAGE] |
| UX-064 | Mode switches MUST NOT produce audible clicks (micro-fade). | [ENGAGE] |
| UX-070 | After Stop, the dock MUST show the summary card: lengths, time saved, speed map, file name, Open folder, Play, Copy chapters. | [ENGAGE] [TASK] |
| UX-071 | "Copy chapters" MUST output YouTube-valid timestamps on the final timeline, or explain the rule and offer a merged version [S67]. | [TASK] |
| UX-072 | Microcopy MUST be plain and kind; at most one light touch per session, never in errors. | [ENGAGE] |

---

## 4. Error and feedback UX

### 4.1 Three levels, never color alone

| Level | Looks like | Where | Lifetime | Example |
|---|---|---|---|---|
| Info | `(i)` icon + word "Info"; text in the muted color (`text-muted`) | Zone F | Until dismissed, or 15 s; always kept in history | Chapters not available in MKV |
| Warning | `(!)` icon in the warning color + word "Warning"; text in the normal text color | Summary in zone B's status line; details in a card below Stop; the banner keeps the state | While the condition lasts, plus 30 s | Frames lost, disk space low |
| Error | `(X)` icon + word "Error" + dashed danger frame on the banner; text in the normal text color | The banner shows the error headline; the error card takes zones D to F; zone C keeps its position | Until dismissed | Disk full, encoder failed |

Status colors (danger, warning, success) color only icons, frames and bars, always next to a word. They never color body or title text: as text they fail contrast in most themes (`text-danger` is 1.90 to 2.93:1 on the dark styles, `text-warning` 2.49:1 in Light).

Errors are shown where the user is looking (the dock), never only in a log, and never as a modal box during a session. This follows the usual guidance: show errors near their source, never rely on color alone, use plain language, give constructive advice, do not blame, and preserve the user's work [S54]. The last point is the heart of UX-083: the work here is the recording.

### 4.2 Message template

```
Banner:  (X) ERROR
         <What happened: a plain headline, 2 to 5 words>
Card:    <Reassurance: what is safe or saved, with times>               [x]
         Why: <the cause in plain words, with the real value: path, MB free, encoder name>
         What to do: <one or two concrete steps>
         [Main fix] [Second fix]
         Copy details   Learn more                                 TSW-Exxx
```

Writing rules: name the thing, not the user ("The disk is full", not "You filled the disk"); no internal component names ("file writer", "muxer", "output"): say what happened to the user's file; give real values; one idea per line; the code is always present but small, at the end; "Copy details" copies the full English log block for bug reports; "Learn more" opens the code's docs page in the UI language.

### 4.3 Error code catalogue

Codes share one namespace, `TSW-Exxx`, for every condition users might search for; the level is shown separately. Hundreds group the area.

| Code | Level | Area and trigger |
|---|---|---|
| TSW-E001 | Error | Start: the output failed to start. |
| TSW-E002 | Error | Start: the chosen video encoder is not available. |
| TSW-E003 | Error | Start: the recording folder is missing or read-only. |
| TSW-E004 | Error | Start: not enough free space for even a few minutes. |
| TSW-E005 | Info | Start: OBS settings cannot be reused as is; the substitute is named. |
| TSW-E006 | Info | Start: chapters are not available in this format. |
| TSW-E101 | Error | Storage: the disk filled up; recording stopped and saved. |
| TSW-E102 | Warning | Storage: about N minutes of space left at the current speed. |
| TSW-E103 | Error | Encoder failed during the recording; stopped and saved. |
| TSW-E104 | Info | OBS closed during a TS recording; the file was saved (shown at next start). |
| TSW-E201 | Warning | Performance: frames lost because the computer could not keep up. |
| TSW-E202 | Warning | Sync: audio and video drifted apart (corrected, or not correctable). |
| TSW-E301 | Info | Coexistence: OBS's own recording started; it is at normal speed. |
| TSW-E302 | Warning | Coexistence: both recordings are running. |
| TSW-E303 | Info | Platform: Wayland, hotkeys only work while OBS is focused. |
| TSW-E304 | Info | Platform: macOS, background hotkeys need Input Monitoring. |
| TSW-E901 | Error | Unexpected internal error; please report it. |

A word about "lost frames": skipping frames is how timelapse works, so performance messages MUST NOT say "dropped" or "skipped frames". They say "lost frames", meaning frames the computer failed to deliver (UX-087).

### 4.4 Example messages for the listed failures

**TSW-E001, output failed to start (Error, before recording)**
> **Recording could not start.** Nothing was recorded, and your scenes are fine.
> Why: OBS reported "Failed to start recording" while opening the file.
> What to do: try again. If it happens again, check the format and encoder in Settings, or share your log (Help > Log Files > Upload Current Log File) in a bug report.
> [Try again] [Open settings] · Copy details · Learn more · TSW-E001

**TSW-E002, encoder unavailable (Error, before recording)**
> **The video encoder isn't available.** Nothing was recorded yet.
> Why: Timelapse Switch is set to NVIDIA NVENC H.264, but OBS could not open it. Another program may be using it, or the graphics driver changed.
> What to do: record with the software encoder (x264) this time, or choose another encoder in Settings.
> [Record with x264] [Open settings] · Copy details · Learn more · TSW-E002

**TSW-E101, disk full (Error, during recording)**
> **Recording stopped: the disk is full.** Everything up to 0:42:10 (0:18:16 of final video) is saved in `D:\Videos\2026-10-15 19-30-00 tswitch.mp4`.
> Why: drive D:\ has 0 MB free.
> What to do: free some space or choose another folder, then press Record to continue in a new file.
> [Open folder] [Change folder...] · Copy details · Learn more · TSW-E101

Spanish:
> **Grabación detenida: el disco está lleno.** Todo hasta 0:42:10 (0:18:16 de duración final) está guardado en `D:\Videos\2026-10-15 19-30-00 tswitch.mp4`.
> Por qué: la unidad D:\ tiene 0 MB libres.
> Qué hacer: libera espacio o elige otra carpeta y pulsa Grabar para seguir en un archivo nuevo.
> [Abrir carpeta] [Cambiar carpeta...] · Copiar detalles · Más información · TSW-E101

**TSW-E102, disk space low (Warning, during recording)**
> **Disk space is running low: about 12 minutes left** at the current speed (timelapse uses less space per minute). Recording continues.
> What to do: free some space now, or plan to stop soon. [Open folder] · TSW-E102

**TSW-E201, frames lost due to load (Warning, during recording)**
> **The computer is busy: 4 % of frames lost in the last minute.** Recording continues; the video may stutter in places.
> Why: the encoder or the graphics card cannot keep up. This often happens when OBS is also streaming or recording.
> What to do: close heavy programs, stop OBS's own recording if you do not need it, or pick a faster encoder preset in Settings.
> [Details] · TSW-E201

Spanish:
> **El equipo está sobrecargado: se perdió el 4 % de los fotogramas en el último minuto.** La grabación sigue; puede haber tirones en algunos tramos.
> Por qué: el codificador o la tarjeta gráfica no dan abasto. Suele pasar cuando OBS también está transmitiendo o grabando.
> Qué hacer: cierra programas pesados, detén la grabación propia de OBS si no la necesitas, o elige un preajuste más rápido en Ajustes.
> [Detalles] · TSW-E201

**TSW-E202, audio/video sync issue (Warning, during recording)**
> **Audio and video drifted apart at 0:31:40, and Timelapse Switch corrected it.** Recording continues. Check that moment when you review the video.
> Why: the microphone and the camera run on slightly different clocks. This is common with USB microphones and phone cameras.
> What to do: nothing right now. If it keeps happening, make a 5-minute test recording and attach your log to a bug report.
> [Details] · TSW-E202
> (If the drift cannot be corrected, the same code is shown as an Error and the recording is stopped and saved, as in TSW-E101.)

**TSW-E301 and TSW-E302, the two-button guards**
> (i) **OBS is recording at normal speed.** That is OBS's own recording. To switch to timelapse, record with this dock. · TSW-E301
> (!) **Two recordings are running:** OBS's own and Timelapse Switch. That means two files and more work for your computer. Stop one if you only need one. · TSW-E302

### 4.5 Log format

OBS users share logs through Help > Log Files > Upload Current Log File [S7], and plugins write into that log. Every line from the plugin follows one format, always in English, whatever the UI language, so support can read any user's log:

```
[timelapse-switch] <event> | key=value key="value with spaces" ...
```

- Levels map to OBS's: info and events at `LOG_INFO`, warnings at `LOG_WARNING`, errors at `LOG_ERROR`.
- Times are `real=` (recorded time) and `final=` (final length) as `HH:MM:SS.mmm`.
- The home folder is written as `~` to avoid leaking user names, because uploaded logs get public links.

```
[timelapse-switch] loaded | version=1.0.0 obs=32.2.2 qt=6.x platform=windows-x64
[timelapse-switch] session start | file="~/Videos/2026-10-15 19-30-00 tswitch.mp4" format=hybrid_mp4 encoder=obs_nvenc_h264_tex fps=30 res=3840x2160 audio_timelapse=silence chapters=on start_mode=real
[timelapse-switch] switch | to=timelapse speed=4 real=00:12:03.400 final=00:12:03.400 trigger=hotkey
[timelapse-switch] speed | from=4 to=8 real=00:31:40.033 final=00:16:58.300 trigger=dock
[timelapse-switch] TSW-E201 warning | lost_frames=4.1% window=60s encoder=obs_nvenc_h264_tex | action="close heavy apps or choose a faster preset"
[timelapse-switch] TSW-E101 error | cause="no space left on device" free_mb=0 real=00:42:10.120 final=00:18:16.500 saved=true | action="free space or choose another folder"
[timelapse-switch] session end | real=00:42:10.120 final=00:18:16.500 sections=3 lost_frames=0.6% saved=true
```

### 4.6 Requirements: errors and feedback

| ID | Requirement | Principle |
|---|---|---|
| UX-080 | Messages MUST use three levels, each with its own icon, word and style; never color alone [S44, S54]. | [SELF] |
| UX-081 | Errors MUST take the banner (icon, ERROR and a plain headline) and show their card below the main slot, which keeps its position; they persist until dismissed (also with Esc). Warnings MUST NOT hide the state and MUST NOT move the main slot. Info MUST be dismissible. | [SELF] [CONTROL] |
| UX-082 | Every message MUST follow the template in 4.2: a plain headline, then reassurance first, why, what to do, one main fix, Copy details and Learn more as links, and the code, small, at the end. Messages MUST NOT name internal components. | [SELF] [LEARN] |
| UX-083 | On any failure during a recording, the plugin MUST finalize and keep the file and MUST state up to which recorded and final time it is saved. | [ROBUST] |
| UX-084 | The dock MUST keep a history of the last 20 messages of the session, reachable from `[?]`. | [CONTROL] |
| UX-085 | Each code MUST have a docs page in English and Spanish (`docs/errors/TSW-Exxx.md`), opened by "Learn more". | [LEARN] |
| UX-086 | Every message MUST be logged in English in the format of 4.5, with the code. | [ROBUST] |
| UX-087 | Performance messages MUST say "lost frames", never "dropped" or "skipped". | [SELF] |
| UX-088 | The preflight MUST estimate minutes of free space at the current settings and refuse to start below about 5 minutes (TSW-E004), warning below 15 minutes during recording (TSW-E102). | [ROBUST] |
| UX-089 | When OBS exits during a TS recording, the plugin MUST finalize the file on the exit event, log it, and show TSW-E104 at the next start, because OBS's own exit confirmation ("OBS is still currently active...") is about OBS's outputs and may not cover the plugin's recorder (to verify) [S7]. | [ROBUST] |

---

## 5. Installation and first run

### 5.1 What we learned from other plugins

| Pain point seen in the wild | Evidence | Our answer |
|---|---|---|
| Windows installer flagged as malicious by the browser and SmartScreen, while the zip of the same release was not. | move-transition #222 [S26] | Code signing (INS-007), a zip alternative, a documented "More info > Run anyway" flow. |
| macOS: "this file may be damaged", installer not working, or OBS crashing at launch with an invalid code signature after a manual copy. | SceneSwitcher #1456, #1290, #1555 [S25]; shaderfilter #90 [S27] | Notarized .pkg as the only regular path; manual copy only in advanced docs (INS-008 to INS-010). |
| Outdated advice: "Control-click and Open". Apple removed that override in macOS Sequoia; users must go to System Settings > Privacy & Security [S36, S37]. | Some plugin READMEs still give the old advice. | Current instructions with screenshots (INS-009). |
| Flatpak OBS users copy the native plugin into a folder; it does not load (`libobs.so.0` not found) or is not picked up. | move-transition #121 [S26]; shaderfilter #91 [S27] | Flatpak extension; the tarball says it is not for Flatpak (INS-013, INS-014). |
| The plugin silently does not load on an older OBS. | source-record #72 [S28] | Minimum version everywhere, `.deb` dependency, a clear log line (INS-001, INS-011, INS-016). |
| OBS will not start after a plugin is removed by hand; leftovers. | StreamFX thread [S19] | A clean uninstaller and documented paths (INS-005, INS-017). |
| Too many files on the release page. | Advanced Scene Switcher 1.36.1 lists 12 assets [S24]. | A "Which file do I need?" table, one recommended file per OS (INS-003). |

Good patterns worth copying: an installer plus ProgramData zip plus portable zip on Windows, a universal `.pkg` on macOS and `.deb` plus Flatpak on Linux (Aitum Vertical, Move, Advanced Scene Switcher [S23, S24, S26]); install docs that say where files go and how to remove them (Udo's README [S32]); a single big download link at the top of the README (Background Removal [S30]); OBS's recommended Windows location `C:\ProgramData\obs-studio\plugins`, since the old `Program Files` location "will stop working in a future version of OBS" [S12].

### 5.2 The overall flow

```
                     README: one "Download" button per OS
                                     |
      +------------------------------+-------------------------------+
      |                              |                               |
   WINDOWS                        macOS                           LINUX

   1 Run the installer (.exe)     1 Open the .pkg                 1 Which OBS do I have?
   2 SmartScreen warning?         2 "Could not verify"?             flatpak list | grep obsproject
     More info > Run anyway         System Settings > Privacy       Flatpak: flatpak install
     (unsigned builds only)         & Security > Open Anyway          ...Plugin.TimelapseSwitch
   3 Close OBS when asked           (unsigned builds only)          Ubuntu: apt install ./*.deb
   4 Installed to                 3 Installed to ~/Library/         Arch: AUR package
     C:\ProgramData\obs-studio\     Application Support/obs-        Other: .tar.xz into ~/.config/
     plugins\timelapse-switch\      studio/plugins/                   obs-studio/plugins/
      |                              |                               |
      +------------------------------+-------------------------------+
                                     |
                                Start OBS
                                     |
             Dock "Timelapse Switch" appears with the welcome card.
             Check: Docks menu, Tools menu, log line "loaded".
```

### 5.3 Windows

**Install (recommended):**
1. Download `timelapse-switch-windows-x64-installer.exe` from the README.
2. If Windows says "Windows protected your PC", click **More info**, then **Run anyway** [S24], only if you downloaded it from the official GitHub page. (Only needed while builds are unsigned.)
3. The installer asks you to close OBS if it is open, then installs to `C:\ProgramData\obs-studio\plugins\timelapse-switch\`.
4. Start OBS. The dock appears.

**Alternatives:** `timelapse-switch-windows-x64.zip`, extracted into `C:\ProgramData\obs-studio\plugins\` so you get `...\plugins\timelapse-switch\bin\64bit\timelapse-switch.dll` and `...\plugins\timelapse-switch\data\` (the structure OBS requires [S12]). For portable or Steam installs of OBS, `timelapse-switch-windows-x64-portable.zip` goes into the OBS folder, merging `obs-plugins` and `data`, as Advanced Scene Switcher documents [S24].

**Uninstall:** Settings > Apps > Installed apps > Timelapse Switch > Uninstall (with an "Also remove my Timelapse Switch settings" option). For zip installs, delete `C:\ProgramData\obs-studio\plugins\timelapse-switch\`.

### 5.4 macOS

**Install:**
1. Download `timelapse-switch-macos-universal.pkg` (Apple Silicon and Intel).
2. Double-click it. If macOS says it could not verify the package (unsigned builds only): click **Done**, open **System Settings > Privacy & Security**, scroll down, click **Open Anyway**, confirm [S37]. Control-click > Open no longer works since macOS Sequoia [S36].
3. The installer puts `timelapse-switch.plugin` in `~/Library/Application Support/obs-studio/plugins/` [S12].
4. Start OBS. If macOS asks about the plugin itself, repeat Open Anyway (users have reported this second prompt [S25]).
5. For hotkeys while working in another app: OBS > Review App Permissions > Input Monitoring [S7].

**Last resort (advanced docs only):** if macOS insists the package is damaged, remove the quarantine flag with `xattr -d com.apple.quarantine <file.pkg>`, the workaround Advanced Scene Switcher documents [S24].

**Uninstall:** in Finder, Go > Go to Folder..., paste `~/Library/Application Support/obs-studio/plugins/`, delete `timelapse-switch.plugin`. Settings live in `~/Library/Application Support/obs-studio/plugin_config/timelapse-switch/`.

### 5.5 Linux

**First, which OBS do you have?** Run `flatpak list | grep obsproject`. If it prints `com.obsproject.Studio`, you have the Flatpak and need the Flatpak extension; nothing else will load.

| Your OBS | Install | Uninstall |
|---|---|---|
| Ubuntu or Debian with OBS 31.1 or newer (OBS PPA) | `sudo apt install ./timelapse-switch-x86_64-ubuntu-24.04.deb` (or 26.04) | `sudo apt remove timelapse-switch` |
| Arch, Manjaro, CachyOS, EndeavourOS | AUR package (name to decide), for example `yay -S <name>` | `sudo pacman -R <name>` |
| Flatpak (Flathub) | `flatpak install flathub com.obsproject.Studio.Plugin.TimelapseSwitch` | `flatpak uninstall com.obsproject.Studio.Plugin.TimelapseSwitch` |
| Anything else | Extract `timelapse-switch-x86_64-linux-gnu.tar.xz` into `~/.config/obs-studio/plugins/` [S12] | Delete `~/.config/obs-studio/plugins/timelapse-switch/` |

Flatpak extensions are installed from the command line or a software center but are not listed on the Flathub website [S31], so the README must give the exact command. On Wayland, hotkeys work only while OBS is focused (section 3.11).

### 5.6 How to know it worked

Three signs, in this order: the **Docks** menu lists "Timelapse Switch"; the **Tools** menu lists "Timelapse Switch Settings..."; Help > Log Files > Show Log Files contains `[timelapse-switch] loaded | version=...`. On OBS 32 or newer, the Plugin Manager added in 32.0 also lists it and can disable it without uninstalling [S11]. If OBS crashed and offers Safe Mode (third-party plugins disabled [S7]), accept, then report the problem with the previous log.

### 5.7 Requirements: installation

| ID | Requirement |
|---|---|
| INS-001 | Builds MUST target the OBS 31.1.x SDK (as the official template pins [S13]) so one binary loads from 31.1 to the current 32.x; OBS 32 refuses plugins built for a newer OBS [S11]. Each release MUST pass a test matrix of 31.1.x, 32.0.x, 32.1.x and 32.2.x on Windows, macOS and Linux. |
| INS-002 | Release assets MUST use version-less names so README buttons can link to `/releases/latest/download/<asset>` [S40]. The version goes in the release title, the tag and the plugin's log line. |
| INS-003 | Every release note MUST start with a "Which file do I need?" table naming one recommended file per OS. |
| INS-004 | Releases MUST include `SHA256SUMS`; the docs explain how to check it (optional for most users). |
| INS-005 | The Windows installer MUST install per machine into `C:\ProgramData\obs-studio\plugins\timelapse-switch\` [S12], ask to close a running OBS, warn if an OBS older than 31.1 is detected, register an uninstaller, and offer to remove settings on uninstall. |
| INS-006 | Windows MUST also ship a ProgramData zip and a portable zip, each with a one-paragraph "where to extract" note inside. |
| INS-007 | Windows builds SHOULD be code-signed (the SignPath Foundation offers free signing to open source projects [S38]; eligibility to confirm). Until then, docs MUST show the SmartScreen flow with screenshots. |
| INS-008 | macOS MUST ship a universal `.pkg` installing `timelapse-switch.plugin` to `~/Library/Application Support/obs-studio/plugins/` [S12]. |
| INS-009 | macOS builds SHOULD be Developer ID signed and notarized, as Apple recommends for software outside the App Store [S36]. Until then, docs MUST show the Privacy & Security > Open Anyway flow and MUST NOT mention Control-click. |
| INS-010 | The quarantine workaround MAY appear only in advanced docs, clearly marked. |
| INS-011 | Linux `.deb` packages MUST exist for Ubuntu 24.04 and 26.04 x86_64 and MUST declare a dependency on `obs-studio (>= 31.1)`. |
| INS-012 | An AUR package SHOULD be published from tagged sources, with a PKGBUILD written by the author. |
| INS-013 | A Flathub extension `com.obsproject.Studio.Plugin.TimelapseSwitch` SHOULD follow after v1.0. Flathub requires disclosure of AI-generated code, forbids AI-generated content in the manifest itself and forbids AI agents opening the submission [S39]: the author writes the manifest and opens the submission personally. |
| INS-014 | The generic tarball MUST say, in its file name note and README, that it does not work with Flatpak OBS. |
| INS-015 | After install, the three signs of 5.6 MUST be true. |
| INS-016 | On an unsupported OBS or a missing dependency, the plugin MUST write one clear English log line naming the required version. |
| INS-017 | Uninstall steps MUST be documented for every channel, including where settings live and OBS 32's Plugin Manager as a way to disable. |
| INS-018 | No telemetry. Installers MUST NOT bundle anything else. The only network access is the opt-in update check. |
| INS-019 | Each OS install guide MUST fit in 5 steps with screenshots; one troubleshooting page MUST cover the seven pain points of 5.1. |
| INS-020 | Installer text and install docs MUST exist in English and Spanish. |

---

## 6. Accessibility and internationalization

### 6.1 Accessibility

The dock is mostly used at a distance, with busy hands and divided attention. That is a situational disability for everyone, so accessibility here is not an extra; it is the main use case.

| ID | Requirement | Principle |
|---|---|---|
| UX-100 | State MUST NOT rely on color: every state has a word, an icon and a frame style [S44]. Release checks include grayscale and color-vision-deficiency simulation screenshots of all 7 styles. | [SELF] |
| UX-101 | Text MUST reach 4.5:1 contrast, and state frames, fills and markers 3:1, against the color they actually sit on, in every style [S45]. The dock's background MUST be measured (OBS paints docks with `bg_base`, or `bg_window` in Classic), not assumed to be `palette(window)`. No Yami style's highlight reaches 3:1 against the dock (1.03 to 2.49:1), so the adjustment is the rule, not a fallback: theme colors are moved in lightness until they pass, and filled areas keep a 2 px frame in the adjusted accent. | [SELF] |
| UX-102 | Every control MUST be reachable with Tab in visual order and work with Space or Enter; arrow keys move between speed chips; Esc dismisses a message; the focus indicator comes from the theme, strengthened where it is weak (UX-110). The focus order MUST be set explicitly in every state. In a row of chips or segments, arrow keys move focus and Space or Enter selects, so arrowing never changes a live speed by accident. | [CONTROL] |
| UX-103 | Every control MUST have an accessible name and description (Qt uses `accessibleName` as what screen readers announce, and falls back to the tooltip for the description [S47]). Icon-only compact buttons MUST have names. Speed chips announce "Speed, 4 times faster, selected". Counters MUST have accessible names in words ("31 minutes 40 seconds recorded, 16 minutes 58 seconds of final video"), never the arrow sign. | [SELF] |
| UX-104 | Start, stop, every switch and every error MUST be announced to screen readers with `QAccessibleAnnouncementEvent` (Qt 6.8 or newer [S49]), falling back to an accessible name change on the banner with an accessibility update event [S48]. CMake chooses between the two (one source file each), not conditional compilation (`docs/coding-rules.md`, P10-8). | [SELF] |
| UX-105 | All sizes MUST be relative to the OBS base font, so OBS's Font Size and Density settings apply [S7]. "Large" glance size MUST meet 20 arcminutes at 1.5 m (section 3.6). Sizes MUST be applied through the dock's own style sheet or by painting, never with `QWidget::setFont` (Yami's `QWidget { font-size }` overrides it), from the base font read after the theme is applied, and again after every theme, Font Size or Density change. | [TASK] |
| UX-106 | No flashing, no blinking; optional motion stops within 5 s and obeys "Reduce motion" [S46]. | [ENGAGE] |
| UX-107 | Main slot buttons MUST be at least 2.5 times the base line height tall; chips and segments at least 1.8 times; Stop at least as tall as a chip, full width, and separated from the speed row by at least one row (a gap or a rule), to prevent mis-hits. Heights MUST be set after the theme is applied, because OBS themes cap button heights. | [ROBUST] |
| UX-108 | Every dock action MUST also exist as a hotkey (hands-free parity). | [TASK] |
| UX-109 | Information in tooltips MUST also be available elsewhere (label, docs or accessible description), since tooltips need a mouse. | [SELF] |
| UX-110 | Keyboard focus MUST be clearly visible: a ring at least 2 px wide reaching 3:1 against the dock, shown for keyboard focus only. | [CONTROL] |
| UX-111 | Status colors (danger, warning, success) MUST NOT color body or title text; they color icons, frames and bars next to a word. Where a theme's own pairing fails 4.5:1 for a label the dock owns (Rachni's white on cyan buttons, 2.30:1), the dock MUST correct that label's color. | [SELF] |

### 6.2 Internationalization: English and Spanish from day one

OBS plugins ship `data/locale/en-US.ini` and `es-ES.ini` and read them with `obs_module_text`. OBS itself ships a single Spanish locale, `es-ES.ini`, with terms such as "Ajustes" (Settings), "Salida" (Output), "Atajos" (Hotkeys), "Paneles" (Docks), "Herramientas" (Tools), "MP4 híbrido", "Archivos de registro" (Log Files) and "Vídeo" [S7]. Our Spanish must match those words whenever it points to an OBS screen.

**Length.** Short strings grow the most in translation: IBM's figures, quoted by the W3C, give 200 to 300 % for strings up to 10 characters and 180 to 200 % for 11 to 20 characters [S51]. Dock labels are exactly that kind of string.

| Key | English | Chars | Spanish | Chars | Growth |
|---|---|---|---|---|---|
| `State.Hero.RealTime` | REAL TIME | 9 | TIEMPO REAL | 11 | 122 % |
| `Counter.Final.Caption` | FINAL VIDEO | 11 | DURACIÓN FINAL | 14 | 127 % |
| `Action.Stop` | Stop and save | 13 | Detener y guardar | 17 | 131 % |
| `Action.ToRealTime` | Back to real time | 17 | Volver a tiempo real | 20 | 118 % |
| `Action.ToTimelapse` | Switch to timelapse x4 | 22 | Pasar a timelapse x4 | 20 | 91 % |
| `Action.Record` | Record with Timelapse Switch | 28 | Grabar con Timelapse Switch | 27 | 96 % |
| `Counter.Recorded.Caption` | RECORDED | 8 | GRABADO | 7 | 88 % |
| `Hint.Separate` | Separate file. OBS's Start Recording still records at normal speed. | 67 | Archivo aparte. «Iniciar Grabación» de OBS sigue grabando a velocidad normal. | 77 | 115 % |

Spanish happens to fit here, but future languages will not, so the layout must allow growth anyway.

| ID | Requirement | Principle |
|---|---|---|
| UX-120 | English (`en-US`) and Spanish (`es-ES`) MUST be complete in v1.0; no UI string may be hardcoded. | [LEARN] |
| UX-121 | Layouts MUST let labels wrap to two lines and grow by 2x, with no fixed widths and no truncation of verbs [S51]. A narrow dock MUST NOT replace a verb label with a shorter noun. | [SELF] |
| UX-122 | A pseudo-localization mode (for example `[!! Ŕéçöŕð ŵîţĥ Ţîɱéļåþšé Šŵîţçĥ !!]`) SHOULD exist for testing truncation. | [ROBUST] |
| UX-123 | Spanish MUST be neutral: "tú" imperatives, OBS's own terms for OBS screens, and no region-marked words where avoidable (for example "duración final" rather than choosing between "vídeo" and "video"). | [EXPECT] |
| UX-124 | The name "Timelapse Switch" MUST NOT be translated; "timelapse" stays as a loan word in Spanish. Locale files MUST carry `#` context comments for translators. | [SELF] |
| UX-125 | Accessible names and descriptions MUST be translated too, including speeds ("4 veces más rápido"). | [SELF] |
| UX-126 | Logs, codes and docs file names stay in English; docs pages exist in both languages. | [ROBUST] |
| UX-127 | Durations use `h:mm:ss` in both languages; the speed sign `×` is language-neutral. | [EXPECT] |

---

## 7. Open questions

1. **Speeds.** Integers only (x2 to x60, by keeping 1 frame in N), or also fractions such as x1.5? What should the custom chip default to: x16?
2. **Pause.** Decided on 2026-10-17: yes, with the mode to resume in chosen while paused (REQ-019, ADR-011, UX-128 to UX-133).
3. **Both recorders at once.** Keep allowing OBS's own recording in parallel (current spec, with a warning), or block it to keep things simple?
4. **Settings scope.** Per OBS profile (like OBS's output settings) or global?
5. **Speed badge in the video.** Should the TS file optionally show a small "x4" badge during timelapse? Rendered only into the TS file it would not leak into a live stream; as a scene source it would. Worth it for v1?
6. **Sped-up audio option.** Worth keeping in v1, or silence only?
7. **Sound cues.** Ship them (and with which audio dependency), or leave for later?
8. **Status bar indicator.** Acceptable to add a widget to OBS's status bar, if technically clean?
9. **Signing.** Pay for Apple Developer ID and notarization, and apply to the SignPath Foundation for Windows, before v1.0? This is the biggest single install-friction lever.
10. **Linux packaging.** AUR name: Arch convention would be `obs-timelapse-switch`, but the OBS policy discourages "OBS" in names. Use `timelapse-switch-obs`? Flathub given the disclosure rules: yes or later?
11. **Remote control.** Expose obs-websocket vendor requests so Stream Deck, Touch Portal or a phone can switch and show the state (also a Wayland workaround)?
12. **Spanish vocabulary.** Follow OBS's "vídeo" (Spain) or the Latin American "video" where the word cannot be avoided?
13. **Update check.** Is an opt-in check of GitHub releases acceptable, or no network at all?
14. **Other architectures.** Windows ARM64 and Linux ARM64 builds in v1, or later?
15. **Speed chips in real time.** A chip clicked in real time only pre-selects the next timelapse speed, while a speed hotkey enters timelapse (3.11). Should the chips also start timelapse (then idle and recording could share one rail: > 1x, x2, x4, x8, x16), or stay "Next speed" with the armed look (UX-008)?
16. **Visual direction.** Native, Studio cards or Transport (`docs/design/visual-design.md`, section 4), or a mix of them?

---

## 8. Sources

All opened between 2026-10-15 and 2026-10-16. OBS source files are cited at tag 32.2.2, the latest release (2026-08-14). ISO pages returned HTTP 403, so principle names come from secondary sources [S55, S56].

**OBS Studio: interface, API and documentation**
- [S1] OBS Knowledge Base, Themes Guide: https://obsproject.com/kb/themes-guide
- [S2] OBS source, Yami theme (palette mapping, utility classes, Controls dock styles): https://github.com/obsproject/obs-studio/blob/32.2.2/frontend/data/themes/Yami.obt
- [S3] OBS source, theme styles (Yami_Default, Classic, Grey, Light, Acri, Rachni, System): https://github.com/obsproject/obs-studio/tree/32.2.2/frontend/data/themes
- [S4] OBS source, Controls dock behavior: https://github.com/obsproject/obs-studio/blob/32.2.2/frontend/widgets/OBSBasicControls.cpp
- [S5] OBS source, status bar and recording icons: https://github.com/obsproject/obs-studio/blob/32.2.2/frontend/widgets/OBSBasicStatusBar.cpp and https://github.com/obsproject/obs-studio/tree/32.2.2/frontend/forms/images
- [S6] OBS source, `setClasses` helper: https://github.com/obsproject/obs-studio/blob/32.2.2/shared/qt/wrappers/qt-wrappers.cpp
- [S7] OBS locale files, English and Spanish: https://github.com/obsproject/obs-studio/blob/32.2.2/frontend/data/locale/en-US.ini and https://github.com/obsproject/obs-studio/blob/32.2.2/frontend/data/locale/es-ES.ini
- [S8] OBS Frontend API reference: https://github.com/obsproject/obs-studio/blob/32.2.2/docs/sphinx/reference-frontend-api.rst (rendered at https://docs.obsproject.com/reference-frontend-api)
- [S9] libobs hotkey API: https://github.com/obsproject/obs-studio/blob/32.2.2/libobs/obs-hotkey.h
- [S11] OBS Studio 32.0.0 release notes (plugin manager, Hybrid MP4/MOV default, no loading of plugins built for newer OBS): https://github.com/obsproject/obs-studio/releases/tag/32.0.0
- [S12] OBS Knowledge Base, Plugins Guide (install locations): https://obsproject.com/kb/plugins-guide
- [S13] OBS plugin template (README, buildspec pinned to 31.1.1, packaging scripts): https://github.com/obsproject/obs-plugintemplate

**OBS community**
- [S14] OBS forum, Resource and IP Policy (AI, branding; last edited 2026-07-29): https://obsproject.com/forum/threads/forum-resource-and-ip-policy.195980/
- [S16] OBS forum, "During recording: switch between regular recording and timelapse" (2023): https://obsproject.com/forum/threads/during-recording-switch-between-regular-recording-and-timelapse.167785/
- [S17] OBS forum, "Timelapse by scene switching" (2023): https://obsproject.com/forum/threads/timelapse-by-scene-switching.171601/
- [S18] OBS forum, "Timelapsing with no editing software" (2015): https://obsproject.com/forum/threads/timelapsing-with-no-editing-software.39187
- [S19] OBS forum, "StreamFX uninstalled now OBS won't even open" (2022): https://obsproject.com/forum/threads/streamfx-uninstalled-now-obs-wont-even-open.158029/
- [S20] OBS issue #13169, global hotkeys not working on Wayland (Flatpak, Kubuntu): https://github.com/obsproject/obs-studio/issues/13169
- [S21] OBS pull request #13680 (draft), global hotkeys on Wayland via xdg-desktop-portal: https://github.com/obsproject/obs-studio/pull/13680
- [S22] obs-wayland-hotkeys plugin: https://github.com/leia-uwu/obs-wayland-hotkeys

**Other plugins: UX and installation**
- [S23] Aitum Vertical (repository, `vertical-canvas.cpp`, locale, release 1.6.6 assets): https://github.com/Aitum/obs-vertical-canvas
- [S24] Advanced Scene Switcher, installation wiki and release 1.36.1: https://github.com/WarmUpTill/SceneSwitcher/wiki/Installation
- [S25] Advanced Scene Switcher issues #1555, #1456, #1290: https://github.com/WarmUpTill/SceneSwitcher/issues/1555 , https://github.com/WarmUpTill/SceneSwitcher/issues/1456 , https://github.com/WarmUpTill/SceneSwitcher/issues/1290
- [S26] Move Transition issues #222 and #121, release 3.2.1: https://github.com/exeldro/obs-move-transition/issues/222 , https://github.com/exeldro/obs-move-transition/issues/121
- [S27] Shaderfilter issues #90 and #91, release 2.6.0: https://github.com/exeldro/obs-shaderfilter/issues/90 , https://github.com/exeldro/obs-shaderfilter/issues/91
- [S28] Source Record issue #72, release 0.4.8: https://github.com/exeldro/obs-source-record/issues/72
- [S30] Background Removal README: https://github.com/royshil/obs-backgroundremoval
- [S31] Flathub OBS plugin extension example (OBSVkCapture): https://github.com/flathub/com.obsproject.Studio.Plugin.OBSVkCapture

- [S32] Udo/obs-timelapse README: https://github.com/Udo/obs-timelapse

**Platforms and distribution**
- [S36] Apple Developer, "Updates to runtime protection in macOS Sequoia": https://developer.apple.com/news/?id=saqachfa
- [S37] Apple Support, opening apps from unidentified developers (Open Anyway): https://support.apple.com/en-us/102445
- [S38] SignPath Foundation (free code signing for open source): https://signpath.org/
- [S39] Flathub requirements (AI disclosure, extensions): https://docs.flathub.org/docs/for-app-authors/requirements
- [S40] GitHub Docs, linking to releases (`/releases/latest/download/`): https://docs.github.com/en/repositories/releasing-projects-on-github/linking-to-releases
- [S67] YouTube Help, video chapters: https://support.google.com/youtube/answer/9884579

**Accessibility, HCI and languages**
- [S44] WCAG 2.2, Understanding 1.4.1 Use of Color: https://www.w3.org/WAI/WCAG22/Understanding/use-of-color.html
- [S45] WCAG 2.2, Understanding 1.4.11 Non-text Contrast: https://www.w3.org/WAI/WCAG22/Understanding/non-text-contrast.html
- [S46] WCAG 2.2, Understanding 2.2.2 Pause, Stop, Hide: https://www.w3.org/WAI/WCAG22/Understanding/pause-stop-hide.html
- [S47] Qt 6, QWidget (accessibleName, accessibleDescription, focus): https://doc.qt.io/qt-6/qwidget.html
- [S48] Qt 6, Accessibility for QWidget applications: https://doc.qt.io/qt-6/accessible-qwidget.html
- [S49] Qt 6, QAccessibleAnnouncementEvent (Qt 6.8): https://doc.qt.io/qt-6/qaccessibleannouncementevent.html
- [S50] Activu, "Optimal Character and Font Sizing for Video Walls and Other Control Room Displays" (16 and 20 arcminutes, citing MIL-PRF-87268D): https://activu.com/uploads/Optimal-Font-Size.pdf
- [S51] W3C Internationalization, "Text size in translation": https://www.w3.org/International/articles/article-text-size
- [S52] Nielsen Norman Group, "Toggle-Switch Guidelines": https://www.nngroup.com/articles/toggle-switch-guidelines/
- [S54] Nielsen Norman Group, "Error-Message Guidelines": https://www.nngroup.com/articles/error-message-guidelines/
- [S55] Seonio wiki, ISO 9241 (names of the seven ISO 9241-110:2020 principles; secondary source): https://seonio.com/wiki/iso-9241
- [S56] NIST, Human Factors and Human-Centered Design (the six human-centred design principles of ISO 9241-210; the page attributes them to 9241-110): https://www.nist.gov/itl/iad/visualization-and-usability-group/human-factors-human-centered-design

