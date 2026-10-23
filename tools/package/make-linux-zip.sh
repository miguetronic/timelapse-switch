#!/bin/bash
#
# Timelapse Switch
# SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Makes the Linux .zip, for any distribution, from the Linux .deb of a build.
#
#   tools/package/make-linux-zip.sh <linux .deb> <output .zip>
#
# The plugin needs only what every OBS on Linux already brings: OBS itself,
# Qt 6.4 or later and glibc 2.34 or later. So the same files work in any
# distribution, and in the Flatpak OBS, once copied into the plugins
# folder OBS reads in the user's home:
#
#   OBS from the distribution  ~/.config/obs-studio/plugins/
#   OBS from Flatpak           ~/.var/app/com.obsproject.Studio/config/
#                              obs-studio/plugins/
#
# The .zip holds the folder to copy there, timelapse-switch (bin/64bit and
# data, the layout OBS expects in that folder), and "How to install.txt".
# The files are the .deb's own, byte for byte. Needs ar, tar and zip.

set -euo pipefail

main() {
  if (($# != 2)); then
    echo 'usage: make-linux-zip.sh <linux .deb> <output .zip>' >&2
    return 2
  fi
  local deb output here
  deb="$(realpath "$1")"
  output="$(realpath -m "$2")"
  here="$(dirname "$(realpath "${BASH_SOURCE[0]}")")"
  # Global: the EXIT trap runs after main has returned.
  work="$(mktemp -d)"
  trap 'rm -rf "${work}"' EXIT

  # A .deb is an ar archive; the files are in data.tar.* (the compression
  # depends on the tool that made it).
  (cd "${work}" && ar x "${deb}")
  mkdir "${work}/deb"
  tar -xf "${work}"/data.tar.* -C "${work}/deb"
  local -r lib="${work}/deb/usr/lib/x86_64-linux-gnu/obs-plugins"
  local -r so="${lib}/timelapse-switch.so"
  local -r data="${work}/deb/usr/share/obs/obs-plugins/timelapse-switch"
  if [[ ! -f "${so}" || ! -d "${data}" ]]; then
    echo 'make-linux-zip.sh: the .deb has no plugin in it' >&2
    return 1
  fi

  local -r plugin="${work}/zip/timelapse-switch"
  mkdir -p "${plugin}/bin/64bit"
  cp "${so}" "${plugin}/bin/64bit/"
  cp -r "${data}" "${plugin}/data"
  cp "${here}/linux-how-to-install.txt" "${work}/zip/How to install.txt"
  rm -f "${output}"
  (cd "${work}/zip" && zip -q -r -X "${output}" .)
  echo "${output}"
}

main "$@"
