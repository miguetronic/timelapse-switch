/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * dock-glyph.hpp: the dock's icons, painted as vectors (docs/design/visual-design.md, 3.8).
 *
 * No SVG files, emoji or font glyphs: OBS's own icons are white in dark themes and black in the
 * System theme, so they vanish in half the themes (UX-018). Each glyph is drawn on a 16 x 16 grid
 * and scaled to its box, in the color the contrast guard gives for the surface under it. Painting
 * straight into the widget's painter keeps them sharp at any screen scale.
 */

#pragma once

#include <QColor>
#include <QRectF>

class QPainter;

enum class TswGlyph {
	None,
	Record,      /* filled circle: tally, Record */
	Play,        /* real time, Back to real time, Play */
	FastForward, /* timelapse, Switch to timelapse */
	Stop,        /* Stop and save */
	Pause,       /* two bars: paused, Pause */
	Check,       /* saved */
	Error,       /* circle with a cross */
	Warning,     /* triangle with a bar and a dot */
	Info,        /* circle with an i: the context line */
	Busy,        /* three dots: starting, saving */
	Folder,      /* Open folder */
	Copy,        /* Copy details */
	LinkOut,     /* Learn more */
	Close,       /* card dismiss */
	Chevron,     /* one chevron, pointing right: timelapse tape, a closed Details section */
	ChevronDown, /* one chevron, pointing down: an open Details section */
	More,        /* three dots in a row: the dock's menu */
};

/* tswPaintGlyph: paints glyph in the square centered in box, in color. Thread: UI. */
void tswPaintGlyph(QPainter &painter, TswGlyph glyph, const QRectF &box, const QColor &color);
