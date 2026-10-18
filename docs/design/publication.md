# Publication design: version 1.0.0, the website and support

*Date: 2026-10-18. Status: approved in conversation, to be reviewed in writing by the maintainer.*

How Timelapse Switch goes public: what the repository, the first release, the website and the support channels contain, and what stays with the maintainer.

## 1. Goals

- Publish version **1.0.0** now: tested on Linux; Windows and macOS builds published as **beta**, clearly labelled "not tested yet, please report problems".
- A repository that looks and works like a maintained project: clear README, community files, automatic releases with checksums and SBOM.
- A website in **English only**, built with Astro and Starlight (ADR-010), published on GitHub Pages, that serves the error pages the plugin already links to.
- Ways for users to support the project, open source friendly, starting with the simplest.

Not goals for 1.0.0: the welcome card, the settings dialog, the checks before recording (they follow in 1.x).

## 2. Repository and release 1.0.0

| Piece | Content |
|---|---|
| `README.md` | Logo, a short demo animation, badges (release, license, build), one-line value, downloads per system (Linux tested; Windows, macOS beta), how it works, cost, "Support the project", the existing AI disclosure and non-affiliation lines |
| `CODE_OF_CONDUCT.md` | Contributor Covenant 2.1, contact by email |
| `.github/ISSUE_TEMPLATE/` | Bug report (asks for the OBS log), feature idea, Windows or macOS report (beta feedback); `config.yml` points questions to Discussions |
| `.github/pull_request_template.md` | Checklist: `tools/check.sh`, tests, Conventional Commit title |
| `.github/FUNDING.yml` | GitHub Sponsors and Ko-fi |
| release-please | Config, manifest and workflow (ADR-008); first release forced with `Release-As: 1.0.0`; the build job attaches the three systems' packages, checksums and the SBOM (`tools/make-sbom.sh`, ADR-009) |
| `CHANGELOG.md` | Written by release-please from Conventional Commits |
| Status line | The README's "early development" note is replaced by the support status per system |

## 3. Website

- **Where:** folder `site/` in this repository; the workflow `pages.yaml` builds it with Astro's official action and publishes it to `https://miguetronic.github.io/timelapse-switch/`.
- **Language:** English only (ADR-010 amended).
- **Pages:**
  - Home: value line, demo animation, how it works (the fast-forward pedal), download buttons per system, support.
  - Guide: install and uninstall, first steps, hotkeys, pause, performance, questions and answers. Written from `docs/` (install, performance, faq) so the two never disagree; the website is the user's version, `docs/` the maintainer's.
  - Errors: one page per `TSW-Exxx` code at `/errors/TSW-Exxx` (the addresses the plugin links to; case kept).
  - Support the project.
  - About: Miguetronic, license, non-affiliation with the OBS Project.
- **Rules:** no cookies, no analytics, no third-party scripts; search with Pagefind; light and dark themes; brand colors and logo from the brand kit.

## 4. Support (public part)

- **GitHub Sponsors:** monthly tiers and one-time amounts chosen by the sponsor. The tiers shown are examples; the maintainer sets the real ones when enrolling.
- **Ko-fi:** one-off donations without a GitHub account.
- **Sponsoring work:** a section explaining that a feature or a platform (for example, testing and polishing Windows or macOS) can be funded, and that priority support can be arranged, by GitHub Sponsors or by email.
- Shown in the README, the website's support page, and the "Sponsor" button GitHub builds from `FUNDING.yml`. Nothing in the plugin itself asks for money.

## 5. Demo and release material

- A synthetic demo made with the sandbox OBS: a neutral scene (an analog clock and a timer) recorded through the plugin, switching between real time and timelapse, plus the dock in OBS's themes. Nothing personal in it.
- A social preview image (1280 x 640) and the release notes text.

## 6. What the maintainer does

1. Create the GitHub repository `miguetronic/timelapse-switch`.
2. Review the local commits, then push (or authorize the push).
3. Enroll in GitHub Sponsors and Ko-fi; set the real tiers.
4. Enable GitHub Pages (source: GitHub Actions) and private vulnerability reporting.
5. Merge the release pull request that release-please opens.

## 7. Kept outside the repository

Some project notes are kept outside the repository. Before every commit, a confidentiality search runs on the repository and on the built website.

## 8. Verification

- `tools/check.sh` passes; the website builds with no broken links, and every `TSW-Exxx` page the plugin links to exists.
- The workflows are checked with `actionlint` locally; release and Pages run for real only after the push.
- The confidentiality search finds nothing in the repository or in `site/dist`.
