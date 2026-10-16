/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * session.c: see session.h.
 */

#include "core/session.h"

#include <stddef.h>

#include "core/check.h"
#include "core/timeline.h"

static const struct tsw_command NOTHING = {TSW_COMMAND_NONE, 0};

static bool is_timelapse_speed(uint32_t speed)
{
	return speed >= 2 && speed <= TSW_MAX_SPEED;
}

/* One step faster or slower: real time (1), x2, x4, x8, x16. */
static uint32_t faster(uint32_t speed)
{
	const uint32_t doubled = speed < 2 ? 2 : speed * 2;
	return doubled > TSW_MAX_SPEED ? TSW_MAX_SPEED : doubled;
}

static uint32_t slower(uint32_t speed)
{
	return speed <= 2 ? 1 : speed / 2;
}

enum tsw_result tsw_session_init(struct tsw_session *session, uint32_t timelapse_speed, uint64_t debounce_ns)
{
	if (!TSW_CHECK(session != NULL && is_timelapse_speed(timelapse_speed)))
		return TSW_E_INVALID;
	session->timelapse_speed = timelapse_speed;
	session->start_in_timelapse = false;
	session->debounce_ns = debounce_ns;
	session->last_switch_ns = 0;
	session->switched = false;
	return TSW_OK;
}

/* While idle: the speed the next recording will start at after this input (1 = real time). */
static uint32_t planned_start(const struct tsw_session *session, enum tsw_input input, uint32_t speed,
			      uint32_t start_speed)
{
	switch (input) {
	case TSW_INPUT_TIMELAPSE:
	case TSW_INPUT_START_TIMELAPSE:
		return session->timelapse_speed;
	case TSW_INPUT_REAL_TIME:
	case TSW_INPUT_START_REAL_TIME:
		return 1;
	case TSW_INPUT_SPEED_KEY:
		return speed;
	case TSW_INPUT_FASTER:
		return faster(start_speed);
	case TSW_INPUT_SLOWER:
		return slower(start_speed);
	case TSW_INPUT_MAIN:
	case TSW_INPUT_RECORD:
	case TSW_INPUT_STOP:
	case TSW_INPUT_SPEED_CHIP:
		break;
	}
	return start_speed;
}

/* While idle, inputs only choose how the next recording starts; they never silently do nothing (UX-024). */
static struct tsw_command input_while_idle(struct tsw_session *session, enum tsw_input input, uint32_t speed)
{
	const uint32_t start_speed = session->start_in_timelapse ? session->timelapse_speed : 1;
	if (input == TSW_INPUT_MAIN || input == TSW_INPUT_RECORD) {
		const struct tsw_command start = {TSW_COMMAND_START, start_speed};
		return start;
	}
	if (input == TSW_INPUT_SPEED_CHIP) {
		session->timelapse_speed = speed; /* the chip chooses the speed; the start mode is its own control */
		return NOTHING;
	}
	const uint32_t planned = planned_start(session, input, speed, start_speed);
	session->start_in_timelapse = planned > 1;
	if (planned > 1)
		session->timelapse_speed = planned;
	return NOTHING;
}

/* While recording at speed current: the speed the input asks for (current when it asks for nothing). */
static uint32_t wanted_speed(struct tsw_session *session, uint32_t current, enum tsw_input input, uint32_t speed)
{
	switch (input) {
	case TSW_INPUT_MAIN:
		return current > 1 ? 1 : session->timelapse_speed;
	case TSW_INPUT_TIMELAPSE:
		return current > 1 ? current : session->timelapse_speed;
	case TSW_INPUT_REAL_TIME:
		return 1;
	case TSW_INPUT_SPEED_CHIP:
		session->timelapse_speed = speed; /* in real time it only chooses the next timelapse speed */
		return current > 1 ? speed : current;
	case TSW_INPUT_SPEED_KEY:
		return speed;
	case TSW_INPUT_FASTER:
		return faster(current);
	case TSW_INPUT_SLOWER:
		return slower(current);
	case TSW_INPUT_RECORD:
	case TSW_INPUT_STOP:
	case TSW_INPUT_START_REAL_TIME:
	case TSW_INPUT_START_TIMELAPSE:
		break;
	}
	return current;
}

struct tsw_command tsw_session_input(struct tsw_session *session, const struct tsw_status *status, enum tsw_input input,
				     uint32_t speed, uint64_t now_ns)
{
	if (!TSW_CHECK(session != NULL && status != NULL))
		return NOTHING;
	const bool takes_speed = input == TSW_INPUT_SPEED_CHIP || input == TSW_INPUT_SPEED_KEY;
	if (takes_speed && !TSW_CHECK(is_timelapse_speed(speed)))
		return NOTHING;

	/* 1. Idle or saving: no switch to make. */
	if (status->phase == TSW_PHASE_IDLE)
		return input_while_idle(session, input, speed);
	if (status->phase != TSW_PHASE_RECORDING)
		return NOTHING; /* saving: the file is being finalized */
	if (!TSW_CHECK(status->speed >= 1 && status->speed <= TSW_MAX_SPEED))
		return NOTHING;
	if (input == TSW_INPUT_STOP) {
		const struct tsw_command stop = {TSW_COMMAND_STOP, 0};
		return stop;
	}

	/* 2. Recording: the speed asked for. A switch between real time and timelapse right after the previous
	 *    one is a bounce (a double press), not a wish (UX-046); speed changes inside timelapse always apply. */
	const uint32_t current = status->speed;
	const uint32_t target = wanted_speed(session, current, input, speed);
	if (target == current)
		return NOTHING;
	const bool mode_switch = (target > 1) != (current > 1);
	if (mode_switch) {
		if (session->switched && now_ns - session->last_switch_ns < session->debounce_ns)
			return NOTHING;
		session->switched = true;
		session->last_switch_ns = now_ns;
	}
	if (target > 1)
		session->timelapse_speed = target;
	const struct tsw_command set_speed = {TSW_COMMAND_SET_SPEED, target};
	return set_speed;
}
