/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * test.h: the smallest test helper that does the job (docs/coding-rules.md, D1).
 *
 * A test is a function; TSW_EXPECT_* records a failure with file and line and keeps going.
 * main() runs the tests listed in TSW_TESTS and returns the number of failures.
 */

#pragma once

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

extern int tsw_test_failures;
extern int tsw_check_failures; /* failed TSW_CHECKs seen by the code under test */

static inline void tsw_expect_u64(uint64_t actual, uint64_t expected, const char *file, int line, const char *text)
{
	if (actual != expected) {
		printf("%s:%d: %s\n    expected %" PRIu64 "\n    got      %" PRIu64 "\n", file, line, text, expected,
		       actual);
		tsw_test_failures++;
	}
}

#define TSW_EXPECT_EQ(actual, expected) \
	tsw_expect_u64((uint64_t)(actual), (uint64_t)(expected), __FILE__, __LINE__, #actual " == " #expected)
#define TSW_EXPECT_TRUE(condition) tsw_expect_u64((condition) ? 1 : 0, 1, __FILE__, __LINE__, #condition)
