/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* result.h: what a function that can fail returns (docs/coding-rules.md, B3). */

#pragma once

enum tsw_result {
	TSW_OK = 0,
	TSW_E_INVALID,       /* a parameter or state broke a TSW_CHECK: a bug, reported as TSW-E901 */
	TSW_E_TIMELINE_FULL, /* more speed switches in one recording than the timeline can hold */
};
