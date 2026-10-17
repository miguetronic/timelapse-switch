/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * contact-sheet.hpp: many pictures of the dock in one, like a photographer's contact sheet, so a
 * person sees every state of one theme at a glance (tests/ui/README.md):
 *
 *   title
 *   label     label     label  ...      one row per width, one column per state,
 *   +-----+   +-----+   +-----+          each picture with its label above it,
 *   |     |   |     |   |     |          all on the theme's own window color
 *   +-----+   +-----+   +-----+
 *   label     label     ...
 *   +---+     +---+
 */

#pragma once

#include <QColor>
#include <QFont>
#include <QImage>
#include <QList>
#include <QString>

struct Shot {
	QString label;
	QImage image;
};

struct SheetLook {
	QColor background; /* the theme's window color */
	QColor text;       /* the theme's text color */
	QFont font;        /* the theme's font */
};

/* Composes the sheet: rows of shots, top to bottom; columns as wide as their widest shot. */
QImage contactSheet(const QString &title, const QList<QList<Shot>> &rows, const SheetLook &look);
