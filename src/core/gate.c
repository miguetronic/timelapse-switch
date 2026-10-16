/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* gate.c: see gate.h. */

#include "core/gate.h"

#include "core/check.h"
#include "core/timeline.h"

static const uint64_t SECOND_NS = 1000000000;

/*
 * floor(a * b / c) without overflowing 64 bits, for our ranges: a up to days in nanoseconds,
 * b up to a frame rate numerator or a sample rate (well under 2^20), c up to 10^12.
 */
static uint64_t mul_div(uint64_t a, uint64_t b, uint64_t c)
{
	return (a / c) * b + (a % c) * b / c;
}

enum tsw_result tsw_video_gate_init(struct tsw_video_gate *gate, uint32_t fps_num, uint32_t fps_den)
{
	if (!TSW_CHECK(gate != NULL))
		return TSW_E_INVALID;
	if (!TSW_CHECK(fps_num > 0 && fps_den > 0))
		return TSW_E_INVALID;

	gate->fps_num = fps_num;
	gate->fps_den = fps_den;
	gate->frames_out = 0;
	gate->origin_ns = 0;
	return TSW_OK;
}

uint32_t tsw_video_gate_copies(struct tsw_video_gate *gate, uint64_t output_ns, uint32_t speed)
{
	if (!TSW_CHECK(gate != NULL))
		return 0;
	if (!TSW_CHECK(speed >= 1 && speed <= TSW_MAX_SPEED))
		return 0;

	/* 1. The first frame opens slot 0 wherever it falls (frames arrive late, at any phase); later frames
	 *    are placed from there. One output frame lasts frame_ns_x_num / fps_num nanoseconds (kept as a
	 *    fraction: 29.97 fps is not whole). */
	if (gate->frames_out == 0)
		gate->origin_ns = output_ns;
	const uint64_t position_ns = (output_ns > gate->origin_ns) ? output_ns - gate->origin_ns : 0;
	const uint64_t frame_ns_x_num = SECOND_NS * gate->fps_den;

	/* 2. Frames due by now: every frame whose slot starts before this position plus half an input step,
	 *    so the input frame nearest to each slot is the one sent. */
	const uint64_t half_step_ns = frame_ns_x_num / (2u * (uint64_t)gate->fps_num * speed);
	const uint64_t frames_due = mul_div(position_ns + half_step_ns, gate->fps_num, frame_ns_x_num) + 1;

	/* 3. Send what is missing: usually 1 or 0, more if OBS skipped frames. */
	if (frames_due <= gate->frames_out)
		return 0;
	const uint64_t copies = frames_due - gate->frames_out;
	gate->frames_out = frames_due;
	return (uint32_t)copies;
}

enum tsw_result tsw_audio_gate_init(struct tsw_audio_gate *gate, uint32_t sample_rate)
{
	if (!TSW_CHECK(gate != NULL))
		return TSW_E_INVALID;
	if (!TSW_CHECK(sample_rate > 0))
		return TSW_E_INVALID;

	gate->sample_rate = sample_rate;
	gate->samples_out = 0;
	return TSW_OK;
}

bool tsw_audio_gate_pass(struct tsw_audio_gate *gate, uint64_t output_end_ns, uint32_t frames)
{
	if (!TSW_CHECK(gate != NULL))
		return false;

	/* 1. How far the audio should reach once this block is over, in samples (nearest sample). */
	const uint64_t half_sample_ns = SECOND_NS / (2u * (uint64_t)gate->sample_rate);
	const uint64_t samples_due = mul_div(output_end_ns + half_sample_ns, gate->sample_rate, SECOND_NS);

	/* 2. Send the block if that brings the audio closer to where it should be. By induction the
	 *    audio then never strays more than half a block from its place, even when the block
	 *    straddles a switch (a speed-based rule fails exactly there). */
	if (gate->samples_out + frames / 2 > samples_due)
		return false;
	gate->samples_out += frames;
	return true;
}
