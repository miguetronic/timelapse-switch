/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * check.h: TSW_CHECK, the "this must be true" test of rule P10-5.
 *
 * Usage:  if (!TSW_CHECK(speed <= TSW_MAX_SPEED)) return TSW_E_INVALID;
 *
 * A failed check never aborts OBS: it reports the failure and evaluates to
 * false, so the caller can recover (docs/coding-rules.md, P10-5).
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * tsw_check_failed: report a failed check.
 * Safe on real-time threads: no allocation, no locking, no I/O.
 * Defined by the plugin (src/fault.c) and by the unit tests.
 */
void tsw_check_failed(const char *file, int line, const char *condition);

#ifdef __cplusplus
}
#endif

/*
 * Evaluates to true if condition holds; otherwise reports it and evaluates to false.
 * The "(report, false)" form is Holzmann's c_assert: the false is visible to the compiler and
 * to static analyzers, which then know the caller's recovery path is always taken.
 */
#define TSW_CHECK(condition) ((condition) ? true : (tsw_check_failed(__FILE__, __LINE__, #condition), false))
