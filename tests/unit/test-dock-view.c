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
	EXPECT_TEXT(view.start_label, "Label.StartIn");
	view = build(&fresh, &session, ended(1, 0));
	TSW_EXPECT_TRUE(!view.pause_shown && !view.stop_shown);
}

/* UX-129 to UX-131: paused from real time, every zone says it will resume in real time. */
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
	TSW_EXPECT_TRUE(view.start_mode_shown);
	EXPECT_TEXT(view.start_label, "Label.ResumeIn");
	TSW_EXPECT_TRUE(!view.start_in_timelapse);
	TSW_EXPECT_TRUE(view.speeds_shown && view.speeds_enabled);
	EXPECT_TEXT(view.speed_label, "Label.Speed");
	TSW_EXPECT_EQ(view.speed_selected, 4); /* the timelapse speed it would use */
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
	TSW_EXPECT_EQ(view.speed_selected, 8);
	TSW_EXPECT_TRUE(view.speeds_enabled);
	EXPECT_TEXT(view.pause_text, "Action.Resume");
}

/* UX-131: what the user chooses while paused shows at once in the hero, the main button and the
 * rails (tsw_session_effective_speed), and none of it resumes by itself. */
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
	TSW_EXPECT_EQ(view.speed_selected, 8); /* the speed chips keep the timelapse speed */

	/* A chip while resuming in real time only chooses the speed of the next timelapse. */
	command = tsw_session_input(&session, &status, TSW_INPUT_SPEED_CHIP, 16, 0);
	TSW_EXPECT_EQ(command.kind, TSW_COMMAND_NONE);
	view = build(&model, &session, status);
	EXPECT_TEXT(view.main_text, "Action.ResumeRealTime");
	TSW_EXPECT_EQ(view.speed_selected, 16);

	command = tsw_session_input(&session, &status, TSW_INPUT_START_TIMELAPSE, 0, 0);
	TSW_EXPECT_EQ(command.kind, TSW_COMMAND_NONE);
	view = build(&model, &session, status);
	EXPECT_TEXT(view.hero_text, "State.Hero.ResumesTimelapse");
	TSW_EXPECT_EQ(view.hero_speed, 16);
	EXPECT_TEXT(view.main_text, "Action.ResumeTimelapse");
	TSW_EXPECT_EQ(view.main_speed, 16);
	TSW_EXPECT_TRUE(view.start_in_timelapse);
	TSW_EXPECT_EQ(view.speed_selected, 16);

	/* While resuming in timelapse a chip changes the speed it resumes at; Slower can reach real time. */
	command = tsw_session_input(&session, &status, TSW_INPUT_SPEED_CHIP, 2, 0);
	TSW_EXPECT_EQ(command.kind, TSW_COMMAND_NONE);
	view = build(&model, &session, status);
	TSW_EXPECT_EQ(view.main_speed, 2);
	TSW_EXPECT_EQ(view.speed_selected, 2);
	command = tsw_session_input(&session, &status, TSW_INPUT_SLOWER, 0, 0);
	TSW_EXPECT_EQ(command.kind, TSW_COMMAND_NONE);
	view = build(&model, &session, status);
	EXPECT_TEXT(view.main_text, "Action.ResumeRealTime");
	TSW_EXPECT_TRUE(!view.start_in_timelapse);
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
	return tsw_test_failures;
}
