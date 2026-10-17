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

/* End of section i as seen at time_ns: where the next one starts, or time_ns for the last one. */
static uint64_t section_end(const struct tsw_timeline *timeline, size_t count, size_t i, uint64_t time_ns)
{
	if (i + 1 < count && timeline->segments[i + 1].start_ns < time_ns)
		return timeline->segments[i + 1].start_ns;
	return time_ns;
}

void tsw_timeline_get_totals(const struct tsw_timeline *timeline, size_t count, uint64_t time_ns,
			     struct tsw_timeline_totals *totals)
{
	if (!TSW_CHECK(totals != NULL))
		return;
	const struct tsw_timeline_totals none = {0};
	*totals = none;
	if (!TSW_CHECK(timeline != NULL && count >= 1 && count <= TSW_TIMELINE_CAPACITY))
		return;

	uint32_t last_speed = 0; /* speed of the last section that was not a pause; 0 = none yet */
	for (size_t i = 0; i < count && timeline->segments[i].start_ns < time_ns; i++) {
		const struct tsw_segment *segment = &timeline->segments[i];
		const uint64_t length_ns = section_end(timeline, count, i, time_ns) - segment->start_ns;
		if (segment->speed == TSW_SPEED_PAUSED) {
			totals->pauses++;
			totals->paused_ns += length_ns;
			continue;
		}
		if (last_speed != 0 && segment->speed != last_speed)
			totals->switches++;
		last_speed = segment->speed;
		if (segment->speed == 1)
			totals->real_time_ns += length_ns;
		else
			totals->timelapse_ns += length_ns;
	}
}

/* The speed covering most of [lo_ns, hi_ns), from section *first on; *first moves past the sections that
 * end before lo_ns, so walking the slices in order walks the sections once. */
static uint8_t slice_speed(const struct tsw_timeline *timeline, size_t count, uint64_t time_ns, size_t *first,
			   uint64_t lo_ns, uint64_t hi_ns)
{
	while (*first + 1 < count && section_end(timeline, count, *first, time_ns) <= lo_ns)
		(*first)++;
	uint64_t cover[TSW_MAX_SPEED + 1] = {0};
	for (size_t i = *first; i < count && timeline->segments[i].start_ns < hi_ns; i++) {
		const uint64_t from = timeline->segments[i].start_ns > lo_ns ? timeline->segments[i].start_ns : lo_ns;
		const uint64_t end = section_end(timeline, count, i, time_ns);
		const uint64_t to = end < hi_ns ? end : hi_ns;
		if (to > from)
			cover[timeline->segments[i].speed] += to - from;
	}
	uint8_t best = (uint8_t)timeline->segments[*first].speed;
	for (uint32_t speed = 0; speed <= TSW_MAX_SPEED; speed++) {
		if (cover[speed] > cover[best])
			best = (uint8_t)speed;
	}
	return best;
}

void tsw_timeline_map(const struct tsw_timeline *timeline, size_t count, uint64_t time_ns, uint8_t map[TSW_MAP_BUCKETS])
{
	if (!TSW_CHECK(map != NULL))
		return;
	if (!TSW_CHECK(timeline != NULL && count >= 1 && count <= TSW_TIMELINE_CAPACITY)) {
		for (size_t b = 0; b < TSW_MAP_BUCKETS; b++)
			map[b] = 1;
		return;
	}
	const uint64_t start_ns = timeline->segments[0].start_ns;
	const uint64_t span_ns = time_ns > start_ns ? time_ns - start_ns : 0;
	size_t first = 0;
	for (size_t b = 0; b < TSW_MAP_BUCKETS; b++) {
		if (span_ns == 0) {
			map[b] = (uint8_t)timeline->segments[0].speed;
			continue;
		}
		const uint64_t lo_ns = start_ns + span_ns * b / TSW_MAP_BUCKETS;
		const uint64_t hi_ns = start_ns + span_ns * (b + 1) / TSW_MAP_BUCKETS;
		map[b] = slice_speed(timeline, count, time_ns, &first, lo_ns, hi_ns);
	}
}
