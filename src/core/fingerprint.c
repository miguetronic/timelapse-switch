/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* fingerprint.c: see fingerprint.h. */

#include "core/fingerprint.h"

enum { GRID = 8 }; /* 8 x 8 = 64 pixels read */

static const uint64_t FNV_OFFSET = 14695981039346656037ULL;
static const uint64_t FNV_PRIME = 1099511628211ULL;

uint64_t tsw_fingerprint(const uint8_t *plane, size_t linesize, size_t width, size_t height)
{
	if (plane == NULL || width == 0 || height == 0 || linesize < width)
		return 0;
	/* FNV-1a over the pixels at the centres of an 8 x 8 grid of cells. */
	uint64_t hash = FNV_OFFSET;
	for (size_t row = 0; row < GRID; row++) {
		const size_t y = (2 * row + 1) * height / (2 * GRID);
		for (size_t column = 0; column < GRID; column++) {
			const size_t x = (2 * column + 1) * width / (2 * GRID);
			hash = (hash ^ plane[y * linesize + x]) * FNV_PRIME;
		}
	}
	return hash;
}
