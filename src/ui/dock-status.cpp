/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * dock-status.cpp: TswStatusCard, zones A and B of the dock as one painted card
 * (docs/design/visual-design.md, 4.2, "Status card").
 *
 *   .======================================.
 *   |######################################|  timelapse: a solid band in the accent
 *   |# ( (o) REC )                        #|  tally pill: am I recording?
 *   |# >> TIMELAPSE               ( x4 )  #|  hero and speed badge: at what speed?
 *   |######################################|
 *   | RECORDED          | FINAL VIDEO      |  captions; the bar marks the slower counter
 *   | 0:31:40           | 0:16:58          |  digits
 *   | This stretch x4   19:37 -> 4:54      |  status line
 *   | > > > > > > > > > > > > > > > > > >  |  tape band: denser chevrons, faster
 *   '======================================'
 *
 * Each state has its own shape, so it reads in grayscale and from across the room (UX-003): idle
 * a soft filled card, real time a hollow card with a 2 px outline, timelapse a tinted card with a
 * solid band and chevrons, saving a dashed outline, error a dashed danger frame.
 *
 * Its height depends only on the width, the language and the theme, never on the state, so the
 * main button below it never moves (UX-002): every row is as tall as the tallest thing it may
 * ever show (the reserve). Nothing in it moves: a state changes at once (4.2, motion).
 */

#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

#include "ui/dock-paint.hpp"
#include "ui/dock-widgets.hpp"

namespace {

const double HERO_STEP = 0.5;   /* pt: the hero is fitted in half points */
const int MAX_FIT_STEPS = 64;   /* from the target down to the floor, at any base size (P10-2) */
const double PILL_GLYPH = 0.55; /* the tally glyph's share of its pill's height */
const double DASH = 2.0;        /* the hairline between A and B: dash 2 px, gap 3 px */
const double DASH_GAP = 3.0;
const double DIVIDER_INK = 0.3;          /* the dashed hairline: text over the card's fill at this strength */
const double TAPE = 0.7;                 /* x L: the tape band's height, and the size of each mark */
const double PLAY_PITCH = 3.0;           /* real time: a play mark every 3 tape heights, sparse and calm */
const int MAX_MARKS = 512;               /* tape marks: more than any dock is wide, at the densest pitch (P10-2) */
const double FINAL_BAR = 3.0;            /* px: the accent bar beside FINAL VIDEO in timelapse */
const double BUSY_LINE = 2.0;            /* px: saving draws one still accent line on the tape */
const double CHEVRON_INSET = 5.0 / 16.0; /* the chevron glyph starts this far into its box (3.8) */

/* The card's own fill and frame in one state (4.2, status card table). */
struct CardPaint {
	QColor fill;
	QColor frame;
	double width;
	bool dashed;
};

CardPaint cardPaint(const TswTheme &t, enum tsw_banner banner)
{
	switch (banner) {
	case TSW_BANNER_REAL_TIME: /* hollow: the panel shows through a 2 px outline */
		return {t.panel, t.frame, 2.0, false};
	case TSW_BANNER_TIMELAPSE: /* filled means now */
		return {t.accentTint, t.accent, 2.0, false};
	case TSW_BANNER_BUSY: /* working: a dashed outline, so it never looks finished */
		return {t.surface, t.shape(t.edge, t.surface), 1.0, true};
	case TSW_BANNER_ERROR:
		return {t.dangerTint, t.danger, 2.0, true};
	case TSW_BANNER_READY:
	case TSW_BANNER_SAVED:
		break;
	}
	return {t.surface, t.hairline, 1.0, false};
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
	case TSW_BANNER_TIMELAPSE: /* the tally: a red dot on a soft red pill, the same in both recording states */
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

/* Chevron pitch by speed, in tape heights: denser means faster, readable without a word. */
double chevronPitch(uint32_t speed)
{
	if (speed >= 16)
		return 0.7;
	if (speed >= 8)
		return 0.9;
	if (speed >= 4)
		return 1.2;
	return 1.6;
}

} // namespace

struct TswStatusCard::Layout {
	int height = 0;
	int pad = 0;
	double inner = 0.0; /* the width of the content */
	QFont tallyFont, heroFont, quietFont, headlineFont, badgeFont, captionFont, digitsFont, statusFont;
	double heroPoints = 0.0;
	int heroGlyph = 0;          /* px: the glyph before a recording hero, and the column it takes */
	int quietGlyph = 0;         /* px: the glyph before an idle hero */
	bool heroGlyphShown = true; /* false only when a recording hero fits without its glyph alone */
	double pillHeight = 0.0;
	double badgeWidth = 0.0;
	double badgeHeight = 0.0;
	bool badgeOnTally = false; /* narrow: the speed joins the tally line (4.2, narrow) */
	double tallyTop = 0.0, tallyHeight = 0.0;
	double heroTop = 0.0, heroHeight = 0.0;
	double dividerY = 0.0; /* the dashed hairline; in timelapse, the bottom edge of the band */
	double readoutTop = 0.0;
	double captionHeight = 0.0, digitsHeight = 0.0, statusHeight = 0.0;
	bool stacked = false;    /* the two columns do not fit side by side: one under the other */
	double gutter = 0.0;     /* between the two columns */
	double secondLeft = 0.0; /* where the second column starts, from the card's left edge */
	double statusTop = 0.0;
	int statusLines = 1;
	double tapeTop = 0.0, tapeHeight = 0.0;
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
}

/* Field by field: the dock shows a view ten times per second, mostly the same one. */
static bool sameContent(const TswStatusContent &a, const TswStatusContent &b)
{
	return a.banner == b.banner && a.tally == b.tally && a.hero == b.hero && a.quiet == b.quiet &&
	       a.boldQuiet == b.boldQuiet && a.speed == b.speed && a.speedValue == b.speedValue &&
	       a.heroGlyph == b.heroGlyph && a.recordedCaption == b.recordedCaption &&
	       a.finalCaption == b.finalCaption && a.recorded == b.recorded && a.finalLength == b.finalLength &&
	       a.dimmed == b.dimmed && a.stretchLabel == b.stretchLabel && a.stretchValues == b.stretchValues;
}

void TswStatusCard::setContent(const TswStatusContent &next)
{
	if (sameContent(next, content))
		return; /* nothing to paint again */
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
 * tally line), each after its glyph when glyph is true. */
static double fitHero(const TswTheme &theme, const TswStatusReserve &reserve, double room, double besides, bool glyph,
		      bool *fits)
{
	const double target = theme.pointsAt(TswType::HERO);
	const double floor = theme.pointsAt(TswType::HERO_FLOOR);
	double points = target;
	for (int step = 0; step < MAX_FIT_STEPS && points >= floor; step++) {
		QFont font = theme.fontAt(1.0, true);
		font.setPointSizeF(points);
		const double lead = glyph ? std::round(cap(font)) + theme.s3 : 0.0; /* the glyph and its gap */
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
	lay.heroGlyphShown = true;
	lay.heroPoints = fitHero(t, reserve, lay.inner, besides, true, &fits);
	if (!fits) {
		/* The narrowest dock at the largest font: the word matters more than the glyph beside it. */
		lay.heroGlyphShown = false;
		lay.heroPoints = fitHero(t, reserve, lay.inner, besides, false, &fits);
	}
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

/*
 * Step 4 of measure: two equal columns; or, when one caption is longer than half the card
 * ("DURACIÓN FINAL" at 220 px), a first column just wide enough for its own text and the rest for
 * the second; or, when even that does not fit, one under the other.
 */
void TswStatusCard::measureReadout(Layout &lay) const
{
	const TswTheme &t = *theme;
	lay.gutter = t.s4; /* room for the timelapse bar between the columns, with air on both sides */
	const auto widths = [&](double *first, double *second) {
		const double digits = tswTextWidth(lay.digitsFont, QStringLiteral("0:00:00"));
		*first = std::ceil(std::max(tswTextWidth(lay.captionFont, content.recordedCaption), digits));
		*second = std::ceil(std::max(tswTextWidth(lay.captionFont, content.finalCaption), digits));
		return *first + lay.gutter + *second <= lay.inner;
	};
	/* Side by side, with digits shrunk toward their floor if they must; else one under the other,
	 * at full size. */
	double first = 0.0;
	double second = 0.0;
	const double target = lay.digitsFont.pointSizeF();
	const double floor = t.pointsAt(TswType::DIGITS_FLOOR);
	bool beside = widths(&first, &second);
	for (int step = 1; step < MAX_FIT_STEPS && !beside && target - step * HERO_STEP >= floor; step++) {
		lay.digitsFont.setPointSizeF(target - step * HERO_STEP);
		beside = widths(&first, &second);
	}
	if (!beside) {
		lay.digitsFont.setPointSizeF(target);
		widths(&first, &second);
	}
	const double half = (lay.inner - lay.gutter) / 2.0;
	lay.stacked = !beside;
	lay.secondLeft = lay.pad + (std::max(first, second) <= half ? half : first) + lay.gutter;
	if (lay.stacked)
		lay.secondLeft = lay.pad;
	lay.clipped = lay.clipped || std::max(first, second) > lay.inner;
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

	/* 5. Top to bottom: state block, hairline, readout, status line, tape. */
	const double pair = lay.captionHeight + t.s2 + lay.digitsHeight;
	lay.tallyTop = lay.pad;
	lay.heroTop = lay.tallyTop + lay.tallyHeight + t.s3;
	lay.dividerY = std::round(lay.heroTop + lay.heroHeight + t.s3);
	lay.readoutTop = lay.dividerY + 1 + t.s3;
	lay.statusTop = lay.readoutTop + (lay.stacked ? 2 * pair + t.s3 : pair) + t.s3;
	lay.tapeHeight = std::round(TAPE * t.L);
	lay.tapeTop = std::round(lay.statusTop + lay.statusLines * lay.statusHeight + t.s3);
	lay.height = static_cast<int>(std::ceil(lay.tapeTop + lay.tapeHeight + lay.pad));
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

double TswStatusCard::smallestPoints() const
{
	const Layout lay = measure(width());
	return std::min({lay.tallyFont.pointSizeF(), lay.captionFont.pointSizeF(), lay.statusFont.pointSizeF()});
}

QString TswStatusCard::statusLine() const
{
	return (content.stretchLabel + " " + content.stretchValues).trimmed();
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
	return cardPaint(*theme, content.banner).fill;
}

QColor TswStatusCard::stateColor() const
{
	return content.banner == TSW_BANNER_TIMELAPSE ? theme->accentFill : fillColor();
}

/* The card, and in timelapse the solid band behind the state block: the one filled accent area, so
 * the state reads by its shape from across the room (4.2). */
void TswStatusCard::paintFrame(QPainter &painter, const Layout &lay) const
{
	const CardPaint now = cardPaint(*theme, content.banner);
	const double radius = theme->corner;
	const QRectF box(rect());
	tswPaintRounded(painter, box, radius, now.fill, QPen(Qt::NoPen));
	if (content.banner == TSW_BANNER_TIMELAPSE) {
		painter.save();
		QPainterPath card;
		card.addRoundedRect(box, radius, radius);
		painter.setClipPath(card);
		painter.fillRect(QRectF(0, 0, box.width(), lay.dividerY + 1), theme->accentFill);
		painter.restore();
	}
	const QPen pen = now.dashed ? dashedPen(now.frame, now.width, 2 * DASH, DASH_GAP)
				    : tswBorder(now.frame, now.width);
	tswPaintRounded(painter, box, radius, QColor(), pen);
}

void TswStatusCard::paintBadge(QPainter &painter, const QRectF &box, const Layout &lay) const
{
	/* The speed running now, filled: the band's text turned into a pill, the number in the band's color. */
	tswPaintRounded(painter, box, box.height() / 2.0, theme->badgeFill, QPen(Qt::NoPen));
	painter.setFont(lay.badgeFont);
	painter.setPen(theme->badgeText);
	const double width = tswTextWidth(lay.badgeFont, content.speed);
	painter.drawText(QPointF(box.center().x() - width / 2.0,
				 tswCapBaseline(lay.badgeFont, box.top(), box.height())),
			 content.speed);
}

void TswStatusCard::paintTally(QPainter &painter, const Layout &lay) const
{
	const PillPaint pill = pillPaint(*theme, content.banner, stateColor());
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
	const QColor under = stateColor();
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
	const QColor ink = content.banner == TSW_BANNER_TIMELAPSE ? theme->onAccent : theme->ink(theme->text, under);
	const double middle = row.center().y();
	const double lead = lay.heroGlyphShown ? lay.heroGlyph + theme->s3 : 0.0;
	if (lay.heroGlyphShown)
		tswPaintGlyph(painter, content.heroGlyph,
			      QRectF(row.left(), middle - lay.heroGlyph / 2.0, lay.heroGlyph, lay.heroGlyph), ink);
	painter.setFont(lay.heroFont);
	painter.setPen(ink);
	painter.drawText(QPointF(row.left() + lead, tswCapBaseline(lay.heroFont, row.top(), row.height())),
			 content.hero);
	if (!lay.badgeOnTally && !content.speed.isEmpty()) {
		const QRectF badge(row.right() - lay.badgeWidth, middle - lay.badgeHeight / 2.0, lay.badgeWidth,
				   lay.badgeHeight);
		paintBadge(painter, badge, lay);
	}
}

void TswStatusCard::paintStretch(QPainter &painter, const Layout &lay) const
{
	if (content.stretchLabel.isEmpty())
		return;
	const QColor under = fillColor();
	const QFontMetricsF metrics(lay.statusFont);
	const double top = lay.statusTop;
	painter.setFont(lay.statusFont);
	painter.setPen(theme->ink(theme->muted, under));
	painter.drawText(QPointF(lay.pad, top + metrics.ascent()), content.stretchLabel);
	/* Two reserved lines are always both used, the label above its values, like the readout's
	 * captions: a short stretch on one line would leave the second as a hole. */
	const double labelWidth = metrics.horizontalAdvance(content.stretchLabel + "  ");
	const bool oneLine = lay.statusLines == 1;
	const QPointF values = oneLine ? QPointF(lay.pad + labelWidth, top + metrics.ascent())
				       : QPointF(lay.pad, top + lay.statusHeight + metrics.ascent());
	/* Values that are still too long (a file name) are shortened in the middle: the start and the
	 * extension both stay. */
	const double room = lay.pad + lay.inner - values.x();
	painter.setPen(content.dimmed ? theme->ink(theme->muted, under) : theme->ink(theme->text, under));
	painter.drawText(values, metrics.elidedText(content.stretchValues, Qt::ElideMiddle, room));
}

/* In timelapse the slower counter is marked by a shape too, not only by its color: a bar in the
 * gutter beside it, so nothing shifts when it appears. */
void TswStatusCard::paintFinalBar(QPainter &painter, const Layout &lay, double left, double top) const
{
	if (content.banner != TSW_BANNER_TIMELAPSE)
		return;
	const double frame = cardPaint(*theme, content.banner).width;
	const double x = lay.stacked ? std::max(frame + 1.0, lay.pad - theme->s2 - FINAL_BAR)
				     : std::round(left - (lay.gutter + FINAL_BAR) / 2.0);
	const double height = lay.captionHeight + theme->s2 + lay.digitsHeight;
	tswPaintRounded(painter, QRectF(x, top, FINAL_BAR, height), FINAL_BAR / 2.0,
			theme->shape(theme->accent, fillColor()), QPen(Qt::NoPen));
}

void TswStatusCard::paintReadout(QPainter &painter, const Layout &lay) const
{
	const QColor under = fillColor();
	const double pair = lay.captionHeight + theme->s2 + lay.digitsHeight;
	const double secondLeft = lay.secondLeft;
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
	paintFinalBar(painter, lay, secondLeft, secondTop);
}

/*
 * The tape band (from the Transport direction): how fast the tape runs, shown without a word.
 * Timelapse: chevrons, packed tighter the faster it goes. Real time: sparse play marks on a line.
 * Saving: one still accent line. Otherwise an empty tape: a hairline (the summary's speed map will
 * live here).
 */
void TswStatusCard::paintTape(QPainter &painter, const Layout &lay) const
{
	const QRectF band(lay.pad, lay.tapeTop, lay.inner, lay.tapeHeight);
	const double size = band.height();
	const double middle = std::round(band.center().y());
	const QColor under = fillColor();
	if (content.banner == TSW_BANNER_BUSY) {
		painter.fillRect(QRectF(band.left(), middle - BUSY_LINE / 2.0, band.width(), BUSY_LINE), theme->accent);
		return;
	}
	const bool timelapse = content.banner == TSW_BANNER_TIMELAPSE;
	if (!timelapse && content.banner != TSW_BANNER_REAL_TIME) {
		painter.fillRect(QRectF(band.left(), middle - 0.5, band.width(), 1.0),
				 tswMix(theme->text, under, DIVIDER_INK / 2.0));
		return;
	}
	const QColor color = timelapse ? theme->shape(theme->accent, under) : theme->frame;
	const double pitch = size * (timelapse ? chevronPitch(content.speedValue) : PLAY_PITCH);
	const int marks = std::min(MAX_MARKS, static_cast<int>((band.width() - size) / pitch) + 1);
	const double left = band.left() - (timelapse ? std::round(CHEVRON_INSET * size) : 0.0);
	painter.save();
	painter.setClipRect(band);
	for (int i = 0; i < marks; i++) {
		const double x = left + i * pitch;
		tswPaintGlyph(painter, timelapse ? TswGlyph::Chevron : TswGlyph::Play,
			      QRectF(x, band.top(), size, size), color);
		if (!timelapse && i + 1 < marks) /* the tape between two marks */
			painter.fillRect(QRectF(x + size + theme->s2, middle - 0.5, pitch - size - 2 * theme->s2, 1.0),
					 color);
	}
	painter.restore();
}

void TswStatusCard::paintEvent(QPaintEvent *event)
{
	(void)event;
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setRenderHint(QPainter::TextAntialiasing);
	const Layout lay = measure(width());
	paintFrame(painter, lay);
	paintTally(painter, lay);
	paintHero(painter, lay);
	if (content.banner != TSW_BANNER_TIMELAPSE) { /* in timelapse the band's edge divides A from B */
		painter.setPen(dashedPen(tswMix(theme->text, fillColor(), DIVIDER_INK), 1.0, DASH, DASH_GAP));
		painter.drawLine(QPointF(lay.pad, lay.dividerY + 0.5),
				 QPointF(lay.pad + lay.inner, lay.dividerY + 0.5));
	}
	paintReadout(painter, lay);
	paintStretch(painter, lay);
	paintTape(painter, lay);
}
