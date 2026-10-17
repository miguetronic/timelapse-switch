/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * dock-glyph.cpp: see dock-glyph.hpp. Every coordinate below is on the 16 x 16 grid of
 * docs/design/visual-design.md, 3.8; the painter is scaled so one grid unit is size / 16 pixels.
 */

#include "ui/dock-glyph.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <algorithm>

namespace {

const double GRID = 16.0;
const double MIN_STROKE_PX = 1.5; /* thinner lines break up when antialiased */

/* Strokes the outline of a path, filled shapes use fillPath instead. */
void stroke(QPainter &p, const QPainterPath &path)
{
	p.setBrush(Qt::NoBrush);
	p.drawPath(path);
}

void fillTriangle(QPainter &p, QPointF a, QPointF b, QPointF c)
{
	QPainterPath path(a);
	path.lineTo(b);
	path.lineTo(c);
	path.closeSubpath();
	p.fillPath(path, p.pen().color());
}

void paintFilled(QPainter &p, TswGlyph glyph)
{
	const QColor color = p.pen().color();
	switch (glyph) {
	case TswGlyph::Record:
		p.setPen(Qt::NoPen);
		p.setBrush(color);
		p.drawEllipse(QPointF(8, 8), 5, 5);
		break;
	case TswGlyph::Play:
		fillTriangle(p, {4, 3}, {13, 8}, {4, 13});
		break;
	case TswGlyph::FastForward:
		fillTriangle(p, {1, 3}, {8, 8}, {1, 13});
		fillTriangle(p, {8, 3}, {15, 8}, {8, 13});
		break;
	case TswGlyph::Stop: {
		QPainterPath square;
		square.addRoundedRect(QRectF(3.5, 3.5, 9, 9), 1.5, 1.5);
		p.fillPath(square, color);
		break;
	}
	case TswGlyph::Pause: {
		/* Two bars in the stop square's box, a third of it wide each, a third apart. */
		QPainterPath bars;
		bars.addRoundedRect(QRectF(3.5, 3, 3, 10), 1, 1);
		bars.addRoundedRect(QRectF(9.5, 3, 3, 10), 1, 1);
		p.fillPath(bars, color);
		break;
	}
	case TswGlyph::Busy:
		p.setPen(Qt::NoPen);
		p.setBrush(color);
		for (const double x : {3.0, 8.0, 13.0})
			p.drawEllipse(QPointF(x, 8), 1.5, 1.5);
		break;
	default:
		break;
	}
}

/* Circle glyphs: error (a cross inside) and info (an i inside). */
void paintCircled(QPainter &p, TswGlyph glyph)
{
	QPainterPath circle;
	circle.addEllipse(QPointF(8, 8), 6.5, 6.5);
	stroke(p, circle);
	if (glyph == TswGlyph::Error) {
		p.drawLine(QPointF(5.5, 5.5), QPointF(10.5, 10.5));
		p.drawLine(QPointF(10.5, 5.5), QPointF(5.5, 10.5));
		return;
	}
	p.drawLine(QPointF(8, 7.2), QPointF(8, 11.5));
	p.setBrush(p.pen().color());
	const QPen pen = p.pen();
	p.setPen(Qt::NoPen);
	p.drawEllipse(QPointF(8, 4.8), 1.0, 1.0);
	p.setPen(pen);
}

void paintFolderOrCopy(QPainter &p, TswGlyph glyph)
{
	QPainterPath path;
	if (glyph == TswGlyph::Folder) {
		path.moveTo(1.5, 4);
		path.lineTo(1.5, 13);
		path.lineTo(14.5, 13);
		path.lineTo(14.5, 4.5);
		path.lineTo(7.5, 4.5);
		path.lineTo(6, 2.8);
		path.lineTo(1.5, 2.8);
		path.closeSubpath();
	} else {
		path.addRoundedRect(QRectF(2, 5, 8, 8), 1.5, 1.5);
		path.moveTo(6, 4.5);
		path.lineTo(6, 3.5);
		path.quadTo(6, 2, 7.5, 2);
		path.lineTo(12.5, 2);
		path.quadTo(14, 2, 14, 3.5);
		path.lineTo(14, 8.5);
		path.quadTo(14, 10, 12.5, 10);
		path.lineTo(10.5, 10);
	}
	stroke(p, path);
}

void paintStroked(QPainter &p, TswGlyph glyph)
{
	switch (glyph) {
	case TswGlyph::Check: {
		const QPointF check[] = {{3, 8.5}, {6.5, 12}, {13, 4.5}};
		p.drawPolyline(check, 3);
		break;
	}
	case TswGlyph::Warning: {
		const QPointF triangle[] = {{8, 1.5}, {15, 14}, {1, 14}};
		p.setBrush(Qt::NoBrush);
		p.drawPolygon(triangle, 3);
		p.drawLine(QPointF(8, 6), QPointF(8, 9.5));
		p.setBrush(p.pen().color());
		p.drawEllipse(QPointF(8, 11.8), 0.5, 0.5);
		break;
	}
	case TswGlyph::LinkOut: {
		QPainterPath box(QPointF(7, 4));
		box.lineTo(2, 4);
		box.lineTo(2, 14);
		box.lineTo(12, 14);
		box.lineTo(12, 9);
		stroke(p, box);
		const QPointF arrow[] = {{8.5, 2}, {14, 2}, {14, 7.5}};
		p.drawPolyline(arrow, 3);
		p.drawLine(QPointF(14, 2), QPointF(7.5, 8.5));
		break;
	}
	case TswGlyph::Close:
		p.drawLine(QPointF(4, 4), QPointF(12, 12));
		p.drawLine(QPointF(12, 4), QPointF(4, 12));
		break;
	case TswGlyph::Chevron: {
		const QPointF chevron[] = {{5, 3}, {10, 8}, {5, 13}};
		p.drawPolyline(chevron, 3);
		break;
	}
	case TswGlyph::Error:
	case TswGlyph::Info:
		paintCircled(p, glyph);
		break;
	case TswGlyph::Folder:
	case TswGlyph::Copy:
		paintFolderOrCopy(p, glyph);
		break;
	default:
		paintFilled(p, glyph);
		break;
	}
}

} // namespace

void tswPaintGlyph(QPainter &painter, TswGlyph glyph, const QRectF &box, const QColor &color)
{
	if (glyph == TswGlyph::None || box.isEmpty() || !color.isValid())
		return;
	const double size = std::min(box.width(), box.height());
	const double scale = size / GRID;
	painter.save();
	painter.setRenderHint(QPainter::Antialiasing);
	painter.translate(box.center().x() - size / 2.0, box.center().y() - size / 2.0);
	painter.scale(scale, scale);
	/* The stroke is set in pixels (at least 1.5) and divided by the scale, so it stays crisp. */
	const double strokePx = std::max(MIN_STROKE_PX, size / 10.0);
	painter.setPen(QPen(color, strokePx / scale, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
	painter.setBrush(Qt::NoBrush);
	paintStroked(painter, glyph);
	painter.restore();
}
