# Publication Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Everything needed to publish Timelapse Switch 1.0.0 on GitHub: repository files, release pipeline, an English website on GitHub Pages, support channels and a synthetic demo.

**Architecture:** The website lives in `site/` (Astro + Starlight) and is published by its own workflow; release-please drives versions and the existing build workflow attaches packages, checksums and the SBOM.

**Tech Stack:** Astro 5 + Starlight, Node 22, GitHub Actions, release-please, actionlint (via pip), the existing sandbox OBS tools.

**Spec:** `docs/design/publication.md`

## Global Constraints

- Version **1.0.0**; Linux "tested", Windows and macOS "beta: not tested yet, please report problems".
- Website: **English only**, `https://miguetronic.github.io/timelapse-switch/`, no cookies, no analytics, no third-party scripts.
- Error pages at `/errors/tsw-exxx/` (lowercase, as Astro builds them); the plugin's link changes to match (ADR-010 amended). Codes: E001-E006, E101-E106, E201-E203, E301-E304, E901.
- Support: GitHub Sponsors (`miguetronic`) and Ko-fi (`miguetronic`); nothing in the plugin asks for money.
- Confidentiality: the maintainer's private search list finds nothing in the repository nor in `site/dist`; it blocks every commit.
- No em or en dashes; Conventional Commits with titles of 72 characters or less, chained with `&&`.
- Nothing is pushed: the maintainer pushes.

## Review Focus

- A user clicks "Learn more" on any error in the dock: the page must exist. Task 4 checks every code in the plugin against the built site.
- A visitor on a phone or with dark mode: Starlight's defaults, checked by building and by screenshots in Task 4.
- Someone on Windows or macOS downloads 1.0.0: the README, the site and the release notes say "beta" before the download button (Tasks 1, 4).
- A sponsor link while the maintainer has not enrolled yet: GitHub hides the button until Sponsors is active; Ko-fi links work only once the page exists. Listed in the hand-over (Task 6).
- A commit with private material: the confidentiality search runs on `site/dist` too (Task 5).

---

### Task 1: Repository front door and community files

**Files:**
- Modify: `README.md`
- Create: `CODE_OF_CONDUCT.md`, `.github/FUNDING.yml`, `.github/ISSUE_TEMPLATE/bug.yml`, `.github/ISSUE_TEMPLATE/idea.yml`, `.github/ISSUE_TEMPLATE/beta-platform.yml`, `.github/ISSUE_TEMPLATE/config.yml`, `.github/pull_request_template.md`
- Create: `docs/images/` (logo SVGs copied from the brand kit's `export/`: `logo.svg`, `logo-dark.svg`, `symbol.svg`)

- [ ] **Step 1:** README: logo with `<picture>` for light and dark, badges (release, license GPL-2.0-or-later, build), replace the "early development" note with a downloads table (Linux tested; Windows, macOS beta), keep "How it works" and "What it costs", add "Support the project" (Sponsors monthly or one-time, Ko-fi, funding a feature or priority support by email), keep the AI disclosure and non-affiliation lines.
- [ ] **Step 2:** `FUNDING.yml` with `github: miguetronic` and `ko_fi: miguetronic`; Contributor Covenant 2.1 with the maintainer's contact address from `SECURITY.md`; issue forms (the bug form requires the OBS log and the system; the beta form asks which system and what happened); `config.yml` sends questions to Discussions; the PR template lists `tools/check.sh`, tests, title format.
- [ ] **Step 3:** Run `tools/check.sh` (16 PASS) and the confidentiality search (no output).
- [ ] **Step 4:** Commit `docs: README, community files and support channels for 1.0.0`.

### Task 2: Release pipeline

**Files:**
- Create: `release-please-config.json`, `.release-please-manifest.json`, `.github/workflows/release-please.yaml`
- Modify: `.github/workflows/push.yaml` (attach checksums and SBOM), `tools/requirements-dev.txt` (actionlint-py), `tools/check.sh` (a step "workflows: actionlint")

**Interfaces:** release-please updates `buildspec.json` (`$.version`, jsonpath) and `CHANGELOG.md`; the tag has no "v" (`docs/releasing.md`).

- [ ] **Step 1:** Add actionlint to `check.sh` and run it: the current workflows pass (or their findings are fixed).
- [ ] **Step 2:** release-please config: release type `simple`, extra file `buildspec.json` by jsonpath `$.version`, `include-v-in-tag: false`, `bootstrap-sha` = current main; manifest `{".": "0.1.0"}`; workflow on push to main. The first release is forced by a commit with footer `Release-As: 1.0.0` (Task 6 hand-over).
- [ ] **Step 3:** In `push.yaml`, after the packages are built for a tag, run `tools/make-sbom.sh` and `sha256sum` on every package, and attach both to the draft release.
- [ ] **Step 4:** `tools/check.sh` passes, actionlint included; update `docs/releasing.md` with the exact sequence. Commit `build(release): release-please and release assets for 1.0.0`.

### Task 3: Synthetic demo and images

**Files:**
- Create: `tools/make-demo.sh` (records a neutral scene in the sandbox OBS through the plugin, switching x1 → x8 → x1, and builds the media), `site/src/assets/demo/` outputs: `demo.webm`, `demo.gif` (README, under 3 MB), `dock-*.png` (from `tests/ui/obs-themes.sh`), `social-preview.png` (1280 x 640: logo, value line, dock)

- [ ] **Step 1:** The scene: an analog clock and a running timer drawn by a generated video (a clock face and digits rendered with ffmpeg filters), in 1080p, no personal content.
- [ ] **Step 2:** `make-demo.sh` reuses `tests/integration/sandbox.sh`; it is a light run (1080p, under 1 minute) and runs only with `--yes` so it never starts by accident.
- [ ] **Step 3:** Check by eye: the GIF shows the clock hands speeding up and slowing down; the social image reads at 50 % size. Commit `docs: synthetic demo, dock images and social preview`.

### Task 4: Website

**Files:**
- Create: `site/` (Astro + Starlight: `package.json` with exact versions and lock file, `astro.config.mjs` with `site` and `base: '/timelapse-switch'`, theme colors from the brand kit, logo, favicon), pages in `site/src/content/docs/`: `index.mdx` (home), `guide/install.md`, `guide/first-steps.md`, `guide/hotkeys.md`, `guide/pause.md`, `guide/performance.md`, `guide/faq.md`, `support.md`, `about.md`, `errors/tsw-exxx.md` (generated, Step 2)
- Create: `tools/site-errors.py` (writes one page per code from the error table in `docs/design/ux-spec.md`: title, what it means, what to do), `tools/check-site.sh` (build, link validation with `starlight-links-validator`, every code in `src/` has a page)
- Modify: `src/ui/dock.cpp:53` to `errors/tsw-e%1/` (lowercase), `docs/decisions.md` ADR-010 (English only; lowercase error addresses), `docs/design/ux-spec.md` where it gives the address

- [ ] **Step 1:** Write `tools/check-site.sh` first; run it: FAIL (no site).
- [ ] **Step 2:** `site-errors.py`, deterministic output, run by `check-site.sh` and compared with the committed pages (a stale page fails).
- [ ] **Step 3:** Scaffold the site and write the pages from `README.md` and `docs/` (install, performance, faq), in English, for users; home with value line, demo, the fast-forward pedal, download buttons (Windows and macOS marked beta), support; About with license and non-affiliation.
- [ ] **Step 4:** Change the plugin's error link, rebuild, run `ctest` and the dock UI test; `check-site.sh` PASS; screenshots of home, a guide page and an error page in light and dark at 375 px and 1280 px, checked by eye.
- [ ] **Step 5:** Commit `feat(site): English website with guide, error pages and support` and `fix(dock): error links match the website's addresses`.

### Task 5: Pages workflow and site checks in CI

**Files:**
- Create: `.github/workflows/pages.yaml` (on push to main touching `site/` or `docs/`: build with `withastro/action`, deploy with `actions/deploy-pages`)
- Modify: `tools/check.sh` (runs `tools/check-site.sh` when Node is present, and the confidentiality search over `site/dist`), `.gitignore` (`site/dist`, `site/node_modules`, `site/.astro`)

- [ ] **Step 1:** actionlint passes on `pages.yaml`; `tools/check.sh` passes with the new steps (now 18 or more). Commit `ci(site): publish the website to GitHub Pages`.

### Task 6: Verification and hand-over

- [ ] **Step 1:** Notes for the maintainer, kept outside the repository.
- [ ] **Step 2:** Final verification: `tools/check.sh` all PASS, `tools/check-site.sh` PASS, confidentiality search over the repository and `site/dist` empty, `git status` clean.
- [ ] **Step 3:** Hand-over to the maintainer, in Spanish: create the repository, push, enroll in Sponsors and Ko-fi, enable Pages (source: GitHub Actions) and private vulnerability reporting, push the `Release-As: 1.0.0` commit, merge the release PR, check the draft release and publish it.
