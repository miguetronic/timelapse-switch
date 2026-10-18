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
| `.github/FUNDING.yml` | Ko-fi |
| release-please | Config, manifest and workflow (ADR-008); first release forced with `Release-As: 1.0.0`; the build job attaches the three systems' packages, checksums and the SBOM (`tools/make-sbom.sh`, ADR-009) |
| `CHANGELOG.md` | Written by release-please from Conventional Commits |
| Status line | The README's "early development" note is replaced by the support status per system |

## 3. Website

- **Where:** folder `site/` in this repository; the workflow `pages.yaml` builds it with Astro's official action and publishes it to `https://miguetronic.github.io/timelapse-switch/`.
- **Language:** English only (ADR-010 amended).
- **Pages:**
  - Home: value line, demo animation, how it works (the fast-forward pedal), download buttons per system, support.
  - Guide: install and uninstall, first steps, hotkeys, pause, performance, questions and answers. Written from `docs/` (install, performance, faq) so the two never disagree; the website is the user's version, `docs/` the maintainer's.
  - Errors: one page per `TSW-Exxx` code at `/errors/tsw-exxx/` (lowercase, as Astro builds them; the plugin's link matches, ADR-010).
  - Support the project.
  - About: Miguetronic, license, non-affiliation with the OBS Project.
- **Rules:** no cookies, no analytics, no third-party scripts; search with Pagefind; light and dark themes; brand colors and logo from the brand kit.

## 4. Support (public part)

- **Ko-fi:** one-off or monthly tips, without an account.
- **GitHub Sponsors: dropped (18-oct-2026).** Enrolling needs a Stripe account, and Stripe does not serve the maintainer's country yet. If that changes, add `github: miguetronic` back to `FUNDING.yml` and the link to the README and the support page.
- **Sponsoring work:** a section explaining that a feature or a platform (for example, testing and polishing Windows or macOS) can be funded, and that priority support can be arranged, asked in Discussions or through Ko-fi.
- Shown in the README, the website's support page, and the "Sponsor" button GitHub builds from `FUNDING.yml`. Nothing in the plugin itself asks for money.

## 5. Demo and release material

- A synthetic demo made with the sandbox OBS: a neutral scene (an analog clock and a timer) recorded through the plugin, switching between real time and timelapse, plus the dock in OBS's themes. Nothing personal in it.
- A social preview image (1280 x 640) and the release notes text.

## 6. What the maintainer does

In this order, so that every link and workflow works from the first push:

1. Show an email address on the GitHub profile: the Code of Conduct and the support page point to it.
2. Claim the Ko-fi page `miguetronic` before its links are public (GitHub Sponsors dropped, see section 4).
3. Create the GitHub repository `miguetronic/timelapse-switch`, empty.
4. In its settings, before the first push: Actions may read and write and may create pull requests (release-please needs both); Discussions on; Pages from GitHub Actions; private vulnerability reporting on.
5. Review the local commits, then push `main` (or authorize the push). Only `main`: never local backup branches.
6. Push the commit with the footer `Release-As: 1.0.0`, merge the release pull request, check the draft release and its files, and publish it.

## 7. Kept outside the repository

Some project notes are kept outside the repository. Before every commit, a confidentiality search runs on the repository and on the built website.

## 8. Verification

- `tools/check.sh` passes; the website builds with no broken links, and every `TSW-Exxx` page the plugin links to exists.
- The workflows are checked with `actionlint` locally; release and Pages run for real only after the push.
- The confidentiality search finds nothing in the repository or in `site/dist`.
