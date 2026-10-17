/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * dock-about.hpp: who made the plugin, where people look for it and never in the way of a recording
 * (docs/design/ux-spec.md, 3.18, UX-138).
 *
 *   dock menu (top corner) --> Help: opens the documentation site
 *                          --> About: this dialog
 *        .--------------------------------------.
 *        |            [ the symbol ]            |
 *        |           Timelapse Switch           |
 *        |             Version 0.1.0            |
 *        |            by Miguetronic            |
 *        |          ->  Documentation           |  links open only on a click
 *        |          ->  miguetronic.com         |
 *        |          ->  GitHub                  |
 *        |          ->  License: GPL-2.0-...    |
 *        |  Not affiliated with or endorsed by  |
 *        |  the OBS Project.                    |
 *        |               ( Close )              |
 *        '--------------------------------------'
 *
 * The dialog is not modal: it never blocks OBS, even mid-recording (UX-013). Like the dock it knows
 * nothing about OBS; dock-obs.cpp gives it the version and where the symbol's pictures are.
 */

#pragma once

#include <QDialog>
#include <QPixmap>
#include <QString>
#include <QVector>

#include "ui/dock-monitor.hpp"
#include "ui/dock-theme.hpp"

class QLabel;
class QSpacerItem;
class QVBoxLayout;
class TswButton;
class TswLink;

/* The documentation site: the menu's Help and the dialog's first link. */
inline constexpr char TSW_DOCS_URL[] = "https://miguetronic.github.io/timelapse-switch/";

/* What only the plugin knows: its version and the symbol's pictures (installed with its data). */
struct TswAboutInfo {
	QString version;    /* "0.1.0" */
	QString symbol;     /* the symbol for light backgrounds, a PNG file; empty: none */
	QString symbolDark; /* the same symbol for dark backgrounds */
};

/* The plugin's symbol, drawn from its picture at the screen's pixel ratio; the dark variant on a dark
 * background. Without a picture it takes no room. */
class TswSymbol : public QWidget {
	Q_OBJECT

public:
	TswSymbol(QWidget *parent, const TswTheme *theme);
	void setFiles(const QString &light, const QString &dark);
	void setSymbolHeight(int height);
	bool hasPicture() const;
	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	const TswTheme *theme;
	QPixmap light;
	QPixmap dark;
	int symbolHeight = 0;
};

/* The About dialog: non-modal, painted on the dock's own panel color so every token keeps its contrast. */
class TswAbout : public QDialog {
	Q_OBJECT

public:
	TswAbout(QWidget *parent, const TswTheme *theme, TswText lookup, const TswAboutInfo &info);
	void restyle(); /* the dock read the theme again: fonts, colors, size */

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	QLabel *addLabel(const QString &text);
	void addGap();
	void addLinks();

	const TswTheme *theme;
	TswText lookup;
	QVBoxLayout *column = nullptr;
	QVector<QSpacerItem *> gaps; /* between the groups: symbol, words, links, notice, Close */
	TswSymbol *symbol = nullptr;
	QLabel *name = nullptr;
	QLabel *version = nullptr;
	QLabel *author = nullptr;
	QVector<TswLink *> links;
	QLabel *notice = nullptr;
	TswButton *close = nullptr;
};
