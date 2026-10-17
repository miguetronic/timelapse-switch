# UI tests

The dock drawn without OBS, in every state, so a person can look at it and a program can check it. There are two ways to run it:

| | Plain look | OBS themes |
|---|---|---|
| Command | `ctest --test-dir build_x86_64` (part of `tools/check.sh`) | `tests/ui/obs-themes.sh` |
| Needs OBS | no | yes, Linux, OBS 31.1 or later |
| Look | Qt's own Fusion look | exactly as OBS draws the dock, in each of its 7 themes |
| Pictures | `build_x86_64/ui-screenshots/*.png` | `build_x86_64/ui-screenshots/<theme>/` and `.../contact-sheets/` |

Both draw the same 8 states (idle, idle starting in timelapse, real time, timelapse, saving, summary, two errors), in English and Spanish, 300, 260 and 220 px wide, and run the same checks (`docs/design/visual-design.md`, 3.13, and the top of `test-dock.cpp`):

| Check | Fails when |
|---|---|
| Texts | the dock asks for a key the locale file lacks |
| Zones and fixed positions | a zone is not below the one before it; the main button moves between states, or Stop between real time, timelapse and saving |
| Fit | a visible widget ends past what the scroll area shows, or the dock cannot shrink to 200 px |
| Nothing cut | a label or painted block is shorter than its lines, a painted text does not fit, or the hero falls below its floor (1.5 times the base size) |
| Contrast | a token pair is under 3:1 (shapes) or 4.5:1 (text), or a label is colored with a status color |
| Words and focus | the main button is named like a state, or Tab goes back up the dock |
| Resize | a dock narrowed (or widened) by hand differs from one made at that width |
| Stop and the card | they show when they should not, or the other way round |

## Why the themes need their own pictures

A theme is not only a set of colors. OBS turns it into a Qt style sheet that also sets fonts, sizes, paddings, borders and corner radii for every widget. For example, the Yami themes give every widget 10 pt Open Sans; that overrides the larger font the dock asks for its banner, so in OBS the banner word is drawn at the normal size. The plain pictures cannot show that; the theme pictures do.

## How the theme pictures are made

Think of a photographer's color chart: first we write down exactly how OBS dresses its widgets, then we dress the dock the same way, without OBS.

```
  1. obs-themes.sh starts a sandbox OBS            2. test-dock (Qt offscreen, no OBS)
     for each theme and each language
  +--------------------------------------+       +---------------------------------------+
  | the theme, the plugin, theme-probe   |       | applies the dump in OBS's order:      |
  | once OBS has loaded, the probe       | dump  | OBS's fonts, "theme:" paths, Fusion,  |
  | writes down: style sheet, palette,   |------>| desktop font, palette, style sheet    |
  | fonts, OBS's built-in pictures, and  | obs-  | puts the dock in an OBSDock docked in |
  | OBS's own dock, idle (a picture)     | themes| an OBSBasic, draws every state, and   |
  | then the script closes OBS           |       | compares the idle one with OBS's own  |
  +--------------------------------------+       +---------------------------------------+
                                                     |                         |
                                         ui-screenshots/<theme>/      ui-screenshots/
                                         one picture per case         contact-sheets/
```

Nothing is recorded: each sandbox OBS only starts, writes the dump and closes, about 2 to 3 seconds each.

### Running it

```
tests/ui/obs-themes.sh                 the dumps (14 short OBS starts), then the pictures: about 45 s
tests/ui/obs-themes.sh --render-only   the pictures again from the last dumps: a few seconds, no OBS
tests/ui/obs-themes.sh --matrix        Default, Light and System at Font Size 8 (Classic density),
                                       10 (Normal) and 12 (Comfortable): 18 short OBS starts
tests/ui/obs-themes.sh --matrix --render-only   the matrix pictures again, no OBS
```

The matrix goes to `build_x86_64/obs-themes-matrix/` and `build_x86_64/ui-screenshots-matrix/`, one folder per theme and size, such as `Yami.Light-12pt-comfortable`.

`--render-only` is the one to use while working on the dock: change `src/ui/dock.cpp`, build, run it, and look at the contact sheets. The dumps only need to be made again when OBS or its themes change.

The test program can also be run by hand:

```
QT_QPA_PLATFORM=offscreen build_x86_64/tests/ui/test-dock data/locale build_x86_64/ui-screenshots build_x86_64/obs-themes
```

### What comes out

| Path | What |
|---|---|
| `build_x86_64/obs-themes/<theme>/` | the dump of one theme (see `theme-probe.cpp` for every file) |
| `build_x86_64/ui-screenshots/<theme>/<lang>-<width>-<state>.png` | the dock in one state, with OBS's dock title bar, as tall as its content plus some spare room (a dock in OBS is usually taller than what it shows) |
| `build_x86_64/ui-screenshots/contact-sheets/<theme>-<lang>.png` | every state of one theme side by side on the theme's window color: 300 px on top, 220 px below, and OBS's own idle dock in the last column |

The themes are the ones that come with OBS: `Yami.Original` (shown as "Default" in OBS), `Yami.Classic`, `Yami.Grey`, `Yami.Acri`, `Yami.Rachni`, `Yami.Light` and `System`.

### What makes it faithful

| Detail | Why it matters |
|---|---|
| The same class names: stand-ins called `OBSBasic` and `OBSDock` (`obs-theme.hpp`) | theme rules pick widgets by class name; `OBSDock > QWidget` is what gives a dock its background and border |
| The same fonts: Open Sans copied out of OBS's program | OBS carries its own copy; the system's may differ |
| The same order: fonts, search paths, Fusion, desktop font, palette, style sheet | it is the order of `OBSApp::InitTheme` and `OBSApp::SetTheme` in OBS 32 |
| The desktop's colors and font: KDE's `kdeglobals` is copied into each sandbox | the System theme takes all its colors from the desktop, and the other themes take some |
| OBS's built-in pictures (`:/res/images/...`) copied, and the style sheet pointed at the copies | they live inside OBS's program, which the test cannot open |

### How we know it is faithful

The probe also draws OBS's own dock, idle, in OBS's language, at 300 and 220 px. test-dock draws its idle dock at the same size and compares every pixel, and the fonts:

```
same  Yami.Light en-US 300 px idle: pixel for pixel as OBS draws it
```

On the reference PC (OBS 32.2.2, Qt 6.12, KDE Plasma on Wayland) all 7 themes, in both languages and at both widths, came out identical. The check was also tried the other way round: leaving out the palette, OBS's fonts or the desktop font each makes it fail, so it is not a check that always passes.

## Reading a failure

| Message | What it means |
|---|---|
| `not as OBS draws it: N pixels differ, first at (x, y)` | the test no longer dresses the dock like OBS; compare the two idle pictures in the last columns of the contact sheet |
| `dock font: ..., but OBS draws ...` | a different font, size or weight than OBS; usually a font that did not load |
| `the dock is N px wide but only M px show; the rest is cut off` | a real problem of the dock: in a narrow OBS dock its content is wider than what shows, so its right side is cut |
| `text cut`, `needs N px`, `is not below the zone before it` | the same checks as the plain test, now under the theme |
| `the probe wrote nothing` | the sandbox folder is kept; its OBS log is in `config/obs-studio/logs/` |
| `NOTE ... dpi` | OBS ran at another font resolution than the test; sizes in points then differ in pixels |

## Limits

- **Linux only**, like the integration test: the sandbox relies on `XDG_CONFIG_HOME`. On Windows and macOS OBS uses another base style and other fonts, so these pictures show the Linux look.
- **Only the themes that come with OBS.** Themes you added to your own OBS are not seen, because the sandbox never reads your OBS settings.
- **Other desktops:** on GNOME and others nothing is copied, so the System theme shows Qt's default colors instead of the desktop's.
- **OBS's defaults** for font scale and density (Settings > Appearance) in the default run, and a screen at 1x scale; the dumps record both. `--matrix` covers the extremes of Font Size and Density for three themes.
- **No hover, focus, pressed or moving states:** nobody moves a mouse over the pictures, and each picture is a first view (transitions only run between states the user caused).
- **OBS's own dock only idle:** the probe cannot put the plugin in the other states, so those are trusted because the same code draws them under the same theme.
- **Docked look:** a floating dock gets a slightly different frame from Qt.
- **"Dark" as OBS reports it:** OBS calls the System theme light even on a dark desktop.

## Files

| File | Job |
|---|---|
| `test-dock.cpp` | the states, the checks, the pictures; the theme mode when given a dumps folder |
| `obs-theme.hpp`, `obs-theme.cpp` | reads a dump and applies it like OBS; the `OBSBasic` and `OBSDock` stand-ins |
| `contact-sheet.hpp`, `contact-sheet.cpp` | composes a contact sheet |
| `theme-probe.cpp` | the test-only OBS module that writes a theme down (CMake option `ENABLE_THEME_PROBE`, on by default, Linux only); never shipped |
| `obs-themes.sh` | runs everything above; shares the sandbox with the integration test (`tests/integration/sandbox.sh`) |
