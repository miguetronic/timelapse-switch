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
 *   theme, font size or density changed --> the dock reads the theme again (UX-016)
 *   the dock's About dialog gets the version and the symbol's pictures from here, so the dock itself
 *   never needs OBS (UX-138)
 */

#include "ui/dock-obs.h"

#include <QMainWindow>
#include <QMetaObject>
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

/* UI thread. OBS ends every theme, Font Size and Density change with this event; the dock reads
 * the theme once the change is complete, after OBS's own restyle (queued). */
void onFrontendEvent(enum obs_frontend_event event, void *data)
{
	if (event == OBS_FRONTEND_EVENT_THEME_CHANGED)
		QMetaObject::invokeMethod(static_cast<TswDock *>(data), "scheduleRestyle", Qt::QueuedConnection);
}

/* The dock, found in OBS's main window by its object name: OBS owns it, so no pointer is kept. */
TswDock *findDock()
{
	auto *window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	return window != nullptr ? window->findChild<TswDock *>("tswDock") : nullptr;
}

/* A file of the plugin's data folder (data/ in the sources), or an empty path if it is missing. */
QString dataFile(const char *name)
{
	char *found = obs_module_file(name);
	const QString path = found != nullptr ? QString::fromUtf8(found) : QString();
	bfree(found);
	return path;
}

} // namespace

void tsw_dock_create(struct tsw_controller *controller, bool details_open)
{
	auto *dock = new TswDock(moduleText);
	dock->setAboutInfo({QString::fromUtf8(PLUGIN_VERSION), dataFile("images/symbol-512.png"),
			    dataFile("images/symbol-dark-512.png")});
	dock->setDetailsOpen(details_open);
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
		return;
	}
	obs_frontend_add_event_callback(onFrontendEvent, dock);
}

bool tsw_dock_details_open(bool *open)
{
	const TswDock *dock = findDock();
	if (dock == nullptr || open == nullptr)
		return false;
	*open = dock->detailsOpen();
	return true;
}

void tsw_dock_destroy(void)
{
	/* The callback goes first: it must never reach a dock that OBS is about to delete. */
	TswDock *dock = findDock();
	if (dock != nullptr)
		obs_frontend_remove_event_callback(onFrontendEvent, dock);
	obs_frontend_remove_dock(DOCK_ID);
}
