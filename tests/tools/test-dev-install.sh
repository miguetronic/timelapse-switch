#!/bin/bash
#
# Timelapse Switch
# SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Tests tools/dev-install.sh against a made-up OBS configuration folder:
# install, reinstall, uninstall (settings kept), uninstall --purge, and
# that another plugin next to it is never touched. Needs the plugin built.
#
#   tests/tools/test-dev-install.sh

set -euo pipefail

failures=0

# Prints PASS or FAIL for one check.
check() {
  local name="$1"
  shift
  if "$@"; then
    echo "PASS  ${name}"
  else
    echo "FAIL  ${name}"
    failures=$((failures + 1))
  fi
}

main() {
  cd "$(git rev-parse --show-toplevel)"
  local config
  config="$(mktemp -d "${TMPDIR:-/tmp}/tsw-dev-install.XXXXXX")"
  trap 'rm -rf -- "'"${config}"'"' EXIT
  mkdir -p "${config}/plugins/other-plugin/bin" \
    "${config}/plugin_config/timelapse-switch"
  echo keep >"${config}/plugins/other-plugin/bin/other.so"
  echo '{}' >"${config}/plugin_config/timelapse-switch/settings.json"
  local tool=(tools/dev-install.sh --config-dir "${config}" --no-build)
  local plugin="${config}/plugins/timelapse-switch"

  "${tool[@]}" >/dev/null
  check 'install: the module' test -f "${plugin}/bin/64bit/timelapse-switch.so"
  check 'install: both locales' test -f "${plugin}/data/locale/es-ES.ini"
  check 'install: no staging left' \
    test ! -e "${config}/plugins/.timelapse-switch.new"

  "${tool[@]}" >/dev/null
  check 'reinstall: same files' test "$(find "${plugin}" -type f | wc -l)" -eq 3

  "${tool[@]}" --uninstall >/dev/null
  check 'uninstall: plugin gone' test ! -e "${plugin}"
  check 'uninstall: settings kept' \
    test -f "${config}/plugin_config/timelapse-switch/settings.json"

  "${tool[@]}" --uninstall --purge >/dev/null
  check 'purge: settings gone' \
    test ! -e "${config}/plugin_config/timelapse-switch"
  local other
  other="$(cat "${config}/plugins/other-plugin/bin/other.so")"
  check 'other plugin untouched' test "${other}" = keep
  check 'purge without uninstall is refused' \
    bash -c '! "$@" --purge 2>/dev/null' _ "${tool[@]}"

  echo "${failures} failures"
  return "${failures}"
}

main "$@"
