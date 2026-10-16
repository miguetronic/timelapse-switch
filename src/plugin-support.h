/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * plugin-support.h: plugin identity and logging.
 *
 * Every message this plugin writes to the OBS log starts with "[timelapse-switch]",
 * so users and maintainers can find our lines among thousands of OBS lines.
 */

#pragma once

#include <util/base.h>

#include "tsw-compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const char *PLUGIN_NAME;
extern const char *PLUGIN_VERSION;

/*
 * tsw_log: write one line to the OBS log, prefixed with the plugin name.
 *   log_level  LOG_DEBUG, LOG_INFO, LOG_WARNING or LOG_ERROR (from OBS).
 *   format     printf-style text; the compiler checks the arguments against it.
 * Thread: any, except the real-time video and audio callbacks (docs/coding-rules.md, B1).
 * Memory: none allocated; messages longer than TSW_LOG_MAX bytes are cut.
 */
void tsw_log(int log_level, const char *format, ...) TSW_PRINTF_FORMAT(2, 3);

#ifdef __cplusplus
}
#endif
