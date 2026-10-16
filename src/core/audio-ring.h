/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * audio-ring.h: hand audio blocks from OBS's audio thread to ours, without locks.
 *
 * OBS delivers the mixed audio in blocks (1024 samples per channel) on its audio thread.
 * Our recorder consumes them on another thread, slightly later. This ring is the
 * conveyor belt between the two: one producer, one consumer, fixed size, no locks, no
 * memory allocated (the caller provides the storage).
 *
 *   OBS audio thread --push--> [ block | block | block | ... ] --pop--> our audio thread
 *                               written  ----->        read  ----->
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/result.h"
#include "tsw-compiler.h"

enum {
	TSW_AUDIO_RING_BLOCKS = 64, /* about 1.4 s at 48 kHz: far more than OBS's audio delay */
	TSW_MAX_CHANNELS = 8,       /* OBS supports up to 7.1 */
};

/* One block of planar audio: one pointer per channel. */
struct tsw_audio_in {
	const float *channel[TSW_MAX_CHANNELS];
};
struct tsw_audio_out {
	float *channel[TSW_MAX_CHANNELS];
};

struct tsw_audio_ring {
	float *storage;    /* TSW_AUDIO_RING_BLOCKS * channels * frames floats, owned by the caller */
	uint32_t channels; /* channels per block */
	uint32_t frames;   /* samples per channel in each block */
	uint64_t timestamps[TSW_AUDIO_RING_BLOCKS];
	volatile long written; /* blocks pushed so far: changed only by the producer */
	volatile long read;    /* blocks popped so far: changed only by the consumer */
};

/* Floats of storage a ring needs for this layout. */
size_t tsw_audio_ring_storage_floats(uint32_t channels, uint32_t frames);

/*
 * tsw_audio_ring_init: prepare an empty ring.
 *   storage   tsw_audio_ring_storage_floats(channels, frames) floats, kept alive while the ring is used.
 *   channels  1 to TSW_MAX_CHANNELS.  frames  samples per channel in each block, at least 1.
 * Returns TSW_OK or TSW_E_INVALID.
 */
enum tsw_result tsw_audio_ring_init(struct tsw_audio_ring *ring, float *storage, uint32_t channels,
				    uint32_t frames) TSW_NODISCARD;

/*
 * tsw_audio_ring_push: copy one block in. Thread: the producer only.
 * Returns false if the ring is full (the block is dropped) or the block has the wrong size.
 */
bool tsw_audio_ring_push(struct tsw_audio_ring *ring, const struct tsw_audio_in *block, uint32_t frames,
			 uint64_t timestamp);

/*
 * tsw_audio_ring_pop: copy the oldest block out, with its timestamp. Thread: the consumer only.
 * Returns false if the ring is empty.
 */
bool tsw_audio_ring_pop(struct tsw_audio_ring *ring, const struct tsw_audio_out *block, uint64_t *timestamp);
