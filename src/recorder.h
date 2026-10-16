/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * recorder.h: the plugin's own recorder (docs/architecture.md, ADR-002, ADR-003).
 *
 * It records the same picture and sound as OBS, with OBS's own encoders and file
 * writers, and lets each frame and audio block through according to the timeline:
 *
 *   OBS picture --> [ video gate ] --> our video feed --> OBS encoder --+
 *                                                                      +--> OBS file writer
 *   OBS sound ----> [ ring ] --> [ audio gate ] --> our audio feed ----+
 *
 * Settings (folder, format, encoders) come from the current OBS profile, unchanged.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "core/result.h"
#include "core/status.h"
#include "tsw-compiler.h"

struct tsw_recorder;

/* tsw_recorder_create: an idle recorder. Returns NULL if memory runs out. Thread: UI. */
struct tsw_recorder *tsw_recorder_create(void);

/* tsw_recorder_close: stop if needed and wait (up to 10 s) for the file to be saved. Thread: UI. */
void tsw_recorder_close(struct tsw_recorder *recorder);

/* tsw_recorder_destroy: tsw_recorder_close, then free the recorder. Thread: UI. */
void tsw_recorder_destroy(struct tsw_recorder *recorder);

/*
 * tsw_recorder_start: start a recording at the given speed (1 = real time).
 * Returns TSW_OK, or an error already written to the log. Thread: UI.
 */
enum tsw_result tsw_recorder_start(struct tsw_recorder *recorder, uint32_t speed) TSW_NODISCARD;

/* tsw_recorder_stop: stop the recording; the file is saved in the background. Thread: UI. */
void tsw_recorder_stop(struct tsw_recorder *recorder);

/*
 * tsw_recorder_set_speed: switch speed now, without pausing (1 = real time).
 * Returns TSW_OK, TSW_E_TIMELINE_FULL or TSW_E_INVALID. Thread: any except the real-time threads.
 */
enum tsw_result tsw_recorder_set_speed(struct tsw_recorder *recorder, uint32_t speed) TSW_NODISCARD;

/* tsw_recorder_is_active: true from start until the file is saved. Thread: any. */
bool tsw_recorder_is_active(struct tsw_recorder *recorder);

/* tsw_recorder_speed: speed of the current section (1 when idle). Thread: any. */
uint32_t tsw_recorder_speed(struct tsw_recorder *recorder);

/* tsw_recorder_add_marker: a numbered chapter marker here (Hybrid MP4; UX-054). Thread: UI. */
void tsw_recorder_add_marker(struct tsw_recorder *recorder);

/*
 * tsw_recorder_get_status: what the recorder is doing now, and how the last recording ended.
 * The path in it is borrowed: valid until the next start or stop. Thread: UI.
 */
void tsw_recorder_get_status(struct tsw_recorder *recorder, struct tsw_status *status);
