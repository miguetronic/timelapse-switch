# Versions and releases

How Timelapse Switch is numbered, and how a version goes from `main` to a download on GitHub.

> **Status:** release-please is configured (`release-please-config.json`, `.release-please-manifest.json`, `.github/workflows/release-please.yaml`) and starts working once the repository is on GitHub. The first release is forced to 1.0.0 (see "The first release" below).

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
| `CHECKSUMS.txt` | SHA-256 of every file, also in the release notes | CI |
| `timelapse-switch-X.Y.Z.cdx.json` | SBOM, CycloneDX 1.6 | `tools/make-sbom.sh`, run by CI |

The release notes start with the support status per system (Linux tested; Windows and macOS beta until they are tested), then the changes, then the checksums.

## Two ways a release is built

- **The usual way, release-please:** merging the release pull request makes release-please create the tag and the release. A tag created with GitHub's default token does not start other workflows, so `release-please.yaml` builds the plugin itself (`build-project.yaml`) and attaches the files, the checksums and the SBOM.
- **By hand, a tag pushed by a person** (for example a pre-release `1.1.0-beta1`): `push.yaml` builds it and creates a draft release with the same files.

## The first release

1. Push the repository to GitHub with the release-please files in place.
2. Push one commit with the footer `Release-As: 1.0.0` (for example `chore: release 1.0.0` with that footer); release-please opens the pull request "release 1.0.0".
3. Review it and merge it: the tag `1.0.0` and the release appear, and the files are attached a few minutes later.

The SBOM can be generated locally at any time with `tools/make-sbom.sh`; the same commit always gives the same file.
