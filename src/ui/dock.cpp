/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * dock.cpp: see dock.hpp. The dock builds its zones once, reads the theme (restyle), and then
 * only changes what the view says. Colors come only from the theme through the contrast guard
 * (UX-016); every state has its own word, glyph and shape, never color alone (UX-003).
 */

#include "ui/dock.hpp"

#include <QAccessible>
#include <QApplication>
#include <QBoxLayout>
#include <QButtonGroup>
#include <QClipboard>
#include <QDesktopServices>
#include <QEvent>
#include <QFileInfo>
#include <QFocusEvent>
#include <QLabel>
#include <QRegularExpression>
#include <QScrollArea>
#include <QShortcut>
#include <QSpacerItem>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <cmath>
#include <cstring>

#include "ui/announce.hpp"
#include "ui/dock-paint.hpp"
#include "ui/dock-widgets.hpp"

namespace {

const int ENTER_NARROW = 260; /* px of visible width: narrower than this, the narrow layout (UX-012) */
const int LEAVE_NARROW = 276; /* and back at this width: 16 px of hysteresis, so it never flickers */
const int SPEEDS[] = {2, 4, 8, 16};
const uint32_t WIDEST_SPEED = 16;
const int START_REAL_TIME = 0; /* ids of the start-mode segments */
const int START_TIMELAPSE = 1;
const double MAIN_ROWS =
	2.75; /* x L: the main button's height, above UX-107's 2.5: an easy target with paint on the hands */
const double CONTROL_ROWS = 1.8; /* x L: Stop and the rails */
const double SMALL_ROWS = 1.6;   /* x L: card pills and the dismiss button */
const char ERROR_PAGE[] = "https://miguetronic.github.io/timelapse-switch/errors/TSW-E%1"; /* ADR-010 */
const char ARROW[] = " \u2192 ";

QString threeDigits(uint32_t code)
{
	return QString("%1").arg(code, 3, 10, QChar('0'));
}

bool isKey(const char *key, const char *expected)
{
	return key != nullptr && std::strcmp(key, expected) == 0;
}

/* Sets a widget's own style sheet only when it changes: a new sheet repolishes the widget. */
void setSheet(QWidget *widget, const QString &sheet)
{
	if (widget->styleSheet() != sheet)
		widget->setStyleSheet(sheet);
}

int rows(const TswTheme &theme, double factor)
{
	return std::max(static_cast<int>(std::lround(factor * theme.L)), theme.hC);
}

/* "READY" read aloud as "Ready": all-capital words are sometimes spelled out by screen readers. */
QString sentence(const QString &word)
{
	return word.isEmpty() ? word : word.left(1).toUpper() + word.mid(1).toLower();
}

} // namespace

TswDock::TswDock(TswText translate, QWidget *parent) : QFrame(parent), lookup(translate), probes(this)
{
	setObjectName("tswDock");
	buildZones();
	connectInputs();
	watchKeyFocus();
	auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
	escape->setContext(Qt::WidgetWithChildrenShortcut);
	connect(escape, &QShortcut::activated, this, [this]() {
		if (view.card_shown)
			emit dismissRequested(); /* UX-102 */
	});
}

QString TswDock::text(const char *key) const
{
	return key != nullptr ? QString::fromUtf8(lookup(key)) : QString();
}

QString TswDock::text(const char *key, const QString &argument) const
{
	/* A speed stays with its word when a label wraps: "timelapse ×4", never "×4" alone. */
	return text(key)
		.replace("%1", argument)
		.replace(QString::fromUtf8(" \u00d7"), QString::fromUtf8("\u00a0\u00d7"));
}

QString TswDock::speedText(uint32_t speed) const
{
	return text("Speed.Value", QString::number(speed));
}

/* Words for a screen reader: "Switch to timelapse ×4" becomes "..., 4 times faster" (UX-125). */
QString TswDock::spoken(const QString &words) const
{
	static const QRegularExpression speed(QString::fromUtf8("[\\s\u00a0]*\u00d7(\\d+)"));
	const QRegularExpressionMatch match = speed.match(words);
	if (!match.hasMatch())
		return words;
	QString out = words;
	return out.replace(match.capturedStart(), match.capturedLength(),
			   ", " + text("A11y.Faster", match.captured(1)));
}

/* "0:31:40" or "52:35" as "31 minutes 40 seconds", never with the arrow sign (UX-103). */
QString TswDock::spokenDuration(const QString &clock) const
{
	QStringList parts = clock.split(':');
	if (parts.size() == 2)
		parts.prepend("0"); /* m:ss: no hours */
	if (parts.size() != 3)
		return clock;
	const char *const one[] = {"A11y.Hour.One", "A11y.Minute.One", "A11y.Second.One"};
	const char *const many[] = {"A11y.Hour.Many", "A11y.Minute.Many", "A11y.Second.Many"};
	QStringList words;
	for (int i = 0; i < 3; i++) {
		const int value = parts[i].toInt();
		if (value > 0 || (i == 2 && words.isEmpty()))
			words << (value == 1 ? text(one[i]) : text(many[i], QString::number(value)));
	}
	return words.join(' ');
}

void TswDock::buildZones()
{
	status = new TswStatusCard(this, &style);
	mainButton = new TswButton(this, &style);
	mainButton->setObjectName("tswMain");
	buildRails();
	pauseButton = new TswButton(this, &style);
	pauseButton->setObjectName("tswPause");
	stopButton = new TswButton(this, &style);
	stopButton->setObjectName("tswStop");
	/* Pause and Stop share zone E, side by side; how wide each is depends only on the width, the
	 * language and the theme (updateStopRow), so Resume appears exactly where Pause was (UX-128). */
	stopRow = new QBoxLayout(QBoxLayout::LeftToRight);
	for (TswButton *button : {pauseButton, stopButton}) {
		button->setShape(TswButton::Shape::Pill);
		QSizePolicy policy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		policy.setHeightForWidth(true);
		button->setSizePolicy(policy);
		stopRow->addWidget(button, 1);
	}
	buildCard();
	buildContext();

	/* The zones, top to bottom; they never change places (UX-002). Each gap is a spacer that the
	 * dock sizes by state and width (updateGaps), so a hidden zone leaves no hole. */
	zones = new QVBoxLayout(this);
	zones->setSpacing(0);
	const auto gap = []() {
		return new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Fixed);
	};
	gapMain = gap();
	gapCard = gap();
	gapSpeeds = gap();
	gapStop = gap();
	gapContext = gap();
	zones->addWidget(status);
	zones->addSpacerItem(gapMain);
	zones->addWidget(mainButton);
	zones->addSpacerItem(gapCard);
	zones->addWidget(card);
	zones->addSpacerItem(gapSpeeds);
	zones->addWidget(speeds);
	zones->addSpacerItem(gapStop);
	zones->addLayout(stopRow);
	zones->addSpacerItem(gapContext);
	zones->addWidget(contextRow);
	zones->addStretch();
}

void TswDock::buildRails()
{
	speeds = new QWidget(this);
	speeds->setObjectName("tswSpeeds");
	startGroup = new QWidget(speeds);
	startCaption = new TswReservedLabel(startGroup);
	startRail = new TswRail(startGroup, &style);
	startRail->setMode(TswRail::Mode::Start);
	/* Object names let a theme author or the UI test pick one control (UX-015, tests/ui). */
	TswSegment *realTime = startRail->addSegment(START_REAL_TIME, TswGlyph::Play, text("Label.RealTime"));
	realTime->setObjectName("tswStartRealTime");
	realTime->setAccessibleName(text("Label.RealTime"));
	TswSegment *timelapse = startRail->addSegment(START_TIMELAPSE, TswGlyph::FastForward, text("Label.Timelapse"));
	timelapse->setObjectName("tswStartTimelapse");
	timelapse->setAccessibleName(text("Label.Timelapse"));
	speedGroup = new QWidget(speeds);
	speedCaption = new TswReservedLabel(speedGroup);
	speedRail = new TswRail(speedGroup, &style);
	for (const int speed : SPEEDS) {
		TswSegment *segment =
			speedRail->addSegment(speed, TswGlyph::None, speedText(static_cast<uint32_t>(speed)));
		segment->setObjectName(QString("tswSpeed%1").arg(speed));
		segment->setAccessibleName(text("A11y.Speed", QString::number(speed)));
	}
	for (QWidget *group : {startGroup, speedGroup}) {
		auto *column = new QVBoxLayout(group);
		column->setContentsMargins(0, 0, 0, 0);
	}
	startGroup->layout()->addWidget(startCaption);
	startGroup->layout()->addWidget(startRail);
	speedGroup->layout()->addWidget(speedCaption);
	speedGroup->layout()->addWidget(speedRail);
	/* While recording the start mode is hidden but keeps its room (below the speeds, arrangeRails), so
	 * zone D is as tall as while paused, when it shows "Resume in": Pause and Stop never move (UX-002,
	 * UX-128). */
	QSizePolicy keep = startGroup->sizePolicy();
	keep.setRetainSizeWhenHidden(true);
	startGroup->setSizePolicy(keep);
	speedsLayout = new QVBoxLayout(speeds);
	speedsLayout->setContentsMargins(0, 0, 0, 0);
	speedsLayout->addWidget(startGroup);
	speedsLayout->addWidget(speedGroup);
}

void TswDock::buildCard()
{
	card = new TswCard(this, &style);
	card->setObjectName("tswCard");
	cardHeader = new QLabel(card);
	cardHeader->setTextFormat(Qt::PlainText);
	cardHeader->setWordWrap(true);
	dismiss = new TswButton(card, &style);
	dismiss->setObjectName("tswDismiss");
	dismiss->setShape(TswButton::Shape::Pill);
	dismiss->setContent(TswGlyph::Close, QString());
	dismiss->setAccessibleName(text("Button.Close"));
	dismiss->setToolTip(text("Button.Close"));
	figureBox = new QWidget(card);
	figure = new QLabel(figureBox);
	figure->setTextFormat(Qt::PlainText);
	figureCaption = new QLabel(figureBox);
	figureCaption->setTextFormat(Qt::PlainText);
	figureCaption->setWordWrap(true);
	figureCaption->setText(text("Summary.Skipped.Caption").toUpper());
	auto *figureColumn = new QVBoxLayout(figureBox);
	figureColumn->setContentsMargins(0, 0, 0, 0);
	figureColumn->setSpacing(0);
	figureColumn->addWidget(figure);
	figureColumn->addWidget(figureCaption);
	cardTitle = new QLabel(card);
	cardTitle->setWordWrap(true);
	cardTitle->setTextFormat(Qt::PlainText);
	cardBody = new QLabel(card);
	cardBody->setWordWrap(true);
	cardTodo = new QLabel(card);
	cardTodo->setWordWrap(true);
	buildCardActions();

	/* The ticket, top to bottom: a small caption and the dismiss button, the content, the actions. */
	auto *header = new QHBoxLayout;
	header->setContentsMargins(0, 0, 0, 0);
	header->addWidget(cardHeader, 1, Qt::AlignVCenter);
	header->addWidget(dismiss, 0, Qt::AlignVCenter);
	cardLayout = new QVBoxLayout(card);
	cardLayout->addLayout(header);
	cardLayout->addWidget(figureBox);
	cardLayout->addWidget(cardTitle);
	cardLayout->addWidget(cardBody);
	cardLayout->addWidget(cardTodo);
	cardLayout->addLayout(cardActions);
	cardLayout->addWidget(linkRow);
}

/* Open folder and Play, then the links Copy details and Learn more. */
void TswDock::buildCardActions()
{
	openFolder = new TswButton(card, &style);
	openFolder->setObjectName("tswOpenFolder");
	play = new TswButton(card, &style);
	play->setObjectName("tswPlay");
	for (TswButton *pill : {openFolder, play}) {
		pill->setShape(TswButton::Shape::Pill);
		/* Side by side only when they fit at the visible width (updateCardRows); so they never make
		 * the dock wider than what shows (3.10). */
		QSizePolicy policy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		policy.setHeightForWidth(true);
		pill->setSizePolicy(policy);
	}
	openFolder->setContent(TswGlyph::Folder, text("Button.OpenFolder"));
	openFolder->setAccessibleName(text("Button.OpenFolder"));
	play->setContent(TswGlyph::Play, text("Button.Play"));
	play->setAccessibleName(text("Button.Play"));
	cardActions = new QBoxLayout(QBoxLayout::LeftToRight);
	cardActions->addWidget(openFolder);
	cardActions->addWidget(play);
	linkRow = new QWidget(card);
	copyDetails = new TswLink(linkRow, &style, TswGlyph::Copy);
	copyDetails->setObjectName("tswCopyDetails");
	copyDetails->setLabel(text("Button.CopyDetails"));
	learnMore = new TswLink(linkRow, &style, TswGlyph::LinkOut);
	learnMore->setObjectName("tswLearnMore");
	learnMore->setLabel(text("Button.LearnMore"));
	auto *links = new QBoxLayout(QBoxLayout::LeftToRight, linkRow);
	links->setContentsMargins(0, 0, 0, 0);
	/* At their own width, also one under the other: a link is its words, not the row. */
	links->addWidget(copyDetails, 0, Qt::AlignLeft);
	links->addWidget(learnMore, 0, Qt::AlignLeft);
	links->addStretch();
}

void TswDock::buildContext()
{
	contextRow = new QWidget(this);
	contextRow->setObjectName("tswContext");
	contextIcon = new TswGlyphIcon(contextRow);
	context = new QLabel(contextRow);
	context->setWordWrap(true);
	context->setTextFormat(Qt::PlainText);
	auto *line = new QHBoxLayout(contextRow);
	line->setContentsMargins(0, 0, 0, 0);
	line->addWidget(contextIcon, 0, Qt::AlignTop);
	line->addWidget(context, 1);
}

void TswDock::connectInputs()
{
	connect(mainButton, &QAbstractButton::clicked, this, [this]() { emit inputRequested(TSW_INPUT_MAIN, 0); });
	connect(pauseButton, &QAbstractButton::clicked, this, [this]() { emit inputRequested(TSW_INPUT_PAUSE, 0); });
	connect(stopButton, &QAbstractButton::clicked, this, [this]() { emit inputRequested(TSW_INPUT_STOP, 0); });
	connect(speedRail->group(), &QButtonGroup::idClicked, this,
		[this](int speed) { emit inputRequested(TSW_INPUT_SPEED_CHIP, static_cast<unsigned>(speed)); });
	connect(startRail->group(), &QButtonGroup::idClicked, this, [this](int id) {
		emit inputRequested(id == START_TIMELAPSE ? TSW_INPUT_START_TIMELAPSE : TSW_INPUT_START_REAL_TIME, 0);
	});
	connect(dismiss, &QAbstractButton::clicked, this, [this]() { emit dismissRequested(); });
	connect(openFolder, &QAbstractButton::clicked, this,
		[this]() { QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath())); });
	connect(play, &QAbstractButton::clicked, this,
		[this]() { QDesktopServices::openUrl(QUrl::fromLocalFile(path)); });
	connect(copyDetails, &QAbstractButton::clicked, this, [this]() { copyErrorDetails(); });
	connect(learnMore, &QAbstractButton::clicked, this,
		[this]() { QDesktopServices::openUrl(QUrl(QString(ERROR_PAGE).arg(threeDigits(view.error_texts)))); });
}

/* The keyboard focus ring shows only for keyboard focus (UX-110). */
void TswDock::watchKeyFocus()
{
	QList<QWidget *> watched{mainButton,  dismiss,   openFolder,  play,
				 copyDetails, learnMore, pauseButton, stopButton};
	for (TswRail *rail : {startRail, speedRail}) {
		for (QAbstractButton *segment : rail->group()->buttons())
			watched << segment;
	}
	for (QWidget *widget : watched)
		widget->installEventFilter(this);
	updateTabOrder(true);
}

/* Tab follows the eye (UX-102): the rails in the order they show (arrangeRails). */
void TswDock::updateTabOrder(bool startFirst)
{
	QList<QWidget *> order{mainButton, dismiss, openFolder, play, copyDetails, learnMore};
	for (TswRail *rail : {startFirst ? startRail : speedRail, startFirst ? speedRail : startRail}) {
		for (QAbstractButton *segment : rail->group()->buttons())
			order << segment;
	}
	order << pauseButton << stopButton;
	for (int i = 1; i < order.size(); i++)
		setTabOrder(order[i - 1], order[i]);
}

QScrollArea *TswDock::inScrollArea(TswDock *dock)
{
	auto *area = new QScrollArea;
	area->setWidget(dock);
	area->setWidgetResizable(true);
	area->setFrameShape(QFrame::NoFrame);
	area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	/* Narrow or wide follows what is visible, never the dock's own width (UX-012). */
	area->viewport()->installEventFilter(dock);
	return area;
}

void TswDock::setReduceMotion(bool reduce)
{
	reduceMotion = reduce;
	if (!reduce)
		return;
	for (TswRail *rail : {startRail, speedRail})
		rail->finishMotion(); /* one under way ends at once */
}

const TswTheme &TswDock::theme() const
{
	return style;
}

bool TswDock::isNarrow() const
{
	return narrow;
}

/* --- Theme author hooks (UX-015) --- */

QColor TswDock::accentColor() const
{
	return hooks.accent;
}

void TswDock::setAccentColor(const QColor &color)
{
	hooks.accent = color;
	scheduleRestyle();
}

QColor TswDock::dangerColor() const
{
	return hooks.danger;
}

void TswDock::setDangerColor(const QColor &color)
{
	hooks.danger = color;
	scheduleRestyle();
}

QColor TswDock::successColor() const
{
	return hooks.success;
}

void TswDock::setSuccessColor(const QColor &color)
{
	hooks.success = color;
	scheduleRestyle();
}

QColor TswDock::warningColor() const
{
	return hooks.warning;
}

void TswDock::setWarningColor(const QColor &color)
{
	hooks.warning = color;
	scheduleRestyle();
}

QColor TswDock::surfaceColor() const
{
	return hooks.surface;
}

void TswDock::setSurfaceColor(const QColor &color)
{
	hooks.surface = color;
	scheduleRestyle();
}

/* --- Following the theme (3.4) --- */

bool TswDock::event(QEvent *event)
{
	const bool handled = QFrame::event(event);
	/* OBS sends no theme event for the theme it starts with: the first pass runs on first polish,
	 * in place, so the first paint already has the theme's colors. */
	if (event->type() == QEvent::Polish && !styled && status != nullptr)
		restyle();
	else if (event->type() == QEvent::ParentChange || event->type() == QEvent::Show)
		scheduleRestyle(); /* now inside OBS's dock: what is behind it may have changed */
	return handled;
}

void TswDock::changeEvent(QEvent *event)
{
	QFrame::changeEvent(event);
	switch (event->type()) {
	case QEvent::StyleChange:
	case QEvent::PaletteChange:
	case QEvent::FontChange:
	case QEvent::ApplicationPaletteChange:
		scheduleRestyle(); /* nothing else here: one switch brings about seven of these */
		break;
	default:
		break;
	}
}

void TswDock::scheduleRestyle()
{
	if (restyling || restylePending)
		return;
	restylePending = true;
	QTimer::singleShot(0, this, &TswDock::restyle);
}

bool TswDock::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == parentWidget() && event->type() == QEvent::Resize)
		updateNarrow(parentWidget()->width());
	if (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut) {
		const auto *focus = static_cast<QFocusEvent *>(event);
		const Qt::FocusReason reason = focus->reason();
		const bool byKey = event->type() == QEvent::FocusIn &&
				   (reason == Qt::TabFocusReason || reason == Qt::BacktabFocusReason ||
				    reason == Qt::ShortcutFocusReason);
		auto *widget = static_cast<QWidget *>(watched);
		widget->setProperty("tswKeyFocus", byKey);
		widget->update();
	}
	return QFrame::eventFilter(watched, event);
}

/*
 * One pass over the theme: read the probes, derive tokens and sizes, apply them. Events that
 * arrive meanwhile are ignored; nothing here reads back a color the dock set itself (3.4).
 */
void TswDock::restyle()
{
	restylePending = false;
	if (restyling)
		return; /* reading the probes can polish the dock again: one pass at a time */
	restyling = true;
	style = probes.read(parentWidget(), hooks);
	styled = true;
	applyReserves();
	applyStyleSheets();
	applyMetrics();
	if (parentWidget() != nullptr)
		updateNarrow(parentWidget()->width());
	updateCardRows(); /* the fonts may have changed */
	updateStopRow();
	refresh(false);
	update();
	restyling = false;
}

/* Every text a zone may show, so each zone is as tall as its tallest (UX-002). */
void TswDock::applyReserves()
{
	const QString widest = QString::number(WIDEST_SPEED);
	TswStatusReserve reserve;
	reserve.realTimeHero = text("State.Hero.RealTime");
	reserve.timelapseHero = text("State.Hero.Timelapse");
	reserve.quietHeroes = {text("State.Hero.StartsRealTime"),
			       text("State.Hero.StartsTimelapse", widest),
			       text("State.Hero.ResumesRealTime"),
			       text("State.Hero.ResumesTimelapse", widest),
			       text("State.Hero.Starting"),
			       text("State.Hero.Saving"),
			       text("State.Hero.Saved")};
	reserve.headlines = {text("Error.E001.Headline"), text("Error.E002.Headline"), text("Error.E103.Headline"),
			     text("Error.E901.Headline")};
	reserve.widestSpeed = speedText(WIDEST_SPEED);
	reserve.stretchLabel = text("Counter.Stretch") + " " + speedText(WIDEST_SPEED);
	reserve.stretchValues = QString("0:00:00") + QString::fromUtf8(ARROW) + "0:00:00";
	status->setReserve(reserve);
	mainButton->setReserve({text("Action.Record"), text("Action.ToTimelapse", widest), text("Action.ToRealTime"),
				text("Action.ResumeRealTime"), text("Action.ResumeTimelapse", widest)});
	pauseButton->setReserve({text("Action.Pause"), text("Action.Resume")});
	stopButton->setReserve({text("Action.Stop")});
	startCaption->setReserve({text("Label.StartIn").toUpper(), text("Label.ResumeIn").toUpper()});
	speedCaption->setReserve(
		{text("Label.Speed").toUpper(), text("Label.NextSpeed").toUpper(), text("Label.SpeedNow").toUpper()});
}

/* Sizes of the labels that are not painted, through their own style sheets (3.6): setFont would
 * lose against Yami's "QWidget { font-size }". Text colors pass the contrast guard (UX-111). */
void TswDock::applyStyleSheets()
{
	const TswTheme &t = style;
	const QString caption = QString("font-size: %1pt; font-weight: 700; color: %2;")
					.arg(t.pointsAt(TswType::CAPTION))
					.arg(t.muted.name());
	setSheet(startCaption, caption);
	setSheet(speedCaption, caption);
	setSheet(context, QString("font-size: %1pt; color: %2;").arg(t.pointsAt(TswType::CONTEXT)).arg(t.muted.name()));
	const QString onCard = t.ink(t.text, t.surface).name();
	const QString cardCaption = QString("font-size: %1pt; font-weight: 700; color: %2;")
					    .arg(t.pointsAt(TswType::CAPTION))
					    .arg(t.ink(t.muted, t.surface).name());
	setSheet(cardHeader, cardCaption);
	setSheet(figureCaption, cardCaption);
	setSheet(figure, QString("font-size: %1pt; font-weight: 700; color: %2;")
				 .arg(t.pointsAt(TswType::FIGURE))
				 .arg(t.ink(t.accent, t.surface).name()));
	for (QLabel *body : {cardTitle, cardBody, cardTodo})
		setSheet(body, QString("font-size: %1pt; color: %2;").arg(t.pointsAt(TswType::BODY)).arg(onCard));
}

/* Margins, gaps and control sizes from the theme's font and density, wide or narrow (3.7, 4.2). */
void TswDock::applyMetrics()
{
	const TswTheme &t = style;
	const int margin = narrow ? t.s2 : t.s3;
	zones->setContentsMargins(margin, margin, margin, margin);
	status->setNarrow(narrow);
	status->updateGeometry();
	mainButton->setLabelFont(t.fontAt(TswType::MAIN, true));
	mainButton->setMinimumRow(static_cast<int>(std::lround(MAIN_ROWS * t.L)));
	for (TswButton *button : {pauseButton, stopButton}) {
		button->setLabelFont(t.fontAt(TswType::STOP, false));
		button->setMinimumRow(rows(t, CONTROL_ROWS));
	}
	stopRow->setSpacing(t.s3);
	for (TswButton *pill : {openFolder, play}) {
		pill->setLabelFont(t.fontAt(TswType::BODY, false));
		pill->setMinimumRow(rows(t, SMALL_ROWS));
	}
	/* Narrow: tighter sides, so labels keep their words on fewer lines at 220 px. */
	for (TswButton *button : {mainButton, pauseButton, stopButton, openFolder, play})
		button->setPadding(narrow ? t.s3 : t.s4);
	const int circle = static_cast<int>(std::lround(SMALL_ROWS * t.L));
	dismiss->setLabelFont(t.fontAt(TswType::BODY, false));
	dismiss->setMinimumRow(circle);
	dismiss->setFixedSize(circle, circle);
	for (TswLink *link : {copyDetails, learnMore})
		link->setLinkFont(t.fontAt(TswType::LINK, false));
	for (QWidget *group : {startGroup, speedGroup})
		static_cast<QVBoxLayout *>(group->layout())->setSpacing(t.s2);
	speedsLayout->setSpacing(narrow ? t.s3 : t.s4);
	const int padding = narrow ? t.s3 : t.s4;
	cardLayout->setContentsMargins(padding, padding - t.s1, padding - t.s1, padding);
	cardLayout->setSpacing(t.s3);
	cardActions->setSpacing(t.s3);
	figureBox->layout()->setSpacing(t.s1);

	contextRow->layout()->setSpacing(t.s2);
	const int line = QFontMetrics(t.fontAt(TswType::CONTEXT, false)).height();
	contextIcon->setGlyph(TswGlyph::Info, t.muted, QSize(tswGlyphSize(t.fontAt(TswType::CONTEXT, false)), line));
	for (TswRail *rail : {startRail, speedRail})
		rail->updateGeometry();
	updateGaps();
}

/* Narrow when the visible width is under 260 px, wide again from 276 px (UX-012). */
void TswDock::updateNarrow(int viewportWidth)
{
	const bool next = narrow ? viewportWidth < LEAVE_NARROW : viewportWidth < ENTER_NARROW;
	if (next == narrow)
		return;
	narrow = next;
	applyMetrics();
	updateCardRows();
	updateStopRow();
}

/*
 * The scroll area asks the dock's height for the viewport's width, but some themes then give the
 * dock less width than that (Classic: 8 px less, from the scroll area's style-sheet frame), and a
 * narrower dock may need more height (columns that stack, labels that wrap). So the dock holds its
 * minimum height at what its real width needs; the scroll area then scrolls instead of squeezing
 * (UX-121: nothing cut).
 */
void TswDock::resizeEvent(QResizeEvent *event)
{
	QFrame::resizeEvent(event);
	updateCardRows();
	updateStopRow();
	const int needed = heightForWidth(width());
	if (needed > 0 && minimumHeight() != needed)
		setMinimumHeight(needed);
}

/*
 * Card rows go one under the other when they do not fit side by side at the dock's width (some
 * themes inset the dock inside the visible area: Classic by 8 px). The two pills share the row
 * equally, so each must fit half of it: a label that would wrap in its half stacks both, full
 * width, instead (3.10).
 */
void TswDock::updateCardRows()
{
	const int margin = narrow ? style.s2 : style.s3;
	const int padding = narrow ? style.s3 : style.s4;
	const int room = width() - 2 * margin - 2 * padding + style.s1;
	const int half = (room - style.s3) / 2;
	const bool pillsFit = !narrow && openFolder->labelLines(half) == 1 && play->labelLines(half) == 1;
	cardActions->setDirection(pillsFit ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
	const int links = copyDetails->sizeHint().width() + style.s3 + learnMore->sizeHint().width();
	auto *linkLayout = static_cast<QBoxLayout *>(linkRow->layout());
	linkLayout->setDirection(links > room ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
	linkLayout->setSpacing(links > room ? 0 : style.s3); /* one under the other: a list, not islands */
}

/*
 * Zone E: Pause and Stop in equal halves while every label they may show fits its half on one line.
 * Otherwise Pause takes what its longer word needs (Pause or Resume) and Stop the rest, so "Stop and
 * save" keeps one line wherever it can. Only when even that would cut a word (the narrowest dock at
 * the largest font) do they stack, full width, as the card's pills do (3.10). A label that still
 * wraps turns both into rounded rectangles: a two-line pill would be a tall stadium (4.2, shapes).
 * The arrangement depends only on the width, the language and the theme, so Resume is exactly where
 * Pause was (UX-128).
 */
void TswDock::updateStopRow()
{
	const int margin = narrow ? style.s2 : style.s3;
	const int room = width() - 2 * margin - style.s3;
	const int half = room / 2;
	const int pauseNeeds = pauseButton->oneLineWidth() + style.s2; /* a pill's round ends eat into its padding */
	const bool halves = pauseButton->oneLineWidth() <= half && stopButton->oneLineWidth() <= room - half;
	const bool beside = halves || stopButton->minimumSizeHint().width() <= room - pauseNeeds;
	const bool fixedPause = beside && !halves;
	/* Ignored: neither widens the dock (3.10). Fixed when Pause takes its own width: the layout would
	 * give an Ignored button no width at all. */
	QSizePolicy policy = pauseButton->sizePolicy();
	policy.setHorizontalPolicy(fixedPause ? QSizePolicy::Fixed : QSizePolicy::Ignored);
	pauseButton->setSizePolicy(policy);
	pauseButton->setMinimumWidth(fixedPause ? pauseNeeds : 0);
	pauseButton->setMaximumWidth(fixedPause ? pauseNeeds : QWIDGETSIZE_MAX);
	/* Stretch shares the width beside each other; stacked it would share out the dock's spare height. */
	stopRow->setStretch(0, beside && !fixedPause ? 1 : 0);
	stopRow->setStretch(1, beside ? 1 : 0);
	stopRow->setDirection(beside ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
	const int full = room + style.s3; /* stacked, each takes the whole row */
	int pauseWidth = full;
	if (beside)
		pauseWidth = fixedPause ? pauseNeeds : half;
	const int stopWidth = beside ? room - pauseWidth : full;
	const bool wraps = pauseButton->mostLines(pauseWidth) > 1 || stopButton->mostLines(stopWidth) > 1;
	for (TswButton *button : {pauseButton, stopButton})
		button->setShape(wraps ? TswButton::Shape::Rounded : TswButton::Shape::Pill);
}

void TswDock::updateGaps()
{
	const int group = narrow ? style.s3 : style.s4;
	const auto setGap = [](QSpacerItem *gap, int height) {
		gap->changeSize(0, height, QSizePolicy::Minimum, QSizePolicy::Fixed);
	};
	setGap(gapMain, group);
	setGap(gapCard, card->isHidden() ? 0 : group);
	setGap(gapSpeeds, speeds->isHidden() ? 0 : group);
	/* Stop sits at least one row away from the rails: the separation of UX-107, without a rule. */
	setGap(gapStop, stopButton->isHidden() ? 0 : std::max(style.L, group));
	setGap(gapContext, contextRow->isHidden() ? 0 : style.s3);
	zones->invalidate();
}

/* --- Showing a view --- */

void TswDock::showView(const tsw_dock_view &next)
{
	view = next;
	path = QString::fromUtf8(next.context_path != nullptr ? next.context_path : "");
	view.context_path = nullptr;                     /* borrowed from the recorder: only the copy above is kept */
	const bool animate = viewShown && !reduceMotion; /* only changes the user caused, never the first view */
	refresh(animate);
	announce();
	viewShown = true;
}

void TswDock::refresh(bool animate)
{
	showStatus();
	showMain();
	showRails(animate);
	showStop();
	showPause();
	showCard();
	showContext();
	updateGaps();
}

void TswDock::showStatus()
{
	TswStatusContent content;
	content.banner = view.banner;
	content.tally = text(view.tally_text);
	content.hero = text(view.hero_text, QString::number(view.hero_speed));
	content.quiet = view.hero_quiet;
	content.boldQuiet = view.banner == TSW_BANNER_ERROR;
	if (view.banner == TSW_BANNER_TIMELAPSE) {
		content.speed = speedText(view.hero_speed);
		content.speedValue = view.hero_speed;
	}
	if (view.banner == TSW_BANNER_READY || view.banner == TSW_BANNER_PAUSED) /* how it starts, or resumes */
		content.heroGlyph = view.start_in_timelapse ? TswGlyph::FastForward : TswGlyph::Play;
	else if (view.banner == TSW_BANNER_REAL_TIME)
		content.heroGlyph = TswGlyph::Play;
	else if (view.banner == TSW_BANNER_TIMELAPSE)
		content.heroGlyph = TswGlyph::FastForward;
	content.recordedCaption = text("Counter.Recorded.Caption");
	content.finalCaption = text("Counter.Final.Caption");
	content.recorded = QString::fromUtf8(view.recorded);
	content.finalLength = QString::fromUtf8(view.final_length);
	content.dimmed = view.counters_dimmed;
	if (view.section_shown) {
		const bool sped = view.section_speed > 1;
		content.stretchLabel =
			text(view.section_label) + (sped ? " " + speedText(view.section_speed) : QString());
		content.stretchValues = QString::fromUtf8(view.section_recorded);
		if (sped)
			content.stretchValues += QString::fromUtf8(ARROW) + QString::fromUtf8(view.section_final);
	} else if (view.card_shown && !path.isEmpty()) {
		/* After a take, the status line names its file: the card below acts on it. The words before
		 * "%1" ("File:") are its label; the name, its value. */
		const QString hint = text("Hint.File");
		content.stretchLabel = hint.left(hint.indexOf("%1")).trimmed();
		content.stretchValues = QFileInfo(path).fileName();
	}
	status->setContent(content);

	/* For screen readers: the state block's name is the tally and the hero; its description the
	 * readout in words, updated without announcing it (3.11). */
	QString hero = spoken(content.hero);
	if (view.banner == TSW_BANNER_REAL_TIME)
		hero = text("Label.RealTime");
	else if (view.banner == TSW_BANNER_TIMELAPSE)
		hero = text("Label.Timelapse") + ", " + text("A11y.Faster", QString::number(view.hero_speed));
	const bool recording = view.banner == TSW_BANNER_REAL_TIME || view.banner == TSW_BANNER_TIMELAPSE;
	const QString name = (recording ? text("A11y.Recording") : sentence(content.tally)) + ", " + hero;
	const QString description = text("A11y.Counters", spokenDuration(content.recorded))
					    .replace("%2", spokenDuration(content.finalLength));
	if (status->accessibleName() != name)
		status->setAccessibleName(name);
	if (status->accessibleDescription() != description)
		status->setAccessibleDescription(description);
}

void TswDock::showMain()
{
	const TswTheme &t = style;
	const QString words = text(view.main_text, QString::number(view.main_speed));
	TswButtonLook look{t.raised, t.panel, QColor(), 0.0, t.text, t.ink(t.text, t.raised)};
	TswGlyph glyph = TswGlyph::Record;
	if (isKey(view.main_text, "Action.ToTimelapse") || isKey(view.main_text, "Action.ResumeTimelapse")) {
		/* Outlined means next: the switch is drawn like the armed speed (4.2), and so is a resume that
		 * will run in timelapse. */
		look = {t.panel, t.panel, t.accent, 2.0, t.ink(t.accent, t.panel), t.accentText};
		glyph = TswGlyph::FastForward;
	} else if (isKey(view.main_text, "Action.ToRealTime") || isKey(view.main_text, "Action.ResumeRealTime")) {
		glyph = TswGlyph::Play;
	} else {
		look.glyph = t.shape(t.danger, t.raised);
	}
	mainButton->setLook(look);
	mainButton->setContent(glyph, words);
	mainButton->setEnabled(view.main_enabled);
	if (mainButton->accessibleName() != spoken(words))
		mainButton->setAccessibleName(spoken(words));
}

/* Idle: the start mode first, then the speeds. Recording, paused and saving: the speeds first, under
 * the main button as always, and below them the room of the mode to resume in, filled while paused,
 * right above the Resume button it qualifies. */
void TswDock::arrangeRails(bool startFirst)
{
	if ((speedsLayout->indexOf(startGroup) == 0) == startFirst)
		return;
	speedsLayout->removeWidget(startGroup);
	speedsLayout->insertWidget(startFirst ? 0 : 1, startGroup);
	updateTabOrder(startFirst);
}

void TswDock::showRails(bool animate)
{
	speeds->setVisible(view.speeds_shown);
	/* A rail that just appeared shows its choice at once; only one already shown slides to it. */
	const bool startWasShown = !startGroup->isHidden();
	arrangeRails(!view.stop_shown);
	startGroup->setVisible(view.start_mode_shown);
	startCaption->setText(text(view.start_label).toUpper());
	speedCaption->setText(text(view.speed_label).toUpper());
	const bool engaged = view.banner == TSW_BANNER_TIMELAPSE;
	speedRail->setEnabled(view.speeds_enabled);
	speedRail->setMode(engaged ? TswRail::Mode::Engaged : TswRail::Mode::Armed);
	speedRail->select(static_cast<int>(view.speed_selected), animate);
	startRail->select(view.start_in_timelapse ? START_TIMELAPSE : START_REAL_TIME, animate && startWasShown);
	/* The selected speed says whether it is the next one or the one running now (UX-103). */
	const QString role = text(engaged ? "A11y.Engaged" : "A11y.Armed");
	for (QAbstractButton *segment : speedRail->group()->buttons()) {
		const QString description = segment->isChecked() ? role : QString();
		if (segment->accessibleDescription() != description)
			segment->setAccessibleDescription(description);
	}
}

void TswDock::showStop()
{
	const TswTheme &t = style;
	stopButton->setLook({QColor(), t.panel, t.edge, 1.0, t.text, t.text});
	stopButton->setContent(TswGlyph::Stop, text("Action.Stop"));
	if (stopButton->accessibleName() != text("Action.Stop"))
		stopButton->setAccessibleName(text("Action.Stop"));
	stopButton->setVisible(view.stop_shown);
	stopButton->setEnabled(view.stop_enabled);
}

/* Pause while recording; while paused, Resume in the same place, with the glyph of the mode it will
 * resume in, as on the main button (UX-128, UX-130). */
void TswDock::showPause()
{
	const TswTheme &t = style;
	const bool resume = isKey(view.pause_text, "Action.Resume");
	TswGlyph glyph = TswGlyph::Pause;
	if (resume)
		glyph = view.start_in_timelapse ? TswGlyph::FastForward : TswGlyph::Play;
	pauseButton->setLook({QColor(), t.panel, t.edge, 1.0, t.text, t.text});
	pauseButton->setContent(glyph, text(view.pause_text));
	const QString name = text(resume ? "A11y.Resume" : "A11y.Pause");
	if (pauseButton->accessibleName() != name)
		pauseButton->setAccessibleName(name);
	pauseButton->setVisible(view.pause_shown);
	pauseButton->setEnabled(view.pause_enabled);
}

void TswDock::showCard()
{
	card->setVisible(view.card_shown);
	if (!view.card_shown)
		return;
	const TswTheme &t = style;
	const TswButtonLook pill{t.raised, t.surface, QColor(), 0.0, t.ink(t.text, t.raised), t.ink(t.text, t.raised)};
	openFolder->setLook(pill);
	play->setLook(pill);
	dismiss->setLook({QColor(), t.surface, QColor(), 0.0, t.ink(t.muted, t.surface), t.text});
	/* The error's reassurance is Bold (what is safe comes first); the summary's file name is not. */
	const bool error = view.error_code != 0;
	setSheet(cardTitle, QString("font-size: %1pt; font-weight: %2; color: %3;")
				    .arg(t.pointsAt(TswType::BODY))
				    .arg(error ? 700 : 400)
				    .arg(t.ink(t.text, t.surface).name()));
	if (error)
		showErrorCard();
	else
		showSummaryCard();
}

/*
 * The summary, as a ticket: the waiting the timelapse skipped as one big figure, the file, the one
 * warm line of the session (UX-072), and what to do with the file. A recording that never sped up
 * skipped nothing, so it shows no figure.
 */
void TswDock::showSummaryCard()
{
	cardHeader->setText(text("Card.Session").toUpper());
	const QString skipped = QString::fromUtf8(view.skipped);
	figureBox->setVisible(!skipped.isEmpty());
	figure->setText(skipped);
	const QString spokenSkipped = text("Summary.Skipped", spokenDuration(skipped));
	if (figure->accessibleName() != spokenSkipped)
		figure->setAccessibleName(spokenSkipped);
	cardTitle->hide(); /* the file is named in the status line, above */
	cardBody->setTextFormat(Qt::PlainText);
	cardBody->setText(text("Summary.Thanks"));
	cardBody->setVisible(view.thank_user);
	cardTodo->hide();
	openFolder->setVisible(!path.isEmpty());
	play->setVisible(!path.isEmpty());
	linkRow->hide();
}

/* What is safe first, then why, what to do, one fix and the links; the code is in the header, so it
 * costs no row of its own (4.2). */
void TswDock::showErrorCard()
{
	cardHeader->setText(text("Card.Error", "TSW-E" + threeDigits(view.error_code)).toUpper());
	figureBox->hide();
	const QByteArray prefix = "Error.E" + threeDigits(view.error_texts).toUtf8();
	const QString saved = text(QByteArray(prefix + ".Saved").constData())
				      .replace("%1", QString::fromUtf8(view.recorded))
				      .replace("%2", QString::fromUtf8(view.final_length));
	cardTitle->setText(saved);
	cardTitle->show();
	const auto labelled = [](const QString &label, const QString &words) {
		return "<b>" + label.toHtmlEscaped() + "</b> " + words.toHtmlEscaped();
	};
	cardBody->setTextFormat(Qt::RichText);
	cardBody->setText(labelled(text("Error.Why"), text(QByteArray(prefix + ".What").constData())));
	cardBody->show();
	cardTodo->setTextFormat(Qt::RichText);
	cardTodo->setText(labelled(text("Error.WhatToDo"), text(QByteArray(prefix + ".Todo").constData())));
	cardTodo->show();
	openFolder->setVisible(view.error_texts == 103 && !path.isEmpty()); /* the one fix: see the file */
	play->hide();
	linkRow->show();
}

void TswDock::showContext()
{
	contextRow->setVisible(view.context_text != nullptr);
	if (view.context_text == nullptr)
		return;
	context->setText(text(view.context_text, QFileInfo(path).fileName()));
}

/* Start, every switch, saved and errors are announced once each (UX-104). */
void TswDock::announce()
{
	if (view.announce_text == nullptr)
		return;
	QString mode = text("Label.RealTime");
	if (view.banner == TSW_BANNER_TIMELAPSE)
		mode = text("Label.Timelapse") + ", " + text("A11y.Faster", QString::number(view.hero_speed));
	if (view.banner == TSW_BANNER_PAUSED && view.start_in_timelapse) /* the mode it will resume in */
		mode = text("Label.Timelapse") + ", " + text("A11y.Faster", QString::number(view.main_speed));
	QString words = text(view.announce_text, mode.left(1).toLower() + mode.mid(1));
	if (view.banner == TSW_BANNER_SAVED)
		words = text(view.announce_text, spokenDuration(QString::fromUtf8(view.final_length)));
	if (view.banner == TSW_BANNER_ERROR)
		words = text(view.announce_text, text(view.hero_text)).replace("%2", cardTitle->text());
	tswAnnounce(status, words, view.banner == TSW_BANNER_ERROR);
}

void TswDock::copyErrorDetails()
{
	const QString details =
		QString("Timelapse Switch TSW-E%1 | file=\"%2\" | details: the [timelapse-switch] lines "
			"of the OBS log (Help > Log Files)")
			.arg(threeDigits(view.error_code), path);
	QApplication::clipboard()->setText(details);
}
