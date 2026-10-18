/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* test-road-compare.c: unit tests of src/core/road-compare.c. Expected values are worked out by hand. */

#include "test.h"

#include <core/check.h>
#include <core/road-compare.h>

enum { CAPACITY = 16 };

static struct tsw_road_record storage_b[CAPACITY], storage_c[CAPACITY];
static struct tsw_road_list b, c;

/* Fresh lists: road B with frames first..last of B, road C with frames first..last of C (1 frame = 10 ns
 * here), each with hash = its timestamp, so both roads agree unless a test changes one. */
static void fill(uint64_t first_b, uint64_t last_b, uint64_t first_c, uint64_t last_c)
{
	TSW_EXPECT_EQ(tsw_road_list_init(&b, storage_b, CAPACITY), TSW_OK);
	TSW_EXPECT_EQ(tsw_road_list_init(&c, storage_c, CAPACITY), TSW_OK);
	for (uint64_t frame = first_b; frame <= last_b; frame++)
		TSW_EXPECT_TRUE(tsw_road_list_add(&b, frame * 10, frame * 10));
	for (uint64_t frame = first_c; frame <= last_c; frame++)
		TSW_EXPECT_TRUE(tsw_road_list_add(&c, frame * 10, frame * 10));
}

/* Break caught: the frames at either end, when only one road was running, counted as differences. */
static void only_the_time_both_ran_is_compared(void)
{
	struct tsw_road_comparison comparison;
	fill(1, 8, 3, 10); /* both ran from frame 3 to frame 8 */
	tsw_road_compare(&b, &c, &comparison);
	TSW_EXPECT_EQ(comparison.compared, 6);
	TSW_EXPECT_EQ(comparison.different, 0);
	TSW_EXPECT_EQ(comparison.only_b, 0);
	TSW_EXPECT_EQ(comparison.only_c, 0);
	TSW_EXPECT_EQ(comparison.first_different_ns, 0);
}

/* Break caught: a frame whose bytes differ (a color or layout mistake on road C) going unnoticed. */
static void a_different_frame_is_found(void)
{
	struct tsw_road_comparison comparison;
	fill(1, 8, 1, 8);
	storage_c[4].hash ^= 1; /* frame 5 */
	storage_c[6].hash ^= 1; /* frame 7 */
	tsw_road_compare(&b, &c, &comparison);
	TSW_EXPECT_EQ(comparison.compared, 8);
	TSW_EXPECT_EQ(comparison.different, 2);
	TSW_EXPECT_EQ(comparison.first_different_ns, 50);
}

/* Break caught: a difference reported without saying which road is wrong: road C handing over the picture
 * road B had one frame before (road C late), the reverse (road B late), or a picture road B never had. */
static void a_difference_says_which_road_is_late(void)
{
	struct tsw_road_comparison comparison;
	fill(1, 8, 1, 8);                      /* hash = timestamp */
	storage_c[3].hash = storage_b[2].hash; /* frame 4 on road C: road B's frame 3 picture */
	storage_b[5].hash = storage_c[4].hash; /* frame 6 on road B: road C's frame 5 picture */
	storage_c[6].hash = 999;               /* frame 7 on road C: a picture no road had */
	tsw_road_compare(&b, &c, &comparison);
	TSW_EXPECT_EQ(comparison.different, 3);
	TSW_EXPECT_EQ(comparison.c_late, 1);
	TSW_EXPECT_EQ(comparison.b_late, 1);
	TSW_EXPECT_EQ(comparison.unmatched, 1);
}

/* Break caught: road C missing a frame, or handing one over at a time road B never had (wrong repeats). */
static void frames_on_one_road_only_are_counted(void)
{
	struct tsw_road_comparison comparison;
	TSW_EXPECT_EQ(tsw_road_list_init(&b, storage_b, CAPACITY), TSW_OK);
	TSW_EXPECT_EQ(tsw_road_list_init(&c, storage_c, CAPACITY), TSW_OK);
	const uint64_t frames_b[6] = {10, 20, 30, 40, 50, 60};
	const uint64_t frames_c[6] = {10, 20, 35, 40, 50, 60}; /* 30 missing, 35 extra */
	for (size_t i = 0; i < 6; i++) {
		TSW_EXPECT_TRUE(tsw_road_list_add(&b, frames_b[i], 1));
		TSW_EXPECT_TRUE(tsw_road_list_add(&c, frames_c[i], 1));
	}
	tsw_road_compare(&b, &c, &comparison);
	TSW_EXPECT_EQ(comparison.compared, 5);
	TSW_EXPECT_EQ(comparison.only_b, 1);
	TSW_EXPECT_EQ(comparison.only_c, 1);
}

/* Break caught: writing past the storage when a recording is longer than the list. */
static void a_full_list_writes_nothing_more(void)
{
	TSW_EXPECT_EQ(tsw_road_list_init(&b, storage_b, 2), TSW_OK);
	TSW_EXPECT_TRUE(tsw_road_list_add(&b, 10, 1));
	TSW_EXPECT_TRUE(tsw_road_list_add(&b, 20, 1));
	TSW_EXPECT_TRUE(!tsw_road_list_add(&b, 30, 1));
	TSW_EXPECT_EQ(b.count, 2);
	TSW_EXPECT_EQ(b.full, 1);
}

/* Break caught: an empty road (it never ran) reported as a perfect match. */
static void an_empty_road_compares_nothing(void)
{
	struct tsw_road_comparison comparison;
	fill(1, 4, 1, 0); /* road C empty */
	tsw_road_compare(&b, &c, &comparison);
	TSW_EXPECT_EQ(comparison.compared, 0);
	TSW_EXPECT_EQ(comparison.different, 0);
}

/* Break caught: misuse accepted silently. */
static void misuse_is_refused(void)
{
	const int checks_before = tsw_check_failures;
	TSW_EXPECT_EQ(tsw_road_list_init(&b, NULL, CAPACITY), TSW_E_INVALID);
	TSW_EXPECT_EQ(tsw_road_list_init(&b, storage_b, 0), TSW_E_INVALID);
	TSW_EXPECT_EQ(tsw_check_failures - checks_before, 2);
}

int main(void)
{
	only_the_time_both_ran_is_compared();
	a_different_frame_is_found();
	frames_on_one_road_only_are_counted();
	a_difference_says_which_road_is_late();
	a_full_list_writes_nothing_more();
	an_empty_road_compares_nothing();
	misuse_is_refused();
	return tsw_test_failures;
}
