/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * recorder-internal.h: what recorder.c and recorder-rt.c share. No other module includes it.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <media-io/audio-io.h>
#include <media-io/video-io.h>
#include <obs.h>
#include <util/threading.h>

#include "core/audio-ring.h"
#include "core/gate.h"
#include "core/timeline.h"

struct tsw_recorder {
	/* Read by the real-time threads. */
	struct tsw_timeline timeline;
	volatile long sections;           /* published sections of the timeline */
	volatile bool capturing;          /* frames and audio blocks are taken in */
	struct tsw_video_gate video_gate; /* OBS video thread only */
	struct tsw_audio_gate audio_gate; /* our audio thread only */
	struct tsw_audio_ring ring;       /* OBS audio thread to our audio thread */
	uint64_t block_ns;                /* duration of one audio block */
	enum video_format format;         /* frame layout, for the copy */
	uint32_t height;

	/* Counters for the summary line; each one has a single writer. */
	volatile long frames_in, frames_sent, frames_lost, blocks_sent, ring_full, ring_empty;
	volatile long feed_peak; /* most frames that waited in our video feed for the encoder */
	/* Diagnostics, written once by their real-time thread: first unit sent, and how late data arrives. */
	uint64_t first_frame_ns, first_block_ns, frame_delay_ns, block_delay_ns;

	/* Owned from start until the file is saved; released by release_media(). */
	float *ring_storage;
	video_t *video;
	audio_t *audio;
	obs_encoder_t *video_encoder;
	obs_encoder_t *audio_encoder;
	obs_output_t *output;
	char *path;
	size_t mix; /* OBS audio track that is recorded */

	pthread_mutex_t switch_mutex; /* one switch at a time (UI and hotkey threads) */
	os_event_t *saved;            /* signaled when the output has stopped */
	volatile bool active;
	bool stopping; /* stop was asked; UI thread only */

	/* Flush after stop (see tsw_recorder_stop): written by the UI thread, read by the graphics thread. */
	volatile bool flushing;  /* send every frame, so the encoder hands over the ones it holds */
	volatile bool forced;    /* the file writer was told to stop */
	long flush_target;       /* frames sent before the stop: all of them must reach the file */
	uint64_t flush_start_ns; /* when stop was asked */
	long flush_seen;         /* graphics thread only: frames written at the last check */
	uint64_t flush_seen_ns;  /* graphics thread only: when that count last grew */
	volatile long speed;
	uint64_t start_ns;
	uint64_t stop_ns;          /* when stop was asked; 0 before */
	bool chapters_unavailable; /* the format has no chapters; logged once per recording */
	uint32_t markers;          /* chapter markers added by hand in this recording */

	/* The last recording that ended, or failed to start (tsw_recorder_get_status). UI thread only. */
	uint32_t outcome;    /* grows by 1 each time */
	uint32_t error_code; /* its TSW-Exxx number, 0 if saved fine */
	uint64_t last_recorded_ns;
	uint64_t last_final_ns;
	char *last_path; /* owned: replaced at the next outcome, freed in tsw_recorder_destroy */
};

/* The real-time callbacks (recorder-rt.c); recorder.c hands them to OBS. */
void tsw_recorder_receive_frame(void *param, struct video_data *frame);
void tsw_recorder_receive_audio(void *param, size_t mix, struct audio_data *data);
bool tsw_recorder_provide_audio(void *param, uint64_t start_ts, uint64_t end_ts, uint64_t *new_ts, uint32_t mixers,
				struct audio_output_data *mixes);
