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
const int MAX_CONTROL = 200; /* px: taller and wider than any theme's button; bounds the scans (P10-2) */
const int MAX_CORNER = 16;   /* px: the largest button corner measured; bounds the scan (P10-2) */
const double QUARTER_CIRCLE = 0.78539816339744831; /* pi / 4: a quarter circle's share of its square */
const double MIN_TEXT_POINTS = 8.0;                /* no text smaller: 6 pt captions were unreadable even up close */
const double CORNER_OF_LINE = 0.22;                /* x L: a button's corner when the theme paints none (4 px) */
const double MIN_LIGHT_FACE = 1.1; /* a light theme's button face must differ this much from the panel */

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

/* What the theme's own button looks like, measured from a picture of it (3.4, 3.7). */
struct ButtonShape {
	int height = 0;      /* px, without the transparent margins some themes add (Yami: 2 px above and below) */
	double radius = 0.0; /* px: its corners */
	QColor face;         /* the color in its middle; invalid if it paints nothing there */
};

/* The first opaque pixel walking from (x, y) by (dx, dy), at most count steps; count if none. */
int firstOpaque(const QImage &image, int x, int y, int dx, int dy, int count)
{
	for (int step = 0; step < count; step++) {
		if (image.pixelColor(x + dx * step, y + dy * step).alpha() > 0)
			return step;
	}
	return count;
}

/*
 * The radius of a corner from how much of its square is see-through: outside a quarter circle of
 * radius r lies r * r * (1 - pi / 4) of the square. Antialiased edges count by their alpha, so the
 * measure is finer than one pixel: 2 px in Classic, 4 px in the other Yami themes.
 */
double cornerRadius(const QImage &image, int left, int top, int side)
{
	double clear = 0.0;
	for (int y = 0; y < side; y++) {
		for (int x = 0; x < side; x++)
			clear += 1.0 - image.pixelColor(left + x, top + y).alphaF();
	}
	return std::sqrt(clear / (1.0 - QUARTER_CIRCLE));
}

/*
 * The height, corners and face a theme's button really shows. Density changes its height, so the
 * dock's controls follow OBS's Density setting (3.7); its corners and face let the dock's own
 * buttons sit next to OBS's without looking foreign.
 */
ButtonShape measureButton(QPushButton *button, double fallbackRadius)
{
	ButtonShape shape;
	shape.radius = fallbackRadius;
	const QSize size = button->sizeHint();
	shape.height = size.height();
	if (size.isEmpty() || size.height() > MAX_CONTROL || size.width() > MAX_CONTROL)
		return shape;
	/* At the size the theme asks for, and without the window color behind it: what is left
	 * see-through is the theme's margin and corners. */
	button->resize(size);
	QImage image(size, QImage::Format_ARGB32_Premultiplied);
	image.fill(Qt::transparent);
	button->render(&image, QPoint(), QRegion(), QWidget::RenderFlags());
	const int top = firstOpaque(image, size.width() / 2, 0, 0, 1, size.height());
	const int bottom =
		size.height() - 1 - firstOpaque(image, size.width() / 2, size.height() - 1, 0, -1, size.height());
	const int left = firstOpaque(image, 0, size.height() / 2, 1, 0, size.width());
	if (bottom < top || left >= size.width() / 2)
		return shape; /* nothing painted: keep the fallbacks */
	shape.height = bottom - top + 1;
	const QColor middle = image.pixelColor(size.width() / 2, size.height() / 2);
	if (middle.alpha() == 255)
		shape.face = middle;
	const int side = std::min({MAX_CORNER, shape.height / 2, size.width() / 2 - left});
	shape.radius =
		std::round(cornerRadius(image, left, top, side)); /* a 1 px border's antialiasing adds a little */
	return shape;
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
	return std::max(MIN_TEXT_POINTS, std::round(b * factor * 2.0) / 2.0);
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
	/* A light theme's buttons are lighter than its panel (OBS Light: white on light grey): the dock's
	 * buttons take that face, so they match the Controls dock. Dark themes keep a mixed face, because
	 * some paint their buttons in a loud color (Rachni: cyan). */
	const bool lightFace = !isDark && buttonFace.isValid() &&
			       tsw_color_luminance(toCore(buttonFace)) > tsw_color_luminance(toCore(panel)) &&
			       tswContrast(buttonFace, panel) >= MIN_LIGHT_FACE;
	raised = lightFace ? buttonFace : tswMix(text, panel, 0.12);
	frame = shape(edge, panel);
	corner = std::max(2, static_cast<int>(std::lround(2.0 * radius)));

	/* 3. Accent: the theme's highlight, moved in lightness until it is a visible state. */
	const QColor highlight = hooks.accent.isValid() ? hooks.accent : hl;
	accent = shape(highlight, panel);
	const QColor fill = tswContrast(highlight, panel) >= SHAPE_CONTRAST ? highlight : accent;
	onAccent = bestOn(fill, {hlText, text, panel});
	accentFill = ensure(fill, onAccent, TEXT_CONTRAST, fill);
	accentText = ink(highlight, panel);
	accentTint = tswMix(accent, panel, 0.20);
	/* The badge on the timelapse band: the lightest of the theme's own colors that stands out from
	 * the band (3:1), its number in the band's color. A light pill in every theme (a dark one on a
	 * bright band reads as a hole); onAccent always qualifies, so there is always one. */
	badgeFill = onAccent;
	for (const QColor &candidate : {text, panel, raised}) {
		if (tswContrast(candidate, accentFill) >= SHAPE_CONTRAST &&
		    tsw_color_luminance(toCore(candidate)) > tsw_color_luminance(toCore(badgeFill)))
			badgeFill = candidate;
	}
	badgeText = ensure(accentFill, badgeFill, TEXT_CONTRAST, onAccent);

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
	const ButtonShape shape = measureButton(button, std::round(CORNER_OF_LINE * theme.L));
	theme.hC = shape.height;
	theme.radius = shape.radius;
	theme.buttonFace = shape.face;
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
