/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * gate.h: turn "where are we in the final video" into "what goes to the encoder".
 *
 * The timeline (timeline.h) gives, for each frame and audio block OBS produces, its
 * position in the final video. The gates compare that position with what has already
 * been sent and decide:
 *   video: how many copies of this frame to send (0 = skip it, 1 = send it, 2+ = OBS
 *          skipped frames, so repeat this one to keep the video in time);
 *   audio: whether this block of samples is sent.
 * Both pick the unit nearest to where it belongs, so rounding errors never add up.
 *
 * Analogy: a ticket gate on a moving walkway. People (frames) keep arriving; the gate
 * lets one through each time the walkway has moved one step.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "core/result.h"
#include "tsw-compiler.h"

struct tsw_video_gate {
	uint32_t fps_num; /* frame rate as a fraction, as OBS gives it: 30/1, 30000/1001 */
	uint32_t fps_den;
	uint64_t frames_out; /* frames sent to the encoder so far */
	uint64_t origin_ns;  /* final-video position of the first frame: slot 0 starts there */
};

struct tsw_audio_gate {
	uint32_t sample_rate; /* samples per second, as OBS gives it: 48000 */
	uint64_t samples_out; /* samples sent to the encoder so far */
};

/*
 * tsw_video_gate_init: prepare a gate for a new recording.
 * Returns TSW_OK or TSW_E_INVALID (frame rate of zero).
 */
enum tsw_result tsw_video_gate_init(struct tsw_video_gate *gate, uint32_t fps_num, uint32_t fps_den) TSW_NODISCARD;

/*
 * tsw_video_gate_copies: how many copies of the current frame to send.
 *   output_ns  position of this frame in the final video (tsw_timeline_output_ns).
 *   speed      speed of the section this frame belongs to (1 to TSW_MAX_SPEED).
 * Returns 0, 1, or more when OBS skipped frames. Updates frames_out.
 * Thread: the video thread. Memory: none allocated.
 */
uint32_t tsw_video_gate_copies(struct tsw_video_gate *gate, uint64_t output_ns, uint32_t speed);

/*
 * tsw_audio_gate_init: prepare a gate for a new recording.
 * Returns TSW_OK or TSW_E_INVALID (sample rate of zero).
 */
enum tsw_result tsw_audio_gate_init(struct tsw_audio_gate *gate, uint32_t sample_rate) TSW_NODISCARD;

/*
 * tsw_audio_gate_pass: whether the current block of audio is sent.
 *   output_end_ns  position in the final video of the END of the block (the moment just after its
 *                  last sample), from tsw_timeline_output_ns.
 *   frames         samples per channel in the block (OBS uses 1024).
 * Returns true if the block is sent; then samples_out grows by frames.
 * Thread: the audio thread. Memory: none allocated.
 */
bool tsw_audio_gate_pass(struct tsw_audio_gate *gate, uint64_t output_end_ns, uint32_t frames);
