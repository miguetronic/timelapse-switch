/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * controller.c: see controller.h.
 */

#include "controller.h"

#include <util/bmem.h>
#include <util/platform.h>

#include "core/check.h"
#include "plugin-support.h"

static const uint64_t DEBOUNCE_NS = 400 * 1000000ull; /* UX-046 */
static const uint32_t DEFAULT_TIMELAPSE_SPEED = 4;

struct tsw_controller {
	struct tsw_recorder *recorder; /* borrowed from plugin-main.c */
	struct tsw_session session;
	struct tsw_dock_model model;
};

struct tsw_controller *tsw_controller_create(struct tsw_recorder *recorder, uint32_t timelapse_speed)
{
	if (!TSW_CHECK(recorder != NULL))
		return NULL;
	struct tsw_controller *controller = bzalloc(sizeof(*controller));
	controller->recorder = recorder;
	if (tsw_session_init(&controller->session, timelapse_speed, DEBOUNCE_NS) != TSW_OK &&
	    tsw_session_init(&controller->session, DEFAULT_TIMELAPSE_SPEED, DEBOUNCE_NS) != TSW_OK) {
		bfree(controller);
		return NULL;
	}
	tsw_dock_model_init(&controller->model);
	return controller;
}

void tsw_controller_destroy(struct tsw_controller *controller)
{
	bfree(controller);
}

void tsw_controller_input(struct tsw_controller *controller, enum tsw_input input, uint32_t speed)
{
	if (!TSW_CHECK(controller != NULL))
		return;
	struct tsw_status status;
	tsw_recorder_get_status(controller->recorder, &status);
	const struct tsw_command command =
		tsw_session_input(&controller->session, &status, input, speed, os_gettime_ns());

	/* Failures are logged by the recorder and shown by the dock through the status. */
	enum tsw_result result = TSW_OK;
	switch (command.kind) {
	case TSW_COMMAND_NONE:
		break;
	case TSW_COMMAND_START:
		result = tsw_recorder_start(controller->recorder, command.speed);
		break;
	case TSW_COMMAND_SET_SPEED:
		result = tsw_recorder_set_speed(controller->recorder, command.speed);
		break;
	case TSW_COMMAND_STOP:
		tsw_recorder_stop(controller->recorder);
		break;
	}
	if (result != TSW_OK)
		tsw_log(LOG_DEBUG, "input %d: command %d not applied (result %d)", (int)input, (int)command.kind,
			(int)result);
}

void tsw_controller_view(struct tsw_controller *controller, struct tsw_dock_view *view)
{
	if (!TSW_CHECK(controller != NULL && view != NULL))
		return;
	struct tsw_status status;
	tsw_recorder_get_status(controller->recorder, &status);
	tsw_dock_view_build(&controller->model, &controller->session, &status, view);
}

void tsw_controller_dismiss(struct tsw_controller *controller)
{
	if (!TSW_CHECK(controller != NULL))
		return;
	tsw_dock_model_dismiss(&controller->model);
}

uint32_t tsw_controller_timelapse_speed(const struct tsw_controller *controller)
{
	if (!TSW_CHECK(controller != NULL))
		return DEFAULT_TIMELAPSE_SPEED;
	return controller->session.timelapse_speed;
}
