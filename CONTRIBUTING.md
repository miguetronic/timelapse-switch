# Contributing to Timelapse Switch

Thank you for helping. This page explains how we work, in five minutes of reading. The details live in the documents linked below, so you only read them when you need them.

## Our five promises

Everything in this project serves these promises to the people who use it:

1. **Your recording is safe.** The plugin never crashes or freezes OBS, and if something fails, the file is saved and you are told what happened.
2. **Anyone can read the code.** Short functions, plain names, step-by-step comments. A curious non-expert should be able to follow the flow.
3. **Every line has a reason.** Requirements, decisions, code, tests and releases are linked by IDs, so you can always ask "why?" and get an answer.
4. **Feedback is honest.** No silent failures: every problem has a code, a log line and a clear message.
5. **It works everywhere OBS works.** Windows, macOS and Linux, from the same code.

And three habits that keep the code small:

- **Parsimony:** solve today's problem with the least code that is still clear. No features or abstractions "for later".
- **DRY (Don't Repeat Yourself):** when the same logic appears twice, it becomes one function.
- **Clean code:** names say what things are, functions do one thing, comments explain why.

## Map of the documentation

| I want to... | Read |
|---|---|
| Understand what the plugin does and how | `README.md`, then `docs/architecture.md` |
| Build it and run the checks | `docs/building.md` |
| Write code | `docs/coding-rules.md` (includes "The Power of Ten", adapted) |
| Know why something is the way it is | `docs/decisions.md` |
| Know what it must do | `docs/requirements.md` and `docs/design/ux-spec.md` |
| Understand quality, tests and traceability | `docs/quality.md` |
| Number a version or publish a release | `docs/releasing.md` |
| Report a security problem | `SECURITY.md` |

## How a change flows

```
   idea or bug
       |
       v
   1. Is there a requirement (REQ-/UX-)?  no --> add or update it first
       |
       v
   2. Real options to choose from?  yes --> record the choice in docs/decisions.md
       |
       v
   3. Branch from main:  feat/short-name  or  fix/short-name
       |
       v
   4. Code + tests + docs in the same branch, following docs/coding-rules.md
       |
       v
   5. tools/check.sh  -->  all PASS
       |
       v
   6. Pull request with the checklist (docs/coding-rules.md, Part C)
       |
       v
   7. Review + CI on Windows, macOS, Linux  -->  merge into main
```

`main` must always build. Nothing reaches it without green checks.

## Commit messages

Every commit follows [Conventional Commits](https://www.conventionalcommits.org/): a type, an optional scope, then one line **written for users**, because it becomes a line of the changelog. Then details if needed, then the IDs:

```
feat: speed changes apply on the next frame

Before, a new speed waited for the next timelapse section.

Refs: REQ-002, UX-041
```

| Type | Use it for | Effect on the version |
|---|---|---|
| `feat` | Something new for users | MINOR |
| `fix` | A bug fix | PATCH |
| `perf` | Faster or lighter, same behavior | PATCH |
| `docs` | Documentation only | none |
| `test` | Tests only | none |
| `build` | Build system, tools | none |
| `ci` | GitHub workflows | none |
| `refactor` | Code change with no change in behavior | none |
| `style` | Formatting only | none |
| `chore` | Housekeeping | none |
| `revert` | Undoes an earlier commit | depends on what it undoes |

- **Breaking changes** add `!` after the type (`feat!: new settings format`) or a `BREAKING CHANGE:` footer. They bump MAJOR.
- **Scope** is optional and names the area: `fix(audio): ...`, `feat(dock): ...`.
- **First line:** 72 characters at most, no period at the end.
- **Pull requests** are merged with "Squash and merge", so the pull request title becomes the commit: it follows the same format.

A Git hook checks the format before every commit. Enable it once per clone:

```bash
git config core.hooksPath tools/git-hooks
```

`tools/check.sh` also checks every commit in the history.

## Versions and changelog

- Versions follow [Semantic Versioning](https://semver.org/), `MAJOR.MINOR.PATCH`. The first public release is 1.0.0.
- **Nobody picks numbers or edits the changelog by hand.** release-please reads the commit types, keeps a release pull request up to date (new version in `buildspec.json`, new section in `CHANGELOG.md`), and publishes when the maintainer merges it.
- Each release includes installers, checksums and an SBOM (`tools/make-sbom.py`).
- Details: `docs/releasing.md`. Security reports: `SECURITY.md`.

## Writing documentation

Documentation is for people, so:

- Friendly and teaching in tone: explain the idea, then the details, with an example or an analogy.
- Diagrams in ASCII inside code blocks, which display the same everywhere. Use Mermaid only when ASCII really can't show it, because GitHub does not always display Mermaid well.
- Short sentences; jargon explained the first time it appears.
- **No em dashes or en dashes.** Use a period, comma, colon or parentheses. `tools/check.sh` checks this.
- Everything in English. The interface is translated into Spanish too.

## How this project is made

Most of the code is written with an AI coding assistant (Claude, by Anthropic) and reviewed by the maintainer, Miguel Parra. The rules and checks in this repository apply to every line, whoever or whatever wrote it.

## License

By contributing, you agree that your contribution is licensed under the GNU General Public License, version 2 or later (`LICENSE`), like the rest of the project and like OBS Studio itself.
