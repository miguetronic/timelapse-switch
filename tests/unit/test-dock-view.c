/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * test-dock-view.c: what each zone of the dock shows in each state (core/dock-view.h).
 */

#include <string.h>

#include "core/dock-view.h"
#include "test.h"

static const uint64_t SECOND_NS = 1000000000;

static void expect_text(const char *actual, const char *expected, int line)
{
	if (actual == NULL || expected == NULL ? actual != expected : strcmp(actual, expected) != 0) {
		printf("%s:%d: expected \"%s\", got \"%s\"\n", __FILE__, line, expected ? expected : "(null)",
		       actual ? actual : "(null)");
		tsw_test_failures++;
	}
}
#define EXPECT_TEXT(actual, expected) expect_text(actual, expected, __LINE__)

static struct tsw_session new_session(void)
{
	struct tsw_session session;
	TSW_EXPECT_EQ(tsw_session_init(&session, 4, 0), TSW_OK);
	return session;
}

static struct tsw_status recording(uint32_t speed)
{
	struct tsw_status status = {0};
	status.phase = TSW_PHASE_RECORDING;
	status.speed = speed;
	status.recorded_ns = 754 * SECOND_NS; /* 0:12:34 */
	status.final_ns = 400 * SECOND_NS;    /* 0:06:40 */
	status.section_recorded_ns = 120 * SECOND_NS;
	status.section_final_ns = 30 * SECOND_NS;
	status.path = "/videos/2026-10-16 10-00-00 tswitch.mp4";
	return status;
}

/* Paused for 0:01:12 after recording at speed: the counters stand where the pause began. */
static struct tsw_status paused(uint32_t speed)
{
	struct tsw_status status = recording(speed);
	status.phase = TSW_PHASE_PAUSED;
	status.section_recorded_ns = 72 * SECOND_NS;
	status.section_final_ns = 0;
	return status;
}

static struct tsw_status ended(uint32_t outcome, uint32_t error_code)
{
	struct tsw_status status = {0};
	status.phase = TSW_PHASE_IDLE;
	status.path = "/videos/2026-10-16 10-00-00 tswitch.mp4";
	status.outcome = outcome;
	status.error_code = error_code;
	status.last_recorded_ns = 5025 * SECOND_NS; /* 1:23:45 */
	status.last_final_ns = 1870 * SECOND_NS;    /* 0:31:10 */
	return status;
}

static struct tsw_dock_view build(struct tsw_dock_model *model, const struct tsw_session *session,
				  struct tsw_status status)
{
	struct tsw_dock_view view;
	tsw_dock_view_build(model, session, &status, &view);
	return view;
}

static void durations_read_as_hours_minutes_seconds(void)
{
	char text[TSW_TIME_TEXT];
	tsw_format_duration(0, text);
	EXPECT_TEXT(text, "0:00:00");
	tsw_format_duration(59 * SECOND_NS + 999999999, text); /* whole seconds, never rounded up */
	EXPECT_TEXT(text, "0:00:59");
	tsw_format_duration(61 * SECOND_NS, text);
	EXPECT_TEXT(text, "0:01:01");
	tsw_format_duration(36000 * SECOND_NS, text);
	EXPECT_TEXT(text, "10:00:00");
	tsw_format_duration(UINT64_MAX, text);
	EXPECT_TEXT(text, "9999:59:59");
}

/* The status line is short: "19:37" under an hour, "1:02:03" from an hour on. */
static void short_durations_drop_the_hours_under_an_hour(void)
{
	char text[TSW_TIME_TEXT];
	tsw_format_short_duration(0, text);
	EXPECT_TEXT(text, "0:00");
	tsw_format_short_duration(294 * SECOND_NS, text);
	EXPECT_TEXT(text, "4:54");
	tsw_format_short_duration(1177 * SECOND_NS, text);
	EXPECT_TEXT(text, "19:37");
	tsw_format_short_duration(3723 * SECOND_NS, text);
	EXPECT_TEXT(text, "1:02:03");
}

static void idle_offers_record_and_the_starting_mode(void)
{
	struct tsw_dock_model model;
	tsw_dock_model_init(&model);
	struct tsw_session session = new_session();
	struct tsw_status status = {0};
	struct tsw_dock_view view = build(&model, &session, status);
	TSW_EXPECT_EQ(view.banner, TSW_BANNER_READY);
	EXPECT_TEXT(view.tally_text, "State.Tally.Ready");
	EXPECT_TEXT(view.hero_text, "State.Hero.StartsRealTime");
	TSW_EXPECT_TRUE(view.hero_quiet);
	TSW_EXPECT_TRUE(view.counters_dimmed);
	EXPECT_TEXT(view.recorded, "0:00:00"); /* UX-006: a dimmed 0:00:00 */
	EXPECT_TEXT(view.final_length, "0:00:00");
	EXPECT_TEXT(view.main_text, "Action.Record");
	TSW_EXPECT_TRUE(view.main_enabled);
	TSW_EXPECT_TRUE(view.speeds_shown && view.speeds_enabled && view.start_mode_shown);
	TSW_EXPECT_TRUE(!view.speeds_real_time);
	EXPECT_TEXT(view.speed_label, "Label.Speed");
	TSW_EXPECT_EQ(view.speed_selected, 4);
	TSW_EXPECT_TRUE(!view.stop_shown);
	TSW_EXPECT_TRUE(!view.card_shown);
	EXPECT_TEXT(view.context_text, "Hint.Separate");
	TSW_EXPECT_TRUE(view.announce_text == NULL);

	session.start_in_timelapse = true;
	view = build(&model, &session, status);
	EXPECT_TEXT(view.hero_text, "State.Hero.StartsTimelapse");
	TSW_EXPECT_EQ(view.hero_speed, 4);
	TSW_EXPECT_TRUE(view.start_in_timelapse);
}

/* The status line is never an empty hole: idle shows the stretch at zero, dimmed like the counters. */
static void idle_shows_the_stretch_at_zero(void)
{
	struct tsw_dock_model model;
	tsw_dock_model_init(&model);
	struct tsw_session session = new_session();
	struct tsw_status status = {0};
	struct tsw_dock_view view = build(&model, &session, status);
	TSW_EXPECT_TRUE(view.section_shown);
	TSW_EXPECT_EQ(view.section_speed, 1);
	EXPECT_TEXT(view.section_recorded, "0:00");
	TSW_EXPECT_TRUE(view.counters_dimmed);
}

static void real_time_offers_the_switch(void)
{
	struct tsw_dock_model model;
	tsw_dock_model_init(&model);
	struct tsw_session session = new_session();
	struct tsw_dock_view view = build(&model, &session, recording(1));
	TSW_EXPECT_EQ(view.banner, TSW_BANNER_REAL_TIME);
	EXPECT_TEXT(view.tally_text, "State.Tally.Rec");
	EXPECT_TEXT(view.hero_text, "State.Hero.RealTime");
	TSW_EXPECT_TRUE(!view.hero_quiet);
	TSW_EXPECT_TRUE(!view.counters_dimmed);
	EXPECT_TEXT(view.recorded, "0:12:34");
	EXPECT_TEXT(view.final_length, "0:06:40");
	EXPECT_TEXT(view.main_text, "Action.ToTimelapse");
	TSW_EXPECT_EQ(view.main_speed, 4);
	EXPECT_TEXT(view.speed_label, "Label.NextSpeed");
	TSW_EXPECT_TRUE(!view.start_mode_shown);
	TSW_EXPECT_TRUE(view.stop_shown && view.stop_enabled);
	EXPECT_TEXT(view.context_text, "Hint.File");
	EXPECT_TEXT(view.context_path, "/videos/2026-10-16 10-00-00 tswitch.mp4");
}

/* UX-007: the status line shows the current stretch in real time too, at speed 1. */
static void real_time_shows_the_current_stretch(void)
{
	struct tsw_dock_model model;
	tsw_dock_model_init(&model);
	struct tsw_session session = new_session();
	struct tsw_dock_view view = build(&model, &session, recording(1));
	TSW_EXPECT_TRUE(view.section_shown);
	TSW_EXPECT_EQ(view.section_speed, 1);
	EXPECT_TEXT(view.section_recorded, "2:00");
}

static void timelapse_offers_the_way_back_and_shows_the_section(void)
{
	struct tsw_dock_model model;
	tsw_dock_model_init(&model);
	struct tsw_session session = new_session();
	struct tsw_dock_view view = build(&model, &session, recording(8));
	TSW_EXPECT_EQ(view.banner, TSW_BANNER_TIMELAPSE);
	EXPECT_TEXT(view.tally_text, "State.Tally.Rec");
	EXPECT_TEXT(view.hero_text, "State.Hero.Timelapse");
	TSW_EXPECT_TRUE(!view.hero_quiet);
	TSW_EXPECT_EQ(view.hero_speed, 8);
	TSW_EXPECT_TRUE(view.section_shown);
	TSW_EXPECT_EQ(view.section_speed, 8);
	EXPECT_TEXT(view.section_recorded, "2:00");
	EXPECT_TEXT(view.section_final, "0:30");
	EXPECT_TEXT(view.main_text, "Action.ToRealTime");
	EXPECT_TEXT(view.speed_label, "Label.SpeedNow");
	TSW_EXPECT_EQ(view.speed_selected, 8); /* the speed in use, whatever chip was chosen before */
	TSW_EXPECT_TRUE(view.stop_shown && view.stop_enabled);
	EXPECT_TEXT(view.context_text, "Hint.AudioSilence");
}

/* UX-011: while saving, what cannot act is disabled, not hidden. */
static void saving_disables_what_cannot_act(void)
{
	struct tsw_dock_model model;
	tsw_dock_model_init(&model);
	struct tsw_session session = new_session();
	struct tsw_status status = recording(1);
	status.phase = TSW_PHASE_SAVING;
	struct tsw_dock_view view = build(&model, &session, status);
	TSW_EXPECT_EQ(view.banner, TSW_BANNER_BUSY);
	EXPECT_TEXT(view.tally_text, "State.Tally.Saving");
	EXPECT_TEXT(view.hero_text, "State.Hero.Saving");
	TSW_EXPECT_TRUE(view.hero_quiet);
	EXPECT_TEXT(view.context_text, "Hint.Saving");
	EXPECT_TEXT(view.main_text, "Action.Record");
	TSW_EXPECT_TRUE(!view.main_enabled);
	TSW_EXPECT_TRUE(view.speeds_shown && !view.speeds_enabled);
	TSW_EXPECT_TRUE(view.stop_shown && !view.stop_enabled);
	EXPECT_TEXT(view.recorded, "0:12:34");
}

/* While saving, the counters stay where Stop was pressed, and so does the last stretch. */
static void saving_keeps_the_last_stretch_frozen(void)
{
	struct tsw_dock_model model;
	tsw_dock_model_init(&model);
	struct tsw_session session = new_session();
	struct tsw_status status = recording(8);
	status.phase = TSW_PHASE_SAVING;
	struct tsw_dock_view view = build(&model, &session, status);
	TSW_EXPECT_TRUE(view.section_shown);
	TSW_EXPECT_EQ(view.section_speed, 8);
	EXPECT_TEXT(view.section_recorded, "2:00");
	EXPECT_TEXT(view.section_final, "0:30");
	TSW_EXPECT_TRUE(!view.counters_dimmed);
}

static void a_saved_recording_opens_the_summary(void)
{
	struct tsw_dock_model model;
	tsw_dock_model_init(&model);
	struct tsw_session session = new_session();
	struct tsw_dock_view view = build(&model, &session, ended(1, 0));
	TSW_EXPECT_TRUE(view.card_shown);
	TSW_EXPECT_EQ(view.error_code, 0);
	TSW_EXPECT_EQ(view.banner, TSW_BANNER_SAVED);
	EXPECT_TEXT(view.tally_text, "State.Tally.Saved");
	EXPECT_TEXT(view.hero_text, "State.Hero.Saved");
	TSW_EXPECT_TRUE(view.hero_quiet);
	TSW_EXPECT_TRUE(!view.counters_dimmed);
	EXPECT_TEXT(view.recorded, "1:23:45");
	EXPECT_TEXT(view.final_length, "0:31:10");
	EXPECT_TEXT(view.skipped, "52:35"); /* one big figure: no empty hours (UX-070) */
	TSW_EXPECT_TRUE(view.thank_user);
	EXPECT_TEXT(view.main_text, "Action.Record");
	TSW_EXPECT_TRUE(view.main_enabled && !view.speeds_shown && !view.stop_shown);
	TSW_EXPECT_TRUE(view.context_text == NULL); /* the file name is in the card */
	EXPECT_TEXT(view.context_path, "/videos/2026-10-16 10-00-00 tswitch.mp4");

	/* The card stays until dismissed; the warm line is used once per OBS session. */
	view = build(&model, &session, ended(1, 0));
	TSW_EXPECT_TRUE(view.card_shown && view.thank_user);
	tsw_dock_model_dismiss(&model);
	view = build(&model, &session, ended(1, 0));
	TSW_EXPECT_TRUE(!view.card_shown);
	TSW_EXPECT_EQ(view.banner, TSW_BANNER_READY);
	view = build(&model, &session, ended(2, 0));
	TSW_EXPECT_TRUE(view.card_shown && !view.thank_user);
}

/* A recording that never sped up skipped no waiting: the summary shows no figure for it. */
static void a_recording_that_never_sped_up_skips_nothing(void)
{
	struct tsw_dock_model model;
	tsw_dock_model_init(&model);
	struct tsw_session session = new_session();
	struct tsw_status status = ended(1, 0);
	status.last_final_ns = status.last_recorded_ns;
	struct tsw_dock_view view = build(&model, &session, status);
	TSW_EXPECT_TRUE(view.card_shown);
	EXPECT_TEXT(view.skipped, "");
}

static void a_failure_opens_the_error_card(void)
{
	struct tsw_dock_model model;
	tsw_dock_model_init(&model);
	struct tsw_session session = new_session();
	struct tsw_dock_view view = build(&model, &session, ended(1, 103));
	TSW_EXPECT_TRUE(view.card_shown);
	TSW_EXPECT_EQ(view.error_code, 103);
	TSW_EXPECT_EQ(view.banner, TSW_BANNER_ERROR);
	EXPECT_TEXT(view.tally_text, "State.Tally.Error");
	EXPECT_TEXT(view.hero_text, "Error.E103.Headline"); /* a plain headline, in Bold (UX-081) */
	TSW_EXPECT_EQ(view.error_texts, 103);
	TSW_EXPECT_TRUE(!view.thank_user); /* never in errors (UX-072) */
	TSW_EXPECT_TRUE(view.context_text == NULL);
	EXPECT_TEXT(view.main_text, "Action.Record");
	TSW_EXPECT_TRUE(view.main_enabled);
}

/* A start that failed recorded nothing: no times to show, and no file. */
static void a_failed_start_shows_no_times(void)
{
	struct tsw_dock_model model;
	tsw_dock_model_init(&model);
	struct tsw_session session = new_session();
	struct tsw_status status = ended(1, 1);
	status.last_recorded_ns = 0;
	status.last_final_ns = 0;
	status.path = "";
	struct tsw_dock_view view = build(&model, &session, status);
	TSW_EXPECT_TRUE(view.card_shown);
	TSW_EXPECT_TRUE(view.counters_dimmed);
	EXPECT_TEXT(view.recorded, "0:00:00");
	EXPECT_TEXT(view.final_length, "0:00:00");
	EXPECT_TEXT(view.hero_text, "Error.E001.Headline");
	TSW_EXPECT_TRUE(view.context_text == NULL);
}

/* A failed start has no file to name, so its status line shows the stretch at zero, dimmed like
 * the counters, as idle does: never an empty hole. */
static void a_failed_start_shows_the_stretch_at_zero(void)
{
	struct tsw_dock_model model;
	tsw_dock_model_init(&model);
	struct tsw_session session = new_session();
	struct tsw_status status = ended(1, 1);
	status.last_recorded_ns = 0;
	status.last_final_ns = 0;
	status.path = "";
	struct tsw_dock_view view = build(&model, &session, status);
	TSW_EXPECT_TRUE(view.section_shown);
	TSW_EXPECT_EQ(view.section_speed, 1);
	EXPECT_TEXT(view.section_recorded, "0:00");
	TSW_EXPECT_TRUE(view.counters_dimmed);
}

/* After a take, the status line is the file's (the dock names it there), not a stretch. */
static void a_saved_take_shows_no_stretch(void)
{
	struct tsw_dock_model model;
	tsw_dock_model_init(&model);
	struct tsw_session session = new_session();
	struct tsw_dock_view view = build(&model, &session, ended(1, 0));
	TSW_EXPECT_TRUE(view.card_shown && !view.section_shown);
}

/* A code the dock has no texts for is shown as the unexpected error, TSW-E901. */
static void an_unknown_error_uses_the_unexpected_error_texts(void)
{
	struct tsw_dock_model model;
	tsw_dock_model_init(&model);
	struct tsw_session session = new_session();
	struct tsw_dock_view view = build(&model, &session, ended(1, 777));
	TSW_EXPECT_EQ(view.error_code, 777);
	TSW_EXPECT_EQ(view.error_texts, 901);
	EXPECT_TEXT(view.hero_text, "Error.E901.Headline");
}

/* UX-104: start, every switch and speed change, saved and errors are announced once each. */
static void changes_of_state_are_announced_once(void)
{
	struct tsw_dock_model model;
	tsw_dock_model_init(&model);
	struct tsw_session session = new_session();
	struct tsw_dock_view view = build(&model, &session, recording(1));
	EXPECT_TEXT(view.announce_text, "A11y.Announce.Start");
	view = build(&model, &session, recording(1));
	TSW_EXPECT_TRUE(view.announce_text == NULL);
	view = build(&model, &session, recording(4));
	EXPECT_TEXT(view.announce_text, "A11y.Announce.Switch");
	view = build(&model, &session, recording(8));
	EXPECT_TEXT(view.announce_text, "A11y.Announce.Switch");
	view = build(&model, &session, recording(1));
	EXPECT_TEXT(view.announce_text, "A11y.Announce.Switch");
	struct tsw_status saving = recording(1);
	saving.phase = TSW_PHASE_SAVING;
	view = build(&model, &session, saving);
	TSW_EXPECT_TRUE(view.announce_text == NULL);
	view = build(&model, &session, ended(1, 0));
	EXPECT_TEXT(view.announce_text, "A11y.Announce.Saved");
	view = build(&model, &session, ended(1, 0));
	TSW_EXPECT_TRUE(view.announce_text == NULL);
	view = build(&model, &session, ended(2, 103));
	EXPECT_TEXT(view.announce_text, "A11y.Announce.Error");
}

/* UX-128: while recording, in real time or in timelapse, Pause sits next to Stop; while saving both
 * are disabled, not hidden (UX-011); idle and the cards have neither. */
static void recording_offers_pause_next_to_stop(void)
{
	struct tsw_dock_model model;
	tsw_dock_model_init(&model);
	struct tsw_session session = new_session();
	const uint32_t speeds[] = {1, 8};
	for (size_t i = 0; i < sizeof(speeds) / sizeof(speeds[0]); i++) {
		struct tsw_dock_view view = build(&model, &session, recording(speeds[i]));
		TSW_EXPECT_TRUE(view.pause_shown && view.pause_enabled);
		EXPECT_TEXT(view.pause_text, "Action.Pause");
		TSW_EXPECT_TRUE(view.stop_shown && view.stop_enabled);
		TSW_EXPECT_TRUE(!view.speeds_real_time && !view.start_mode_shown); /* as approved: one speed rail */
	}
	struct tsw_status saving = recording(1);
	saving.phase = TSW_PHASE_SAVING;
	struct tsw_dock_view view = build(&model, &session, saving);
	TSW_EXPECT_TRUE(view.pause_shown && !view.pause_enabled);
	EXPECT_TEXT(view.pause_text, "Action.Pause");

	struct tsw_dock_model fresh;
	tsw_dock_model_init(&fresh);
	struct tsw_status idle = {0};
	view = build(&fresh, &session, idle);
	TSW_EXPECT_TRUE(!view.pause_shown);
	view = build(&fresh, &session, ended(1, 0));
	TSW_EXPECT_TRUE(!view.pause_shown && !view.stop_shown);
}

/* UX-129 to UX-131: paused from real time, every zone says it will resume in real time. Zone D is one
 * rail, "Resume in": Real time (speed 1) and the speeds, as tall as the recording speed rail, so
 * nothing below it moves. */
static void paused_from_real_time_resumes_in_real_time(void)
{
	struct tsw_dock_model model;
	tsw_dock_model_init(&model);
	struct tsw_session session = new_session();
	struct tsw_dock_view view = build(&model, &session, paused(1));
	TSW_EXPECT_EQ(view.banner, TSW_BANNER_PAUSED);
	EXPECT_TEXT(view.tally_text, "State.Tally.Paused"); /* never REC: no recording look */
	EXPECT_TEXT(view.hero_text, "State.Hero.ResumesRealTime");
	TSW_EXPECT_TRUE(view.hero_quiet);
	EXPECT_TEXT(view.main_text, "Action.ResumeRealTime");
	TSW_EXPECT_EQ(view.main_speed, 1);
	TSW_EXPECT_TRUE(view.main_enabled);
	TSW_EXPECT_TRUE(!view.start_mode_shown); /* no second rail: the paused zone D is one rail */
	TSW_EXPECT_TRUE(!view.start_in_timelapse);
	TSW_EXPECT_TRUE(view.speeds_shown && view.speeds_enabled && view.speeds_real_time);
	EXPECT_TEXT(view.speed_label, "Label.ResumeIn");
	TSW_EXPECT_EQ(view.speed_selected, 1); /* the "Real time" segment */
	TSW_EXPECT_TRUE(view.pause_shown && view.pause_enabled);
	EXPECT_TEXT(view.pause_text, "Action.Resume"); /* the same button, in the same place (UX-128) */
	TSW_EXPECT_TRUE(view.stop_shown && view.stop_enabled);
	TSW_EXPECT_TRUE(!view.card_shown);
	EXPECT_TEXT(view.context_text, "Hint.Paused");
}

/* Paused from timelapse x8, it resumes in timelapse x8 unless the user chooses otherwise. */
static void paused_from_timelapse_resumes_in_timelapse(void)
{
	struct tsw_dock_model model;
	tsw_dock_model_init(&model);
	struct tsw_session session = new_session();
	session.timelapse_speed = 8; /* as recording at x8 leaves it */
	struct tsw_dock_view view = build(&model, &session, paused(8));
	TSW_EXPECT_EQ(view.banner, TSW_BANNER_PAUSED);
	EXPECT_TEXT(view.tally_text, "State.Tally.Paused");
	EXPECT_TEXT(view.hero_text, "State.Hero.ResumesTimelapse");
	TSW_EXPECT_EQ(view.hero_speed, 8);
	EXPECT_TEXT(view.main_text, "Action.ResumeTimelapse");
	TSW_EXPECT_EQ(view.main_speed, 8);
	TSW_EXPECT_TRUE(view.start_in_timelapse);
	TSW_EXPECT_TRUE(view.speeds_real_time && view.speeds_enabled && !view.start_mode_shown);
	EXPECT_TEXT(view.speed_label, "Label.ResumeIn");
	TSW_EXPECT_EQ(view.speed_selected, 8);
	EXPECT_TEXT(view.pause_text, "Action.Resume");
}

/* UX-131: what the user chooses while paused shows at once in the hero, the main button and the
 * rail (tsw_session_effective_speed), and none of it resumes by itself. The rail's "Real time" sends
 * TSW_INPUT_START_REAL_TIME and a speed segment TSW_INPUT_SPEED_KEY: resume in timelapse at it. */
static void choosing_while_paused_changes_how_it_resumes(void)
{
	struct tsw_dock_model model;
	tsw_dock_model_init(&model);
	struct tsw_session session = new_session();
	session.timelapse_speed = 8;
	const struct tsw_status status = paused(8);
	struct tsw_command command = tsw_session_input(&session, &status, TSW_INPUT_START_REAL_TIME, 0, 0);
	TSW_EXPECT_EQ(command.kind, TSW_COMMAND_NONE);
	struct tsw_dock_view view = build(&model, &session, status);
	EXPECT_TEXT(view.hero_text, "State.Hero.ResumesRealTime");
	EXPECT_TEXT(view.main_text, "Action.ResumeRealTime");
	TSW_EXPECT_TRUE(!view.start_in_timelapse);
	TSW_EXPECT_EQ(view.speed_selected, 1);

	/* A speed segment chooses timelapse at that speed, from real time too. */
	command = tsw_session_input(&session, &status, TSW_INPUT_SPEED_KEY, 16, 0);
	TSW_EXPECT_EQ(command.kind, TSW_COMMAND_NONE);
	view = build(&model, &session, status);
	EXPECT_TEXT(view.hero_text, "State.Hero.ResumesTimelapse");
	TSW_EXPECT_EQ(view.hero_speed, 16);
	EXPECT_TEXT(view.main_text, "Action.ResumeTimelapse");
	TSW_EXPECT_EQ(view.main_speed, 16);
	TSW_EXPECT_TRUE(view.start_in_timelapse);
	TSW_EXPECT_EQ(view.speed_selected, 16);

	command = tsw_session_input(&session, &status, TSW_INPUT_SPEED_KEY, 2, 0);
	TSW_EXPECT_EQ(command.kind, TSW_COMMAND_NONE);
	view = build(&model, &session, status);
	TSW_EXPECT_EQ(view.main_speed, 2);
	TSW_EXPECT_EQ(view.speed_selected, 2);

	/* The hotkeys choose on the same rail: Slower from x2 is real time. */
	command = tsw_session_input(&session, &status, TSW_INPUT_SLOWER, 0, 0);
	TSW_EXPECT_EQ(command.kind, TSW_COMMAND_NONE);
	view = build(&model, &session, status);
	EXPECT_TEXT(view.main_text, "Action.ResumeRealTime");
	TSW_EXPECT_TRUE(!view.start_in_timelapse);
	TSW_EXPECT_EQ(view.speed_selected, 1);
	TSW_EXPECT_EQ(view.banner, TSW_BANNER_PAUSED);
}

/* UX-132: while paused the counters stand still (they are not dimmed: they are what is in the file)
 * and the status line counts the pause, in full: "Paused for 0:01:12". */
static void paused_counts_the_pause_not_the_recording(void)
{
	struct tsw_dock_model model;
	tsw_dock_model_init(&model);
	struct tsw_session session = new_session();
	struct tsw_dock_view view = build(&model, &session, paused(4));
	TSW_EXPECT_TRUE(!view.counters_dimmed);
	EXPECT_TEXT(view.recorded, "0:12:34");
	EXPECT_TEXT(view.final_length, "0:06:40");
	TSW_EXPECT_TRUE(view.section_shown);
	EXPECT_TEXT(view.section_label, "Counter.Paused");
	TSW_EXPECT_EQ(view.section_speed, 1); /* no arrow: nothing is added to the final video */
	EXPECT_TEXT(view.section_recorded, "0:01:12");

	view = build(&model, &session, recording(4));
	EXPECT_TEXT(view.section_label, "Counter.Stretch");
	struct tsw_status idle = {0};
	view = build(&model, &session, idle);
	EXPECT_TEXT(view.section_label, "Counter.Stretch");
}

/* UX-104: a pause and a resume are announced once each; a choice made while paused is not a change of
 * state, and a stop while paused saves without a word, as any stop. */
static void a_pause_and_a_resume_are_announced_once(void)
{
	struct tsw_dock_model model;
	tsw_dock_model_init(&model);
	struct tsw_session session = new_session();
	struct tsw_dock_view view = build(&model, &session, recording(1));
	EXPECT_TEXT(view.announce_text, "A11y.Announce.Start");
	struct tsw_status status = paused(1);
	view = build(&model, &session, status);
	EXPECT_TEXT(view.announce_text, "A11y.Announce.Paused");
	view = build(&model, &session, status);
	TSW_EXPECT_TRUE(view.announce_text == NULL);
	const struct tsw_command command = tsw_session_input(&session, &status, TSW_INPUT_START_TIMELAPSE, 0, 0);
	TSW_EXPECT_EQ(command.kind, TSW_COMMAND_NONE);
	view = build(&model, &session, status);
	TSW_EXPECT_TRUE(view.announce_text == NULL);
	view = build(&model, &session, recording(4));
	EXPECT_TEXT(view.announce_text, "A11y.Announce.Resumed");
	view = build(&model, &session, recording(4));
	TSW_EXPECT_TRUE(view.announce_text == NULL);
	view = build(&model, &session, paused(4));
	EXPECT_TEXT(view.announce_text, "A11y.Announce.Paused");
	status = paused(4);
	status.phase = TSW_PHASE_SAVING;
	view = build(&model, &session, status);
	TSW_EXPECT_TRUE(view.announce_text == NULL);
}

static void a_new_recording_closes_the_card(void)
{
	struct tsw_dock_model model;
	tsw_dock_model_init(&model);
	struct tsw_session session = new_session();
	struct tsw_dock_view view = build(&model, &session, ended(1, 0));
	TSW_EXPECT_TRUE(view.card_shown);
	struct tsw_status status = recording(1);
	status.outcome = 1;
	view = build(&model, &session, status);
	TSW_EXPECT_TRUE(!view.card_shown);
	TSW_EXPECT_EQ(view.banner, TSW_BANNER_REAL_TIME);
	status = ended(1, 0); /* the same outcome again: nothing new to show */
	view = build(&model, &session, status);
	TSW_EXPECT_TRUE(!view.card_shown);
}

/* --- Monitoring: the details, the speed map and the health line (ux-spec 3.17) --- */

static const uint64_t MIB = 1048576;

static void expect_amount(const struct tsw_amount *amount, const char *unit, const char *number, const char *second,
			  int line)
{
	expect_text(amount->unit, unit, line);
	expect_text(amount->number, number, line);
	expect_text(amount->second, second, line);
}
#define EXPECT_AMOUNT(amount, unit, number, second) expect_amount(&(amount), unit, number, second, __LINE__)

/* A recording 0:31:40 long (0:16:58 of final video), with everything the details show. */
static struct tsw_status monitored(uint32_t speed)
{
	struct tsw_status status = recording(speed);
	status.recorded_ns = 1900 * SECOND_NS;
	status.final_ns = 1018 * SECOND_NS;
	status.totals.real_time_ns = 723 * SECOND_NS;  /* 12:03 */
	status.totals.timelapse_ns = 1177 * SECOND_NS; /* 19:37 */
	status.totals.paused_ns = 72 * SECOND_NS;      /* 1:12 */
	status.totals.switches = 3;
	status.totals.pauses = 1;
	for (size_t i = 0; i < TSW_MAP_BUCKETS; i++)
		status.map[i] = (uint8_t)(i < 16 ? 1 : (i < 40 ? 4 : 1));
	status.feed_waiting = 1;
	status.feed_size = 16;
	status.file_bytes = 1288490189;            /* 1.2 GB */
	status.disk_free_bytes = 100 * 1024 * MIB; /* 100 GB */
	status.bytes_per_second = 664064;          /* 38 MB per minute */
	return status;
}

/* Break caught: sizes in the wrong unit, a number of four digits, or a decimal where it is noise. */
static void sizes_read_in_megabytes_gigabytes_and_terabytes(void)
{
	struct tsw_amount amount;
	tsw_format_size(0, &amount);
	EXPECT_AMOUNT(amount, "Unit.MB", "0", "");
	tsw_format_size(307200, &amount); /* 0.29 MB */
	EXPECT_AMOUNT(amount, "Unit.MB", "0.3", "");
	tsw_format_size(9 * MIB + MIB / 2, &amount);
	EXPECT_AMOUNT(amount, "Unit.MB", "9.5", "");
	tsw_format_size(10 * MIB - MIB / 50, &amount); /* 9.98: one decimal would read 10.0 */
	EXPECT_AMOUNT(amount, "Unit.MB", "10", "");
	tsw_format_size(38 * MIB, &amount);
	EXPECT_AMOUNT(amount, "Unit.MB", "38", "");
	tsw_format_size(999 * MIB + MIB * 6 / 10, &amount); /* 999.6 MB would round to 1000 */
	EXPECT_AMOUNT(amount, "Unit.GB", "1.0", "");
	tsw_format_size(1288490189, &amount);
	EXPECT_AMOUNT(amount, "Unit.GB", "1.2", "");
	tsw_format_size(212 * 1024 * MIB, &amount);
	EXPECT_AMOUNT(amount, "Unit.GB", "212", "");
	tsw_format_size(1843 * 1024 * MIB, &amount); /* 1843 GB */
	EXPECT_AMOUNT(amount, "Unit.TB", "1.8", "");
	tsw_format_size(UINT64_MAX, &amount); /* 16 million TB: whole, and it still fits */
	EXPECT_AMOUNT(amount, "Unit.TB", "9999999", "");
}

/* Break caught: more time promised than the disk holds (rounded up), or hours written as minutes. */
static void time_left_reads_in_minutes_and_hours_rounded_down(void)
{
	struct tsw_amount amount;
	tsw_format_time_left(UINT64_MAX, &amount);
	TSW_EXPECT_TRUE(amount.unit == NULL);
	tsw_format_time_left(0, &amount);
	EXPECT_AMOUNT(amount, "Unit.UnderMinute", "", "");
	tsw_format_time_left(59, &amount);
	EXPECT_AMOUNT(amount, "Unit.UnderMinute", "", "");
	tsw_format_time_left(60, &amount);
	EXPECT_AMOUNT(amount, "Unit.Minutes", "1", "");
	tsw_format_time_left(599, &amount);
	EXPECT_AMOUNT(amount, "Unit.Minutes", "9", "");
	tsw_format_time_left(3599, &amount);
	EXPECT_AMOUNT(amount, "Unit.Minutes", "59", "");
	tsw_format_time_left(3600 + 59, &amount);
	EXPECT_AMOUNT(amount, "Unit.Hours", "1", "");
	tsw_format_time_left(3600 + 33 * 60 + 59, &amount);
	EXPECT_AMOUNT(amount, "Unit.HoursMinutes", "1", "33");
	tsw_format_time_left(10 * 3600 - 1, &amount);
	EXPECT_AMOUNT(amount, "Unit.HoursMinutes", "9", "59");
	tsw_format_time_left(161692, &amount); /* 44 h 54 min */
	EXPECT_AMOUNT(amount, "Unit.Hours", "44", "");
	tsw_format_time_left(UINT64_MAX - 1, &amount);
	EXPECT_AMOUNT(amount, "Unit.Hours", "9999999", "");
}

/* Break caught: the ratio upside down, a division by an empty final video, or noise in the decimals. */
static void compression_reads_as_times_shorter(void)
{
	struct tsw_amount amount;
	tsw_format_compression(1900 * SECOND_NS, 1018 * SECOND_NS, &amount); /* 1.87 */
	EXPECT_AMOUNT(amount, "Unit.Times", "1.9", "");
	tsw_format_compression(754 * SECOND_NS, 754 * SECOND_NS, &amount);
	EXPECT_AMOUNT(amount, "Unit.Times", "1.0", "");
	tsw_format_compression(1600 * SECOND_NS, 100 * SECOND_NS, &amount);
	EXPECT_AMOUNT(amount, "Unit.Times", "16", "");
	tsw_format_compression(996 * SECOND_NS, 100 * SECOND_NS, &amount); /* 9.96 */
	EXPECT_AMOUNT(amount, "Unit.Times", "10", "");
	tsw_format_compression(5 * SECOND_NS, 0, &amount);
	TSW_EXPECT_TRUE(amount.unit == NULL);
}

/* UX-135: everything the details show while recording, from the recorder's counters. */
static void details_show_where_the_time_went(void)
{
	struct tsw_dock_model model;
	tsw_dock_model_init(&model);
	struct tsw_session session = new_session();
	const struct tsw_status status = monitored(4);
	struct tsw_dock_view view = build(&model, &session, status);
	TSW_EXPECT_TRUE(view.details_shown);
	EXPECT_TEXT(view.waiting_skipped, "14:42");
	EXPECT_AMOUNT(view.compression, "Unit.Times", "1.9", "");
	EXPECT_TEXT(view.real_time, "12:03");
	EXPECT_TEXT(view.timelapse, "19:37");
	EXPECT_TEXT(view.paused, "1:12");
	TSW_EXPECT_EQ(view.switches, 3);
	TSW_EXPECT_EQ(view.pauses, 1);
	TSW_EXPECT_EQ(view.chapters, 4);
	EXPECT_AMOUNT(view.file_size, "Unit.GB", "1.2", "");
	EXPECT_AMOUNT(view.file_rate, "Unit.MB", "38", "");
	EXPECT_AMOUNT(view.disk_free, "Unit.GB", "100", "");
	EXPECT_AMOUNT(view.time_left, "Unit.Hours", "44", "");
	TSW_EXPECT_EQ(view.backlog, 1);
	TSW_EXPECT_EQ(view.backlog_size, 16);
	TSW_EXPECT_TRUE(view.map_shown);
	TSW_EXPECT_EQ(view.map[0], 1);
	TSW_EXPECT_EQ(view.map[20], 4);
	TSW_EXPECT_EQ(view.map[TSW_MAP_BUCKETS - 1], 1);

	struct tsw_status paused_now = status;
	paused_now.phase = TSW_PHASE_PAUSED;
	TSW_EXPECT_TRUE(build(&model, &session, paused_now).details_shown);
	struct tsw_status saving = status;
	saving.phase = TSW_PHASE_SAVING;
	TSW_EXPECT_TRUE(build(&model, &session, saving).details_shown);
}

/* What is not measured yet is not known, never a made-up zero. */
static void details_say_what_is_not_known_yet(void)
{
	struct tsw_dock_model model;
	tsw_dock_model_init(&model);
	struct tsw_session session = new_session();
	struct tsw_status status = monitored(1);
	status.file_bytes = 0;
	status.disk_free_bytes = 0;
	status.bytes_per_second = 0;
	status.final_ns = 0;
	struct tsw_dock_view view = build(&model, &session, status);
	TSW_EXPECT_TRUE(view.file_size.unit == NULL);
	TSW_EXPECT_TRUE(view.file_rate.unit == NULL);
	TSW_EXPECT_TRUE(view.disk_free.unit == NULL);
	TSW_EXPECT_TRUE(view.time_left.unit == NULL);
	TSW_EXPECT_TRUE(view.compression.unit == NULL);
	/* Right at the start the map has no slice yet (no section): it is not drawn as one long pause. */
	memset(status.map, 0, sizeof(status.map));
	view = build(&model, &session, status);
	TSW_EXPECT_TRUE(!view.map_shown);
}

/* Idle and after a recording there are no details to show. */
static void idle_shows_no_details_and_no_health(void)
{
	struct tsw_dock_model model;
	tsw_dock_model_init(&model);
	struct tsw_session session = new_session();
	struct tsw_status status = {0};
	status.frames_lost = 5; /* never shown idle: idle has no recording to be about */
	struct tsw_dock_view view = build(&model, &session, status);
	TSW_EXPECT_TRUE(!view.details_shown);
	TSW_EXPECT_TRUE(!view.map_shown);
	TSW_EXPECT_EQ(view.health, 0);
}

/* UX-134, break caught: a warning while all is well (noise mid-recording). */
static void health_is_silent_when_all_is_well(void)
{
	struct tsw_dock_model model;
	tsw_dock_model_init(&model);
	struct tsw_session session = new_session();
	struct tsw_dock_view view = build(&model, &session, monitored(4));
	TSW_EXPECT_EQ(view.health, 0);
	TSW_EXPECT_TRUE(view.health_text == NULL);
	EXPECT_TEXT(view.context_text, "Hint.AudioSilence");
}

/* UX-134: only the most urgent issue shows: disk low, then frames lost, then the encoder behind. */
static void health_shows_only_the_most_urgent_issue(void)
{
	struct tsw_dock_model model;
	tsw_dock_model_init(&model);
	struct tsw_session session = new_session();
	struct tsw_status status = monitored(4);
	status.frames_lost = 37;
	status.feed_waiting = 12;
	status.disk_free_bytes = 330 * MIB; /* 330 MB at 38 MB per minute: 8 minutes */
	struct tsw_dock_view view = build(&model, &session, status);
	TSW_EXPECT_EQ(view.health, TSW_HEALTH_DISK_LOW);
	TSW_EXPECT_EQ(view.health_code, 102);
	EXPECT_TEXT(view.health_text, "Health.E102");
	EXPECT_AMOUNT(view.health_amount, "Unit.Minutes", "8", "");

	tsw_dock_model_init(&model);
	status.disk_free_bytes = 100 * 1024 * MIB;
	view = build(&model, &session, status);
	TSW_EXPECT_EQ(view.health, TSW_HEALTH_LOST_FRAMES);
	TSW_EXPECT_EQ(view.health_code, 201);
	EXPECT_TEXT(view.health_text, "Health.E201");
	EXPECT_AMOUNT(view.health_amount, "Unit.Count", "37", "");

	tsw_dock_model_init(&model);
	status.frames_lost = 0;
	view = build(&model, &session, status);
	TSW_EXPECT_EQ(view.health, TSW_HEALTH_ENCODER_BEHIND);
	TSW_EXPECT_EQ(view.health_code, 203);
	EXPECT_TEXT(view.health_text, "Health.E203");
	TSW_EXPECT_TRUE(view.health_amount.unit == NULL);
}

/* UX-134: lost frames stay, with their count, until the recording ends; the next one starts clean. */
static void lost_frames_stay_until_the_recording_ends(void)
{
	struct tsw_dock_model model;
	tsw_dock_model_init(&model);
	struct tsw_session session = new_session();
	struct tsw_status status = monitored(1);
	status.frames_lost = 3;
	TSW_EXPECT_EQ(build(&model, &session, status).health, TSW_HEALTH_LOST_FRAMES);
	status.recorded_ns += 3600 * SECOND_NS;
	TSW_EXPECT_EQ(build(&model, &session, status).health, TSW_HEALTH_LOST_FRAMES);
	struct tsw_status paused_now = status;
	paused_now.phase = TSW_PHASE_PAUSED;
	TSW_EXPECT_EQ(build(&model, &session, paused_now).health, TSW_HEALTH_LOST_FRAMES);
	TSW_EXPECT_EQ(build(&model, &session, ended(1, 0)).health, 0);
	struct tsw_status next = monitored(1);
	next.recorded_ns = 5 * SECOND_NS;
	next.outcome = 1;
	TSW_EXPECT_EQ(build(&model, &session, next).health, 0);
}

/* UX-063 and ux-spec 4.1: an issue whose condition ends stays 30 s more, so a condition at its edge
 * never makes the line blink; a disk warning keeps the time left it last had. */
static void an_issue_that_ends_stays_thirty_seconds_more(void)
{
	struct tsw_dock_model model;
	tsw_dock_model_init(&model);
	struct tsw_session session = new_session();
	struct tsw_status status = monitored(4);
	status.feed_waiting = 9;
	TSW_EXPECT_EQ(build(&model, &session, status).health, TSW_HEALTH_ENCODER_BEHIND);
	status.feed_waiting = 1;
	status.recorded_ns += 29 * SECOND_NS;
	TSW_EXPECT_EQ(build(&model, &session, status).health, TSW_HEALTH_ENCODER_BEHIND);
	status.recorded_ns += 2 * SECOND_NS;
	TSW_EXPECT_EQ(build(&model, &session, status).health, 0);

	status.disk_free_bytes = 330 * MIB;
	TSW_EXPECT_EQ(build(&model, &session, status).health, TSW_HEALTH_DISK_LOW);
	status.phase = TSW_PHASE_PAUSED; /* paused: the file stops growing, the rate is not known */
	status.bytes_per_second = 0;
	const struct tsw_dock_view view = build(&model, &session, status);
	TSW_EXPECT_EQ(view.health, TSW_HEALTH_DISK_LOW);
	EXPECT_AMOUNT(view.health_amount, "Unit.Minutes", "8", "");
}

/* UX-011 comes first while saving: the line says to keep OBS open, whatever the health was. */
static void saving_says_keep_obs_open_whatever_the_health(void)
{
	struct tsw_dock_model model;
	tsw_dock_model_init(&model);
	struct tsw_session session = new_session();
	struct tsw_status status = monitored(4);
	status.frames_lost = 2;
	status.phase = TSW_PHASE_SAVING;
	const struct tsw_dock_view view = build(&model, &session, status);
	TSW_EXPECT_EQ(view.health, 0);
	EXPECT_TEXT(view.context_text, "Hint.Saving");
}

/* UX-104: a new issue is announced once, politely; its count going up is not news. */
static void a_new_issue_is_announced_once(void)
{
	struct tsw_dock_model model;
	tsw_dock_model_init(&model);
	struct tsw_session session = new_session();
	struct tsw_status status = monitored(1);
	struct tsw_dock_view view = build(&model, &session, status);
	EXPECT_TEXT(view.announce_text, "A11y.Announce.Start");
	status.frames_lost = 1;
	view = build(&model, &session, status);
	EXPECT_TEXT(view.announce_text, "A11y.Announce.Health");
	status.frames_lost = 2;
	view = build(&model, &session, status);
	TSW_EXPECT_TRUE(view.announce_text == NULL);
	status.disk_free_bytes = 330 * MIB;
	view = build(&model, &session, status);
	EXPECT_TEXT(view.announce_text, "A11y.Announce.Health");
}

/* UX-135 and 3.13: after a recording the summary card shows its speed map; an error card does not, and
 * a map is never shown twice (an old one with a later outcome). */
static void the_summary_shows_the_map_of_the_recording(void)
{
	struct tsw_dock_model model;
	tsw_dock_model_init(&model);
	struct tsw_session session = new_session();
	struct tsw_status status = monitored(4);
	(void)build(&model, &session, status);
	status.phase = TSW_PHASE_SAVING;
	(void)build(&model, &session, status);
	struct tsw_dock_view view = build(&model, &session, ended(1, 0));
	TSW_EXPECT_TRUE(view.card_shown && view.map_shown);
	TSW_EXPECT_EQ(view.map[20], 4);
	TSW_EXPECT_TRUE(!view.details_shown);
	view = build(&model, &session, ended(1, 0));
	TSW_EXPECT_TRUE(view.map_shown); /* the card keeps it while it is open */
	tsw_dock_model_dismiss(&model);
	TSW_EXPECT_TRUE(!build(&model, &session, ended(1, 0)).map_shown);
	TSW_EXPECT_TRUE(!build(&model, &session, ended(2, 0)).map_shown);

	tsw_dock_model_init(&model);
	status.phase = TSW_PHASE_RECORDING;
	(void)build(&model, &session, status);
	view = build(&model, &session, ended(1, 103));
	TSW_EXPECT_TRUE(view.card_shown && !view.map_shown);
}

int main(void)
{
	durations_read_as_hours_minutes_seconds();
	short_durations_drop_the_hours_under_an_hour();
	idle_offers_record_and_the_starting_mode();
	idle_shows_the_stretch_at_zero();
	real_time_offers_the_switch();
	real_time_shows_the_current_stretch();
	timelapse_offers_the_way_back_and_shows_the_section();
	saving_disables_what_cannot_act();
	saving_keeps_the_last_stretch_frozen();
	a_saved_recording_opens_the_summary();
	a_recording_that_never_sped_up_skips_nothing();
	a_failure_opens_the_error_card();
	a_failed_start_shows_no_times();
	a_failed_start_shows_the_stretch_at_zero();
	a_saved_take_shows_no_stretch();
	an_unknown_error_uses_the_unexpected_error_texts();
	changes_of_state_are_announced_once();
	a_new_recording_closes_the_card();
	recording_offers_pause_next_to_stop();
	paused_from_real_time_resumes_in_real_time();
	paused_from_timelapse_resumes_in_timelapse();
	choosing_while_paused_changes_how_it_resumes();
	paused_counts_the_pause_not_the_recording();
	a_pause_and_a_resume_are_announced_once();
	sizes_read_in_megabytes_gigabytes_and_terabytes();
	time_left_reads_in_minutes_and_hours_rounded_down();
	compression_reads_as_times_shorter();
	details_show_where_the_time_went();
	details_say_what_is_not_known_yet();
	idle_shows_no_details_and_no_health();
	health_is_silent_when_all_is_well();
	health_shows_only_the_most_urgent_issue();
	lost_frames_stay_until_the_recording_ends();
	an_issue_that_ends_stays_thirty_seconds_more();
	saving_says_keep_obs_open_whatever_the_health();
	a_new_issue_is_announced_once();
	the_summary_shows_the_map_of_the_recording();
	return tsw_test_failures;
}
