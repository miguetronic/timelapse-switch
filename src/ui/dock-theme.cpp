/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * dock-theme.cpp: see dock-theme.hpp. The formulas are the ones of docs/design/visual-design.md,
 * 3.5; each token says what it is for, and the contrast guard is core/color.c.
 */

#include "ui/dock-theme.hpp"

#include <QAbstractScrollArea>
#include <QFontInfo>
#include <QGuiApplication>
#include <QFontMetrics>
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QWidget>
#include <algorithm>
#include <cmath>

#include "core/color.h"

namespace {

const double SHAPE_CONTRAST = 3.0; /* WCAG 1.4.11: frames, fills, markers, glyphs */
const double TEXT_CONTRAST = 4.5;  /* WCAG 1.4.3: text */
const double DARK_LUMINANCE = 0.179;
const int SAMPLE = 4;        /* px: the background is drawn this big and its center is read */
const int MAX_CONTROL = 200; /* px: taller than any theme's button; bounds the scan below (P10-2) */

tsw_color toCore(const QColor &color)
{
	const QColor rgb = color.toRgb();
	return tsw_color_from_rgb8(static_cast<uint8_t>(rgb.red()), static_cast<uint8_t>(rgb.green()),
				   static_cast<uint8_t>(rgb.blue()));
}

QColor fromCore(const tsw_color &color)
{
	return QColor::fromRgbF(static_cast<float>(color.red), static_cast<float>(color.green),
				static_cast<float>(color.blue));
}

QColor ensure(const QColor &color, const QColor &under, double target, const QColor &fallback)
{
	return fromCore(tsw_color_ensure_contrast(toCore(color), toCore(under), target, toCore(fallback)));
}

/* A probe label with one of OBS's text classes (text-muted, text-danger, ...). */
QLabel *classProbe(QWidget *owner, const char *themeClass)
{
	auto *probe = new QLabel(owner);
	probe->setProperty("class", themeClass);
	probe->hide();
	return probe;
}

/* The color of a class probe, or fallback when the theme does not define the class. */
QColor classColor(QLabel *probe, const QColor &plain, const QColor &fallback)
{
	probe->ensurePolished();
	const QColor color = probe->palette().color(QPalette::WindowText);
	return color == plain ? fallback : color;
}

/* One pixel of what a widget paints behind its children, or an invalid color if it paints nothing.
 * render() draws the source region at the target's origin, so a small image is enough. */
QColor backgroundOf(QWidget *widget)
{
	QImage image(SAMPLE, SAMPLE, QImage::Format_ARGB32_Premultiplied);
	image.fill(Qt::transparent);
	const QPoint center(widget->width() / 2, widget->height() / 2);
	const QRect sample(center - QPoint(SAMPLE / 2, SAMPLE / 2), QSize(SAMPLE, SAMPLE));
	widget->render(&image, QPoint(), QRegion(sample), QWidget::DrawWindowBackground);
	const QColor pixel = image.pixelColor(SAMPLE / 2, SAMPLE / 2);
	return pixel.alpha() == 255 ? pixel : QColor();
}

/*
 * The dock's real background: what the scroll area around it paints (OBS styles it as the dock's
 * body), or its viewport, or the application's window color. OBS paints docks with its own
 * background color, which is often not the palette's (UX-101).
 */
QColor measurePanel(QWidget *viewport, const QColor &fallback)
{
	QWidget *area = viewport != nullptr ? qobject_cast<QAbstractScrollArea *>(viewport->parentWidget()) : nullptr;
	for (QWidget *candidate : {area, viewport}) {
		if (candidate == nullptr || candidate->width() <= SAMPLE || candidate->height() <= SAMPLE)
			continue;
		const QColor color = backgroundOf(candidate);
		if (color.isValid())
			return color;
	}
	return fallback;
}

/*
 * The height a theme's button really shows: its size hint without the transparent margins some
 * themes add around it (Yami: 2 px above and below). Density changes it, so the dock's controls
 * follow OBS's Density setting (3.7).
 */
int controlHeight(QPushButton *button)
{
	const QSize size = button->sizeHint();
	if (size.isEmpty() || size.height() > MAX_CONTROL)
		return size.height();
	QImage image(size, QImage::Format_ARGB32_Premultiplied);
	image.fill(Qt::transparent);
	button->render(&image, QPoint(), QRegion(), QWidget::DrawWindowBackground);
	const int middle = size.width() / 2;
	int top = 0;
	while (top < size.height() && image.pixelColor(middle, top).alpha() == 0)
		top++;
	int bottom = size.height() - 1;
	while (bottom > top && image.pixelColor(middle, bottom).alpha() == 0)
		bottom--;
	return bottom >= top ? bottom - top + 1 : size.height();
}

/* Of the candidates, the one that reads best on fill. */
QColor bestOn(const QColor &fill, std::initializer_list<QColor> candidates)
{
	QColor best;
	double bestContrast = 0.0;
	for (const QColor &candidate : candidates) {
		const double contrast = tswContrast(candidate, fill);
		if (contrast > bestContrast) {
			best = candidate;
			bestContrast = contrast;
		}
	}
	return best;
}

} // namespace

QColor tswMix(const QColor &fg, const QColor &bg, double amount)
{
	return fromCore(tsw_color_mix(toCore(fg), toCore(bg), amount));
}

double tswContrast(const QColor &a, const QColor &b)
{
	return tsw_color_contrast(toCore(a), toCore(b));
}

QColor TswTheme::shape(const QColor &color, const QColor &under) const
{
	return ensure(color, under, SHAPE_CONTRAST, text);
}

QColor TswTheme::ink(const QColor &color, const QColor &under) const
{
	return ensure(color, under, TEXT_CONTRAST, text);
}

QColor TswTheme::hover(const QColor &faceColor) const
{
	return tswMix(text, faceColor, 0.06);
}

QColor TswTheme::pressed(const QColor &faceColor) const
{
	return tswMix(faceColor, panel, 0.5);
}

double TswTheme::pointsAt(double factor) const
{
	return std::round(b * factor * 2.0) / 2.0;
}

QFont TswTheme::fontAt(double factor, bool bold) const
{
	QFont scaled = font;
	scaled.setPointSizeF(pointsAt(factor));
	scaled.setWeight(bold ? QFont::Bold : QFont::Normal); /* OBS ships Open Sans 400 and 700 only */
	return scaled;
}

void TswTheme::derive(const TswThemeHooks &hooks)
{
	/* 1. Spacing: one unit from the line height (4 px at OBS's defaults), five steps. */
	u = std::max(3, static_cast<int>(std::lround(L / 4.5)));
	s1 = std::max(2, u / 2);
	s2 = u;
	s3 = 2 * u;
	s4 = 3 * u;
	s5 = 4 * u;

	/* 2. Neutrals: text laid over the panel at a few strengths. */
	isDark = tsw_color_luminance(toCore(panel)) < DARK_LUMINANCE;
	muted = ink(mutedIn, panel);
	hairline = tswMix(text, panel, 0.16);
	edge = tswMix(text, panel, 0.30);
	surface = hooks.surface.isValid() ? hooks.surface : tswMix(text, panel, 0.06);
	raised = tswMix(text, panel, 0.12);

	/* 3. Accent: the theme's highlight, moved in lightness until it is a visible state. */
	const QColor highlight = hooks.accent.isValid() ? hooks.accent : hl;
	accent = shape(highlight, panel);
	const QColor fill = tswContrast(highlight, panel) >= SHAPE_CONTRAST ? highlight : accent;
	onAccent = bestOn(fill, {hlText, text, panel});
	accentFill = ensure(fill, onAccent, TEXT_CONTRAST, fill);
	accentText = ink(highlight, panel);
	accentTint = tswMix(accent, panel, 0.20);

	/* 4. Status colors: for glyphs, frames and bars only, never for text (UX-111). */
	danger = shape(hooks.danger.isValid() ? hooks.danger : dangerIn, panel);
	success = shape(hooks.success.isValid() ? hooks.success : successIn, panel);
	warning = shape(hooks.warning.isValid() ? hooks.warning : warningIn, panel);
	dangerTint = tswMix(danger, panel, 0.18);

	/* 5. Focus: the accent, visible against the panel and against the theme's buttons. */
	focus = shape(accent, panel);
	if (tswContrast(focus, face) < SHAPE_CONTRAST)
		focus = shape(focus, face);
	if (tswContrast(focus, panel) < SHAPE_CONTRAST || tswContrast(focus, face) < SHAPE_CONTRAST)
		focus = text;
}

TswProbes::TswProbes(QWidget *owner)
	: label(new QLabel(owner)),
	  muted(classProbe(owner, "text-muted")),
	  danger(classProbe(owner, "text-danger")),
	  warning(classProbe(owner, "text-warning")),
	  success(classProbe(owner, "text-success")),
	  button(new QPushButton(owner))
{
	label->hide();
	button->hide();
	for (QWidget *probe : std::initializer_list<QWidget *>{label, muted, danger, warning, success, button}) {
		probe->setAttribute(Qt::WA_DontShowOnScreen);
		probe->setAccessibleName(QString()); /* never announced: it is never shown */
	}
}

TswTheme TswProbes::read(QWidget *background, const TswThemeHooks &hooks) const
{
	TswTheme theme;
	label->ensurePolished();
	button->ensurePolished();
	const QPalette palette = label->palette();
	theme.font = label->font();
	theme.b = QFontInfo(theme.font).pointSizeF();
	theme.L = QFontMetrics(theme.font).lineSpacing();
	theme.hC = controlHeight(button);
	theme.text = palette.color(QPalette::WindowText);
	theme.face = button->palette().color(QPalette::Button);
	theme.hl = palette.color(QPalette::Highlight);
	theme.hlText = palette.color(QPalette::HighlightedText);
	theme.panel = measurePanel(background, QGuiApplication::palette().color(QPalette::Window));
	theme.mutedIn = classColor(muted, theme.text, palette.color(QPalette::PlaceholderText));
	theme.isYami = theme.mutedIn != palette.color(QPalette::PlaceholderText);
	/* Without the classes a monochrome dot is still a dot: the text color (3.4). */
	theme.dangerIn = classColor(danger, theme.text, theme.text);
	theme.warningIn = classColor(warning, theme.text, theme.text);
	theme.successIn = classColor(success, theme.text, theme.text);
	theme.derive(hooks);
	return theme;
}
