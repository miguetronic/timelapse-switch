/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* test-gate.c: unit tests of src/core/gate.c. Expected values are worked out by hand. */

#include "test.h"

#include <core/gate.h>

static const uint64_t SECOND = 1000000000;

/* Final video position of input frame k at 30 fps, at the given speed: k / 30 / speed seconds. */
static uint64_t frame_position_30fps(uint64_t k, uint64_t speed)
{
	return k * SECOND / 30 / speed;
}

/* Break caught: real-time frames being dropped or doubled. */
static void real_time_sends_every_frame_once(void)
{
	struct tsw_video_gate gate;
	TSW_EXPECT_EQ(tsw_video_gate_init(&gate, 30, 1), TSW_OK);
	uint64_t sent = 0;
	for (uint64_t k = 0; k < 30; k++) {
		const uint32_t copies = tsw_video_gate_copies(&gate, frame_position_30fps(k, 1), 1);
		TSW_EXPECT_EQ(copies, 1);
		sent += copies;
	}
	TSW_EXPECT_EQ(sent, 30);
}

/* Break caught: timelapse keeping the wrong share of frames, or an uneven rhythm (jerky playback). */
static void timelapse_x4_sends_one_frame_in_four_at_a_steady_rhythm(void)
{
	struct tsw_video_gate gate;
	TSW_EXPECT_EQ(tsw_video_gate_init(&gate, 30, 1), TSW_OK);
	for (uint64_t k = 0; k < 40; k++) {
		const uint32_t expected = (k % 4 == 0) ? 1 : 0; /* frames 0, 4, 8, ... */
		TSW_EXPECT_EQ(tsw_video_gate_copies(&gate, frame_position_30fps(k, 4), 4), expected);
	}
	TSW_EXPECT_EQ(gate.frames_out, 10);
}

/*
 * Break caught: the frame rate fraction mishandled (29.97 fps treated as 30, or its denominator
 * ignored), which makes the video drift. Rounding the frame duration to whole nanoseconds is
 * harmless at this scale: 0.05 frames in 24 hours, absorbed by rounding to the nearest frame.
 */
static void real_time_at_29_97_fps_never_drifts_over_24_hours(void)
{
	struct tsw_video_gate gate;
	TSW_EXPECT_EQ(tsw_video_gate_init(&gate, 30000, 1001), TSW_OK);
	uint64_t not_once = 0;
	/* 24 h at 30000/1001 fps: frames k = 0 .. 2589410 start before 86400 s (hand-computed). */
	for (uint64_t k = 0; k <= 2589410; k++) {
		if (tsw_video_gate_copies(&gate, k * 1001 * SECOND / 30000, 1) != 1)
			not_once++;
	}
	TSW_EXPECT_EQ(not_once, 0);
	TSW_EXPECT_EQ(gate.frames_out, 2589411);
}

/* Break caught: a frame OBS skipped making the video fall behind the audio for the rest of the recording. */
static void frame_skipped_by_obs_is_covered_by_a_repeat(void)
{
	struct tsw_video_gate gate;
	TSW_EXPECT_EQ(tsw_video_gate_init(&gate, 30, 1), TSW_OK);
	for (uint64_t k = 0; k < 3; k++)
		TSW_EXPECT_EQ(tsw_video_gate_copies(&gate, frame_position_30fps(k, 1), 1), 1);
	/* frame 3 never arrives */
	TSW_EXPECT_EQ(tsw_video_gate_copies(&gate, frame_position_30fps(4, 1), 1), 2);
	TSW_EXPECT_EQ(gate.frames_out, 5);
}

/* Final video position of the END of audio block k (1024 samples at 48 kHz), at the given speed. */
static uint64_t block_end_position_48k(uint64_t k, uint64_t speed)
{
	return (k + 1) * 1024 * SECOND / 48000 / speed;
}

/* Break caught: real-time audio losing blocks (gaps) or repeating them. */
static void real_time_sends_every_audio_block(void)
{
	struct tsw_audio_gate gate;
	TSW_EXPECT_EQ(tsw_audio_gate_init(&gate, 48000), TSW_OK);
	uint64_t not_sent = 0;
	for (uint64_t k = 0; k < 1000; k++) {
		if (!tsw_audio_gate_pass(&gate, block_end_position_48k(k, 1), 1024))
			not_sent++;
	}
	TSW_EXPECT_EQ(not_sent, 0);
	TSW_EXPECT_EQ(gate.samples_out, 1024000);
}

/* Break caught: timelapse audio keeping the wrong share of blocks, so it ends up longer or shorter than the video. */
static void timelapse_x4_sends_one_audio_block_in_four(void)
{
	struct tsw_audio_gate gate;
	TSW_EXPECT_EQ(tsw_audio_gate_init(&gate, 48000), TSW_OK);
	for (uint64_t k = 0; k < 400; k++) {
		const bool expected =
			(k % 4 == 1); /* blocks 1, 5, 9, ...: each when its end is nearest to an output slot */
		TSW_EXPECT_EQ(tsw_audio_gate_pass(&gate, block_end_position_48k(k, 4), 1024), expected);
	}
	TSW_EXPECT_EQ(gate.samples_out, 102400);
}

/* Break caught: a zero frame rate or sample rate accepted, dividing by zero later in the video or audio thread. */
static void zero_rates_are_rejected(void)
{
	const int checks_before = tsw_check_failures;
	struct tsw_video_gate video;
	struct tsw_audio_gate audio;
	TSW_EXPECT_EQ(tsw_video_gate_init(&video, 0, 1), TSW_E_INVALID);
	TSW_EXPECT_EQ(tsw_video_gate_init(&video, 30, 0), TSW_E_INVALID);
	TSW_EXPECT_EQ(tsw_audio_gate_init(&audio, 0), TSW_E_INVALID);
	TSW_EXPECT_EQ(tsw_video_gate_init(NULL, 30, 1), TSW_E_INVALID);
	TSW_EXPECT_EQ(tsw_audio_gate_init(NULL, 48000), TSW_E_INVALID);
	TSW_EXPECT_EQ(tsw_check_failures - checks_before, 5);
}

/* Break caught: a bad speed or missing gate crashing the real-time thread (division by zero, NULL). */
static void bad_speed_or_missing_gate_sends_nothing(void)
{
	const int checks_before = tsw_check_failures;
	struct tsw_video_gate video;
	TSW_EXPECT_EQ(tsw_video_gate_init(&video, 30, 1), TSW_OK);
	TSW_EXPECT_EQ(tsw_video_gate_copies(&video, 0, 0), 0);
	TSW_EXPECT_EQ(tsw_video_gate_copies(&video, 0, 17), 0);
	TSW_EXPECT_EQ(tsw_video_gate_copies(NULL, 0, 1), 0);
	TSW_EXPECT_TRUE(!tsw_audio_gate_pass(NULL, 0, 1024));
	TSW_EXPECT_EQ(tsw_check_failures - checks_before, 4);
}

/*
 * Break caught: the first frame rounded to the second slot and sent twice to fill the first one,
 * which delays the whole video by a frame. Frames reach the recorder a few frames late, so the
 * first one can sit anywhere between two slots.
 */
static void first_frame_takes_the_first_slot_wherever_it_falls(void)
{
	struct tsw_video_gate gate;
	TSW_EXPECT_EQ(tsw_video_gate_init(&gate, 30, 1), TSW_OK);
	const uint64_t first = 25000000; /* 25 ms: past the middle of the first 33.3 ms slot */
	TSW_EXPECT_EQ(tsw_video_gate_copies(&gate, first, 1), 1);
	for (uint64_t k = 1; k < 30; k++)
		TSW_EXPECT_EQ(tsw_video_gate_copies(&gate, first + frame_position_30fps(k, 1), 1), 1);
	TSW_EXPECT_EQ(gate.frames_out, 30);
}

int main(void)
{
	real_time_sends_every_frame_once();
	first_frame_takes_the_first_slot_wherever_it_falls();
	timelapse_x4_sends_one_frame_in_four_at_a_steady_rhythm();
	real_time_at_29_97_fps_never_drifts_over_24_hours();
	frame_skipped_by_obs_is_covered_by_a_repeat();
	real_time_sends_every_audio_block();
	timelapse_x4_sends_one_audio_block_in_four();
	zero_rates_are_rejected();
	bad_speed_or_missing_gate_sends_nothing();
	return tsw_test_failures;
}
