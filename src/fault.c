/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* fault.c: see fault.h. */

#include "fault.h"

#include <util/threading.h>

#include "core/check.h"
#include "plugin-support.h"

enum { FAULT_SLOTS = 16 }; /* the last 16 failures are kept; older ones are only counted */

struct fault {
	const char *file;
	int line;
	const char *condition;
};

static struct fault faults[FAULT_SLOTS];
static volatile long faults_seen; /* every thread adds to it */
static long faults_logged;        /* UI thread only */

void tsw_check_failed(const char *file, int line, const char *condition)
{
	/* shortcut: a slot being rewritten while the UI thread reads it can mix two reports;
	 * acceptable for a diagnostic line, make the slots atomic if it ever matters. */
	const long number = os_atomic_inc_long(&faults_seen);
	struct fault *slot = &faults[(size_t)(number - 1) % FAULT_SLOTS];
	slot->file = file;
	slot->line = line;
	slot->condition = condition;
}

void tsw_fault_log_new(void)
{
	const long seen = os_atomic_load_long(&faults_seen);
	const long first = (seen - faults_logged > FAULT_SLOTS) ? seen - FAULT_SLOTS : faults_logged;
	if (first > faults_logged)
		tsw_log(LOG_ERROR, "TSW-E901 error | %ld failed checks not shown", first - faults_logged);
	for (long i = first; i < seen; i++) {
		const struct fault *fault = &faults[(size_t)i % FAULT_SLOTS];
		tsw_log(LOG_ERROR, "TSW-E901 error | check failed: %s | at=%s:%d", fault->condition, fault->file,
			fault->line);
	}
	faults_logged = seen;
}
