/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* stage-ring.c: see stage-ring.h. */

#include "core/stage-ring.h"

#include "core/check.h"

#include <string.h>

#include <util/threading.h> /* os_atomic_*: OBS's portable atomics, header only */

/* The slot of position n (positions grow forever; slots are used in turn). */
static size_t slot_at(long position)
{
	return (size_t)position % TSW_STAGE_SLOTS;
}

/* Frame intervals from one frame to the next, rounded to the nearest, at least 1. OBS draws at exact
 * multiples of the interval, so the rounding only guards against a clock that is not. */
static uint32_t intervals_between(const struct tsw_stage_ring *ring, uint64_t from, uint64_t to)
{
	if (to <= from)
		return 1;
	const uint64_t intervals = (to - from + ring->interval_ns / 2) / ring->interval_ns;
	if (intervals < 1)
		return 1;
	return intervals > UINT32_MAX ? UINT32_MAX : (uint32_t)intervals;
}

enum tsw_result tsw_stage_ring_init(struct tsw_stage_ring *ring, uint64_t interval_ns)
{
	if (!TSW_CHECK(ring != NULL && interval_ns >= 1))
		return TSW_E_INVALID;
	memset(ring, 0, sizeof(*ring));
	ring->interval_ns = interval_ns;
	return TSW_OK;
}

bool tsw_stage_ring_reclaim(struct tsw_stage_ring *ring, size_t *slot, bool *mapped)
{
	if (!TSW_CHECK(ring != NULL && slot != NULL && mapped != NULL))
		return false;
	struct tsw_stage_slot *back = &ring->slots[slot_at(ring->reclaimed)];
	if (os_atomic_load_long(&back->state) != TSW_STAGE_DONE)
		return false;
	*slot = slot_at(ring->reclaimed);
	*mapped = back->mapped;
	back->mapped = false;
	os_atomic_set_long(&back->state, TSW_STAGE_FREE);
	ring->reclaimed++;
	return true;
}

bool tsw_stage_ring_claim(struct tsw_stage_ring *ring, uint64_t timestamp, size_t *slot, size_t *previous)
{
	if (!TSW_CHECK(ring != NULL && slot != NULL && previous != NULL))
		return false;
	*previous = SIZE_MAX;
	struct tsw_stage_slot *next = &ring->slots[slot_at(ring->claimed)];
	if (os_atomic_load_long(&next->state) != TSW_STAGE_FREE) {
		os_atomic_set_long(&ring->unstaged, ring->unstaged + 1); /* the frame staged before covers it */
		return false;
	}

	/* 1. The frame staged before this one now knows how many intervals it covers. */
	if (ring->pending) {
		struct tsw_stage_slot *before = &ring->slots[slot_at(ring->claimed - 1)];
		before->count = intervals_between(ring, before->timestamp, timestamp);
		*previous = slot_at(ring->claimed - 1);
	}

	/* 2. This frame takes the next slot, and waits for the frame after it. */
	next->timestamp = timestamp;
	next->count = 0;
	next->mapped = false;
	os_atomic_set_long(&next->state, TSW_STAGE_STAGED);
	*slot = slot_at(ring->claimed);
	ring->claimed++;
	ring->pending = true;
	return true;
}

void tsw_stage_ring_publish(struct tsw_stage_ring *ring, size_t slot, bool mapped)
{
	if (!TSW_CHECK(ring != NULL && slot < TSW_STAGE_SLOTS))
		return;
	struct tsw_stage_slot *handed = &ring->slots[slot];
	/* Only a frame claim returned as previous: staged, with its count known. */
	if (!TSW_CHECK(os_atomic_load_long(&handed->state) == TSW_STAGE_STAGED && handed->count >= 1))
		return;
	handed->mapped = mapped;
	if (!mapped)
		os_atomic_set_long(&ring->unmapped, ring->unmapped + 1);
	os_atomic_set_long(&handed->state, TSW_STAGE_READY); /* last: the copier may take it from here */
}

bool tsw_stage_ring_next(struct tsw_stage_ring *ring, size_t *slot)
{
	if (!TSW_CHECK(ring != NULL && slot != NULL))
		return false;
	const size_t oldest = slot_at(ring->copied); /* only this thread changes copied */
	if (os_atomic_load_long(&ring->slots[oldest].state) != TSW_STAGE_READY)
		return false;
	*slot = oldest;
	return true;
}

void tsw_stage_ring_done(struct tsw_stage_ring *ring)
{
	if (!TSW_CHECK(ring != NULL))
		return;
	struct tsw_stage_slot *copied = &ring->slots[slot_at(ring->copied)];
	if (!TSW_CHECK(os_atomic_load_long(&copied->state) == TSW_STAGE_READY))
		return;
	os_atomic_set_long(&copied->state, TSW_STAGE_DONE);
	os_atomic_set_long(&ring->copied, ring->copied + 1);
}

bool tsw_stage_ring_still_mapped(const struct tsw_stage_ring *ring, size_t slot)
{
	if (!TSW_CHECK(ring != NULL && slot < TSW_STAGE_SLOTS))
		return false;
	const long state = ring->slots[slot].state;
	return (state == TSW_STAGE_READY || state == TSW_STAGE_DONE) && ring->slots[slot].mapped;
}
