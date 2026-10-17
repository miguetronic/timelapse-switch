/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * dock-theme.hpp: how the dock reads the active OBS theme and mixes its own colors and sizes
 * from it (docs/design/visual-design.md, 3.4 to 3.7).
 *
 * Think of a tailor who measures the customer before cutting: the dock measures the theme after
 * OBS has dressed it, through a few hidden widgets it never styles (the probes), and only then
 * cuts its sizes and colors. It never measures itself.
 *
 *   probes (font, palette, OBS classes) --> inputs --> tokens (contrast guard, core/color.h)
 *                                                  --> metrics (b, L, u, control height)
 */

#pragma once

#include <QColor>
#include <QFont>

class QLabel;
class QPushButton;
class QWidget;

/* Colors a theme author may set on the dock (UX-015); invalid ones come from the theme. */
struct TswThemeHooks {
	QColor accent;
	QColor danger;
	QColor success;
	QColor warning;
	QColor surface;
};

struct TswTheme {
	/* Inputs (3.4). */
	double b = 10.0;     /* OBS base font size, points */
	int L = 18;          /* its line height, pixels */
	int hC = 28;         /* the theme's control height: Density shows here */
	double radius = 4.0; /* the theme's button corners, px: 2 in Classic, 4 in the other Yami themes */
	QFont font;          /* the probe's font: every painted text starts from it */
	QColor panel;        /* what OBS really paints behind the dock */
	QColor text;         /* body and title text */
	QColor face;         /* the theme's button face (for the focus ring) */
	QColor buttonFace;   /* what the theme really paints on a button: OBS Light's is white */
	QColor hl;           /* the theme's highlight */
	QColor hlText;       /* text on it */
	QColor mutedIn;      /* OBS class text-muted, or the placeholder color */
	QColor dangerIn;     /* text-danger, or text */
	QColor warningIn;    /* text-warning, or text */
	QColor successIn;    /* text-success, or text */
	bool isYami = false; /* the OBS utility classes exist */
	bool isDark = true;  /* panel luminance below 0.179 */

	/* Spacing (3.7): one unit from the font, five steps. */
	int u = 4;
	int s1 = 2;
	int s2 = 4;
	int s3 = 8;
	int s4 = 12;
	int s5 = 16;

	/* Corners (3.7): a container's corner is its content's plus the space between, so cards and
	 * the main button get twice the theme's button radius (8 px, or 4 px in Classic). */
	int corner = 8;

	/* Tokens (3.5). */
	QColor muted;       /* captions, hints, idle digits */
	QColor hairline;    /* separators (decorative) */
	QColor edge;        /* outlines of quiet buttons */
	QColor surface;     /* cards and tracks */
	QColor raised;      /* painted button faces: the theme's own in light themes */
	QColor accent;      /* armed outlines, timelapse frames */
	QColor accentFill;  /* filled timelapse areas, the engaged speed */
	QColor onAccent;    /* text and glyphs on accentFill */
	QColor badgeFill;   /* the speed badge on the solid timelapse band: the band's text as a fill */
	QColor badgeText;   /* its number: the band's color, readable on the badge */
	QColor accentText;  /* accent words on the panel: links */
	QColor danger;      /* record dot, error frame */
	QColor success;     /* check glyph */
	QColor warning;     /* warning glyph */
	QColor focus;       /* keyboard focus ring: 3:1 against the panel and the theme's buttons */
	QColor accentTint;  /* the timelapse status card */
	QColor dangerTint;  /* REC pill, error card, Stop hold */
	QColor warningTint; /* PAUSED pill */
	QColor onWarning;   /* the pause badge's bars on a warning fill */
	QColor frame;       /* the 2 px outline of the real-time status card: a shape, 3:1 */

	/* shape: a glyph, line, frame or marker on under (3:1); ink: a text on under (4.5:1). */
	QColor shape(const QColor &color, const QColor &under) const;
	QColor ink(const QColor &color, const QColor &under) const;
	/* hover: a face under the mouse; pressed: a face being pressed. */
	QColor hover(const QColor &face) const;
	QColor pressed(const QColor &face) const;
	/* The probe's font at factor times b, rounded to 0.5 pt so a restyle does not churn, and never
	 * below 8 pt: at OBS's Font Size 8 a 0.75b caption would be 6 pt, unreadable even up close. */
	QFont fontAt(double factor, bool bold) const;
	/* The same size as a point value, for widget style sheets (same floor). */
	double pointsAt(double factor) const;

	/* Fills the spacing and the tokens from the inputs. */
	void derive(const TswThemeHooks &hooks);
};

/* mix: fg over bg at amount (0 to 1); contrast: the WCAG 2 ratio. Thin wrappers of core/color.h. */
QColor tswMix(const QColor &fg, const QColor &bg, double amount);
double tswContrast(const QColor &a, const QColor &b);

/*
 * TswProbes: the hidden, never-styled widgets the theme is read from. Created once as children of
 * the dock (Qt owns them); read() polishes them first, so it sees the theme as OBS applied it.
 */
class TswProbes {
public:
	explicit TswProbes(QWidget *owner);

	/* The inputs, read from the probes and from what OBS paints behind the dock (background). */
	TswTheme read(QWidget *background, const TswThemeHooks &hooks) const;

private:
	QLabel *label;
	QLabel *muted;
	QLabel *danger;
	QLabel *warning;
	QLabel *success;
	QPushButton *button;
};
