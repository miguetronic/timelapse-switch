/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * dock-about.cpp: see dock-about.hpp. Sizes follow the dock's type scale (docs/design/visual-design.md,
 * 3.6 and 4.2); colors come from the theme through the contrast guard, like the dock's.
 */

#include "ui/dock-about.hpp"

#include <QDesktopServices>
#include <QLabel>
#include <QPainter>
#include <QSpacerItem>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

#include "ui/dock-paint.hpp"
#include "ui/dock-widgets.hpp"

namespace {

const double SYMBOL_ROWS = 3.2;             /* x L: the symbol's height (about 58 px at OBS's defaults) */
const double WIDTH_ROWS = 18.0;             /* x L: the dialog's width (324 px at OBS's defaults) */
const double NAME = 1.5;                    /* x b: the plugin's name, the dialog's one large text */
const double NOTICE = 0.85;                 /* x b: the non-affiliation line, small and muted */
const double CLOSE_ROWS = 1.6;              /* x L: the Close pill, as the card's pills */
const double SYMBOL_ASPECT = 512.0 / 224.0; /* the symbol's pictures are 512 x 224 */

/* The links, in the order of ux-spec 3.18; each opens only when clicked. */
const struct {
	const char *key;
	const char *url;
} LINKS[] = {
	{"About.Documentation", TSW_DOCS_URL},
	{"About.Website", "https://miguetronic.com"},
	{"About.GitHub", "https://github.com/miguetronic/timelapse-switch"},
	{"About.License", "https://github.com/miguetronic/timelapse-switch/blob/main/LICENSE"},
};

} // namespace

/* --- TswSymbol --- */

TswSymbol::TswSymbol(QWidget *parent, const TswTheme *style) : QWidget(parent), theme(style)
{
	setObjectName("tswSymbol");
	setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void TswSymbol::setFiles(const QString &lightFile, const QString &darkFile)
{
	light = lightFile.isEmpty() ? QPixmap() : QPixmap(lightFile);
	dark = darkFile.isEmpty() ? QPixmap() : QPixmap(darkFile);
	setVisible(hasPicture());
	updateGeometry();
}

void TswSymbol::setSymbolHeight(int height)
{
	symbolHeight = height;
	updateGeometry();
	update();
}

bool TswSymbol::hasPicture() const
{
	return !light.isNull() || !dark.isNull();
}

QSize TswSymbol::sizeHint() const
{
	if (!hasPicture())
		return {0, 0};
	return {static_cast<int>(std::lround(symbolHeight * SYMBOL_ASPECT)), symbolHeight};
}

QSize TswSymbol::minimumSizeHint() const
{
	return sizeHint();
}

void TswSymbol::paintEvent(QPaintEvent *event)
{
	(void)event;
	const QPixmap &source = theme->isDark && !dark.isNull() ? dark : (light.isNull() ? dark : light);
	if (source.isNull())
		return;
	/* Scaled once per paint to the device pixels it takes, smoothly: crisp at any screen scale. */
	const qreal ratio = devicePixelRatioF();
	QPixmap scaled = source.scaled(size() * ratio, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	scaled.setDevicePixelRatio(ratio);
	QPainter painter(this);
	const QSizeF shown = QSizeF(scaled.size()) / ratio;
	painter.drawPixmap(QPointF((width() - shown.width()) / 2.0, (height() - shown.height()) / 2.0), scaled);
}

/* --- TswAbout --- */

TswAbout::TswAbout(QWidget *parent, const TswTheme *style, TswText translate, const TswAboutInfo &info)
	: QDialog(parent),
	  theme(style),
	  lookup(translate)
{
	setObjectName("tswAbout");
	setModal(false); /* never blocks OBS, even mid-recording (UX-013) */
	setWindowTitle(QString::fromUtf8(lookup("About.Title")));
	column = new QVBoxLayout(this);
	symbol = new TswSymbol(this, theme);
	symbol->setFiles(info.symbol, info.symbolDark);
	column->addWidget(symbol, 0, Qt::AlignHCenter);
	addGap();
	name = addLabel(QString::fromUtf8(lookup("Dock.Title")));
	version = addLabel(QString::fromUtf8(lookup("About.Version")).replace("%1", info.version));
	author = addLabel(QString::fromUtf8(lookup("About.By")));
	addGap();
	addLinks();
	addGap();
	notice = addLabel(QString::fromUtf8(lookup("About.NotAffiliated")));
	addGap();
	close = new TswButton(this, theme);
	close->setObjectName("tswAboutClose");
	close->setShape(TswButton::Shape::Pill);
	close->setContent(TswGlyph::None, QString::fromUtf8(lookup("Button.Close")));
	close->setAccessibleName(QString::fromUtf8(lookup("Button.Close")));
	column->addWidget(close, 0, Qt::AlignHCenter);
	connect(close, &QAbstractButton::clicked, this, &QDialog::close);
	restyle();
}

/* A gap between two groups, sized by restyle. */
void TswAbout::addGap()
{
	auto *gap = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Fixed);
	column->addSpacerItem(gap);
	gaps.append(gap);
}

QLabel *TswAbout::addLabel(const QString &text)
{
	auto *label = new QLabel(text, this);
	label->setTextFormat(Qt::PlainText);
	label->setWordWrap(true);
	label->setAlignment(Qt::AlignHCenter);
	column->addWidget(label);
	return label;
}

/* Documentation, miguetronic.com, GitHub and the license, one under the other, centered. */
void TswAbout::addLinks()
{
	for (const auto &link : LINKS) {
		auto *button = new TswLink(this, theme, TswGlyph::LinkOut);
		button->setLabel(QString::fromUtf8(lookup(link.key)));
		button->setOnPanel(true);
		button->setToolTip(QString::fromUtf8(link.url));
		const QUrl url(QString::fromUtf8(link.url));
		connect(button, &QAbstractButton::clicked, this, [url]() { QDesktopServices::openUrl(url); });
		column->addWidget(button, 0, Qt::AlignHCenter);
		links.append(button);
	}
}

void TswAbout::restyle()
{
	const TswTheme &t = *theme;
	const auto sheet = [](double points, bool bold, const QColor &color) {
		return QString("font-size: %1pt; font-weight: %2; color: %3;")
			.arg(points)
			.arg(bold ? 700 : 400)
			.arg(color.name());
	};
	name->setStyleSheet(sheet(t.pointsAt(NAME), true, t.ink(t.text, t.panel)));
	version->setStyleSheet(sheet(t.pointsAt(TswType::STATUS), false, t.muted));
	author->setStyleSheet(sheet(t.pointsAt(TswType::BODY), false, t.ink(t.text, t.panel)));
	notice->setStyleSheet(sheet(t.pointsAt(NOTICE), false, t.muted));
	for (TswLink *link : links)
		link->setLinkFont(t.fontAt(TswType::LINK, false));
	close->setLabelFont(t.fontAt(TswType::BODY, false));
	close->setMinimumRow(std::max(static_cast<int>(std::lround(CLOSE_ROWS * t.L)), t.hC));
	close->setPadding(t.s5);
	close->setLook({t.raised, t.panel, QColor(), 0.0, t.ink(t.text, t.raised), t.ink(t.text, t.raised)});
	close->setFixedWidth(close->sizeHint().width());
	symbol->setSymbolHeight(static_cast<int>(std::lround(SYMBOL_ROWS * t.L)));
	/* The rhythm (3.7): rows s1 apart, groups a group gap apart. */
	column->setContentsMargins(t.s5, t.s5, t.s5, t.s4);
	column->setSpacing(t.s1);
	for (QSpacerItem *gap : gaps)
		gap->changeSize(0, t.s4 - t.s1, QSizePolicy::Minimum, QSizePolicy::Fixed);
	column->invalidate();
	const int width = static_cast<int>(std::lround(WIDTH_ROWS * t.L));
	/* The notice in lines of similar length, never one word alone on the last line. */
	QFont noticeFont = t.font;
	noticeFont.setPointSizeF(t.pointsAt(NOTICE));
	notice->setFixedWidth(tswBalancedWidth(noticeFont, notice->text(), width - 2 * t.s5));
	column->setAlignment(notice, Qt::AlignHCenter);
	setFixedSize(width, column->heightForWidth(width));
	update();
}

/* The dock's panel color, so the dialog matches the dock and every token keeps its contrast (UX-016). */
void TswAbout::paintEvent(QPaintEvent *event)
{
	(void)event;
	QPainter painter(this);
	painter.fillRect(rect(), theme->panel);
}
