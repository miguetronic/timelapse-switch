# Versions and releases

How Timelapse Switch is numbered, and how a version goes from `main` to a download on GitHub.

## Version numbers: Semantic Versioning

Versions follow [Semantic Versioning 2.0.0](https://semver.org/): three numbers, `MAJOR.MINOR.PATCH`.

Think of it as a promise to users, readable from the number alone:

```
   1 . 4 . 2
   |   |   |
   |   |   +-- PATCH: "only fixes, update without worry"
   |   +------ MINOR: "something new, nothing you rely on changes"
   +---------- MAJOR: "something changed that you may need to adapt to"
```

**What counts as what, for this plugin (from 1.0.0 on):**

| Bump | When | Examples |
|---|---|---|
| MAJOR | Something users rely on stops working as before | Settings or hotkeys must be set again; an OBS version, operating system or encoder is no longer supported; files are named differently |
| MINOR | New behavior, nothing existing breaks | A new speed, a new option, a new language, a new chapter format |
| PATCH | Fixes only, no new behavior | A bug fix, a crash fix, a corrected translation |

**Before 1.0.0** (initial development, where we are now): anything may change. A new feature bumps MINOR (`0.2.0`), a fix bumps PATCH (`0.2.1`). Version `1.0.0` means "stable: the promises above now apply".

## Where the version lives

There is exactly one place: the `version` field of `buildspec.json`. CMake reads it and puts it into the plugin binary (`PLUGIN_VERSION`, shown in the OBS log), the Windows file properties, the macOS installer and the names of the installer files. Nothing else holds a version number, so they can never disagree.

It always holds **numbers only**, `X.Y.Z`, because CMake's `project(VERSION ...)` and the Windows and macOS version fields accept nothing else. `tools/check.sh` verifies this.

## Pre-releases

Before a version is final, testers can try it as a pre-release:

| Form | Meaning | Example |
|---|---|---|
| `X.Y.Z-betaN` | Feature complete, being tested; bugs expected | `1.0.0-beta1` |
| `X.Y.Z-rcN` | Release candidate: becomes the final version unless a serious bug appears | `1.0.0-rc1` |

These are the only pre-release forms the release workflow recognizes (`.github/workflows/push.yaml`, from the OBS template). Keep N between 1 and 9: SemVer compares `beta10` and `beta2` as text, so `beta10` would sort before `beta2`.

The suffix lives **only in the Git tag**. For `1.0.0-beta1`, `buildspec.json` already says `1.0.0` (the version being prepared), the tag is `1.0.0-beta1`, and GitHub shows the release as "Timelapse Switch 1.0.0-beta1", marked as a pre-release. The installer files inside it are named with `1.0.0`; that is how the OBS template works.

Order, from oldest to newest: `1.0.0-beta1` < `1.0.0-beta2` < `1.0.0-rc1` < `1.0.0`.

## Releasing, step by step

```
   main, all checks green
        |
        v
   1. buildspec.json      "version": "X.Y.Z"   (numbers only, also for betas)
   2. CHANGELOG.md        final versions: move the "Unreleased" lines under "## [X.Y.Z] - YYYY-MM-DD"
                          betas and release candidates: leave them under "Unreleased"
   3. test report         docs/test-reports/X.Y.Z.md (see docs/quality.md, section 5)
   4. commit              chore(release): X.Y.Z
   5. tag                 git tag -a X.Y.Z -m "Timelapse Switch X.Y.Z"   (no "v"; X.Y.Z-betaN for a beta)
   6. push the tag        CI builds Windows, macOS and Linux and creates a DRAFT release
   7. review the draft    check the files, paste the CHANGELOG section, then publish
```

**Why the tag has no "v":** the release workflow from the OBS template only accepts tags that are exactly a version (`1.2.3`, `1.2.3-beta1`, `1.2.3-rc1`). Any other tag builds, but no release is created.

**Why a draft:** nothing reaches users until a person has looked at it. The workflow marks betas and release candidates as pre-releases on GitHub automatically.

## Changelog

Every pull request that changes something users can notice adds a line to `CHANGELOG.md`, under `## [Unreleased]`, in one of these groups ([Keep a Changelog](https://keepachangelog.com/en/1.1.0/)): **Added**, **Changed**, **Fixed**, **Removed**, **Deprecated**, **Security**. Lines are written for users, not developers: "Switching speed now takes effect on the next frame", not "refactor valve state".
