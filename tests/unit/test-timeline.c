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

/* Break caught: a frame or late audio block paced with the speed of the wrong section. */
static void speed_is_the_one_of_the_section_at_that_time(void)
{
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 0, 0, 1), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 1, 10 * SECOND, 8), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_speed(&timeline, 2, 9 * SECOND), 1);
	TSW_EXPECT_EQ(tsw_timeline_speed(&timeline, 2, 10 * SECOND), 8);
	TSW_EXPECT_EQ(tsw_timeline_speed(&timeline, 2, 60 * SECOND), 8);
}

/* Break caught: reading the speed of a missing timeline or of sections never written. */
static void speed_of_an_impossible_request_is_real_time(void)
{
	const int checks_before = tsw_check_failures;
	TSW_EXPECT_EQ(tsw_timeline_speed(NULL, 1, 0), 1);
	TSW_EXPECT_EQ(tsw_timeline_speed(&timeline, 0, 0), 1);
	TSW_EXPECT_EQ(tsw_timeline_speed(&timeline, TSW_TIMELINE_CAPACITY + 1, 0), 1);
	TSW_EXPECT_EQ(tsw_check_failures - checks_before, 3);
}

/* Break caught: the final video moving on while the recording is paused. */
static void pause_holds_the_final_video_still(void)
{
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 0, 0, 1), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_pause(&timeline, 1, 10 * SECOND), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_output_ns(&timeline, 2, 20 * SECOND), 10 * SECOND);
}

/* Break caught: a gap or a jump in the final video where the pause was. */
static void resume_continues_where_the_pause_began(void)
{
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 0, 0, 1), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_pause(&timeline, 1, 10 * SECOND), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 2, 25 * SECOND, 1), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_output_ns(&timeline, 3, 30 * SECOND), 15 * SECOND);
}

/* Break caught: a pause in timelapse losing or adding final video time. */
static void pause_inside_timelapse_resumes_at_the_same_place(void)
{
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 0, 0, 4), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_pause(&timeline, 1, 8 * SECOND), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 2, 20 * SECOND, 4), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_output_ns(&timeline, 3, 20 * SECOND), 2 * SECOND);
	TSW_EXPECT_EQ(tsw_timeline_output_ns(&timeline, 3, 24 * SECOND), 3 * SECOND);
}

/* Break caught: frames and audio of a pause being treated as real time or timelapse. */
static void speed_during_a_pause_is_paused(void)
{
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 0, 0, 4), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_pause(&timeline, 1, 10 * SECOND), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_speed(&timeline, 2, 15 * SECOND), TSW_SPEED_PAUSED);
	TSW_EXPECT_EQ(tsw_timeline_speed(&timeline, 2, 5 * SECOND), 4);
}

/* Break caught: the "recorded" counter counting the time spent paused. */
static void recorded_time_leaves_out_the_pauses(void)
{
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 0, 2 * SECOND, 1), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_pause(&timeline, 1, 12 * SECOND), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 2, 27 * SECOND, 4), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_recorded_ns(&timeline, 3, 20 * SECOND), 10 * SECOND); /* during the pause */
	TSW_EXPECT_EQ(tsw_timeline_recorded_ns(&timeline, 3, 32 * SECOND), 15 * SECOND);
	TSW_EXPECT_EQ(tsw_timeline_recorded_ns(&timeline, 3, 1 * SECOND), 0); /* before the start */
}

/* Break caught: a recording starting paused, or a second pause in a row hiding the speed to resume at. */
static void pause_cannot_start_a_recording_nor_follow_a_pause(void)
{
	const int checks_before = tsw_check_failures;
	TSW_EXPECT_EQ(tsw_timeline_pause(&timeline, 0, 0), TSW_E_INVALID);
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 0, 0, 1), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_pause(&timeline, 1, 10 * SECOND), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_pause(&timeline, 2, 12 * SECOND), TSW_E_INVALID);
	TSW_EXPECT_EQ(tsw_check_failures - checks_before, 2);
}

/* Break caught: a pause written past the end of a full timeline, or taking the last free section, so
 * that its resume would not fit and the recording would stay paused for ever. */
static void pause_needs_room_for_its_resume(void)
{
	TSW_EXPECT_EQ(tsw_timeline_pause(&timeline, TSW_TIMELINE_CAPACITY, 0), TSW_E_TIMELINE_FULL);

	/* One free section left: no pause, its resume would not fit. */
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, TSW_TIMELINE_CAPACITY - 3, 0, 1), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, TSW_TIMELINE_CAPACITY - 2, 0, 4), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_pause(&timeline, TSW_TIMELINE_CAPACITY - 1, SECOND), TSW_E_TIMELINE_FULL);

	/* Two free sections: the pause, then its resume in the very last one. */
	TSW_EXPECT_EQ(tsw_timeline_pause(&timeline, TSW_TIMELINE_CAPACITY - 2, SECOND), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, TSW_TIMELINE_CAPACITY - 1, 2 * SECOND, 1), TSW_OK);
}

/* Break caught: the details telling a wrong split of the session (a pause counted as recording, a
 * resume at the same speed counted as a switch). */
static void totals_split_real_time_timelapse_and_pauses(void)
{
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 0, 0, 1), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 1, 10 * SECOND, 4), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_pause(&timeline, 2, 30 * SECOND), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 3, 40 * SECOND, 4), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 4, 44 * SECOND, 1), TSW_OK);
	struct tsw_timeline_totals totals;
	tsw_timeline_get_totals(&timeline, 5, 50 * SECOND, &totals);
	TSW_EXPECT_EQ(totals.real_time_ns, 16 * SECOND);
	TSW_EXPECT_EQ(totals.timelapse_ns, 24 * SECOND);
	TSW_EXPECT_EQ(totals.paused_ns, 10 * SECOND);
	TSW_EXPECT_EQ(totals.switches, 2);
	TSW_EXPECT_EQ(totals.pauses, 1);
}

/* Break caught: totals counting time after now, or before the start. */
static void totals_stop_at_the_time_asked(void)
{
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 0, 5 * SECOND, 1), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 1, 15 * SECOND, 8), TSW_OK);
	struct tsw_timeline_totals totals;
	tsw_timeline_get_totals(&timeline, 2, 12 * SECOND, &totals);
	TSW_EXPECT_EQ(totals.real_time_ns, 7 * SECOND);
	TSW_EXPECT_EQ(totals.timelapse_ns, 0);
	tsw_timeline_get_totals(&timeline, 2, 1 * SECOND, &totals);
	TSW_EXPECT_EQ(totals.real_time_ns + totals.timelapse_ns + totals.paused_ns, 0);
}

/* Break caught: the speed map showing sections in the wrong place, or missing a pause. */
static void map_shows_each_slice_at_its_speed(void)
{
	uint8_t map[TSW_MAP_BUCKETS];
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 0, 0, 1), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 1, 12 * SECOND, 4), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_pause(&timeline, 2, 24 * SECOND), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 3, 36 * SECOND, 1), TSW_OK);
	tsw_timeline_map(&timeline, 4, 48 * SECOND, map); /* 48 slices of 1 s */
	TSW_EXPECT_EQ(map[0], 1);
	TSW_EXPECT_EQ(map[11], 1);
	TSW_EXPECT_EQ(map[12], 4);
	TSW_EXPECT_EQ(map[23], 4);
	TSW_EXPECT_EQ(map[24], TSW_SPEED_PAUSED);
	TSW_EXPECT_EQ(map[35], TSW_SPEED_PAUSED);
	TSW_EXPECT_EQ(map[36], 1);
	TSW_EXPECT_EQ(map[47], 1);
}

/* Break caught: a slice shared by two sections taking the smaller part. */
static void map_slice_takes_the_speed_that_covers_most_of_it(void)
{
	uint8_t map[TSW_MAP_BUCKETS];
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 0, 0, 1), TSW_OK);
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 1, 10300 * (SECOND / 1000), 8), TSW_OK);
	tsw_timeline_map(&timeline, 2, 48 * SECOND, map);
	TSW_EXPECT_EQ(map[9], 1);
	TSW_EXPECT_EQ(map[10], 8); /* 10 s to 11 s: 0.3 s in real time, 0.7 s at x8 */
}

/* Break caught: an empty map (garbage) right at the start. */
static void map_before_any_time_shows_the_first_speed(void)
{
	uint8_t map[TSW_MAP_BUCKETS];
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 0, 5 * SECOND, 4), TSW_OK);
	tsw_timeline_map(&timeline, 1, 5 * SECOND, map);
	TSW_EXPECT_EQ(map[0], 4);
	TSW_EXPECT_EQ(map[TSW_MAP_BUCKETS - 1], 4);
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
	speed_is_the_one_of_the_section_at_that_time();
	speed_of_an_impossible_request_is_real_time();
	pause_holds_the_final_video_still();
	resume_continues_where_the_pause_began();
	pause_inside_timelapse_resumes_at_the_same_place();
	speed_during_a_pause_is_paused();
	recorded_time_leaves_out_the_pauses();
	pause_cannot_start_a_recording_nor_follow_a_pause();
	pause_needs_room_for_its_resume();
	totals_split_real_time_timelapse_and_pauses();
	totals_stop_at_the_time_asked();
	map_shows_each_slice_at_its_speed();
	map_slice_takes_the_speed_that_covers_most_of_it();
	map_before_any_time_shows_the_first_speed();
	return tsw_test_failures;
}
