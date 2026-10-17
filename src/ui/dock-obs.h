/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * dock-obs.h: puts the dock into OBS (Docks menu) and keeps it up to date (UX-001).
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct tsw_controller;

/*
 * tsw_dock_create: add the dock; it shows the controller's view and sends it the clicks.
 *   details_open  whether its Details section starts open: the user's last choice, kept in the
 *                 plugin's settings (UX-135).
 * Thread: UI.
 */
void tsw_dock_create(struct tsw_controller *controller, bool details_open);

/* tsw_dock_details_open: whether the Details section is open now, to keep in the settings; false (and
 * *open untouched) when there is no dock. Thread: UI. */
bool tsw_dock_details_open(bool *open);

/* tsw_dock_destroy: remove the dock, if it is there. Thread: UI. */
void tsw_dock_destroy(void);

#ifdef __cplusplus
}
#endif
