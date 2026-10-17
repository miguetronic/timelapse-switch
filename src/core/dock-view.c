/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * dock-view.c: see dock-view.h. The monitoring part (health, details, speed map) follows
 * docs/design/ux-spec.md, 3.17: silence when all is well, one line when something needs attention,
 * everything else on request.
 */

#include "core/dock-view.h"

#include <stddef.h>
#include <string.h>

#include "core/check.h"

static const uint64_t SECOND_NS = 1000000000;
static const uint64_t MAX_SECONDS = 9999ull * 3600 + 59ull * 60 + 59;
static const char ZERO_TIME[] = "0:00:00"; /* idle and a failed start: dimmed, never dashes (UX-006) */
static const uint64_t MINUTE_S = 60;
static const uint64_t HOUR_S = 3600;
static const uint64_t WHOLE_HOURS_FROM = 10;        /* from 10 hours on, the minutes are noise: "93 h" */
static const uint64_t MAX_NUMBER = 9999999;         /* the largest number a TSW_NUMBER_TEXT holds */
static const uint64_t DECIMALS_BELOW = 100;         /* in tenths: one decimal under 10, whole from there on */
static const uint64_t UNIT_LIMIT = 1000;            /* a size stays under 1000 of its unit: "1.0 GB", never "1000 MB" */
static const uint64_t HEALTH_HOLD_NS = 30000000000; /* ux-spec 4.1: a warning lasts its condition plus 30 s */

/* Sizes in units of 1024, as OBS's own stats and most systems show them. */
static const struct size_unit {
	uint64_t bytes;
	const char *key;
} SIZE_UNITS[] = {
	{1ull << 20, "Unit.MB"},
	{1ull << 30, "Unit.GB"},
	{1ull << 40, "Unit.TB"},
};
enum { SIZE_UNIT_COUNT = sizeof(SIZE_UNITS) / sizeof(SIZE_UNITS[0]) };

/* The health issues, most urgent first (UX-134), with their code and words. */
static const struct health_texts {
	uint32_t issue; /* enum tsw_health_issue */
	uint32_t code;  /* TSW-E number */
	const char *text;
} HEALTH[TSW_HEALTH_KINDS] = {
	{TSW_HEALTH_DISK_LOW, 102, "Health.E102"},
	{TSW_HEALTH_LOST_FRAMES, 201, "Health.E201"},
	{TSW_HEALTH_ENCODER_BEHIND, 203, "Health.E203"},
};

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

/* --- Amounts: sizes, time left, compression --- */

/* An amount with nothing in it: not known. */
static void clear_amount(struct tsw_amount *amount)
{
	memset(amount, 0, sizeof(*amount));
}

/* Writes value, capped at MAX_NUMBER, without leading zeros. */
static void write_count(uint64_t value, char text[TSW_NUMBER_TEXT])
{
	char digits[TSW_NUMBER_TEXT];
	size_t count = 0;
	uint64_t rest = value > MAX_NUMBER ? MAX_NUMBER : value;
	do {
		digits[count++] = (char)('0' + rest % 10);
		rest /= 10;
	} while (rest > 0 && count < TSW_NUMBER_TEXT - 1);
	size_t at = 0;
	while (count > 0)
		text[at++] = digits[--count];
	text[at] = '\0';
}

/* value / unit, rounded to the nearest tenth and to the nearest whole, exact for any 64-bit value. */
static void divide(uint64_t value, uint64_t unit, uint64_t *tenths, uint64_t *whole)
{
	const uint64_t quotient = value / unit;
	const uint64_t rest = value % unit;
	if (quotient >= MAX_NUMBER) { /* beyond what is shown anyway; and quotient * 10 cannot overflow */
		*tenths = MAX_NUMBER * 10;
		*whole = MAX_NUMBER;
		return;
	}
	/* rest < unit, so rest * 10 stays far below 2^64 for any unit used here (at most 2^40, or a final
	 * length in ns). */
	*tenths = quotient * 10 + (rest * 10 + unit / 2) / unit;
	*whole = quotient + (rest >= unit - rest ? 1 : 0);
}

/* "0", "9.5" or "1.0" under 10; whole from 10 on ("10", "212"). */
static void write_tenths(uint64_t tenths, uint64_t whole, char text[TSW_NUMBER_TEXT])
{
	if (tenths == 0 || tenths >= DECIMALS_BELOW) {
		write_count(tenths == 0 ? 0 : whole, text);
		return;
	}
	text[0] = (char)('0' + tenths / 10);
	text[1] = '.';
	text[2] = (char)('0' + tenths % 10);
	text[3] = '\0';
}

void tsw_format_size(uint64_t bytes, struct tsw_amount *amount)
{
	if (!TSW_CHECK(amount != NULL))
		return;
	clear_amount(amount);
	for (size_t i = 0; i < SIZE_UNIT_COUNT; i++) {
		uint64_t tenths = 0;
		uint64_t whole = 0;
		divide(bytes, SIZE_UNITS[i].bytes, &tenths, &whole);
		if (tenths < DECIMALS_BELOW || whole < UNIT_LIMIT || i + 1 == SIZE_UNIT_COUNT) {
			amount->unit = SIZE_UNITS[i].key;
			write_tenths(tenths, whole, amount->number);
			return;
		}
	}
}

void tsw_format_time_left(uint64_t seconds, struct tsw_amount *amount)
{
	if (!TSW_CHECK(amount != NULL))
		return;
	clear_amount(amount);
	if (seconds == UINT64_MAX)
		return; /* not known */
	const uint64_t hours = seconds / HOUR_S;
	const uint64_t minutes = seconds / MINUTE_S % 60;
	if (seconds < MINUTE_S) {
		amount->unit = "Unit.UnderMinute";
	} else if (hours == 0) {
		amount->unit = "Unit.Minutes";
		write_count(minutes, amount->number);
	} else if (hours >= WHOLE_HOURS_FROM || minutes == 0) {
		amount->unit = "Unit.Hours";
		write_count(hours, amount->number);
	} else {
		amount->unit = "Unit.HoursMinutes";
		write_count(hours, amount->number);
		write_count(minutes, amount->second);
	}
}

void tsw_format_compression(uint64_t recorded_ns, uint64_t final_ns, struct tsw_amount *amount)
{
	if (!TSW_CHECK(amount != NULL))
		return;
	clear_amount(amount);
	if (final_ns == 0)
		return; /* nothing in the final video yet: no ratio */
	uint64_t tenths = 0;
	uint64_t whole = 0;
	divide(recorded_ns, final_ns, &tenths, &whole);
	amount->unit = "Unit.Times";
	write_tenths(tenths, whole, amount->number);
}

/* A new recording, or none: no issue is held any more. */
static void forget_health(struct tsw_dock_model *model)
{
	memset(model->health_until_ns, 0, sizeof(model->health_until_ns));
	model->health_clock_ns = 0;
	model->disk_seconds_left = UINT64_MAX;
}

void tsw_dock_model_init(struct tsw_dock_model *model)
{
	if (!TSW_CHECK(model != NULL))
		return;
	memset(model, 0, sizeof(*model));
	model->told_banner = TSW_BANNER_READY;
	forget_health(model);
}

void tsw_dock_model_dismiss(struct tsw_dock_model *model)
{
	if (!TSW_CHECK(model != NULL))
		return;
	model->card_open = false;
}

/* The card after a recording: opened by a new outcome, closed by dismiss or by the next recording. */
/* A map with any slice drawn: right at the start, before the first section, every slice is still 0. */
static bool map_drawn(const uint8_t map[TSW_MAP_BUCKETS])
{
	for (size_t i = 0; i < TSW_MAP_BUCKETS; i++) {
		if (map[i] != TSW_SPEED_PAUSED)
			return true;
	}
	return false;
}

/* The card after a recording: opened by a new outcome, closed by dismiss or by the next recording. The
 * speed map of the recording under way is kept for its summary card (3.13). */
static void update_card(struct tsw_dock_model *model, const struct tsw_status *status)
{
	if (status->phase != TSW_PHASE_IDLE) {
		model->card_open = false;
		model->seen_outcome = status->outcome;
		memcpy(model->map, status->map, sizeof(model->map));
		model->map_known = map_drawn(status->map);
		return;
	}
	if (status->outcome != model->seen_outcome) {
		model->seen_outcome = status->outcome;
		model->card_open = true;
		model->thank_now = !model->thanked && status->error_code == 0;
		model->thanked = model->thanked || model->thank_now;
		model->card_map = model->map_known && status->error_code == 0;
		model->map_known = false; /* given to this card: a later one never shows it again */
	}
}

static bool is_recording(enum tsw_banner banner)
{
	return banner == TSW_BANNER_REAL_TIME || banner == TSW_BANNER_TIMELAPSE;
}

/* What screen readers hear, once per change of state or speed (UX-104); NULL when nothing changed. A
 * choice made while paused is not a change of state: the main button and the rails say it. */
static const char *announcement(struct tsw_dock_model *model, const struct tsw_dock_view *view)
{
	const uint32_t speed = view->banner == TSW_BANNER_TIMELAPSE ? view->hero_speed : 1;
	const bool same = view->banner == model->told_banner && speed == model->told_speed;
	const bool was_recording = is_recording(model->told_banner);
	const bool was_paused = model->told_banner == TSW_BANNER_PAUSED;
	const bool new_issue = view->health != 0 && view->health != model->told_health;
	model->told_banner = view->banner;
	model->told_speed = speed;
	model->told_health = view->health;
	if (same) /* a new health issue is news too; its count going up is not (UX-134) */
		return new_issue ? "A11y.Announce.Health" : NULL;
	if (view->banner == TSW_BANNER_PAUSED)
		return "A11y.Announce.Paused";
	if (is_recording(view->banner)) {
		if (was_paused)
			return "A11y.Announce.Resumed";
		return was_recording ? "A11y.Announce.Switch" : "A11y.Announce.Start";
	}
	if (view->banner == TSW_BANNER_SAVED)
		return "A11y.Announce.Saved";
	if (view->banner == TSW_BANNER_ERROR)
		return "A11y.Announce.Error";
	return NULL;
}

/* Nothing recorded (idle, or a start that failed): the counters at zero, dimmed (UX-006), and the
 * status line too, so it is never an empty hole. */
static void show_zero_times(struct tsw_dock_view *view)
{
	view->counters_dimmed = true;
	memcpy(view->recorded, ZERO_TIME, sizeof(ZERO_TIME));
	memcpy(view->final_length, ZERO_TIME, sizeof(ZERO_TIME));
	view->section_shown = true;
	view->section_speed = 1;
	tsw_format_short_duration(0, view->section_recorded);
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
	if (model->card_map) { /* the summary's speed map (3.13) */
		memcpy(view->map, model->map, sizeof(view->map));
		view->map_shown = true;
	}
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
	view->pause_enabled = false;
	view->main_enabled = false; /* main_text stays "Action.Record": the next action once saved */
	view->speed_label = "Label.Speed";
	view->context_text = "Hint.Saving";
}

/*
 * Paused: nothing reaches the file. The counters stand still, the status line counts the pause, and
 * the hero, the main button and the one "Resume in" rail (Real time and the speeds) all name the
 * mode it will resume in; Pause becomes Resume in the same place (UX-128 to UX-132). The rail is
 * zone D's only rail, as while recording, so nothing below it moves.
 */
static void build_paused(const struct tsw_session *session, const struct tsw_status *status, struct tsw_dock_view *view)
{
	const uint32_t resume = tsw_session_effective_speed(session, status);
	const bool timelapse = resume > 1;
	view->banner = TSW_BANNER_PAUSED;
	view->tally_text = "State.Tally.Paused";
	view->hero_text = timelapse ? "State.Hero.ResumesTimelapse" : "State.Hero.ResumesRealTime";
	view->hero_quiet = true; /* sentence case: a pause never looks like a recording (UX-129) */
	view->hero_speed = timelapse ? resume : session->timelapse_speed;
	view->main_text = timelapse ? "Action.ResumeTimelapse" : "Action.ResumeRealTime";
	view->main_speed = resume;
	view->section_shown = true;
	view->section_label = "Counter.Paused";
	view->section_speed = 1;
	tsw_format_duration(status->section_recorded_ns, view->section_recorded);
	view->section_final[0] = '\0';
	view->start_in_timelapse = timelapse;
	view->speeds_real_time = true;
	view->speed_label = "Label.ResumeIn";
	view->speed_selected = resume; /* 1 selects "Real time" */
	view->pause_text = "Action.Resume";
	view->context_text = "Hint.Paused";
}

/* Recording, paused or saving: the counters, the switch, the speed rail, Pause and Stop. */
static void build_recording(const struct tsw_session *session, const struct tsw_status *status,
			    struct tsw_dock_view *view)
{
	tsw_format_duration(status->recorded_ns, view->recorded);
	tsw_format_duration(status->final_ns, view->final_length);
	view->speeds_shown = true;
	view->speeds_enabled = true;
	view->pause_shown = true;
	view->pause_enabled = true;
	view->pause_text = "Action.Pause";
	view->stop_shown = true;
	view->stop_enabled = true;
	view->main_enabled = true;
	if (status->phase == TSW_PHASE_PAUSED) {
		build_paused(session, status, view);
		return;
	}
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

/*
 * The details (UX-135, ux-spec 3.17): where the time went, how the file and the disk keep up, and the
 * speed map, all from the recorder's counters. What is not measured yet stays not known.
 */
static void show_details(const struct tsw_status *status, const struct tsw_health *health, struct tsw_dock_view *view)
{
	const struct tsw_timeline_totals *totals = &status->totals;
	view->details_shown = true;
	const uint64_t skipped = status->recorded_ns > status->final_ns ? status->recorded_ns - status->final_ns : 0;
	tsw_format_short_duration(skipped, view->waiting_skipped);
	tsw_format_compression(status->recorded_ns, status->final_ns, &view->compression);
	tsw_format_short_duration(totals->real_time_ns, view->real_time);
	tsw_format_short_duration(totals->timelapse_ns, view->timelapse);
	tsw_format_short_duration(totals->paused_ns, view->paused);
	view->switches = totals->switches;
	view->pauses = totals->pauses;
	view->chapters = totals->switches + 1;
	if (status->file_bytes > 0)
		tsw_format_size(status->file_bytes, &view->file_size);
	if (status->bytes_per_second > 0)
		tsw_format_size(status->bytes_per_second * MINUTE_S, &view->file_rate);
	if (status->disk_free_bytes > 0)
		tsw_format_size(status->disk_free_bytes, &view->disk_free);
	tsw_format_time_left(health->disk_seconds_left, &view->time_left);
	view->backlog = status->feed_waiting;
	view->backlog_size = status->feed_size;
	memcpy(view->map, status->map, sizeof(view->map));
	view->map_shown = map_drawn(status->map);
}

/*
 * Health (UX-134): the most urgent issue, in zone F, while recording or paused (while saving, F says to
 * keep OBS open: UX-011). Each issue stays HEALTH_HOLD_NS of recorded time after its condition ends, so
 * a condition at its edge never makes the line blink (UX-063); lost frames never end, since they are
 * counted from the start.
 */
static void show_health(struct tsw_dock_model *model, const struct tsw_status *status, const struct tsw_health *health,
			struct tsw_dock_view *view)
{
	const uint64_t now = status->recorded_ns;
	if (status->phase == TSW_PHASE_IDLE || now < model->health_clock_ns)
		forget_health(model);
	model->health_clock_ns = now;
	if (status->phase == TSW_PHASE_IDLE)
		return;
	/* 1. What is wrong now, and what was until a moment ago. */
	uint32_t issues = 0;
	for (size_t i = 0; i < TSW_HEALTH_KINDS; i++) {
		if ((health->issues & HEALTH[i].issue) != 0)
			model->health_until_ns[i] = now + HEALTH_HOLD_NS;
		if (now < model->health_until_ns[i])
			issues |= HEALTH[i].issue;
	}
	if ((health->issues & (uint32_t)TSW_HEALTH_DISK_LOW) != 0)
		model->disk_seconds_left = health->disk_seconds_left;
	if (status->phase == TSW_PHASE_SAVING)
		return;
	/* 2. The most urgent one, with its value. */
	for (size_t i = 0; i < TSW_HEALTH_KINDS; i++) {
		if ((issues & HEALTH[i].issue) == 0)
			continue;
		view->health = HEALTH[i].issue;
		view->health_code = HEALTH[i].code;
		view->health_text = HEALTH[i].text;
		if (HEALTH[i].issue == TSW_HEALTH_DISK_LOW) {
			tsw_format_time_left(model->disk_seconds_left, &view->health_amount);
		} else if (HEALTH[i].issue == TSW_HEALTH_LOST_FRAMES) {
			view->health_amount.unit = "Unit.Count";
			write_count(status->frames_lost, view->health_amount.number);
		}
		return;
	}
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
	view->section_label = "Counter.Stretch";
	view->context_path = status->path != NULL ? status->path : "";

	/* The recorder's counters, read once for the health line and the details. */
	const struct tsw_health_input input = {status->frames_lost, status->feed_waiting, status->feed_size,
					       status->disk_free_bytes, status->bytes_per_second};
	struct tsw_health health;
	tsw_health_evaluate(&input, &health);

	if (status->phase != TSW_PHASE_IDLE) {
		build_recording(session, status, view);
		show_details(status, &health, view);
	} else if (model->card_open) {
		build_card(model, status, view);
	} else {
		build_idle(session, view);
	}
	show_health(model, status, &health, view);
	view->announce_text = announcement(model, view);
}
