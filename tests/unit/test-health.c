/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* test-health.c: when the recording needs attention, and silence otherwise (core/health.h). */

#include "test.h"

#include <core/health.h>

static const uint64_t SECOND = 1000000000;
static const uint64_t MB = 1000000;

static struct tsw_health_input healthy(void)
{
	struct tsw_health_input input = {0};
	input.feed_waiting = 1;
	input.feed_size = 16;
	input.disk_free_bytes = 500000 * MB;
	input.bytes_per_second = 2 * MB;
	return input;
}

/* Break caught: a warning shown while everything is fine (noise during a recording). */
static void all_is_well_means_silence(void)
{
	const struct tsw_health_input input = healthy();
	struct tsw_health health;
	tsw_health_evaluate(&input, &health);
	TSW_EXPECT_EQ(health.issues, 0);
}

/* Break caught: lost frames not reported (TSW-E201). */
static void lost_frames_are_reported(void)
{
	struct tsw_health_input input = healthy();
	input.frames_lost = 3;
	struct tsw_health health;
	tsw_health_evaluate(&input, &health);
	TSW_EXPECT_EQ(health.issues, TSW_HEALTH_LOST_FRAMES);
}

/* Break caught: the encoder falling behind noticed too late, or a normal small backlog reported. */
static void encoder_behind_from_half_the_feed(void)
{
	struct tsw_health_input input = healthy();
	struct tsw_health health;
	input.feed_waiting = 7;
	tsw_health_evaluate(&input, &health);
	TSW_EXPECT_EQ(health.issues, 0);
	input.feed_waiting = 8;
	tsw_health_evaluate(&input, &health);
	TSW_EXPECT_EQ(health.issues, TSW_HEALTH_ENCODER_BEHIND);
}

/* Break caught: the disk filling up without warning, or a warning with plenty of space (TSW-E102). */
static void disk_low_under_ten_minutes(void)
{
	struct tsw_health_input input = healthy();
	struct tsw_health health;
	input.disk_free_bytes = 600 * MB; /* 300 s at 2 MB/s */
	tsw_health_evaluate(&input, &health);
	TSW_EXPECT_EQ(health.issues, TSW_HEALTH_DISK_LOW);
	TSW_EXPECT_EQ(health.disk_seconds_left, 300);
	input.disk_free_bytes = 6000 * MB; /* 3000 s */
	tsw_health_evaluate(&input, &health);
	TSW_EXPECT_EQ(health.issues, 0);
	TSW_EXPECT_EQ(health.disk_seconds_left, 3000);
}

/* Break caught: a disk warning made up from nothing (no rate yet, or paused: the file is not growing). */
static void unknown_disk_never_warns(void)
{
	struct tsw_health_input input = healthy();
	struct tsw_health health;
	input.disk_free_bytes = 10 * MB;
	input.bytes_per_second = 0;
	tsw_health_evaluate(&input, &health);
	TSW_EXPECT_EQ(health.issues, 0);
	TSW_EXPECT_EQ(health.disk_seconds_left, UINT64_MAX);
}

/* Break caught: several issues hiding each other. */
static void several_issues_are_all_kept(void)
{
	struct tsw_health_input input = healthy();
	input.frames_lost = 1;
	input.feed_waiting = 16;
	input.disk_free_bytes = 60 * MB;
	struct tsw_health health;
	tsw_health_evaluate(&input, &health);
	TSW_EXPECT_EQ(health.issues, (uint32_t)TSW_HEALTH_DISK_LOW | (uint32_t)TSW_HEALTH_LOST_FRAMES |
					     (uint32_t)TSW_HEALTH_ENCODER_BEHIND);
}

/* Break caught: a wrong file growth rate, so the minutes of disk left are wrong. */
static void disk_meter_measures_the_growth_rate(void)
{
	struct tsw_disk_meter meter;
	tsw_disk_meter_reset(&meter);
	TSW_EXPECT_EQ(tsw_disk_meter_rate(&meter), 0);
	tsw_disk_meter_add(&meter, 0, 0);
	TSW_EXPECT_EQ(tsw_disk_meter_rate(&meter), 0); /* one sample: unknown */
	for (uint64_t s = 1; s <= 40; s++)
		tsw_disk_meter_add(&meter, s * SECOND, s * 3 * MB);
	TSW_EXPECT_EQ(tsw_disk_meter_rate(&meter), 3 * MB);
}

/* Break caught: the rate kept from long ago after the file stops growing (a pause): it must fall to 0. */
static void disk_meter_forgets_old_samples(void)
{
	struct tsw_disk_meter meter;
	tsw_disk_meter_reset(&meter);
	for (uint64_t s = 0; s < 10; s++)
		tsw_disk_meter_add(&meter, s * SECOND, s * 3 * MB);
	for (uint64_t s = 10; s < 10 + TSW_DISK_SAMPLES; s++)
		tsw_disk_meter_add(&meter, s * SECOND, 27 * MB);
	TSW_EXPECT_EQ(tsw_disk_meter_rate(&meter), 0);
}

int main(void)
{
	all_is_well_means_silence();
	lost_frames_are_reported();
	encoder_behind_from_half_the_feed();
	disk_low_under_ten_minutes();
	unknown_disk_never_warns();
	several_issues_are_all_kept();
	disk_meter_measures_the_growth_rate();
	disk_meter_forgets_old_samples();
	return tsw_test_failures;
}
