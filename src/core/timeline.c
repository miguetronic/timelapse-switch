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

uint64_t tsw_timeline_output_ns(const struct tsw_timeline *timeline, size_t count, uint64_t time_ns)
{
	if (!TSW_CHECK(timeline != NULL))
		return 0;
	if (!TSW_CHECK(count >= 1 && count <= TSW_TIMELINE_CAPACITY))
		return 0;

	const struct tsw_segment *segment = segment_at(timeline, count, time_ns);
	if (time_ns < segment->start_ns)
		return segment->output_ns; /* only before the first section: the recording has not started */
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
