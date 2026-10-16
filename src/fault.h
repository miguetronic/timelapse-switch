/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * fault.h: where failed TSW_CHECKs go (docs/coding-rules.md, P10-5).
 *
 * A check can fail on a real-time thread, where writing to the log is not allowed.
 * So tsw_check_failed (core/check.h) only notes the failure in a small table, and the
 * UI thread later writes what it finds there to the OBS log, as error TSW-E901.
 */

#pragma once

/* tsw_fault_log_new: write the checks that failed since the last call to the OBS log. Thread: UI. */
void tsw_fault_log_new(void);
