/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * dock.hpp: the dock (docs/design/ux-spec.md, section 3), drawn in the "Studio cards" direction
 * (docs/design/visual-design.md, 4.2). It only draws a view (core/dock-view.h) and reports clicks;
 * it knows nothing about OBS, so the UI test can draw it without OBS (tests/ui). dock-obs.cpp
 * connects it to the controller.
 *
 *   STATUS   .-----------------------------------.  status card: zones A and B
 *            | ( (o) REC )                       |
 *            | >> TIMELAPSE             ( x4 )   |
 *            | RECORDED 0:31:40  FINAL 0:16:58   |
 *            '-----------------------------------'
 *   ACTION   (  >  Back to real time            )  C main button
 *   BELOW    SPEED NOW                             D rails (or the summary or error card)
 *            (( x2 |#x4#| x8 | x16 ))
 *            (  || Pause  ) (  []  Stop and save )  E Pause (Resume while paused) and Stop
 *   CONTEXT  (i) Audio: silence during timelapse   F context line, or the health line when needed
 *            > DETAILS                             the Details, on request (dock-monitor.hpp)
 *
 * The status card's top corner holds a small menu button: Help and About (dock-about.hpp).
 * Everything it paints is mixed from the active OBS theme (dock-theme.hpp), read again whenever
 * the theme, the font size or the density changes.
 */

#pragma once

#include <QColor>
#include <QFrame>
#include <QString>

#include "core/dock-view.h"
#include "ui/dock-about.hpp"
#include "ui/dock-monitor.hpp"
#include "ui/dock-theme.hpp"

class QBoxLayout;
class QLabel;
class QMenu;
class QScrollArea;
class QSpacerItem;
class QVBoxLayout;
class TswButton;
class TswCard;
class TswGlyphIcon;
class TswLink;
class TswRail;
class TswReservedLabel;
class TswStatusCard;

class TswDock : public QFrame {
	Q_OBJECT
	/* Theme author hooks (UX-015): a theme may set them in its style sheet; they still pass the
	 * contrast guard. Invalid by default: the colors then come from the theme. */
	Q_PROPERTY(QColor accentColor READ accentColor WRITE setAccentColor)
	Q_PROPERTY(QColor dangerColor READ dangerColor WRITE setDangerColor)
	Q_PROPERTY(QColor successColor READ successColor WRITE setSuccessColor)
	Q_PROPERTY(QColor warningColor READ warningColor WRITE setWarningColor)
	Q_PROPERTY(QColor surfaceColor READ surfaceColor WRITE setSurfaceColor)

public:
	explicit TswDock(TswText translate, QWidget *parent = nullptr);

	/* Shows a view; cheap to call several times per second with the same view. */
	void showView(const tsw_dock_view &view);

	/* The dock as OBS shows it: in a scroll area, which gives wrapped texts the height they need (and
	 * scrolls, instead of cutting them, when the dock is short). Takes ownership of dock. */
	static QScrollArea *inScrollArea(TswDock *dock);

	/* What the About dialog shows that only the plugin knows: its version and its symbol (UX-138). */
	void setAboutInfo(const TswAboutInfo &info);
	/* The About dialog, not modal: from the menu, and for the UI test. */
	void showAbout();

	/* The Details section: open or closed as the user left it (UX-135); never opened by the dock. */
	void setDetailsOpen(bool open);
	bool detailsOpen() const;

	/* Reduce motion (UX-045): the one animation, the thumb's slide, becomes instant, and one under
	 * way ends at once (3.12). Off until the Feedback settings exist. */
	void setReduceMotion(bool reduce);

	/* The colors and sizes in use, for the UI test's contrast checks. */
	const TswTheme &theme() const;
	bool isNarrow() const;

	QColor accentColor() const;
	void setAccentColor(const QColor &color);
	QColor dangerColor() const;
	void setDangerColor(const QColor &color);
	QColor successColor() const;
	void setSuccessColor(const QColor &color);
	QColor warningColor() const;
	void setWarningColor(const QColor &color);
	QColor surfaceColor() const;
	void setSurfaceColor(const QColor &color);

public slots:
	/* Reads the theme again, once, after the current burst of events (3.4, "When to restyle"). */
	void scheduleRestyle();

signals:
	void inputRequested(int input, unsigned speed); /* an enum tsw_input, and the speed of a segment */
	void dismissRequested();                        /* the card's dismiss button, or Esc */
	void detailsOpenChanged(bool open);             /* the user opened or closed the Details */

protected:
	bool event(QEvent *event) override;
	void changeEvent(QEvent *event) override;
	bool eventFilter(QObject *watched, QEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;

private:
	QString text(const char *key) const;
	QString text(const char *key, const QString &argument) const;
	QString speedText(uint32_t speed) const;
	QString spoken(const QString &words) const;
	QString spokenDuration(const QString &clock) const;
	void buildZones();
	void buildMenu();
	void buildRails();
	void buildResumeRail();
	void buildCard();
	void buildCardMap();
	void buildCardActions();
	void buildContext();
	void connectInputs();
	void watchKeyFocus();
	void restyle();
	void applyMetrics();
	void applyStyleSheets();
	void applyReserves();
	void updateNarrow(int viewportWidth);
	void updateGaps();
	void refresh(bool animate);
	void showStatus();
	void showMain();
	void showRails(bool animate);
	void showStop();
	void showPause();
	void showCard();
	void showSummaryCard();
	void showErrorCard();
	void updateCardRows();
	void updateStopRow();
	void showContext();
	void showHealth();
	void announce();
	void copyErrorDetails();

	TswText lookup;
	tsw_dock_view view{};
	QString path; /* the file of the view, copied: the view's pointer is only borrowed */
	TswTheme style;
	TswThemeHooks hooks;
	TswProbes probes;
	bool narrow = false;
	bool styled = false;    /* the theme was read at least once */
	bool restyling = false; /* events caused by restyle() itself are ignored */
	bool restylePending = false;
	bool viewShown = false; /* a first view was shown: later changes may animate */
	bool reduceMotion = false;

	QVBoxLayout *zones = nullptr;
	QSpacerItem *gapMain = nullptr; /* the gaps between the zones, set by state and width */
	QSpacerItem *gapCard = nullptr;
	QSpacerItem *gapSpeeds = nullptr;
	QSpacerItem *gapStop = nullptr;
	QSpacerItem *gapContext = nullptr;
	QSpacerItem *gapDetails = nullptr;
	TswStatusCard *status = nullptr;     /* zones A and B */
	TswMenuButton *menuButton = nullptr; /* in the status card's top corner: Help and About (UX-138) */
	QMenu *menu = nullptr;
	TswAbout *about = nullptr; /* made when first asked for */
	TswAboutInfo aboutInfo;
	TswButton *mainButton = nullptr; /* zone C */
	QWidget *speeds = nullptr;       /* zone D */
	QVBoxLayout *speedsLayout = nullptr;
	QWidget *startGroup = nullptr;
	TswReservedLabel *startCaption = nullptr;
	TswRail *startRail = nullptr;
	QWidget *speedGroup = nullptr;
	TswReservedLabel *speedCaption = nullptr;
	TswRail *speedRail = nullptr;
	TswRail *resumeRail = nullptr;    /* while paused, in the speed rail's place: Real time and the speeds */
	QBoxLayout *stopRow = nullptr;    /* zone E: Pause and Stop side by side (updateStopRow) */
	TswButton *pauseButton = nullptr; /* Pause, or Resume while paused, in the same place (UX-128) */
	TswButton *stopButton = nullptr;
	QWidget *contextRow = nullptr; /* zone F */
	TswGlyphIcon *contextIcon = nullptr;
	QLabel *context = nullptr;
	TswCard *card = nullptr; /* the summary or error card, below C */
	QBoxLayout *cardLayout = nullptr;
	QLabel *cardHeader = nullptr; /* SESSION, or ERROR TSW-E103: the ticket's small caption */
	TswButton *dismiss = nullptr;
	QWidget *figureBox = nullptr; /* the summary's big figure: the waiting skipped */
	QLabel *figure = nullptr;
	QLabel *figureCaption = nullptr;
	QWidget *cardMapBox = nullptr; /* the summary's speed map (3.13), with its caption */
	QLabel *cardMapCaption = nullptr;
	TswSpeedMap *cardMap = nullptr;
	QLabel *cardTitle = nullptr; /* the file name, or the error's reassurance */
	QLabel *cardBody = nullptr;
	QLabel *cardTodo = nullptr;
	QBoxLayout *cardActions = nullptr; /* side by side, or one under the other when narrow */
	TswButton *openFolder = nullptr;
	TswButton *play = nullptr;
	QWidget *linkRow = nullptr;
	TswLink *copyDetails = nullptr;
	TswLink *learnMore = nullptr;
	TswDetails *details = nullptr; /* below zone F, on request */
};
