/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* audio-ring.c: see audio-ring.h. */

#include "core/audio-ring.h"

#include "core/check.h"

#include <string.h>

#include <util/threading.h> /* os_atomic_*: OBS's portable atomics, header only */

/* First float of the block stored at position index (positions grow forever; slots wrap around). */
static float *block_storage(const struct tsw_audio_ring *ring, long index)
{
	const size_t slot = (size_t)index % TSW_AUDIO_RING_BLOCKS;
	return ring->storage + slot * ring->channels * ring->frames;
}

size_t tsw_audio_ring_storage_floats(uint32_t channels, uint32_t frames)
{
	return (size_t)TSW_AUDIO_RING_BLOCKS * channels * frames;
}

enum tsw_result tsw_audio_ring_init(struct tsw_audio_ring *ring, float *storage, uint32_t channels, uint32_t frames)
{
	if (!TSW_CHECK(ring != NULL && storage != NULL))
		return TSW_E_INVALID;
	if (!TSW_CHECK(channels >= 1 && channels <= TSW_MAX_CHANNELS && frames >= 1))
		return TSW_E_INVALID;

	ring->storage = storage;
	ring->channels = channels;
	ring->frames = frames;
	ring->written = 0;
	ring->read = 0;
	return TSW_OK;
}

bool tsw_audio_ring_push(struct tsw_audio_ring *ring, const struct tsw_audio_in *block, uint32_t frames,
			 uint64_t timestamp)
{
	if (!TSW_CHECK(ring != NULL && block != NULL))
		return false;
	if (!TSW_CHECK(frames == ring->frames))
		return false;

	/* 1. Copy the samples into the next free slot, if there is one. */
	const long written = ring->written; /* only this thread changes it */
	if (written - os_atomic_load_long(&ring->read) >= TSW_AUDIO_RING_BLOCKS)
		return false;
	float *destination = block_storage(ring, written);
	for (uint32_t channel = 0; channel < ring->channels; channel++) {
		const float *source = block->channel[channel];
		memcpy(destination + (size_t)channel * ring->frames, source, ring->frames * sizeof(float));
	}
	ring->timestamps[(size_t)written % TSW_AUDIO_RING_BLOCKS] = timestamp;

	/* 2. Publish it: the consumer sees the new count only after the samples are in place. */
	os_atomic_set_long(&ring->written, written + 1);
	return true;
}

bool tsw_audio_ring_pop(struct tsw_audio_ring *ring, const struct tsw_audio_out *block, uint64_t *timestamp)
{
	if (!TSW_CHECK(ring != NULL && block != NULL && timestamp != NULL))
		return false;

	/* 1. Copy the oldest block out, if there is one. */
	const long read = ring->read; /* only this thread changes it */
	if (read == os_atomic_load_long(&ring->written))
		return false;
	const float *source = block_storage(ring, read);
	for (uint32_t channel = 0; channel < ring->channels; channel++) {
		float *destination = block->channel[channel];
		memcpy(destination, source + (size_t)channel * ring->frames, ring->frames * sizeof(float));
	}
	*timestamp = ring->timestamps[(size_t)read % TSW_AUDIO_RING_BLOCKS];

	/* 2. Free the slot for the producer. */
	os_atomic_set_long(&ring->read, read + 1);
	return true;
}
