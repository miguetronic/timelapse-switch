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
	TSW_MAX_SPEED = 16,           /* fastest timelapse: x16 (REQ-002) */
	TSW_TIMELINE_CAPACITY = 4096, /* speed switches per recording; far beyond any real use */
};

/* One section of the recording, at one speed. */
struct tsw_segment {
	uint64_t start_ns;  /* OBS time where the section starts (os_gettime_ns clock) */
	uint64_t output_ns; /* position in the final video at that same moment */
	uint32_t speed;     /* 1 = real time, N = timelapse xN */
};

struct tsw_timeline {
	struct tsw_segment segments[TSW_TIMELINE_CAPACITY];
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
 * tsw_timeline_output_ns: position in the final video, in nanoseconds, of OBS time time_ns.
 *   count  number of published sections (at least 1).
 * Times before the first section map to 0.
 * Thread: any reader. Memory: none allocated.
 */
uint64_t tsw_timeline_output_ns(const struct tsw_timeline *timeline, size_t count, uint64_t time_ns);

/*
 * tsw_timeline_speed: speed of the section that contains OBS time time_ns.
 *   count  number of published sections (at least 1).
 * Returns 1 to TSW_MAX_SPEED; 1 if the arguments are invalid (after reporting a failed check).
 * Thread: any reader. Memory: none allocated.
 */
uint32_t tsw_timeline_speed(const struct tsw_timeline *timeline, size_t count, uint64_t time_ns);
