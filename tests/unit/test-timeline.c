/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* test-timeline.c: unit tests of src/core/timeline.c. Expected values are worked out by hand. */

#include "test.h"

#include <core/timeline.h>

static const uint64_t SECOND = 1000000000;

static struct tsw_timeline timeline; /* 4096 sections: too big for the stack */

/* Break caught: output time not following the clock in real time. */
static void real_time_follows_the_clock(void)
{
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 0, 7 * SECOND, 1), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_output_ns(&timeline, 1, 12 * SECOND), 5 * SECOND);
}

/* Break caught: timelapse not slowing the final video by its speed. */
static void timelapse_x4_advances_four_times_slower(void)
{
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 0, 0, 4), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_output_ns(&timeline, 1, 8 * SECOND), 2 * SECOND);
}

/* Break caught: the final video jumping or restarting at a switch. Example from timeline.h. */
static void switch_continues_where_the_previous_section_ended(void)
{
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 0, 0, 1), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 1, 10 * SECOND, 4), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 2, 30 * SECOND, 1), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_output_ns(&timeline, 3, 30 * SECOND), 15 * SECOND);
	TSW_EXPECT_EQ(tsw_timeline_output_ns(&timeline, 3, 40 * SECOND), 25 * SECOND);
}

/* Break caught: late audio being placed with the speed of a later section. */
static void earlier_time_uses_the_section_of_that_time(void)
{
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 0, 0, 1), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 1, 10 * SECOND, 4), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_output_ns(&timeline, 2, 6 * SECOND), 6 * SECOND);
}

/* Break caught: a frame or audio block older than the recording start wrapping around to a huge time. */
static void time_before_the_recording_maps_to_zero(void)
{
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 0, 10 * SECOND, 1), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_output_ns(&timeline, 1, 9 * SECOND), 0);
}

/* Break caught: a speed of 0 (division by zero) or above x16 being accepted. */
static void speed_outside_1_to_16_is_rejected(void)
{
	const int checks_before = tsw_check_failures;
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 0, 0, 0), TSW_E_INVALID);
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 0, 0, 17), TSW_E_INVALID);
	TSW_EXPECT_EQ(tsw_check_failures - checks_before, 2);
}

/* Break caught: a switch out of order, which would break the backwards search and the output times. */
static void switch_earlier_than_the_previous_section_is_rejected(void)
{
	const int checks_before = tsw_check_failures;
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 0, 10 * SECOND, 1), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 1, 9 * SECOND, 4), TSW_E_INVALID);
	TSW_EXPECT_EQ(tsw_check_failures - checks_before, 1);
}

/* Break caught: writing past the end of the timeline after too many switches. */
static void switch_when_the_timeline_is_full_is_refused(void)
{
	const int checks_before = tsw_check_failures;
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, TSW_TIMELINE_CAPACITY, 0, 1), TSW_E_TIMELINE_FULL);
	TSW_EXPECT_EQ(tsw_check_failures - checks_before, 0); /* a known limit, not a bug */
}

/* Break caught: reading sections that were never written (count 0) or beyond the array. */
static void reading_an_impossible_count_returns_zero(void)
{
	const int checks_before = tsw_check_failures;
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 0, 0, 1), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_output_ns(&timeline, 0, 5 * SECOND), 0);
	TSW_EXPECT_EQ(tsw_timeline_output_ns(&timeline, TSW_TIMELINE_CAPACITY + 1, 5 * SECOND), 0);
	TSW_EXPECT_EQ(tsw_check_failures - checks_before, 2);
}

/* Break caught: a missing timeline (NULL) being dereferenced and crashing OBS. */
static void missing_timeline_is_rejected(void)
{
	const int checks_before = tsw_check_failures;
	TSW_EXPECT_EQ(tsw_timeline_switch(NULL, 0, 0, 1), TSW_E_INVALID);
	TSW_EXPECT_EQ(tsw_timeline_output_ns(NULL, 1, 5 * SECOND), 0);
	TSW_EXPECT_EQ(tsw_check_failures - checks_before, 2);
}

int main(void)
{
	real_time_follows_the_clock();
	timelapse_x4_advances_four_times_slower();
	switch_continues_where_the_previous_section_ended();
	earlier_time_uses_the_section_of_that_time();
	time_before_the_recording_maps_to_zero();
	speed_outside_1_to_16_is_rejected();
	switch_earlier_than_the_previous_section_is_rejected();
	switch_when_the_timeline_is_full_is_refused();
	reading_an_impossible_count_returns_zero();
	missing_timeline_is_rejected();
	return tsw_test_failures;
}
