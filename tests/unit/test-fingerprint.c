/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* test-fingerprint.c: the cheap fingerprint that tells a repeated picture (core/fingerprint.h). */

#include "test.h"

#include <core/fingerprint.h>

enum { WIDTH = 64, HEIGHT = 36, LINESIZE = 80 };

static uint8_t plane[LINESIZE * HEIGHT];

static void fill(uint8_t seed)
{
	for (size_t i = 0; i < sizeof(plane); i++)
		plane[i] = (uint8_t)(seed + i * 7);
}

/* Break caught: the same picture giving two fingerprints (repeats never counted). */
static void same_picture_same_fingerprint(void)
{
	fill(3);
	const uint64_t first = tsw_fingerprint(plane, LINESIZE, WIDTH, HEIGHT);
	fill(3);
	TSW_EXPECT_EQ(tsw_fingerprint(plane, LINESIZE, WIDTH, HEIGHT), first);
}

/* Break caught: a new picture taken for a repeat (camera noise changes many pixels a little). */
static void a_small_change_changes_the_fingerprint(void)
{
	fill(3);
	const uint64_t first = tsw_fingerprint(plane, LINESIZE, WIDTH, HEIGHT);
	for (size_t i = 0; i < sizeof(plane); i++)
		plane[i] ^= 1; /* noise: every pixel one level off */
	TSW_EXPECT_TRUE(tsw_fingerprint(plane, LINESIZE, WIDTH, HEIGHT) != first);
}

/* Break caught: reading the padding at the end of each line (outside the picture), which may hold garbage. */
static void padding_is_never_read(void)
{
	fill(3);
	const uint64_t first = tsw_fingerprint(plane, LINESIZE, WIDTH, HEIGHT);
	for (size_t y = 0; y < HEIGHT; y++)
		for (size_t x = WIDTH; x < LINESIZE; x++)
			plane[y * LINESIZE + x] = 0xAA;
	TSW_EXPECT_EQ(tsw_fingerprint(plane, LINESIZE, WIDTH, HEIGHT), first);
}

/* Break caught: a crash or garbage on a missing or empty plane. */
static void missing_plane_gives_zero(void)
{
	TSW_EXPECT_EQ(tsw_fingerprint(NULL, LINESIZE, WIDTH, HEIGHT), 0);
	TSW_EXPECT_EQ(tsw_fingerprint(plane, LINESIZE, 0, HEIGHT), 0);
}

int main(void)
{
	same_picture_same_fingerprint();
	a_small_change_changes_the_fingerprint();
	padding_is_never_read();
	missing_plane_gives_zero();
	return tsw_test_failures;
}
