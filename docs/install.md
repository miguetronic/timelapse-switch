# Install and uninstall

How to put Timelapse Switch into OBS Studio from a build, and how to take it out again, cleanly.

> **Installing a release?** The packages of each release (Windows `.msi`, macOS `.pkg`, Linux `.zip` and `.deb`) are explained, system by system, in the website's guide: https://miguetronic.github.io/timelapse-switch/guide/install/. This page is about the install tool for builds (`tools/dev-install.sh`), which puts the plugin in **one folder of its own** in your user account: installing adds that folder, uninstalling removes it, and nothing of OBS itself, of other plugins or of your system is changed.

## 1. Where it lives

OBS looks for plugins in a folder of your user account. Timelapse Switch puts everything it needs in one subfolder there, and its settings in another:

```
  ~/.config/obs-studio/                     OBS's folder for your user (Linux)
  |
  +-- plugins/
  |   +-- other-plugin/                     other plugins: never touched
  |   +-- timelapse-switch/                 <- the plugin, all of it
  |       +-- bin/64bit/timelapse-switch.so     the plugin itself
  |       +-- data/locale/en-US.ini             its texts, English
  |       +-- data/locale/es-ES.ini             its texts, Spanish
  |       +-- data/images/symbol-512.png        its symbol, for the About dialog
  |       +-- data/images/symbol-dark-512.png   the same, for dark themes
  |
  +-- plugin_config/
      +-- timelapse-switch/                 <- its settings and hotkeys,
          +-- settings.json                    created when OBS first runs it
```

Think of it as a drawer in a shared cupboard: the plugin only ever opens its own drawer. Because the drawer is in your user folder:

- **no administrator password** (`sudo`) is needed;
- **an OBS update never overwrites it**, and it never overwrites OBS;
- **removing it is removing one folder**, and OBS is exactly as it was before.

Recordings made with the plugin go where your OBS profile records (Settings > Output > Recording path), with " tswitch" at the end of the name. Uninstalling never touches them.

## 2. Before you start

- **Close OBS.** OBS loads plugins when it starts, and replacing a plugin that is loaded can crash it. The install tool refuses to run while OBS is open.
- OBS 31.1 or later.

## 3. Install from a build (Linux)

Build the plugin first (`docs/building.md`), then, from the project folder:

```
tools/dev-install.sh
```

It builds whatever changed, then copies the result into `~/.config/obs-studio/plugins/timelapse-switch/` **in one step**: the new copy is prepared next to the old one and then takes its place, so OBS never finds half an installation. Running it again updates the plugin the same way.

Then start OBS and:

1. Open **Docks > Timelapse Switch** (OBS adds new docks hidden; the welcome card that shows it by itself is planned).
2. Bind your keys in **Settings > Hotkeys**: every action starts with "Timelapse Switch:" and none is bound at first.
3. Check it loaded: **Help > Log Files > View Current Log** has the line `[timelapse-switch] plugin loaded`.

## 4. Uninstall

Close OBS, then:

```
tools/dev-install.sh --uninstall           removes the plugin, keeps your settings and hotkeys
tools/dev-install.sh --uninstall --purge   also removes the settings
```

Without the tool, it is the same as deleting the folders by hand:

```
rm -r ~/.config/obs-studio/plugins/timelapse-switch          the plugin
rm -r ~/.config/obs-studio/plugin_config/timelapse-switch    its settings (optional)
```

The tool removes only those two folders, and checks the exact path before it removes anything.

## 5. What it never does

The install tool, and the plugin itself:

- The tool never writes into OBS's own folders (`/usr/lib/obs-plugins`, `/usr/share/obs`) or anywhere else in the system.
- The plugin never changes OBS's settings, scenes, profiles or other plugins. The plugin reads your profile's recording settings, and never writes them (REQ-010).
- The plugin never sends anything over the network.

## 6. For contributors: how this is checked

`tests/tools/test-dev-install.sh` (run by `ctest`) installs, reinstalls and uninstalls into a made-up OBS folder that also holds another plugin and existing settings, and checks that exactly the plugin's files appear and disappear (the module and every file of `data/`), that settings survive an uninstall unless `--purge` is given, and that the other plugin is never touched.
