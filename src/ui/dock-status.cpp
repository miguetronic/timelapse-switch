/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * dock-status.cpp: TswStatusCard, zones A and B of the dock as one painted card
 * (docs/design/visual-design.md, 4.2, "Status card").
 *
 *   .--------------------------------------.
 *   | ( (o) REC )                          |  tally pill: am I recording?
 *   | >> TIMELAPSE                ( x4 )   |  hero and speed badge: at what speed?
 *   | - - - - - - - - - - - - - - - - - -  |  dashed hairline
 *   | RECORDED          FINAL VIDEO        |  captions
 *   | 0:31:40           0:16:58            |  digits
 *   | This stretch x4   19:37 -> 4:54      |  status line
 *   '--------------------------------------'
 *
 * Its height depends only on the width, the language and the theme, never on the state, so the
 * main button below it never moves (UX-002): every row is as tall as the tallest thing it may
 * ever show (the reserve).
 */

#include <QEasingCurve>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

#include "ui/dock-paint.hpp"
#include "ui/dock-widgets.hpp"

namespace {

const int FADE_MS = 180;         /* fill and frame cross-fade on a change of state (4.2, motion) */
const int SWEEP_MS = 1600;       /* one pass of the saving line */
const int SWEEP_PASSES = 3;      /* 4.8 s in all: a repeating motion ends within 5 s (UX-063) */
const double SWEEP_SHARE = 0.3;  /* the moving part is this share of the line */
const double HERO_STEP = 0.5;    /* pt: the hero is fitted in half points */
const int MAX_FIT_STEPS = 64;    /* from the target down to the floor, at any base size (P10-2) */
const double CARD_RADIUS = 0.45; /* x L, at least MIN_CARD_RADIUS (4.2, shapes) */
const int MIN_CARD_RADIUS = 6;
const double PILL_GLYPH = 0.55; /* the tally glyph's share of its pill's height */
const double DASH = 2.0;        /* the hairline between A and B: dash 2 px, gap 3 px */
const double DASH_GAP = 3.0;

/* The card's own fill and frame in one state (4.2, status card table). */
struct CardPaint {
	QColor fill;
	QColor frame;
	double width;
	bool dashed;
};

CardPaint cardPaint(const TswTheme &theme, enum tsw_banner banner)
{
	if (banner == TSW_BANNER_TIMELAPSE)
		return {theme.accentTint, theme.accent, 2.0, false}; /* filled means now */
	if (banner == TSW_BANNER_ERROR)
		return {theme.dangerTint, theme.danger, 2.0, true};
	return {theme.surface, theme.hairline, 1.0, false};
}

/* The tally pill in one state: what fills it, what outlines it, its glyph and their colors. */
struct PillPaint {
	QColor fill;
	QColor frame;
	TswGlyph glyph;
	QColor glyphColor;
	QColor textColor;
};

PillPaint pillPaint(const TswTheme &t, enum tsw_banner banner, const QColor &under)
{
	const QColor quiet = t.ink(t.muted, under);
	switch (banner) {
	case TSW_BANNER_REAL_TIME:
	case TSW_BANNER_TIMELAPSE: /* the tally: a red dot on a soft red pill, in both recording states */
		return {t.dangerTint, QColor(), TswGlyph::Record, t.shape(t.danger, t.dangerTint),
			t.ink(t.text, t.dangerTint)};
	case TSW_BANNER_BUSY:
		return {QColor(), t.shape(t.muted, under), TswGlyph::Busy, quiet, quiet};
	case TSW_BANNER_SAVED:
		return {QColor(), t.shape(t.success, under), TswGlyph::Check, t.shape(t.success, under), t.text};
	case TSW_BANNER_ERROR:
		return {t.surface, QColor(), TswGlyph::Error, t.shape(t.danger, t.surface), t.text};
	case TSW_BANNER_READY:
		break;
	}
	return {QColor(), t.shape(t.muted, under), TswGlyph::None, quiet, quiet};
}

QPen dashedPen(const QColor &color, double width, double dash, double gap)
{
	QPen pen(color, width);
	pen.setDashPattern({dash / width, gap / width}); /* Qt measures dashes in pen widths */
	pen.setCapStyle(Qt::FlatCap);
	return pen;
}

double cap(const QFont &font)
{
	return QFontMetricsF(font).capHeight();
}

} // namespace

struct TswStatusCard::Layout {
	int height = 0;
	int pad = 0;
	double inner = 0.0; /* the width of the content */
	QFont tallyFont, heroFont, quietFont, headlineFont, badgeFont, captionFont, digitsFont, statusFont;
	double heroPoints = 0.0;
	int heroGlyph = 0;  /* px: the glyph before a recording hero, and the column it takes */
	int quietGlyph = 0; /* px: the glyph before an idle hero */
	double pillHeight = 0.0;
	double badgeWidth = 0.0;
	double badgeHeight = 0.0;
	bool badgeOnTally = false; /* narrow: the speed joins the tally line (4.2, narrow) */
	double tallyTop = 0.0, tallyHeight = 0.0;
	double heroTop = 0.0, heroHeight = 0.0;
	double dividerY = 0.0;
	double readoutTop = 0.0;
	double captionHeight = 0.0, digitsHeight = 0.0, statusHeight = 0.0;
	bool stacked = false; /* the two columns do not fit side by side: one under the other */
	double column = 0.0;  /* the width of one column */
	int statusLines = 1;
	bool clipped = false;
};

TswStatusCard::TswStatusCard(QWidget *parent, const TswTheme *style) : QWidget(parent), theme(style)
{
	setObjectName("tswStatus");
	/* Not Fixed: Qt would cap the height at sizeHint's, measured at another width. The height always
	 * comes from heightForWidth, and the layout's stretch keeps the extra room below. */
	QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	policy.setHeightForWidth(true);
	setSizePolicy(policy);
	fade.setDuration(FADE_MS);
	fade.setEasingCurve(QEasingCurve::OutCubic);
	fade.setStartValue(0.0);
	fade.setEndValue(1.0);
	sweep.setDuration(SWEEP_MS);
	sweep.setLoopCount(SWEEP_PASSES);
	sweep.setStartValue(0.0);
	sweep.setEndValue(1.0);
	connect(&fade, &QVariantAnimation::valueChanged, this, [this]() { update(); });
	connect(&sweep, &QVariantAnimation::valueChanged, this, [this]() { update(); });
	connect(&sweep, &QVariantAnimation::finished, this, [this]() { update(); });
}

void TswStatusCard::setContent(const TswStatusContent &next, bool animate)
{
	const bool changed = next.banner != content.banner;
	if (changed) {
		previous = content.banner;
		fade.stop();
		sweep.stop();
		if (animate)
			fade.start();
		if (animate && next.banner == TSW_BANNER_BUSY)
			sweep.start(); /* a first view shows the line still: nothing was caused yet */
	}
	content = next;
	update();
}

void TswStatusCard::setReserve(const TswStatusReserve &texts)
{
	reserve = texts;
	updateGeometry();
	update();
}

void TswStatusCard::setNarrow(bool isNarrow)
{
	if (narrow == isNarrow)
		return;
	narrow = isNarrow;
	updateGeometry();
	update();
}

/* The largest hero size, from the target down to the floor, at which both recording heroes fit:
 * REAL TIME alone, TIMELAPSE with the speed badge beside it (besides, 0 when the badge is on the
 * tally line). */
static double fitHero(const TswTheme &theme, const TswStatusReserve &reserve, double room, double besides, bool *fits)
{
	const double target = theme.pointsAt(TswType::HERO);
	const double floor = theme.pointsAt(TswType::HERO_FLOOR);
	double points = target;
	for (int step = 0; step < MAX_FIT_STEPS && points >= floor; step++) {
		QFont font = theme.fontAt(1.0, true);
		font.setPointSizeF(points);
		const double lead = std::round(cap(font)) + theme.s3; /* the glyph and its gap */
		const double realTime = lead + tswTextWidth(font, reserve.realTimeHero);
		const double timelapse = lead + tswTextWidth(font, reserve.timelapseHero) + besides;
		if (std::max(realTime, timelapse) <= room) {
			*fits = true;
			return points;
		}
		points -= HERO_STEP;
	}
	*fits = false;
	return floor;
}

/* Steps 1 to 3 of measure: the badge, the fitted hero, and the rows of zone A. */
void TswStatusCard::measureStateBlock(Layout &lay) const
{
	const TswTheme &t = *theme;

	/* 1. The speed badge, as wide as the widest speed so the hero line never shifts. */
	lay.badgeHeight = std::round(cap(lay.badgeFont) + 2 * t.s3);
	lay.badgeWidth =
		std::max(lay.badgeHeight, std::ceil(tswTextWidth(lay.badgeFont, reserve.widestSpeed)) + 2 * t.s3);
	lay.badgeOnTally = narrow;

	/* 2. The hero: the largest size at which REAL TIME and TIMELAPSE (and the badge) fit. */
	bool fits = true;
	const double besides = lay.badgeOnTally ? 0.0 : t.s3 + lay.badgeWidth;
	lay.heroPoints = fitHero(t, reserve, lay.inner, besides, &fits);
	lay.heroFont = t.fontAt(1.0, true);
	lay.heroFont.setPointSizeF(lay.heroPoints);
	lay.heroGlyph = static_cast<int>(std::round(cap(lay.heroFont)));
	lay.quietGlyph = tswGlyphSize(lay.quietFont);
	lay.clipped = !fits;

	/* 3. Rows: each as tall as the tallest thing it may show at this width. */
	lay.pillHeight = std::round(cap(lay.tallyFont) + 2 * t.s2 + 2);
	lay.tallyHeight = lay.badgeOnTally ? std::max(lay.pillHeight, lay.badgeHeight) : lay.pillHeight;
	double hero = std::round(cap(lay.heroFont)) + 2;
	if (!lay.badgeOnTally)
		hero = std::max(hero, lay.badgeHeight);
	const int quietRoom = static_cast<int>(lay.inner) - lay.heroGlyph - t.s3;
	for (const QString &text : reserve.quietHeroes)
		hero = std::max(hero, double(tswWrappedHeight(lay.quietFont, text, quietRoom)));
	for (const QString &text : reserve.headlines)
		hero = std::max(hero, double(tswWrappedHeight(lay.headlineFont, text, static_cast<int>(lay.inner))));
	lay.heroHeight = hero;
}

/* Step 4 of measure: two columns, unless a caption or the digits do not fit beside each other. */
void TswStatusCard::measureReadout(Layout &lay) const
{
	const TswTheme &t = *theme;
	lay.column = (lay.inner - t.s4) / 2.0;
	const double captions = std::max(tswTextWidth(lay.captionFont, content.recordedCaption),
					 tswTextWidth(lay.captionFont, content.finalCaption));
	const double digits = tswTextWidth(lay.digitsFont, QStringLiteral("0:00:00"));
	lay.stacked = captions > lay.column || digits > lay.column;
	if (lay.stacked)
		lay.column = lay.inner;
	lay.clipped = lay.clipped || captions > lay.inner || digits > lay.inner;
	lay.captionHeight = std::round(cap(lay.captionFont)) + 2;
	lay.digitsHeight = std::round(cap(lay.digitsFont)) + 2;
	lay.statusHeight = QFontMetricsF(lay.statusFont).height();
	const double oneLine = tswTextWidth(lay.statusFont, reserve.stretchLabel + "  " + reserve.stretchValues);
	lay.statusLines = oneLine <= lay.inner ? 1 : 2;
}

/* Zones A and B for one width: fonts, rows and whether anything cannot fit. */
TswStatusCard::Layout TswStatusCard::measure(int width) const
{
	const TswTheme &t = *theme;
	Layout lay;
	lay.pad = narrow ? t.s3 : t.s4;
	lay.inner = std::max(1, width - 2 * lay.pad);
	lay.tallyFont = t.fontAt(TswType::TALLY, true);
	lay.quietFont = t.fontAt(TswType::QUIET, false);
	lay.headlineFont = t.fontAt(TswType::QUIET, true);
	lay.badgeFont = t.fontAt(TswType::BADGE, true);
	lay.captionFont = t.fontAt(TswType::CAPTION, true);
	lay.digitsFont = t.fontAt(TswType::DIGITS, true);
	lay.statusFont = t.fontAt(TswType::STATUS, false);
	measureStateBlock(lay);
	measureReadout(lay);

	/* 5. Top to bottom. */
	const int pair = static_cast<int>(lay.captionHeight + t.s2 + lay.digitsHeight);
	const double readout = (lay.stacked ? 2 * pair + t.s3 : pair) + t.s3 + lay.statusLines * lay.statusHeight;
	lay.tallyTop = lay.pad;
	lay.heroTop = lay.tallyTop + lay.tallyHeight + t.s3;
	lay.dividerY = lay.heroTop + lay.heroHeight + t.s3;
	lay.readoutTop = lay.dividerY + 1 + t.s3;
	lay.height = static_cast<int>(std::ceil(lay.readoutTop + readout + lay.pad));
	return lay;
}

bool TswStatusCard::hasHeightForWidth() const
{
	return true;
}

int TswStatusCard::heightForWidth(int width) const
{
	return measure(width).height;
}

QSize TswStatusCard::sizeHint() const
{
	return {300, heightForWidth(300)};
}

QSize TswStatusCard::minimumSizeHint() const
{
	return {0, 0}; /* the height comes from heightForWidth at the width the layout gives */
}

double TswStatusCard::heroPoints() const
{
	return measure(width()).heroPoints;
}

bool TswStatusCard::clipped() const
{
	const Layout lay = measure(width());
	if (lay.clipped)
		return true;
	/* What shows now must fit too: the tally pill, and the badge beside it when narrow. */
	const bool badge = lay.badgeOnTally && !content.speed.isEmpty();
	return pillWidth(lay) + (badge ? theme->s3 + lay.badgeWidth : 0.0) > lay.inner;
}

/* The tally pill: padding, glyph, gap, word, padding. */
double TswStatusCard::pillWidth(const Layout &lay) const
{
	const bool glyph = pillPaint(*theme, content.banner, theme->surface).glyph != TswGlyph::None;
	const double glyphWidth = glyph ? std::round(lay.pillHeight * PILL_GLYPH) + theme->s2 : 0.0;
	return std::ceil(2 * theme->s3 + glyphWidth + tswTextWidth(lay.tallyFont, content.tally));
}

QColor TswStatusCard::fillColor() const
{
	const QColor now = cardPaint(*theme, content.banner).fill;
	if (fade.state() != QAbstractAnimation::Running)
		return now;
	return tswMix(now, cardPaint(*theme, previous).fill, fade.currentValue().toDouble());
}

void TswStatusCard::paintFrame(QPainter &painter) const
{
	const CardPaint now = cardPaint(*theme, content.banner);
	QColor frame = now.frame;
	if (fade.state() == QAbstractAnimation::Running)
		frame = tswMix(now.frame, cardPaint(*theme, previous).frame, fade.currentValue().toDouble());
	const double radius = std::max(MIN_CARD_RADIUS, static_cast<int>(std::lround(theme->L * CARD_RADIUS)));
	const QPen pen = now.dashed ? dashedPen(frame, now.width, 2 * DASH, DASH_GAP) : tswBorder(frame, now.width);
	const QRectF box(rect());
	tswPaintRounded(painter, box, radius, fillColor(), pen);
	if (content.banner != TSW_BANNER_BUSY)
		return;

	/* Saving: a 2 px accent line along the bottom inner edge; it sweeps, then stays still. */
	const double y = box.bottom() - 1.0 - 2.0;
	const double left = radius;
	const double length = box.width() - 2 * radius;
	double from = left;
	double span = length;
	if (sweep.state() == QAbstractAnimation::Running) {
		const double at = sweep.currentValue().toDouble();
		span = length * SWEEP_SHARE;
		from = left + (length + span) * at - span;
	}
	painter.save();
	painter.setClipRect(QRectF(left, y, length, 2.0));
	painter.fillRect(QRectF(from, y, span, 2.0), theme->accent);
	painter.restore();
}

void TswStatusCard::paintBadge(QPainter &painter, const QRectF &box, const Layout &lay) const
{
	/* The speed running now: the one filled accent pill (filled means now). */
	tswPaintRounded(painter, box, box.height() / 2.0, theme->accentFill, tswBorder(theme->accent, 2.0));
	painter.setFont(lay.badgeFont);
	painter.setPen(theme->onAccent);
	const double width = tswTextWidth(lay.badgeFont, content.speed);
	painter.drawText(QPointF(box.center().x() - width / 2.0,
				 tswCapBaseline(lay.badgeFont, box.top(), box.height())),
			 content.speed);
}

void TswStatusCard::paintTally(QPainter &painter, const Layout &lay) const
{
	const QColor under = fillColor();
	const PillPaint pill = pillPaint(*theme, content.banner, under);
	const int glyph = pill.glyph == TswGlyph::None ? 0 : static_cast<int>(std::round(lay.pillHeight * PILL_GLYPH));
	const double gap = glyph > 0 ? theme->s2 : 0.0;
	const double top = lay.tallyTop + (lay.tallyHeight - lay.pillHeight) / 2.0;
	const QRectF box(lay.pad, top, pillWidth(lay), lay.pillHeight);
	tswPaintRounded(painter, box, box.height() / 2.0, pill.fill, tswBorder(pill.frame, 1.0));
	const QRectF glyphBox(box.left() + theme->s3, box.center().y() - glyph / 2.0, glyph, glyph);
	tswPaintGlyph(painter, pill.glyph, glyphBox, pill.glyphColor);
	painter.setFont(lay.tallyFont);
	painter.setPen(pill.textColor);
	painter.drawText(QPointF(glyphBox.right() + gap, tswCapBaseline(lay.tallyFont, box.top(), box.height())),
			 content.tally);
	if (lay.badgeOnTally && !content.speed.isEmpty()) {
		const double badgeTop = lay.tallyTop + (lay.tallyHeight - lay.badgeHeight) / 2.0;
		paintBadge(painter,
			   QRectF(lay.pad + lay.inner - lay.badgeWidth, badgeTop, lay.badgeWidth, lay.badgeHeight),
			   lay);
	}
}

void TswStatusCard::paintHero(QPainter &painter, const Layout &lay) const
{
	const QColor under = fillColor();
	const QRectF row(lay.pad, lay.heroTop, lay.inner, lay.heroHeight);
	if (content.quiet) {
		/* Quiet heroes: sentence case, smaller, Regular (Bold for an error headline), wrapping. */
		const bool glyph = content.heroGlyph != TswGlyph::None;
		const double left = glyph ? lay.heroGlyph + theme->s3 : 0.0;
		const QFont font = content.boldQuiet ? lay.headlineFont : lay.quietFont;
		const int room = static_cast<int>(row.width() - left);
		const int textHeight = tswWrappedHeight(font, content.hero, room);
		const double top = std::round(row.top() + (row.height() - textHeight) / 2.0); /* centered in its row */
		const QRectF text(row.left() + left, top, tswBalancedWidth(font, content.hero, room), textHeight);
		const double firstLine = text.top() + QFontMetricsF(font).height() / 2.0;
		const QRectF glyphBox(row.left(), firstLine - lay.quietGlyph / 2.0, lay.quietGlyph, lay.quietGlyph);
		tswPaintGlyph(painter, content.heroGlyph, glyphBox, theme->ink(theme->muted, under));
		painter.setFont(font);
		painter.setPen(theme->ink(theme->text, under));
		painter.drawText(text, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, content.hero);
		return;
	}
	/* Recording heroes: big capitals, a glyph before them, and in timelapse the speed badge. */
	const bool timelapse = content.banner == TSW_BANNER_TIMELAPSE;
	const QColor glyphColor = timelapse ? theme->ink(theme->accent, under) : theme->ink(theme->text, under);
	const double middle = row.center().y();
	tswPaintGlyph(painter, content.heroGlyph,
		      QRectF(row.left(), middle - lay.heroGlyph / 2.0, lay.heroGlyph, lay.heroGlyph), glyphColor);
	painter.setFont(lay.heroFont);
	painter.setPen(theme->ink(theme->text, under));
	painter.drawText(QPointF(row.left() + lay.heroGlyph + theme->s3,
				 tswCapBaseline(lay.heroFont, row.top(), row.height())),
			 content.hero);
	if (!lay.badgeOnTally && !content.speed.isEmpty()) {
		const QRectF badge(row.right() - lay.badgeWidth, middle - lay.badgeHeight / 2.0, lay.badgeWidth,
				   lay.badgeHeight);
		paintBadge(painter, badge, lay);
	}
}

void TswStatusCard::paintStretch(QPainter &painter, const Layout &lay, double top) const
{
	if (content.stretchLabel.isEmpty())
		return;
	const QColor under = fillColor();
	const QFontMetricsF metrics(lay.statusFont);
	painter.setFont(lay.statusFont);
	painter.setPen(theme->ink(theme->muted, under));
	painter.drawText(QPointF(lay.pad, top + metrics.ascent()), content.stretchLabel);
	const double labelWidth = metrics.horizontalAdvance(content.stretchLabel + "  ");
	const bool oneLine = lay.statusLines == 1 ||
			     labelWidth + metrics.horizontalAdvance(content.stretchValues) <= lay.inner;
	const QPointF values = oneLine ? QPointF(lay.pad + labelWidth, top + metrics.ascent())
				       : QPointF(lay.pad, top + lay.statusHeight + metrics.ascent());
	painter.setPen(theme->ink(theme->text, under));
	painter.drawText(values, content.stretchValues);
}

void TswStatusCard::paintReadout(QPainter &painter, const Layout &lay) const
{
	const QColor under = fillColor();
	const double pair = lay.captionHeight + theme->s2 + lay.digitsHeight;
	const double secondLeft = lay.stacked ? lay.pad : lay.pad + lay.column + theme->s4;
	const double secondTop = lay.stacked ? lay.readoutTop + pair + theme->s3 : lay.readoutTop;
	const QColor digits = content.dimmed ? theme->ink(theme->muted, under) : theme->ink(theme->text, under);
	/* In timelapse the counter that grows slower carries the timelapse color (4.2). */
	const QColor finalDigits = content.banner == TSW_BANNER_TIMELAPSE ? theme->ink(theme->accent, under) : digits;
	const struct {
		double left;
		double top;
		const QString &caption;
		const QString &value;
		QColor color;
	} columns[] = {{double(lay.pad), lay.readoutTop, content.recordedCaption, content.recorded, digits},
		       {secondLeft, secondTop, content.finalCaption, content.finalLength, finalDigits}};
	for (const auto &column : columns) {
		painter.setFont(lay.captionFont);
		painter.setPen(theme->ink(theme->muted, under));
		painter.drawText(QPointF(column.left, tswCapBaseline(lay.captionFont, column.top, lay.captionHeight)),
				 column.caption);
		const double digitsTop = column.top + lay.captionHeight + theme->s2;
		painter.setFont(lay.digitsFont);
		painter.setPen(column.color);
		painter.drawText(QPointF(column.left, tswCapBaseline(lay.digitsFont, digitsTop, lay.digitsHeight)),
				 column.value);
	}
	paintStretch(painter, lay, secondTop + pair + theme->s3);
}

void TswStatusCard::paintEvent(QPaintEvent *event)
{
	(void)event;
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setRenderHint(QPainter::TextAntialiasing);
	const Layout lay = measure(width());
	paintFrame(painter);
	paintTally(painter, lay);
	paintHero(painter, lay);
	painter.setPen(dashedPen(tswMix(theme->text, fillColor(), 0.22), 1.0, DASH, DASH_GAP));
	painter.drawLine(QPointF(lay.pad, lay.dividerY + 0.5), QPointF(lay.pad + lay.inner, lay.dividerY + 0.5));
	paintReadout(painter, lay);
}
