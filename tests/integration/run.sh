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
#                            [--scenario standard|long|real]
#                            [--encoder <OBS encoder id>] [--keep]
#
# Linux only. It starts a separate OBS with its own temporary configuration
# (XDG_CONFIG_HOME), so the user's OBS settings, scenes and plugins are never
# touched; its obs-websocket listens on port 4466 instead of the usual 4455.
# Exit status: 0 when every check passes.

set -euo pipefail

readonly PORT=4466
readonly PROFILE='tsw-test'

#######################################
# Writes the OBS configuration of the sandbox: profile, scene, websocket,
# plugin settings, and installs the freshly built plugin and sync probe.
# Arguments:
#   Sandbox directory, test media file, resolution height, encoder id.
#######################################
make_sandbox() {
  local sandbox="$1" media="$2" height="$3" encoder="$4"
  local width=$((height * 16 / 9))
  local obs="${sandbox}/config/obs-studio"
  mkdir -p "${obs}/basic/profiles/${PROFILE}" "${obs}/basic/scenes" \
    "${obs}/plugin_config/obs-websocket" \
    "${obs}/plugin_config/timelapse-switch" \
    "${obs}/plugins/timelapse-switch/bin/64bit" \
    "${obs}/plugins/timelapse-switch/data" \
    "${obs}/plugins/tsw-sync-probe/bin/64bit" "${sandbox}/recordings"

  cat >"${obs}/user.ini" <<EOF
[General]
FirstRun=true
ConfirmOnExit=false
HotkeyFocusType=NeverDisableHotkeys
[Basic]
Profile=${PROFILE}
ProfileDir=${PROFILE}
SceneCollection=${PROFILE}
SceneCollectionFile=${PROFILE}
EOF

  cat >"${obs}/basic/profiles/${PROFILE}/basic.ini" <<EOF
[General]
Name=${PROFILE}
[Output]
Mode=Advanced
FilenameFormatting=${PROFILE} %CCYY-%MM-%DD %hh-%mm-%ss
[AdvOut]
RecType=Standard
RecFilePath=${sandbox}/recordings
RecFormat2=hybrid_mp4
RecTracks=1
RecEncoder=${encoder}
RecAudioEncoder=ffmpeg_aac
Track1Bitrate=192
[Video]
BaseCX=${width}
BaseCY=${height}
OutputCX=${width}
OutputCY=${height}
FPSType=0
FPSCommon=30
ColorFormat=NV12
ColorSpace=709
ColorRange=Partial
[Audio]
SampleRate=48000
ChannelSetup=Stereo
EOF

  cat >"${obs}/basic/profiles/${PROFILE}/recordEncoder.json" <<EOF
{"rate_control": "CQP", "qp": 22, "keyint_sec": 2, "bf": 0}
EOF

  # One scene with the test media filling the canvas, and no microphone or
  # desktop audio: only the test beeps reach the recording.
  cat >"${obs}/basic/scenes/${PROFILE}.json" <<EOF
{
  "name": "${PROFILE}",
  "current_scene": "Test",
  "current_program_scene": "Test",
  "scene_order": [{"name": "Test"}],
  "sources": [
    {"id": "scene", "versioned_id": "scene", "name": "Test",
     "settings": {"id_counter": 1, "items": [
       {"name": "Test media", "id": 1, "visible": true, "pos": {"x": 0, "y": 0},
        "align": 5, "bounds_type": 2, "bounds_align": 0,
        "bounds": {"x": ${width}, "y": ${height}}}]}},
    {"id": "ffmpeg_source", "versioned_id": "ffmpeg_source",
     "name": "Test media", "mixers": 1, "volume": 1.0,
     "settings": {"is_local_file": true, "local_file": "${media}",
                  "looping": true, "restart_on_activate": false}}
  ],
  "transitions": [],
  "current_transition": "Fade",
  "transition_duration": 300
}
EOF

  cat >"${obs}/plugin_config/obs-websocket/config.json" <<EOF
{"server_enabled": true, "server_port": ${PORT}, "auth_required": false,
 "first_load": false, "alerts_enabled": false}
EOF

  # Stop on the first press (no hold): obs-websocket presses keys, it cannot
  # hold them.
  echo '{"stop_hold_ms": 0}' \
    >"${obs}/plugin_config/timelapse-switch/settings.json"

  cp build_x86_64/rundir/RelWithDebInfo/timelapse-switch.so \
    "${obs}/plugins/timelapse-switch/bin/64bit/"
  cp -r data/locale "${obs}/plugins/timelapse-switch/data/"
  # The test-only module that logs when OBS delivers each beep and frame.
  cp build_x86_64/integration/tsw-sync-probe.so \
    "${obs}/plugins/tsw-sync-probe/bin/64bit/"
}

# Prints the newest file matching a pattern in a directory, or nothing.
newest() {
  find "$1" -maxdepth 1 -name "$2" -printf '%T@ %p\n' 2>/dev/null \
    | sort -n | tail -1 | cut -d' ' -f2-
}

#######################################
# Closes the sandbox OBS cleanly (SIGTERM), or forcefully after 30 s.
# Arguments:
#   Process id.
#######################################
close_obs() {
  local pid="$1"
  kill -TERM "${pid}" 2>/dev/null || return 0
  local i
  for ((i = 0; i < 60; i++)); do
    if ! kill -0 "${pid}" 2>/dev/null; then
      return 0
    fi
    sleep 0.5
  done
  echo "OBS did not close in 30 s; killing it" >&2
  kill -KILL "${pid}" 2>/dev/null || true
}

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
  cmake --build build_x86_64 >/dev/null
  local media="${PWD}/build_x86_64/integration/test-media.mkv"
  if [[ ! -f "${media}" ]]; then
    mkdir -p "$(dirname "${media}")"
    tests/integration/make-test-media.sh "${media}"
  fi

  # 2. A sandbox OBS with its own configuration.
  local sandbox
  sandbox="$(mktemp -d "${TMPDIR:-/tmp}/timelapse-switch-it.XXXXXX")"
  make_sandbox "${sandbox}" "${media}" "${height}" "${encoder}"
  XDG_CONFIG_HOME="${sandbox}/config" obs --multi --minimize-to-tray \
    --disable-shutdown-check --disable-updater --profile "${PROFILE}" \
    --collection "${PROFILE}" >"${sandbox}/obs-output.txt" 2>&1 &
  local pid=$!
  trap 'close_obs '"${pid}" EXIT

  local logs="${sandbox}/config/obs-studio/logs" log=''
  local i
  for ((i = 0; i < 60; i++)); do
    sleep 0.5
    log="$(newest "${logs}" '*.txt')"
    if [[ -n "${log}" ]]; then
      break
    fi
  done
  if [[ -z "${log}" ]]; then
    echo "FAIL  OBS did not start (see ${sandbox}/obs-output.txt)" >&2
    return 1
  fi

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
