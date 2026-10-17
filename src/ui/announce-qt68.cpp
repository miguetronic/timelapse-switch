/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * announce-qt68.cpp: see announce.hpp. Built with Qt 6.8 or newer.
 */

#include "ui/announce.hpp"

#include <QAccessible>
#include <QWidget>

void tswAnnounce(QWidget *source, const QString &words, bool assertive)
{
	if (source == nullptr || words.isEmpty())
		return;
	QAccessibleAnnouncementEvent event(source, words);
	event.setPoliteness(assertive ? QAccessible::AnnouncementPoliteness::Assertive
				      : QAccessible::AnnouncementPoliteness::Polite);
	QAccessible::updateAccessibility(&event);
}
