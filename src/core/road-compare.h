/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * road-compare.h: road C's verification (ADR-012). While one recording runs, both roads hand over every
 * frame: road B (OBS's copy) and road C (ours). Each road writes down when each frame was rendered and a
 * hash of all its bytes; at the end the two lists are compared. Road C is right only if, over the time
 * both roads ran, they handed over the same frames, at the same times, with the same bytes.
 *
 *   road B:  t1 h1   t2 h2   t3 h3   t4 h4
 *   road C:          t2 h2   t3 h3   t4 h4   t5 h5     compared: t2 to t4 (both ran); all equal = right
 *
 * One writer per list, no locks, no memory allocated (the caller gives the storage).
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/result.h"
#include "tsw-compiler.h"

struct tsw_road_record {
	uint64_t timestamp; /* when OBS rendered the frame */
	uint64_t hash;      /* of all its bytes (tsw_plane_hash) */
};

/* What one road handed over, in order. */
struct tsw_road_list {
	struct tsw_road_record *records; /* capacity records, owned by the caller */
	size_t capacity;
	volatile long count; /* records written: changed only by the writer */
	volatile long full;  /* frames not written down because the list was full */
};

/* The comparison, over the time both roads ran. */
struct tsw_road_comparison {
	long compared;               /* frames both roads handed over */
	long different;              /* of those, frames whose bytes differ */
	long only_b;                 /* frames only road B handed over */
	long only_c;                 /* frames only road C handed over */
	long c_late;                 /* different frames where road C had road B's picture of the frame before */
	long b_late;                 /* different frames where road B had road C's picture of the frame before */
	long unmatched;              /* different frames that are neither: a picture the other road never had */
	uint64_t first_different_ns; /* when the first different frame was rendered; 0 if none */
};

/* tsw_road_list_init: an empty list over storage (capacity records). Returns TSW_OK or TSW_E_INVALID. */
enum tsw_result tsw_road_list_init(struct tsw_road_list *list, struct tsw_road_record *storage,
				   size_t capacity) TSW_NODISCARD;

/* tsw_road_list_add: write one frame down. Thread: the list's writer. Returns false when the list is full. */
bool tsw_road_list_add(struct tsw_road_list *list, uint64_t timestamp, uint64_t hash);

/*
 * tsw_road_compare: compare the two lists over the time both covered (from the later first frame to the
 * earlier last frame). Thread: any, once neither list is written any more.
 */
void tsw_road_compare(const struct tsw_road_list *b, const struct tsw_road_list *c,
		      struct tsw_road_comparison *comparison);
