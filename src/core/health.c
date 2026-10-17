/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* health.c: see health.h. */

#include "core/health.h"

#include <stddef.h>

#include "core/check.h"

static const uint64_t MS_NS = 1000000;

void tsw_disk_meter_reset(struct tsw_disk_meter *meter)
{
	if (!TSW_CHECK(meter != NULL))
		return;
	const struct tsw_disk_meter empty = {0};
	*meter = empty;
}

void tsw_disk_meter_add(struct tsw_disk_meter *meter, uint64_t now_ns, uint64_t file_bytes)
{
	if (!TSW_CHECK(meter != NULL))
		return;
	meter->at_ns[meter->next] = now_ns;
	meter->bytes[meter->next] = file_bytes;
	meter->next = (meter->next + 1) % TSW_DISK_SAMPLES;
	if (meter->count < TSW_DISK_SAMPLES)
		meter->count++;
}

uint64_t tsw_disk_meter_rate(const struct tsw_disk_meter *meter)
{
	if (!TSW_CHECK(meter != NULL) || meter->count < 2)
		return 0;
	const uint32_t newest = (meter->next + TSW_DISK_SAMPLES - 1) % TSW_DISK_SAMPLES;
	const uint32_t oldest = meter->count < TSW_DISK_SAMPLES ? 0 : meter->next;
	const uint64_t elapsed_ms = (meter->at_ns[newest] - meter->at_ns[oldest]) / MS_NS;
	if (elapsed_ms == 0 || meter->bytes[newest] <= meter->bytes[oldest])
		return 0; /* not growing: paused, or nothing written yet */
	/* Per millisecond first: bytes times 1000 stays far below 2^64 for any real file. */
	return (meter->bytes[newest] - meter->bytes[oldest]) * 1000 / elapsed_ms;
}

void tsw_health_evaluate(const struct tsw_health_input *input, struct tsw_health *health)
{
	if (!TSW_CHECK(input != NULL && health != NULL))
		return;
	health->issues = 0;
	health->disk_seconds_left = UINT64_MAX;
	if (input->disk_free_bytes > 0 && input->bytes_per_second > 0) {
		health->disk_seconds_left = input->disk_free_bytes / input->bytes_per_second;
		if (health->disk_seconds_left < TSW_DISK_WARN_SECONDS)
			health->issues |= (uint32_t)TSW_HEALTH_DISK_LOW;
	}
	if (input->frames_lost > 0)
		health->issues |= (uint32_t)TSW_HEALTH_LOST_FRAMES;
	if (input->feed_size > 0 && input->feed_waiting * 2 >= input->feed_size)
		health->issues |= (uint32_t)TSW_HEALTH_ENCODER_BEHIND;
}
