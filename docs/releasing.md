# Versions and releases

How Timelapse Switch is numbered, and how a version goes from `main` to a download on GitHub.

> **Status:** the release automation (release-please) is switched on when the repository is published on GitHub. Until then, nothing is released; commits already follow the format it needs.

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

| Bump | When | Examples |
|---|---|---|
| MAJOR | Something users rely on stops working as before | Settings or hotkeys must be set again; an OBS version, operating system or encoder is no longer supported; files are named differently |
| MINOR | New behavior, nothing existing breaks | A new speed, a new option, a new language |
| PATCH | Fixes only, no new behavior | A bug fix, a crash fix, a corrected translation |

**The first public release is 1.0.0.** Until the plugin works for real, nothing is released, so the first version users see is already a stable one.

## Who decides the number: the commits

Every commit follows [Conventional Commits](https://www.conventionalcommits.org/) (see `CONTRIBUTING.md`), and its type decides the next version:

| Commits since the last release include... | Next version (from 1.2.3) |
|---|---|
| only `fix:` or `perf:` | 1.2.**4** |
| at least one `feat:` | 1.**3**.0 |
| at least one breaking change (`feat!:` or a `BREAKING CHANGE:` footer) | **2**.0.0 |
| only `docs:`, `test:`, `build:`, `ci:`, `refactor:`, `chore:` | no release |

The commit with the biggest impact wins, and a release bumps one number only: five `feat` commits give 1.3.0, not 1.7.0.

## How a release happens: the release pull request

The tool is [release-please](https://github.com/googleapis/release-please). Think of it as a secretary who takes note of every change and keeps a draft of the next release on your desk:

```
   commits reach main:   fix: ...   feat: ...   fix: ...
            |
            v
   release-please keeps ONE pull request open and rewrites it after every push:
     "chore(main): release 1.3.0"
       - buildspec.json  version 1.3.0
       - CHANGELOG.md    new section, grouped into Features and Bug Fixes
            |
            |   when you want to publish: review that pull request and merge it
            v
   tag 1.3.0 + GitHub release + installers for Windows, macOS and Linux
   + checksums + SBOM
```

- What the pull request shows is exactly what will be published. Merging is the only decision.
- Not ready to publish? Leave it open; it keeps updating itself.
- Tags have no "v" in front (`1.3.0`), which is what the OBS template's build workflow expects.
- To force a specific number (for example the first `1.0.0`), add the footer `Release-As: 1.0.0` to a commit.

## Where the version lives

In one place only: the `version` field of `buildspec.json`, written by release-please. CMake puts it into the plugin binary (`PLUGIN_VERSION`, shown in the OBS log), the Windows file properties, the macOS installer and the installer file names. It holds numbers only (`X.Y.Z`), which `tools/check.sh` verifies.

## Changelog

`CHANGELOG.md` is generated from the commit messages; nobody edits it by hand. That is why the first line of every commit is written **for users**: "Speed changes now apply on the next frame", not "refactor valve state".

## What each release contains

| File | What it is | How it is made |
|---|---|---|
| Installers (`.exe`, `.pkg`, `.deb`, archives) | The plugin for each operating system | CI, from the release tag |
| `CHECKSUMS.txt` | SHA-256 of every file | CI (from the OBS template) |
| `timelapse-switch-X.Y.Z.cdx.json` | SBOM, CycloneDX 1.6 | `tools/make-sbom.sh` |

The SBOM can be generated locally at any time with `tools/make-sbom.sh`; the same commit always gives the same file.
