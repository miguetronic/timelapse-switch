#!/bin/bash
#
# Timelapse Switch
# SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
# SPDX-License-Identifier: GPL-2.0-or-later
#
# The sandbox OBS shared by run.sh and measure.sh: a separate OBS with its own
# temporary configuration (XDG_CONFIG_HOME), so the user's OBS settings,
# scenes and plugins are never touched; its obs-websocket listens on port
# 4466 instead of the usual 4455. Source this file; it does nothing alone.

readonly PORT=4466
readonly PROFILE='tsw-test'

#######################################
# Writes the OBS configuration of the sandbox: profile, scene, websocket,
# plugin settings, and installs the freshly built plugin and, if asked, the
# sync probe.
# Arguments:
#   Sandbox directory, test media file, resolution height, encoder id,
#   1 to install the sync probe or 0 not to. The probe asks OBS for every
#   frame in memory, so a performance measurement must leave it out.
#######################################
make_sandbox() {
  local sandbox="$1" media="$2" height="$3" encoder="$4" probe="$5"
  local width=$((height * 16 / 9))
  local obs="${sandbox}/config/obs-studio"
  mkdir -p "${obs}/basic/profiles/${PROFILE}" "${obs}/basic/scenes" \
    "${obs}/plugin_config/obs-websocket" \
    "${obs}/plugin_config/timelapse-switch" \
    "${obs}/plugins/timelapse-switch/bin/64bit" \
    "${obs}/plugins/timelapse-switch/data" "${sandbox}/recordings"

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
  if ((probe == 1)); then
    # The test-only module that logs when OBS delivers each beep and frame.
    mkdir -p "${obs}/plugins/tsw-sync-probe/bin/64bit"
    cp build_x86_64/integration/tsw-sync-probe.so \
      "${obs}/plugins/tsw-sync-probe/bin/64bit/"
  fi
}

# Prints the newest file matching a pattern in a directory, or nothing.
newest() {
  find "$1" -maxdepth 1 -name "$2" -printf '%T@ %p\n' 2>/dev/null \
    | sort -n | tail -1 | cut -d' ' -f2-
}

#######################################
# Builds the plugin and makes the test media once.
# Outputs:
#   The path of the test media.
#######################################
prepare_build() {
  cmake --build build_x86_64 >/dev/null
  local media="${PWD}/build_x86_64/integration/test-media.mkv"
  if [[ ! -f "${media}" ]]; then
    mkdir -p "$(dirname "${media}")"
    tests/integration/make-test-media.sh "${media}" >&2
  fi
  echo "${media}"
}

#######################################
# Starts the sandbox OBS and waits until it writes its log.
# Globals:
#   OBS_PID   set to the process id of the sandbox OBS.
#   OBS_LOG   set to the path of its log file.
# Arguments:
#   Sandbox directory.
# Returns:
#   0 once OBS writes its log, 1 if it does not within 30 s.
#######################################
start_obs() {
  local sandbox="$1"
  XDG_CONFIG_HOME="${sandbox}/config" obs --multi --minimize-to-tray \
    --disable-shutdown-check --disable-updater --profile "${PROFILE}" \
    --collection "${PROFILE}" >"${sandbox}/obs-output.txt" 2>&1 &
  OBS_PID=$!
  OBS_LOG=''
  local logs="${sandbox}/config/obs-studio/logs"
  local i
  for ((i = 0; i < 60; i++)); do
    sleep 0.5
    OBS_LOG="$(newest "${logs}" '*.txt')"
    if [[ -n "${OBS_LOG}" ]]; then
      return 0
    fi
  done
  echo "FAIL  OBS did not start (see ${sandbox}/obs-output.txt)" >&2
  close_obs "${OBS_PID}"
  return 1
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
