/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* test-main.c: shared pieces of every unit test program. */

#include "test.h"

#include <core/check.h>

int tsw_test_failures = 0;
int tsw_check_failures = 0;

/* In tests, a failed TSW_CHECK is counted so a test can expect (or forbid) it. */
void tsw_check_failed(const char *file, int line, const char *condition)
{
	(void)file;
	(void)line;
	(void)condition;
	tsw_check_failures++;
}
