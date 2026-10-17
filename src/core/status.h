/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * status.h: what the recorder reports to the dock and the hotkeys, a few times per second.
 * Filled by tsw_recorder_get_status (recorder.h); read by the session and the dock view.
 */

#pragma once

#include <stdint.h>

#include "core/timeline.h"

enum tsw_phase {
	TSW_PHASE_IDLE,      /* no recording (also after one ended) */
	TSW_PHASE_RECORDING, /* frames and audio are recorded */
	TSW_PHASE_PAUSED,    /* recording, but paused: nothing reaches the file until it resumes */
	TSW_PHASE_SAVING,    /* stop was asked; the file is being finalized */
};

struct tsw_status {
	enum tsw_phase phase;
	uint32_t speed;               /* while recording: 1 = real time, N = timelapse xN; while paused: the
					 speed in use before the pause */
	uint64_t recorded_ns;         /* real time recorded since the start, pauses left out */
	uint64_t final_ns;            /* length of the final video so far */
	uint64_t section_recorded_ns; /* real time since the last switch (while paused: how long it has been paused) */
	uint64_t section_final_ns;    /* what that adds to the final video */
	const char *path;             /* the file being recorded, or the last one; borrowed, may be empty */

	/* The last recording that ended, or failed to start. */
	uint32_t outcome;          /* grows by 1 each time; 0 = none yet in this OBS session */
	uint32_t error_code;       /* 0 = saved fine; otherwise the number of its TSW-Exxx code */
	uint64_t last_recorded_ns; /* its recorded time and final length */
	uint64_t last_final_ns;

	/* Monitoring while recording, paused or saving (docs/design/ux-spec.md, 3.17); zero when idle. */
	struct tsw_timeline_totals totals; /* where the time went so far */
	uint8_t map[TSW_MAP_BUCKETS];      /* the speed map of the recording so far */
	uint64_t frames_lost;              /* frames lost because the encoder could not keep up */
	uint32_t feed_waiting;             /* frames waiting for the encoder now */
	uint32_t feed_size;                /* frames the feed can hold */
	uint64_t file_bytes;               /* size of the file so far; 0 = unknown */
	uint64_t disk_free_bytes;          /* free space where it is written; 0 = unknown */
	uint64_t bytes_per_second;         /* how fast the file grows; 0 = unknown or not growing */
};
