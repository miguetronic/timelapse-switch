/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * stage-ring.h: hand frames from OBS's graphics thread to our copier thread, without locks or waits
 * (road C, ADR-012).
 *
 * On road C the graphics thread only asks the card to copy each frame into a staging slot, and goes
 * on rendering the preview. A frame later, when the card is done, it maps the slot and hands it to
 * our copier thread, which copies the frame into our video feed. Then the slot comes back.
 *
 *   graphics thread:  claim -> (card copies) -> publish ............................ reclaim
 *   copier thread:                              next -> copy into our feed -> done
 *
 * A frame is handed over when the next one is drawn, because only then is it known how many frame
 * intervals it covers: usually 1, more when OBS rendered late (as OBS's own feed repeats it), or
 * when every slot was busy and the frames drawn meanwhile could not be staged.
 *
 * Slots are used in turn, so they are handed over and come back in the order the frames were drawn.
 * This file only keeps the books; the caller does the graphics work it says. One graphics thread and
 * one copier thread; no memory allocated.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/result.h"
#include "tsw-compiler.h"

enum {
	TSW_STAGE_SLOTS = 4, /* one being copied by the card, one by the copier, one coming back, one spare */
};

/* Where a slot is: who owns it, and what it holds. */
enum tsw_stage_state {
	TSW_STAGE_FREE = 0, /* graphics thread: can take the next frame */
	TSW_STAGE_STAGED,   /* graphics thread: the card is copying the frame into it */
	TSW_STAGE_READY,    /* copier thread: mapped (or failed to map), waiting to be copied */
	TSW_STAGE_DONE,     /* graphics thread: copied; unmap it, then it is free */
};

struct tsw_stage_slot {
	volatile long state; /* enum tsw_stage_state; the hand-over between threads */
	uint64_t timestamp;  /* when OBS drew the frame */
	uint32_t count;      /* frame intervals it covers, known once the next frame is drawn */
	bool mapped;         /* the caller mapped it, and must unmap it when it comes back */
};

struct tsw_stage_ring {
	struct tsw_stage_slot slots[TSW_STAGE_SLOTS];
	uint64_t interval_ns;   /* one frame at OBS's frame rate */
	long claimed;           /* frames staged so far: graphics thread only */
	long reclaimed;         /* slots come back so far: graphics thread only */
	volatile long copied;   /* slots the copier finished so far: copier thread only */
	volatile long unstaged; /* frames drawn while every slot was busy: written by the graphics thread */
	volatile long unmapped; /* frames whose slot could not be mapped: written by the graphics thread */
	bool pending;           /* the last staged frame waits for the next one: graphics thread only */
};

/*
 * tsw_stage_ring_init: prepare an empty ring.
 *   interval_ns  one frame at OBS's frame rate, at least 1.
 * Returns TSW_OK or TSW_E_INVALID.
 */
enum tsw_result tsw_stage_ring_init(struct tsw_stage_ring *ring, uint64_t interval_ns) TSW_NODISCARD;

/*
 * tsw_stage_ring_reclaim: the next slot the copier finished, now free again. Thread: graphics, before
 * claim on every frame, until it returns false.
 *   *slot    the slot that came back.
 *   *mapped  true if the caller mapped it and must unmap it now.
 */
bool tsw_stage_ring_reclaim(struct tsw_stage_ring *ring, size_t *slot, bool *mapped);

/*
 * tsw_stage_ring_claim: a slot for the frame OBS drew at timestamp, if one is free. Thread: graphics.
 *   *slot      where the caller stages this frame (only when it returns true).
 *   *previous  the frame staged before it, now known to cover its intervals: the caller maps it, then
 *              calls tsw_stage_ring_publish; SIZE_MAX when there is none.
 * Returns false when every slot is busy: this frame is not staged (it counts in unstaged), and the
 * frame staged before it will cover its interval too.
 */
bool tsw_stage_ring_claim(struct tsw_stage_ring *ring, uint64_t timestamp, size_t *slot, size_t *previous);

/*
 * tsw_stage_ring_publish: hand the slot claim returned as previous to the copier. Thread: graphics.
 *   mapped  true if the map succeeded; false hands it over with nothing to copy (it counts in unmapped).
 */
void tsw_stage_ring_publish(struct tsw_stage_ring *ring, size_t slot, bool mapped);

/*
 * tsw_stage_ring_next: the oldest slot handed over and not copied yet. Thread: copier.
 *   *slot  the slot; its timestamp, count and mapped fields say what to copy (nothing if not mapped).
 * Returns false when there is none. Call tsw_stage_ring_done when the copy is finished.
 */
bool tsw_stage_ring_next(struct tsw_stage_ring *ring, size_t *slot);

/* tsw_stage_ring_done: the slot tsw_stage_ring_next returned is copied; give it back. Thread: copier. */
void tsw_stage_ring_done(struct tsw_stage_ring *ring);

/*
 * tsw_stage_ring_still_mapped: whether a slot is mapped and has not come back yet. Thread: whoever
 * closes the ring, once neither thread uses it: each such slot must be unmapped.
 */
bool tsw_stage_ring_still_mapped(const struct tsw_stage_ring *ring, size_t slot);
