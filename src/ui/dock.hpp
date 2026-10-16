/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * dock.hpp: the dock's widgets (docs/design/ux-spec.md, section 3). It only draws a view
 * (core/dock-view.h) and reports clicks; it knows nothing about OBS, so the UI test can
 * draw it without OBS (tests/ui). dock-obs.cpp connects it to the controller.
 *
 *   +-- A banner: state word, icon, frame style ---------+
 *   |   B counters, section, summary or error card       |
 *   |   C main button (the most likely next action)      |
 *   |   D speed chips; while idle, the starting mode     |
 *   |   E Stop and save (smaller, set apart)             |
 *   |   F context line                                   |
 *   +----------------------------------------------------+
 */

#pragma once

#include <QFrame>
#include <QString>

#include "core/dock-view.h"

class QBoxLayout;
class QButtonGroup;
class QLabel;
class QPushButton;
class QRadioButton;
class QScrollArea;

/* Translates a locale key: obs_module_text in OBS, a reader of the .ini files in the UI test. */
using TswText = const char *(*)(const char *key);

class TswDock : public QFrame {
	Q_OBJECT

public:
	explicit TswDock(TswText translate, QWidget *parent = nullptr);

	/* Shows a view; cheap to call several times per second with the same view. */
	void showView(const tsw_dock_view &view);

	/* The dock as OBS shows it: in a scroll area, which gives wrapped texts the height they need (and
	 * scrolls, instead of cutting them, when the dock is short). Takes ownership of dock. */
	static QScrollArea *inScrollArea(TswDock *dock);

signals:
	void inputRequested(int input, unsigned speed); /* an enum tsw_input, and the speed of a chip */
	void dismissRequested();                        /* the card's Close button */

protected:
	void changeEvent(QEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;

private:
	QString text(const char *key) const;
	QString text(const char *key, const QString &argument) const;
	void buildBanner();
	void buildCounters();
	void buildCard();
	void buildButtons();
	void buildSpeedRow();
	void applyBannerStyle();
	void refresh();
	void showBanner();
	void showCounters();
	void showCard();
	void showButtons();
	void showSpeedRow();
	void showContext();
	void copyErrorDetails();

	TswText lookup;
	tsw_dock_view view{};
	QString path; /* the file of the view, copied: the view's pointer is only borrowed */
	bool compact = false;
	bool styled = false; /* the banner style matches view.banner */
	enum tsw_banner styledBanner = TSW_BANNER_READY;

	QFrame *banner = nullptr; /* zone A */
	QLabel *bannerIcon = nullptr;
	QLabel *bannerText = nullptr;
	QLabel *bannerDetail = nullptr;
	QLabel *counters = nullptr; /* zone B */
	QLabel *section = nullptr;
	QFrame *card = nullptr;
	QLabel *cardTitle = nullptr;
	QLabel *cardBody = nullptr;
	QPushButton *openFolder = nullptr;
	QPushButton *play = nullptr;
	QPushButton *copyDetails = nullptr;
	QPushButton *learnMore = nullptr;
	QPushButton *closeCard = nullptr;
	QBoxLayout *cardActions = nullptr; /* side by side, or one under the other when narrow */
	QPushButton *mainButton = nullptr; /* zone C */
	QWidget *speedRow = nullptr;       /* zone D */
	QLabel *speedLabel = nullptr;
	QButtonGroup *chips = nullptr;
	QWidget *startRow = nullptr;
	QBoxLayout *startLayout = nullptr; /* side by side, or one under the other when narrow */
	QRadioButton *startRealTime = nullptr;
	QRadioButton *startTimelapse = nullptr;
	QPushButton *stopButton = nullptr; /* zone E */
	QLabel *context = nullptr;         /* zone F */
};
