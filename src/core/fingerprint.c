/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* fingerprint.c: see fingerprint.h. */

#include "core/fingerprint.h"

#include <string.h>

enum {
	GRID = 8,          /* 8 x 8 = 64 pixels read */
	HALVES = 2 * GRID, /* half cells across the picture: a cell's centre falls on an odd one */
};

static const uint64_t FNV_PRIME = 1099511628211ULL;

uint64_t tsw_fingerprint(const uint8_t *plane, size_t linesize, size_t width, size_t height)
{
	if (plane == NULL || width == 0 || height == 0 || linesize < width)
		return 0;
	/* FNV-1a over the pixels at the centres of an 8 x 8 grid of cells. */
	uint64_t hash = TSW_HASH_START;
	for (size_t row = 0; row < GRID; row++) {
		const size_t y = (2 * row + 1) * height / HALVES;
		for (size_t column = 0; column < GRID; column++) {
			const size_t x = (2 * column + 1) * width / HALVES;
			hash = (hash ^ plane[y * linesize + x]) * FNV_PRIME;
		}
	}
	return hash;
}

uint64_t tsw_plane_hash(uint64_t hash, const uint8_t *plane, size_t linesize, size_t row_bytes, size_t rows)
{
	if (plane == NULL)
		return hash;
	/* FNV-1a over 8-byte words, then over the bytes left at the end of each row. Every step is a one-to-one
	 * mapping of the hash for a given input, so a single different byte always gives a different hash. */
	for (size_t row = 0; row < rows; row++) {
		const uint8_t *line = plane + row * linesize;
		size_t x = 0;
		for (; x + sizeof(uint64_t) <= row_bytes; x += sizeof(uint64_t)) {
			uint64_t word;
			memcpy(&word, line + x, sizeof(word));
			hash = (hash ^ word) * FNV_PRIME;
		}
		for (; x < row_bytes; x++)
			hash = (hash ^ line[x]) * FNV_PRIME;
	}
	return hash;
}
