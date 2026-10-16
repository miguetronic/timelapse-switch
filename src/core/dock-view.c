/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * dock-view.c: see dock-view.h.
 */

#include "core/dock-view.h"

#include <stddef.h>
#include <string.h>

#include "core/check.h"

static const uint64_t SECOND_NS = 1000000000;
static const uint64_t MAX_SECONDS = 9999ull * 3600 + 59ull * 60 + 59;
static const char NO_TIME[] = "-:--:--";

void tsw_format_duration(uint64_t ns, char text[TSW_TIME_TEXT])
{
	if (!TSW_CHECK(text != NULL))
		return;
	uint64_t seconds = ns / SECOND_NS;
	if (seconds > MAX_SECONDS)
		seconds = MAX_SECONDS;
	const uint64_t hours = seconds / 3600;
	const uint64_t minutes = seconds / 60 % 60;

	/* Hours without leading zeros, then ":mm:ss". Written by hand: no printf in the core. */
	char digits[4];
	size_t count = 0;
	uint64_t rest = hours;
	do {
		digits[count++] = (char)('0' + rest % 10);
		rest /= 10;
	} while (rest > 0 && count < sizeof(digits));
	size_t at = 0;
	while (count > 0)
		text[at++] = digits[--count];
	const uint64_t pairs[2] = {minutes, seconds % 60};
	for (size_t i = 0; i < 2; i++) {
		text[at++] = ':';
		text[at++] = (char)('0' + pairs[i] / 10);
		text[at++] = (char)('0' + pairs[i] % 10);
	}
	text[at] = '\0';
}

void tsw_dock_model_init(struct tsw_dock_model *model)
{
	if (!TSW_CHECK(model != NULL))
		return;
	memset(model, 0, sizeof(*model));
}

void tsw_dock_model_dismiss(struct tsw_dock_model *model)
{
	if (!TSW_CHECK(model != NULL))
		return;
	model->card_open = false;
}

/* The card after a recording: opened by a new outcome, closed by dismiss or by the next recording. */
static void update_card(struct tsw_dock_model *model, const struct tsw_status *status)
{
	if (status->phase != TSW_PHASE_IDLE) {
		model->card_open = false;
		model->seen_outcome = status->outcome;
		return;
	}
	if (status->outcome != model->seen_outcome) {
		model->seen_outcome = status->outcome;
		model->card_open = true;
		model->thank_now = !model->thanked && status->error_code == 0;
		model->thanked = model->thanked || model->thank_now;
	}
}

/* Idle: Record, the starting mode, and the reminder that this is a separate file (UX-021). */
static void build_idle(const struct tsw_session *session, struct tsw_dock_view *view)
{
	view->banner = TSW_BANNER_READY;
	view->banner_text = "State.Idle";
	view->banner_detail = session->start_in_timelapse ? "State.WillStartTimelapse" : "State.WillStartRealTime";
	view->banner_speed = session->timelapse_speed;
	view->counters_dimmed = true;
	memcpy(view->recorded, NO_TIME, sizeof(NO_TIME));
	memcpy(view->final_length, NO_TIME, sizeof(NO_TIME));
	view->speeds_shown = true;
	view->speeds_enabled = true;
	view->speed_label = "Label.Speed";
	view->start_mode_shown = true;
	view->context_text = "Hint.Separate";
}

/* After a recording: the summary, or the error with what was saved. */
static void build_card(const struct tsw_dock_model *model, const struct tsw_status *status, struct tsw_dock_view *view)
{
	view->card_shown = true;
	view->error_code = status->error_code;
	view->banner = status->error_code == 0 ? TSW_BANNER_SAVED : TSW_BANNER_ERROR;
	view->banner_text = status->error_code == 0 ? "State.Saved" : "State.Error";
	view->thank_user = model->thank_now;
	if (status->last_recorded_ns == 0) { /* the start failed: nothing was recorded */
		view->counters_dimmed = true;
		memcpy(view->recorded, NO_TIME, sizeof(NO_TIME));
		memcpy(view->final_length, NO_TIME, sizeof(NO_TIME));
	} else {
		tsw_format_duration(status->last_recorded_ns, view->recorded);
		tsw_format_duration(status->last_final_ns, view->final_length);
	}
	const uint64_t skipped =
		status->last_recorded_ns > status->last_final_ns ? status->last_recorded_ns - status->last_final_ns : 0;
	tsw_format_duration(skipped, view->skipped);
	view->context_text = (status->path != NULL && *status->path != '\0') ? "Hint.File" : NULL;
}

/* Recording or saving: the live counters, the switch, the speed row and Stop. */
static void build_recording(const struct tsw_session *session, const struct tsw_status *status,
			    struct tsw_dock_view *view)
{
	const bool saving = status->phase == TSW_PHASE_SAVING;
	const bool timelapse = !saving && status->speed > 1;
	tsw_format_duration(status->recorded_ns, view->recorded);
	tsw_format_duration(status->final_ns, view->final_length);
	view->speeds_shown = true;
	view->speeds_enabled = !saving;
	view->stop_shown = true;
	view->stop_enabled = !saving;
	view->main_enabled = !saving;
	view->context_text = timelapse ? "Hint.AudioSilence" : "Hint.File";
	if (saving) {
		view->banner = TSW_BANNER_BUSY;
		view->banner_text = "State.Saving";
		view->speed_label = "Label.Speed";
		return; /* main_text stays "Action.Record", disabled: the next action once saved (UX-011) */
	}
	if (timelapse) {
		view->banner = TSW_BANNER_TIMELAPSE;
		view->banner_text = "State.Timelapse";
		view->banner_speed = status->speed;
		view->section_shown = true;
		tsw_format_duration(status->section_recorded_ns, view->section_recorded);
		tsw_format_duration(status->section_final_ns, view->section_final);
		view->main_text = "Action.ToRealTime";
		view->speed_label = "Label.SpeedNow";
		view->speed_selected = status->speed;
		return;
	}
	view->banner = TSW_BANNER_REAL_TIME;
	view->banner_text = "State.RealTime";
	view->main_text = "Action.ToTimelapse";
	view->main_speed = session->timelapse_speed;
	view->speed_label = "Label.NextSpeed";
}

void tsw_dock_view_build(struct tsw_dock_model *model, const struct tsw_session *session,
			 const struct tsw_status *status, struct tsw_dock_view *view)
{
	if (!TSW_CHECK(model != NULL && session != NULL && status != NULL && view != NULL))
		return;
	update_card(model, status);

	/* What every state shares; each state then fills its own zones. */
	memset(view, 0, sizeof(*view));
	view->main_text = "Action.Record";
	view->main_enabled = true;
	view->speed_selected = session->timelapse_speed;
	view->start_in_timelapse = session->start_in_timelapse;
	view->context_path = status->path != NULL ? status->path : "";

	if (status->phase != TSW_PHASE_IDLE)
		build_recording(session, status, view);
	else if (model->card_open)
		build_card(model, status, view);
	else
		build_idle(session, view);
}
