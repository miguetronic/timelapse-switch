/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * dock-paint.cpp: see dock-paint.hpp.
 */

#include "ui/dock-paint.hpp"

#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QWidget>
#include <algorithm>
#include <cmath>

namespace {

const int UNBOUNDED_HEIGHT = 100000; /* px: room for any wrapped text when measuring it */
const double RING_WIDTH = 2.0;       /* UX-110: at least 2 px */
const double RING_INSET = 1.0;
const double GLYPH_OF_LINE = 0.8; /* a glyph is this share of its text's line: 16 px at 1.1b */
const int MAX_BALANCE_STEPS = 16; /* a binary search over a width under 65536 px (P10-2) */

} // namespace

QPen tswBorder(const QColor &color, double width)
{
	if (!color.isValid() || width <= 0.0)
		return QPen(Qt::NoPen);
	QPen pen(color, width);
	pen.setJoinStyle(Qt::RoundJoin);
	return pen;
}

void tswPaintRounded(QPainter &painter, const QRectF &box, double radius, const QColor &fill, const QPen &border)
{
	painter.save();
	painter.setRenderHint(QPainter::Antialiasing);
	if (fill.isValid() && fill.alpha() > 0) {
		QPainterPath path;
		path.addRoundedRect(box, radius, radius);
		painter.fillPath(path, fill);
	}
	if (border.style() != Qt::NoPen) {
		/* The stroke is centered on its line: inset by half its width, so it stays inside box. */
		const double half = border.widthF() / 2.0;
		const QRectF inner = box.adjusted(half, half, -half, -half);
		const double innerRadius = std::max(0.0, radius - half);
		painter.setPen(border);
		painter.setBrush(Qt::NoBrush);
		painter.drawRoundedRect(inner, innerRadius, innerRadius);
	}
	painter.restore();
}

void tswPaintFocusRing(QPainter &painter, const QRectF &box, double radius, const QColor &color)
{
	const QRectF ring = box.adjusted(RING_INSET, RING_INSET, -RING_INSET, -RING_INSET);
	tswPaintRounded(painter, ring, std::max(0.0, radius - RING_INSET), QColor(), tswBorder(color, RING_WIDTH));
}

bool tswHasKeyFocus(const QWidget *widget)
{
	return widget->hasFocus() && widget->property("tswKeyFocus").toBool();
}

double tswCapBaseline(const QFont &font, double top, double height)
{
	const QFontMetricsF metrics(font);
	return std::round(top + (height + metrics.capHeight()) / 2.0);
}

double tswTextWidth(const QFont &font, const QString &text)
{
	return QFontMetricsF(font).horizontalAdvance(text);
}

int tswWrappedHeight(const QFont &font, const QString &text, int width)
{
	const QFontMetrics metrics(font);
	if (text.isEmpty())
		return metrics.height();
	const QRect bounds =
		metrics.boundingRect(QRect(0, 0, std::max(1, width), UNBOUNDED_HEIGHT), Qt::TextWordWrap, text);
	return std::max(metrics.height(), bounds.height());
}

int tswGlyphSize(const QFont &font)
{
	return static_cast<int>(std::lround(QFontMetrics(font).height() * GLYPH_OF_LINE));
}

int tswBalancedWidth(const QFont &font, const QString &text, int width)
{
	const QFontMetrics metrics(font);
	const int height = tswWrappedHeight(font, text, width);
	if (height <= metrics.height())
		return width; /* one line: nothing to balance */
	/* Fits: as many lines as at full width, and no unbreakable word sticking out. */
	const auto fits = [&](int candidate) {
		const QRect bounds =
			metrics.boundingRect(QRect(0, 0, candidate, UNBOUNDED_HEIGHT), Qt::TextWordWrap, text);
		return bounds.height() <= height && bounds.width() <= candidate;
	};
	int low = width / 2;
	int high = width; /* always fits */
	for (int step = 0; step < MAX_BALANCE_STEPS && high - low > 1; step++) {
		const int middle = (low + high) / 2;
		if (fits(middle))
			high = middle;
		else
			low = middle;
	}
	return high;
}
