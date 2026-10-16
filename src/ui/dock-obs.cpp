/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * dock-obs.cpp: see dock-obs.h. OBS owns the dock (in its scroll area) once it is added; the
 * refresh timer is a child of the dock, so both go away together.
 *
 *   timer (100 ms) --> controller view --> dock draws it
 *   dock click -----> controller input --> recorder; then the dock draws the new view at once
 */

#include "ui/dock-obs.h"

#include <QScrollArea>
#include <QTimer>
#include <obs-frontend-api.h>
#include <obs-module.h>

#include "controller.h"
#include "plugin-support.h"
#include "ui/dock.hpp"

namespace {

const char DOCK_ID[] = "timelapse-switch";
const int REFRESH_MS = 100; /* UX-060: the banner follows the recorder within 100 ms */

const char *moduleText(const char *key)
{
	return obs_module_text(key);
}

} // namespace

void tsw_dock_create(struct tsw_controller *controller)
{
	auto *dock = new TswDock(moduleText);
	auto refresh = [dock, controller]() {
		tsw_dock_view view;
		tsw_controller_view(controller, &view);
		dock->showView(view);
	};
	QObject::connect(dock, &TswDock::inputRequested, dock, [controller, refresh](int input, unsigned speed) {
		tsw_controller_input(controller, static_cast<enum tsw_input>(input), speed);
		refresh();
	});
	QObject::connect(dock, &TswDock::dismissRequested, dock, [controller, refresh]() {
		tsw_controller_dismiss(controller);
		refresh();
	});
	auto *timer = new QTimer(dock);
	QObject::connect(timer, &QTimer::timeout, dock, refresh);
	timer->start(REFRESH_MS);
	refresh();
	QScrollArea *area = TswDock::inScrollArea(dock);
	if (!obs_frontend_add_dock_by_id(DOCK_ID, obs_module_text("Dock.Title"), area)) {
		tsw_log(LOG_WARNING, "TSW-E901 warning | the dock could not be added | id=%s", DOCK_ID);
		delete area;
	}
}

void tsw_dock_destroy(void)
{
	obs_frontend_remove_dock(DOCK_ID);
}
