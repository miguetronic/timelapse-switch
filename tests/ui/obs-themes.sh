#!/bin/bash
#
# Timelapse Switch
# SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
# SPDX-License-Identifier: GPL-2.0-or-later
#
# The dock in every OBS theme, as OBS draws it (tests/ui/README.md).
#
#   tests/ui/obs-themes.sh                 write each theme down, then draw
#   tests/ui/obs-themes.sh --render-only   draw again from the last dumps
#   tests/ui/obs-themes.sh --matrix        the same for OBS's Font Size and
#                                          Density extremes (with
#                                          --render-only: draw again)
#
# 1. For each theme and language, a sandbox OBS (tests/integration/sandbox.sh)
#    starts with that theme, that language and the theme probe, which writes
#    the theme and a picture of OBS's own dock into
#    build_x86_64/obs-themes/<theme>/ once OBS has loaded; then OBS is
#    closed. Nothing is recorded.
# 2. test-dock dresses the dock in each theme and draws every state, in
#    English and Spanish, 300 and 220 px wide, into
#    build_x86_64/ui-screenshots/<theme>/, plus one contact sheet per theme
#    and language in build_x86_64/ui-screenshots/contact-sheets/.
#
# 3. With --matrix, Default, Light and System are written down again at Font
#    Size 8 with Classic density, 10 with Normal and 12 with Comfortable
#    (Settings > Appearance), into build_x86_64/obs-themes-matrix/, and drawn
#    into build_x86_64/ui-screenshots-matrix/.
#
# Linux only. Your own OBS settings are never read or changed; the desktop's
# colors and fonts (KDE's kdeglobals) are copied into each sandbox, so the
# System theme looks as it does on this desktop.
# Exit status: 0 when every theme was written down and every check passed.

set -euo pipefail

# shellcheck source=tests/integration/sandbox.sh
source "$(dirname "${BASH_SOURCE[0]}")/../integration/sandbox.sh"

readonly DUMPS='build_x86_64/obs-themes'
readonly PICTURES='build_x86_64/ui-screenshots'
readonly MATRIX_DUMPS='build_x86_64/obs-themes-matrix'
readonly MATRIX_PICTURES='build_x86_64/ui-screenshots-matrix'
readonly PROBE='build_x86_64/ui-theme-probe/tsw-theme-probe.so'
readonly TEST_DOCK='build_x86_64/tests/ui/test-dock'
# The themes that come with OBS 31 and 32 (Settings > Appearance).
readonly -a THEMES=(
  com.obsproject.Yami.Original
  com.obsproject.Yami.Classic
  com.obsproject.Yami.Grey
  com.obsproject.Yami.Acri
  com.obsproject.Yami.Rachni
  com.obsproject.Yami.Light
  com.obsproject.System
)
# The languages of the pictures; each one gets its own OBS run, so OBS's own
# dock can be compared in every language.
readonly -a LANGUAGES=(en-US es-ES)
# The matrix (docs/design/visual-design.md, 3.13): themes, and Font Size with
# OBS's density id (-2 Classic, -3 Compact, -4 Normal, -5 Comfortable).
readonly -a MATRIX_THEMES=(
  com.obsproject.Yami.Original
  com.obsproject.Yami.Light
  com.obsproject.System
)
readonly -a MATRIX_SIZES=('8 -2 classic' '10 -4 normal' '12 -5 comfortable')
# Half seconds to wait for the probe: OBS usually loads in 2 to 10 s.
readonly WAIT_STEPS=120

# Prints a message to stderr.
err() {
  echo "$*" >&2
}

#######################################
# Waits until the probe has written theme.json, or OBS has gone away.
# Arguments:
#   Dump folder, process id of OBS.
# Returns:
#   0 once theme.json exists, 1 otherwise.
#######################################
wait_for_dump() {
  local out="$1" pid="$2"
  local i
  for ((i = 0; i < WAIT_STEPS; i++)); do
    if [[ -f "${out}/theme.json" ]]; then
      return 0
    fi
    if ! kill -0 "${pid}" 2>/dev/null; then
      return 1
    fi
    sleep 0.5
  done
  return 1
}

#######################################
# Writes one theme down: a sandbox OBS with that theme, that language and
# the probe. The theme files are the same in every language; the pictures
# of OBS's own dock are per language.
# Arguments:
#   Theme id, such as com.obsproject.Yami.Light; language, such as es-ES;
#   the dump folder; optionally a Font Size and a density id.
# Outputs:
#   One line saying where the dump is, or why it failed.
# Returns:
#   0 when the dump is complete and OBS used the theme asked for.
#######################################
dump_theme() {
  local theme="$1" language="$2" out="$3" font="${4:-}" density="${5:-}"
  local sandbox
  sandbox="$(mktemp -d "${TMPDIR:-/tmp}/timelapse-switch-theme.XXXXXX")"
  make_sandbox "${sandbox}" '' 1080 obs_x264 0
  set_theme "${sandbox}" "${theme}"
  if [[ -n "${font}" ]]; then
    # Right after the theme, so both lines land in its [Appearance] section.
    printf 'FontScale=%s\nDensity=%s\n' "${font}" "${density}" \
      >>"${sandbox}/config/obs-studio/user.ini"
  fi
  set_language "${sandbox}" "${language}"
  install_module "${sandbox}" "${PROBE}"
  local desktop="${XDG_CONFIG_HOME:-${HOME}/.config}/kdeglobals"
  if [[ -f "${desktop}" ]]; then
    cp "${desktop}" "${sandbox}/config/"
  fi
  mkdir -p "${out}"
  rm -f "${out}/theme.json"

  TSW_THEME_DUMP="${out}" start_obs "${sandbox}" || return 1
  local pid="${OBS_PID}" log="${OBS_LOG}"
  trap 'close_obs '"${pid}" EXIT
  local status=0
  if ! wait_for_dump "${out}" "${pid}"; then
    err "FAIL  ${theme} ${language}: the probe wrote nothing (see ${log})"
    status=1
  elif grep -q 'Loading theme .* failed' "${log}"; then
    err "FAIL  ${theme} ${language}: OBS did not load it (see ${log})"
    status=1
  fi
  close_obs "${pid}"
  trap - EXIT
  if ((status == 0)); then
    echo "theme ${theme} ${language}: ${out}"
    rm -rf "${sandbox}"
  else
    err "      the sandbox is kept: ${sandbox}"
  fi
  return "${status}"
}

#######################################
# Writes down every theme of the default run, in every language.
# Returns:
#   0 when every dump is complete.
#######################################
dump_all() {
  local status=0 theme language out
  for theme in "${THEMES[@]}"; do
    out="${PWD}/${DUMPS}/${theme#com.obsproject.}"
    rm -rf "${out}"
    for language in "${LANGUAGES[@]}"; do
      dump_theme "${theme}" "${language}" "${out}" || status=1
    done
  done
  return "${status}"
}

#######################################
# Writes down the matrix: each matrix theme at each Font Size and density.
# Returns:
#   0 when every dump is complete.
#######################################
dump_matrix() {
  local status=0 theme language size font density name out
  for theme in "${MATRIX_THEMES[@]}"; do
    for size in "${MATRIX_SIZES[@]}"; do
      read -r font density name <<<"${size}"
      out="${PWD}/${MATRIX_DUMPS}/${theme#com.obsproject.}-${font}pt-${name}"
      rm -rf "${out}"
      for language in "${LANGUAGES[@]}"; do
        dump_theme "${theme}" "${language}" "${out}" "${font}" "${density}" \
          || status=1
      done
    done
  done
  return "${status}"
}

main() {
  local render_only=0 matrix=0
  while (($# > 0)); do
    case "$1" in
      --render-only) render_only=1 ;;
      --matrix) matrix=1 ;;
      *) err "unknown option: $1" && return 2 ;;
    esac
    shift
  done
  cd "$(git rev-parse --show-toplevel)"
  cmake --build build_x86_64 >/dev/null

  local status=0 dumps="${DUMPS}" pictures="${PICTURES}"
  if ((matrix == 1)); then
    dumps="${MATRIX_DUMPS}"
    pictures="${MATRIX_PICTURES}"
  fi
  if ((render_only == 0 && matrix == 0)); then
    dump_all || status=1
  elif ((render_only == 0)); then
    dump_matrix || status=1
  fi
  QT_QPA_PLATFORM=offscreen "${TEST_DOCK}" data/locale "${pictures}" \
    "${dumps}" || status=1
  echo "contact sheets: ${PWD}/${pictures}/contact-sheets/"
  return "${status}"
}

main "$@"
