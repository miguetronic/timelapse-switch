/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * timeline.h: where each moment of the recording lands in the final video.
 *
 * The recording is a list of sections, each at one speed. A section starts when the
 * user switches speed. Given any OBS time, the timeline answers "what time is that in
 * the final video?". Real time advances the final video 1:1; timelapse xN advances it
 * N times slower.
 *
 *   OBS time:     0s ......... 10s ..................... 30s ........ 40s
 *   speed:        x1 (real)    x4 (timelapse)            x1
 *   final video:  0s ......... 10s ........ 15s          15s ........ 25s
 *
 * A pause is one more section, in which the final video does not move at all: nothing
 * recorded while paused reaches the file, and the next section continues from there.
 *
 * Pure logic with no OBS dependency: one writer appends sections, readers on other
 * threads read them; entries [0, count) are complete and never change, so a reader only
 * needs the count the writer published (the plugin publishes it atomically).
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "core/result.h"
#include "tsw-compiler.h"

enum {
	TSW_SPEED_PAUSED = 0,         /* the speed of a paused section: the final video stands still */
	TSW_MAX_SPEED = 16,           /* fastest timelapse: x16 (REQ-002) */
	TSW_TIMELINE_CAPACITY = 4096, /* speed switches per recording; far beyond any real use */
	TSW_MAP_BUCKETS = 48,         /* slices of the speed map (tsw_timeline_map) */
};

/* One section of the recording, at one speed. */
struct tsw_segment {
	uint64_t start_ns;  /* OBS time where the section starts (os_gettime_ns clock) */
	uint64_t output_ns; /* position in the final video at that same moment */
	uint32_t speed;     /* 1 = real time, N = timelapse xN, TSW_SPEED_PAUSED = paused */
};

struct tsw_timeline {
	struct tsw_segment segments[TSW_TIMELINE_CAPACITY];
};

/* Where the time of a recording went, for the dock's details and the log (tsw_timeline_get_totals). */
struct tsw_timeline_totals {
	uint64_t real_time_ns; /* recorded in real time */
	uint64_t timelapse_ns; /* recorded in timelapse, before it was sped up */
	uint64_t paused_ns;    /* spent paused */
	uint32_t switches;     /* changes of speed; a pause, or resuming at the same speed, is not one */
	uint32_t pauses;
};

/*
 * tsw_timeline_switch: start a new section at start_ns with the given speed.
 *   count     number of sections already in the timeline (0 to start a recording).
 *   start_ns  OBS time of the switch; never earlier than the previous section.
 *   speed     1 to TSW_MAX_SPEED.
 * Returns TSW_OK, TSW_E_INVALID or TSW_E_TIMELINE_FULL. On success the caller publishes count + 1.
 * Thread: the single writer. Memory: none allocated.
 */
enum tsw_result tsw_timeline_switch(struct tsw_timeline *timeline, size_t count, uint64_t start_ns,
				    uint32_t speed) TSW_NODISCARD;

/*
 * tsw_timeline_pause: start a paused section at start_ns; the next tsw_timeline_switch resumes.
 *   count     number of sections already in the timeline: at least 1 (a recording never starts
 *             paused), and the last one must not be paused already.
 *   start_ns  OBS time of the pause; never earlier than the previous section.
 * Returns TSW_OK, TSW_E_INVALID or TSW_E_TIMELINE_FULL. A pause is refused unless one more section
 * stays free after it, so its resume always fits. On success the caller publishes count + 1.
 * Thread: the single writer. Memory: none allocated.
 */
enum tsw_result tsw_timeline_pause(struct tsw_timeline *timeline, size_t count, uint64_t start_ns) TSW_NODISCARD;

/*
 * tsw_timeline_output_ns: position in the final video, in nanoseconds, of OBS time time_ns.
 *   count  number of published sections (at least 1).
 * Times before the first section map to 0.
 * Thread: any reader. Memory: none allocated.
 */
uint64_t tsw_timeline_output_ns(const struct tsw_timeline *timeline, size_t count, uint64_t time_ns);

/*
 * tsw_timeline_speed: speed of the section that contains OBS time time_ns.
 *   count  number of published sections (at least 1).
 * Returns 1 to TSW_MAX_SPEED, or TSW_SPEED_PAUSED inside a pause; 1 if the arguments are invalid
 * (after reporting a failed check).
 * Thread: any reader. Memory: none allocated.
 */
uint32_t tsw_timeline_speed(const struct tsw_timeline *timeline, size_t count, uint64_t time_ns);

/*
 * tsw_timeline_recorded_ns: real time recorded from the start up to OBS time time_ns, pauses left out.
 *   count  number of published sections (at least 1).
 * Returns 0 before the first section, or if the arguments are invalid (after a failed check).
 * Thread: the UI thread (it walks every section, at most TSW_TIMELINE_CAPACITY). Memory: none allocated.
 */
uint64_t tsw_timeline_recorded_ns(const struct tsw_timeline *timeline, size_t count, uint64_t time_ns);

/*
 * tsw_timeline_get_totals: where the time from the start up to OBS time time_ns went.
 *   count  number of published sections (at least 1).
 * The final video is real_time_ns plus each timelapse stretch divided by its speed (tsw_timeline_output_ns).
 * Thread: the UI thread (it walks every section). Memory: none allocated.
 */
void tsw_timeline_get_totals(const struct tsw_timeline *timeline, size_t count, uint64_t time_ns,
			     struct tsw_timeline_totals *totals);

/*
 * tsw_timeline_map: the recording from its start up to time_ns, cut into TSW_MAP_BUCKETS equal slices of
 * OBS time; each slice gets the speed that covers most of it (1, 2 to TSW_MAX_SPEED, or TSW_SPEED_PAUSED).
 * Before any time has passed, every slice gets the speed of the first section.
 * Thread: the UI thread. Memory: none allocated.
 */
void tsw_timeline_map(const struct tsw_timeline *timeline, size_t count, uint64_t time_ns,
		      uint8_t map[TSW_MAP_BUCKETS]);
