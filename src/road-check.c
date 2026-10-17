/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* road-check.c: see road-check.h. What runs for every frame is in road-check-rt.c. */

#include "road-check.h"

#include <util/bmem.h>
#include <util/threading.h>

#include "plugin-support.h"
#include "road-check-internal.h"

enum {
	LIST_CAPACITY = 131072, /* frames per road: 72 minutes at 30 fps; 2 MB per road */
};

/* The bytes and rows of picture in each plane of the formats road C supports. */
static bool describe_planes(struct tsw_road_check *check, enum video_format format, size_t width, size_t height)
{
	switch (format) {
	case VIDEO_FORMAT_NV12:
		check->planes = 2;
		check->row_bytes[0] = check->row_bytes[1] = width; /* the second plane: U and V side by side */
		check->rows[0] = height;
		check->rows[1] = height / 2;
		return true;
	case VIDEO_FORMAT_I420:
		check->planes = 3;
		check->row_bytes[0] = width;
		check->row_bytes[1] = check->row_bytes[2] = width / 2;
		check->rows[0] = height;
		check->rows[1] = check->rows[2] = height / 2;
		return true;
	case VIDEO_FORMAT_I444:
		check->planes = 3;
		check->row_bytes[0] = check->row_bytes[1] = check->row_bytes[2] = width;
		check->rows[0] = check->rows[1] = check->rows[2] = height;
		return true;
	default:
		return false;
	}
}

struct tsw_road_check *tsw_road_check_create(const struct obs_video_info *ovi, tsw_gpu_road_receive forward,
					     void *param)
{
	if (ovi == NULL || forward == NULL)
		return NULL;
	struct tsw_road_check *check = bzalloc(sizeof(*check));
	check->forward = forward;
	check->param = param;
	check->storage = bmalloc((size_t)2 * LIST_CAPACITY * sizeof(*check->storage));
	if (!describe_planes(check, ovi->output_format, ovi->output_width, ovi->output_height) ||
	    tsw_road_list_init(&check->b, check->storage, LIST_CAPACITY) != TSW_OK ||
	    tsw_road_list_init(&check->c, check->storage + LIST_CAPACITY, LIST_CAPACITY) != TSW_OK) {
		tsw_road_check_finish(check, false);
		return NULL;
	}
	return check;
}

void tsw_road_check_finish(struct tsw_road_check *check, bool report)
{
	if (check == NULL)
		return;
	if (report) {
		struct tsw_road_comparison comparison;
		tsw_road_compare(&check->b, &check->c, &comparison);
		const uint64_t first_ns = check->b.count > 0 ? check->storage[0].timestamp : 0;
		const double first_different_s = comparison.first_different_ns != 0
							 ? (double)(comparison.first_different_ns - first_ns) / 1e9
							 : 0.0;
		tsw_log(LOG_INFO,
			"road check: frames compared %ld, different %ld (first %.3f s after road B's first), only on "
			"road B %ld, only on road C %ld, not written down %ld",
			comparison.compared, comparison.different, first_different_s, comparison.only_b,
			comparison.only_c, os_atomic_load_long(&check->b.full) + os_atomic_load_long(&check->c.full));
	}
	bfree(check->storage);
	bfree(check);
}
