/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * session.h: what each button and hotkey means in each state (docs/design/ux-spec.md, 3.3).
 *
 * The user's inputs become at most one command for the recorder. The session also keeps the
 * user's choices: the speed of the next timelapse and, while idle, whether the recording will
 * start in real time or in timelapse. Pure logic with no OBS dependency, unit tested.
 *
 *   button or hotkey ---> [ session ] ---> command: start, set speed, stop, or nothing
 *                             ^
 *   recorder status ----------+  (idle, recording at speed N, saving)
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "core/result.h"
#include "core/status.h"
#include "tsw-compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

enum tsw_input {
	TSW_INPUT_MAIN,            /* the big button: Record, or the switch while recording */
	TSW_INPUT_RECORD,          /* hotkey "Record" */
	TSW_INPUT_STOP,            /* "Stop and save" */
	TSW_INPUT_TIMELAPSE,       /* hotkey "Switch to timelapse" */
	TSW_INPUT_REAL_TIME,       /* hotkey "Back to real time" */
	TSW_INPUT_SPEED_CHIP,      /* a speed chip of the dock: chooses the timelapse speed */
	TSW_INPUT_SPEED_KEY,       /* a speed hotkey: chooses the speed and goes to timelapse (UX spec 3.11) */
	TSW_INPUT_FASTER,          /* hotkey "Faster": real time, x2, x4, x8, x16 */
	TSW_INPUT_SLOWER,          /* hotkey "Slower": the other way */
	TSW_INPUT_START_REAL_TIME, /* dock, while idle: the recording will start in real time */
	TSW_INPUT_START_TIMELAPSE, /* dock, while idle: the recording will start in timelapse */
};

enum tsw_command_kind {
	TSW_COMMAND_NONE,
	TSW_COMMAND_START,     /* start recording at speed */
	TSW_COMMAND_SET_SPEED, /* switch to speed now */
	TSW_COMMAND_STOP,      /* stop and save */
};

struct tsw_command {
	enum tsw_command_kind kind;
	uint32_t speed; /* START and SET_SPEED: 1 = real time, N = timelapse xN */
};

struct tsw_session {
	uint32_t timelapse_speed; /* the chosen chip: speed of the next timelapse, 2 to TSW_MAX_SPEED */
	bool start_in_timelapse;  /* while idle: the recording will start in timelapse */
	uint64_t debounce_ns;     /* a switch this soon after the previous one is ignored (UX-046) */
	uint64_t last_switch_ns;  /* when the last switch between real time and timelapse happened */
	bool switched;            /* last_switch_ns is set */
};

/*
 * tsw_session_init: a new session; the next timelapse runs at timelapse_speed (2 to TSW_MAX_SPEED),
 * and a switch within debounce_ns of the previous one is ignored.
 * Returns TSW_OK or TSW_E_INVALID. Thread: UI. Memory: none allocated.
 */
enum tsw_result tsw_session_init(struct tsw_session *session, uint32_t timelapse_speed,
				 uint64_t debounce_ns) TSW_NODISCARD;

/*
 * tsw_session_input: what an input means now.
 *   status  what the recorder is doing (only phase and speed are read).
 *   speed   for the speed chip and speed keys: 2 to TSW_MAX_SPEED; ignored otherwise.
 *   now_ns  the time of the input, for the debounce.
 * Returns the command for the recorder, often TSW_COMMAND_NONE. Thread: UI. Memory: none allocated.
 */
struct tsw_command tsw_session_input(struct tsw_session *session, const struct tsw_status *status, enum tsw_input input,
				     uint32_t speed, uint64_t now_ns);

#ifdef __cplusplus
}
#endif
