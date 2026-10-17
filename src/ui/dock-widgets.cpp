/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * dock-widgets.cpp: the dock's painted controls (see dock-widgets.hpp). The status card is in
 * dock-status.cpp. Sizes follow docs/design/visual-design.md, 3.7 and 4.2.
 */

#include "ui/dock-widgets.hpp"

#include <QButtonGroup>
#include <QEasingCurve>
#include <QEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

#include "ui/dock-paint.hpp"

namespace {

const int THUMB_INSET = 2;          /* px between the track and its thumb (4.2) */
const int SLIDE_MS = 160;           /* the thumb's slide (4.2, motion) */
const double DISABLED_INK = 0.6;    /* disabled content is muted at 60 % (4.2, main button) */
const double RING_GAP = 2.0;        /* px between a frame and the focus ring inside it */
const double THUMB_FRAME = 2.0;     /* px: the outline of an armed or engaged thumb */
const double POINT_STEP = 0.5;      /* pt: rail labels shrink in half points */
const int MAX_FIT_STEPS = 16;       /* from 1.1b down to the floor at any base size (P10-2) */
const double STACKED_FLOOR = 0.8;   /* x b: a stacked rail label shrinks this far before it wraps */
const double LEAD_WORD_FLOOR = 0.9; /* x b: a lead rail keeps the lead's word down to this size */

} // namespace

/* --- TswReservedLabel --- */

TswReservedLabel::TswReservedLabel(QWidget *parent) : QLabel(parent)
{
	setWordWrap(true);
	setTextFormat(Qt::PlainText);
}

void TswReservedLabel::setReserve(const QStringList &texts)
{
	reserve = texts;
	updateGeometry();
}

int TswReservedLabel::heightForWidth(int width) const
{
	int height = QLabel::heightForWidth(width);
	const QMargins margins = contentsMargins();
	const int textWidth = width - margins.left() - margins.right();
	for (const QString &text : reserve)
		height = std::max(height, tswWrappedHeight(font(), text, textWidth) + margins.top() + margins.bottom());
	return height;
}

QSize TswReservedLabel::minimumSizeHint() const
{
	/* Wrapping labels may be as narrow as the dock needs; their height follows heightForWidth. */
	return {0, QLabel::minimumSizeHint().height()};
}

/* --- TswGlyphIcon --- */

TswGlyphIcon::TswGlyphIcon(QWidget *parent) : QWidget(parent)
{
	setAttribute(Qt::WA_TransparentForMouseEvents);
}

void TswGlyphIcon::setGlyph(TswGlyph shape, const QColor &paint, const QSize &box)
{
	if (glyph == shape && color == paint && size() == box && minimumSize() == box)
		return; /* the context line sets it on every view */
	glyph = shape;
	color = paint;
	setFixedSize(box);
	update();
}

void TswGlyphIcon::paintEvent(QPaintEvent *event)
{
	(void)event;
	QPainter painter(this);
	tswPaintGlyph(painter, glyph, QRectF(rect()), color);
}

/* --- TswButton --- */

TswButton::TswButton(QWidget *parent, const TswTheme *style) : QAbstractButton(parent), theme(style)
{
	setAttribute(Qt::WA_Hover);
	setFocusPolicy(Qt::StrongFocus);
	/* Not Fixed: Qt would cap the height at sizeHint's, measured at another width. */
	QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	policy.setHeightForWidth(true);
	setSizePolicy(policy);
}

void TswButton::setContent(TswGlyph icon, const QString &text)
{
	if (glyph == icon && label == text)
		return;
	glyph = icon;
	label = text;
	updateGeometry();
	update();
}

void TswButton::setReserve(const QStringList &labels)
{
	reserve = labels;
	updateGeometry();
}

void TswButton::setLook(const TswButtonLook &paint)
{
	if (paint.face == look.face && paint.under == look.under && paint.frame == look.frame &&
	    paint.frameWidth == look.frameWidth && paint.glyph == look.glyph && paint.label == look.label)
		return; /* the same look again: no repaint */
	look = paint;
	update();
}

void TswButton::setShape(Shape kind)
{
	if (shape == kind)
		return; /* zone E sets it on every refresh */
	shape = kind;
	update();
}

void TswButton::setLabelFont(const QFont &font)
{
	labelFont = font;
	updateGeometry();
	update();
}

void TswButton::setMinimumRow(int height)
{
	minimumRow = height;
	updateGeometry();
}

void TswButton::setPadding(int pixels)
{
	if (padding == pixels)
		return;
	padding = pixels;
	updateGeometry();
	update();
}

int TswButton::sidePadding() const
{
	return padding >= 0 ? padding : theme->s4;
}

void TswButton::setHoldProgress(double progress)
{
	hold = std::clamp(progress, 0.0, 1.0);
	update();
}

int TswButton::glyphSize() const
{
	return glyph == TswGlyph::None ? 0 : tswGlyphSize(labelFont);
}

/* The width left for the label once the side padding and the glyph are taken. */
static int labelWidth(int width, int padding, int glyph, int gap)
{
	return std::max(1, width - 2 * padding - (glyph > 0 ? glyph + gap : 0));
}

int TswButton::lineCount(const QString &text, int width) const
{
	if (text.isEmpty())
		return 0;
	/* Usually one or two lines (UX-121); more only in the narrowest dock at the largest font, and
	 * then the button grows: a label is never cut. */
	const int lineHeight = QFontMetrics(labelFont).lineSpacing();
	const int lines = (tswWrappedHeight(labelFont, text, width) + lineHeight / 2) / lineHeight;
	return std::max(lines, 1);
}

int TswButton::labelLines(int width) const
{
	const int gap = glyphSize() > 0 && !label.isEmpty() ? theme->s3 : 0;
	return lineCount(label, labelWidth(width, sidePadding(), glyphSize(), gap));
}

int TswButton::mostLines(int width) const
{
	const int gap = glyphSize() > 0 ? theme->s3 : 0;
	const int textWidth = labelWidth(width, sidePadding(), glyphSize(), gap);
	int lines = lineCount(label, textWidth);
	for (const QString &text : reserve)
		lines = std::max(lines, lineCount(text, textWidth));
	return lines;
}

int TswButton::oneLineWidth() const
{
	double widest = tswTextWidth(labelFont, label);
	for (const QString &text : reserve)
		widest = std::max(widest, tswTextWidth(labelFont, text));
	const int glyphWidth = glyphSize() > 0 ? glyphSize() + theme->s3 : 0;
	return static_cast<int>(std::ceil(widest)) + 1 + glyphWidth + 2 * sidePadding();
}

bool TswButton::hasHeightForWidth() const
{
	return true;
}

int TswButton::heightForWidth(int width) const
{
	/* The tallest of the labels it may show, so swapping them moves nothing below (UX-002). */
	const int textWidth = labelWidth(width, sidePadding(), glyphSize(), theme->s3);
	int lines = lineCount(label, textWidth);
	for (const QString &text : reserve)
		lines = std::max(lines, lineCount(text, textWidth));
	const int content = lines * QFontMetrics(labelFont).lineSpacing() + 2 * theme->s2;
	return std::max(minimumRow, content);
}

QSize TswButton::sizeHint() const
{
	const int glyphWidth = glyphSize() > 0 ? glyphSize() + theme->s3 : 0;
	const int width = static_cast<int>(std::ceil(tswTextWidth(labelFont, label))) + glyphWidth + 2 * sidePadding();
	return {width, heightForWidth(width)};
}

QSize TswButton::minimumSizeHint() const
{
	/* As narrow as the longest word allows: the label wraps instead of losing its verb (UX-004). */
	int word = 0;
	QStringList texts = reserve;
	texts << label;
	for (const QString &text : texts) {
		for (const QString &part : text.split(' ', Qt::SkipEmptyParts))
			word = std::max(word, static_cast<int>(std::ceil(tswTextWidth(labelFont, part))));
	}
	const int glyphWidth = glyphSize() > 0 ? glyphSize() + theme->s3 : 0;
	const int width = shape == Shape::Pill && label.isEmpty() ? minimumRow : word + glyphWidth + 2 * sidePadding();
	return {width, minimumRow};
}

double TswButton::radius() const
{
	if (shape == Shape::Pill)
		return height() / 2.0;
	return theme->corner; /* from the theme's own buttons: 8 px, 4 px in Classic (3.7) */
}

void TswButton::paintFace(QPainter &painter, const QRectF &box) const
{
	const bool enabled = isEnabled();
	QColor face = enabled ? look.face : theme->surface;
	if (enabled && isDown())
		face = theme->pressed(face.isValid() ? face : look.under);
	else if (enabled && underMouse())
		face = theme->hover(face.isValid() ? face : look.under);
	const QPen frame = enabled ? tswBorder(look.frame, look.frameWidth) : QPen(Qt::NoPen);
	tswPaintRounded(painter, box, radius(), face, frame);
	if (hold > 0.0) {
		/* Stop's hold fill grows from the left, with a stronger leading edge (UX-052). */
		painter.save();
		QPainterPath clip;
		clip.addRoundedRect(box, radius(), radius());
		painter.setClipPath(clip);
		const double edge = box.left() + box.width() * hold;
		painter.fillRect(QRectF(box.left(), box.top(), edge - box.left(), box.height()), theme->dangerTint);
		painter.fillRect(QRectF(edge - 2.0, box.top(), 2.0, box.height()),
				 theme->shape(theme->danger, theme->dangerTint));
		painter.restore();
	}
}

void TswButton::paintContent(QPainter &painter, QRectF box) const
{
	const int glyph_px = glyphSize();
	const int gap = glyph_px > 0 && !label.isEmpty() ? theme->s3 : 0;
	const int room = labelWidth(static_cast<int>(box.width()), sidePadding(), glyph_px, gap);
	const int textWidth = tswBalancedWidth(labelFont, label, room);
	const QRect bounds = QFontMetrics(labelFont).boundingRect(
		QRect(0, 0, textWidth, static_cast<int>(box.height())), Qt::TextWordWrap, label);
	const double textHeight = label.isEmpty() ? 0.0 : bounds.height();
	const double blockWidth = glyph_px + gap + (label.isEmpty() ? 0 : bounds.width());
	const double left = std::round(box.center().x() - blockWidth / 2.0);
	const QColor glyphColor = isEnabled() ? look.glyph : theme->muted;
	const QColor labelColor = isEnabled() ? look.label : theme->muted;
	if (!isEnabled())
		painter.setOpacity(DISABLED_INK);
	const QRectF glyphBox(left, box.center().y() - glyph_px / 2.0, glyph_px, glyph_px);
	tswPaintGlyph(painter, glyph, glyphBox, glyphColor);
	if (!label.isEmpty()) {
		const QRectF textBox(left + glyph_px + gap, box.center().y() - textHeight / 2.0, bounds.width() + 1,
				     textHeight);
		painter.setFont(labelFont);
		painter.setPen(labelColor);
		painter.drawText(textBox, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, label);
	}
	painter.setOpacity(1.0);
}

void TswButton::paintEvent(QPaintEvent *event)
{
	(void)event;
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	const QRectF box(rect());
	paintFace(painter, box);
	/* Pressed: the content moves 1 px down, like a key that travels. */
	paintContent(painter, isDown() ? box.translated(0, 1) : box);
	if (tswHasKeyFocus(this)) {
		/* Always inside the button, with a gap: the edge is where an armed button has its accent
		 * outline (outlined means next), so a focused Record must not look armed. 3:1 against the
		 * face it is drawn on, whatever that face is (UX-110). */
		const double inset = std::max(look.frameWidth, 1.0) + RING_GAP;
		const QColor face = look.face.isValid() ? look.face : look.under;
		tswPaintFocusRing(painter, box, radius(), theme->shape(theme->focus, face), inset, TswRing::Solid);
	}
}

/* --- TswLink --- */

TswLink::TswLink(QWidget *parent, const TswTheme *style, TswGlyph shape)
	: QAbstractButton(parent),
	  theme(style),
	  glyph(shape)
{
	setAttribute(Qt::WA_Hover);
	setFocusPolicy(Qt::StrongFocus);
	setCursor(Qt::PointingHandCursor);
	setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void TswLink::setLabel(const QString &text)
{
	setAccessibleName(text);
	setText(text);
	updateGeometry();
}

void TswLink::setOnPanel(bool onPanel)
{
	panel = onPanel;
	update();
}

QColor TswLink::under() const
{
	return panel ? theme->panel : theme->surface;
}

void TswLink::setLinkFont(const QFont &font)
{
	linkFont = font;
	updateGeometry();
}

QSize TswLink::sizeHint() const
{
	const QFontMetrics metrics(linkFont);
	const int glyph_px = tswGlyphSize(linkFont);
	/* Room on both sides for the focus ring, which is drawn on the edge: the glyph stays near the
	 * card's text edge, yet clear of the ring. */
	const int width = theme->s2 + glyph_px + theme->s2 + metrics.horizontalAdvance(text()) + theme->s2;
	return {width, std::max(metrics.height() + 2 * theme->s2, static_cast<int>(theme->L * 1.6))};
}

QSize TswLink::minimumSizeHint() const
{
	/* Side by side only when both fit at the visible width (TswDock::updateNarrow), one under the
	 * other otherwise: so a link never makes the dock wider than what shows (3.10). */
	return {0, sizeHint().height()};
}

void TswLink::paintEvent(QPaintEvent *event)
{
	(void)event;
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	const int glyph_px = tswGlyphSize(linkFont);
	const QColor color = theme->ink(theme->accentText, under());
	const double middle = height() / 2.0;
	tswPaintGlyph(painter, glyph, QRectF(theme->s2, middle - glyph_px / 2.0, glyph_px, glyph_px), color);
	QFont font = linkFont;
	font.setUnderline(underMouse() || tswHasKeyFocus(this)); /* the link shows itself when reached */
	painter.setFont(font);
	painter.setPen(color);
	const double left = theme->s2 + glyph_px + theme->s2;
	painter.drawText(QPointF(left, tswCapBaseline(font, 0, height())), text());
	if (tswHasKeyFocus(this)) {
		/* Around the glyph and the words, not the whole row a stacked list may give the link. */
		const QRectF content(0, 0, std::min(width(), sizeHint().width()), height());
		tswPaintFocusRing(painter, content, theme->s2, theme->shape(theme->focus, under()), 0.0,
				  TswRing::Solid);
	}
}

/* --- TswSegment --- */

TswSegment::TswSegment(TswRail *owner, TswGlyph shape, const QString &text)
	: QToolButton(owner),
	  rail(owner),
	  glyph(shape),
	  label(text)
{
	setAttribute(Qt::WA_Hover);
	setCheckable(true);
	setAutoRaise(true);
}

void TswSegment::setLabel(const QString &text)
{
	label = text;
	update();
}

QFont TswSegment::labelFont() const
{
	QFont font = rail->theme()->fontAt(TswType::RAIL, isChecked());
	font.setPointSizeF(rail->fitAt(rail->width()).points);
	return font;
}

QColor TswSegment::under() const
{
	return isChecked() ? rail->thumbFill() : rail->theme()->surface;
}

QColor TswSegment::labelColor() const
{
	const TswTheme *theme = rail->theme();
	if (!isEnabled())
		return theme->ink(theme->muted, theme->surface);
	if (isChecked() && rail->mode() == TswRail::Mode::Engaged)
		return theme->onAccent;
	return theme->ink(theme->text, under());
}

int TswSegment::besideWidth(const QFont &font, bool words) const
{
	const TswTheme *theme = rail->theme();
	const int glyph_px = glyph == TswGlyph::None ? 0 : tswGlyphSize(font);
	const double text = words ? tswTextWidth(font, label) : 0.0;
	const int gap = glyph_px > 0 && words ? theme->s2 : 0;
	/* Air on both sides inside the thumb, so a Bold label never touches its outline. */
	return static_cast<int>(std::ceil(glyph_px + gap + 2 * (theme->s2 + theme->s1) + text));
}

bool TswSegment::fitsBeside(int width, const QFont &font) const
{
	return besideWidth(font, true) <= width;
}

bool TswSegment::glyphOnly() const
{
	return rail->isLead(this) && !rail->fitAt(rail->width()).leadWords;
}

bool TswSegment::fitsUnder(int width, const QFont &font) const
{
	return tswTextWidth(font, label) <= width - 2 * rail->theme()->s2;
}

int TswSegment::stackedHeight(int width, const QFont &font) const
{
	const TswTheme *theme = rail->theme();
	const int words = tswWrappedHeight(font, label, width - 2 * theme->s2); /* may wrap at the largest font */
	return tswGlyphSize(font) + theme->s1 + words + 2 * theme->s2;
}

/* Too narrow for glyph and word side by side: the word goes under its glyph, smaller if it must,
 * and wraps only when even that is too wide (3.10). */
void TswSegment::paintStacked(QPainter &painter, const QRectF &box, int glyphSize) const
{
	const TswTheme *theme = rail->theme();
	const QFont font = painter.font();
	const int room = static_cast<int>(box.width()) - 2 * theme->s2;
	const int words = tswWrappedHeight(font, label, room);
	const double top = std::round((box.height() - (glyphSize + theme->s1 + words)) / 2.0);
	tswPaintGlyph(painter, glyph, QRectF(box.center().x() - glyphSize / 2.0, top, glyphSize, glyphSize),
		      painter.pen().color());
	const QRectF text(theme->s2, top + glyphSize + theme->s1, room, words);
	painter.drawText(text, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, label);
}

QSize TswSegment::sizeHint() const
{
	return {static_cast<int>(tswTextWidth(labelFont(), label)) + 2 * rail->theme()->s3, rail->theme()->L};
}

QSize TswSegment::minimumSizeHint() const
{
	return {0, 0};
}

void TswSegment::paintEvent(QPaintEvent *event)
{
	(void)event;
	const TswTheme *theme = rail->theme();
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	const QRectF box(rect());
	const double radius = std::min(box.height() / 2.0, double(theme->corner));
	const double round = rail->isStacked() ? radius : box.height() / 2.0;
	if (!isChecked() && underMouse() && isEnabled())
		tswPaintRounded(painter, box, round, theme->hover(theme->surface), QPen(Qt::NoPen));
	const QFont font = labelFont();
	const QColor color = labelColor();
	const int glyph_px = glyph == TswGlyph::None ? 0 : tswGlyphSize(font);
	painter.setFont(font);
	painter.setPen(color);
	if (glyph != TswGlyph::None && rail->isStacked()) {
		paintStacked(painter, box, glyph_px);
	} else if (glyphOnly()) {
		tswPaintGlyph(painter, glyph,
			      QRectF(box.center().x() - glyph_px / 2.0, box.center().y() - glyph_px / 2.0, glyph_px,
				     glyph_px),
			      color);
	} else {
		const double gap = glyph_px > 0 ? theme->s2 : 0;
		const double block = glyph_px + gap + tswTextWidth(font, label);
		const double left = std::round(box.center().x() - block / 2.0);
		tswPaintGlyph(painter, glyph, QRectF(left, box.center().y() - glyph_px / 2.0, glyph_px, glyph_px),
			      color);
		painter.drawText(QPointF(left + glyph_px + gap, tswCapBaseline(font, 0, box.height())), label);
	}
	if (tswHasKeyFocus(this)) {
		/* Dashed: a solid outline on a segment means "the next speed" (outlined means next). On the
		 * thumb it sits inside the thumb's outline, with a gap, so the two never merge. */
		const double inset = isChecked() ? THUMB_FRAME + RING_GAP : 1.0;
		tswPaintFocusRing(painter, box, round, theme->shape(theme->focus, under()), inset, TswRing::Dashed);
	}
}

void TswSegment::keyPressEvent(QKeyEvent *event)
{
	switch (event->key()) {
	case Qt::Key_Left:
	case Qt::Key_Up:
		rail->moveFocus(this, -1); /* focus only: a live speed never changes by accident (UX-102) */
		return;
	case Qt::Key_Right:
	case Qt::Key_Down:
		rail->moveFocus(this, 1);
		return;
	case Qt::Key_Return:
	case Qt::Key_Enter:
		click();
		return;
	default:
		QToolButton::keyPressEvent(event);
	}
}

/* --- TswRail --- */

TswRail::TswRail(QWidget *parent, const TswTheme *theme)
	: QWidget(parent),
	  style(theme),
	  buttons(new QButtonGroup(this))
{
	buttons->setExclusive(true);
	QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred); /* see TswButton */
	policy.setHeightForWidth(true);
	setSizePolicy(policy);
	slide.setDuration(SLIDE_MS);
	slide.setEasingCurve(QEasingCurve::OutCubic);
	connect(&slide, &QVariantAnimation::valueChanged, this, [this]() { update(); });
}

TswSegment *TswRail::addSegment(int id, TswGlyph glyph, const QString &label)
{
	auto *segment = new TswSegment(this, glyph, label);
	buttons->addButton(segment, id);
	segments.append(segment);
	connect(segment, &QAbstractButton::toggled, this, [this]() { update(); });
	layoutSegments();
	updateFocusPolicies();
	return segment;
}

QButtonGroup *TswRail::group() const
{
	return buttons;
}

const TswTheme *TswRail::theme() const
{
	return style;
}

void TswRail::setMode(Mode mode)
{
	if (current == mode)
		return;
	current = mode;
	update();
	for (TswSegment *segment : segments)
		segment->update();
}

TswRail::Mode TswRail::mode() const
{
	return current;
}

void TswRail::select(int id, bool animate)
{
	QAbstractButton *button = buttons->button(id);
	const int index = static_cast<int>(segments.indexOf(static_cast<TswSegment *>(button)));
	if (index < 0)
		return;
	if (index != selected) {
		/* From where the thumb is now (also mid-slide) to the new segment: left edge and width, as
		 * a lead rail's segments differ in width. */
		const QRectF from = selected >= 0 ? thumbBox() : segmentBox(index);
		slide.stop();
		if (animate && selected >= 0) {
			slideFrom = from;
			slide.setStartValue(0.0);
			slide.setEndValue(1.0);
			slide.start();
		}
		selected = index;
	}
	button->setChecked(true);
	updateFocusPolicies();
	update();
}

bool TswRail::isMoving() const
{
	return slide.state() == QAbstractAnimation::Running;
}

void TswRail::finishMotion()
{
	if (!isMoving())
		return;
	slide.stop();
	update();
}

/* One tab stop for the whole rail: the selected segment; arrows reach the others (UX-102). */
void TswRail::updateFocusPolicies()
{
	for (int i = 0; i < segments.size(); i++)
		segments[i]->setFocusPolicy(i == std::max(0, selected) ? Qt::StrongFocus : Qt::ClickFocus);
}

void TswRail::moveFocus(TswSegment *from, int step)
{
	const int index = static_cast<int>(segments.indexOf(from));
	const int next = std::clamp(index + step, 0, static_cast<int>(segments.size()) - 1);
	if (next != index)
		segments[next]->setFocus(Qt::TabFocusReason); /* keyboard focus: the ring shows */
}

bool TswRail::hasHeightForWidth() const
{
	return true;
}

TswRail::Fit TswRail::fitAt(int width) const
{
	const double largest = style->pointsAt(TswType::RAIL);
	if (segments.isEmpty())
		return {largest, false};
	if (lead && segments.size() > 1)
		return fitLead(width);
	const int each = (width - 2 * THUMB_INSET) / static_cast<int>(segments.size());
	QFont font = style->fontAt(TswType::RAIL, true); /* Bold: the widest a label gets */
	const auto all = [&](bool beside) {
		for (const TswSegment *segment : segments) {
			if (beside ? !segment->fitsBeside(each, font) : !segment->fitsUnder(each, font))
				return false;
		}
		return true;
	};
	/* 1. Beside the glyphs, from 1.1b down to 1.0b; 2. under them, down to the stacked floor. */
	const double floors[] = {style->pointsAt(TswType::BODY), style->pointsAt(STACKED_FLOOR)};
	for (int pass = 0; pass < 2; pass++) {
		for (int step = 0; step < MAX_FIT_STEPS; step++) {
			const double points = largest - step * POINT_STEP;
			if (points < floors[pass])
				break;
			font.setPointSizeF(points);
			if (all(pass == 0))
				return {points, pass == 1};
		}
	}
	return {floors[1], true}; /* the narrowest dock at the largest font: the words wrap */
}

bool TswRail::isStacked() const
{
	return fitAt(width()).stacked;
}

int TswRail::heightForWidth(int width) const
{
	const int height = std::max(static_cast<int>(std::lround(1.8 * style->L)), style->hC); /* UX-107 */
	const Fit fit = fitAt(width);
	if (!fit.stacked)
		return height;
	const int segmentWidth = (width - 2 * THUMB_INSET) / static_cast<int>(segments.size());
	QFont font = style->fontAt(TswType::RAIL, true);
	font.setPointSizeF(fit.points);
	int stacked = 0;
	for (const TswSegment *segment : segments)
		stacked = std::max(stacked, segment->stackedHeight(segmentWidth, font));
	return std::max(height, stacked + 2 * THUMB_INSET);
}

QSize TswRail::sizeHint() const
{
	return {200, heightForWidth(200)};
}

QSize TswRail::minimumSizeHint() const
{
	/* Each segment at least as wide as it is tall, so x16 always fits (4.2, narrow). */
	const int height = std::max(static_cast<int>(std::lround(1.8 * style->L)), style->hC);
	return {static_cast<int>(segments.size()) * height * 3 / 4 + 2 * THUMB_INSET, height};
}

QRectF TswRail::segmentBox(int index) const
{
	const double inner = width() - 2.0 * THUMB_INSET;
	const double top = THUMB_INSET;
	const double tall = height() - 2.0 * THUMB_INSET;
	if (!lead || segments.size() < 2) {
		const double each = segments.isEmpty() ? inner : inner / static_cast<double>(segments.size());
		return {THUMB_INSET + each * index, top, each, tall};
	}
	const double first = fitAt(width()).lead;
	if (index == 0)
		return {double(THUMB_INSET), top, first, tall};
	const double each = (inner - first) / static_cast<double>(segments.size() - 1);
	return {THUMB_INSET + first + each * (index - 1), top, each, tall};
}

/* The thumb now: under the selected segment, or on its way there. */
QRectF TswRail::thumbBox() const
{
	const QRectF to = segmentBox(selected);
	if (slide.state() != QAbstractAnimation::Running)
		return to;
	const double t = slide.currentValue().toDouble();
	const auto mix = [t](double a, double b) {
		return a + (b - a) * t;
	};
	return {mix(slideFrom.left(), to.left()), to.top(), mix(slideFrom.width(), to.width()), to.height()};
}

void TswRail::setLead(bool isLead)
{
	lead = isLead;
	layoutSegments();
	update();
}

bool TswRail::isLead(const TswSegment *segment) const
{
	return lead && !segments.isEmpty() && segments.first() == segment;
}

void TswRail::refit()
{
	layoutSegments();
	updateGeometry();
	update();
}

/*
 * A lead rail on one row, always: the lead (Real time) at its own width, with its word, from 1.1b
 * down to 0.9b; then, where its word cannot fit, its glyph alone (its name stays in its tooltip and
 * accessible name), the segments equal, from 1.1b down. Every segment gets the room it needs plus an
 * equal share of what is left (3.10).
 */
TswRail::Fit TswRail::fitLead(int width) const
{
	const double inner = width - 2.0 * THUMB_INSET;
	const auto others = static_cast<double>(segments.size() - 1);
	const double largest = style->pointsAt(TswType::RAIL);
	const double floors[] = {style->pointsAt(LEAD_WORD_FLOOR), style->pointsAt(STACKED_FLOOR)};
	QFont font = style->fontAt(TswType::RAIL, true); /* Bold: the widest a label gets */
	for (int pass = 0; pass < 2; pass++) {
		const bool words = pass == 0;
		for (int step = 0; step < MAX_FIT_STEPS; step++) {
			const double points = largest - step * POINT_STEP;
			if (points < floors[pass])
				break;
			font.setPointSizeF(points);
			int chip = 0;
			for (int i = 1; i < segments.size(); i++)
				chip = std::max(chip, segments[i]->besideWidth(font, true));
			const double first = words ? segments[0]->besideWidth(font, true) : chip;
			const double spare = inner - first - others * chip;
			if (spare >= 0.0)
				return {points, false, words, first + spare / (others + 1.0)};
		}
	}
	return {floors[1], false, false, inner / (others + 1.0)};
}

/* For the UI test: a label wider than its segment would run past its edge. Stacked rails wrap their
 * words instead, so only one-row rails are measured. */
bool TswRail::clipped() const
{
	const Fit fit = fitAt(width());
	if (fit.stacked)
		return false;
	QFont font = style->fontAt(TswType::RAIL, true);
	font.setPointSizeF(fit.points);
	for (int i = 0; i < segments.size(); i++) {
		const bool words = !(lead && i == 0) || fit.leadWords;
		if (segments[i]->besideWidth(font, words) > segmentBox(i).width() + 0.5)
			return true;
	}
	return false;
}

/* A pill on one row; a rounded rectangle when the labels stack, never a tall stadium. */
double TswRail::thumbRadius(const QRectF &thumb) const
{
	return isStacked() ? std::min(thumb.height() / 2.0, double(style->corner)) : thumb.height() / 2.0;
}

void TswRail::layoutSegments()
{
	for (int i = 0; i < segments.size(); i++)
		segments[i]->setGeometry(segmentBox(i).toAlignedRect());
}

void TswRail::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	slide.stop();
	layoutSegments();
}

void TswRail::changeEvent(QEvent *event)
{
	QWidget::changeEvent(event);
	if (event->type() == QEvent::EnabledChange)
		update();
}

QColor TswRail::thumbFill() const
{
	if (!isEnabled())
		return style->panel;
	if (current == Mode::Engaged)
		return style->accentFill;
	if (current == Mode::Start && selected == 0)
		return style->raised;
	return style->panel;
}

void TswRail::paintEvent(QPaintEvent *event)
{
	(void)event;
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	const QRectF track(rect());
	const double trackRadius = isStacked() ? std::min(track.height() / 2.0, style->corner + double(THUMB_INSET))
					       : track.height() / 2.0;
	tswPaintRounded(painter, track, trackRadius, style->surface, tswBorder(style->hairline, 1.0));
	if (selected < 0)
		return;

	/* The thumb: filled when the speed runs now, outlined when it is the next one (4.2). */
	const QRectF thumb = thumbBox();
	const double radius = thumbRadius(thumb);
	const bool startRealTime = current == Mode::Start && selected == 0;
	QPen frame = tswBorder(style->shape(style->accent, style->surface), THUMB_FRAME);
	if (!isEnabled())
		frame = tswBorder(style->edge, THUMB_FRAME);
	else if (current == Mode::Engaged)
		frame = tswBorder(style->accent, THUMB_FRAME);
	else if (startRealTime)
		frame = tswBorder(style->edge, 1.0);
	tswPaintRounded(painter, thumb, radius, thumbFill(), frame);
}

/* --- TswCard --- */

TswCard::TswCard(QWidget *parent, const TswTheme *style) : QWidget(parent), theme(style) {}

void TswCard::paintEvent(QPaintEvent *event)
{
	(void)event;
	QPainter painter(this);
	tswPaintRounded(painter, QRectF(rect()), theme->corner, theme->surface, tswBorder(theme->hairline, 1.0));
}
