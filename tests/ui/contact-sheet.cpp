/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * contact-sheet.cpp: see contact-sheet.hpp.
 */

#include "contact-sheet.hpp"

#include <QFontMetrics>
#include <QPainter>
#include <algorithm>

namespace {

const int MARGIN = 24;     /* around the whole sheet */
const int COLUMN_GAP = 20; /* between pictures side by side */
const int ROW_GAP = 32;    /* between rows */
const int LABEL_GAP = 6;   /* between a label and its picture */
const double TITLE = 1.3;  /* the title's size, relative to the labels */

/* The widest picture of each column, over every row. */
QList<int> columnWidths(const QList<QList<Shot>> &rows)
{
	QList<int> widths;
	for (const QList<Shot> &row : rows) {
		for (qsizetype i = 0; i < row.size(); i++) {
			if (widths.size() <= i)
				widths.append(0);
			widths[i] = std::max(widths[i], row[i].image.width());
		}
	}
	return widths;
}

int rowHeight(const QList<Shot> &row, int labelHeight)
{
	int tallest = 0;
	for (const Shot &shot : row)
		tallest = std::max(tallest, shot.image.height());
	return labelHeight + LABEL_GAP + tallest;
}

QFont titleFont(QFont font)
{
	if (font.pointSizeF() > 0)
		font.setPointSizeF(font.pointSizeF() * TITLE);
	else
		font.setPixelSize(static_cast<int>(font.pixelSize() * TITLE));
	font.setBold(true);
	return font;
}

} // namespace

QImage contactSheet(const QString &title, const QList<QList<Shot>> &rows, const SheetLook &look)
{
	/* 1. Measure: columns as wide as their widest picture, rows as tall as their tallest. */
	const QFont heading = titleFont(look.font);
	const int titleHeight = QFontMetrics(heading).height();
	const int labelHeight = QFontMetrics(look.font).height();
	const QList<int> widths = columnWidths(rows);
	int width = 2 * MARGIN;
	for (const int column : widths)
		width += column + COLUMN_GAP;
	int height = 2 * MARGIN + titleHeight;
	for (const QList<Shot> &row : rows)
		height += ROW_GAP + rowHeight(row, labelHeight);

	/* 2. Paint: the theme's window color, the title, then each row, label above picture. */
	QImage sheet(width - COLUMN_GAP, height, QImage::Format_RGB32);
	sheet.fill(look.background);
	QPainter painter(&sheet);
	painter.setRenderHint(QPainter::TextAntialiasing);
	painter.setPen(look.text);
	painter.setFont(heading);
	painter.drawText(QRect(MARGIN, MARGIN, width, titleHeight), Qt::AlignLeft | Qt::AlignVCenter, title);
	painter.setFont(look.font);
	const QFontMetrics metrics(look.font);
	int top = MARGIN + titleHeight + ROW_GAP;
	for (const QList<Shot> &row : rows) {
		int left = MARGIN;
		for (qsizetype i = 0; i < row.size(); i++) {
			const QString label = metrics.elidedText(row[i].label, Qt::ElideRight, widths[i]);
			painter.drawText(QRect(left, top, widths[i], labelHeight), Qt::AlignLeft | Qt::AlignVCenter,
					 label);
			painter.drawImage(QPoint(left, top + labelHeight + LABEL_GAP), row[i].image);
			left += widths[i] + COLUMN_GAP;
		}
		top += rowHeight(row, labelHeight) + ROW_GAP;
	}
	return sheet;
}
