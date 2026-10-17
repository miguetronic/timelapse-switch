/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * health.h: whether the recording needs the user's attention, and nothing when it does not
 * (docs/design/ux-spec.md, 3.17). Pure logic with no OBS dependency, unit tested.
 *
 *   counters of the recorder ----> [ health ] ----> issues: none (silence), or
 *   free disk, file size rate ---->                  disk low, frames lost, encoder behind
 *
 * The disk meter turns the size of the file, sampled about once per second, into how fast it
 * grows, so the free space can be told in minutes.
 */

#pragma once

#include <stdint.h>

#include "tsw-compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
	TSW_DISK_SAMPLES = 16,       /* the disk meter's window: about 15 s at one sample per second */
	TSW_DISK_WARN_SECONDS = 600, /* warn when less than 10 minutes of space are left (TSW-E102) */
};

/* How fast the file grows, from its last TSW_DISK_SAMPLES sizes. */
struct tsw_disk_meter {
	uint64_t at_ns[TSW_DISK_SAMPLES];
	uint64_t bytes[TSW_DISK_SAMPLES];
	uint32_t count; /* samples held, up to TSW_DISK_SAMPLES */
	uint32_t next;  /* where the next sample goes */
};

/* Issues, as bits of tsw_health.issues; the dock shows the most urgent one (ux-spec 3.17). */
enum tsw_health_issue {
	TSW_HEALTH_DISK_LOW = 1,       /* TSW-E102: less than TSW_DISK_WARN_SECONDS of space left */
	TSW_HEALTH_LOST_FRAMES = 2,    /* TSW-E201: frames lost because the computer could not keep up */
	TSW_HEALTH_ENCODER_BEHIND = 4, /* TSW-E203: half the feed or more waits for the encoder */
};

struct tsw_health_input {
	uint64_t frames_lost;      /* since the start */
	uint32_t feed_waiting;     /* frames waiting for the encoder now */
	uint32_t feed_size;        /* frames the feed can hold */
	uint64_t disk_free_bytes;  /* free space where the file is written; 0 = unknown */
	uint64_t bytes_per_second; /* how fast the file grows; 0 = unknown or not growing */
};

struct tsw_health {
	uint32_t issues;            /* bits of enum tsw_health_issue; 0 = all is well */
	uint64_t disk_seconds_left; /* at the current rate; UINT64_MAX when unknown */
};

/* tsw_disk_meter_reset: an empty meter, for a new recording. Thread: UI. */
void tsw_disk_meter_reset(struct tsw_disk_meter *meter);

/* tsw_disk_meter_add: the file's size at now_ns (about once per second). Thread: UI. */
void tsw_disk_meter_add(struct tsw_disk_meter *meter, uint64_t now_ns, uint64_t file_bytes);

/* tsw_disk_meter_rate: bytes per second over the window; 0 if unknown (fewer than 2 samples) or not growing. */
uint64_t tsw_disk_meter_rate(const struct tsw_disk_meter *meter);

/* tsw_health_evaluate: the issues of the recording now. Thread: UI. Memory: none allocated. */
void tsw_health_evaluate(const struct tsw_health_input *input, struct tsw_health *health);

#ifdef __cplusplus
}
#endif
