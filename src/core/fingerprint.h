/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * fingerprint.h: a cheap fingerprint of a picture, to count pictures that OBS delivers twice in a row
 * (diagnostics: a repeated picture looks like a stutter). It reads 64 pixels on an 8 x 8 grid, so it is
 * safe on the real-time video thread (rule B1): no allocation, no locks, a few hundred instructions.
 * A camera's noise changes those pixels from one picture to the next; a repeated picture does not.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * tsw_fingerprint: fingerprint of the first plane of a picture (luma in NV12 and I420).
 *   linesize  bytes from one line to the next (may be more than width).
 *   width, height  the picture, in pixels of that plane; only those are read.
 * Returns the fingerprint, or 0 if plane is NULL or the size is empty. Thread: any.
 */
uint64_t tsw_fingerprint(const uint8_t *plane, size_t linesize, size_t width, size_t height);

#ifdef __cplusplus
}
#endif
