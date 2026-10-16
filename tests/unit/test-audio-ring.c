/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* test-audio-ring.c: unit tests of src/core/audio-ring.c. Expected values are worked out by hand. */

#include "test.h"

#include <core/audio-ring.h>

enum { CHANNELS = 2, FRAMES = 4 }; /* tiny blocks keep the expected values readable */

static float storage[TSW_AUDIO_RING_BLOCKS * CHANNELS * FRAMES];
static struct tsw_audio_ring ring;

/* Block number n: left channel n.0, n.1, n.2, n.3; right channel the same plus 100. */
static void make_block(float left[FRAMES], float right[FRAMES], int n, struct tsw_audio_in *block)
{
	for (int i = 0; i < FRAMES; i++) {
		left[i] = (float)n + 0.1f * (float)i;
		right[i] = (float)n + 100.0f + 0.1f * (float)i;
	}
	block->channel[0] = left;
	block->channel[1] = right;
}

/* Break caught: samples, channels or the timestamp mixed up on the way through the ring. */
static void block_comes_out_as_it_went_in(void)
{
	float left[FRAMES], right[FRAMES], out_left[FRAMES], out_right[FRAMES];
	struct tsw_audio_in in = {0};
	struct tsw_audio_out out = {{out_left, out_right}};
	uint64_t timestamp = 0;
	TSW_EXPECT_EQ(tsw_audio_ring_init(&ring, storage, CHANNELS, FRAMES), TSW_OK);
	make_block(left, right, 7, &in);
	TSW_EXPECT_TRUE(tsw_audio_ring_push(&ring, &in, FRAMES, 123456789));
	TSW_EXPECT_TRUE(tsw_audio_ring_pop(&ring, &out, &timestamp));
	TSW_EXPECT_EQ(timestamp, 123456789);
	for (int i = 0; i < FRAMES; i++) {
		TSW_EXPECT_TRUE(out_left[i] == left[i]);
		TSW_EXPECT_TRUE(out_right[i] == right[i]);
	}
}

/* Break caught: the consumer reading a slot the producer has not filled yet (stale or garbage audio). */
static void empty_ring_gives_nothing(void)
{
	float out_left[FRAMES], out_right[FRAMES];
	struct tsw_audio_out out = {{out_left, out_right}};
	uint64_t timestamp = 0;
	TSW_EXPECT_EQ(tsw_audio_ring_init(&ring, storage, CHANNELS, FRAMES), TSW_OK);
	TSW_EXPECT_TRUE(!tsw_audio_ring_pop(&ring, &out, &timestamp));
}

/* Break caught: a full ring overwriting audio the consumer has not read yet. */
static void full_ring_refuses_the_block_and_keeps_the_oldest(void)
{
	float left[FRAMES], right[FRAMES], out_left[FRAMES], out_right[FRAMES];
	struct tsw_audio_in in = {0};
	struct tsw_audio_out out = {{out_left, out_right}};
	uint64_t timestamp = 0;
	TSW_EXPECT_EQ(tsw_audio_ring_init(&ring, storage, CHANNELS, FRAMES), TSW_OK);
	for (int n = 0; n < TSW_AUDIO_RING_BLOCKS; n++) {
		make_block(left, right, n, &in);
		TSW_EXPECT_TRUE(tsw_audio_ring_push(&ring, &in, FRAMES, (uint64_t)n));
	}
	make_block(left, right, 999, &in);
	TSW_EXPECT_TRUE(!tsw_audio_ring_push(&ring, &in, FRAMES, 999));
	TSW_EXPECT_TRUE(tsw_audio_ring_pop(&ring, &out, &timestamp));
	TSW_EXPECT_EQ(timestamp, 0);
	TSW_EXPECT_TRUE(out_left[0] == 0.0f);
}

/* Break caught: blocks out of order or overwritten when the ring wraps around (positions vs slots). */
static void blocks_keep_their_order_over_many_laps(void)
{
	float left[FRAMES], right[FRAMES], out_left[FRAMES], out_right[FRAMES];
	struct tsw_audio_in in = {0};
	struct tsw_audio_out out = {{out_left, out_right}};
	uint64_t timestamp = 0;
	int pushed = 0, popped = 0, out_of_order = 0;
	TSW_EXPECT_EQ(tsw_audio_ring_init(&ring, storage, CHANNELS, FRAMES), TSW_OK);
	/* Push 3, pop 2, until 1000 blocks went through: the ring fills up and wraps around about 15 times. */
	while (popped < 1000) {
		for (int i = 0; i < 3; i++) {
			make_block(left, right, pushed, &in);
			if (tsw_audio_ring_push(&ring, &in, FRAMES, (uint64_t)pushed))
				pushed++;
		}
		for (int i = 0; i < 2 && tsw_audio_ring_pop(&ring, &out, &timestamp); i++) {
			out_of_order += (timestamp != (uint64_t)popped) || (out_left[0] != (float)popped);
			popped++;
		}
	}
	TSW_EXPECT_EQ(out_of_order, 0);
}

/* Break caught: a ring set up with an impossible layout, or fed a block of the wrong size (memory overrun). */
static void impossible_layouts_and_sizes_are_rejected(void)
{
	float left[FRAMES], right[FRAMES], out_left[FRAMES], out_right[FRAMES];
	struct tsw_audio_in in = {0};
	struct tsw_audio_out out = {{out_left, out_right}};
	uint64_t timestamp = 0;
	const int checks_before = tsw_check_failures;
	TSW_EXPECT_EQ(tsw_audio_ring_init(NULL, storage, CHANNELS, FRAMES), TSW_E_INVALID);
	TSW_EXPECT_EQ(tsw_audio_ring_init(&ring, NULL, CHANNELS, FRAMES), TSW_E_INVALID);
	TSW_EXPECT_EQ(tsw_audio_ring_init(&ring, storage, 0, FRAMES), TSW_E_INVALID);
	TSW_EXPECT_EQ(tsw_audio_ring_init(&ring, storage, TSW_MAX_CHANNELS + 1, FRAMES), TSW_E_INVALID);
	TSW_EXPECT_EQ(tsw_audio_ring_init(&ring, storage, CHANNELS, 0), TSW_E_INVALID);
	TSW_EXPECT_EQ(tsw_audio_ring_init(&ring, storage, CHANNELS, FRAMES), TSW_OK);
	make_block(left, right, 1, &in);
	TSW_EXPECT_TRUE(!tsw_audio_ring_push(&ring, &in, FRAMES + 1, 0)); /* would read past the caller's block */
	TSW_EXPECT_TRUE(!tsw_audio_ring_push(NULL, &in, FRAMES, 0));
	TSW_EXPECT_TRUE(!tsw_audio_ring_push(&ring, NULL, FRAMES, 0));
	TSW_EXPECT_TRUE(!tsw_audio_ring_pop(NULL, &out, &timestamp));
	TSW_EXPECT_TRUE(!tsw_audio_ring_pop(&ring, NULL, &timestamp));
	TSW_EXPECT_TRUE(!tsw_audio_ring_pop(&ring, &out, NULL));
	TSW_EXPECT_EQ(tsw_check_failures - checks_before, 11);
}

int main(void)
{
	block_comes_out_as_it_went_in();
	empty_ring_gives_nothing();
	full_ring_refuses_the_block_and_keeps_the_oldest();
	blocks_keep_their_order_over_many_laps();
	impossible_layouts_and_sizes_are_rejected();
	return tsw_test_failures;
}
