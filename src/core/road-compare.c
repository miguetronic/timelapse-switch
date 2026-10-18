/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* road-compare.c: see road-compare.h. */

#include "core/road-compare.h"

#include "core/check.h"

#include <string.h>

#include <util/threading.h> /* os_atomic_*: OBS's portable atomics, header only */

enum tsw_result tsw_road_list_init(struct tsw_road_list *list, struct tsw_road_record *storage, size_t capacity)
{
	if (!TSW_CHECK(list != NULL && storage != NULL && capacity > 0))
		return TSW_E_INVALID;
	list->records = storage;
	list->capacity = capacity;
	list->count = 0;
	list->full = 0;
	return TSW_OK;
}

bool tsw_road_list_add(struct tsw_road_list *list, uint64_t timestamp, uint64_t hash)
{
	if (!TSW_CHECK(list != NULL))
		return false;
	const long count = list->count; /* only this thread changes it */
	if ((size_t)count >= list->capacity) {
		os_atomic_set_long(&list->full, list->full + 1);
		return false;
	}
	list->records[count].timestamp = timestamp;
	list->records[count].hash = hash;
	os_atomic_set_long(&list->count, count + 1);
	return true;
}

/* Which road a different frame shows is late: the one holding the other road's picture of the frame before. */
static void classify_difference(const struct tsw_road_list *b, const struct tsw_road_list *c, size_t i, size_t j,
				struct tsw_road_comparison *comparison)
{
	if (i > 0 && b->records[i - 1].hash == c->records[j].hash)
		comparison->c_late++;
	else if (j > 0 && c->records[j - 1].hash == b->records[i].hash)
		comparison->b_late++;
	else
		comparison->unmatched++;
}

void tsw_road_compare(const struct tsw_road_list *b, const struct tsw_road_list *c,
		      struct tsw_road_comparison *comparison)
{
	if (!TSW_CHECK(b != NULL && c != NULL && comparison != NULL))
		return;
	memset(comparison, 0, sizeof(*comparison));
	const size_t count_b = (size_t)os_atomic_load_long(&b->count);
	const size_t count_c = (size_t)os_atomic_load_long(&c->count);
	if (count_b == 0 || count_c == 0)
		return;

	/* 1. The time both roads ran: from the later first frame to the earlier last frame. */
	const uint64_t first_b = b->records[0].timestamp, first_c = c->records[0].timestamp;
	const uint64_t last_b = b->records[count_b - 1].timestamp, last_c = c->records[count_c - 1].timestamp;
	const uint64_t from = first_b > first_c ? first_b : first_c;
	const uint64_t to = last_b < last_c ? last_b : last_c;

	/* 2. Walk both lists in step (each is in time order), inside that time only. */
	size_t i = 0, j = 0;
	while (i < count_b && j < count_c) {
		const struct tsw_road_record *rb = &b->records[i], *rc = &c->records[j];
		const bool inside_b = rb->timestamp >= from && rb->timestamp <= to;
		const bool inside_c = rc->timestamp >= from && rc->timestamp <= to;
		if (rb->timestamp == rc->timestamp) {
			comparison->compared += inside_b;
			if (inside_b && rb->hash != rc->hash) {
				comparison->different++;
				classify_difference(b, c, i, j, comparison);
				if (comparison->first_different_ns == 0)
					comparison->first_different_ns = rb->timestamp;
			}
			i++;
			j++;
		} else if (rb->timestamp < rc->timestamp) {
			comparison->only_b += inside_b;
			i++;
		} else {
			comparison->only_c += inside_c;
			j++;
		}
	}
}
