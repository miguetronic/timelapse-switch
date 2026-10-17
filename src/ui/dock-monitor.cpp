/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * dock-monitor.cpp: see dock-monitor.hpp. Sizes and colors follow docs/design/visual-design.md, 3.5
 * to 3.8 and 4.2: every color comes from the theme's tokens through the contrast guard.
 */

#include "ui/dock-monitor.hpp"

#include <QGridLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

#include "ui/dock-glyph.hpp"
#include "ui/dock-paint.hpp"
#include "ui/dock-widgets.hpp"

namespace {

const double MAP_ROWS = 0.75;      /* x L: the speed map's height, a thin strip (UX-136) */
const double MAP_RADIUS = 2.0;     /* px: the corners of each stretch */
const double MAP_SEAM = 1.0;       /* px between two stretches at different speeds */
const double HATCH_WIDTH = 1.25;   /* px: the timelapse hatching */
const double TAPE_INK = 0.3;       /* the line a pause leaves visible: text over the surface */
const double TIMELAPSE_INK = 0.2;  /* the tint under the hatching: accent over the surface */
const double REAL_TIME_INK = 0.12; /* real time's quiet fill: text over the surface, as a raised face */
const double MENU_ROWS = 1.6;      /* x L: the menu button, as the card's round dismiss button */
const double TOGGLE_ROWS = 1.6;    /* x L: the Details toggle, an easy target */
const double MENU_GLYPH = 0.55;    /* the menu glyph's share of its button, as the tally pill's glyph */
const int MAX_HATCH_LINES = 512;   /* more than any stretch needs at the densest pitch (P10-2) */
const char UNKNOWN_KEY[] = "Details.Unknown";

/* Hatch pitch by speed, in strip heights: denser means faster, as on the status card's tape. */
double hatchPitch(uint8_t speed)
{
	if (speed >= 16)
		return 0.2;
	if (speed >= 8)
		return 0.28;
	if (speed >= 4)
		return 0.4;
	return 0.6;
}

/* The values of the Details, top to bottom, in groups set apart by a hairline (ux-spec 3.17). */
struct RowSpec {
	const char *key;
	int group;
};
const RowSpec ROWS[] = {
	{"Details.Skipped", 0},  {"Details.Compression", 0}, {"Details.RealTime", 1}, {"Details.Timelapse", 1},
	{"Details.Paused", 1},   {"Details.Switches", 1},    {"Details.Pauses", 1},   {"Details.Chapters", 1},
	{"Details.FileSize", 2}, {"Details.Growth", 2},      {"Details.DiskFree", 2}, {"Details.RoomFor", 2},
	{"Details.Backlog", 2},
};
enum Value {
	SKIPPED,
	COMPRESSION,
	REAL_TIME,
	TIMELAPSE,
	PAUSED,
	SWITCHES,
	PAUSES,
	CHAPTERS,
	FILE_SIZE,
	GROWTH,
	DISK_FREE,
	ROOM_FOR,
	BACKLOG,
	VALUE_COUNT
};
static_assert(sizeof(ROWS) / sizeof(ROWS[0]) == VALUE_COUNT, "one row per value");
const int GROUPS = 3;

} // namespace

QString tswAmountText(TswText lookup, const tsw_amount &amount)
{
	if (amount.unit == nullptr)
		return QString();
	const QString decimal = QString::fromUtf8(lookup("Number.Decimal"));
	QString number = QString::fromUtf8(amount.number);
	number.replace('.', decimal);
	/* A number never parts from its unit when a line wraps: "8 min", never "8" and "min" (3.3). */
	static const QRegularExpression numberSpace("(\\d) ");
	return QString::fromUtf8(lookup(amount.unit))
		.replace("%1", number)
		.replace("%2", QString::fromUtf8(amount.second))
		.replace(numberSpace, QString::fromUtf8("\\1\u00a0"));
}

TswMapColors tswMapColors(const TswTheme &t, const QColor &surface)
{
	TswMapColors colors;
	colors.realTime = tswMix(t.text, surface, REAL_TIME_INK);
	colors.realTimeEdge = t.shape(t.edge, surface);
	colors.timelapse = tswMix(t.accent, surface, TIMELAPSE_INK);
	colors.hatch = t.shape(t.shape(t.accent, colors.timelapse), surface);
	colors.tape = tswMix(t.text, surface, TAPE_INK);
	return colors;
}

/* --- TswSpeedMap --- */

TswSpeedMap::TswSpeedMap(QWidget *parent, const TswTheme *style) : QWidget(parent), theme(style)
{
	setObjectName("tswSpeedMap");
	QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	policy.setHeightForWidth(true);
	setSizePolicy(policy);
}

void TswSpeedMap::setMap(const uint8_t map[TSW_MAP_BUCKETS], TswText lookup)
{
	const QVector<uint8_t> next(map, map + TSW_MAP_BUCKETS);
	if (next == slices)
		return; /* the map changes about once a minute, the view ten times per second */
	slices = next;
	setAccessibleDescription(tswSpokenMap(lookup, map));
	update();
}

QVector<uint8_t> TswSpeedMap::map() const
{
	return slices;
}

bool TswSpeedMap::hasHeightForWidth() const
{
	return true;
}

int TswSpeedMap::heightForWidth(int width) const
{
	(void)width;
	return std::max(8, static_cast<int>(std::lround(MAP_ROWS * theme->L)));
}

QSize TswSpeedMap::sizeHint() const
{
	return {200, heightForWidth(200)};
}

QSize TswSpeedMap::minimumSizeHint() const
{
	return {0, heightForWidth(0)}; /* as narrow as the dock: the slices share whatever width there is */
}

/* One stretch at one speed: plain in real time; in timelapse a tint, an outline and 45 degree hatching,
 * denser the faster it went, so it never relies on color (UX-136). */
void TswSpeedMap::paintRun(QPainter &painter, const QRectF &box, uint8_t speed, const TswMapColors &colors) const
{
	if (speed == 1) {
		tswPaintRounded(painter, box, MAP_RADIUS, colors.realTime, tswBorder(colors.realTimeEdge, 1.0));
		return;
	}
	tswPaintRounded(painter, box, MAP_RADIUS, colors.timelapse, QPen(Qt::NoPen));
	painter.save();
	QPainterPath clip;
	clip.addRoundedRect(box, MAP_RADIUS, MAP_RADIUS);
	painter.setClipPath(clip);
	painter.setPen(QPen(colors.hatch, HATCH_WIDTH, Qt::SolidLine, Qt::FlatCap));
	const double pitch = std::max(2.5, hatchPitch(speed) * box.height());
	const int lines = std::min(MAX_HATCH_LINES, static_cast<int>((box.width() + box.height()) / pitch) + 2);
	for (int i = 0; i < lines; i++) {
		const double x = box.left() - box.height() + i * pitch;
		painter.drawLine(QPointF(x, box.bottom()), QPointF(x + box.height(), box.top()));
	}
	painter.restore();
	tswPaintRounded(painter, box, MAP_RADIUS, QColor(), tswBorder(colors.hatch, 1.0));
}

void TswSpeedMap::paintEvent(QPaintEvent *event)
{
	(void)event;
	if (slices.size() != TSW_MAP_BUCKETS)
		return;
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	const TswMapColors colors = tswMapColors(*theme, theme->surface);
	const QRectF strip(rect());
	const double middle = std::round(strip.center().y());
	/* The tape under the stretches: where the recording was paused, only this line shows (a gap). */
	painter.fillRect(QRectF(strip.left(), middle - 0.5, strip.width(), 1.0), colors.tape);
	const double slice = strip.width() / static_cast<double>(TSW_MAP_BUCKETS);
	int start = 0;
	for (int i = 1; i <= TSW_MAP_BUCKETS; i++) {
		if (i < TSW_MAP_BUCKETS && slices[i] == slices[start])
			continue;
		if (slices[start] != TSW_SPEED_PAUSED) {
			const double left = strip.left() + start * slice + (start > 0 ? MAP_SEAM / 2.0 : 0.0);
			const double right = strip.left() + i * slice - (i < TSW_MAP_BUCKETS ? MAP_SEAM / 2.0 : 0.0);
			paintRun(painter, QRectF(left, strip.top(), right - left, strip.height()), slices[start],
				 colors);
		}
		start = i;
	}
}

/* --- TswDisclosure --- */

TswDisclosure::TswDisclosure(QWidget *parent, const TswTheme *style) : QAbstractButton(parent), theme(style)
{
	setObjectName("tswDetailsToggle");
	setCheckable(true);
	setAttribute(Qt::WA_Hover);
	setFocusPolicy(Qt::StrongFocus);
	setCursor(Qt::PointingHandCursor);
	setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void TswDisclosure::setCaptionFont(const QFont &font)
{
	captionFont = font;
	updateGeometry();
	update();
}

QSize TswDisclosure::sizeHint() const
{
	const int glyph = tswGlyphSize(captionFont);
	const int width =
		glyph + theme->s2 + static_cast<int>(std::ceil(tswTextWidth(captionFont, text()))) + theme->s2;
	return {width, std::max(static_cast<int>(std::lround(TOGGLE_ROWS * theme->L)), theme->hC)};
}

QSize TswDisclosure::minimumSizeHint() const
{
	return sizeHint();
}

void TswDisclosure::paintEvent(QPaintEvent *event)
{
	(void)event;
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	/* Muted like every caption; the words come forward under the mouse or the keyboard. */
	const bool reached = (underMouse() || tswHasKeyFocus(this)) && isEnabled();
	const QColor color = reached ? theme->text : theme->muted;
	const int glyph = tswGlyphSize(captionFont);
	const QRectF glyphBox(0, (height() - glyph) / 2.0, glyph, glyph);
	tswPaintGlyph(painter, isChecked() ? TswGlyph::ChevronDown : TswGlyph::Chevron, glyphBox, color);
	painter.setFont(captionFont);
	painter.setPen(color);
	painter.drawText(QPointF(glyph + theme->s2, tswCapBaseline(captionFont, 0, height())), text());
	if (tswHasKeyFocus(this))
		tswPaintFocusRing(painter, QRectF(rect()), theme->s2, theme->shape(theme->focus, theme->panel), 0.0,
				  TswRing::Solid);
}

/* --- TswDetails --- */

TswDetails::TswDetails(QWidget *parent, const TswTheme *style, TswText translate)
	: QWidget(parent),
	  theme(style),
	  lookup(translate)
{
	setObjectName("tswDetails");
	disclosure = new TswDisclosure(this, theme);
	disclosure->setText(QString::fromUtf8(lookup("Details.Toggle")).toUpper());
	disclosure->setAccessibleName(QString::fromUtf8(lookup("Details.Toggle")));
	card = new TswCard(this, theme);
	card->setObjectName("tswDetailsCard");
	card->hide();
	cardLayout = new QVBoxLayout(card);
	buildRows();
	auto *column = new QVBoxLayout(this);
	column->setContentsMargins(0, 0, 0, 0);
	column->addWidget(disclosure, 0, Qt::AlignLeft);
	column->addWidget(card);
	connect(disclosure, &QAbstractButton::toggled, this, [this](bool open) {
		card->setVisible(open);
		emit openChanged(open);
	});
}

/* The rows of values in their groups, then the speed map with its caption. */
void TswDetails::buildRows()
{
	for (int group = 0; group < GROUPS; group++) {
		if (group > 0) {
			auto *rule = new QWidget(card);
			rule->setFixedHeight(1);
			rules.append(rule);
			cardLayout->addWidget(rule);
		}
		auto *grid = new QGridLayout;
		grid->setContentsMargins(0, 0, 0, 0);
		grid->setColumnStretch(0, 1);
		groups.append(grid);
		cardLayout->addLayout(grid);
	}
	for (const RowSpec &spec : ROWS)
		addRow(spec.key, spec.group);
	auto *rule = new QWidget(card);
	rule->setFixedHeight(1);
	rules.append(rule);
	cardLayout->addWidget(rule);
	mapBox = new QWidget(card);
	mapCaption = new TswReservedLabel(mapBox);
	mapCaption->setText(QString::fromUtf8(lookup("Details.Map")).toUpper());
	speedMap = new TswSpeedMap(mapBox, theme);
	speedMap->setObjectName("tswDetailsMap");
	speedMap->setAccessibleName(QString::fromUtf8(lookup("Details.Map")));
	auto *mapColumn = new QVBoxLayout(mapBox);
	mapColumn->setContentsMargins(0, 0, 0, 0);
	mapColumn->addWidget(mapCaption);
	mapColumn->addWidget(speedMap);
	cardLayout->addWidget(mapBox);
}

/* A label on the left that wraps if it must, its value on the right that never does. */
void TswDetails::addRow(const char *key, int group)
{
	QGridLayout *grid = groups[group];
	const int at = grid->rowCount();
	/* As narrow as the dock needs (its minimum stays under 200 px, so the visible width decides: 3.10);
	 * at the widths it supports a label keeps its longest word whole and wraps the rest. */
	auto *label = new TswReservedLabel(card);
	label->setText(QString::fromUtf8(lookup(key)));
	auto *value = new QLabel(card);
	value->setTextFormat(Qt::PlainText);
	value->setAlignment(Qt::AlignRight | Qt::AlignTop);
	grid->addWidget(label, at, 0, Qt::AlignTop);
	grid->addWidget(value, at, 1, Qt::AlignTop | Qt::AlignRight);
	rows.append({key, label, value});
}

void TswDetails::setOpen(bool open)
{
	const QSignalBlocker quiet(disclosure); /* a choice restored, not made: nothing to remember */
	disclosure->setChecked(open);
	card->setVisible(open);
}

bool TswDetails::isOpen() const
{
	return disclosure->isChecked();
}

TswDisclosure *TswDetails::toggle() const
{
	return disclosure;
}

/* A value's look: in the text color, or muted while it is not known. */
void TswDetails::styleValue(QLabel *value) const
{
	const TswTheme &t = *theme;
	const bool unknown = value->property("tswUnknown").toBool();
	value->setStyleSheet(QString("font-size: %1pt; color: %2;")
				     .arg(t.pointsAt(TswType::STATUS))
				     .arg((unknown ? t.ink(t.muted, t.surface) : t.ink(t.text, t.surface)).name()));
}

/* Sets a value only when it changes: the view comes ten times per second, a clock changes once per
 * second, and its style only when it becomes known. Not known: a muted "…". */
void TswDetails::setValue(int row, const QString &value)
{
	if (row < 0 || row >= rows.size())
		return;
	QLabel *label = rows[row].value;
	const QString shown = value.isEmpty() ? QString::fromUtf8(lookup(UNKNOWN_KEY)) : value;
	if (label->text() != shown)
		label->setText(shown);
	if (label->property("tswUnknown").toBool() != value.isEmpty()) {
		label->setProperty("tswUnknown", value.isEmpty());
		styleValue(label);
	}
}

QString tswSpokenMap(TswText lookup, const uint8_t map[TSW_MAP_BUCKETS])
{
	QStringList words;
	int last = -1;
	for (int i = 0; i < TSW_MAP_BUCKETS; i++) {
		const uint8_t speed = map[i];
		if (speed == last)
			continue;
		last = speed;
		if (speed == TSW_SPEED_PAUSED)
			words << QString::fromUtf8(lookup("A11y.Paused"));
		else if (speed == 1)
			words << QString::fromUtf8(lookup("Label.RealTime"));
		else
			words << QString::fromUtf8(lookup("Label.Timelapse")) + " " +
					 QString::fromUtf8(lookup("A11y.Faster")).replace("%1", QString::number(speed));
	}
	return words.join(", ");
}

void TswDetails::showView(const tsw_dock_view &view)
{
	setVisible(view.details_shown);
	if (!view.details_shown)
		return;
	const auto amount = [this](const tsw_amount &value) {
		return tswAmountText(lookup, value);
	};
	setValue(SKIPPED, QString::fromUtf8(view.waiting_skipped));
	setValue(COMPRESSION, amount(view.compression));
	setValue(REAL_TIME, QString::fromUtf8(view.real_time));
	setValue(TIMELAPSE, QString::fromUtf8(view.timelapse));
	setValue(PAUSED, QString::fromUtf8(view.paused));
	setValue(SWITCHES, QString::number(view.switches));
	setValue(PAUSES, QString::number(view.pauses));
	setValue(CHAPTERS, QString::number(view.chapters));
	setValue(FILE_SIZE, amount(view.file_size));
	const QString rate = amount(view.file_rate);
	setValue(GROWTH, rate.isEmpty() ? rate : QString::fromUtf8(lookup("Unit.PerMinute")).replace("%1", rate));
	setValue(DISK_FREE, amount(view.disk_free));
	setValue(ROOM_FOR, amount(view.time_left));
	setValue(BACKLOG, view.backlog_size == 0 ? QString()
						 : QString::fromUtf8(lookup("Details.Backlog.Value"))
							   .replace("%1", QString::number(view.backlog))
							   .replace("%2", QString::number(view.backlog_size)));
	mapBox->setVisible(view.map_shown);
	rules.back()->setVisible(view.map_shown);
	speedMap->setMap(view.map, lookup);
}

/* Small, calm type: the labels muted, the values in the text color, right-aligned digits (3.6). */
void TswDetails::restyle(bool narrow)
{
	const TswTheme &t = *theme;
	disclosure->setCaptionFont(t.fontAt(TswType::CAPTION, true));
	const int padding = narrow ? t.s3 : t.s4;
	cardLayout->setContentsMargins(padding, padding - t.s1, padding, padding);
	cardLayout->setSpacing(t.s3);
	static_cast<QVBoxLayout *>(layout())->setSpacing(t.s2);
	const QString label = QString("font-size: %1pt; color: %2;")
				      .arg(t.pointsAt(TswType::STATUS))
				      .arg(t.ink(t.muted, t.surface).name());
	for (const Row &row : rows) {
		row.label->setStyleSheet(label);
		styleValue(row.value);
	}
	for (QGridLayout *grid : groups) {
		grid->setHorizontalSpacing(t.s3);
		grid->setVerticalSpacing(t.s1);
	}
	for (QWidget *rule : rules)
		rule->setStyleSheet(QString("background: %1;").arg(t.hairline.name()));
	mapCaption->setStyleSheet(QString("font-size: %1pt; font-weight: 700; color: %2;")
					  .arg(t.pointsAt(TswType::CAPTION))
					  .arg(t.ink(t.muted, t.surface).name()));
	mapBox->layout()->setSpacing(t.s2); /* a caption sits close to what it names (3.7) */
	speedMap->updateGeometry();
	speedMap->update();
}

/* --- TswMenuButton --- */

TswMenuButton::TswMenuButton(QWidget *parent, const TswTheme *style) : QToolButton(parent), theme(style)
{
	setObjectName("tswMenu");
	setAttribute(Qt::WA_Hover);
	setFocusPolicy(Qt::StrongFocus);
	setPopupMode(QToolButton::InstantPopup);
	setAutoRaise(true);
}

void TswMenuButton::setUnder(const QColor &color)
{
	if (color == under)
		return;
	under = color;
	update();
}

QColor TswMenuButton::underColor() const
{
	return under.isValid() ? under : theme->panel;
}

QColor TswMenuButton::glyphColor() const
{
	return theme->shape(theme->muted, underColor());
}

void TswMenuButton::paintEvent(QPaintEvent *event)
{
	(void)event;
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	const QRectF box(rect());
	const QColor face = underColor();
	/* Quiet until reached: a circle only under the mouse, and a firmer one while pressed or while its
	 * menu is open, so the menu reads as coming from it (4.2, dismiss). */
	if (isDown())
		tswPaintRounded(painter, box, box.height() / 2.0, theme->hover(theme->hover(face)), QPen(Qt::NoPen));
	else if (underMouse() && isEnabled())
		tswPaintRounded(painter, box, box.height() / 2.0, theme->hover(face), QPen(Qt::NoPen));
	const double glyph = std::round(box.height() * MENU_GLYPH);
	tswPaintGlyph(painter, TswGlyph::More,
		      QRectF(box.center().x() - glyph / 2.0, box.center().y() - glyph / 2.0, glyph, glyph),
		      glyphColor());
	if (tswHasKeyFocus(this))
		tswPaintFocusRing(painter, box, box.height() / 2.0, theme->shape(theme->focus, face), 1.0,
				  TswRing::Solid);
}

int tswMenuButtonSize(const TswTheme &theme)
{
	return static_cast<int>(std::lround(MENU_ROWS * theme.L));
}

int tswMenuButtonInset(const TswTheme &theme)
{
	const int size = tswMenuButtonSize(theme);
	return (size - static_cast<int>(std::lround(size * MENU_GLYPH))) / 2;
}
