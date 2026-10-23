# Building Timelapse Switch

This guide takes you from an empty folder to a built plugin, one step at a time. Every command here was run on the maintainer's machine; the timings are real.

## The big picture first

Building a plugin is like baking from a recipe card:

```
   CMakeLists.txt + presets         cmake --preset ...          cmake --build ...
   (the recipe card)        ---->   (read the recipe, check  ---->  (actually cook:
                                     the kitchen, write a            compile and link)
                                     shopping list = build/)              |
                                                                          v
                                                       build_x86_64/rundir/.../timelapse-switch.so
                                                       (the plugin OBS loads)
```

- **CMake** reads the recipe (`CMakeLists.txt`) and prepares a build folder. This is *configuring*, done once.
- **Ninja** does the actual compiling, only of what changed. This is *building*, done after every change.
- **`tools/check.sh`** is the quality inspector: format, warnings, two static analyzers and the project rules, all at once.

## Linux (tested on CachyOS / Arch with OBS 32.2.2)

### 1. Install the tools (once)

| Tool | Why | Arch / CachyOS package |
|---|---|---|
| CMake 3.28 or newer | Reads the recipe | `cmake` |
| Ninja | Compiles fast | `ninja` |
| GCC | The C and C++ compiler | `gcc` |
| OBS Studio with its headers | The API we build against | `obs-studio` |
| Qt 6 | The dock's toolkit | `qt6-base` |
| clang-tidy | Static analyzer #1 | `clang` |
| Python 3 | Runs the pinned formatting tools | `python` |
| jq, uuidgen | Generate the SBOM (`tools/make-sbom.sh`) | `jq`, `util-linux` |
| wixl, zip (optional) | Only to try the release files locally (`tools/release-files.sh`): the Windows `.msi` and the Linux `.zip` | `msitools`, `zip` |

On Ubuntu, the names differ; the CI script `.github/scripts/utils.zsh/setup_ubuntu` lists them.

### 2. Get the code and the formatting tools (once)

```bash
git clone https://github.com/miguetronic/timelapse-switch.git
cd timelapse-switch
python3 -m venv .venv
.venv/bin/pip install -r tools/requirements-dev.txt
git config core.hooksPath tools/git-hooks
```

The `python3` and `pip` lines install clang-format 19.1.1, gersemi 0.21.0, ShellCheck and shfmt **inside the project folder** (`.venv/`, ignored by Git). clang-format and gersemi are the exact versions the OBS CI uses, so your formatting and GitHub's always agree. Nothing is installed system-wide. The last line turns on the Git hook that checks the commit message format (`CONTRIBUTING.md`).

### 3. Configure (once, about 2 seconds)

```bash
cmake --preset ubuntu-x86_64
```

The preset is called "ubuntu" because it comes from the official OBS template, but it works on any Linux. It creates the folder `build_x86_64/`.

### 4. Build (after every change, about 1 second)

```bash
cmake --build build_x86_64
```

The plugin appears at `build_x86_64/rundir/RelWithDebInfo/timelapse-switch.so`, with its translations next to it in `timelapse-switch/locale/`.

Any compiler warning stops the build. That is on purpose (rule P10-10 in `docs/coding-rules.md`): fix the warning, don't silence it.

### 5. Check everything (before every commit)

```bash
tools/check.sh
```

It runs six steps and ends with a summary. All lines must say PASS:

```
Summary
  PASS  format: C and C++ (clang-format)
  PASS  format: CMake (gersemi)
  PASS  build (zero warnings)
  PASS  clang-tidy (zero warnings)
  PASS  gcc -fanalyzer (zero warnings)
  PASS  shell: ShellCheck (zero warnings)
  PASS  shell: layout (shfmt)
  PASS  shell: lines of 80 characters or less
  PASS  P10-1: no goto, setjmp or longjmp
  PASS  P10-8: no token pasting or variadic macros
  PASS  P10-8: conditional compilation only in tsw-compiler.h
  PASS  Style: no em dashes or en dashes in any file
  PASS  Commits: Conventional Commits format
  PASS  Version: buildspec.json is X.Y.Z (0.1.0)
```

To fix formatting automatically instead of only checking it:

```bash
PATH="$PWD/.venv/bin:$PATH" ./build-aux/run-clang-format
PATH="$PWD/.venv/bin:$PATH" ./build-aux/run-gersemi
.venv/bin/shfmt -w -i 2 -ci -bn tools/*.sh tools/git-hooks/*
```

### 6. Try it in OBS

To try the build in your own OBS, close OBS and run `tools/dev-install.sh`: it copies the build into OBS's per-user plugin folder (`~/.config/obs-studio/plugins/timelapse-switch/`), and `tools/dev-install.sh --uninstall` removes it again. Everything about installing and uninstalling, and what it never touches, is in `docs/install.md`.

## Windows and macOS

The GitHub workflows in `.github/workflows/` build both on every push, using the official OBS template scripts (`.github/scripts/Build-Windows.ps1` and `build-macos`). To build locally on those systems, follow the "Building" section of the [OBS plugin template](https://github.com/obsproject/obs-plugintemplate), with the presets `windows-x64` and `macos`.

## Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| `chmod on .git/config.lock failed: Operation not permitted` when cloning onto an NTFS disk | The disk is mounted with ntfs-3g without an owner, so every file belongs to root and only root may change permissions | Add `uid=1000,gid=1000` (your user and group IDs) to that disk's line in `/etc/fstab` and remount it. Your data is not touched. |
| Git shows every file as changed after cloning on NTFS | NTFS has no executable bit | `git config core.fileMode false` in the repository |
| clang-tidy says `unknown argument: '-mno-direct-extern-access'` | The OBS build adds GCC-only flags | Use `tools/check.sh`, which removes them for clang-tidy |
| `clang-format is more recent than version 19.1.1` | The system clang-format is used instead of the pinned one | Run through `tools/check.sh`, or put `.venv/bin` first in `PATH` |
