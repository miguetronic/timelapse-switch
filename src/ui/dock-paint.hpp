/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * dock-paint.hpp: small painting helpers shared by the dock's widgets: rounded shapes whose
 * border stays inside their box, the keyboard focus ring (UX-110), and text placed by its capitals
 * so words in uppercase sit visually centered in pills and rows.
 */

#pragma once

#include <QColor>
#include <QFont>
#include <QPen>
#include <QRectF>
#include <QString>

class QPainter;
class QWidget;

/* The type scale of the "Studio cards" direction, in multiples of the OBS base size b (4.2). */
namespace TswType {
const double TALLY = 0.75;
const double HERO = 2.25;
const double HERO_FLOOR = 1.5;
const double QUIET = 1.35;
const double BADGE = 1.5;
const double CAPTION = 0.75;
const double DIGITS = 1.6;
const double STATUS = 0.9;
const double MAIN = 1.1;
const double RAIL = 1.1;
const double STOP = 1.0;
const double BODY = 1.0;
const double LINK = 0.95;
const double CODE = 0.8;
const double CONTEXT = 0.9;
} // namespace TswType

/* Fills box with rounded corners, then strokes its border inside the box (width 0: no border). */
void tswPaintRounded(QPainter &painter, const QRectF &box, double radius, const QColor &fill, const QPen &border);

/* A plain solid pen for borders; an invalid color gives no border. */
QPen tswBorder(const QColor &color, double width);

/* How a focus ring is drawn: solid, or dashed where a solid outline already means something. */
enum class TswRing { Solid, Dashed };

/* The keyboard focus ring: 2 px in color, inset px inside box (1 px, or more to clear a frame),
 * following the radius (UX-110). */
void tswPaintFocusRing(QPainter &painter, const QRectF &box, double radius, const QColor &color, double inset,
		       TswRing style);

/* Is this widget focused from the keyboard (Tab, Backtab or a shortcut)? Set by the dock. */
bool tswHasKeyFocus(const QWidget *widget);

/* The baseline that centers the capitals of font vertically in a row from top to top + height. */
double tswCapBaseline(const QFont &font, double top, double height);

/* The width of a single line of text in font. */
double tswTextWidth(const QFont &font, const QString &text);

/* The height of text wrapped at width in font (at least one line). */
int tswWrappedHeight(const QFont &font, const QString &text, int width);

/* The size of a glyph next to a text in font: a little taller than its capitals (3.8). */
int tswGlyphSize(const QFont &font);

/* The narrowest width at which text still takes as many lines as at width: two-line labels then
 * break into lines of similar length instead of leaving one word alone on the second line. */
int tswBalancedWidth(const QFont &font, const QString &text, int width);
