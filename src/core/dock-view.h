/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * dock-view.h: what each zone of the dock shows (docs/design/ux-spec.md, 3.2 to 3.4).
 *
 *   recorder status + session choices ---> [ dock view ] ---> words, numbers and styles per zone
 *
 * The zones never move (UX-002):
 *   A banner    B counters (and the summary or error card)    C main button
 *   D speed row    E stop button    F context line
 *
 * Words are locale keys (the .ini files in data/locale). The dock translates them and puts the number
 * or name it is given in place of "%1". Pure logic with no OBS or Qt dependency, unit tested.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "core/session.h"
#include "core/status.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
	TSW_TIME_TEXT = 16, /* room for "h:mm:ss" up to 9999 hours, and the final zero */
};

enum tsw_banner {
	TSW_BANNER_READY,     /* idle: neutral */
	TSW_BANNER_BUSY,      /* saving: neutral, with a spinner */
	TSW_BANNER_REAL_TIME, /* outlined frame with the record dot */
	TSW_BANNER_TIMELAPSE, /* filled with the theme highlight, heavier frame, chevrons */
	TSW_BANNER_SAVED,     /* the summary card: calm */
	TSW_BANNER_ERROR,     /* the error card: danger frame */
};

struct tsw_dock_view {
	/* A. Banner */
	enum tsw_banner banner;
	const char *banner_text;   /* key; "%1" = banner_speed */
	const char *banner_detail; /* key or NULL: how the recording will start */
	uint32_t banner_speed;

	/* B. Counters */
	bool counters_dimmed; /* idle: "-:--:--" */
	char recorded[TSW_TIME_TEXT];
	char final_length[TSW_TIME_TEXT];
	bool section_shown; /* timelapse: the current section */
	char section_recorded[TSW_TIME_TEXT];
	char section_final[TSW_TIME_TEXT];

	/* B. Card after a recording: summary (error_code 0) or error */
	bool card_shown;
	uint32_t error_code;         /* TSW-E number; 0 = summary */
	char skipped[TSW_TIME_TEXT]; /* summary: waiting the timelapse saved */
	bool thank_user;             /* summary: the one warm line of the OBS session (UX-072) */

	/* C. Main button: it always sends TSW_INPUT_MAIN */
	const char *main_text; /* key; "%1" = main_speed */
	uint32_t main_speed;
	bool main_enabled;

	/* D. Speed row */
	bool speeds_shown;
	const char *speed_label; /* key */
	uint32_t speed_selected;
	bool speeds_enabled;
	bool start_mode_shown; /* idle: "Start in: real time / timelapse" */
	bool start_in_timelapse;

	/* E. Stop button */
	bool stop_shown;
	bool stop_enabled;

	/* F. Context line */
	const char *context_text; /* key or NULL; "%1" = the file name of context_path */
	const char *context_path; /* borrowed from the status; the dock shows its file name */
};

/* What the dock remembers between two views: the card after a recording. */
struct tsw_dock_model {
	uint32_t seen_outcome; /* the last outcome already turned into a card */
	bool card_open;
	bool thanked;   /* the warm line was used in this OBS session */
	bool thank_now; /* the open card carries it */
};

/* tsw_format_duration: ns as "h:mm:ss" (whole seconds, at most 9999:59:59). Memory: none allocated. */
void tsw_format_duration(uint64_t ns, char text[TSW_TIME_TEXT]);

/* tsw_dock_model_init: no card yet. Thread: UI. */
void tsw_dock_model_init(struct tsw_dock_model *model);

/* tsw_dock_model_dismiss: close the summary or error card. Thread: UI. */
void tsw_dock_model_dismiss(struct tsw_dock_model *model);

/*
 * tsw_dock_view_build: the view for the current status and choices. A recording that ended since the
 * last call opens its card; a new recording closes it. Thread: UI. Memory: none allocated.
 */
void tsw_dock_view_build(struct tsw_dock_model *model, const struct tsw_session *session,
			 const struct tsw_status *status, struct tsw_dock_view *view);

#ifdef __cplusplus
}
#endif
