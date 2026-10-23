#!/bin/bash
#
# Timelapse Switch
# SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Makes the Windows installer (.msi) from the Windows .zip of a build.
#
#   tools/package/make-msi.sh <windows .zip> <version> <output .msi>
#
# The .zip holds the folder timelapse-switch\ (bin\64bit and data), exactly
# as OBS wants it in C:\ProgramData\obs-studio\plugins. The installer puts
# that same folder there, so both ways of installing give the same result.
# What the installer is (name, folder, upgrades) is in timelapse-switch.wxs.
#
# Runs on Linux: wixl and wixl-heat come with msitools (Ubuntu: apt install
# wixl). An MSI version is numbers only, so "1.1.0-beta1" becomes 1.1.0.

set -euo pipefail

main() {
  if (($# != 3)); then
    echo 'usage: make-msi.sh <windows .zip> <version> <output .msi>' >&2
    return 2
  fi
  local zip output here
  zip="$(realpath "$1")"
  output="$(realpath -m "$3")"
  here="$(dirname "$(realpath "${BASH_SOURCE[0]}")")"
  local -r version="${2%%-*}"
  if [[ ! "${version}" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "make-msi.sh: not a version: $2" >&2
    return 2
  fi
  # Global: the EXIT trap runs after main has returned.
  work="$(mktemp -d)"
  trap 'rm -rf "${work}"' EXIT

  unzip -q "${zip}" -d "${work}/files"
  if [[ ! -d "${work}/files/timelapse-switch/bin/64bit" ]]; then
    echo 'make-msi.sh: the .zip has no timelapse-switch/bin/64bit' >&2
    return 1
  fi
  # One component per file, in the folder PLUGINDIR of the .wxs.
  (cd "${work}/files" && find timelapse-switch -type f | sort) \
    | wixl-heat --prefix 'timelapse-switch/' --var var.SourceDir \
      --directory-ref PLUGINDIR --component-group PluginFiles \
      --win64 >"${work}/files.wxs"
  # Uninstalling removes the folders too, not just the files: in each
  # folder, a component that removes it. Its GUID comes from the folder's
  # id; it may change between versions, because an upgrade first removes
  # the old version completely (RemoveExistingProducts runs early).
  {
    echo '<Wix xmlns="http://schemas.microsoft.com/wix/2006/wi"><Fragment>'
    local id guid
    local -a ids=(PLUGINDIR)
    mapfile -t -O 1 ids < <(grep -o 'Directory Id="dir[0-9A-F]*"' \
      "${work}/files.wxs" | cut -d'"' -f2)
    for id in "${ids[@]}"; do
      guid="$(uuidgen --sha1 --namespace @oid --name "timelapse-switch/${id}")"
      echo "<DirectoryRef Id=\"${id}\"><Component Id=\"rm${id}\"" \
        "Guid=\"${guid}\" Win64=\"yes\"><CreateFolder/>" \
        "<RemoveFolder Id=\"rm${id}\" On=\"uninstall\"/></Component>" \
        '</DirectoryRef>'
    done
    echo '<ComponentGroup Id="PluginFolders">'
    printf '<ComponentRef Id="rm%s"/>' "${ids[@]}"
    echo '</ComponentGroup></Fragment></Wix>'
  } >"${work}/folders.wxs"
  # The installer's windows (wixl's WixUI_Minimal: welcome and licence,
  # progress, done), with our text and pictures instead of the toolkit's.
  cp -r "$(find /usr/share -maxdepth 1 -name 'wixl-*' | head -1)/ext" \
    "${work}/ext"
  chmod -R u+w "${work}/ext"
  cp "${here}/installer-dialog.bmp" "${work}/ext/ui/bitmaps/dlgbmp.bmp"
  cp "${here}/installer-banner.bmp" "${work}/ext/ui/bitmaps/bannrbmp.bmp"
  cp "${here}/license.rtf" "${work}/License.rtf"
  cd "${work}"
  wixl --arch x64 --extdir "${work}/ext" --ext ui -D Win64=yes \
    -D Version="${version}" \
    -D SourceDir="${work}/files/timelapse-switch" \
    -D Icon="${here}/timelapse-switch.ico" \
    -o "${output}" "${here}/timelapse-switch.wxs" "${work}/files.wxs" \
    "${work}/folders.wxs"
  echo "${output}"
}

main "$@"
