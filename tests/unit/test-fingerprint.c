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

enum { ODD_ROW = 61 }; /* bytes per row that are not a multiple of 8: the hash's last, shorter step */

/* Break caught: road C's verification (ADR-012) reporting two identical frames as different because
 * their lines are laid out with different padding. */
static void whole_hash_ignores_padding(void)
{
	static uint8_t narrow[ODD_ROW * HEIGHT];
	fill(5);
	for (size_t y = 0; y < HEIGHT; y++)
		for (size_t x = 0; x < ODD_ROW; x++)
			narrow[y * ODD_ROW + x] = plane[y * LINESIZE + x];
	const uint64_t padded = tsw_plane_hash(TSW_HASH_START, plane, LINESIZE, ODD_ROW, HEIGHT);
	TSW_EXPECT_EQ(tsw_plane_hash(TSW_HASH_START, narrow, ODD_ROW, ODD_ROW, HEIGHT), padded);
}

/* Break caught: road C's verification missing a difference in a single byte, wherever it is: the first,
 * the last one of a row (in the shorter step), or the very last of the plane. */
static void whole_hash_sees_one_byte(void)
{
	const size_t places[3] = {0, 2 * LINESIZE + ODD_ROW - 1, (HEIGHT - 1) * LINESIZE + ODD_ROW - 1};
	fill(5);
	const uint64_t first = tsw_plane_hash(TSW_HASH_START, plane, LINESIZE, ODD_ROW, HEIGHT);
	for (size_t i = 0; i < 3; i++) {
		plane[places[i]] ^= 1;
		TSW_EXPECT_TRUE(tsw_plane_hash(TSW_HASH_START, plane, LINESIZE, ODD_ROW, HEIGHT) != first);
		plane[places[i]] ^= 1;
	}
	TSW_EXPECT_EQ(tsw_plane_hash(TSW_HASH_START, plane, LINESIZE, ODD_ROW, HEIGHT), first);
}

/* Break caught: the same bytes in another plane giving the same hash (planes swapped unnoticed). */
static void whole_hash_depends_on_what_came_before(void)
{
	fill(5);
	const uint64_t once = tsw_plane_hash(TSW_HASH_START, plane, LINESIZE, WIDTH, HEIGHT);
	TSW_EXPECT_TRUE(tsw_plane_hash(once, plane, LINESIZE, WIDTH, HEIGHT) != once);
}

int main(void)
{
	same_picture_same_fingerprint();
	a_small_change_changes_the_fingerprint();
	padding_is_never_read();
	missing_plane_gives_zero();
	whole_hash_ignores_padding();
	whole_hash_sees_one_byte();
	whole_hash_depends_on_what_came_before();
	return tsw_test_failures;
}
