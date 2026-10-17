#!/bin/bash
#
# Timelapse Switch
# SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Installs the plugin just built into OBS's per-user plugin folder, or
# removes it (Linux). It only ever touches its own folders:
#
#   ~/.config/obs-studio/plugins/timelapse-switch/        the plugin
#   ~/.config/obs-studio/plugin_config/timelapse-switch/  its settings
#                                                         (only --purge)
#
# Nothing of OBS itself, of other plugins or of the system is changed, no
# sudo is needed, and an OBS update never overwrites it (docs/install.md).
#
#   tools/dev-install.sh                       build, then install
#   tools/dev-install.sh --uninstall           remove the plugin
#   tools/dev-install.sh --uninstall --purge   also remove its settings
#
# Options for tests: --config-dir DIR (an OBS configuration folder other
# than the user's; then a running OBS is not a reason to stop) and
# --no-build.

set -euo pipefail

readonly NAME='timelapse-switch'
readonly BUILD_DIR='build_x86_64'

# Prints an error to stderr.
err() {
  echo "dev-install: $*" >&2
}

#######################################
# Stops unless no OBS is running: replacing a plugin OBS has loaded can
# crash it, and OBS rewrites its configuration when it closes.
# Arguments:
#   1 if the target is the user's own OBS configuration, 0 otherwise.
#######################################
require_obs_closed() {
  local own="$1"
  if ((own == 1)) && pgrep -x obs >/dev/null; then
    err 'OBS is running. Close OBS first, then run this again.'
    return 1
  fi
}

#######################################
# Removes a folder only if it is exactly <config>/<kind>/timelapse-switch.
# Arguments:
#   Folder to remove, OBS configuration folder, kind (plugins or
#   plugin_config).
#######################################
remove_own_folder() {
  local folder="$1" config="$2" kind="$3"
  if [[ "${folder}" != "${config}/${kind}/${NAME}" || -z "${config}" ]]; then
    err "refusing to remove ${folder}: not this plugin's folder"
    return 1
  fi
  if [[ -e "${folder}" ]]; then
    rm -rf -- "${folder}"
    echo "removed ${folder}"
  else
    echo "nothing to remove at ${folder}"
  fi
}

#######################################
# Copies the built plugin into place in one step: the new copy is made
# next to the old one and then takes its place, so OBS never finds half
# an installation. All of the plugin's data goes with it (its texts in
# locale/, its pictures in images/), as the build left it in the rundir.
# Arguments:
#   OBS configuration folder.
#######################################
install_plugin() {
  local config="$1"
  local rundir="${BUILD_DIR}/rundir/RelWithDebInfo"
  if [[ ! -f "${rundir}/${NAME}.so" || ! -d "${rundir}/${NAME}/locale" ]]; then
    err "no build in ${rundir}; build first (docs/building.md)"
    return 1
  fi
  local target="${config}/plugins/${NAME}"
  local staging="${config}/plugins/.${NAME}.new"
  rm -rf -- "${staging}"
  mkdir -p "${staging}/bin/64bit" "${staging}/data"
  cp "${rundir}/${NAME}.so" "${staging}/bin/64bit/"
  cp -r "${rundir}/${NAME}/." "${staging}/data/"
  if [[ -e "${target}" ]]; then
    remove_own_folder "${target}" "${config}" plugins >/dev/null
  fi
  mv -- "${staging}" "${target}"
  echo "installed into ${target}:"
  (cd "${target}" && find . -type f | sort | sed 's/^/  /')
  echo 'Next: start OBS, then Docks > Timelapse Switch, and Settings > Hotkeys.'
}

main() {
  local config="${XDG_CONFIG_HOME:-${HOME}/.config}/obs-studio"
  local own=1 uninstall=0 purge=0 build=1
  while (($# > 0)); do
    case "$1" in
      --uninstall) uninstall=1 ;;
      --purge) purge=1 ;;
      --no-build) build=0 ;;
      --config-dir) config="$2" own=0 && shift ;;
      *) err "unknown option: $1" && return 2 ;;
    esac
    shift
  done
  if ((purge == 1 && uninstall == 0)); then
    err '--purge only goes with --uninstall'
    return 2
  fi
  cd "$(git rev-parse --show-toplevel)"
  require_obs_closed "${own}"

  if ((uninstall == 1)); then
    remove_own_folder "${config}/plugins/${NAME}" "${config}" plugins
    if ((purge == 1)); then
      remove_own_folder "${config}/plugin_config/${NAME}" "${config}" \
        plugin_config
    else
      echo "settings kept in ${config}/plugin_config/${NAME}" \
        '(--purge removes them)'
    fi
    return 0
  fi
  if ((build == 1)); then
    cmake --build "${BUILD_DIR}" >/dev/null
  fi
  install_plugin "${config}"
}

main "$@"
