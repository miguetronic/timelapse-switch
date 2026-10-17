/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * announce.hpp: tells screen readers about a change of state (UX-104).
 *
 * Qt 6.8 added announcements (QAccessibleAnnouncementEvent); Linux builds of OBS 31.1 may use an
 * older Qt. CMake picks one of two files (rule P10-8, no #if inside a function):
 *   announce-qt68.cpp    a real announcement, polite or assertive
 *   announce-legacy.cpp  the source's accessible name becomes the words, and the name change is sent
 */

#pragma once

#include <QString>

class QWidget;

/* tswAnnounce: says words through source; assertive interrupts (errors). Thread: UI. */
void tswAnnounce(QWidget *source, const QString &words, bool assertive);
