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

const int THUMB_INSET = 2;       /* px between the track and its thumb (4.2) */
const int SLIDE_MS = 160;        /* the thumb's slide (4.2, motion) */
const double MAIN_RADIUS = 0.6;  /* x L: the main button's corners, a rounded rectangle */
const double DISABLED_INK = 0.6; /* disabled content is muted at 60 % (4.2, main button) */
const double CARD_RADIUS = 0.45; /* x L, at least MIN_CARD_RADIUS */
const int MIN_CARD_RADIUS = 6;
const int MAX_LINES = 2; /* labels wrap to two lines at most (UX-121) */

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
	look = paint;
	update();
}

void TswButton::setShape(Shape kind)
{
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
	const int lineHeight = QFontMetrics(labelFont).lineSpacing();
	const int lines = (tswWrappedHeight(labelFont, text, width) + lineHeight / 2) / lineHeight;
	return std::clamp(lines, 1, MAX_LINES);
}

bool TswButton::hasHeightForWidth() const
{
	return true;
}

int TswButton::heightForWidth(int width) const
{
	/* The tallest of the labels it may show, so swapping them moves nothing below (UX-002). */
	const int textWidth = labelWidth(width, theme->s4, glyphSize(), theme->s3);
	int lines = lineCount(label, textWidth);
	for (const QString &text : reserve)
		lines = std::max(lines, lineCount(text, textWidth));
	const int content = lines * QFontMetrics(labelFont).lineSpacing() + 2 * theme->s2;
	return std::max(minimumRow, content);
}

QSize TswButton::sizeHint() const
{
	const int glyphWidth = glyphSize() > 0 ? glyphSize() + theme->s3 : 0;
	const int width = static_cast<int>(tswTextWidth(labelFont, label)) + glyphWidth + 2 * theme->s4;
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
	const int width = shape == Shape::Pill && label.isEmpty() ? minimumRow : word + glyphWidth + 2 * theme->s4;
	return {width, minimumRow};
}

double TswButton::radius() const
{
	if (shape == Shape::Pill)
		return height() / 2.0;
	return std::round(theme->L * MAIN_RADIUS);
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
	const int room = labelWidth(static_cast<int>(box.width()), theme->s4, glyph_px, gap);
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
	if (tswHasKeyFocus(this))
		tswPaintFocusRing(painter, box, radius(), theme->focus);
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

void TswLink::setLinkFont(const QFont &font)
{
	linkFont = font;
	updateGeometry();
}

QSize TswLink::sizeHint() const
{
	const QFontMetrics metrics(linkFont);
	const int glyph_px = tswGlyphSize(linkFont);
	const int width = glyph_px + theme->s2 + metrics.horizontalAdvance(text()) + 2 * theme->s1;
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
	const QColor color = theme->ink(theme->accentText, theme->surface);
	const double middle = height() / 2.0;
	tswPaintGlyph(painter, glyph, QRectF(theme->s1, middle - glyph_px / 2.0, glyph_px, glyph_px), color);
	QFont font = linkFont;
	font.setUnderline(underMouse() || tswHasKeyFocus(this)); /* the link shows itself when reached */
	painter.setFont(font);
	painter.setPen(color);
	const double left = theme->s1 + glyph_px + theme->s2;
	painter.drawText(QPointF(left, tswCapBaseline(font, 0, height())), text());
	if (tswHasKeyFocus(this))
		tswPaintFocusRing(painter, QRectF(rect()), theme->s2, theme->focus);
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
	return rail->theme()->fontAt(TswType::RAIL, isChecked());
}

QColor TswSegment::labelColor() const
{
	const TswTheme *theme = rail->theme();
	if (!isEnabled())
		return theme->ink(theme->muted, theme->surface);
	if (isChecked() && rail->mode() == TswRail::Mode::Engaged)
		return theme->onAccent;
	return theme->text;
}

bool TswSegment::stacksAt(int width) const
{
	if (glyph == TswGlyph::None)
		return false;
	const QFont bold = rail->theme()->fontAt(TswType::RAIL, true); /* the widest it gets */
	const double need = tswGlyphSize(bold) + 3 * rail->theme()->s2 + tswTextWidth(bold, label);
	return need > width;
}

int TswSegment::stackedHeight() const
{
	const QFont bold = rail->theme()->fontAt(TswType::RAIL, true);
	return tswGlyphSize(bold) + rail->theme()->s1 + QFontMetrics(bold).height() + 2 * rail->theme()->s2;
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
	const double radius = std::min(box.height(), box.width()) / 2.0;
	if (!isChecked() && underMouse() && isEnabled())
		tswPaintRounded(painter, box, radius, theme->hover(theme->surface), QPen(Qt::NoPen));
	const QFont font = labelFont();
	const QColor color = labelColor();
	const int glyph_px = glyph == TswGlyph::None ? 0 : tswGlyphSize(font);
	painter.setFont(font);
	painter.setPen(color);
	if (glyph != TswGlyph::None && rail->isStacked()) {
		/* Too narrow for glyph and word side by side: the word goes under its glyph (3.10). */
		const double top = (box.height() - (glyph_px + theme->s1 + QFontMetrics(font).height())) / 2.0;
		tswPaintGlyph(painter, glyph, QRectF(box.center().x() - glyph_px / 2.0, top, glyph_px, glyph_px),
			      color);
		const QRectF words(0, top + glyph_px + theme->s1, box.width(), QFontMetrics(font).height());
		painter.drawText(words, Qt::AlignHCenter | Qt::AlignVCenter, label);
	} else {
		const double gap = glyph_px > 0 ? theme->s2 : 0;
		const double block = glyph_px + gap + tswTextWidth(font, label);
		const double left = std::round(box.center().x() - block / 2.0);
		tswPaintGlyph(painter, glyph, QRectF(left, box.center().y() - glyph_px / 2.0, glyph_px, glyph_px),
			      color);
		painter.drawText(QPointF(left + glyph_px + gap, tswCapBaseline(font, 0, box.height())), label);
	}
	if (tswHasKeyFocus(this))
		tswPaintFocusRing(painter, box, radius, theme->focus);
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
		const double from = selected >= 0 ? segmentBox(selected).left() : segmentBox(index).left();
		slide.stop();
		if (animate && selected >= 0) {
			slide.setStartValue(from);
			slide.setEndValue(segmentBox(index).left());
			slide.start();
		}
		selected = index;
	}
	button->setChecked(true);
	updateFocusPolicies();
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

/* When one label does not fit beside its glyph, all go under theirs: one rhythm per rail. */
bool TswRail::stacksAt(int width) const
{
	if (segments.isEmpty())
		return false;
	const int segmentWidth = (width - 2 * THUMB_INSET) / static_cast<int>(segments.size());
	for (const TswSegment *segment : segments) {
		if (segment->stacksAt(segmentWidth))
			return true;
	}
	return false;
}

bool TswRail::isStacked() const
{
	return stacksAt(width());
}

int TswRail::heightForWidth(int width) const
{
	const int height = std::max(static_cast<int>(std::lround(1.8 * style->L)), style->hC); /* UX-107 */
	if (!stacksAt(width))
		return height;
	return std::max(height, segments.first()->stackedHeight() + 2 * THUMB_INSET);
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
	const double each = segments.isEmpty() ? inner : inner / static_cast<double>(segments.size());
	return {THUMB_INSET + each * index, double(THUMB_INSET), each, height() - 2.0 * THUMB_INSET};
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

void TswRail::paintEvent(QPaintEvent *event)
{
	(void)event;
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	const QRectF track(rect());
	tswPaintRounded(painter, track, track.height() / 2.0, style->surface, tswBorder(style->hairline, 1.0));
	if (selected < 0)
		return;

	/* The thumb: filled when the speed runs now, outlined when it is the next one (4.2). */
	QRectF thumb = segmentBox(selected);
	if (slide.state() == QAbstractAnimation::Running)
		thumb.moveLeft(slide.currentValue().toDouble());
	const double radius = thumb.height() / 2.0;
	const bool startRealTime = current == Mode::Start && selected == 0;
	if (!isEnabled())
		tswPaintRounded(painter, thumb, radius, style->panel, tswBorder(style->edge, 2.0));
	else if (current == Mode::Engaged)
		tswPaintRounded(painter, thumb, radius, style->accentFill, tswBorder(style->accent, 2.0));
	else if (startRealTime)
		tswPaintRounded(painter, thumb, radius, style->raised, tswBorder(style->edge, 1.0));
	else
		tswPaintRounded(painter, thumb, radius, style->panel,
				tswBorder(style->shape(style->accent, style->surface), 2.0));
}

/* --- TswCard --- */

TswCard::TswCard(QWidget *parent, const TswTheme *style) : QWidget(parent), theme(style) {}

void TswCard::paintEvent(QPaintEvent *event)
{
	(void)event;
	QPainter painter(this);
	const double radius = std::max(MIN_CARD_RADIUS, static_cast<int>(std::lround(theme->L * CARD_RADIUS)));
	tswPaintRounded(painter, QRectF(rect()), radius, theme->surface, tswBorder(theme->hairline, 1.0));
}
