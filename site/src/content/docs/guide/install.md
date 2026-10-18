---
title: Install and uninstall
description: How to put Timelapse Switch into OBS Studio, and take it out again, cleanly.
---

Timelapse Switch lives in **one folder of its own**: installing it adds that folder, uninstalling removes it. Nothing of OBS itself, of other plugins or of your system is changed.

**Before you start:** close OBS (it loads plugins when it starts), and check you have **OBS Studio 31.1 or later**. Downloads are on the [latest release](https://github.com/miguetronic/timelapse-switch/releases/latest).

## Linux

Tested on every release.

**Ubuntu and Debian (OBS from your distribution or the OBS PPA):** download the `.deb` file and install it:

```sh
sudo apt install ./timelapse-switch-*.deb
```

To uninstall: `sudo apt remove timelapse-switch`.

**Flatpak OBS** is not supported yet: a Flatpak only loads plugins packaged as Flatpak extensions.

## Windows (beta)

Built on every release, not tested yet. Please [tell us how it works](https://github.com/miguetronic/timelapse-switch/issues/new?template=beta-platform.yml).

1. Download the `windows-x64` `.zip` file.
2. Extract it into `C:\ProgramData\obs-studio\plugins`, the folder OBS recommends for plugins, so you get `C:\ProgramData\obs-studio\plugins\timelapse-switch`.

To uninstall, delete that `timelapse-switch` folder.

## macOS (beta)

Built on every release, not tested yet. Please [tell us how it works](https://github.com/miguetronic/timelapse-switch/issues/new?template=beta-platform.yml).

1. Download the `macos-universal` `.pkg` file.
2. Open it. The installer is not signed yet, so macOS may refuse it the first time: right-click the file, choose **Open**, then **Open** again.

It installs `timelapse-switch.plugin` into the `obs-studio/plugins` folder under `Library/Application Support`. To uninstall, delete `timelapse-switch.plugin` from there (check both `/Library/Application Support/obs-studio/plugins` and the same path in your own home folder).

## After installing

1. Start OBS and open **Docks > Timelapse Switch** (OBS adds new docks hidden).
2. Bind your keys in **Settings > Hotkeys**: every action starts with "Timelapse Switch:".
3. Check it loaded: **Help > Log Files > View Current Log** has the line `[timelapse-switch] plugin loaded`.

Your settings and hotkeys are kept in OBS's `plugin_config/timelapse-switch` folder; uninstalling keeps them, so a later install picks them up again. Recordings are never touched.

## Check a download

Every release lists the SHA-256 checksum of each file, and an SBOM (the list of what the plugin was built with). To check a file on Linux or macOS: `sha256sum <file>` (or `shasum -a 256 <file>`), and compare with the release notes.
