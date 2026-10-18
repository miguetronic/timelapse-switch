#!/bin/bash
#
# Timelapse Switch
# SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Makes the demo shown in the README and on the website: a sandbox OBS
# (tests/integration/sandbox.sh) records a neutral scene, an analog clock,
# through the plugin, in real time, then at x8, then in real time again.
#
#   tools/make-demo.sh --yes --font <ttf>
#
# Writes docs/images/demo.gif (README), and site/src/assets/demo/demo.webm
# and demo-poster.png (website). A light run (1080p, under a minute of
# recording), but it starts OBS and records, so it only runs with --yes.
# Needs Pillow and ffmpeg; the dock pictures and the social preview come
# from tools/demo/images.py, after tests/ui/obs-themes.sh.

set -euo pipefail

# shellcheck source=tests/integration/sandbox.sh
source "$(dirname "${BASH_SOURCE[0]}")/../tests/integration/sandbox.sh"

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
  cd "$(git rev-parse --show-toplevel)"
  prepare_build >/dev/null

  local sandbox
  sandbox="$(mktemp -d "${TMPDIR:-/tmp}/timelapse-switch-demo.XXXXXX")"
  python3 tools/demo/clock.py --out "${sandbox}/clock.mp4" --font "${font}"
  make_sandbox "${sandbox}" "${sandbox}/clock.mp4" 1080 \
    hevc_ffmpeg_vaapi_tex 0
  start_obs "${sandbox}" || return 1
  local pid="${OBS_PID}" log="${OBS_LOG}"
  trap 'close_obs '"${pid}" EXIT
  .venv/bin/python tests/integration/drive_obs.py --port "${PORT}" \
    --log "${log}" --scenario demo
  close_obs "${pid}"
  trap - EXIT

  mkdir -p docs/images site/src/assets/demo
  python3 tools/demo/media.py \
    --recording "$(newest "${sandbox}/recordings" '*tswitch*')" \
    --log "${log}" --font "${font}" --gif docs/images/demo.gif \
    --webm site/src/assets/demo/demo.webm \
    --poster site/src/assets/demo/demo-poster.png
  rm -rf "${sandbox}"
  ls -l docs/images/demo.gif site/src/assets/demo/
}

main "$@"
