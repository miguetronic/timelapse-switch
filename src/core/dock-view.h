/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * dock-view.h: what each zone of the dock shows (docs/design/ux-spec.md, 3.2 to 3.4), and the
 * monitoring on top of them: one health line when needed, the details on request (3.17).
 *
 *   recorder status + session choices ---> [ dock view ] ---> words, numbers and styles per zone
 *
 * The zones never move (UX-002), in four bands (docs/design/visual-design.md, 3.1):
 *   STATUS   A state block: a tally line (REC) and a hero line (TIMELAPSE x4)
 *            B readout: recorded and final length, and a status line (the current stretch)
 *   ACTION   C main button
 *   BELOW    D speed rails, or the summary or error card    E Pause (or Resume) and Stop and save
 *   CONTEXT  F one muted line, or the health line while the recording needs attention
 *            the Details section, closed until the user opens it (the dock remembers that)
 *
 * Words are locale keys (the .ini files in data/locale). The dock translates them and puts the number
 * or name it is given in place of "%1". Pure logic with no OBS or Qt dependency, unit tested.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "core/health.h"
#include "core/session.h"
#include "core/status.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
	TSW_TIME_TEXT = 16,   /* room for "h:mm:ss" up to 9999 hours, and the final zero */
	TSW_NUMBER_TEXT = 8,  /* room for a number up to 9999999, or "9.9", and the final zero */
	TSW_HEALTH_KINDS = 4, /* disk low, both recording, frames lost, encoder behind (tsw_health_issue) */
};

/*
 * An amount and its unit, for the details and the health line. The dock puts number (and second) in
 * place of "%1" (and "%2") in the unit's locale key, such as "Unit.GB" = "%1 GB", and writes the
 * decimal point the language's way: "1.2 GB", or "1,2 GB" in Spanish.
 */
struct tsw_amount {
	const char *unit;             /* key; NULL when the amount is not known (yet) */
	char number[TSW_NUMBER_TEXT]; /* "1.2", "38", "93"; a point before a decimal */
	char second[TSW_NUMBER_TEXT]; /* the "%2" of a unit with two numbers ("1 h 33 min"), else empty */
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
	uint32_t speed_selected; /* 2 to 16; while paused also 1, the "Real time" segment */
	bool speeds_enabled;
	bool speeds_real_time;   /* paused: one "Resume in" rail, Real time and the speeds (UX-131) */
	bool start_mode_shown;   /* idle: "Start in: real time / timelapse" */
	bool start_in_timelapse; /* idle: it will start in timelapse; paused: it will resume in timelapse */

	/* E. Pause (it sends TSW_INPUT_PAUSE) and Stop, side by side (UX-128) */
	const char *pause_text; /* key: "Pause", or "Resume" while paused, in the same place */
	bool pause_shown;
	bool pause_enabled;
	bool stop_shown;
	bool stop_enabled;

	/* F. Context line */
	const char *context_text; /* key or NULL; "%1" = the file name of context_path */
	const char *context_path; /* borrowed from the status; the dock shows its file name */

	/* F. Health (UX-134): while recording or paused, the most urgent issue takes the context line's
	 * place, with a warning glyph and its code; nothing when all is well. */
	uint32_t health;                 /* one TSW_HEALTH_* bit, or 0 */
	uint32_t health_code;            /* its TSW-E number: 102, 302, 201 or 203 */
	const char *health_text;         /* key; "%1" = health_amount */
	struct tsw_amount health_amount; /* E102: the time left; E201: the frames lost */

	/* Details (UX-135, ux-spec 3.17): shown on request while recording, paused or saving. */
	bool details_shown;
	char waiting_skipped[TSW_TIME_TEXT]; /* recorded minus final, short: "14:42" */
	struct tsw_amount compression;       /* recorded / final: "1.9", unit "Unit.Times" */
	char real_time[TSW_TIME_TEXT];       /* where the recorded time went, short: "12:03" */
	char timelapse[TSW_TIME_TEXT];       /* before it was sped up */
	char paused[TSW_TIME_TEXT];
	uint32_t switches;
	uint32_t pauses;
	uint32_t chapters;           /* one per stretch at one speed: switches + 1 (markers left out) */
	struct tsw_amount file_size; /* not known until the file is first measured */
	struct tsw_amount file_rate; /* how much the file grows per minute; the dock adds "/min" */
	struct tsw_amount disk_free;
	struct tsw_amount time_left; /* the room left on the disk at that rate; not known while not growing */
	uint32_t backlog;            /* frames waiting for the encoder now ("1 of 16") */
	uint32_t backlog_size;       /* frames the feed can hold */

	/* The speed map (UX-136): in the details while recording, and in the summary card after it. */
	bool map_shown;
	uint8_t map[TSW_MAP_BUCKETS]; /* per slice: 1, 2 to 16, or TSW_SPEED_PAUSED */

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
	uint32_t told_health;        /* the health issue they last heard about, 0 for none */

	/* Health, held on screen a while after its condition ends, so it never blinks (UX-063); not both
	 * recordings, which the user ends on purpose. The clock is the recorded time, which stands still while
	 * paused: an issue seen before a pause stays. */
	uint64_t health_until_ns[TSW_HEALTH_KINDS]; /* per issue: the recorded time it stays until */
	uint64_t health_clock_ns;                   /* the recorded time last seen; less means a new recording */
	uint64_t disk_seconds_left;                 /* the last time left seen with the disk low, for a held warning */
	uint8_t map[TSW_MAP_BUCKETS];               /* the speed map of the recording under way, or of the last one */
	bool map_known;                             /* map holds a recording's map not yet given to a card */
	bool card_map;                              /* the open card shows map */
};

/* tsw_format_duration: ns as "h:mm:ss" (whole seconds, at most 9999:59:59). Memory: none allocated. */
void tsw_format_duration(uint64_t ns, char text[TSW_TIME_TEXT]);

/* tsw_format_short_duration: ns as "m:ss" under an hour, "h:mm:ss" from an hour on. Memory: none. */
void tsw_format_short_duration(uint64_t ns, char text[TSW_TIME_TEXT]);

/*
 * tsw_format_size: bytes as MB, GB or TB (of 1024, as OBS and most systems show them), whichever keeps
 * the number under 1000: one decimal under 10 ("1.2 GB"), whole above ("212 GB"). Memory: none.
 */
void tsw_format_size(uint64_t bytes, struct tsw_amount *amount);

/*
 * tsw_format_time_left: seconds as minutes ("9 min"), hours and minutes ("1 h 33 min"), or hours from
 * 10 hours on ("93 h"), always rounded down: the disk never lasts less than it says. Under a minute,
 * "Unit.UnderMinute"; UINT64_MAX (not known), no unit. Memory: none.
 */
void tsw_format_time_left(uint64_t seconds, struct tsw_amount *amount);

/*
 * tsw_format_compression: how many times shorter the final video is than the recorded time, with one
 * decimal under 10 ("1.9"), whole above ("16"); not known while the final video is empty. Memory: none.
 */
void tsw_format_compression(uint64_t recorded_ns, uint64_t final_ns, struct tsw_amount *amount);

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
