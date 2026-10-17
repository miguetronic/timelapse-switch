# Quality and traceability

How we make sure Timelapse Switch is reliable, and how anyone (a contributor, a user, an auditor) can check it.

> **Honest scope.** This project is not certified against any standard. It follows practices *inspired by* the standards below, as used in industrial software, scaled to a small open source plugin. Where we say "inspired by", that is exactly what we mean.

## 1. What "quality" means here

We use the eight product qualities of **ISO/IEC 25010** as a checklist, with what each one means for this plugin and where the evidence lives.

| Quality (ISO/IEC 25010) | What it means for Timelapse Switch | How we get it | Evidence |
|---|---|---|---|
| Functional suitability | The file comes out with the right sections sped up, nothing more, nothing less | Requirements with IDs, tests per requirement | `docs/requirements.md`, `tests/` |
| Performance efficiency | 4K30 without lost frames on modest hardware, and its cost stated plainly | Real-time rules, measurement before release | REQ-014 and REQ-018, `docs/performance.md` |
| Compatibility | Works next to OBS's own recording and other plugins | Public OBS API only, own recorder | ADR-002, ADR-006 |
| Interaction capability (usability) | Obvious state, hands-free use, clear errors | UX specification with IDs | `docs/design/ux-spec.md` |
| Reliability | Never crashes OBS, never loses a recording | "The Power of Ten" rules, checks that recover | `docs/coding-rules.md`, `tools/check.sh` |
| Security | No surprises: no network, no data collection | No network code; dependencies are OBS's own; private reporting | `SECURITY.md`, SBOM per release |
| Maintainability | Anyone can read and change the code safely | Short functions, contracts, decisions recorded | `docs/coding-rules.md`, `docs/decisions.md` |
| Flexibility (portability) | Windows, macOS and Linux from the same code | CMake per-OS files, CI builds for all three | `.github/workflows/` |

## 2. Standards we follow

| Standard or guide | What we take from it |
|---|---|
| ISO/IEC 25010 | The quality checklist above. |
| ISO/IEC/IEEE 12207 (software life cycle) | A defined path from idea to release: requirement, decision, code, review, test, release (section 4). |
| ISO/IEC/IEEE 29148 (requirements) | Requirements that are numbered, testable, prioritized and with a verification method. |
| ISO/IEC/IEEE 29119 (testing) | Test levels and a test report per release (section 5). |
| ISO/IEC/IEEE 26514 (user documentation) | User docs organized by task, with examples, written for the reader's level. |
| ISO 9241-110 and 9241-210 (usability) | Interaction principles and human-centred design behind the dock (`docs/design/ux-spec.md`). |
| "The Power of Ten" (G. J. Holzmann, NASA/JPL) | The ten coding rules, adapted (`docs/coding-rules.md`). |
| SEI CERT C | Many clang-tidy checks (`cert-*`) that catch dangerous uses of the C library. |

## 3. Traceability: from idea to release and back

Every requirement can be followed forward to the code and tests that implement it, and every line of code can be followed back to the reason it exists.

```
  requirement           decision             code                      test                  release
  REQ-005  --------->  ADR-003  -------->  timeline.c  ----------->  test_timeline.c  -->  CHANGELOG 0.2.0
  "audio stays          "frames pass        /* Implements: REQ-005 */  /* Verifies: REQ-005 */   "Added: ..."
   in sync"              through memory"
       ^                                                                                       |
       +-------------------------------- commit trailer "Refs: REQ-005" ------------------------+
```

**ID families**

| Prefix | What | Where it is defined |
|---|---|---|
| `REQ-` | Functional, compatibility, reliability and performance requirements | `docs/requirements.md` |
| `UX-`, `INS-` | Interface and installation requirements | `docs/design/ux-spec.md` |
| `ADR-` | Decisions | `docs/decisions.md` |
| `P10-`, `B` | Coding rules | `docs/coding-rules.md` |
| `TSW-E` | Error codes shown to users | `docs/design/ux-spec.md` 4.3, later `docs/errors/` |

**How to tag**

- In code, above the function that implements it: `/* Implements: REQ-005, UX-031 */`
- In tests, above the test: `/* Verifies: REQ-005 */`
- In commits, as a trailer line: `Refs: REQ-005`

A script (`tools/trace.py`, planned) will collect these tags into a matrix (`docs/traceability.md`) and report requirements with no code or no test.

## 4. Life cycle of a change

```
  1. Requirement    a REQ or UX item exists (or is added) and is clear
  2. Decision       if there were real options, an ADR records the choice
  3. Code           on a branch, following docs/coding-rules.md
  4. Checks         tools/check.sh all green on the developer's machine
  5. Review         the maintainer reviews with the checklist in coding-rules.md, Part C
  6. CI             GitHub builds for Windows, macOS and Linux, and runs the checks
  7. Merge          into main, which always builds
  8. Release        merge the release pull request (release-please): tag, changelog, installers, SBOM
```

**Definition of done** for any change: steps 1 to 7 complete, docs updated in the same pull request, IDs referenced.

## 5. Testing

| Level | What | How | When |
|---|---|---|---|
| Unit | Pure logic (timeline math, audio share, limits, what each input means, what the dock shows) | C tests with CTest, no OBS needed | Every check run, every push |
| UI | The dock in every state, English and Spanish, 300, 260 and 220 px wide | `tests/ui/test-dock.cpp` with CTest on Qt's offscreen platform, no OBS: every text exists, zones keep their order and their positions, nothing is cut or wider than what shows, the hero never falls below its floor, colors pass the contrast guard, no text in a status color, verbs on the main button, Tab in visual order, a minimum width of 200 px, and a dock narrowed by hand equals one made narrow; a picture of each case for review | Every check run, every push |
| UI in OBS themes | The same states inside each of OBS's 7 themes, exactly as OBS draws them, System also with a light palette; hover, pressed, keyboard focus, disabled and the Stop hold; and Default, Light and Classic at Font Size 8, 10 and 12 with their densities | `tests/ui/obs-themes.sh` (and `--matrix`): a sandbox OBS per theme and language writes the theme down (a test-only probe, nothing is recorded); `test-dock` then applies it, runs the same checks, draws a contact sheet per theme and language and one of the interactions per theme, and compares the idle dock with OBS's own, pixel for pixel (`tests/ui/README.md`) | After each change to the dock's look, and before each release |
| Integration | The plugin inside a real OBS | `tests/integration/run.sh`: a separate OBS records a test media while a script presses the hotkeys; the file is checked frame by frame (`tests/integration/README.md`) | After each change to the recorder, and before each release |
| Sync | Audio and video alignment across switches | Part of the integration test: every beep of the file is compared with where OBS itself had it (sync probe) | Same as integration |
| Platform | Windows, macOS, Linux | CI builds; manual runs by testers | Before each release |
| Load | 4K30, long sessions | `tests/integration/run.sh --resolution 2160 --scenario long` on the reference PC, **only with the maintainer's explicit approval** | Before 1.0 and after performance changes |
| Cost | What recording costs (CPU, RAM, graphics card, bus) with OBS's own Record button and with the plugin, same scene and settings | `tests/integration/measure.sh` on the reference PC, **only with the maintainer's explicit approval**; results in `docs/performance.md` | Before 1.0 and whenever the way frames travel changes (REQ-018) |

Each release gets a short test report (what was run, on what, results), kept in `docs/test-reports/`.

## 6. Main risks and how we handle them

A light version of a failure mode analysis (FMEA): what could go wrong, how we would notice, and what limits the damage.

| Risk | Effect on the user | Detection | Mitigation |
|---|---|---|---|
| Audio drifts from video | Voice and picture out of sync | Sync test (section 5); runtime drift check (TSW-E202) | Timeline math is unit tested; drift bounded to one audio block (REQ-005) |
| Plugin bug in a real-time callback | OBS freezes or crashes, recording lost | P10 rules, two static analyzers, checks that recover | No allocation, locks or I/O in callbacks (REQ-013) |
| 4K too heavy on modest hardware | Lost frames | Lost-frame counter (TSW-E201) | Measurement before release (REQ-014); plan B in ADR-003 |
| Disk fills up | Recording stops | Free-space estimate before and during recording | Warn early (TSW-E102), stop and save cleanly (TSW-E101) |
| User confuses the two record buttons | Recording at the wrong speed | Cross warnings (TSW-E301, TSW-E302) | Distinct labels and file names (`docs/design/ux-spec.md`, section 3) |
| An OBS update changes behavior | Plugin stops working | CI builds against new OBS versions | Public API only (REQ-008) |
