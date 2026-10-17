/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* timeline.c: see timeline.h. */

#include "core/timeline.h"

#include "core/check.h"

/* The section that contains time_ns. Searches backwards: callers ask about recent times. */
static const struct tsw_segment *segment_at(const struct tsw_timeline *timeline, size_t count, uint64_t time_ns)
{
	for (size_t i = count; i > 1; i--) {
		if (timeline->segments[i - 1].start_ns <= time_ns)
			return &timeline->segments[i - 1];
	}
	return &timeline->segments[0];
}

enum tsw_result tsw_timeline_switch(struct tsw_timeline *timeline, size_t count, uint64_t start_ns, uint32_t speed)
{
	if (!TSW_CHECK(timeline != NULL))
		return TSW_E_INVALID;
	if (count >= TSW_TIMELINE_CAPACITY)
		return TSW_E_TIMELINE_FULL;
	if (!TSW_CHECK(speed >= 1 && speed <= TSW_MAX_SPEED))
		return TSW_E_INVALID;
	if (count > 0 && !TSW_CHECK(start_ns >= timeline->segments[count - 1].start_ns))
		return TSW_E_INVALID;

	struct tsw_segment *segment = &timeline->segments[count];
	segment->output_ns = (count == 0) ? 0 : tsw_timeline_output_ns(timeline, count, start_ns);
	segment->start_ns = start_ns;
	segment->speed = speed;
	return TSW_OK;
}

enum tsw_result tsw_timeline_pause(struct tsw_timeline *timeline, size_t count, uint64_t start_ns)
{
	if (!TSW_CHECK(timeline != NULL))
		return TSW_E_INVALID;
	if (count + 1 >= TSW_TIMELINE_CAPACITY)
		return TSW_E_TIMELINE_FULL; /* keep a section free for the resume: a pause never gets stuck */
	if (!TSW_CHECK(count > 0 && timeline->segments[count - 1].speed != TSW_SPEED_PAUSED))
		return TSW_E_INVALID;
	if (!TSW_CHECK(start_ns >= timeline->segments[count - 1].start_ns))
		return TSW_E_INVALID;

	struct tsw_segment *segment = &timeline->segments[count];
	segment->output_ns = tsw_timeline_output_ns(timeline, count, start_ns);
	segment->start_ns = start_ns;
	segment->speed = TSW_SPEED_PAUSED;
	return TSW_OK;
}

uint64_t tsw_timeline_output_ns(const struct tsw_timeline *timeline, size_t count, uint64_t time_ns)
{
	if (!TSW_CHECK(timeline != NULL))
		return 0;
	if (!TSW_CHECK(count >= 1 && count <= TSW_TIMELINE_CAPACITY))
		return 0;

	const struct tsw_segment *segment = segment_at(timeline, count, time_ns);
	if (time_ns < segment->start_ns || segment->speed == TSW_SPEED_PAUSED)
		return segment->output_ns; /* before the first section, or paused: the final video stands still */
	return segment->output_ns + (time_ns - segment->start_ns) / segment->speed;
}

uint32_t tsw_timeline_speed(const struct tsw_timeline *timeline, size_t count, uint64_t time_ns)
{
	if (!TSW_CHECK(timeline != NULL))
		return 1;
	if (!TSW_CHECK(count >= 1 && count <= TSW_TIMELINE_CAPACITY))
		return 1;
	return segment_at(timeline, count, time_ns)->speed;
}

uint64_t tsw_timeline_recorded_ns(const struct tsw_timeline *timeline, size_t count, uint64_t time_ns)
{
	if (!TSW_CHECK(timeline != NULL))
		return 0;
	if (!TSW_CHECK(count >= 1 && count <= TSW_TIMELINE_CAPACITY))
		return 0;

	/* Add up the part of each section before time_ns, except the paused ones. */
	uint64_t recorded_ns = 0;
	for (size_t i = 0; i < count; i++) {
		const struct tsw_segment *segment = &timeline->segments[i];
		if (segment->start_ns >= time_ns)
			break;
		const uint64_t end_ns = (i + 1 < count && timeline->segments[i + 1].start_ns < time_ns)
						? timeline->segments[i + 1].start_ns
						: time_ns;
		if (segment->speed != TSW_SPEED_PAUSED)
			recorded_ns += end_ns - segment->start_ns;
	}
	return recorded_ns;
}
