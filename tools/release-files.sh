#!/bin/bash
#
# Timelapse Switch
# SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Puts together the files of a release, from the build artifacts of the CI.
#
#   tools/release-files.sh <artifacts folder> <release folder> <version>
#
# Run from the repository (the SBOM reads buildspec.json and git). Both ways
# of releasing use it, release-please.yaml and push.yaml, so a release has
# the same files whichever way it was made:
#
#   timelapse-switch-windows-x64.msi     Windows installer   (make-msi.sh)
#   timelapse-switch-windows-x64.zip     Windows, by hand
#   timelapse-switch-macos.pkg           macOS installer
#   timelapse-switch-linux-x86_64.zip    any Linux, Flatpak OBS too
#                                        (make-linux-zip.sh)
#   timelapse-switch-linux-x86_64.deb    Ubuntu and Debian
#   ...-<version>-*                      source, debug symbols, SBOM
#   CHECKSUMS.txt                        SHA-256 of every file
#
# The packages people download have no version in their name, so the guide
# can link straight to the latest one:
# .../releases/latest/download/timelapse-switch-windows-x64.msi
#
# Needs wixl (Ubuntu: apt install wixl), ar, zip, jq and uuidgen.

set -euo pipefail

readonly NAME='timelapse-switch'

#######################################
# Moves the one file matching a pattern to its name without the version.
# Arguments:
#   Release folder, pattern, new name.
#######################################
rename_one() {
  local -a found
  mapfile -t found < <(find "$1" -maxdepth 1 -name "$2")
  if ((${#found[@]} != 1)); then
    echo "release-files.sh: ${#found[@]} files match $2, one expected" >&2
    return 1
  fi
  mv "${found[0]}" "$1/$3"
}

main() {
  if (($# != 3)); then
    echo 'usage: release-files.sh <artifacts> <release folder> <version>' >&2
    return 2
  fi
  local -r artifacts="$1" release="$2" version="$3"
  local here
  here="$(dirname "$(realpath "${BASH_SOURCE[0]}")")"
  mkdir -p "${release}"
  find "${artifacts}" -mindepth 2 -maxdepth 2 -type f \
    \( -name '*.zip' -o -name '*.pkg' -o -name '*.deb' -o -name '*.ddeb' \
    -o -name '*.tar.xz' \) -exec mv -t "${release}" {} +

  # Every system must be there: a release without one of them is not made.
  rename_one "${release}" "${NAME}-*-windows-x64.zip" \
    "${NAME}-windows-x64.zip"
  rename_one "${release}" "${NAME}-*-macos-universal.pkg" "${NAME}-macos.pkg"
  rename_one "${release}" "${NAME}-*-linux-gnu.deb" \
    "${NAME}-linux-x86_64.deb"

  "${here}/package/make-msi.sh" "${release}/${NAME}-windows-x64.zip" \
    "${version}" "${release}/${NAME}-windows-x64.msi"
  "${here}/package/make-linux-zip.sh" "${release}/${NAME}-linux-x86_64.deb" \
    "${release}/${NAME}-linux-x86_64.zip"
  "${here}/make-sbom.sh" "${release}/${NAME}-${version}.cdx.json"
  rm -f "${release}/CHECKSUMS.txt"
  (cd "${release}" && sha256sum -- * >CHECKSUMS.txt)
  cat "${release}/CHECKSUMS.txt"
}

main "$@"
