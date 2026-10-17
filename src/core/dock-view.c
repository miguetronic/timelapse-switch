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
static const char ZERO_TIME[] = "0:00:00"; /* idle and a failed start: dimmed, never dashes (UX-006) */

/* The error codes the dock has texts for (data/locale); any other one shows the E901 texts. */
struct error_texts {
	uint32_t code;
	const char *headline; /* key: the hero of the error state, 2 to 5 plain words (ux-spec 4.2) */
};
static const struct error_texts ERRORS[] = {
	{1, "Error.E001.Headline"},
	{2, "Error.E002.Headline"},
	{103, "Error.E103.Headline"},
	{901, "Error.E901.Headline"},
};
enum { ERROR_COUNT = sizeof(ERRORS) / sizeof(ERRORS[0]) };
static const size_t UNEXPECTED_ERROR = ERROR_COUNT - 1; /* the last one: E901 */

/* Writes digits of value, without leading zeros, at text + at; returns the new end. */
static size_t write_number(char text[TSW_TIME_TEXT], size_t at, uint64_t value)
{
	char digits[4];
	size_t count = 0;
	do {
		digits[count++] = (char)('0' + value % 10);
		value /= 10;
	} while (value > 0 && count < sizeof(digits));
	while (count > 0)
		text[at++] = digits[--count];
	return at;
}

/* Writes ":nn" for each pair (minutes, seconds), with two digits each. */
static size_t write_pairs(char text[TSW_TIME_TEXT], size_t at, const uint64_t *pairs, size_t count)
{
	for (size_t i = 0; i < count && i < 2; i++) {
		text[at++] = ':';
		text[at++] = (char)('0' + pairs[i] / 10);
		text[at++] = (char)('0' + pairs[i] % 10);
	}
	return at;
}

/* Written by hand: no printf in the core. */
void tsw_format_duration(uint64_t ns, char text[TSW_TIME_TEXT])
{
	if (!TSW_CHECK(text != NULL))
		return;
	uint64_t seconds = ns / SECOND_NS;
	if (seconds > MAX_SECONDS)
		seconds = MAX_SECONDS;
	const uint64_t pairs[2] = {seconds / 60 % 60, seconds % 60};
	size_t at = write_number(text, 0, seconds / 3600);
	at = write_pairs(text, at, pairs, 2);
	if (!TSW_CHECK(at < TSW_TIME_TEXT))
		at = TSW_TIME_TEXT - 1;
	text[at] = '\0';
}

void tsw_format_short_duration(uint64_t ns, char text[TSW_TIME_TEXT])
{
	if (!TSW_CHECK(text != NULL))
		return;
	const uint64_t seconds = ns / SECOND_NS;
	if (seconds >= 3600) { /* from an hour on, the hours matter more than saving room */
		tsw_format_duration(ns, text);
		return;
	}
	const uint64_t pairs[1] = {seconds % 60};
	size_t at = write_number(text, 0, seconds / 60);
	at = write_pairs(text, at, pairs, 1);
	if (!TSW_CHECK(at < TSW_TIME_TEXT))
		at = TSW_TIME_TEXT - 1;
	text[at] = '\0';
}

void tsw_dock_model_init(struct tsw_dock_model *model)
{
	if (!TSW_CHECK(model != NULL))
		return;
	memset(model, 0, sizeof(*model));
	model->told_banner = TSW_BANNER_READY;
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

static bool is_recording(enum tsw_banner banner)
{
	return banner == TSW_BANNER_REAL_TIME || banner == TSW_BANNER_TIMELAPSE;
}

/* What screen readers hear, once per change of state or speed (UX-104); NULL when nothing changed. */
static const char *announcement(struct tsw_dock_model *model, const struct tsw_dock_view *view)
{
	const uint32_t speed = view->banner == TSW_BANNER_TIMELAPSE ? view->hero_speed : 1;
	const bool same = view->banner == model->told_banner && speed == model->told_speed;
	const bool was_recording = is_recording(model->told_banner);
	model->told_banner = view->banner;
	model->told_speed = speed;
	if (same)
		return NULL;
	if (is_recording(view->banner))
		return was_recording ? "A11y.Announce.Switch" : "A11y.Announce.Start";
	if (view->banner == TSW_BANNER_SAVED)
		return "A11y.Announce.Saved";
	if (view->banner == TSW_BANNER_ERROR)
		return "A11y.Announce.Error";
	return NULL;
}

static void show_zero_times(struct tsw_dock_view *view)
{
	view->counters_dimmed = true;
	memcpy(view->recorded, ZERO_TIME, sizeof(ZERO_TIME));
	memcpy(view->final_length, ZERO_TIME, sizeof(ZERO_TIME));
}

/* The current stretch for the status line: live while recording, frozen while saving. */
static void show_stretch(const struct tsw_status *status, uint32_t speed, struct tsw_dock_view *view)
{
	view->section_shown = true;
	view->section_speed = speed;
	tsw_format_short_duration(status->section_recorded_ns, view->section_recorded);
	tsw_format_short_duration(status->section_final_ns, view->section_final);
}

/* Idle: Record, the starting mode, and the reminder that this is a separate file (UX-021). */
static void build_idle(const struct tsw_session *session, struct tsw_dock_view *view)
{
	view->banner = TSW_BANNER_READY;
	view->tally_text = "State.Tally.Ready";
	view->hero_text = session->start_in_timelapse ? "State.Hero.StartsTimelapse" : "State.Hero.StartsRealTime";
	view->hero_quiet = true;
	view->hero_speed = session->timelapse_speed;
	show_zero_times(view);
	/* The status line too, at zero and dimmed like the counters: never an empty hole. */
	view->section_shown = true;
	view->section_speed = 1;
	tsw_format_short_duration(0, view->section_recorded);
	view->speeds_shown = true;
	view->speeds_enabled = true;
	view->speed_label = "Label.Speed";
	view->start_mode_shown = true;
	view->context_text = "Hint.Separate";
}

/* The error texts for a code: its own, or the unexpected error's. */
static const struct error_texts *error_texts(uint32_t code)
{
	for (size_t i = 0; i < ERROR_COUNT; i++) {
		if (ERRORS[i].code == code)
			return &ERRORS[i];
	}
	return &ERRORS[UNEXPECTED_ERROR];
}

/* After a recording: the summary, or the error with what was saved. The file name is in the card. */
static void build_card(const struct tsw_dock_model *model, const struct tsw_status *status, struct tsw_dock_view *view)
{
	const bool failed = status->error_code != 0;
	view->card_shown = true;
	view->error_code = status->error_code;
	view->banner = failed ? TSW_BANNER_ERROR : TSW_BANNER_SAVED;
	view->tally_text = failed ? "State.Tally.Error" : "State.Tally.Saved";
	view->hero_quiet = true;
	view->hero_text = "State.Hero.Saved";
	if (failed) {
		const struct error_texts *texts = error_texts(status->error_code);
		view->error_texts = texts->code;
		view->hero_text = texts->headline;
	}
	view->thank_user = model->thank_now;
	if (status->last_recorded_ns == 0) { /* the start failed: nothing was recorded */
		show_zero_times(view);
	} else {
		tsw_format_duration(status->last_recorded_ns, view->recorded);
		tsw_format_duration(status->last_final_ns, view->final_length);
	}
	/* The waiting skipped, as one big figure without empty hours ("52:35"); none if nothing was. */
	const uint64_t skipped =
		status->last_recorded_ns > status->last_final_ns ? status->last_recorded_ns - status->last_final_ns : 0;
	if (skipped >= SECOND_NS)
		tsw_format_short_duration(skipped, view->skipped);
}

/* Saving: the counters and the last stretch freeze, and what cannot act is disabled, not hidden
 * (UX-011). */
static void build_saving(struct tsw_dock_view *view)
{
	view->banner = TSW_BANNER_BUSY;
	view->tally_text = "State.Tally.Saving";
	view->hero_text = "State.Hero.Saving";
	view->hero_quiet = true;
	view->speeds_enabled = false;
	view->stop_enabled = false;
	view->main_enabled = false; /* main_text stays "Action.Record": the next action once saved */
	view->speed_label = "Label.Speed";
	view->context_text = "Hint.Saving";
}

/* Recording or saving: the live counters, the switch, the speed rail and Stop. */
static void build_recording(const struct tsw_session *session, const struct tsw_status *status,
			    struct tsw_dock_view *view)
{
	tsw_format_duration(status->recorded_ns, view->recorded);
	tsw_format_duration(status->final_ns, view->final_length);
	view->speeds_shown = true;
	view->speeds_enabled = true;
	view->stop_shown = true;
	view->stop_enabled = true;
	view->main_enabled = true;
	const bool timelapse = status->speed > 1;
	show_stretch(status, timelapse ? status->speed : 1, view);
	if (status->phase == TSW_PHASE_SAVING) {
		build_saving(view);
		return;
	}
	view->tally_text = "State.Tally.Rec";
	if (timelapse) {
		view->banner = TSW_BANNER_TIMELAPSE;
		view->hero_text = "State.Hero.Timelapse";
		view->hero_speed = status->speed;
		view->main_text = "Action.ToRealTime";
		view->speed_label = "Label.SpeedNow";
		view->speed_selected = status->speed;
		view->context_text = "Hint.AudioSilence";
		return;
	}
	view->banner = TSW_BANNER_REAL_TIME;
	view->hero_text = "State.Hero.RealTime";
	view->main_text = "Action.ToTimelapse";
	view->main_speed = session->timelapse_speed;
	view->speed_label = "Label.NextSpeed";
	view->context_text = "Hint.File";
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
	view->announce_text = announcement(model, view);
}
