#!/bin/bash
#
# Timelapse Switch
# SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Writes the test video used by the integration test.
#
#   tests/integration/make-test-media.sh <output.mkv>
#
# 5 minutes at 30 fps with a stereo 48 kHz track:
#   picture  16 vertical bars that spell the frame number in binary (bit 0 on
#            the left, white = 1), so any frame of a recording can be traced
#            back to the exact moment it shows;
#   sound    a 1 kHz beep of 50 ms at the start of every second, i.e. exactly
#            on the frames whose number is a multiple of 30.
# Big flat bars survive any encoder, so the analysis can read them back.

set -euo pipefail

readonly SECONDS_LONG=300
readonly FPS=30

main() {
  local output="$1"
  local beep='0.5*sin(2*PI*1000*t)*lt(mod(t\,1)\,0.05)'
  # The bars are computed on a 16x1 picture (one pixel per bit) and then
  # enlarged, which is much faster than computing every pixel.
  ffmpeg -v error -y \
    -f lavfi -i "color=c=black:s=16x1:r=${FPS}:d=${SECONDS_LONG},format=gray,\
geq=lum='if(mod(floor(N/pow(2\,X))\,2)\,235\,16)',\
scale=640:360:flags=neighbor,format=yuv420p" \
    -f lavfi -i "aevalsrc=${beep}|${beep}:s=48000:d=${SECONDS_LONG}" \
    -c:v ffv1 -c:a pcm_s16le -shortest "${output}"
  echo "Test media written: ${output}"
}

main "$@"
