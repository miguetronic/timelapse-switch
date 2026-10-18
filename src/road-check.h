/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * road-check.h: road C's verification mode (ADR-012), TIMELAPSE_SWITCH_ROAD=verify. The recording runs
 * on road C, and road B runs at the same time only to be compared: each road writes down every frame it
 * hands over, with the time OBS rendered it and a hash of all its bytes (core/road-compare.h). When the
 * recording ends, one log line says whether both roads handed over the same frames.
 *
 *   OBS's memory feed (road B)  --> tsw_road_check_receive_b: write down, nothing else
 *   our copier (road C)         --> tsw_road_check_receive_c: write down, then on to the recorder
 *
 * For tests: the hashes cost about 1 ms per 4K frame on each road.
 */

#pragma once

#include <stdbool.h>

#include <media-io/video-io.h>
#include <obs.h>

#include "gpu-road.h"
#include "tsw-compiler.h"

struct tsw_road_check;

/* tsw_road_check_create: room for both lists; frames road C hands over go on to forward(param). Thread: UI.
 * Returns NULL if the format is not one road C supports. */
struct tsw_road_check *tsw_road_check_create(const struct obs_video_info *ovi, tsw_gpu_road_receive forward,
					     void *param) TSW_NODISCARD;

/* Road B's frames: OBS's raw video callback. Thread: OBS video. */
void tsw_road_check_receive_b(void *param, struct video_data *frame);

/* Road C's frames: road C's receive callback, before the recorder. Thread: our copier. */
void tsw_road_check_receive_c(void *param, struct video_data *frame);

/* tsw_road_check_finish: compare, log the result if report, and free. Thread: UI, once neither road
 * hands over frames. Safe with NULL. */
void tsw_road_check_finish(struct tsw_road_check *check, bool report);
