/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * tsw-compiler.h: the only file that handles differences between compilers.
 *
 * Power of Ten, rule 8 allows conditional compilation only where it cannot be avoided,
 * each use justified. Keeping all of it here means the rest of the code reads the same
 * on every compiler. Each macro expands to one complete attribute, or to nothing.
 */

#pragma once

/* Justification: GCC and Clang (Linux, macOS) support these attributes; MSVC (Windows) does not. */
#if defined(__GNUC__) || defined(__clang__)

/* Lets the compiler check printf-style arguments against the format text. */
#define TSW_PRINTF_FORMAT(format_index, first_arg_index) \
	__attribute__((format(printf, format_index, first_arg_index)))

/* Makes the compiler warn when a caller ignores the result (Power of Ten, rule 7). */
#define TSW_NODISCARD __attribute__((warn_unused_result))

#else

#define TSW_PRINTF_FORMAT(format_index, first_arg_index)
#define TSW_NODISCARD

#endif

/* Justification: road C (ADR-012) is tested on Linux only so far; on Windows and macOS it stays off
 * until it is tested there, and road B is used. 1 where it may be used, 0 elsewhere. */
#if defined(__linux__)
#define TSW_GPU_ROAD_PLATFORM 1
#else
#define TSW_GPU_ROAD_PLATFORM 0
#endif
