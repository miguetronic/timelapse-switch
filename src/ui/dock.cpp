/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * dock.cpp: see dock.hpp. Colors come only from the theme (QPalette and OBS's utility classes,
 * UX-016); every state has its own word, icon and frame style, never color alone (UX-003).
 */

#include "ui/dock.hpp"

#include <QAccessible>
#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QDesktopServices>
#include <QEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QStyle>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

namespace {

const int COMPACT_WIDTH = 260; /* UX-012: narrower than this, the dock uses shorter words */
const int SPEEDS[] = {2, 4, 8, 16};
const char ERROR_PAGE[] = "https://miguetronic.github.io/timelapse-switch/errors/TSW-E%1"; /* ADR-010 */

/* An OBS theme utility class (UX-016): set it, then let the style apply it again. */
void setThemeClass(QWidget *widget, const char *name)
{
	widget->setProperty("class", name);
	widget->style()->unpolish(widget);
	widget->style()->polish(widget);
}

/* The theme's danger color, read from a label with OBS's "text-danger" class (the text color outside OBS). */
QColor dangerColor(const QWidget *reference)
{
	QLabel probe;
	probe.setPalette(reference->palette());
	probe.setProperty("class", "text-danger");
	probe.ensurePolished();
	return probe.palette().color(QPalette::WindowText);
}

/* A larger, bold copy of a font, whether it is sized in points or pixels. */
QFont scaledFont(QFont font, double factor)
{
	if (font.pointSizeF() > 0)
		font.setPointSizeF(font.pointSizeF() * factor);
	else
		font.setPixelSize(static_cast<int>(font.pixelSize() * factor));
	font.setBold(true);
	return font;
}

/* The banner icon in one color, so it follows the theme (UX-018); none while idle. */
QPixmap drawBannerIcon(enum tsw_banner banner, const QColor &color, int size, qreal ratio)
{
	if (banner == TSW_BANNER_READY)
		return {};
	QPixmap pixmap(QSize(size, size) * ratio);
	pixmap.setDevicePixelRatio(ratio);
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setPen(QPen(color, size / 8.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
	painter.setBrush(color);
	const double s = size;
	switch (banner) {
	case TSW_BANNER_REAL_TIME: /* the record dot */
		painter.drawEllipse(QRectF(s * 0.2, s * 0.2, s * 0.6, s * 0.6));
		break;
	case TSW_BANNER_TIMELAPSE: /* fast forward: two triangles */
		for (const double left : {0.1, 0.5}) {
			const QPointF triangle[] = {{s * left, s * 0.2},
						    {s * (left + 0.4), s * 0.5},
						    {s * left, s * 0.8}};
			painter.drawPolygon(triangle, 3);
		}
		break;
	case TSW_BANNER_SAVED: { /* a check mark */
		const QPointF check[] = {{s * 0.2, s * 0.55}, {s * 0.42, s * 0.75}, {s * 0.8, s * 0.28}};
		painter.drawPolyline(check, 3);
		break;
	}
	case TSW_BANNER_ERROR: /* a cross in a circle */
		painter.setBrush(Qt::NoBrush);
		painter.drawEllipse(QRectF(s * 0.1, s * 0.1, s * 0.8, s * 0.8));
		painter.drawLine(QPointF(s * 0.35, s * 0.35), QPointF(s * 0.65, s * 0.65));
		painter.drawLine(QPointF(s * 0.65, s * 0.35), QPointF(s * 0.35, s * 0.65));
		break;
	case TSW_BANNER_BUSY: /* three dots */
		for (const double x : {0.2, 0.5, 0.8})
			painter.drawEllipse(QPointF(s * x, s * 0.5), s * 0.07, s * 0.07);
		break;
	case TSW_BANNER_READY:
		break;
	}
	return pixmap;
}

/* The codes the dock has texts for; any other one is shown as the unexpected-error message. */
uint32_t knownError(uint32_t code)
{
	return (code == 1 || code == 2 || code == 103) ? code : 901;
}

QString threeDigits(uint32_t code)
{
	return QString("%1").arg(code, 3, 10, QChar('0'));
}

} // namespace

TswDock::TswDock(TswText translate, QWidget *parent) : QFrame(parent), lookup(translate)
{
	setObjectName("tswDock");
	buildBanner();
	buildCounters();
	buildCard();
	buildButtons();
	buildSpeedRow();
	context = new QLabel(this);
	context->setWordWrap(true);
	setThemeClass(context, "text-muted");

	/* Names of the zones, for the UI test and for themes that want to style them. */
	counters->setObjectName("tswCounters");
	section->setObjectName("tswSection");
	card->setObjectName("tswCard");
	mainButton->setObjectName("tswMain");
	speedRow->setObjectName("tswSpeeds");
	stopButton->setObjectName("tswStop");
	context->setObjectName("tswContext");

	/* The zones, top to bottom; they never change places (UX-002). Stop sits one row apart (UX-107). */
	auto *zones = new QVBoxLayout(this);
	zones->addWidget(banner);
	zones->addWidget(counters);
	zones->addWidget(section);
	zones->addWidget(card);
	zones->addWidget(mainButton);
	zones->addWidget(speedRow);
	zones->addSpacing(fontMetrics().height());
	zones->addWidget(stopButton, 0, Qt::AlignLeft);
	zones->addWidget(context);
	zones->addStretch();
}

QString TswDock::text(const char *key) const
{
	return key != nullptr ? QString::fromUtf8(lookup(key)) : QString();
}

QString TswDock::text(const char *key, const QString &argument) const
{
	return text(key).replace("%1", argument);
}

void TswDock::buildBanner()
{
	banner = new QFrame(this);
	banner->setObjectName("tswBanner");
	bannerIcon = new QLabel(banner);
	bannerText = new QLabel(banner);
	bannerText->setFont(scaledFont(bannerText->font(), 1.8)); /* about 2x the base font (UX spec 3.6) */
	bannerText->setWordWrap(true);
	bannerDetail = new QLabel(banner);
	bannerDetail->setWordWrap(true);
	auto *line = new QHBoxLayout;
	line->addWidget(bannerIcon, 0, Qt::AlignVCenter);
	line->addWidget(bannerText, 1);
	auto *column = new QVBoxLayout(banner);
	column->addLayout(line);
	column->addWidget(bannerDetail);
}

void TswDock::buildCounters()
{
	QFont digits = font();
	digits.setFeature(QFont::Tag("tnum"), 1); /* digits of equal width, so counters do not jitter (UX-006) */
	counters = new QLabel(this);
	counters->setFont(digits);
	counters->setWordWrap(true);
	section = new QLabel(this);
	section->setFont(digits);
	section->setWordWrap(true);
}

void TswDock::buildCard()
{
	card = new QFrame(this);
	card->setFrameShape(QFrame::StyledPanel);
	cardTitle = new QLabel(card);
	cardTitle->setFont(scaledFont(cardTitle->font(), 1.0));
	cardTitle->setWordWrap(true);
	setThemeClass(cardTitle, "text-danger");
	cardBody = new QLabel(card);
	cardBody->setWordWrap(true);
	cardBody->setTextFormat(Qt::PlainText);
	openFolder = new QPushButton(text("Button.OpenFolder"), card);
	play = new QPushButton(text("Button.Play"), card);
	copyDetails = new QPushButton(text("Button.CopyDetails"), card);
	learnMore = new QPushButton(text("Button.LearnMore"), card);
	closeCard = new QPushButton(text("Button.Close"), card);
	cardActions = new QBoxLayout(QBoxLayout::LeftToRight);
	for (QPushButton *button : {openFolder, play, copyDetails, learnMore})
		cardActions->addWidget(button);
	auto *column = new QVBoxLayout(card);
	column->addWidget(cardTitle);
	column->addWidget(cardBody);
	column->addLayout(cardActions);
	column->addWidget(closeCard, 0, Qt::AlignRight);

	connect(openFolder, &QPushButton::clicked, this,
		[this]() { QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath())); });
	connect(play, &QPushButton::clicked, this, [this]() { QDesktopServices::openUrl(QUrl::fromLocalFile(path)); });
	connect(copyDetails, &QPushButton::clicked, this, [this]() { copyErrorDetails(); });
	connect(learnMore, &QPushButton::clicked, this, [this]() {
		QDesktopServices::openUrl(QUrl(QString(ERROR_PAGE).arg(threeDigits(knownError(view.error_code)))));
	});
	connect(closeCard, &QPushButton::clicked, this, [this]() { emit dismissRequested(); });
}

void TswDock::buildButtons()
{
	mainButton = new QPushButton(this);
	mainButton->setFont(scaledFont(mainButton->font(), 1.0));
	mainButton->setMinimumHeight(fontMetrics().height() * 5 / 2); /* UX-107 */
	connect(mainButton, &QPushButton::clicked, this, [this]() { emit inputRequested(TSW_INPUT_MAIN, 0); });
	stopButton = new QPushButton(this);
	connect(stopButton, &QPushButton::clicked, this, [this]() { emit inputRequested(TSW_INPUT_STOP, 0); });
}

void TswDock::buildSpeedRow()
{
	speedRow = new QWidget(this);
	startRow = new QWidget(speedRow);
	startRealTime = new QRadioButton(text("Label.RealTime"), startRow);
	startTimelapse = new QRadioButton(text("Label.Timelapse"), startRow);
	startLayout = new QBoxLayout(QBoxLayout::LeftToRight, startRow);
	startLayout->setContentsMargins(0, 0, 0, 0);
	startLayout->addWidget(new QLabel(text("Label.StartIn"), startRow));
	startLayout->addWidget(startRealTime);
	startLayout->addWidget(startTimelapse);
	startLayout->addStretch();
	connect(startRealTime, &QRadioButton::clicked, this,
		[this]() { emit inputRequested(TSW_INPUT_START_REAL_TIME, 0); });
	connect(startTimelapse, &QRadioButton::clicked, this,
		[this]() { emit inputRequested(TSW_INPUT_START_TIMELAPSE, 0); });

	speedLabel = new QLabel(speedRow);
	chips = new QButtonGroup(this);
	auto *chipLine = new QHBoxLayout;
	chipLine->addWidget(speedLabel);
	for (const int speed : SPEEDS) {
		auto *chip = new QToolButton(speedRow);
		chip->setText(QString::fromUtf8("×%1").arg(speed));
		chip->setCheckable(true);
		chip->setMinimumHeight(fontMetrics().height() * 9 / 5); /* UX-107 */
		chip->setAccessibleName(text("A11y.Speed", QString::number(speed)));
		chips->addButton(chip, speed);
		chipLine->addWidget(chip, 1);
	}
	connect(chips, &QButtonGroup::idClicked, this,
		[this](int speed) { emit inputRequested(TSW_INPUT_SPEED_CHIP, static_cast<unsigned>(speed)); });

	auto *column = new QVBoxLayout(speedRow);
	column->setContentsMargins(0, 0, 0, 0);
	column->addWidget(startRow);
	column->addLayout(chipLine);
}

QScrollArea *TswDock::inScrollArea(TswDock *dock)
{
	auto *area = new QScrollArea;
	area->setWidget(dock);
	area->setWidgetResizable(true);
	area->setFrameShape(QFrame::NoFrame);
	area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	return area;
}

void TswDock::showView(const tsw_dock_view &next)
{
	view = next;
	path = QString::fromUtf8(next.context_path != nullptr ? next.context_path : "");
	view.context_path = nullptr; /* borrowed from the recorder: only the copy above is kept */
	refresh();
}

void TswDock::refresh()
{
	/* Narrow dock: rows of choices become columns, so every function stays (UX-012). */
	const QBoxLayout::Direction direction = compact ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight;
	startLayout->setDirection(direction);
	cardActions->setDirection(direction);
	showBanner();
	showCounters();
	showCard();
	showButtons();
	showSpeedRow();
	showContext();
}

void TswDock::applyBannerStyle()
{
	styled = true;
	styledBanner = view.banner;
	const QColor danger = dangerColor(this);
	QString frame = "border: 1px solid palette(mid);";
	QString words = "palette(window-text)";
	QColor iconColor = palette().color(QPalette::WindowText);
	if (view.banner == TSW_BANNER_BUSY) {
		frame = "border: 1px dashed palette(mid);";
	} else if (view.banner == TSW_BANNER_REAL_TIME) {
		frame = "border: 2px solid palette(window-text);"; /* outlined */
	} else if (view.banner == TSW_BANNER_TIMELAPSE) {
		frame = "border: 3px solid palette(highlight); background: palette(highlight);"; /* filled */
		words = "palette(highlighted-text)";
		iconColor = palette().color(QPalette::HighlightedText);
	} else if (view.banner == TSW_BANNER_ERROR) {
		frame = QString("border: 2px dashed %1;").arg(danger.name()); /* danger frame */
		iconColor = danger;
	}
	banner->setStyleSheet(QString("QFrame#tswBanner { %1 border-radius: 4px; }"
				      " QFrame#tswBanner QLabel { color: %2; background: transparent; border: none; }")
				      .arg(frame, words));
	const QPixmap icon =
		drawBannerIcon(view.banner, iconColor, bannerText->fontMetrics().height(), devicePixelRatioF());
	bannerIcon->setPixmap(icon);
	bannerIcon->setVisible(!icon.isNull());
}

void TswDock::showBanner()
{
	if (!styled || styledBanner != view.banner)
		applyBannerStyle();
	const QString speed = QString::number(view.banner_speed);
	const QString words = text(view.banner_text, speed);
	const bool error = view.banner == TSW_BANNER_ERROR;
	const QString detail = error ? "TSW-E" + threeDigits(view.error_code) : text(view.banner_detail, speed);
	bannerDetail->setVisible(!detail.isEmpty());
	if (bannerText->text() == words && bannerDetail->text() == detail)
		return;
	bannerText->setText(words);
	bannerDetail->setText(detail);

	/* Screen readers hear every change of state (UX-104). */
	banner->setAccessibleName(detail.isEmpty() ? words : words + ". " + detail);
	QAccessibleEvent event(banner, QAccessible::NameChanged);
	QAccessible::updateAccessibility(&event);
}

void TswDock::showCounters()
{
	counters->setEnabled(!view.counters_dimmed); /* dimmed while idle */
	const QString recorded = QString::fromUtf8(view.recorded) + " " + text("Counter.Recorded");
	const QString final = QString::fromUtf8(view.final_length) + " " + text("Counter.Final");
	counters->setText(compact ? recorded + "\n" + final : recorded + QString::fromUtf8("  →  ") + final);
	section->setVisible(view.section_shown);
	if (view.section_shown)
		section->setText(text("Counter.Section") + "  " + QString::fromUtf8(view.section_recorded) +
				 QString::fromUtf8("  →  ") + QString::fromUtf8(view.section_final));
}

void TswDock::showCard()
{
	card->setVisible(view.card_shown);
	if (!view.card_shown)
		return;
	const bool failed = view.error_code != 0;
	openFolder->setVisible(!failed && !path.isEmpty());
	play->setVisible(!failed && !path.isEmpty());
	copyDetails->setVisible(failed);
	learnMore->setVisible(failed);
	cardTitle->setVisible(failed);
	if (!failed) {
		QString body = text("Summary.Skipped", QString::fromUtf8(view.skipped));
		if (view.thank_user)
			body += " " + text("Summary.Thanks"); /* once per OBS session (UX-072) */
		cardBody->setText(body);
		return;
	}

	/* What happened, what is safe, what to do (UX-082). */
	const QByteArray prefix = "Error.E" + threeDigits(knownError(view.error_code)).toUtf8();
	cardTitle->setText(text(QByteArray(prefix + ".What").constData()));
	const QString saved = text(QByteArray(prefix + ".Saved").constData())
				      .replace("%1", QString::fromUtf8(view.recorded))
				      .replace("%2", QString::fromUtf8(view.final_length));
	cardBody->setText(saved + "\n" + text("Error.WhatToDo") + " " + text(QByteArray(prefix + ".Todo").constData()));
}

void TswDock::showButtons()
{
	const QString speed = QString::number(view.main_speed);
	const QString main = text(view.main_text, speed);
	const QByteArray shortKey = QByteArray(view.main_text) + ".Short";
	mainButton->setText(compact ? text(shortKey.constData(), speed) : main);
	mainButton->setToolTip(main); /* the full words stay reachable (UX-012, UX-103) */
	mainButton->setAccessibleName(main);
	mainButton->setEnabled(view.main_enabled);
	const QString stop = text("Action.Stop");
	stopButton->setText(compact ? text("Action.Stop.Short") : stop);
	stopButton->setToolTip(stop);
	stopButton->setAccessibleName(stop);
	stopButton->setVisible(view.stop_shown);
	stopButton->setEnabled(view.stop_enabled);
}

void TswDock::showSpeedRow()
{
	speedRow->setVisible(view.speeds_shown);
	speedRow->setEnabled(view.speeds_enabled);
	speedLabel->setText(text(view.speed_label));
	speedLabel->setVisible(!compact && view.speed_label != nullptr);
	if (QAbstractButton *chip = chips->button(static_cast<int>(view.speed_selected)))
		chip->setChecked(true);
	startRow->setVisible(view.start_mode_shown);
	startRealTime->setChecked(!view.start_in_timelapse);
	startTimelapse->setChecked(view.start_in_timelapse);
}

void TswDock::showContext()
{
	context->setVisible(view.context_text != nullptr);
	context->setText(text(view.context_text, QFileInfo(path).fileName()));
}

void TswDock::copyErrorDetails()
{
	const QString details =
		QString("Timelapse Switch TSW-E%1 | file=\"%2\" | details: the [timelapse-switch] lines "
			"of the OBS log (Help > Log Files)")
			.arg(threeDigits(view.error_code), path);
	QApplication::clipboard()->setText(details);
}

void TswDock::changeEvent(QEvent *event)
{
	QFrame::changeEvent(event);
	if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange) {
		styled = false; /* the OBS theme changed: draw the banner again with its colors (UX-016) */
		showBanner();
	}
}

void TswDock::resizeEvent(QResizeEvent *event)
{
	QFrame::resizeEvent(event);
	const bool narrow = width() < COMPACT_WIDTH;
	if (narrow != compact) {
		compact = narrow;
		refresh();
	}
}
