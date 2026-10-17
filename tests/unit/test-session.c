/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * test-session.c: what each button and hotkey does in each state (core/session.h).
 */

#include "core/session.h"
#include "test.h"

static const uint64_t MS = 1000000;
static const uint64_t DEBOUNCE_NS = 400 * 1000000;

static struct tsw_status idle(void)
{
	struct tsw_status status = {0};
	status.phase = TSW_PHASE_IDLE;
	return status;
}

static struct tsw_status recording(uint32_t speed)
{
	struct tsw_status status = {0};
	status.phase = TSW_PHASE_RECORDING;
	status.speed = speed;
	return status;
}

/* Paused; speed is the one in use before the pause. */
static struct tsw_status paused(uint32_t speed)
{
	struct tsw_status status = {0};
	status.phase = TSW_PHASE_PAUSED;
	status.speed = speed;
	return status;
}

static struct tsw_status saving(void)
{
	struct tsw_status status = {0};
	status.phase = TSW_PHASE_SAVING;
	status.speed = 1;
	return status;
}

static struct tsw_session new_session(void)
{
	struct tsw_session session;
	TSW_EXPECT_EQ(tsw_session_init(&session, 4, DEBOUNCE_NS), TSW_OK);
	return session;
}

/* The command expected from one input; time is far from any earlier switch unless given. */
static void expect(struct tsw_session *session, struct tsw_status status, enum tsw_input input, uint32_t speed,
		   uint64_t now_ns, enum tsw_command_kind kind, uint32_t command_speed, int line)
{
	const struct tsw_command command = tsw_session_input(session, &status, input, speed, now_ns);
	if (command.kind != kind ||
	    (kind != TSW_COMMAND_NONE && kind != TSW_COMMAND_STOP && command.speed != command_speed)) {
		printf("%s:%d: expected command %d at x%u, got %d at x%u\n", __FILE__, line, (int)kind, command_speed,
		       (int)command.kind, command.speed);
		tsw_test_failures++;
	}
}
#define EXPECT_COMMAND(session, status, input, speed, now, kind, command_speed) \
	expect(session, status, input, speed, now, kind, command_speed, __LINE__)

static void record_starts_in_real_time_by_default(void)
{
	struct tsw_session session = new_session();
	EXPECT_COMMAND(&session, idle(), TSW_INPUT_MAIN, 0, 0, TSW_COMMAND_START, 1);
	EXPECT_COMMAND(&session, idle(), TSW_INPUT_RECORD, 0, 0, TSW_COMMAND_START, 1);
}

static void record_starts_in_timelapse_when_chosen(void)
{
	struct tsw_session session = new_session();
	EXPECT_COMMAND(&session, idle(), TSW_INPUT_START_TIMELAPSE, 0, 0, TSW_COMMAND_NONE, 0);
	EXPECT_COMMAND(&session, idle(), TSW_INPUT_MAIN, 0, 0, TSW_COMMAND_START, 4);
	EXPECT_COMMAND(&session, idle(), TSW_INPUT_START_REAL_TIME, 0, 0, TSW_COMMAND_NONE, 0);
	EXPECT_COMMAND(&session, idle(), TSW_INPUT_MAIN, 0, 0, TSW_COMMAND_START, 1);
}

static void record_hotkey_does_nothing_while_recording(void)
{
	struct tsw_session session = new_session();
	EXPECT_COMMAND(&session, recording(1), TSW_INPUT_RECORD, 0, 0, TSW_COMMAND_NONE, 0);
	EXPECT_COMMAND(&session, recording(4), TSW_INPUT_RECORD, 0, 0, TSW_COMMAND_NONE, 0);
}

static void main_button_switches_while_recording(void)
{
	struct tsw_session session = new_session();
	EXPECT_COMMAND(&session, recording(1), TSW_INPUT_MAIN, 0, 0, TSW_COMMAND_SET_SPEED, 4);
	EXPECT_COMMAND(&session, recording(4), TSW_INPUT_MAIN, 0, 1000 * MS, TSW_COMMAND_SET_SPEED, 1);
}

static void chip_in_real_time_only_chooses_the_next_speed(void)
{
	struct tsw_session session = new_session();
	EXPECT_COMMAND(&session, recording(1), TSW_INPUT_SPEED_CHIP, 8, 0, TSW_COMMAND_NONE, 0);
	EXPECT_COMMAND(&session, recording(1), TSW_INPUT_MAIN, 0, 0, TSW_COMMAND_SET_SPEED, 8);
}

static void chip_in_timelapse_changes_the_speed_now(void)
{
	struct tsw_session session = new_session();
	EXPECT_COMMAND(&session, recording(4), TSW_INPUT_SPEED_CHIP, 8, 0, TSW_COMMAND_SET_SPEED, 8);
	EXPECT_COMMAND(&session, recording(4), TSW_INPUT_SPEED_CHIP, 4, 0, TSW_COMMAND_NONE, 0); /* already x4 */
}

static void speed_key_goes_to_timelapse_at_that_speed(void)
{
	struct tsw_session session = new_session();
	EXPECT_COMMAND(&session, recording(1), TSW_INPUT_SPEED_KEY, 8, 0, TSW_COMMAND_SET_SPEED, 8);
	EXPECT_COMMAND(&session, recording(8), TSW_INPUT_SPEED_KEY, 2, 0, TSW_COMMAND_SET_SPEED, 2);
	EXPECT_COMMAND(&session, recording(1), TSW_INPUT_MAIN, 0, 1000 * MS, TSW_COMMAND_SET_SPEED, 2);
}

/* UX-024: while idle, switch and speed keys never silently do nothing: they set the starting mode. */
static void keys_while_idle_choose_how_to_start(void)
{
	struct tsw_session session = new_session();
	EXPECT_COMMAND(&session, idle(), TSW_INPUT_SPEED_KEY, 8, 0, TSW_COMMAND_NONE, 0);
	TSW_EXPECT_TRUE(session.start_in_timelapse);
	EXPECT_COMMAND(&session, idle(), TSW_INPUT_MAIN, 0, 0, TSW_COMMAND_START, 8);

	session = new_session();
	EXPECT_COMMAND(&session, idle(), TSW_INPUT_TIMELAPSE, 0, 0, TSW_COMMAND_NONE, 0);
	TSW_EXPECT_TRUE(session.start_in_timelapse);
	EXPECT_COMMAND(&session, idle(), TSW_INPUT_REAL_TIME, 0, 0, TSW_COMMAND_NONE, 0);
	TSW_EXPECT_TRUE(!session.start_in_timelapse);
}

static void switch_hotkeys_only_act_in_the_other_mode(void)
{
	struct tsw_session session = new_session();
	EXPECT_COMMAND(&session, recording(4), TSW_INPUT_TIMELAPSE, 0, 0, TSW_COMMAND_NONE, 0);
	EXPECT_COMMAND(&session, recording(1), TSW_INPUT_REAL_TIME, 0, 0, TSW_COMMAND_NONE, 0);
	EXPECT_COMMAND(&session, recording(1), TSW_INPUT_TIMELAPSE, 0, 0, TSW_COMMAND_SET_SPEED, 4);
	EXPECT_COMMAND(&session, recording(4), TSW_INPUT_REAL_TIME, 0, 1000 * MS, TSW_COMMAND_SET_SPEED, 1);
}

static void faster_and_slower_step_through_the_speeds(void)
{
	struct tsw_session session = new_session();
	EXPECT_COMMAND(&session, recording(1), TSW_INPUT_FASTER, 0, 0, TSW_COMMAND_SET_SPEED, 2);
	EXPECT_COMMAND(&session, recording(2), TSW_INPUT_FASTER, 0, 0, TSW_COMMAND_SET_SPEED, 4);
	EXPECT_COMMAND(&session, recording(8), TSW_INPUT_FASTER, 0, 0, TSW_COMMAND_SET_SPEED, 16);
	EXPECT_COMMAND(&session, recording(16), TSW_INPUT_FASTER, 0, 0, TSW_COMMAND_NONE, 0);
	EXPECT_COMMAND(&session, recording(16), TSW_INPUT_SLOWER, 0, 0, TSW_COMMAND_SET_SPEED, 8);
	TSW_EXPECT_EQ(session.timelapse_speed, 8);
	EXPECT_COMMAND(&session, recording(2), TSW_INPUT_SLOWER, 0, 1000 * MS, TSW_COMMAND_SET_SPEED, 1);
	TSW_EXPECT_EQ(session.timelapse_speed, 8); /* real time keeps the last chosen timelapse speed */
	EXPECT_COMMAND(&session, recording(1), TSW_INPUT_SLOWER, 0, 2000 * MS, TSW_COMMAND_NONE, 0);
}

static void faster_and_slower_while_idle_change_the_start(void)
{
	struct tsw_session session = new_session();
	EXPECT_COMMAND(&session, idle(), TSW_INPUT_FASTER, 0, 0, TSW_COMMAND_NONE, 0);
	TSW_EXPECT_TRUE(session.start_in_timelapse);
	TSW_EXPECT_EQ(session.timelapse_speed, 2);
	EXPECT_COMMAND(&session, idle(), TSW_INPUT_FASTER, 0, 0, TSW_COMMAND_NONE, 0);
	TSW_EXPECT_EQ(session.timelapse_speed, 4);
	EXPECT_COMMAND(&session, idle(), TSW_INPUT_SLOWER, 0, 0, TSW_COMMAND_NONE, 0);
	EXPECT_COMMAND(&session, idle(), TSW_INPUT_SLOWER, 0, 0, TSW_COMMAND_NONE, 0);
	TSW_EXPECT_TRUE(!session.start_in_timelapse);
	EXPECT_COMMAND(&session, idle(), TSW_INPUT_MAIN, 0, 0, TSW_COMMAND_START, 1);
}

static void stop_only_while_recording(void)
{
	struct tsw_session session = new_session();
	EXPECT_COMMAND(&session, recording(1), TSW_INPUT_STOP, 0, 0, TSW_COMMAND_STOP, 0);
	EXPECT_COMMAND(&session, recording(8), TSW_INPUT_STOP, 0, 0, TSW_COMMAND_STOP, 0);
	EXPECT_COMMAND(&session, idle(), TSW_INPUT_STOP, 0, 0, TSW_COMMAND_NONE, 0);
	EXPECT_COMMAND(&session, saving(), TSW_INPUT_STOP, 0, 0, TSW_COMMAND_NONE, 0);
}

static void nothing_happens_while_saving(void)
{
	struct tsw_session session = new_session();
	EXPECT_COMMAND(&session, saving(), TSW_INPUT_MAIN, 0, 0, TSW_COMMAND_NONE, 0);
	EXPECT_COMMAND(&session, saving(), TSW_INPUT_RECORD, 0, 0, TSW_COMMAND_NONE, 0);
	EXPECT_COMMAND(&session, saving(), TSW_INPUT_SPEED_KEY, 8, 0, TSW_COMMAND_NONE, 0);
	EXPECT_COMMAND(&session, saving(), TSW_INPUT_FASTER, 0, 0, TSW_COMMAND_NONE, 0);
	TSW_EXPECT_EQ(session.timelapse_speed, 4);
}

/* UX-046: a second switch between real time and timelapse within 400 ms is a bounce, not a wish. */
static void a_quick_second_switch_is_ignored(void)
{
	struct tsw_session session = new_session();
	const uint64_t t = 10000 * MS;
	EXPECT_COMMAND(&session, recording(1), TSW_INPUT_MAIN, 0, t, TSW_COMMAND_SET_SPEED, 4);
	EXPECT_COMMAND(&session, recording(4), TSW_INPUT_MAIN, 0, t + 300 * MS, TSW_COMMAND_NONE, 0);
	EXPECT_COMMAND(&session, recording(4), TSW_INPUT_REAL_TIME, 0, t + 399 * MS, TSW_COMMAND_NONE, 0);
	EXPECT_COMMAND(&session, recording(4), TSW_INPUT_MAIN, 0, t + 400 * MS, TSW_COMMAND_SET_SPEED, 1);
}

static void speed_changes_inside_timelapse_are_never_ignored(void)
{
	struct tsw_session session = new_session();
	EXPECT_COMMAND(&session, recording(1), TSW_INPUT_MAIN, 0, 0, TSW_COMMAND_SET_SPEED, 4);
	EXPECT_COMMAND(&session, recording(4), TSW_INPUT_SPEED_KEY, 8, 100 * MS, TSW_COMMAND_SET_SPEED, 8);
	EXPECT_COMMAND(&session, recording(8), TSW_INPUT_FASTER, 0, 150 * MS, TSW_COMMAND_SET_SPEED, 16);
}

static void invalid_speeds_are_refused(void)
{
	struct tsw_session session;
	TSW_EXPECT_EQ(tsw_session_init(&session, 1, DEBOUNCE_NS), TSW_E_INVALID);
	TSW_EXPECT_EQ(tsw_session_init(&session, 17, DEBOUNCE_NS), TSW_E_INVALID);
	session = new_session();
	const int checks_before = tsw_check_failures;
	EXPECT_COMMAND(&session, recording(4), TSW_INPUT_SPEED_CHIP, 1, 0, TSW_COMMAND_NONE, 0);
	EXPECT_COMMAND(&session, recording(1), TSW_INPUT_SPEED_KEY, 17, 0, TSW_COMMAND_NONE, 0);
	TSW_EXPECT_EQ(session.timelapse_speed, 4);
	TSW_EXPECT_EQ(tsw_check_failures - checks_before, 2);
}

/* Break caught: the Pause button or hotkey not pausing a recording. */
static void pause_pauses_a_recording(void)
{
	struct tsw_session session = new_session();
	EXPECT_COMMAND(&session, recording(1), TSW_INPUT_PAUSE, 0, 0, TSW_COMMAND_PAUSE, 0);
	session = new_session();
	EXPECT_COMMAND(&session, recording(8), TSW_INPUT_PAUSE, 0, 0, TSW_COMMAND_PAUSE, 0);
}

/* Break caught: resuming at another speed than the one before the pause. */
static void resume_returns_to_the_speed_before_the_pause(void)
{
	struct tsw_session session = new_session();
	EXPECT_COMMAND(&session, paused(4), TSW_INPUT_MAIN, 0, 0, TSW_COMMAND_RESUME, 4);
	EXPECT_COMMAND(&session, paused(1), TSW_INPUT_RECORD, 0, 1000 * MS, TSW_COMMAND_RESUME, 1);
	EXPECT_COMMAND(&session, paused(8), TSW_INPUT_PAUSE, 0, 2000 * MS, TSW_COMMAND_RESUME, 8);
}

/* Break caught: Stop being ignored while paused. */
static void stop_works_while_paused(void)
{
	struct tsw_session session = new_session();
	EXPECT_COMMAND(&session, paused(1), TSW_INPUT_STOP, 0, 0, TSW_COMMAND_STOP, 0);
}

/* Break caught: a mode or speed key resuming a pause by surprise, or its choice being lost. */
static void keys_while_paused_choose_how_to_resume(void)
{
	struct tsw_session session = new_session();
	EXPECT_COMMAND(&session, paused(1), TSW_INPUT_TIMELAPSE, 0, 0, TSW_COMMAND_NONE, 0);
	EXPECT_COMMAND(&session, paused(1), TSW_INPUT_MAIN, 0, 0, TSW_COMMAND_RESUME, 4);

	session = new_session();
	EXPECT_COMMAND(&session, paused(4), TSW_INPUT_REAL_TIME, 0, 0, TSW_COMMAND_NONE, 0);
	EXPECT_COMMAND(&session, paused(4), TSW_INPUT_MAIN, 0, 0, TSW_COMMAND_RESUME, 1);

	session = new_session();
	EXPECT_COMMAND(&session, paused(1), TSW_INPUT_SPEED_KEY, 8, 0, TSW_COMMAND_NONE, 0);
	EXPECT_COMMAND(&session, paused(1), TSW_INPUT_MAIN, 0, 0, TSW_COMMAND_RESUME, 8);

	session = new_session();
	EXPECT_COMMAND(&session, paused(4), TSW_INPUT_FASTER, 0, 0, TSW_COMMAND_NONE, 0);
	EXPECT_COMMAND(&session, paused(4), TSW_INPUT_MAIN, 0, 0, TSW_COMMAND_RESUME, 8);

	session = new_session();
	EXPECT_COMMAND(&session, paused(2), TSW_INPUT_SLOWER, 0, 0, TSW_COMMAND_NONE, 0);
	EXPECT_COMMAND(&session, paused(2), TSW_INPUT_MAIN, 0, 0, TSW_COMMAND_RESUME, 1);
}

/* Break caught: the speed chip resuming in timelapse when the pause came from real time. */
static void chip_while_paused_follows_the_mode_to_resume_in(void)
{
	struct tsw_session session = new_session();
	EXPECT_COMMAND(&session, paused(1), TSW_INPUT_SPEED_CHIP, 8, 0, TSW_COMMAND_NONE, 0);
	EXPECT_COMMAND(&session, paused(1), TSW_INPUT_MAIN, 0, 0, TSW_COMMAND_RESUME, 1);

	session = new_session();
	EXPECT_COMMAND(&session, paused(4), TSW_INPUT_SPEED_CHIP, 8, 0, TSW_COMMAND_NONE, 0);
	EXPECT_COMMAND(&session, paused(4), TSW_INPUT_MAIN, 0, 0, TSW_COMMAND_RESUME, 8);
}

/* Break caught: a choice made in one pause carrying over to the next pause. */
static void the_choice_to_resume_ends_with_the_pause(void)
{
	struct tsw_session session = new_session();
	EXPECT_COMMAND(&session, paused(1), TSW_INPUT_TIMELAPSE, 0, 0, TSW_COMMAND_NONE, 0);
	EXPECT_COMMAND(&session, paused(1), TSW_INPUT_MAIN, 0, 0, TSW_COMMAND_RESUME, 4);
	EXPECT_COMMAND(&session, recording(4), TSW_INPUT_PAUSE, 0, 1000 * MS, TSW_COMMAND_PAUSE, 0);
	EXPECT_COMMAND(&session, paused(4), TSW_INPUT_MAIN, 0, 2000 * MS, TSW_COMMAND_RESUME, 4);
}

/* Break caught: a double press of Pause pausing and resuming at once (UX-046). */
static void a_quick_second_pause_press_is_ignored(void)
{
	struct tsw_session session = new_session();
	EXPECT_COMMAND(&session, recording(1), TSW_INPUT_PAUSE, 0, 0, TSW_COMMAND_PAUSE, 0);
	EXPECT_COMMAND(&session, paused(1), TSW_INPUT_PAUSE, 0, 100 * MS, TSW_COMMAND_NONE, 0);
	EXPECT_COMMAND(&session, paused(1), TSW_INPUT_PAUSE, 0, 1000 * MS, TSW_COMMAND_RESUME, 1);
}

/* Break caught: a pause right after a speed switch taken for a bounce of that switch. */
static void a_pause_right_after_a_switch_is_not_a_bounce(void)
{
	struct tsw_session session = new_session();
	EXPECT_COMMAND(&session, recording(1), TSW_INPUT_MAIN, 0, 0, TSW_COMMAND_SET_SPEED, 4);
	EXPECT_COMMAND(&session, recording(4), TSW_INPUT_PAUSE, 0, 100 * MS, TSW_COMMAND_PAUSE, 0);
}

/* Break caught: Pause acting when there is nothing to pause. */
static void pause_does_nothing_while_idle_or_saving(void)
{
	struct tsw_session session = new_session();
	EXPECT_COMMAND(&session, idle(), TSW_INPUT_PAUSE, 0, 0, TSW_COMMAND_NONE, 0);
	EXPECT_COMMAND(&session, saving(), TSW_INPUT_PAUSE, 0, 0, TSW_COMMAND_NONE, 0);
	EXPECT_COMMAND(&session, idle(), TSW_INPUT_MAIN, 0, 0, TSW_COMMAND_START, 1); /* idle choices untouched */
}

/* Break caught: the mode keys reading the speed before a pause, or the default start, so that once a
 * different mode is chosen the other key can no longer choose back (the timelapse and real-time keys
 * act only when the effective mode is the other one). */
static void effective_speed_is_what_the_next_frames_will_use(void)
{
	struct tsw_session session = new_session();
	TSW_EXPECT_EQ(tsw_session_effective_speed(&session, &(struct tsw_status){0}), 1);
	EXPECT_COMMAND(&session, idle(), TSW_INPUT_START_TIMELAPSE, 0, 0, TSW_COMMAND_NONE, 0);
	const struct tsw_status idle_status = idle();
	TSW_EXPECT_EQ(tsw_session_effective_speed(&session, &idle_status), 4);

	const struct tsw_status recording_x8 = recording(8);
	TSW_EXPECT_EQ(tsw_session_effective_speed(&session, &recording_x8), 8);

	session = new_session();
	const struct tsw_status paused_x1 = paused(1);
	TSW_EXPECT_EQ(tsw_session_effective_speed(&session, &paused_x1), 1);
	EXPECT_COMMAND(&session, paused(1), TSW_INPUT_TIMELAPSE, 0, 0, TSW_COMMAND_NONE, 0);
	TSW_EXPECT_EQ(tsw_session_effective_speed(&session, &paused_x1), 4);
	EXPECT_COMMAND(&session, paused(1), TSW_INPUT_REAL_TIME, 0, 0, TSW_COMMAND_NONE, 0);
	TSW_EXPECT_EQ(tsw_session_effective_speed(&session, &paused_x1), 1);
}

int main(void)
{
	record_starts_in_real_time_by_default();
	record_starts_in_timelapse_when_chosen();
	record_hotkey_does_nothing_while_recording();
	main_button_switches_while_recording();
	chip_in_real_time_only_chooses_the_next_speed();
	chip_in_timelapse_changes_the_speed_now();
	speed_key_goes_to_timelapse_at_that_speed();
	keys_while_idle_choose_how_to_start();
	switch_hotkeys_only_act_in_the_other_mode();
	faster_and_slower_step_through_the_speeds();
	faster_and_slower_while_idle_change_the_start();
	stop_only_while_recording();
	nothing_happens_while_saving();
	a_quick_second_switch_is_ignored();
	speed_changes_inside_timelapse_are_never_ignored();
	invalid_speeds_are_refused();
	pause_pauses_a_recording();
	resume_returns_to_the_speed_before_the_pause();
	stop_works_while_paused();
	keys_while_paused_choose_how_to_resume();
	chip_while_paused_follows_the_mode_to_resume_in();
	the_choice_to_resume_ends_with_the_pause();
	a_quick_second_pause_press_is_ignored();
	a_pause_right_after_a_switch_is_not_a_bounce();
	effective_speed_is_what_the_next_frames_will_use();
	pause_does_nothing_while_idle_or_saving();
	return tsw_test_failures;
}
