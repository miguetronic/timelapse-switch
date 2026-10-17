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
 * The zones never move (UX-002), in four bands (docs/design/visual-design.md, 3.1):
 *   STATUS   A state block: a tally line (REC) and a hero line (TIMELAPSE x4)
 *            B readout: recorded and final length, and a status line (the current stretch)
 *   ACTION   C main button
 *   BELOW    D speed rails, or the summary or error card    E Pause (or Resume) and Stop and save
 *   CONTEXT  F one muted line
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

/* The kind of state the state block shows; each has its own word, glyph and frame (UX-003). */
enum tsw_banner {
	TSW_BANNER_READY,     /* idle: quiet */
	TSW_BANNER_BUSY,      /* saving: quiet, with busy dots */
	TSW_BANNER_REAL_TIME, /* recording at normal speed */
	TSW_BANNER_TIMELAPSE, /* recording sped up: the one filled, accent state */
	TSW_BANNER_PAUSED,    /* recording, paused: nothing reaches the file until it resumes (UX-129) */
	TSW_BANNER_SAVED,     /* the summary card is open */
	TSW_BANNER_ERROR,     /* the error card is open */
};

struct tsw_dock_view {
	/* A. State block: two signals, never merged (UX-003) */
	enum tsw_banner banner;
	uint32_t hero_speed;    /* timelapse: the speed now; idle and paused: the timelapse speed it would use */
	const char *tally_text; /* key: READY, REC, PAUSED, SAVING, SAVED or ERROR */
	const char *hero_text;  /* key; "%1" = hero_speed */
	bool hero_quiet;        /* sentence case and smaller: never looks like a recording */

	/* B. Readout */
	bool counters_dimmed; /* idle: a dimmed 0:00:00 (UX-006) */
	char recorded[TSW_TIME_TEXT];
	char final_length[TSW_TIME_TEXT];
	bool section_shown; /* the current stretch, in the status line (UX-007): live, frozen while saving, 0:00 if none */
	uint32_t section_speed;               /* 1 in real time and while paused */
	const char *section_label;            /* key: "This stretch", or "Paused for" while paused (UX-132) */
	char section_recorded[TSW_TIME_TEXT]; /* short: "19:37"; while paused, how long: "0:01:12" */
	char section_final[TSW_TIME_TEXT];

	/* Card after a recording, below C: summary (error_code 0) or error */
	bool card_shown;
	uint32_t error_code;         /* TSW-E number; 0 = summary */
	uint32_t error_texts;        /* the code whose texts the dock shows: 1, 2, 103, or 901 for any other */
	char skipped[TSW_TIME_TEXT]; /* summary: waiting the timelapse saved, "52:35"; empty if none */
	bool thank_user;             /* summary: the one warm line of the OBS session (UX-072) */

	/* C. Main button: it always sends TSW_INPUT_MAIN */
	const char *main_text; /* key; "%1" = main_speed */
	uint32_t main_speed;   /* while paused: the speed it will resume at, 1 = real time */
	bool main_enabled;

	/* D. Speed row */
	bool speeds_shown;
	const char *speed_label; /* key */
	uint32_t speed_selected;
	bool speeds_enabled;
	bool start_mode_shown;   /* idle: "Start in: real time / timelapse"; paused: "Resume in" (UX-131) */
	bool start_in_timelapse; /* idle: it will start in timelapse; paused: it will resume in timelapse */
	const char *start_label; /* key: "Start in", or "Resume in" while paused */

	/* E. Pause (it sends TSW_INPUT_PAUSE) and Stop, side by side (UX-128) */
	const char *pause_text; /* key: "Pause", or "Resume" while paused, in the same place */
	bool pause_shown;
	bool pause_enabled;
	bool stop_shown;
	bool stop_enabled;

	/* F. Context line */
	const char *context_text; /* key or NULL; "%1" = the file name of context_path */
	const char *context_path; /* borrowed from the status; the dock shows its file name */

	/* Screen readers (UX-104) */
	const char *announce_text; /* key or NULL: a change of state to announce once */
};

/* What the dock remembers between two views: the card after a recording, and what was announced. */
struct tsw_dock_model {
	uint32_t seen_outcome; /* the last outcome already turned into a card */
	bool card_open;
	bool thanked;                /* the warm line was used in this OBS session */
	bool thank_now;              /* the open card carries it */
	enum tsw_banner told_banner; /* the state screen readers last heard about */
	uint32_t told_speed;         /* and its speed */
};

/* tsw_format_duration: ns as "h:mm:ss" (whole seconds, at most 9999:59:59). Memory: none allocated. */
void tsw_format_duration(uint64_t ns, char text[TSW_TIME_TEXT]);

/* tsw_format_short_duration: ns as "m:ss" under an hour, "h:mm:ss" from an hour on. Memory: none. */
void tsw_format_short_duration(uint64_t ns, char text[TSW_TIME_TEXT]);

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
