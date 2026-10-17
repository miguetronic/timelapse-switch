#!/bin/bash
#
# Timelapse Switch
# SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Integration test: a real OBS records with the plugin, then the file is
# checked (tests/integration/README.md).
#
#   tests/integration/run.sh [--resolution 1080|2160]
#                            [--scenario standard|long|real|pause|pauses]
#                            [--encoder <OBS encoder id>] [--keep]
#
# Linux only. It starts a separate OBS with its own temporary configuration
# (sandbox.sh), so the user's OBS settings, scenes and plugins are never
# touched.
# Exit status: 0 when every check passes.

set -euo pipefail

# shellcheck source=tests/integration/sandbox.sh
source "$(dirname "${BASH_SOURCE[0]}")/sandbox.sh"

main() {
  local height=1080 scenario='standard' encoder='hevc_ffmpeg_vaapi_tex'
  local keep=0
  while (($# > 0)); do
    case "$1" in
      --resolution) height="$2" && shift ;;
      --scenario) scenario="$2" && shift ;;
      --encoder) encoder="$2" && shift ;;
      --keep) keep=1 ;;
      *) echo "unknown option: $1" >&2 && return 2 ;;
    esac
    shift
  done
  cd "$(git rev-parse --show-toplevel)"

  # 1. Build, and make the test media once.
  local media
  media="$(prepare_build)"

  # 2. A sandbox OBS with its own configuration, and the sync probe.
  local sandbox
  sandbox="$(mktemp -d "${TMPDIR:-/tmp}/timelapse-switch-it.XXXXXX")"
  make_sandbox "${sandbox}" "${media}" "${height}" "${encoder}" 1
  start_obs "${sandbox}" || return 1
  local pid="${OBS_PID}" log="${OBS_LOG}"
  trap 'close_obs '"${pid}" EXIT

  # 3. Record the scenario, close OBS, check the file.
  local status=0
  .venv/bin/python tests/integration/drive_obs.py --port "${PORT}" \
    --log "${log}" --scenario "${scenario}" || status=$?
  close_obs "${pid}"
  trap - EXIT
  # A clean exit makes OBS count the memory it never got back (rule B2).
  if ! grep -q 'Number of memory leaks: 0$' "${log}"; then
    echo "FAIL  memory leaks: $(grep 'Number of memory leaks' "${log}")"
    status=1
  fi
  if ((status == 0)); then
    local recording
    recording="$(newest "${sandbox}/recordings" '*tswitch*')"
    echo "recording: ${recording}"
    .venv/bin/python tests/integration/analyze.py --recording "${recording}" \
      --log "${log}" || status=$?
  fi
  echo "OBS log: ${log}"
  if ((keep == 0 && status == 0)); then
    rm -rf "${sandbox}"
  fi
  return "${status}"
}

main "$@"
