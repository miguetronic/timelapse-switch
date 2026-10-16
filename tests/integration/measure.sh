#!/bin/bash
#
# Timelapse Switch
# SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Performance measurement: what recording costs the computer with OBS's own
# Record button and with the plugin in real time, same scene, same encoder
# settings (tests/integration/README.md, "Measuring the cost").
#
#   tests/integration/measure.sh [--resolution 1080|2160] [--seconds 300]
#                                [--idle 60] [--after 30]
#                                [--encoder <OBS encoder id>]
#
# Each mode gets a fresh sandbox OBS (sandbox.sh): a minute idle, then the
# recording, then half a minute once the file is saved. The sync probe is
# left out, because it would ask OBS for every frame in memory and make
# OBS's own recording look as costly as the plugin's. The report goes to
# build_x86_64/measure/<date and time>/.
#
# This is a load test: run it on the reference PC only with the
# maintainer's approval (docs/quality.md, section 5).

set -euo pipefail

# shellcheck source=tests/integration/sandbox.sh
source "$(dirname "${BASH_SOURCE[0]}")/sandbox.sh"

#######################################
# Measures one mode in its own sandbox OBS and keeps its log.
# Arguments:
#   Mode (obs or plugin), test media, height, encoder, idle seconds,
#   recording seconds, seconds after the recording, report folder.
#######################################
measure_mode() {
  local mode="$1" media="$2" height="$3" encoder="$4" idle="$5"
  local seconds="$6" after="$7" out="$8"
  local sandbox
  sandbox="$(mktemp -d "${TMPDIR:-/tmp}/timelapse-switch-measure.XXXXXX")"
  make_sandbox "${sandbox}" "${media}" "${height}" "${encoder}" 0
  start_obs "${sandbox}" || return 1
  local pid="${OBS_PID}" log="${OBS_LOG}"
  trap 'close_obs '"${pid}" EXIT

  local status=0
  .venv/bin/python tests/integration/measure.py run --port "${PORT}" \
    --pid "${pid}" --log "${log}" --mode "${mode}" --idle "${idle}" \
    --seconds "${seconds}" --after "${after}" --out "${out}" || status=$?
  close_obs "${pid}"
  trap - EXIT
  # OBS writes its own per-frame timings to the log when it closes.
  cp "${log}" "${out}/${mode}.log"
  rm -rf "${sandbox}"
  return "${status}"
}

main() {
  local height=2160 seconds=300 idle=60 after=30
  local encoder='hevc_ffmpeg_vaapi_tex'
  while (($# > 0)); do
    case "$1" in
      --resolution) height="$2" && shift ;;
      --seconds) seconds="$2" && shift ;;
      --idle) idle="$2" && shift ;;
      --after) after="$2" && shift ;;
      --encoder) encoder="$2" && shift ;;
      *) echo "unknown option: $1" >&2 && return 2 ;;
    esac
    shift
  done
  cd "$(git rev-parse --show-toplevel)"

  local media out
  media="$(prepare_build)"
  out="build_x86_64/measure/$(date +%Y-%m-%d_%H-%M-%S)"
  mkdir -p "${out}"
  local mode
  for mode in obs plugin; do
    measure_mode "${mode}" "${media}" "${height}" "${encoder}" "${idle}" \
      "${seconds}" "${after}" "${out}"
  done
  .venv/bin/python tests/integration/measure.py report "${out}"
  echo "report: ${out}/report.md"
}

main "$@"
