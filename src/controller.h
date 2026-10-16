/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * controller.h: the one place where buttons and hotkeys act, so both give the same result (UX-053).
 *
 *   hotkeys (plugin-main.c) --+
 *                             +--> [ controller ] --> session: what the input means (core/session.h)
 *   dock buttons (ui/) -------+          |
 *                                        +--> recorder: start, switch, stop (recorder.h)
 *   dock timer --> tsw_controller_view: what the dock shows now (core/dock-view.h)
 *
 * Every function runs on the UI thread.
 */

#pragma once

#include <stdint.h>

#include "core/dock-view.h"
#include "core/session.h"
#include "recorder.h"

#ifdef __cplusplus
extern "C" {
#endif

struct tsw_controller;

/* tsw_controller_create: the next timelapse runs at timelapse_speed. Returns NULL if memory runs out. */
struct tsw_controller *tsw_controller_create(struct tsw_recorder *recorder, uint32_t timelapse_speed);

void tsw_controller_destroy(struct tsw_controller *controller);

/* tsw_controller_input: a button or hotkey was pressed; speed only for the speed chip and keys. */
void tsw_controller_input(struct tsw_controller *controller, enum tsw_input input, uint32_t speed);

/* tsw_controller_view: what the dock shows now. */
void tsw_controller_view(struct tsw_controller *controller, struct tsw_dock_view *view);

/* tsw_controller_dismiss: close the summary or error card. */
void tsw_controller_dismiss(struct tsw_controller *controller);

/* tsw_controller_timelapse_speed: the chosen speed of the next timelapse, to save in the settings. */
uint32_t tsw_controller_timelapse_speed(const struct tsw_controller *controller);

#ifdef __cplusplus
}
#endif
