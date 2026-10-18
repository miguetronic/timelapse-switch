---
title: Install and uninstall
description: How to put Timelapse Switch into OBS Studio, and take it out again, cleanly.
---

Each package adds only the plugin's own files, and removing it takes them away again. Your recordings, scenes and other plugins are never touched.

**Before you start:** close OBS (it loads plugins when it starts), and check you have **OBS Studio 31.1 or later**. Downloads are on the [latest release](https://github.com/miguetronic/timelapse-switch/releases/latest).

## Linux

Tested on the maintainer's computer. The `.deb` package is new in 1.0.0: if anything goes wrong with it, please [report it](https://github.com/miguetronic/timelapse-switch/issues/new?template=bug.yml).

**Ubuntu 24.04 or later**, with OBS Studio from the [OBS PPA](https://launchpad.net/~obsproject/+archive/ubuntu/obs-studio) (Ubuntu's own OBS is older than 31.1): download the `.deb` file and install it:

```sh
sudo apt install ./timelapse-switch-*.deb
```

It installs the plugin into OBS's system plugin folders (`/usr/lib/x86_64-linux-gnu/obs-plugins` and `/usr/share/obs/obs-plugins/timelapse-switch`). To uninstall: `sudo apt remove timelapse-switch`.

**Other distributions:** build it from source (see the repository's `docs/building.md` and `docs/install.md`).

**Flatpak OBS** is not supported yet: a Flatpak only loads plugins packaged as Flatpak extensions.

## Windows (beta)

Built on every release, not tested yet. Please [tell us how it works](https://github.com/miguetronic/timelapse-switch/issues/new?template=beta-platform.yml).

1. Download the `windows-x64` `.zip` file.
2. Extract it into `C:\ProgramData\obs-studio\plugins`, the folder OBS recommends for plugins, so you get `C:\ProgramData\obs-studio\plugins\timelapse-switch\bin` and `...\data`. Create the `plugins` folder if it does not exist (Windows asks for administrator permission). Explorer's "Extract All" adds a folder level of its own: check the path before starting OBS.

To uninstall, delete that `timelapse-switch` folder.

## macOS (beta)

Built on every release, not tested yet. Please [tell us how it works](https://github.com/miguetronic/timelapse-switch/issues/new?template=beta-platform.yml).

1. Download the `macos-universal` `.pkg` file.
2. Open it. The installer is not signed yet, so macOS blocks it the first time. Open **System Settings > Privacy & Security**, scroll down to the message about the installer, click **Open Anyway**, then open the file again.

It installs `timelapse-switch.plugin` into `~/Library/Application Support/obs-studio/plugins` (the `Library` folder of your home). To uninstall, delete it from there.

## After installing

1. Start OBS and open **Docks > Timelapse Switch** (OBS adds new docks hidden).
2. Bind your keys in **Settings > Hotkeys**: every action starts with "Timelapse Switch:".
3. Check it loaded: **Help > Log Files > View Current Log** has the line `[timelapse-switch] plugin loaded`.

Your settings and hotkeys are kept in OBS's `plugin_config/timelapse-switch` folder; uninstalling keeps them, so a later install picks them up again. Recordings are never touched.

## Check a download

Every release lists the SHA-256 checksum of each file, and an SBOM (the list of what the plugin was built with). To check a file on Linux or macOS: `sha256sum <file>` (or `shasum -a 256 <file>`), and compare with the release notes.
