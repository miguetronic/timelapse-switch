/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * dock-obs.h: puts the dock into OBS (Docks menu) and keeps it up to date (UX-001).
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct tsw_controller;

/* tsw_dock_create: add the dock; it shows the controller's view and sends it the clicks. Thread: UI. */
void tsw_dock_create(struct tsw_controller *controller);

/* tsw_dock_destroy: remove the dock, if it is there. Thread: UI. */
void tsw_dock_destroy(void);

#ifdef __cplusplus
}
#endif
