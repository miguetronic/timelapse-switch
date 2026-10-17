/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * dock-monitor.hpp: the dock's monitoring and its menu (docs/design/ux-spec.md, 3.17 and 3.18), in
 * the "Studio cards" look (docs/design/visual-design.md, 4.2).
 *
 * Monitoring follows one rule: silence when all is well, one line when something needs attention,
 * everything else only on request. Think of a car's dashboard: the speed is always there, a warning
 * lamp lights only when needed, and the trip computer shows its numbers when you press its button.
 *
 *   TswSpeedMap    the recording as a thin strip: real time plain, timelapse hatched (denser when
 *                  faster), a pause as a gap; in the Details and in the summary card
 *   TswDisclosure  the "Details" toggle: a caption with a chevron, closed until the user opens it
 *   TswDetails     the toggle and, when open, a card of values: where the time went, the file and
 *                  the disk, the encoder's backlog, and the speed map
 *   TswMenuButton  the small round button in the status card's top corner that opens the dock's
 *                  menu: Help and About (UX-138)
 *
 * Nothing here costs anything measurable (UX-137): the values are the recorder's own counters, set
 * on labels only when they change, and the map is 48 slices painted with a few rectangles.
 */

#pragma once

#include <QAbstractButton>
#include <QColor>
#include <QToolButton>
#include <QVector>
#include <QWidget>

#include "core/dock-view.h"
#include "ui/dock-theme.hpp"

class QGridLayout;
class QLabel;
class QVBoxLayout;
class TswCard;

/* Translates a locale key: obs_module_text in OBS, a reader of the .ini files in the UI test. */
using TswText = const char *(*)(const char *key);

/* An amount in its unit and the language's decimal mark ("1,2 GB" in Spanish); empty when not known. */
QString tswAmountText(TswText lookup, const tsw_amount &amount);

/* A speed map in words for a screen reader: its stretches in order ("Real time, timelapse 4 times
 * faster, paused"), never a list of slices. */
QString tswSpokenMap(TswText lookup, const uint8_t map[TSW_MAP_BUCKETS]);

/* The colors of a speed map on a surface; public so the UI test can check their contrast (UX-136). */
struct TswMapColors {
	QColor realTime;     /* real time: a plain, quiet fill */
	QColor realTimeEdge; /* and its outline, 3:1 on the surface */
	QColor timelapse;    /* timelapse: a soft tint under the hatching */
	QColor hatch;        /* its lines and outline, 3:1 on the tint and on the surface */
	QColor tape;         /* the line a pause leaves visible: decorative */
};
TswMapColors tswMapColors(const TswTheme &theme, const QColor &surface);

/* The speed map (UX-136): TSW_MAP_BUCKETS slices of the recording, painted from the theme's tokens. */
class TswSpeedMap : public QWidget {
	Q_OBJECT

public:
	TswSpeedMap(QWidget *parent, const TswTheme *theme);
	/* Shows map; with lookup, also says it in words for screen readers (only when it changed). */
	void setMap(const uint8_t map[TSW_MAP_BUCKETS], TswText lookup);
	QVector<uint8_t> map() const;
	bool hasHeightForWidth() const override;
	int heightForWidth(int width) const override;
	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	void paintRun(QPainter &painter, const QRectF &box, uint8_t speed, const TswMapColors &colors) const;

	const TswTheme *theme;
	QVector<uint8_t> slices;
};

/* "Details" with a chevron: a checkable button, open when checked; never opened by the dock itself. */
class TswDisclosure : public QAbstractButton {
	Q_OBJECT

public:
	TswDisclosure(QWidget *parent, const TswTheme *theme);
	void setCaptionFont(const QFont &font);
	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	const TswTheme *theme;
	QFont captionFont;
};

/* The Details section (UX-135): its toggle, and the card of values below it. */
class TswDetails : public QWidget {
	Q_OBJECT

public:
	TswDetails(QWidget *parent, const TswTheme *theme, TswText lookup);
	void showView(const tsw_dock_view &view);
	void setOpen(bool open); /* only from the saved choice; the dock never opens it by itself */
	bool isOpen() const;
	void restyle(bool narrow); /* fonts, colors and spacing from the theme */
	TswDisclosure *toggle() const;

signals:
	void openChanged(bool open); /* the user opened or closed it: worth remembering */

private:
	struct Row {
		const char *key; /* the label's locale key */
		QLabel *label;
		QLabel *value;
	};
	void buildRows();
	void addRow(const char *key, int group);
	void setValue(int row, const QString &value);
	void styleValue(QLabel *value) const;

	const TswTheme *theme;
	TswText lookup;
	TswDisclosure *disclosure = nullptr;
	TswCard *card = nullptr;
	QVBoxLayout *cardLayout = nullptr;
	QVector<QGridLayout *> groups;
	QVector<Row> rows;
	QLabel *mapCaption = nullptr;
	TswSpeedMap *speedMap = nullptr;
	QVector<QWidget *> rules;  /* hairlines between the groups */
	QWidget *mapBox = nullptr; /* the map's caption and strip, close together */
};

/* The menu button's side, px: square, as tall as the card's round dismiss button. */
int tswMenuButtonSize(const TswTheme &theme);
/* The room between the button's edge and its glyph, px: it may overhang padding, the dots never do. */
int tswMenuButtonInset(const TswTheme &theme);

/* The menu button (UX-138): a "⋯" in a circle that shows only on hover, focus or press; it sits in the
 * status card's top corner and takes the color of what is under it. */
class TswMenuButton : public QToolButton {
	Q_OBJECT

public:
	TswMenuButton(QWidget *parent, const TswTheme *theme);
	void setUnder(const QColor &color); /* what it sits on: the status card's fill, or its band */
	QColor glyphColor() const;          /* for the UI test's contrast check */
	QColor underColor() const;

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	const TswTheme *theme;
	QColor under;
};
