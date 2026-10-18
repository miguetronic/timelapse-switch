#!/bin/bash
#
# Timelapse Switch
# SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Makes the demo shown in the README and on the website: a sandbox OBS
# (tests/integration/sandbox.sh), with a neutral scene (an analog clock) and
# the dock, runs on a virtual screen (Xvfb); the pointer clicks Record, the
# switch to timelapse x8, the way back and Stop, while ffmpeg films the
# window. tools/demo/media.py then speeds up the timelapse stretch, as the
# plugin does with the file.
#
#   tools/make-demo.sh --yes --font <ttf>
#
# Writes docs/images/demo.gif (README), and site/src/assets/demo/demo.webm
# and demo-poster.png (website). A light run (720p, under a minute of
# recording, software rendering), but it starts OBS and records, so it only
# runs with --yes. Needs Xvfb, xdotool, ffmpeg and Pillow. Your own OBS and
# your screen are not touched. The dock pictures and the social preview come
# from tools/demo/images.py, after tests/ui/obs-themes.sh.

set -euo pipefail

# shellcheck source=tests/integration/sandbox.sh
source "$(dirname "${BASH_SOURCE[0]}")/../tests/integration/sandbox.sh"

readonly SCREEN=':77'
# Where the dock's buttons are, with the window of tools/demo/obs-window.ini.
readonly SPEED8=(1161 432)
readonly BIG_BUTTON=(1126 298)
readonly STOP=(1200 420)
# A resting place for the pointer, away from the preview (OBS outlines the
# source under the pointer).
readonly REST=(1120 645)

#######################################
# Moves the pointer to a point in 12 small steps, so the viewer can follow.
# Arguments:
#   x, y.
#######################################
glide() {
  local X Y i
  eval "$(DISPLAY="${SCREEN}" xdotool getmouselocation --shell \
    | grep -E '^(X|Y)=')"
  for ((i = 1; i <= 12; i++)); do
    DISPLAY="${SCREEN}" xdotool mousemove \
      $((X + ($1 - X) * i / 12)) $((Y + ($2 - Y) * i / 12))
    sleep 0.04
  done
}

#######################################
# Glides to a point and clicks it.
# Arguments:
#   x, y.
#######################################
click() {
  glide "$1" "$2"
  sleep 0.4
  DISPLAY="${SCREEN}" xdotool click 1
}

#######################################
# Films the window while clicking through the demo; writes the capture and
# the time of each click.
# Arguments:
#   Capture file, marks file.
#######################################
take() {
  local capture="$1" marks="$2" start
  DISPLAY="${SCREEN}" xdotool mousemove "${REST[@]}"
  click "${SPEED8[@]}"
  glide "${REST[@]}"
  ffmpeg -loglevel error -y -f x11grab -framerate 30 \
    -video_size 1280x875 -i "${SCREEN}+0,25" \
    -c:v libx264 -preset ultrafast -qp 0 "${capture}" &
  local ffmpeg_pid=$!
  # EPOCHREALTIME uses the locale's decimal separator, awk wants a dot.
  start="${EPOCHREALTIME/,/.}"
  mark() {
    local now="${EPOCHREALTIME/,/.}"
    awk -v now="${now}" -v start="${start}" -v name="$1" \
      'BEGIN { printf "%s %.3f\n", name, now - start }' >>"${marks}"
  }
  : >"${marks}"
  sleep 1.5 && click "${BIG_BUTTON[@]}" && mark record
  sleep 5 && click "${BIG_BUTTON[@]}" && mark timelapse
  sleep 1 && glide "${REST[@]}"
  sleep 30 && click "${BIG_BUTTON[@]}" && mark realtime
  sleep 4 && click "${STOP[@]}" && mark stop
  sleep 1 && glide "${REST[@]}"
  sleep 4.5 && mark end
  kill -INT "${ffmpeg_pid}"
  wait "${ffmpeg_pid}" || true
}

main() {
  local yes=0 font=''
  while (($# > 0)); do
    case "$1" in
      --yes) yes=1 ;;
      --font) font="$2" && shift ;;
      *) echo "unknown option: $1" >&2 && return 2 ;;
    esac
    shift
  done
  if ((yes == 0)) || [[ -z "${font}" ]]; then
    echo 'make-demo.sh records in a sandbox OBS: run it with --yes' \
      '--font <ttf>' >&2
    return 2
  fi
  if [[ -e "/tmp/.X11-unix/X${SCREEN#:}" ]]; then
    echo "make-demo.sh: the virtual screen ${SCREEN} is in use" >&2
    return 1
  fi
  cd "$(git rev-parse --show-toplevel)"
  prepare_build >/dev/null

  local sandbox
  sandbox="$(mktemp -d "${TMPDIR:-/tmp}/timelapse-switch-demo.XXXXXX")"
  # Five minutes, so the clock never starts over during the take.
  python3 tools/demo/clock.py --seconds 300 --out "${sandbox}/clock.mp4" \
    --font "${font}"
  # Software rendering on the virtual screen: x264, not a GPU encoder.
  make_sandbox "${sandbox}" "${sandbox}/clock.mp4" 720 obs_x264 0
  cat tools/demo/obs-window.ini >>"${sandbox}/config/obs-studio/user.ini"

  Xvfb "${SCREEN}" -screen 0 1600x900x24 -nolisten tcp >/dev/null 2>&1 &
  local xvfb_pid=$!
  sleep 2
  # Not start_obs: its --minimize-to-tray would hide the window.
  DISPLAY="${SCREEN}" QT_QPA_PLATFORM=xcb WAYLAND_DISPLAY='' \
    XDG_CONFIG_HOME="${sandbox}/config" obs --multi \
    --disable-shutdown-check --disable-updater --profile "${PROFILE}" \
    --collection "${PROFILE}" >"${sandbox}/obs-output.txt" 2>&1 &
  local obs_pid=$!
  trap 'close_obs '"${obs_pid}"'; kill '"${xvfb_pid}" EXIT
  sleep 12
  take "${sandbox}/take.mkv" "${sandbox}/marks.txt"
  close_obs "${obs_pid}"
  kill "${xvfb_pid}"
  trap - EXIT

  mkdir -p docs/images site/src/assets/demo
  python3 tools/demo/media.py --take "${sandbox}/take.mkv" \
    --marks "${sandbox}/marks.txt" --font "${font}" \
    --gif docs/images/demo.gif --webm site/src/assets/demo/demo.webm \
    --poster site/src/assets/demo/demo-poster.png
  cat "${sandbox}/marks.txt"
  rm -rf "${sandbox}"
  ls -l docs/images/demo.gif site/src/assets/demo/
}

main "$@"
