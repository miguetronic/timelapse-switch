# Timelapse Switch: Visual Design of the Dock

| Field | Value |
|---|---|
| Version | 0.1, for the author's choice of direction |
| Date | 2026-10-16 |
| Status | Design. Nothing here is built yet; prototypes come next. |
| Readers | The author, the engineers who build the prototypes, theme authors, testers |
| Builds on | `docs/design/ux-spec.md` (what the dock does, with `UX-xxx` IDs). This document says how it looks. |

**How to read this document**

- **Section 1** is what we saw when we put today's dock inside every OBS theme. **Section 2** is the design principles. **Section 3** is the *baseline*: everything the three directions share (structure, words, the theme engine, color tokens, type, spacing, icons, narrow mode, accessibility, motion, tests). **Section 4** is the three visual directions, each complete enough to build without asking. **Section 5** is how to choose between them, and **section 6** lists what changed in `ux-spec.md`.
- **IDs.** Requirements keep their `UX-xxx` IDs from `ux-spec.md`. Where this document changes a requirement, the change is also made in `ux-spec.md` (section 6 lists them).
- **Sketches** are ASCII so they align in any editor. Legend: `(o)` record dot, `>` play, `>>` fast forward, `[]` stop, `(v)` check, `(X)` error, `(!)` warning, `(i)` info, `...` busy, `x` close. `x4` stands for the speed `×4` the UI shows. `#` is a filled area, `=` a thick line, `-` a thin line, `:` a dashed line. `*x4*` or `#x4#` is an *engaged* speed (running now), `(x4)` an *armed* speed (the next one). `( ... )` is a pill or a rounded button, `(( ... ))` a pill track or a button with an accent outline.
- **Units.** `b` is the OBS base font size in points, read after the theme is applied (10 pt with OBS's defaults). `L` is the line height of that font in pixels (18 px at the defaults). `u` is the spacing unit (4 px at the defaults). Every size in this document is written in `b`, `L` or `u`, so OBS's Font Size and Density settings scale the whole dock.
- **Colors** are named tokens (`panel`, `accent`, `onAccent`, ...) computed from the active theme at runtime (section 3.5). A hex value in this document is only an example of what a token becomes in one theme; the code never contains it.
- **Keywords.** MUST, SHOULD and MAY follow RFC 2119, as in `ux-spec.md`.

**An analogy for the whole document.** An OBS theme is like a box of paint tubes that the user picked: a few colors, a font, a size. The dock never brings its own paint. It mixes everything it needs from the user's tubes (the *tokens*), and before it puts a color on the canvas it holds it next to the color underneath and checks that the eye can tell them apart (the *contrast guard*). The three directions are three ways of painting the same picture with the same tubes.

---

## 1. What we saw

### 1.1 The dock today, inside OBS

The theme render harness (`tests/ui/obs-themes.sh`, see `tests/ui/README.md`) draws the dock exactly as OBS 32.2.2 does, in the 7 themes that come with OBS, in English and Spanish, at 300 and 220 px. Its contact sheets are in `build_x86_64/ui-screenshots/contact-sheets/`. Looking at them next to OBS's own Controls dock:

```
  Controls (OBS)                 Timelapse Switch today (Yami Default, timelapse)
 +---------------------------+  +-------------------------------------------+
 | [ Start Streaming       ] |  | ##  >> TIMELAPSE x4  ###################  |  banner: body size text
 | [ Stop Recording   ][ || ]|  |  0:31:40 recorded -> 0:16:58 final        |
 | [ Settings              ] |  |  this section 0:19:37 -> 0:04:54          |  main button moved down
 |                           |  | [        Back to real time               ]|
 |                           |  |  Speed now:       [x2][*x4*][x8][x16]     |  chips far from label
 |                           |  |                                           |
 |                           |  | [ Stop and save ]                         |  Stop looks unfinished
 |                           |  |  Audio: silence during timelapse          |
 +---------------------------+  +-------------------------------------------+
   4 px margins, 0 to 2 px gaps    9 px margins, gaps from 6 to 24 px
```

What stands out, theme by theme:

1. **The state is not big.** Yami's style sheet sets `QWidget { font-size: 10pt }`, which beats the `setFont` the dock uses for its banner, so "TIMELAPSE ×4" is as small as the body text in every Yami theme. Only the System theme shows it large. The dock has no visual entry point.
2. **The special state whispers.** The timelapse banner is filled with the theme's highlight color, which is 1.03:1 against the dock in Acri (invisible), 1.12:1 in Classic and at most 2.49:1 anywhere. Real time, the calm state, has a 2 px frame in the text color at 13 to 18:1, so it is the loudest thing on screen.
3. **Things move.** The main button moves down 26 px when the section line appears, and after a take the Record button jumps from 104 px to 226 or 348 px while Open folder and Play take its old place.
4. **Spanish at 220 px is cut off.** The narrow layout is chosen from the dock's own width, which never drops below the wide layout's minimum, so the right side is clipped (`×16`, "Grabar con Timelapse Switc", card buttons).
5. **Shapes vanish.** Cards use `QFrame::StyledPanel`, which Yami draws with a border in the dock's own color. Borders drawn with `palette(mid)` disappear (Mid equals the window color). The "Start in" radio buttons have no Yami style: the unchecked one is nearly invisible on dark themes and invisible in Rachni.
6. **The selected speed is told by color alone,** and weakly: 1.12 to 1.89:1 between the checked and unchecked chip in the Yami themes. In Acri the checked chip is darker than the others, so it looks unselected.
7. **Status colors as text fail.** The error title uses `text-danger`, 1.9 to 2.9:1 on every dark Yami theme.
8. **Rachni** paints every button cyan with white text (2.30:1), and its highlight is a dusky rose close to the danger red. **Light**'s border color is lighter than its panel, so separators look inverted. **Classic** paints tool buttons flat in the panel color, so chips read as plain text.

### 1.2 The numbers behind it

Contrast is the WCAG ratio, from 1:1 (same color) to 21:1 (black on white). Text needs 4.5:1; shapes that carry meaning (a frame, a fill, a selected marker) need 3:1.

| Pair (against the dock's real background) | Default | Classic | Grey | Acri | Rachni | Light |
|---|---|---|---|---|---|---|
| Theme highlight (timelapse fill) | 1.92 | 1.12 | 1.80 | 1.03 | 2.49 | 1.64 |
| Checked chip against an unchecked one | 1.39 | 1.12 | 1.33 | 1.17 | 1.69 | 1.89 |
| `text-danger` (error title) | 2.36 | 1.90 | 2.21 | 2.93 | 2.51 | 4.81 |
| `text-success` / `text-warning` | 4.29 / 4.56 | 3.44 / 3.66 | 4.01 / 4.26 | 5.32 / 5.65 | 4.56 / 4.84 | 2.65 / 2.49 |
| Button text on button | 10.33 | 8.59 | 9.89 | 14.71 | 2.30 | 18.16 |

The dock's real background is what OBS paints behind it: `bg_base` in every Yami theme except Classic, where it is `bg_window` (#3A393A). In System it is the desktop's window color.

The lesson: **the theme's own colors are not state cues.** They were chosen to look good, not to tell states apart. Every direction below keeps the theme's hue and adjusts only its lightness until the eye can tell the difference (section 3.5).

---

## 2. Design principles

These apply to every direction. When two of them conflict, the earlier one wins.

1. **Clarity during a recording comes first.** The dock is read in a one-second glance from 1 to 3 m away, with paint, glue or flour on the hands. If a choice makes the dock prettier but slower to read, it loses. The state is the loudest thing; the next action is second; everything else is quiet.
2. **Two signals, never merged.** A *tally* (the record dot, the word REC) answers "am I recording?". A *mode* line (REAL TIME or TIMELAPSE and the speed) answers "at what speed?". Like the red light on a studio camera and the speed dial of a tape deck, they live side by side and never replace each other.
3. **Nothing moves.** At a given width and language, the state block, the counters, the main button and Stop keep their pixel positions in every state. The hand learns where to click, like the pedals of a car.
4. **Native to OBS, by construction.** Every color is mixed at runtime from the active theme; every size is relative to OBS's font; the dock follows a theme change, a Font Size change and a Density change while OBS runs. The dock never brings a color of its own, and never needs a theme to be redesigned for it.
5. **Never color alone.** Every state has a word, a glyph and a shape. A selected control is bold and marked, not only tinted. The dock must read correctly in grayscale.
6. **Armed is outlined, engaged is filled.** A speed that will be used next looks outlined; the speed that is running now looks filled. A pre-selection never looks like a running state.
7. **Buttons are verbs at every width.** "Switch to timelapse ×4" wraps to two lines before it ever becomes "Timelapse ×4", which is the name of a state.
8. **Calm by default.** No blinking. Motion is short, has a purpose, stops by itself and obeys Reduce motion.
9. **Beauty is rhythm, alignment and restraint.** One spacing unit, two font weights, one accent at a time, every edge on the same two vertical lines. A dock that looks finished is trusted.

---

## 3. Baseline: what every direction shares

The three directions in section 4 differ only in how they *look*. Everything in this section is the same in all of them, and is built once.

### 3.1 Anatomy and fixed geometry

The six zones of `ux-spec.md` 3.2 stay, in the same order, grouped into four bands:

```
 +-- Timelapse Switch -------------------------- [][x] +
 |  STATUS   A  state block: tally line + hero line     |  fixed height
 |           B  readout: captions, digits, status line  |  fixed height
 |  ACTION   C  main button                             |  fixed y, fixed height
 |  BELOW    D  idle: start mode + speed rail           |
 |              recording or saving: caption + rail     |  same height in all four
 |              paused: caption + "Resume in" rail      |
 |              summary or error: the card (D to F)     |
 |           E  Pause (Resume) and Stop, after a gap    |  fixed y in recording states
 |  CONTEXT  F  one muted line with an info glyph       |
 +-----------------------------------------------------+
```

**Fixed positions (UX-002).** At a given width, language, font and density:

- **A** has a fixed height: the tallest of its contents over all states, measured with `QFontMetrics` at the current width (section 3.6, "hero box").
- **B** has a fixed height of three rows: captions, digits and a *status line*. The status line shows the current stretch while recording ("This stretch 12:03" in real time, "This stretch ×4 19:37 → 4:54" in timelapse), a one-line warning summary when a warning is active, and nothing (but keeps its height) when idle.
- **C** therefore starts at the same y in every state. Its height is fixed too (section 3.7).
- **D** has the same height in real time, timelapse, paused and saving, so **E** keeps its y in all recording states, paused included: Resume appears exactly where Pause was (UX-128). While paused D shows one rail, "Resume in" (Real time and the speeds), in the speed rail's place and exactly as tall, so nothing is kept empty in any state.
- **Cards** (summary and error) open *below* C, in the D to F area. They never push C down. A habitual click on Record after a take lands on Record.
- A **warning** never inserts a row above C: its summary takes B's status line, and its details go in a card in the F area (below Stop).

The UI test asserts this: `tswMain.y` (and the menu button's place) is equal across all 17 cases, the Details open or closed, and `tswStop.y`, `tswPause.x` and the height of D's rail are equal across real time, timelapse, the three paused cases and saving, at each width and language; and D keeps no empty room (section 3.13).

### 3.2 States, zone by zone

| State | A tally | A hero | B readout | B status line | C main | D | E | F |
|---|---|---|---|---|---|---|---|---|
| Idle, real time start | READY | Starts in real time (quiet) | 0:00:00 and 0:00:00, muted | empty | Record with Timelapse Switch | Start in: Real time / Timelapse; Timelapse speed rail, armed | hidden | (i) Separate-file hint |
| Idle, timelapse start | READY | Starts in timelapse ×4 (quiet) | as above | empty | as above | as above, Timelapse selected | hidden | as above |
| Recording, real time | (o) REC | REAL TIME | live | This stretch 12:03 | Switch to timelapse ×4 | Next speed rail, armed | Pause, Stop and save | File: name |
| Recording, timelapse | (o) REC | TIMELAPSE ×4 | live | This stretch ×4 19:37 → 4:54 | Back to real time | Speed now rail, engaged | Pause, Stop and save | (i) Audio: silence during timelapse |
| Paused, resumes in real time | (‖) PAUSED, and a pause sign | Resumes in real time (quiet) | standing still | Paused for 0:01:12 | Resume in real time | Resume in: Real time, ×2, ×4, ×8, ×16; Real time armed | Resume, Stop and save | (i) Nothing is recorded until you resume. |
| Paused, resumes in timelapse | as above | Resumes in timelapse ×4 (quiet) | as above | as above | Resume in timelapse ×4 (armed) | as above, ×4 armed | as above | as above |
| Saving | ... SAVING | Finishing the file (quiet) | frozen | empty | Record with Timelapse Switch, disabled | Speed rail, disabled | Pause and Stop, disabled | Keep OBS open until the file is saved. |
| Summary | (v) SAVED | File ready (quiet) | the last take | empty | Record with Timelapse Switch | Summary card | hidden | hidden |
| Error | (X) ERROR | Recording stopped (headline) | the last take, or 0:00:00 muted | empty | Record with Timelapse Switch | Error card | hidden | hidden |
| Warning (while recording) | as recording | as recording | live | (!) Warning: short text | as recording | as recording | as recording | Warning card with details |

"Quiet" heroes are sentence case, regular weight and smaller than the recording heroes (section 3.6), so idle, paused, saving and summary never take the look of a recording. While paused, the mode and speed chosen to resume in (by default the ones before the pause) show at once in the hero, the main button, the "Resume in" rail and the glyph of the Resume button; none of them resumes by itself (UX-131). Starting (the short moment between Record and the first frame) looks like Saving with the tally STARTING and the hero "Starting".

**Changes to the core view** (`src/core/dock-view.c`, unit tested) that this table needs: a tally kind and a hero key per state instead of `banner_text` and `banner_detail`; the stretch counters in real time too (`section_shown` in both recording states); the warning summary; the file name moving into the summary and error cards; and the announcement text of each change (section 3.11). The banner kinds (`enum tsw_banner`) stay.

### 3.3 Words

New and changed locale keys. Every one exists in `data/locale/en-US.ini` and `es-ES.ini`. Spanish follows UX-123 (neutral, "tú", OBS's own terms). Keys not listed keep their current text.

| Key | English | Spanish | Where |
|---|---|---|---|
| `State.Tally.Ready` | READY | LISTO | A, idle |
| `State.Tally.Rec` | REC | GRABANDO | A, both recording states |
| `State.Tally.Paused` | PAUSED | EN PAUSA | A, paused |
| `State.Tally.Starting` | STARTING | INICIANDO | A |
| `State.Tally.Saving` | SAVING | GUARDANDO | A |
| `State.Tally.Saved` | SAVED | GUARDADO | A, summary |
| `State.Tally.Error` | ERROR | ERROR | A, error |
| `State.Hero.RealTime` | REAL TIME | TIEMPO REAL | A |
| `State.Hero.Timelapse` | TIMELAPSE | TIMELAPSE | A; the speed is drawn next to it |
| `State.Hero.StartsRealTime` | Starts in real time | Empieza en tiempo real | A, idle |
| `State.Hero.StartsTimelapse` | Starts in timelapse ×%1 | Empieza en timelapse ×%1 | A, idle |
| `State.Hero.ResumesRealTime` | Resumes in real time | Seguirá en tiempo real | A, paused |
| `State.Hero.ResumesTimelapse` | Resumes in timelapse ×%1 | Seguirá en timelapse ×%1 | A, paused ("Se reanudará" would not fit 300 px on one line) |
| `State.Hero.Starting` | Starting | Iniciando | A |
| `State.Hero.Saving` | Finishing the file | Terminando el archivo | A |
| `State.Hero.Saved` | File ready | Archivo listo | A, summary |
| `Speed.Value` | ×%1 | ×%1 | speed next to the hero, chips |
| `Counter.Recorded.Caption` | RECORDED | GRABADO | B |
| `Counter.Final.Caption` | FINAL VIDEO | DURACIÓN FINAL | B |
| `Counter.Stretch` | This stretch | Este tramo | B status line |
| `Counter.Paused` | Paused for | En pausa desde hace | B status line, paused, before "0:01:12" |
| `Action.Pause` / `Action.Resume` | Pause / Resume | Pausar / Reanudar | E, the same button |
| `Action.ResumeRealTime` | Resume in real time | Reanudar en tiempo real | C, paused |
| `Action.ResumeTimelapse` | Resume in timelapse ×%1 | Reanudar en timelapse ×%1 | C, paused |
| `Label.StartIn` | Start in | Empezar en | D caption, idle (no colon) |
| `Label.ResumeIn` | Resume in | Reanudar en | D caption, paused, over the one rail |
| `A11y.ResumeChoice` | how it will resume | así se reanudará | description of the chosen segment of that rail |
| `Label.Speed` | Timelapse speed | Velocidad del timelapse | D caption, idle and saving |
| `Label.NextSpeed` | Next speed | Próxima velocidad | D caption, real time |
| `Label.SpeedNow` | Speed now | Velocidad actual | D caption, timelapse |
| `Hint.Saving` | Keep OBS open until the file is saved. | No cierres OBS hasta que se guarde el archivo. | F, saving |
| `Hint.Paused` | Nothing is recorded until you resume. | No se graba nada hasta que reanudes. | F, paused |
| `Error.E001.Headline` | Could not start | No se pudo iniciar | A hero, error |
| `Error.E002.Headline` | Encoder not available | Codificador no disponible | A hero, error |
| `Error.E103.Headline` | Recording stopped | Grabación detenida | A hero, error |
| `Error.E901.Headline` | Something went wrong | Algo salió mal | A hero, error |
| `Error.E103.What` | The file could not be written. | No se pudo escribir el archivo. | error card, after "Why:" (no internal names such as "file writer") |
| `Error.Why` | Why: | Por qué: | error card |
| `Card.Error` | Error %1 (shown ERROR TSW-E103) | Error %1 | error card header: the code costs no row of its own |
| `Card.Session` | Session (shown SESSION) | Sesión | summary card header |
| `Summary.Skipped.Caption` | of waiting skipped (shown in capitals) | de espera ahorrados | under the summary's big figure, "52:35" |
| `A11y.Hour.One` / `.Many` | 1 hour / %1 hours | 1 hora / %1 horas | spoken durations |
| `A11y.Minute.One` / `.Many` | 1 minute / %1 minutes | 1 minuto / %1 minutos | spoken durations |
| `A11y.Second.One` / `.Many` | 1 second / %1 seconds | 1 segundo / %1 segundos | spoken durations |
| `A11y.Counters` | %1 recorded, %2 of final video | %1 grabados, %2 de duración final | B accessible name |
| `A11y.Armed` | next timelapse speed | próxima velocidad del timelapse | chip description |
| `A11y.Engaged` | speed now | velocidad actual | chip description |
| `A11y.Announce.Start` | Recording started in %1 | Grabación iniciada en %1 | announcement; %1 is the hero words |
| `A11y.Announce.Switch` | Now %1 | Ahora %1 | announcement |
| `A11y.Announce.Paused` / `.Resumed` | Paused. Resumes in %1 / Recording resumed in %1 | En pausa. Seguirá en %1 / Grabación reanudada en %1 | announcements; %1 is the mode |
| `A11y.Pause` / `A11y.Resume` | Pause recording / Resume recording | Pausar la grabación / Reanudar la grabación | E accessible names (UX-128) |
| `A11y.Announce.Saved` | Saved. %1 of final video | Guardado. %1 de duración final | announcement |
| `A11y.Announce.Error` | Error: %1. %2 | Error: %1. %2 | announcement; headline, then the reassurance |

**Removed:** `Action.Record.Short`, `Action.ToTimelapse.Short`, `Action.ToRealTime.Short` and `Action.Stop.Short`. The main button and Stop wrap to two lines instead (UX-004). `State.WillStartRealTime`, `State.WillStartTimelapse`, `State.RealTime`, `State.Timelapse`, `State.Saving`, `State.Saved`, `State.Error`, `Counter.Section` and the colons in `Label.*` are replaced by the keys above. `Chapter.*` and `Hotkey.*` keys do not change.

### 3.4 Reading the theme

Think of a tailor who measures the customer before cutting: the dock measures the theme after OBS has dressed it, and only then cuts its sizes and colors. It never measures itself (that would be measuring the suit instead of the customer).

**Inputs**, read in one function, `restyle()`, from *probes*: a few hidden, unstyled widgets that the dock owns but never styles (a `QLabel`, a `QPushButton`, a `QToolButton`, and a `QLabel` and a `QFrame` per OBS class tested). They are polished with `ensurePolished()` before reading.

| Input | How it is read | Fallback |
|---|---|---|
| `b`, the base size in points | `QFontInfo(probeLabel.font()).pointSizeF()` | none needed |
| `L`, the line height in pixels | `QFontMetrics(probeLabel.font()).lineSpacing()` | |
| `hC`, the theme's control height | `probeButton.sizeHint().height()` (density shows here) | |
| `panel` | the dock's real background: render the scroll area's own background (`QWidget::render` with `DrawWindowBackground` and no children) into a 4 x 4 image and take the center pixel | palette `Window` when the pixel is transparent |
| `text`, `face`, `faceText`, `hl`, `hlText`, `placeholder` | palette roles `WindowText`, `Button`, `ButtonText`, `Highlight`, `HighlightedText`, `PlaceholderText` | |
| `toolFace` | render an unchecked probe `QToolButton` and take the center pixel | `face` |
| `mutedIn`, `dangerIn`, `warningIn`, `successIn` | `WindowText` of a probe `QLabel` with class `text-muted`, `text-danger`, `text-warning`, `text-success` | when the class leaves the color unchanged: `placeholder` (muted) or `text` (the others: a monochrome dot is still a dot) |
| `wellIn` | render a probe `QFrame` with class `frame-notice` and take the center pixel | `surface` (section 3.5) |
| `isYami` | the `text-muted` probe differs from a plain label | false |
| `isDark` | relative luminance of `panel` below 0.179 | |

`obs_frontend_is_theme_dark()` is not used: it is undefined for the System theme. The config key `Appearance/Theme` is not used either: while the user previews styles in Settings it still names the old theme.

**When to restyle.** OBS sends every theme change, Font Size change and Density change through one function, which ends in `OBS_FRONTEND_EVENT_THEME_CHANGED`. One switch also delivers about seven Qt events per widget, including a middle state with no style sheet and a 9 pt font. So:

1. `dock-obs.cpp` registers `obs_frontend_add_event_callback`; on `OBS_FRONTEND_EVENT_THEME_CHANGED` it calls `scheduleRestyle()` through `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`. It removes the callback before `obs_frontend_remove_dock`.
2. `TswDock::changeEvent` calls `scheduleRestyle()` on `StyleChange`, `PaletteChange`, `FontChange` and `ApplicationPaletteChange`, and does nothing else there. This keeps the UI test and System's palette switches working.
3. `scheduleRestyle()` sets a pending flag and posts one `QTimer::singleShot(0, this, &TswDock::restyle)`. Seven events become one pass, which sees the final theme.
4. `restyle()` sets `restyling = true`, reads the inputs, computes the tokens and metrics, rebuilds each child style sheet string and calls `setStyleSheet` only on the widgets whose string changed, sets minimum heights, clears the glyph cache, recomputes the narrow breakpoint, and calls `update()`. Events that arrive while `restyling` is true are ignored. Neither `setPalette` nor any color read from a widget the dock has styled is ever used (this is how another plugin crashed OBS with a stack overflow on a dark and light switch).
5. The first pass runs on the first `Polish` event, because OBS does not send `THEME_CHANGED` for the theme it starts with.

**Theme author hooks (UX-015).** `TswDock` declares `Q_PROPERTY(QColor accentColor ...)`, `dangerColor`, `successColor`, `warningColor` and `surfaceColor`, invalid by default. A theme can set them in its `.ovt` (`TswDock { qproperty-accentColor: #4a6dd8; }`); when a property is valid after polish it replaces the matching input, and still goes through the contrast guard. The plugin never writes them.

### 3.5 Color tokens

**Three helpers,** in a pure C module with unit tests (`src/core/color.c`, so the math is tested without Qt):

- `mix(fg, bg, amount)`: `fg` laid over `bg` at `amount` opacity, per sRGB channel.
- `contrast(a, b)`: the WCAG 2 ratio from relative luminance.
- `ensureContrast(color, bg, target)`: if `contrast(color, bg) >= target`, return `color`. Otherwise keep its hue and saturation (HSL) and move its lightness toward white when `bg` is dark (luminance below 0.179, where black and white text have equal contrast) or toward black when `bg` is light, in 1 % steps, at most 100 steps (P10-2), and return the first color that reaches the target. If none does, return `text`.

From these, two everyday words: **`shape(color, surface)`** is `ensureContrast(color, surface, 3.0)` and **`ink(color, surface)`** is `ensureContrast(color, surface, 4.5)`.

**The contrast guard.** Tokens are *intentions*, not final paint. Every glyph, line, frame or marker is painted with `shape(token, the surface right under it)`, and every text with `ink(token, that surface)`. A red dot on a dark well, on a grey button or on a pink tint gets three slightly different reds, each one visible. This is the one rule that makes all three directions safe in every theme, including themes that do not exist yet.

**The tokens:**

| Token | Formula | Used for |
|---|---|---|
| `panel` | measured (3.4) | what everything sits on |
| `text` | `WindowText` | all body and title text |
| `muted` | `ink(mutedIn, panel)` | captions, hints, idle digits |
| `hairline` | `mix(text, panel, 0.16)` | separators (decorative) |
| `edge` | `mix(text, panel, 0.30)` | outlines of neutral containers and quiet buttons |
| `surface` | `mix(text, panel, 0.06)` | cards, tracks, quiet fills |
| `raised` | `mix(text, panel, 0.12)` | painted button faces (Cards, Transport) |
| `well` | `wellIn` | the sunken status well (Native) |
| `accent` | `shape(hl, panel)` | armed outlines, timelapse frames, rails |
| `accentFill` | `hl` if `contrast(hl, panel) >= 3`, else `accent`; then moved away from `onAccent` until text on it reaches 4.5:1 | filled timelapse areas, engaged speed |
| `onAccent` | whichever of `hlText`, `text` and `panel` has the most contrast on `accentFill` | text and glyphs on `accentFill` |
| `accentText` | `ink(hl, panel)` | accent words on the panel (links) |
| `danger` | `shape(dangerIn, panel)` | record dot, error frame, hold fill edge |
| `success`, `warning` | `shape(successIn, panel)`, `shape(warningIn, panel)` | check glyph, warning glyph |
| `focus` | `accent` moved until it reaches 3:1 against both `panel` and `face` | keyboard focus ring |
| `accentTint`, `dangerTint` | `mix(accent, panel, 0.20)`, `mix(danger, panel, 0.18)` | tinted surfaces (Cards, hold fill) |
| `warningTint` | `mix(warning, panel, 0.18)` | the PAUSED pill |
| `onWarning` | whichever of `panel` and `text` has the most contrast on `warning`, through `shape()` | the bars of the pause sign |
| `hover(c)`, `pressed(c)` | `mix(text, c, 0.06)`, `mix(c, panel, 0.5)` | states of painted controls |

**One rule for filled accent areas:** they are always painted with a 2 px border in `accent`. When `accentFill` equals `accent` the border is invisible; when the fill had to stay darker so that white text reads on it (Classic, Grey), the border keeps the 3:1 edge against the panel.

**What the tokens become** (computed with the formulas above from each theme's palette and classes; System shown with KDE Breeze Dark, Qt's Fusion light palette and the default Windows light palette):

| Token | Default | Classic | Grey | Acri | Rachni | Light | System, Breeze Dark | System, Fusion light | System, Windows |
|---|---|---|---|---|---|---|---|---|---|
| `panel` | #272A33 | #3A393A | #2F2F2F | #181819 | #232629 | #E5E5E5 | #202326 | #EFEFEF | #F0F0F0 |
| `text` | #FFFFFF | #FFFFFF | #FFFFFF | #FFFFFF | #FFFFFF | #0A0A0A | #FCFCFC | #000000 | #000000 |
| `muted` | #969696 | #A2A2A2 | #969696 | #969696 | #969696 | #646464 | #A1A9B1 | #6C6C6C | #6D6D6D |
| `surface` | #34373F | #464546 | #3B3B3B | #262627 | #303336 | #D8D8D8 | #2D3033 | #E1E1E1 | #E2E2E2 |
| `raised` | #41444B | #525152 | #484848 | #343435 | #3D4043 | #CBCBCB | #3A3D40 | #D2D2D2 | #D3D3D3 |
| `well` | #13141A | #2E2D2E | #151515 | #090909 | #151617 | #C1C1C1 | #2D3033 | #E1E1E1 | #E2E2E2 |
| `accent` | #486CD7 | #4688C2 | #5173D9 | #4760B2 | #A45674 | #357DFF | #3080AA | #308CC6 | #0078D7 |
| `accentFill` | #486CD7 | #3A79B1 | #4C6FD8 | #4760B2 | #A45674 | #357DFF | #3080AA | #308CC6 | #0078D7 |
| `onAccent` | #FFFFFF | #FFFFFF | #FFFFFF | #FFFFFF | #FFFFFF | #0A0A0A | #000000 | #000000 | #000000 |
| `danger` | #DF2342 | #E54B65 | #E12F4D | #C51D38 | #D71F3E | #C01C37 | #DD0000 | #C00000 | #C00000 |
| `success` | #25A231 | #25A231 | #25A231 | #25A231 | #25A231 | #22972E | #008000 | #008000 | #008000 |
| `warning` | #B88A16 | #B88A16 | #B88A16 | #B88A16 | #B88A16 | #A67C14 | #C08000 | #BC7D00 | #BC7D00 |
| `focus` | #6B88DF | #699FCE | #6F8BDF | #556DBB | #FFFFFF | #357DFF | #3080AA | #308CC6 | #0078D7 |

And the contrast they reach:

| Pair | Default | Classic | Grey | Acri | Rachni | Light | Breeze Dark | Fusion light | Windows |
|---|---|---|---|---|---|---|---|---|---|
| `accent` on `panel` (shape, needs 3) | 3.00 | 3.05 | 3.06 | 3.03 | 3.01 | 3.02 | 3.60 | 3.21 | 3.95 |
| `accentFill` on `panel` (with the 2 px `accent` border) | 3.00 | 2.49 | 2.92 | 3.03 | 3.01 | 3.02 | 3.60 | 3.21 | 3.95 |
| `onAccent` on `accentFill` (text, needs 4.5) | 4.77 | 4.62 | 4.59 | 5.86 | 5.05 | 5.21 | 4.79 | 5.69 | 4.67 |
| `muted` on `panel` (text) | 4.85 | 4.50 | 4.53 | 6.00 | 5.14 | 4.70 | 6.64 | 4.57 | 4.54 |
| `danger` on `panel` (shape) | 3.04 | 3.03 | 3.01 | 3.05 | 3.01 | 4.81 | 3.06 | 5.63 | 5.69 |
| `focus` on `panel` (shape) | 4.24 | 4.08 | 4.10 | 3.63 | 15.21 | 3.02 | 3.60 | 3.21 | 3.95 |
| `text` on `raised` (text) | 9.75 | 7.90 | 9.15 | 12.44 | 10.43 | 12.20 | 10.65 | 13.89 | 14.03 |

Two notes. The guard only ever changes *lightness*, so Rachni stays rose, Acri stays navy-blue and Light stays blue; but in Acri the timelapse blue (#4760B2) is much brighter than Acri's own highlight (#131A30), because Acri's highlight is darker than its panel and could never carry a state. In Light the timelapse blue (#357DFF) is deeper than Light's pale highlight (#8CB5FF) for the same reason. In Rachni, the `focus` ring is white, because no shade of rose reaches 3:1 against Rachni's cyan buttons.

**Status colors are never body text** (UX-111). Danger, warning and success color glyphs, frames, bars and dots, always next to a word. Text stays `text` or `muted`. Where a theme's own pairing fails for a label the dock owns (Rachni's white on cyan buttons, 2.30:1), the dock corrects only that label's color with `ink()`.

### 3.6 Type

OBS ships Open Sans in **two weights: Regular (400) and Bold (700)**. Every direction uses only these two; a 600 weight would be synthesized or fall back.

**How sizes are applied.** Never with `setFont` (Yami's `QWidget { font-size }` overrides it). Either with a widget-level style sheet on the label (`font-size: 20pt; font-weight: 700;`), rebuilt in `restyle()` from `b`, or, for text the dock paints itself, with a `QFont` copied from the probe's font and sized with `setPointSizeF` in the paint code (style sheets do not touch painted text). Sizes are rounded to 0.5 pt so a restyle does not churn.

**The scale,** in multiples of `b`. Each direction picks its values from this vocabulary (section 4):

| Role | Range | Weight | Case |
|---|---|---|---|
| caption (zone captions, tally) | 0.75b to 0.85b | Bold | UPPERCASE |
| small (status line, context, code) | 0.8b to 0.9b | Regular | sentence |
| body (card text, chips) | 1.0b to 1.1b | Regular; Bold when selected | sentence |
| button (main, Stop) | 1.0b to 1.15b | Bold for main | sentence |
| digits (readout) | 1.4b to 2.0b | Bold | tabular |
| quiet hero (idle, saving, summary, error) | 1.3b to 1.5b | Regular (error headline: Bold) | sentence |
| hero (recording states) | 2.0b to 2.75b target | Bold | UPPERCASE |

**Hero box.** The hero is auto-fitted: for the current width, the largest size between the direction's *floor* and its *target* at which the widest recording hero of the current language ("TIMELAPSE ×16", "TIEMPO REAL") fits on one line. The A block then reserves the height of the tallest hero over all states at that width (quiet heroes may wrap to two lines in narrow docks; the reservation includes that). So A's height depends on width, language, font and density, never on the state. With the Glance size setting at Large (UX-045), the target becomes 3.5b for every direction.

**How far it reads** (24-inch 1080p screen, Open Sans cap height 0.714 em, comfortable = distance / 172, readable = distance / 215, from `ux-spec.md` 3.6):

| Hero size at b = 10 pt | Cap height | Comfortable to | Readable to |
|---|---|---|---|
| 1.0b (today in Yami) | 9.5 px, 2.6 mm | 0.45 m | 0.57 m |
| 2.0b | 19.0 px, 5.3 mm | 0.91 m | 1.13 m |
| 2.25b | 21.4 px, 5.9 mm | 1.02 m | 1.27 m |
| 2.75b | 26.2 px, 7.3 mm | 1.25 m | 1.56 m |
| 3.5b (Large) | 33.3 px, 9.2 mm | 1.59 m | 1.98 m |

The *shape* of the state block (filled, outlined, dashed) is recognized from much farther than its text; that is why every direction makes the timelapse block a large filled area.

**Digits.** Open Sans digits all have the same advance (0.571 em), so counters do not jitter; the `tnum` feature is not needed (it also needs Qt 6.7, newer than some Linux builds of OBS). Digits the dock paints itself are placed in fixed cells of one digit advance, which keeps them steady in any font.

**Measured widths** (Open Sans, in em; multiply by the size in pixels): "TIMELAPSE ×16" Bold 7.38, "TIEMPO REAL" Bold 6.57, "REAL TIME" Bold 5.15, "Record with Timelapse Switch" Bold 14.89, "Grabar con Timelapse Switch" Bold 14.43, "Volver a tiempo real" Bold 10.05, "DURACIÓN FINAL" Bold 8.61, "0:31:40" Bold 3.42.

### 3.7 Spacing and sizes

**The unit.** `u = max(3, round(L / 4.5))`: 4 px at the defaults, 3 px at Font Size 8, 5 px at Font Size 12. The spacing steps are `s1 = max(2, u / 2)`, `s2 = u`, `s3 = 2u`, `s4 = 3u`, `s5 = 4u`. OBS's Density setting reaches the dock through `hC`, the theme's own control height: every control's minimum height is at least `hC` (table below), so Comfortable gives taller controls and Classic density shorter ones, exactly like OBS's own buttons. Gaps follow the font only.

**Rhythm.** Inside a group, rows are `s2` apart; between groups, `s3` or more. Every control spans the same left and right edges. Captions sit above their control, never to its left, and have no colon.

**Sizes,** set with `setMinimumHeight` after every restyle (a minimum height wins over Yami's `max-height` cap on buttons; a style sheet `min-height` would not, because it measures only the content):

| Control | Minimum height | Notes |
|---|---|---|
| Main button (C) | `max(2.5 L, lines x L + 2 x s2)` | its label may wrap to two lines (UX-107) |
| Speed chips and start-mode segments | `max(1.8 L, hC)` | |
| Stop (E) | `max(1.8 L, hC)` | full width; at least one row (`L`) away from D (UX-107) |
| Small icon buttons (card dismiss) | `max(1.6 L, hC)`, square | |
| Card action buttons | `max(1.6 L, hC)` | |

**Yami's button padding.** Yami pads every `QPushButton` with 16 to 18 px on each side, which alone overflows a 220 px dock at Font Size 12. When `isYami`, the dock's style sheet sets `#tswDock QPushButton { padding-left: 2u; padding-right: 2u; }` (and `u` in narrow mode). Box properties are only ever set under Yami: in System, any border or background rule turns a native button into a flat style-sheet box.

### 3.8 Glyphs

All glyphs are vectors painted with `QPainter` at the widget's `devicePixelRatioF()`, in the color the contrast guard gives for the surface under them. No SVG files, no emoji, no font glyphs (OBS's own icon classes are white in dark themes and black in System, so they vanish in half the themes). Painted pixmaps are cached per glyph, size, color and ratio; the cache is cleared by `restyle()` and when `paintEvent` sees a new ratio (a floating dock moved to another screen).

Each glyph is defined on a 16 x 16 grid and scaled to its size (default `round(0.9 L)`, 16 px at the defaults). Stroke width is `max(1.5, size / 10)` px, round caps and joins.

| Glyph | Geometry on the 16 grid | Used for |
|---|---|---|
| record | filled circle, center (8, 8), radius 5 | tally, Record button |
| record, lamp | the record circle plus a ring 1.5 px wide at radius 6.5 in the surface's ink color | tally on a filled area |
| play | filled triangle (4, 3), (13, 8), (4, 13) | real time, Back to real time, Play |
| fast forward | two filled triangles (1, 3)-(8, 8)-(1, 13) and (8, 3)-(15, 8)-(8, 13) | timelapse, Switch to timelapse |
| stop | filled square (3.5, 3.5) to (12.5, 12.5), corner radius 1.5 | Stop and save |
| check | polyline (3, 8.5), (6.5, 12), (13, 4.5) | saved |
| error | circle radius 6.5 stroked, plus lines (5.5, 5.5)-(10.5, 10.5) and (10.5, 5.5)-(5.5, 10.5) | error |
| warning | triangle (8, 1.5), (15, 14), (1, 14) stroked, line (8, 6)-(8, 9.5), dot (8, 11.8) radius 1 | warning |
| info | circle radius 6.5 stroked, dot (8, 4.8) radius 1, line (8, 7)-(8, 11.5) | context line |
| busy | three dots at (3, 8), (8, 8), (13, 8), radius 1.5 | starting, saving |
| pause | two filled bars (3.5, 3) to (6.5, 13) and (9.5, 3) to (12.5, 13), corner radius 1 | paused, Pause |
| folder | outline (1.5, 4) to (14.5, 13) with a tab (1.5, 4)-(6, 4)-(7.5, 2.5)-(14.5, 2.5) | Open folder |
| copy | two rounded squares (2, 5)-(10, 13) and (6, 2)-(14, 10), stroked | Copy details |
| link out | square (2, 4)-(12, 14) stroked, arrow (8, 2)-(14, 2)-(14, 8) and (14, 2)-(8, 8) | Learn more |
| close | lines (4, 4)-(12, 12) and (12, 4)-(4, 12) | card dismiss |
| chevron | open polyline (5, 3), (10, 8), (5, 13) | timelapse bands (Native, Transport) |

The plugin's own icon (UX-027, record dot with chevrons) is the record glyph overlapping the right half of the fast-forward glyph.

### 3.9 Building blocks

Every direction is built from the same few classes; each direction only paints them differently.

| Class | Base | Job |
|---|---|---|
| `TswTheme` | plain struct | the inputs and tokens of 3.4 and 3.5, plus `b`, `L`, `u`, `hC`, `isYami`, `isDark`; filled by `readTheme(dock)` |
| `TswStatus` | `QFrame` | zone A; paints its own background and frame from the tokens (so it looks the same under Yami and System); holds the tally and hero |
| `TswReadout` | `QWidget` | zone B: two columns and the status line |
| `TswButton` | `QPushButton` | main button and Stop: a glyph and a word-wrapped label inside, both transparent to the mouse; the button's own `text()` stays empty and its accessible name carries the words; `heightForWidth` grows to two lines |
| `TswRail` | `QWidget` | a row of checkable segments in an exclusive `QButtonGroup`: speeds, and the start mode; one tab stop, arrow keys move focus, Space or Enter selects |
| `TswSegment` | `QToolButton` | one segment; property `tswArmed`; properties `tswPos` = first, middle or last |
| `TswCard` | `QFrame` | summary and error cards |
| `tswGlyph()` | function | paints a glyph (3.8) |

All files follow `docs/coding-rules.md`: functions under 60 lines, bounded loops, no recursion, every visible text from the locale files, every interactive widget with an accessible name. The current `setThemeClass`, `dangerColor` and `drawBannerIcon` helpers are replaced by `TswTheme` and `tswGlyph()`.

### 3.10 Narrow mode

**When.** The dock is narrow when the *viewport* of its scroll area is narrower than 260 px, and leaves narrow mode at 276 px or wider (16 px of hysteresis, so a dock resized near the edge does not flicker). The dock watches the viewport with an event filter; its own width is never used (UX-012). Its `minimumSizeHint().width()` MUST stay at or below 200 px at Font Size 12 with Comfortable density, so the viewport always decides.

**What changes,** in every direction:

| Zone | Wide | Narrow |
|---|---|---|
| A | hero at target size | hero auto-fit down to the floor; the speed may move to the tally line (per direction) |
| B | two columns | two columns; if a caption does not fit its column, the columns stack and B grows (only at this width) |
| C | label on one line | label wraps to two lines; never a shorter noun (UX-004) |
| D | four equal speeds, two equal start-mode segments | the same; a start-mode label that does not fit goes under its glyph |
| E | full width | full width, label may wrap |
| Cards | actions side by side | actions one under the other, full width |
| Yami button padding | `2u` each side | `u` each side |

### 3.11 Accessibility

- **Grayscale.** Each state has its own shape: READY plain, REAL TIME outlined, TIMELAPSE filled with chevrons, PAUSED held between two thick bars with a solid pause sign, SAVING dashed with dots, SAVED with a check, ERROR dashed with a cross. Release screenshots include a grayscale pass (UX-100).
- **Focus order** is set with `setTabOrder` per state, in visual order: idle: main, start mode, speeds; recording: main, speeds, Pause, Stop; paused: main, the "Resume in" rail, Resume, Stop; summary: main, card dismiss, Open folder, Play; error: main, card dismiss, fix button, Copy details, Learn more. Esc dismisses an open card (UX-102).
- **Keyboard focus ring** (UX-110): shown only for keyboard focus (Tab, Backtab or a shortcut, caught by a `FocusIn` filter that sets a `tswKeyFocus` property, cleared on `FocusOut`). Painted widgets draw a 2 px ring in `focus`, inset 1 px, following their shape. Theme-drawn widgets under Yami get `border: 2px solid <focus>` from the dock's style sheet; under System the platform's focus frame stays.
- **Rails.** Arrow keys move focus between segments without selecting; Space or Enter selects. Arrowing through the speeds during a timelapse therefore never changes the live speed by accident.
- **Names.** Every control has an accessible name in the UI language. The state block's name is the tally and hero ("Recording, timelapse, 4 times faster"); B's name uses words ("31 minutes 40 seconds recorded, 16 minutes 58 seconds of final video", never "→"); chips add "next timelapse speed" or "speed now".
- **Announcements** (UX-104): start, every switch, every speed change, saved and errors use `QAccessibleAnnouncementEvent` (polite, assertive for errors). It exists from Qt 6.8; Linux builds of OBS 31.1 can have older Qt. Per P10-8 the choice is made by CMake picking `src/ui/announce-qt68.cpp` or `src/ui/announce-legacy.cpp` (an accessible name change on the state block plus `QAccessible::NameChanged`), never with `#if` in a function.
- **Targets** are at least `1.8 L` tall (32 px at the defaults), above WCAG 2.5.8's 24 px.
- **Reduce motion** turns every animation into an instant change (3.12).

### 3.12 Motion

All motion is optional decoration on top of a state that is already complete without it. Rules for every direction:

- No blinking, ever (UX-063). Nothing loops for more than 5 seconds.
- Transitions last 120 to 180 ms with an ease-out curve (`QVariantAnimation`, `QEasingCurve::OutCubic`), and only between two states the user caused.
- The Stop hold fill (UX-052) is functional, not decoration: it always runs, and fills over the hold time.
- Until the Feedback settings (UX-045) exist, transitions are on, and a repeating motion is allowed only when it ends by itself within 5 seconds (the saving sweep, Transport's chevron drift). With Reduce motion on, everything is instant.

### 3.13 How we verify

The theme harness already draws every state in every theme. The baseline adds these checks to `tests/ui/test-dock.cpp`, in both the plain and the theme mode:

| Check | Fails when |
|---|---|
| Viewport fit | any visible widget's right edge is past the scroll area's viewport, at 220, 260 and 300 px |
| Fixed positions | `tswMain.y` differs between any two states, or `tswStop.y`, `tswPause.x` or the height of D's rail between real time, timelapse, paused and saving, at one width and language |
| No reserve | zone D has empty room: more than a group gap between its visible captions and rails, or below the last one |
| Zone E | Pause and Stop are neither one row (same y and height, Pause on the left, not touching) nor stacked (same x and width, Pause above) |
| Cut words | a painted button is narrower than its longest word, glyph and padding (a single word cannot wrap), or a rail's label is wider than its segment |
| Quick pauses | 60 Pause and Resume presses, one in four a double press within 400 ms, leave a label or the state block saying something else than the core's state, or move Pause, Stop or the main button (UX-133) |
| Hero size | the effective hero size is below the direction's floor, measured with `QFontInfo` (labels) or the painted font (exposed as a property for the test) |
| State contrast | `accent`, `danger` and the timelapse fill (with its border) are below 3:1 against what they sit on; `onAccent`, `text` and `muted` below 4.5:1 |
| No status text | a label's text color is a danger, warning or success token |
| Verbs | the main button's accessible name (its words) equals any state hero or tally text |
| Focus order | Tab from the main button does not visit widgets in visual order |
| Minimum width | `minimumSizeHint().width()` above 200 px |

The harness also gains a Font Size and Density matrix (`obs-themes.sh --matrix`: Font Size 8 with Classic density, 10 with Normal, 12 with Comfortable, in Default, Light and Classic; System was dropped from it because OBS's Font Size and Density do not reach System, and is drawn instead with a light desktop palette too), because those settings change sizes the default run never sees. A sandbox OBS spot check stays part of each release (UX-019).

---

## 4. Three visual directions

### 4.0 At a glance

| | A. Native | B. Studio cards | C. Transport |
|---|---|---|---|
| One line | Looks like OBS shipped it | A calm, premium instrument | An on-air tally and a gearbox |
| Built from | the theme's own buttons, plus one painted status well | painted cards, pills and a sliding thumb | painted strip, tiles and bars |
| Status | a sunken well, filled with the accent in timelapse | a soft card that turns tinted and framed in timelapse | a full-width strip that turns solid in timelapse |
| Hero | 2.0b | 2.25b | 2.75b |
| Corners | the theme's on its buttons (4 px; Classic 2 px); 4 px on the painted well | 8 px cards, round pills | 2 px everywhere |
| Density | tight, like the Controls dock (4 px) | airy (8 to 12 px) | firm (8 px), big targets |
| Reads well to | about 1.1 m (text), farther by shape | about 1.3 m | about 1.6 m (text), 3 m or more by shape |
| Motion | none | thumb slide, color cross-fade | chevron drift (5 s), strip cross-fade |
| Height in timelapse, 300 px, defaults | about 325 px | about 385 px | about 410 px |
| Effort | lowest | medium | medium |
| Risk | inherits each theme's quirks (Rachni cyan, Acri low contrast) | looks less like OBS in heavy themes | the strongest look; may feel loud next to OBS's docks |

All three share section 3 completely: zones, positions, words, tokens, contrast guard, sizes, narrow rules, accessibility and tests. They differ only in what follows.

---

### 4.1 Direction A: Native

**Concept.** The dock looks like the OBS team made it and put it next to Controls. Every button, chip and segment is drawn by the active theme, so hover, pressed, focus and disabled are exactly what the user sees everywhere else in OBS. The dock adds only the three things OBS lacks: a large state line in a sunken *well* (like the display of a hardware recorder), a recording tally, and markers that show selection without color. Beauty here is restraint: OBS's own margins, one well, one accent.

**Sketches, 300 px, English.**

```
 Idle                                        Recording, real time
+-- Timelapse Switch ------------ [][x] +    +-- Timelapse Switch ------------ [][x] +
| .-----------------------------------. |    | .===================================. |
| | READY                             | |    | | (o) REC                           | |  2 px muted frame
| | >  Starts in real time            | |    | | >  REAL TIME                      | |  hero 2.0b
| '-----------------------------------' |    | '===================================' |
|  RECORDED          FINAL VIDEO        |    |  RECORDED          FINAL VIDEO        |
|  0:00:00           0:00:00            |    |  0:12:03           0:12:03            |
|                                       |    |  This stretch 12:03                   |
| [ (o)  Record with Timelapse Switch ] |    | [ >>  Switch to timelapse x4        ] |
|  START IN                             |    |  NEXT SPEED                           |
| [ >  Real time    |  >> Timelapse   ] |    | [  x2  | (x4) |  x8  |  x16  ]        |
|  TIMELAPSE SPEED                      |    |  -----------------------------------  |
| [  x2  | (x4) |  x8  |  x16  ]        |    | [ []  Stop and save                 ] |
| (i) Separate file. OBS's Start        |    | File: 2026-10-15 19-30-00 tswitch.mp4 |
|     Recording still records at ...    |    +---------------------------------------+
+---------------------------------------+

 Recording, timelapse                        Error (E103)
+-- Timelapse Switch ------------ [][x] +    +-- Timelapse Switch ------------ [][x] +
| ##################################### |    | .:::::::::::::::::::::::::::::::::::. |
| # (o) REC                       > > # |    | : (X) ERROR                         : |  2 px dashed danger
| # >> TIMELAPSE x4               > > # |    | : Recording stopped                 : |  headline, Bold
| ##################################### |    | ':::::::::::::::::::::::::::::::::::' |
|  RECORDED          FINAL VIDEO        |    |  RECORDED          FINAL VIDEO        |
|  0:31:40           0:16:58            |    |  1:23:45           0:31:10            |
|  This stretch x4  19:37 -> 4:54       |    |                                       |
| [ >   Back to real time             ] |    | [ (o)  Record with Timelapse Switch ] |
|  SPEED NOW                            |    | .-----------------------------------. |
| [  x2  |*x4*  |  x8  |  x16  ]        |    | | Everything up to 1:23:45 is     x | |  reassurance in Bold, dismiss
|  -----------------------------------  |    | | saved (0:31:10 of final video).   | |
| [ []  Stop and save                 ] |    | | Why: the file could not be        | |
| (i) Audio: silence during timelapse   |    | | written.                          | |
+---------------------------------------+    | | What to do: check the file ...    | |
                                             | | [ Open folder ]                   | |
                                             | | Copy details   Learn more   E103  | |  flat links, code small
                                             | '-----------------------------------' |
                                             +---------------------------------------+
```

**Sketch, narrow (220 px, Spanish, timelapse).**

```
+-- Timelapse Switch --- [][x] +
| ############################ |
| # (o) GRABANDO        x16  # |  speed moves to the tally line
| # >> TIMELAPSE        > >  # |  hero auto-fit
| ############################ |
| GRABADO      DURACIÓN FINAL  |
| 0:31:40      0:16:58         |
| Este tramo x16 19:37 -> 1:13 |
| [ >  Volver a tiempo real  ] |
| VELOCIDAD ACTUAL             |
| [ x2 | x4 | x8 |*x16*]       |
|  --------------------------  |
| [ []  Detener y guardar    ] |
| (i) Audio: silencio durante  |
|     el timelapse             |
+------------------------------+
```

**Layout and spacing.** Dock margins `s2` (4 px) on all sides, like the Controls dock. Well padding `s3` (8 px) left and right, `1.5u` (6 px) top and bottom; tally to hero `s1`. Gaps: A to B `s2`; B to C `s3`; C to D caption `s3`; caption to its rail `s1`; start rail to speed caption `s3`; D to the rule `s3`; rule to Stop `s3`; Stop to F `s3`. Readout columns split the width 50/50, left aligned, with an `s3` gutter. The rule above Stop is a 1 px line in `hairline`, full width.

**Typography.** Tally 0.8b Bold caps. Hero 2.0b Bold caps (floor 1.5b; Large 3.5b). Quiet hero 1.3b Regular (error headline 1.3b Bold). Readout captions 0.8b Bold caps in `muted`; digits 1.5b Bold. Status line 0.9b Regular in `muted`, its digits in `text`. Main label 1.0b Bold. Rail labels 1.0b, Bold when selected. Card text 1.0b; reassurance Bold. Context 0.9b `muted`. All label sizes come from one widget style sheet rebuilt in `restyle()`:

```css
#tswTally { font-size: 8pt; font-weight: 700; }
#tswHero { font-size: 20pt; font-weight: 700; }
#tswReadout QLabel[tswRole="caption"] { font-size: 8pt; font-weight: 700; color: #969696; }
#tswReadout QLabel[tswRole="digits"] { font-size: 15pt; font-weight: 700; }
```

(The numbers are what `restyle()` writes at b = 10 pt in Default; they are never typed in the code.)

**The status well (A).** A `TswStatus` that paints a rounded rectangle (radius `round(0.22 L)`, 4 px) and its frame itself, so Yami and System look alike:

| State | Fill | Frame | Tally | Hero | Glyphs |
|---|---|---|---|---|---|
| Idle | `well` | none | READY, `ink(muted, well)` | quiet, `text` | play or fast forward, `ink(muted, well)` |
| Real time | `well` | 2 px solid `muted` | record glyph `shape(danger, well)` + REC in `text` | REAL TIME, `text` | play, `text` |
| Timelapse | `accentFill` | 2 px solid `accent` | record lamp (`danger` dot, ring in `onAccent`) + REC in `onAccent` | TIMELAPSE ×4, `onAccent` | fast forward in `onAccent`; on the right edge, two chevrons in `onAccent` at 35 % opacity |
| Starting, saving | `well` | 2 px dashed `muted` (dash 4, gap 3) | busy glyph + STARTING or SAVING, `ink(muted, well)` | quiet | |
| Summary | `well` | none | check `shape(success, well)` + SAVED | quiet, File ready | |
| Error | `well` | 2 px dashed `danger` | error glyph `shape(danger, well)` + ERROR in `text` | headline, Bold | |

Salience follows importance: the timelapse well is the only filled accent area on screen; real time has a calm grey frame and the red dot; idle has nothing but the well. Contrast: the timelapse fill with its border is at least 3:1 against the panel in every theme (table in 3.5); `onAccent` on it is 4.59 to 5.86:1; the real-time frame (`muted`) is 4.50 to 6.64:1; the record dot on the well is 3.06 to 5.00:1 after the guard; the error frame is 3.01 to 5.69:1.

**Main button (C).** A `TswButton` drawn by the theme as a plain `QPushButton`, exactly like OBS's Start Recording, full width, `2.5 L` tall. The glyph (record, fast forward or play) sits left of the label, `0.9 L`, in the button's text color; the record dot alone is `shape(danger, face)`. Native does not use OBS's `button-primary` class: its fill is the theme's primary color, and in this dock the accent fill is reserved for "timelapse is running". The button is primary by its size, its Bold label and its position.

**Rails (D).** `TswSegment`s are plain `QToolButton`s drawn by the theme, in a `TswRail`. Under Yami they are joined into one control by the dock's style sheet (inner corners square, 1 px gap of panel between segments):

```css
#tswDock QToolButton[tswPos="first"] { border-top-right-radius: 0; border-bottom-right-radius: 0; }
#tswDock QToolButton[tswPos="middle"] { border-radius: 0; }
#tswDock QToolButton[tswPos="last"] { border-top-left-radius: 0; border-bottom-left-radius: 0; }
#tswDock QToolButton:checked { font-weight: 700; }
#tswDock QToolButton[tswArmed="true"]:checked { background-color: <toolFace>; border: 2px solid <shape(accent, toolFace)>; }
```

The values in angle brackets are tokens that `restyle()` writes into the string. Under System the segments stay separate (2 px apart) and native. The selected segment is always Bold and carries a **bar**: 2 px tall (`max(2, L / 9)`), 40 % of the segment's width, centered, 3 px above its bottom edge, painted by `TswSegment::paintEvent` after the theme has drawn the button, in the segment's own text color (so it contrasts with whatever fill the theme chose). *Engaged* (timelapse running) is the theme's checked fill plus Bold and the bar. *Armed* (idle and real time) is the unchecked face with a 2 px `accent` frame, Bold and the bar; under System the frame is painted over the native button, 2 px, inset 2 px. When a theme draws tool buttons flat in the panel color (Classic: `toolFace` within 1.1:1 of `panel`), the dock adds a 1 px `edge` border to the segments under Yami so they still read as buttons. Start-mode segments carry the play and fast-forward glyphs before their words; when the Timelapse segment is selected it is drawn armed (timelapse is not running yet), when Real time is selected it is drawn checked.

**Stop (E).** A plain theme `QPushButton`, full width, `1.8 L`, stop glyph in its text color, label Regular, below the hairline rule. The hold fill (UX-052) is painted over it from the left: `danger` at 30 % opacity, with a 2 px leading edge in `shape(danger, face)`.

**Readout (B).** On the panel, no container. Captions above, digits below, the status line spans both columns. In idle the digits are `muted` "0:00:00".

**Cards.** A `TswCard` painted like the idle well (`well` fill, same radius). The header row holds the title (summary: the file name in Bold; error: the reassurance in Bold) and a dismiss button at the right: a `QPushButton` with class `btn-tool` and the painted close glyph as its icon, square, with the accessible name "Close". Summary: file name, the delight line ("You skipped 52:35 of waiting. Nice session!"), then Open folder and Play as plain theme buttons with folder and play glyphs. Error: reassurance (Bold), "Why:" line, "What to do:" line, one fix button (Open folder for E103; none until the settings dialog exists for E001 and E002), then a row of flat buttons (`setFlat(true)`, copy and link-out glyphs, text in `accentText`) for Copy details and Learn more, and the code at the right in 0.8b `muted`.

**Context line (F).** Info glyph `ink(muted, panel)` and text in `muted`, 0.9b, wrapping.

**Narrow.** Well padding drops to `1.5u`. If "TIMELAPSE ×16" does not fit at 1.5b, the speed moves to the right end of the tally line as a Bold label in the same color. Everything else follows 3.10.

**Motion.** None. Native behaves like OBS: states change instantly. Only the Stop hold fill moves.

**Accessibility.** As in 3.11. Under Yami the keyboard focus ring comes from the dock's style sheet (`#tswDock [tswKeyFocus="true"] { border: 2px solid <focus>; }`); under System the platform draws it.

**Per-theme notes.** Acri: the theme's buttons are dark navy on an almost black panel (1.21:1); the dock looks exactly like Acri's own Controls dock, and its timelapse well is the one bright area. Rachni: the main button, Stop and the card buttons are Rachni cyan; their labels are corrected to dark grey (`ink`, #404040, 4.51:1) because white on cyan is 2.30:1. Classic: radius 2 px is not detectable, so the well keeps 4 px; chips get the `edge` border. Light: the well is a darker grey (#C1C1C1) on the light panel, the timelapse well is a clear blue with near-black text (5.21:1). System: everything is native Fusion or Windows; the well is `surface`.

**Strengths and risks.** Lowest effort and the most "official" feel; any future OBS theme is followed automatically. But it also inherits every theme's weaknesses outside the status well, and its hero is the smallest of the three (comfortable to about 0.9 m at Normal size).

---

### 4.2 Direction B: Studio cards

**Concept.** A calm, premium instrument panel: soft cards floating on the OBS panel, pills for small status, and one idea carried everywhere: **outlined means next, filled means now.** The accent outline marks what will happen (the armed speed, the "Switch to timelapse" button); the accent fill marks timelapse running (the status card's band, the engaged speed). Everything is painted by the dock from the tokens, so the shapes are identical under Yami and System while the colors are each theme's own.

**Sketches, 300 px, English.**

```
 Idle                                        Recording, real time
+-- Timelapse Switch ------------ [][x] +    +-- Timelapse Switch ------------ [][x] +
|                                       |    |                                       |
|  .---------------------------------.  |    |  .=================================.  |
|  | ( READY )                       |  |    |  | ( (o) REC )                     |  |  tally pill, danger tint
|  | >  Starts in real time          |  |    |  | >  REAL TIME                    |  |  hero 2.5b
|  | - - - - - - - - - - - - - - -   |  |    |  | - - - - - - - - - - - - - - -   |  |
|  | RECORDED       FINAL VIDEO      |  |    |  | RECORDED       FINAL VIDEO      |  |  hollow: 2 px frame,
|  | 0:00:00        0:00:00          |  |    |  | 0:12:03        0:12:03          |  |  no fill
|  | This stretch 0:00               |  |    |  | This stretch 12:03              |  |
|  | ------------------------------- |  |    |  | >  --  >  --  >  --  >  --  >   |  |  tape: sparse play marks
|  '---------------------------------'  |    |  '================================='  |
|                                       |    |                                       |
|  ( (o) Record with Timelapse Switch ) |    |  ((  >>  Switch to timelapse x4    )) |  accent outline = next
|                                       |    |                                       |
|  START IN                             |    |  NEXT SPEED                           |
|  (( >  Real time  |  >> Timelapse  )) |    |  ((  x2  | (x4) |  x8  |  x16  ))     |  armed thumb: outlined
|  TIMELAPSE SPEED                      |    |                                       |
|  ((  x2  | (x4) |  x8  |  x16  ))     |    |  (  []  Stop and save              )  |  quiet pill
|                                       |    |  (i) File: 2026-10-15 19-30-00 ...    |
|  (i) Separate file. OBS's Start ...   |    +---------------------------------------+
+---------------------------------------+

 Recording, timelapse                        Summary
+-- Timelapse Switch ------------ [][x] +    +-- Timelapse Switch ------------ [][x] +
|                                       |    |                                       |
|  .=================================.  |    |  .---------------------------------.  |
|  |#################################|  |    |  | ( (v) SAVED )                   |  |
|  |# ( (o) REC )                   #|  |    |  | File ready                      |  |
|  |# >> TIMELAPSE           ( x4 ) #|  |    |  | - - - - - - - - - - - - - - -   |  |
|  |#################################|  |    |  | RECORDED       FINAL VIDEO      |  |
|  | RECORDED      |FINAL VIDEO      |  |    |  | 1:23:45        0:31:10          |  |
|  | 0:31:40       |0:16:58          |  |    |  | File: 2026-10-15 19-30-00 ts... |  |  the file it acts on
|  | This stretch x4  19:37 -> 4:54  |  |    |  | ------------------------------- |  |
|  | > > > > > > > > > > > > > > >   |  |    |  '---------------------------------'  |
|  '================================='  |    |                                       |
|                                       |    |  ( (o) Record with Timelapse Switch ) |
|  (  >   Back to real time          )  |    |                                       |
|                                       |    |  .---------------------------------.  |
|  SPEED NOW                            |    |  | SESSION                       x |  |  a ticket
|  ((  x2  |#*x4*#|  x8  |  x16  ))     |    |  | 52:35                           |  |  the waiting skipped,
|                                       |    |  | OF WAITING SKIPPED              |  |  one big figure
|  (  []  Stop and save              )  |    |  | Nice session!                   |  |
|  (i) Audio: silence during timelapse  |    |  | ( Open folder )  ( >  Play )    |  |
+---------------------------------------+    |  '---------------------------------'  |
                                             +---------------------------------------+
```

In timelapse the card is tinted and framed, with a solid accent band behind the tally and hero, a light speed badge, a bar beside FINAL VIDEO (the counter that grows slower) and chevrons on the tape, denser when faster.

**Sketch, narrow (220 px, Spanish, timelapse).**

```
+-- Timelapse Switch --- [][x] +
| .==========================. |
| |##########################| |
| |# ((o) GRABANDO) ( x16 ) #| |  badge joins the tally line
| |# >> TIMELAPSE           #| |
| |##########################| |
| | GRABADO  |DURACIÓN FINAL | |
| | 0:31:40  |0:16:58        | |
| | Este tramo x16           | |  two lines: the label above
| | 19:37 -> 1:13            | |  its values
| | >>>>>>>>>>>>>>>>>>>>>>>> | |  x16: chevrons packed tight
| '==========================' |
| ( >  Volver a tiempo real  ) |
| VELOCIDAD ACTUAL             |
| (( x2 | x4 | x8 |#x16#))     |
|                              |
| ( []  Detener y guardar    ) |
+------------------------------+
```

(In narrow mode the status line may take two lines; B reserves them at that width.)

**Layout and spacing.** Dock margins `s3` (8 px). Group gap `s4` (12 px) between the status card, the main button, the rails and Stop; the gap above Stop is at least `L` (18 px), which is the UX-107 separation without a rule. Card padding `s4` (12 px); rows inside a card `s3` (8 px); caption to rail `s2` (4 px). The status card holds zones A and B as one group, split by a dashed `hairline` (dash 2, gap 3) at `s3` from both; in timelapse the band's lower edge splits them instead.

**Typography.** Tally pill text 0.75b Bold caps. Hero 2.5b Bold caps (floor 1.5b; Large 3.5b). Quiet hero 1.35b Regular (error headline Bold). Speed badge 1.5b Bold. Readout captions 0.75b Bold caps, `ink(muted, card fill)`; digits 1.9b Bold (in a narrow dock they shrink to 1.5b before the columns stack). Status line 0.9b. Main label 1.1b Bold. Rail labels 1.1b, Bold when selected (shrinking to 1.0b beside their glyphs, then to 0.8b under them, before they wrap). Rail captions 0.75b Bold caps `muted`. Card header 0.75b Bold caps `muted`; the summary's figure 2.0b Bold in `ink(accent, surface)`; body 1.0b; links 0.95b. No text is smaller than 8 pt, whatever the Font Size: at Font Size 8 a 0.75b caption would be 6 pt. The status card and the rails paint their text; the other labels use widget style sheets as in Native.

**Shapes.** Cards and the main button: twice the corner radius of the theme's own button, measured from a picture of it (8 px in the Yami themes, 4 px in Classic), so the dock's corners follow the theme. Pills (tally, badge, rails, Stop, card actions): radius half their height; a rail whose labels stack under their glyphs uses rounded rectangles instead, never tall stadiums. The main button is a rounded rectangle, because it may hold two lines.

**Status card (A and B), by state.** Each state has its own shape, so idle, real time, timelapse and paused differ without color: soft and filled, hollow and outlined, a solid band, a pause sign as big as the card.

| State | Fill | Frame | Tally pill | Hero |
|---|---|---|---|---|
| Idle | `surface` | 1 px `hairline` | outlined 1 px `muted`, READY in `ink(muted, surface)` | quiet, play or fast-forward glyph in `muted` |
| Real time | none: the panel shows through | 2 px `frame` (`shape(edge, panel)`) | filled `dangerTint`, record glyph `shape(danger, dangerTint)`, REC in `text` | REAL TIME in `text`, play glyph |
| Timelapse | `accentTint`, and a solid band of `accentFill` behind the tally and hero rows | 2 px `accent` | as real time | fast-forward glyph and TIMELAPSE in `onAccent`; speed badge at the right: a pill in `badgeFill` (the lightest theme color with 3:1 on the band), ×4 in the band's color |
| Paused | none: the panel shows through | 1 px `frame`, held between two side bars `min(0.3 L, pad / 2)` wide in `shape(warning, panel)` that follow the corners | filled `warningTint`, pause glyph `shape(warning, warningTint)`, PAUSED in `ink(text, warningTint)`; at the right end of the tally line a pause sign: a pill the size of the speed badge in `shape(warning, panel)`, its bars in `onWarning` | quiet: Resumes in real time, or Resumes in timelapse ×4, after the play or fast-forward glyph in `muted` |
| Starting, saving | `surface` | 1 px dashed `shape(edge, surface)` | outlined, busy glyph, STARTING or SAVING | quiet |
| Summary | `surface` | 1 px `hairline` | outlined 1 px `shape(success, surface)`, check glyph, SAVED | quiet, File ready |
| Error | `dangerTint` | 2 px dashed `danger` (dash 4, gap 3) | filled `surface`, error glyph `shape(danger, surface)`, ERROR | headline, Bold, `text` |

While paused the pause sign is the loudest thing in the card; it is a tally ("am I recording?"), so it sits on the tally line, never beside the hero, and it overhangs the gaps above and below that line instead of growing the card. The amber is OBS's own `text-warning` (the convention of OBS's status bar, which shows a yellow pause icon); a theme without it gives the text color, and the shapes still tell the state.

In timelapse the FINAL VIDEO digits are painted in `ink(accent, accentTint)`, and a 3 px `accent` bar stands in the gutter beside them: the counter that grows slower is marked by color and by shape. Contrast: `text` on `accentTint` is 9.2 to 15.0:1; `accent` on `accentTint` for words and glyphs after `ink()` is at least 4.5:1; the 2 px `accent` frame is at least 3:1 against the panel; `text` on `dangerTint` is 9.8 to 15.9:1; the record dot on `dangerTint` after `shape()` is 3.00 to 4.07:1.

**The status line** keeps its row in every state, so it is never an empty hole: the current stretch while recording, frozen while saving, at zero and dimmed when nothing was recorded (idle, a start that failed), and the file's name after a take. When the row needs two lines (narrow docks), the label always takes the first and its values the second.

**The tape band** (from Transport), along the bottom inner edge of the card, `0.7 L` tall, shows how fast the tape runs without a word: in timelapse, chevrons in `shape(accent, accentTint)` whose pitch tightens with the speed (×2 1.6, ×4 1.2, ×8 0.9, ×16 0.7 band heights); in real time, sparse play marks joined by a line in `frame`; while paused, the same with pause marks; while saving, one still 2 px line in `accent`; otherwise an empty tape, one faint hairline.

**Main button (C).** Painted `TswButton`, full width, `2.75 L` (above the 2.5 L of UX-107: an easier target with paint on the hands):

| Action | Face | Frame | Glyph | Label |
|---|---|---|---|---|
| Record with Timelapse Switch | `raised` | none | record, `shape(danger, raised)` | `text` |
| Switch to timelapse ×4 (armed) | `panel` | 2 px `accent` | fast forward, `ink(accent, panel)` | `accentText` |
| Back to real time | `raised` | none | play, `text` | `text` |
| Resume in timelapse ×4 (paused, armed) | `panel` | 2 px `accent` | fast forward, `ink(accent, panel)` | `accentText` |
| Resume in real time (paused) | `raised` | none | play, `text` | `text` |
| Disabled (saving) | `surface` | none | glyph in `muted` at 60 % | `muted` at 60 % |

`raised` is the theme's own button face in light themes (OBS Light's white), so the dock's buttons match the Controls dock; dark themes keep the mixed face, because some paint their buttons in a loud color (Rachni's cyan). Hover moves the face to `hover(face)`; pressed to `pressed(face)` and the content 1 px down. The keyboard focus ring is 2 px `focus`, always inside the button with a 2 px gap past any frame: the edge is where the armed button has its accent outline, so focus never reads as "next".

**Rails (D).** A `TswRail` paints a pill track (`surface` fill, 1 px `hairline`, height `max(1.8 L, hC)`); its `TswSegment`s paint only their labels. The selection is a **thumb**: a pill inset 2 px inside the track, under the selected segment.

| Selection | Thumb | Label |
|---|---|---|
| Engaged (timelapse running) | `accentFill`, 2 px `accent` border | `onAccent`, Bold |
| Armed (idle, real time) | `panel` (a cut-out look), 2 px `shape(accent, surface)` border | `text`, Bold |
| Start mode, Real time selected | `raised`, 1 px `edge` | `text`, Bold |
| Unselected segment | none; `hover(surface)` capsule on hover | `text`, Regular |

**The "Resume in" rail** (paused) is the speed rail's twin in the same place: Real time, then ×2, ×4, ×8 and ×16, all armed (a choice for later, never filled). Real time is a *lead* segment: as wide as its play glyph and word need, the speeds sharing the rest, every segment with an equal share of the spare room, so the five fit one row: with their words at 300 and 260 px in both languages (labels shrink from 1.1b to 0.9b first; Spanish at 260 px needs it), and where even then "Real time" cannot fit (220 px, Font Size 12 in Spanish), its play glyph alone in five equal segments, the words in its tooltip and accessible name. It never takes a second row, so it is always as tall as the speed rail.

When the selection changes, the thumb slides to its new segment in 160 ms (OutCubic), its width too where segments differ. The armed outline is 3.00 to 3.47:1 against the track after the guard; the engaged thumb is filled, so armed and engaged differ in shape, not only in color.

**Pause and Stop (E).** Two pills side by side, `s3` apart, `max(1.8 L, hC)`, no fill, 1 px `edge` frame, glyph and label in `text`, Regular: Pause (two bars) and Stop and save (square). While paused the first reads Resume, with the glyph of the mode it will resume in, as on the main button. Equal halves while every label they may show fits its half on one line; else Pause takes what its longer word needs and Stop the rest, so "Stop and save" keeps one line wherever it can (Spanish at 300 px, English at 260 px); only where a word would be cut (220 px at Font Size 12) they stack, full width, like the card's pills. When a label still wraps both become rounded rectangles, never tall stadiums. The arrangement depends only on the width, the language and the theme, so Resume is exactly where Pause was. The hold fill grows from the left of Stop as `dangerTint` with a 2 px leading edge in `shape(danger, dangerTint)`.

**Cards (summary, error), as tickets** (from Transport). A `TswCard` with `surface` fill, 1 px `hairline`, the cards' radius, below the main button at `s4`. Header: a small caption, SESSION or ERROR TSW-E103 (0.75b Bold caps `muted`; the code costs no row of its own), and a round dismiss button (circle `1.6 L`, close glyph, `hover(surface)` on hover). Summary: the waiting the timelapse skipped as one big figure ("52:35", no empty hours; none when the take never sped up) with OF WAITING SKIPPED under it, the delight line, then two pills, Open folder and Play (`raised`, glyph and label in `text`, height `1.6 L`); the file is named in the status line above, which the card acts on. Error: reassurance (Bold), "Why:", "What to do:" (one short sentence or two), the fix pill, then Copy details and Learn more as text links in `accentText` (underlined on hover and on keyboard focus), side by side while they fit. The pills share a row only while neither label wraps in its half; else they stack, full width. The speed map, when the recorder provides sections, is a pill-shaped strip in the summary card: real-time stretches in `raised`, timelapse stretches in `accentTint` hatched with 45 degree lines in `accent` at 30 % and labelled ×N when wide enough (hatching, so it never relies on color).

**Context line (F).** As in Native: an info glyph and `muted` text, 0.9b, on the panel.

**Narrow.** Dock margins `s2`, card padding `s3`, group gap `s3`. The speed badge moves from the hero line to the right end of the tally line. Rails stay one row (×16 at 1.1b is 25 px in a 52 px segment). Card pills stack full width.

**Motion** (with Native's restraint). Only the thumb slides, in 160 ms; Reduce motion makes it instant and ends a slide under way (UX-045). The status card changes state at once, and the saving line is still: the cross-fade and the sweep of the first prototype were dropped. The Stop hold fill is functional and always runs.

**Accessibility.** As in 3.11. The status card is one painted widget: its accessible name is the tally and hero, its description the readout in words, updated without announcing every second; announcements come only from state changes.

**Per-theme notes.** Rachni: buttons are the dock's own neutral grey (`raised`), not Rachni's cyan, and the accent is Rachni's rose (#A45674); the dock reads as a calm Rachni rather than a loud one. Acri: the band is a bright blue (#4760B2) over a deep navy tint; it reads as Acri. Light: the buttons are OBS Light's white, the band a clear blue with near-black text, the tint pale. Classic: 4 px corners, from Classic's 2 px buttons; Classic also gives the dock 8 px less width than the scroll area's viewport, so the dock holds its height at what its real width needs and scrolls rather than squeezes. System: identical shapes in Fusion and Windows, colors from the desktop.

**Strengths and risks.** The most refined and modern; the "outlined next, filled now" idea makes the whole dock teach itself. It looks least like OBS's stock docks in heavy themes (Rachni, Acri), and it paints more, so more code.

**Prototype notes** (branch `ui-dir-cards`). Built as written, with these choices the text left open or that the screenshots asked for:

- *Focus on a speed segment is a dashed ring.* A solid ring around a segment looks exactly like the armed thumb, so it would say "this is the next speed" when it is only focused; on the thumb it sits inside the thumb's outline. Buttons keep a solid ring, always inside the button with a 2 px gap, so it never takes the edge where the armed button has its outline.
- *Two-line labels are balanced* ("Record with / Timelapse Switch", never "Record with Timelapse / Switch"), and a speed stays with its word through a no-break space ("timelapse ×4", never "×4" alone on a line). The warm summary line gets its own line.
- *The control height* `hC` is the button's painted height, not its size hint: Yami adds 2 px of transparent margin above and below, so the hint said 36 px where OBS's buttons show 32 px.
- *Extremes, never clipping.* At Font Size 12 in a dock narrowed by a scroll bar, the main button may take three lines, a start-mode word may wrap under its glyph, and a recording hero that cannot fit at its floor size drops its leading glyph before it would shrink or clip.
- *No glyph cache.* Glyphs are painted straight into each widget's painter, which is sharp at any screen scale; a cache can come later if a measurement shows the need (`docs/coding-rules.md`, D3).
- *Not built yet, for lack of data in the view:* the speed map in the summary card (its place is the tape band), the warning line and card, the Starting state (no such phase is reported yet; its words exist), the Large glance size and Reduce motion settings (the dock has `setReduceMotion`), and the Stop hold fill (painted when given a progress; nothing reports a hotkey hold to the dock yet).
- *Pause* (2026-10-17, REQ-019, UX-128 to UX-133), with these choices the screenshots asked for: the pause sign sits on the tally line, because beside the hero it made "Resumes in timelapse ×16" wrap and every state's card grow; the side bars carry the frame's shape over a thin neutral outline, because a 2 px amber outline with slightly thicker sides read as a color change only; the maintainer then chose one "Resume in" rail (Real time and the speeds) over the two rails of the first build, whose second rail kept about 60 px of empty room in every recording state; and a rail that appears shows its choice at once, without the thumb's slide.
- *Monitoring and attribution* (2026-10-17, REQ-020, UX-134 to UX-138), as built: the **health line** takes zone F with the warning glyph in `warning`, its words in `text` and its code small in `muted`, never split at the hyphen; an issue stays 30 s of recorded time after its condition ends (4.1 of the UX spec), so it never blinks, except both recordings running (TSW-E302), which the user ends on purpose and which goes at once. The **Details** sit last, below F, so opening them moves nothing: a caption-style toggle (chevron and DETAILS, `muted`, `text` under the mouse) and a card of label and value rows in three groups set apart by hairlines (time, then file and disk, then the map), labels `muted`, values `text`, both 0.9b; a value not known yet is a muted "…"; a number never parts from its unit when a line wraps. The **speed map** is a strip `0.75 L` tall on a card: real time a quiet fill (`text` at 12 % over the card) with a 3:1 outline, timelapse a tint hatched at 45 degrees in the accent, denser when faster (x2 0.6, x4 0.4, x8 0.28, x16 0.2 strip heights), a pause a gap where only the tape line shows; the summary card shows it under the warm line. The **menu button** is a circle `1.6 L` with three dots in the status card's top corner, quiet until hovered, focused or pressed, at the end of the tally line (the pause sign and the narrow speed badge move left of it), or, where even the widest tally, its badge and the button cannot share the line (the narrowest dock at the largest font), in a row of its own above it; its clear margin may reach into the card's padding, its dots never past the content. While its menu is open it keeps a firmer circle, so the menu reads as coming from it. The **About** dialog is painted on the dock's panel color, centered: the symbol (the plugin's own pictures, the dark variant on dark themes), the name at 1.5b Bold, the version `muted`, "by Miguetronic", four links in `accentText` with the link-out glyph, the non-affiliation line `muted` in balanced lines, and a Close pill.
- *Grafts after the panel review* (2026-10-17, the maintainer chose this direction with them): from Transport, the hollow real-time card, the solid timelapse band, the tape band, the accent bar beside FINAL VIDEO, the summary ticket with its big figure, the error code in the card header and the taller main button; a larger hero and digits; from Native, restraint in motion, the theme's own corners and light button faces, and the tighter narrow margins. The tables above describe the result.

---

### 4.3 Direction C: Transport

**Concept.** An on-air tally light and a tape deck's transport. At the top, a full-width **strip** like a studio's ON AIR sign: empty when ready, outlined in real time, solid accent in timelapse, readable from across the room by its shape alone. Inside the strip, a **tape band** whose texture shows the speed: sparse play marks at normal speed, chevrons in timelapse, packed tighter the faster it goes. Below, big studio-clock digits; then a deck of square-cornered keys: one large main key, a **gearbox** of four speed keys with big numerals, and a long Stop bar, set apart by a rule. Sharp 2 px corners, uppercase captions, one state color at a time.

**Sketches, 300 px, English.**

```
 Idle (strip: surface fill, no frame)        Recording, real time
+-- Timelapse Switch ------------ [][x] +    +-- Timelapse Switch ------------ [][x] +
|                                       |    |=======================================|
| READY                                 |    || (o) REC                    0:12:03  ||  hollow strip,
| >  Starts in real time                |    || REAL TIME                           ||  2 px muted frame
| ------------------------------------- |    || >  -  -  >  -  -  >  -  -  >  -  -  ||  tape band: play marks
|                                       |    |=======================================|
|  RECORDED            FINAL VIDEO      |    |  RECORDED            FINAL VIDEO      |
|  0:00:00             0:00:00          |    |  0:12:03             0:12:03          |
|                                       |    |  THIS STRETCH  12:03                  |
| +-----------------------------------+ |    | +-----------------------------------+ |
| | (o)  Record with Timelapse Switch | |    | | #>>#  Switch to timelapse x4      | |  armed key: accent
| +-----------------------------------+ |    | +-----------------------------------+ |  inner frame
|  START IN                             |    |  NEXT SPEED                           |
| [ >  REAL TIME  ][(>> TIMELAPSE)]     |    | [ x2 ][(x4)][ x8 ][ x16]              |
|  TIMELAPSE SPEED                      |    |  ===================================  |
| [ x2 ][(x4)][ x8 ][ x16]              |    | [ []  Stop and save                 ] |
|                                       |    | File: 2026-10-15 19-30-00 tswitch.mp4 |
| (i) Separate file. OBS's Start ...    |    +---------------------------------------+
+---------------------------------------+

 Recording, timelapse                        Error (E103)
+-- Timelapse Switch ------------ [][x] +    +-- Timelapse Switch ------------ [][x] +
|#######################################|    |:::::::::::::::::::::::::::::::::::::::|
|# (o) REC                    0:31:40  #|    |: (X) ERROR                           :|  dashed danger frame
|# TIMELAPSE                      x4   #|    |: Recording stopped                   :|
|# > > > > > > > > > > > > > > > > >   #|    |:                                     :|
|#######################################|    |:::::::::::::::::::::::::::::::::::::::|
|  RECORDED            FINAL VIDEO      |    |  RECORDED            FINAL VIDEO      |
|  0:31:40            |0:16:58          |    |  1:23:45             0:31:10          |
|  THIS STRETCH x4  19:37 -> 4:54       |    |                                       |
| +-----------------------------------+ |    | +-----------------------------------+ |
| | >   Back to real time             | |    | | (o)  Record with Timelapse Switch | |
| +-----------------------------------+ |    | +-----------------------------------+ |
|  SPEED NOW                            |    | +- ERROR TSW-E103 -----------------x+ |  ticket card
| [ x2 ][#x4#][ x8 ][ x16]              |    | | Everything up to 1:23:45 is saved | |
|  ===================================  |    | | (0:31:10 of final video).         | |
| [ []  Stop and save                 ] |    | | Why: the file could not be ...    | |
| (i) Audio: silence during timelapse   |    | | What to do: check the file ...    | |
+---------------------------------------+    | | [ Open folder ]                   | |
                                             | | Copy details   Learn more         | |
                                             | +-----------------------------------+ |
                                             +---------------------------------------+
```

**Sketch, narrow (220 px, Spanish, timelapse).**

```
+-- Timelapse Switch --- [][x] +
|##############################|
|# (o) GRABANDO       0:31:40 #|
|# TIMELAPSE             x16  #|  auto-fit, down to 1.6b
|# >>>>>>>>>>>>>>>>>>>>>>>>>> #|  x16: chevrons packed tight
|##############################|
| GRABADO      DURACIÓN FINAL  |
| 0:31:40     |0:16:58         |
| ESTE TRAMO x16 19:37 -> 1:13 |
| +--------------------------+ |
| | >  Volver a tiempo real  | |
| +--------------------------+ |
| VELOCIDAD ACTUAL             |
| [ x2 ][ x4 ][ x8 ][#x16#]    |
| ============================ |
| [ []  Detener y guardar    ] |
+------------------------------+
```

**Layout and spacing.** The dock has no outer margin at the top, left and right: the strip spans the dock's full width, right under the title bar. Every other group has side margins `s3` (8 px). Strip padding `2.5u` (10 px) left and right, `s3` top and bottom; tally line to hero `s1`; hero to band `s2`. Gaps: strip to B `s4`; B to C `s4`; C to D caption `s4`; caption to keys `s2`; D to the rule `s4`; rule to Stop `s4`; Stop to F `s3`. The rule above Stop is 2 px in `edge`, full width inside the margins. Gearbox keys are 2 px apart.

**Typography.** Strip tally 0.85b Bold caps, its time 0.85b Bold digits, right aligned. Hero 2.75b Bold caps (floor 1.6b; Large 3.5b); the speed (×4) is part of the hero line, right aligned, same size. Quiet hero 1.5b Regular (error headline 1.5b Bold). Readout captions 0.75b Bold caps `muted`; digits 2.0b Bold (auto-fit down to 1.5b). Status line: its label 0.8b Bold caps `muted`, its digits 0.9b. Main key label 1.15b Bold. Gearbox numerals 1.5b Bold, the × sign at 0.8 of that size. Start-mode keys 0.9b Bold caps. Stop 1.0b Bold. Card header 0.75b Bold caps; body 1.0b. The strip, the readout digits, the keys and the gearbox paint their text; the rest uses widget style sheets.

**Shapes.** Every corner is 2 px. Keys and bars are rectangles; the only round things are the record dot and the focus ring's corners.

**The strip (A), by state.** Fixed height (at the defaults: 8 + 15 + 2 + 50 + 4 + 14 + 8 = about 101 px).

| State | Fill | Frame | Tally line | Hero line | Tape band |
|---|---|---|---|---|---|
| Idle | `surface` | none; 1 px `hairline` at the bottom | READY, `ink(muted, surface)` | quiet hero with play or fast-forward glyph | empty: one 1 px `hairline` line |
| Real time | none (panel) | 2 px `muted`, inside the strip | record lamp in `danger` + REC in `text`; time | REAL TIME, `text` | a 1 px `muted` line with play glyphs every 3 band heights |
| Timelapse | `accentFill` | 2 px `accent` along the bottom edge (keeps 3:1 in Classic and Grey) | record lamp (ring in `onAccent`) + REC in `onAccent`; time | TIMELAPSE and ×4, `onAccent` | chevrons in `onAccent` at 40 % opacity, pitch by speed: ×2 1.6, ×4 1.2, ×8 0.9, ×16 0.7 band heights |
| Starting, saving | `surface` | 2 px dashed `muted` | busy glyph + STARTING or SAVING | quiet | a progress line in `accent` sweeping for at most 5 s, then static |
| Summary | `surface` | none | check `shape(success, surface)` + SAVED | quiet, File ready | the speed map, when the recorder provides sections; else empty |
| Error | `surface` | 2 px dashed `danger` | error glyph `shape(danger, surface)` + ERROR | headline, Bold | empty |

The band height is `0.8 L`. Its texture is the one decoration that also carries meaning: even without reading, denser chevrons mean faster. The record lamp on the timelapse strip is the `danger` dot with a 1.5 px ring in `onAccent`, because red on the accent fill is about 1:1 (Default 1.01, Light 1.60); the ring is 4.59 to 5.86:1. Strip fill against the panel: 3.00 to 3.95:1, plus the bottom border in Classic (2.49 fill, 3.05 border) and Grey (2.92, 3.06).

**Readout (B).** Two columns of big digits. In timelapse, the FINAL VIDEO column gets a 3 px bar on its left in `accent` (the `|` in the sketch): the slower counter is marked by shape. Status line below.

**Main key (C).** A painted `TswButton`, full width, `max(3.0 L, lines x L + 2 x s3)` (54 px at the defaults; taller than the 2.5 L minimum, on purpose), corners 2 px. Glyph `1.3 L` on the left, label after it.

| Action | Face | Frame | Glyph | Label |
|---|---|---|---|---|
| Record with Timelapse Switch | `raised` | none | record, `shape(danger, raised)` | `text` |
| Switch to timelapse ×4 (armed) | `raised` | 2 px inside, `shape(accent, raised)` | fast forward, `shape(accent, raised)` | `text` |
| Back to real time | `raised` | none | play, `text` | `text` |
| Disabled | `surface` | none | `muted` at 60 % | `muted` at 60 % |

Hover: `hover(raised)`; pressed: `pressed(raised)` and the content 1 px down, like a key that travels.

**Gearbox (D).** Four `TswSegment` keys, equal width, 2 px apart, height `max(2.2 L, hC)`:

| Key | Face | Marker | Numeral |
|---|---|---|---|
| Engaged | `accentFill`, 2 px `accent` border | a 3 px bar along the top edge in `onAccent`, like a lit key | `onAccent`, Bold |
| Armed | `raised` | 2 px inner frame `shape(accent, raised)` | `text`, Bold |
| Other | `raised` | none | `text`, Regular |

Start mode (idle) uses two keys of the same kind above the speeds: REAL TIME with the play glyph, TIMELAPSE with fast forward. Real time selected is drawn with a 2 px inner frame in `text`; Timelapse selected is drawn armed. In narrow docks the glyph goes above the word.

**Stop (E).** A bar below the rule, full width, `max(2.0 L, hC)`, corners 2 px, no fill, 1 px `edge` frame, stop glyph and Bold label in `text`. The hold fill grows from the left as `dangerTint` with a 2 px leading edge in `shape(danger, dangerTint)`.

**Cards ("tickets").** Rectangular, corners 2 px, 1 px `edge` frame, `surface` fill, below the main key at `s4`. A header line in 0.75b Bold caps `muted` (summary: SESSION; error: ERROR and the code, e.g. ERROR TSW-E103) with the dismiss key at its right. Summary: the waiting saved as a big figure ("52:35" in 2.0b Bold, "skipped" in 0.8b caps under it), the file name, the delight line, then Open folder and Play as keys (`raised`, `1.8 L`). Error: reassurance (Bold), "Why:", "What to do:", the fix key, and Copy details and Learn more as flat text keys in `accentText`.

**Context line (F).** As in Native.

**Narrow.** Strip padding `s3`; the hero auto-fits down to 1.6b with the speed still on its line ("TIMELAPSE ×16" fits at 2.0b in 204 px); readout digits auto-fit down to 1.5b; gearbox numerals 1.3b; start-mode glyphs above their words; ticket actions stack.

**Motion.** After each switch to timelapse or speed change, the chevrons drift to the right at one pitch per 1.5 s for 5 s, then stop (UX-063). The strip cross-fades in 120 ms on a state change. Reduce motion makes both instant. Hold fill always.

**Accessibility.** As in 3.11. The strip is one painted widget with the state as its accessible name; the gearbox keys are real buttons with names and the armed or engaged description.

**Per-theme notes.** Acri: the strip is a bright blue (#4760B2) band across Acri's near-black dock; strong, but the same hue as Acri's accents. Rachni: a rose strip (#A45674) with white text, keys in neutral `raised` grey. Light: a clear blue strip with near-black text (5.21:1); the hollow real-time strip has a 2 px grey frame (#646464). Classic: the strip fill is a calmer steel blue (#3A79B1) with a lighter bottom border. System: same shapes, desktop colors (Breeze: #3080AA strip with black text, 4.79:1).

**Strengths and risks.** The fastest to read and the clearest at distance; the tape band makes the speed felt, not only read. It is the boldest look: next to OBS's quiet docks the solid strip is loud, by design, while timelapse runs, and the dock is taller (about 85 px more than Native at the defaults).

---

## 5. Choosing

All three meet the baseline, so the choice is about character, not safety. A fair way to choose:

1. **Build the three prototypes** behind one switch (a hidden setting or an environment variable read only in tests), so the same build shows each.
2. **Run the theme harness** for each: 7 themes x 2 languages x 3 widths, plus the Font Size and Density matrix. All checks of 3.13 must pass.
3. **Glance test (T2)** with 5 people: show the dock for one second at 1.5 m and at 3 m, in Default, Acri, Rachni and Light, in color and in grayscale; ask "recording? which speed?".
4. **Look at it docked next to Controls** for a whole session. The one that still feels right after an hour wins ties.

Expected trade-off: Native is the safest bet for an official feel and the least code; Studio cards is the most beautiful at desk distance; Transport wins the glance test at distance. The baseline lets the author also mix: for example, Transport's tape band inside Native's well.

---

## 6. Changes to `ux-spec.md`

Made in the same commit as this document:

| Where | Change |
|---|---|
| Header | version 0.2; points to this document for the visual layer |
| Section 1, principles 1 and 6 | the two signals (tally and mode); colors mixed from the theme with a contrast guard |
| 2.5, target T5 | Spanish at 220 and 260 px, measured against the visible area |
| 3.2 | zones grouped in bands; fixed positions defined; cards below C |
| 3.4 | mockups redrawn to the baseline (tally and hero, readout, rails, Stop, cards below C, compact keeps verbs) |
| 3.5 | labels table follows section 3.3 of this document |
| 3.6 | sizes applied through style sheets or painting; measured baseline (the banner is 1.0b in Yami today); per-direction targets |
| 3.7 | the System theme is hidden in OBS 32 unless selected or High Contrast is on; the measured contrast of theme colors |
| 3.8 and 3.9 | the idle wording ("Starts in timelapse x4"); the welcome card opens below the main slot |
| 3.13 | summary mockup with the card below the main button |
| 3.14 | UX-002, UX-003, UX-004, UX-006, UX-007, UX-008, UX-010, UX-011, UX-012, UX-016, UX-018, UX-019 amended; UX-015 added at the end of the list (the number was free) |
| 4.1 and 4.2 | error location (banner headline, card below C); status colors only on glyphs; template order and the no-internal-names rule |
| 4.6 | UX-081 and UX-082 amended |
| 6.1 | UX-101, UX-102, UX-103, UX-104, UX-105, UX-107 amended; UX-110 and UX-111 added at the end |
| 6.2 | growth table uses the new keys; UX-121 amended (no noun instead of a verb) |
| 7 | open questions 15 (speed chips in real time) and 16 (which direction) |

---

## 7. Sources

- The theme research, the HCI audit and the theme render harness of this design round (2026-10-16). The harness is in the repository (`tests/ui/obs-themes.sh`, `tests/ui/README.md`); the research and audit notes, with their resolved style sheets and contrast tables, stayed outside it.
- OBS 32.2.2 source: theme engine `frontend/OBSApp_Themes.cpp`, dock wrapper `frontend/docks/OBSDock.cpp`, Controls dock `frontend/widgets/OBSBasicControls.cpp`, themes in `frontend/data/themes/` (https://github.com/obsproject/obs-studio/tree/32.2.2).
- OBS issue #11498 (System theme unsupported): https://github.com/obsproject/obs-studio/issues/11498
- WCAG 2.2, contrast (relative luminance), 1.4.1 Use of Color, 1.4.11 Non-text Contrast, 2.2.2 Pause, Stop, Hide, 2.5.8 Target Size: https://www.w3.org/TR/WCAG22/
- Qt 6: Qt Style Sheets reference, `QWidget::render`, `QAccessibleAnnouncementEvent` (Qt 6.8): https://doc.qt.io/qt-6/
- Character size for distance reading (16 and 20 arcminutes): see `ux-spec.md` [S50].
- Open Sans metrics: measured from the fonts OBS ships (`OpenSans-Regular.ttf`, `OpenSans-Bold.ttf`).
