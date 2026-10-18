/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * road-check-internal.h: what road-check.c and road-check-rt.c share. No other module includes it.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "core/road-compare.h"
#include "road-check.h"

enum {
	TSW_CHECK_PLANES = 3, /* at most: Y, U and V */
};

struct tsw_road_check {
	size_t planes;
	size_t row_bytes[TSW_CHECK_PLANES]; /* bytes of picture in each row of each plane */
	size_t rows[TSW_CHECK_PLANES];
	struct tsw_road_list b;          /* written by OBS's video thread */
	struct tsw_road_list c;          /* written by our copier thread */
	struct tsw_road_record *storage; /* both lists, owned */
	tsw_gpu_road_receive forward;
	void *param;
};

/* How many planes to walk: the format's, never more than the arrays hold. */
static inline size_t tsw_road_check_planes(const struct tsw_road_check *check)
{
	return check->planes < TSW_CHECK_PLANES ? check->planes : TSW_CHECK_PLANES;
}
