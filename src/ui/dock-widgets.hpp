/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * dock-widgets.hpp: the painted building blocks of the dock, "Studio cards" direction
 * (docs/design/visual-design.md, 3.9 and 4.2).
 *
 * One idea runs through all of them: outlined means next, filled means now. Every shape is
 * painted here from the theme's tokens (dock-theme.hpp), so the shapes are the same under every
 * OBS theme while the colors are each theme's own:
 *
 *   TswStatusCard   zones A and B: tally pill, hero, speed badge, readout, status line
 *   TswButton       main button, Stop, card pills and the round dismiss button
 *   TswRail         a pill track with a sliding thumb, holding TswSegments (real buttons)
 *   TswCard         the summary and error cards
 *   TswLink         Copy details, Learn more
 *   TswGlyphIcon    a glyph on its own (the context line)
 *   TswReservedLabel a label as tall as the tallest text it may show, so nothing moves
 *
 * None of them knows OBS; they only read the TswTheme the dock owns.
 */

#pragma once

#include <QAbstractButton>
#include <QLabel>
#include <QStringList>
#include <QToolButton>
#include <QVariantAnimation>
#include <QVector>

#include "core/dock-view.h"
#include "ui/dock-glyph.hpp"
#include "ui/dock-theme.hpp"

class QButtonGroup;

/* A label whose height is the tallest of the texts it may show at this width (UX-002). */
class TswReservedLabel : public QLabel {
	Q_OBJECT

public:
	explicit TswReservedLabel(QWidget *parent);
	void setReserve(const QStringList &texts);
	int heightForWidth(int width) const override;
	QSize minimumSizeHint() const override;

private:
	QStringList reserve;
};

/* A glyph alone, as tall as a line of the text next to it. */
class TswGlyphIcon : public QWidget {
	Q_OBJECT

public:
	explicit TswGlyphIcon(QWidget *parent);
	void setGlyph(TswGlyph glyph, const QColor &color, const QSize &box); /* drawn centered in box */

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	TswGlyph glyph = TswGlyph::None;
	QColor color;
};

/* What zones A and B show now; the dock fills it from the view and the locale. */
struct TswStatusContent {
	enum tsw_banner banner = TSW_BANNER_READY;
	QString tally;          /* READY, REC, ... */
	QString hero;           /* REAL TIME, or a quiet sentence */
	bool quiet = true;      /* quiet heroes are smaller and Regular */
	bool boldQuiet = false; /* the error headline */
	QString speed;          /* the badge in timelapse: "×4"; else empty */
	TswGlyph heroGlyph = TswGlyph::None;
	QString recordedCaption; /* RECORDED */
	QString finalCaption;    /* FINAL VIDEO */
	QString recorded;        /* 0:31:40 */
	QString finalLength;     /* 0:16:58 */
	bool dimmed = true;      /* idle zeros */
	QString stretchLabel;    /* "This stretch ×4", or empty */
	QString stretchValues;   /* "19:37 → 4:54" */
};

/* The texts zones A and B may ever show at this language, so their height never changes. */
struct TswStatusReserve {
	QString realTimeHero;    /* REAL TIME: the hero is fitted so both recording heroes fit, */
	QString timelapseHero;   /* TIMELAPSE, with the speed badge beside it when wide */
	QStringList quietHeroes; /* sentences, Regular */
	QStringList headlines;   /* error headlines, Bold */
	QString widestSpeed;     /* ×16 */
	QStringList tallies;
	QString stretchLabel; /* the longest status line, in two parts */
	QString stretchValues;
};

/* Zones A and B as one card: the state block above a dashed hairline, the readout below. */
class TswStatusCard : public QWidget {
	Q_OBJECT
	Q_PROPERTY(double tswHeroPoints READ heroPoints) /* for the UI test: the hero's painted size */
	Q_PROPERTY(bool tswClipped READ clipped)         /* for the UI test: a painted text did not fit */

public:
	TswStatusCard(QWidget *parent, const TswTheme *theme);
	void setContent(const TswStatusContent &content, bool animate);
	void setReserve(const TswStatusReserve &reserve);
	void setNarrow(bool narrow);
	bool hasHeightForWidth() const override;
	int heightForWidth(int width) const override;
	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;
	double heroPoints() const;
	bool clipped() const;

	struct Layout; /* fonts and rows for one width (dock-widgets.cpp) */

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	Layout measure(int width) const;
	void measureStateBlock(Layout &layout) const;
	void measureReadout(Layout &layout) const;
	void paintFrame(QPainter &painter) const;
	void paintTally(QPainter &painter, const Layout &layout) const;
	void paintHero(QPainter &painter, const Layout &layout) const;
	void paintBadge(QPainter &painter, const QRectF &box, const Layout &layout) const;
	double pillWidth(const Layout &layout) const;
	void paintReadout(QPainter &painter, const Layout &layout) const;
	void paintStretch(QPainter &painter, const Layout &layout, double top) const;
	QColor fillColor() const;

	const TswTheme *theme;
	TswStatusContent content;
	TswStatusReserve reserve;
	bool narrow = false;
	enum tsw_banner previous = TSW_BANNER_READY; /* for the cross-fade */
	QVariantAnimation fade;                      /* 0 = previous state's colors, 1 = this one's */
	QVariantAnimation sweep;                     /* saving: the line that sweeps for at most 5 s */
};

/* How a TswButton is painted; the dock picks one per action (4.2, main button table). */
struct TswButtonLook {
	QColor face;  /* invalid: no fill */
	QColor under; /* what the button sits on: the hover fill of a button without face */
	QColor frame; /* invalid: no frame */
	double frameWidth = 0.0;
	QColor glyph;
	QColor label;
};

/* A painted button with a glyph and a label that wraps to two lines (UX-004, UX-121). Its own
 * text() stays empty: the accessible name carries the words. */
class TswButton : public QAbstractButton {
	Q_OBJECT

public:
	enum class Shape { Rounded, Pill };
	TswButton(QWidget *parent, const TswTheme *theme);
	void setContent(TswGlyph glyph, const QString &label);
	void setReserve(const QStringList &labels);
	void setLook(const TswButtonLook &look);
	void setShape(Shape shape);
	void setLabelFont(const QFont &font);
	void setMinimumRow(int height);
	void setHoldProgress(double progress); /* Stop's hold fill (UX-052), 0 to 1 */
	bool hasHeightForWidth() const override;
	int heightForWidth(int width) const override;
	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	int glyphSize() const;
	int lineCount(const QString &text, int width) const;
	double radius() const;
	void paintFace(QPainter &painter, const QRectF &box) const;
	void paintContent(QPainter &painter, QRectF box) const;

	const TswTheme *theme;
	TswGlyph glyph = TswGlyph::None;
	QString label;
	QStringList reserve;
	TswButtonLook look;
	Shape shape = Shape::Rounded;
	QFont labelFont;
	int minimumRow = 0;
	double hold = 0.0;
};

/* A text link with a glyph: Copy details, Learn more (accentText, underlined on hover and focus). */
class TswLink : public QAbstractButton {
	Q_OBJECT

public:
	TswLink(QWidget *parent, const TswTheme *theme, TswGlyph glyph);
	void setLabel(const QString &text);
	void setLinkFont(const QFont &font);
	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	const TswTheme *theme;
	TswGlyph glyph;
	QFont linkFont;
};

class TswRail;

/* One segment of a rail: a real button with a name, painting only its label (the rail paints the
 * track and the thumb under it). Arrow keys move focus without selecting (UX-102). */
class TswSegment : public QToolButton {
	Q_OBJECT

public:
	TswSegment(TswRail *rail, TswGlyph glyph, const QString &label);
	void setLabel(const QString &label);
	bool stacksAt(int width) const; /* the label does not fit beside its glyph at this width */
	int stackedHeight() const;
	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;

protected:
	void paintEvent(QPaintEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;

private:
	QColor labelColor() const;
	QFont labelFont() const;

	TswRail *rail;
	TswGlyph glyph;
	QString label;
};

/* A pill track with a thumb under the selected segment; the thumb slides when it changes. */
class TswRail : public QWidget {
	Q_OBJECT

public:
	enum class Mode { Armed, Engaged, Start }; /* outlined next, filled now, or the start mode */
	TswRail(QWidget *parent, const TswTheme *theme);
	TswSegment *addSegment(int id, TswGlyph glyph, const QString &label);
	QButtonGroup *group() const;
	void setMode(Mode mode);
	Mode mode() const;
	void select(int id, bool animate);
	void moveFocus(TswSegment *from, int step);
	const TswTheme *theme() const;
	bool stacksAt(int width) const; /* labels go under their glyphs at this width */
	bool isStacked() const;
	bool hasHeightForWidth() const override;
	int heightForWidth(int width) const override;
	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;

protected:
	void paintEvent(QPaintEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void changeEvent(QEvent *event) override;

private:
	QRectF segmentBox(int index) const;
	void layoutSegments();
	void updateFocusPolicies();

	const TswTheme *style;
	QButtonGroup *buttons;
	QVector<TswSegment *> segments;
	Mode current = Mode::Armed;
	int selected = -1;       /* index in segments */
	QVariantAnimation slide; /* the thumb's left edge, from the old segment to the new one */
};

/* A soft card: surface fill, hairline frame, rounded corners. Its content is a normal layout. */
class TswCard : public QWidget {
	Q_OBJECT

public:
	TswCard(QWidget *parent, const TswTheme *theme);

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	const TswTheme *theme;
};
