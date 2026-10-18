/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * road-check-rt.c: road C's verification on the threads that hand over frames (road-check.h). It follows
 * docs/coding-rules.md, B1, checked by tools/check.sh: no allocation, no logging, no locks, no waits.
 */

#include "core/fingerprint.h"
#include "road-check-internal.h"

/* A hash of every byte of the frame's picture, plane after plane (padding left out). */
static uint64_t frame_hash(const struct tsw_road_check *check, const struct video_data *frame)
{
	uint64_t hash = TSW_HASH_START;
	for (size_t plane = 0; plane < tsw_road_check_planes(check); plane++)
		hash = tsw_plane_hash(hash, frame->data[plane], frame->linesize[plane], check->row_bytes[plane],
				      check->rows[plane]);
	return hash;
}

void tsw_road_check_receive_b(void *param, struct video_data *frame)
{
	struct tsw_road_check *check = param;
	(void)tsw_road_list_add(&check->b, frame->timestamp, frame_hash(check, frame));
}

void tsw_road_check_receive_c(void *param, struct video_data *frame)
{
	struct tsw_road_check *check = param;
	(void)tsw_road_list_add(&check->c, frame->timestamp, frame_hash(check, frame));
	check->forward(check->param, frame);
}
