/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * test-stage-ring.c: unit tests of src/core/stage-ring.c. Expected values are worked out by hand,
 * except in the long run, which checks rules that must hold for every frame.
 *
 * The two threads are played one after the other, in the order the test chooses: draw() does what
 * the graphics thread does on every frame (recorder-gpu-rt.c), copy() what the copier does.
 */

#include "test.h"

#include <core/check.h>
#include <core/stage-ring.h>

static const uint64_t FRAME = 33333333; /* one frame at 30 fps, in ns */
static const size_t NONE = SIZE_MAX;

static struct tsw_stage_ring ring;

/* What the graphics thread saw on one frame. */
struct drawn {
	bool staged;     /* the frame got a slot */
	size_t slot;     /* that slot */
	size_t previous; /* the slot handed over to the copier, or NONE */
	size_t freed;    /* slots that came back */
	size_t unmap;    /* of those, slots the caller had to unmap */
};

/* The graphics thread on one frame drawn at timestamp: reclaim, claim, then map and publish the
 * frame before it (the map succeeds if map_works). */
static struct drawn draw(uint64_t timestamp, bool map_works)
{
	struct drawn drawn = {false, NONE, NONE, 0, 0};
	size_t slot;
	bool mapped;
	while (tsw_stage_ring_reclaim(&ring, &slot, &mapped)) {
		drawn.freed++;
		drawn.unmap += mapped;
	}
	drawn.staged = tsw_stage_ring_claim(&ring, timestamp, &drawn.slot, &drawn.previous);
	if (drawn.previous != NONE)
		tsw_stage_ring_publish(&ring, drawn.previous, map_works);
	return drawn;
}

/* The copier on its oldest slot, if there is one: what it holds, then give it back. */
static bool copy(struct tsw_stage_slot *copied)
{
	size_t slot;
	if (!tsw_stage_ring_next(&ring, &slot))
		return false;
	*copied = ring.slots[slot];
	tsw_stage_ring_done(&ring);
	return true;
}

/* Break caught: a frame handed over before the next one is drawn (its count unknown), or the copier
 * reading a frame that was never handed over. */
static void a_frame_is_handed_over_when_the_next_is_drawn(void)
{
	struct tsw_stage_slot copied;
	TSW_EXPECT_EQ(tsw_stage_ring_init(&ring, FRAME), TSW_OK);
	struct drawn first = draw(10 * FRAME, true);
	TSW_EXPECT_TRUE(first.staged);
	TSW_EXPECT_EQ(first.slot, 0);
	TSW_EXPECT_EQ(first.previous, NONE);
	TSW_EXPECT_TRUE(!copy(&copied)); /* nothing handed over yet */

	struct drawn second = draw(11 * FRAME, true);
	TSW_EXPECT_EQ(second.slot, 1);
	TSW_EXPECT_EQ(second.previous, 0);
	TSW_EXPECT_TRUE(copy(&copied));
	TSW_EXPECT_EQ(copied.timestamp, 10 * FRAME);
	TSW_EXPECT_EQ(copied.count, 1);
	TSW_EXPECT_TRUE(copied.mapped);
	TSW_EXPECT_TRUE(!copy(&copied)); /* the second frame waits for the third */

	struct drawn third = draw(12 * FRAME, true);
	TSW_EXPECT_EQ(third.freed, 1); /* the first slot came back, to be unmapped */
	TSW_EXPECT_EQ(third.unmap, 1);
}

/* Break caught: OBS rendering late (a lagged frame) leaves intervals that no frame covers, and the
 * video falls behind the audio. */
static void a_late_frame_covers_the_intervals_until_the_next(void)
{
	struct tsw_stage_slot copied;
	TSW_EXPECT_EQ(tsw_stage_ring_init(&ring, FRAME), TSW_OK);
	(void)draw(0, true);
	(void)draw(3 * FRAME, true); /* OBS skipped two draws */
	TSW_EXPECT_TRUE(copy(&copied));
	TSW_EXPECT_EQ(copied.timestamp, 0);
	TSW_EXPECT_EQ(copied.count, 3);
}

/* Break caught: with the copier behind, a busy slot overwritten (a frame copied while the card writes
 * it), or the frames that could not be staged leaving a hole in the video. */
static void busy_slots_stage_nothing_and_the_frame_before_covers_it(void)
{
	struct tsw_stage_slot copied;
	TSW_EXPECT_EQ(tsw_stage_ring_init(&ring, FRAME), TSW_OK);
	for (uint64_t frame = 0; frame < 4; frame++)
		TSW_EXPECT_TRUE(draw(frame * FRAME, true).staged); /* slots 0 to 3; 0, 1 and 2 handed over */
	struct drawn busy = draw(4 * FRAME, true);
	TSW_EXPECT_TRUE(!busy.staged);
	TSW_EXPECT_EQ(busy.previous, NONE);
	TSW_EXPECT_TRUE(!draw(5 * FRAME, true).staged);
	TSW_EXPECT_EQ(ring.unstaged, 2);

	for (uint64_t frame = 0; frame < 3; frame++) {
		TSW_EXPECT_TRUE(copy(&copied));
		TSW_EXPECT_EQ(copied.timestamp, frame * FRAME);
		TSW_EXPECT_EQ(copied.count, 1);
	}
	TSW_EXPECT_TRUE(!copy(&copied)); /* slot 3 still waits for the next frame */

	struct drawn back = draw(6 * FRAME, true);
	TSW_EXPECT_EQ(back.freed, 3);
	TSW_EXPECT_TRUE(back.staged);
	TSW_EXPECT_EQ(back.slot, 0);
	TSW_EXPECT_EQ(back.previous, 3);
	TSW_EXPECT_TRUE(copy(&copied));
	TSW_EXPECT_EQ(copied.timestamp, 3 * FRAME);
	TSW_EXPECT_EQ(copied.count, 3); /* its own interval and the two frames that found no slot */
}

/* Break caught: a slot whose map failed read by the copier, or unmapped although it was never mapped. */
static void a_failed_map_hands_over_nothing_to_copy(void)
{
	struct tsw_stage_slot copied;
	TSW_EXPECT_EQ(tsw_stage_ring_init(&ring, FRAME), TSW_OK);
	(void)draw(0, true);
	(void)draw(FRAME, false);
	TSW_EXPECT_EQ(ring.unmapped, 1);
	TSW_EXPECT_TRUE(copy(&copied));
	TSW_EXPECT_TRUE(!copied.mapped);
	TSW_EXPECT_EQ(copied.count, 1);
	struct drawn next = draw(2 * FRAME, true);
	TSW_EXPECT_EQ(next.freed, 1);
	TSW_EXPECT_EQ(next.unmap, 0);
}

/* Break caught: a slot left mapped when the recording closes (a leak, or a crash on the next map), or
 * one unmapped twice. */
static void closing_unmaps_exactly_the_slots_still_mapped(void)
{
	struct tsw_stage_slot copied;
	TSW_EXPECT_EQ(tsw_stage_ring_init(&ring, FRAME), TSW_OK);
	(void)draw(0, true);
	(void)draw(FRAME, true);
	(void)draw(2 * FRAME, true);    /* slots 0 and 1 mapped and handed over, slot 2 staged */
	TSW_EXPECT_TRUE(copy(&copied)); /* slot 0 copied, not come back yet */
	TSW_EXPECT_TRUE(tsw_stage_ring_still_mapped(&ring, 0));
	TSW_EXPECT_TRUE(tsw_stage_ring_still_mapped(&ring, 1));
	TSW_EXPECT_TRUE(!tsw_stage_ring_still_mapped(&ring, 2));
	TSW_EXPECT_TRUE(!tsw_stage_ring_still_mapped(&ring, 3));
}

/* Break caught: misuse that would hand the copier a frame twice or never, accepted silently. */
static void misuse_is_refused(void)
{
	const int checks_before = tsw_check_failures;
	TSW_EXPECT_EQ(tsw_stage_ring_init(&ring, 0), TSW_E_INVALID);
	TSW_EXPECT_EQ(tsw_stage_ring_init(&ring, FRAME), TSW_OK);
	struct drawn first = draw(0, true);
	tsw_stage_ring_publish(&ring, first.slot, true); /* its count is not known yet */
	tsw_stage_ring_publish(&ring, TSW_STAGE_SLOTS, true);
	tsw_stage_ring_done(&ring); /* nothing was taken */
	TSW_EXPECT_EQ(tsw_check_failures - checks_before, 4);
	TSW_EXPECT_EQ(ring.slots[first.slot].state, TSW_STAGE_STAGED);
	TSW_EXPECT_EQ(ring.copied, 0);
}

/* A small, fixed pseudo-random sequence (a linear congruential generator), so every run is the same. */
static uint32_t lcg_state = 12345;
static uint32_t lcg(uint32_t below)
{
	lcg_state = lcg_state * 1103515245u + 12345u;
	return (lcg_state >> 16) % below;
}

struct run_tally {
	uint64_t first_ns;      /* when the first frame was drawn */
	uint64_t expected_ns;   /* where the next frame handed over must start */
	uint64_t intervals;     /* intervals the frames handed over cover */
	uint64_t out_of_order;  /* frames handed over that do not start where the previous one ended */
	uint64_t failed_maps;   /* maps made to fail */
	uint64_t not_staged;    /* frames that found every slot busy */
	uint64_t wrong_unmaps;  /* slots come back marked mapped although their map failed, or the reverse */
	uint64_t copied_mapped; /* frames handed over with their map working */
};

/* The copier, a random number of times: checks each frame it takes starts where the last one ended. */
static void copier_turn(struct run_tally *tally)
{
	struct tsw_stage_slot copied;
	for (uint32_t turns = lcg(3); turns > 0 && copy(&copied); turns--) {
		tally->out_of_order += (copied.timestamp != tally->expected_ns);
		tally->expected_ns = copied.timestamp + copied.count * FRAME;
		tally->intervals += copied.count;
		tally->copied_mapped += copied.mapped;
	}
}

/* Break caught: over a long recording with OBS lagging, the copier stalling and maps failing, any frame
 * handed over out of order, an interval covered twice or never, or a busy slot reused. */
static void a_long_run_covers_every_interval_once_and_in_order(void)
{
	TSW_EXPECT_EQ(tsw_stage_ring_init(&ring, FRAME), TSW_OK);
	struct run_tally tally = {0};
	uint64_t now = 0, mapped_handed = 0, unmapped_back = 0, mapped_back = 0;
	for (uint32_t frame = 0; frame < 18000; frame++) {            /* 10 minutes at 30 fps */
		now += (lcg(50) == 0) ? (1 + lcg(3)) * FRAME : FRAME; /* OBS lags 1 frame in 50 */
		if (frame == 0)
			tally.first_ns = tally.expected_ns = now; /* the first frame handed over starts here */
		const bool map_works = lcg(500) != 0;
		struct drawn drawn = draw(now, map_works);
		tally.not_staged += !drawn.staged;
		tally.failed_maps += (drawn.previous != NONE && !map_works);
		mapped_handed += (drawn.previous != NONE && map_works);
		mapped_back += drawn.unmap;
		unmapped_back += drawn.freed - drawn.unmap;
		if (lcg(200) != 0) /* now and then the copier stalls for a frame */
			copier_turn(&tally);
	}
	for (uint32_t turns = 0; turns < 16; turns++)
		copier_turn(&tally);

	/* Every interval from the first frame up to the one still pending is covered exactly once. */
	const struct tsw_stage_slot *pending = &ring.slots[(size_t)(ring.claimed - 1) % TSW_STAGE_SLOTS];
	TSW_EXPECT_EQ(tally.out_of_order, 0);
	TSW_EXPECT_EQ(tally.expected_ns, pending->timestamp);
	TSW_EXPECT_EQ(tally.intervals * FRAME, pending->timestamp - tally.first_ns);
	TSW_EXPECT_EQ(ring.unmapped, tally.failed_maps);
	TSW_EXPECT_EQ(ring.unstaged, tally.not_staged);
	TSW_EXPECT_TRUE(tally.not_staged > 0); /* the run did exercise a full ring */
	TSW_EXPECT_TRUE(tally.failed_maps > 0);
	TSW_EXPECT_EQ(tally.copied_mapped, mapped_handed);
	TSW_EXPECT_TRUE(mapped_back <= mapped_handed);
	TSW_EXPECT_TRUE(unmapped_back <= tally.failed_maps);
	printf("    long run: %" PRIu64 " intervals, %" PRIu64 " frames found no slot, %" PRIu64 " maps failed\n",
	       tally.intervals, tally.not_staged, tally.failed_maps);
}

int main(void)
{
	a_frame_is_handed_over_when_the_next_is_drawn();
	a_late_frame_covers_the_intervals_until_the_next();
	busy_slots_stage_nothing_and_the_frame_before_covers_it();
	a_failed_map_hands_over_nothing_to_copy();
	closing_unmaps_exactly_the_slots_still_mapped();
	misuse_is_refused();
	a_long_run_covers_every_interval_once_and_in_order();
	return tsw_test_failures;
}
