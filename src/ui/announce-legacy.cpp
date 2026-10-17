/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * announce-legacy.cpp: see announce.hpp. Built with Qt older than 6.8, which has no announcements:
 * the source's accessible name becomes the words, and screen readers hear the name change. The
 * dock sets the name back to the state on its next refresh.
 */

#include "ui/announce.hpp"

#include <QAccessible>
#include <QWidget>

void tswAnnounce(QWidget *source, const QString &words, bool assertive)
{
	(void)assertive; /* older Qt cannot say how urgent it is */
	if (source == nullptr || words.isEmpty())
		return;
	source->setAccessibleName(words);
	QAccessibleEvent event(source, QAccessible::NameChanged);
	QAccessible::updateAccessibility(&event);
}
