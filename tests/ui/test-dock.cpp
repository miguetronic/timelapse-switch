/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * test-dock.cpp: the dock drawn without OBS (Qt's offscreen platform), in every state, in English
 * and Spanish, at a normal width, at the narrow breakpoint and at the narrowest one it must
 * support (docs/design/visual-design.md, 3.13). It checks:
 *   - every text the dock asks for exists in the locale file, so no raw key ever shows (UX-120);
 *   - the zones keep their order from top to bottom (UX-002), and the main button keeps its y in
 *     every state, Pause and Stop theirs in every recording state, paused included (fixed
 *     positions), side by side in one row; zone D keeps no empty room, and the paused "Resume in"
 *     rail is exactly as tall as the recording speed rail;
 *   - nothing is wider than what is visible, even at 220 px (UX-012), and the dock can shrink to
 *     200 px;
 *   - the main button is at least 2.5 lines tall (UX-107), and its words are a verb, never a state;
 *   - no text is cut: every label and painted block is as tall as its lines need (UX-121), no word
 *     is wider than its button or rail segment, and the painted hero never falls below its floor;
 *   - the colors pass the contrast guard, and no label is colored with a status color (UX-111);
 *   - no text is smaller than 8 pt, whatever OBS's Font Size;
 *   - Tab visits the controls in visual order (UX-102);
 *   - Pause, Stop and the card appear only when they should, and the card's two pills sit side by
 *     side only when neither label wraps;
 *   - Pause and Resume pressed quickly, many times, always show the state the core is in, and
 *     nothing moves (UX-133);
 *   - Reduce motion stops the one animation, the thumb's slide (UX-045).
 * It also saves a picture of each case, for a person to look at.
 *
 *   test-dock <folder with en-US.ini and es-ES.ini> <folder for the pictures> [<OBS theme dumps>]
 *
 * With a third folder (written by tests/ui/obs-themes.sh, one subfolder per OBS theme), it dresses
 * the dock in each theme exactly as OBS would (obs-theme.hpp), puts it in an OBS-like dock frame,
 * runs the same checks, compares fonts and background with OBS's own dock, and saves the pictures
 * in <pictures>/<theme>/ plus one contact sheet per theme and language in <pictures>/contact-sheets/,
 * and one more per theme of what a mouse and a keyboard do to the controls: hover, pressed, keyboard
 * focus (also on an armed and an engaged speed), disabled, and the Stop hold.
 * Without it (as in CTest, where there is no OBS), it draws the plain Qt look, as before.
 */

#include <QAbstractButton>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFontInfo>
#include <QJsonObject>
#include <QLabel>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "contact-sheet.hpp"
#include "core/check.h"
#include "core/dock-view.h"
#include "core/session.h"
#include "obs-theme.hpp"
#include "ui/dock-theme.hpp"
#include "ui/dock-widgets.hpp"
#include "ui/dock.hpp"

namespace {

const uint64_t SECOND_NS = 1000000000;
const int WIDTHS[] = {300, 260, 220};
const char *const LOCALES[] = {"en-US", "es-ES"};
const int TALL = 1000;        /* px: a dock tall enough for any state, before it is fitted */
const int SPARE_ROOM = 120;   /* px below the content: a dock in OBS is usually taller than what it shows */
const int CENTRAL_WIDTH = 40; /* px of OBS's main window beside the dock, as in OBS */
const char *const ZONES[] = {"tswStatus", "tswMain", "tswCard", "tswSpeeds", "tswStop", "tswContext"};
const int MAX_MINIMUM_WIDTH = 200; /* px: the dock must shrink this far, so the viewport decides (3.10) */
const int MAX_FOCUS_STEPS = 200;   /* more than the dock has widgets: the focus chain is a loop */
const double HERO_FLOOR = 1.5;     /* x b: the smallest the recording hero may get (4.2) */
const double SHAPE = 3.0;          /* WCAG contrast for shapes and text */
const double TEXT = 4.5;
const double TEXT_FLOOR = 7.99;         /* pt: no text smaller than 8 pt (rounding aside), even at Font Size 8 */
const double HOLD = 0.6;                /* how far the Stop hold fill is drawn in its picture */
const uint64_t DEBOUNCE_NS = 400000000; /* a second Pause within 400 ms is a double press (UX-133) */
const int QUICK_PRESSES = 60;           /* Pause and Resume pressed this many times in a row */

int failures = 0;
std::map<std::string, std::string> texts; /* the locale file being tested */
std::set<std::string> missing;            /* keys the dock asked for that the file lacks */

void fail(const QString &what)
{
	std::printf("FAIL  %s\n", qPrintable(what));
	failures++;
}

/* The dock's translator in this test: the locale file, read below; a missing key is noted. */
const char *lookup(const char *key)
{
	const auto found = texts.find(key);
	if (found == texts.end()) {
		missing.insert(key);
		return key;
	}
	return found->second.c_str();
}

/* Reads Key="Value" lines, as OBS does; # starts a comment. */
bool loadLocale(const QString &path)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		return false;
	texts.clear();
	while (!file.atEnd()) {
		const QString line = QString::fromUtf8(file.readLine()).trimmed();
		const qsizetype equals = line.indexOf('=');
		if (line.startsWith('#') || equals < 0)
			continue;
		QString value = line.mid(equals + 1);
		if (value.size() >= 2 && value.startsWith('"') && value.endsWith('"'))
			value = value.mid(1, value.size() - 2);
		texts[line.left(equals).toStdString()] = value.toStdString();
	}
	return true;
}

struct Case {
	const char *name;
	tsw_status status;
	bool startInTimelapse;
	int choice = -1;          /* an input made before the picture (a tsw_input), or -1 */
	uint32_t choiceSpeed = 0; /* its speed */
};

tsw_status status(enum tsw_phase phase, uint32_t speed, uint32_t outcome, uint32_t error)
{
	tsw_status s{};
	s.phase = phase;
	s.speed = speed;
	s.recorded_ns = 1900 * SECOND_NS;
	s.final_ns = 1018 * SECOND_NS;
	s.section_recorded_ns = 1177 * SECOND_NS;
	s.section_final_ns = 294 * SECOND_NS;
	s.path = "/videos/2026-10-15 19-30-00 tswitch.mp4";
	s.outcome = outcome;
	s.error_code = error;
	s.last_recorded_ns = 5025 * SECOND_NS;
	s.last_final_ns = 1870 * SECOND_NS;
	return s;
}

/* Paused for 0:01:12 after recording at speed. */
tsw_status pausedAt(uint32_t speed)
{
	tsw_status s = status(TSW_PHASE_PAUSED, speed, 0, 0);
	s.section_recorded_ns = 72 * SECOND_NS;
	s.section_final_ns = 0;
	return s;
}

/* A start that failed: nothing recorded, no file. */
tsw_status startFailed()
{
	tsw_status s = status(TSW_PHASE_IDLE, 1, 1, 1);
	s.last_recorded_ns = 0;
	s.last_final_ns = 0;
	s.path = "";
	return s;
}

tsw_dock_view viewOf(const Case &c)
{
	tsw_session session{};
	if (tsw_session_init(&session, 4, 0) != TSW_OK)
		fail("session init");
	session.start_in_timelapse = c.startInTimelapse;
	if (c.status.phase == TSW_PHASE_PAUSED && c.status.speed > 1)
		session.timelapse_speed = c.status.speed; /* as recording at that speed leaves it */
	if (c.choice >= 0)
		(void)tsw_session_input(&session, &c.status, static_cast<tsw_input>(c.choice), c.choiceSpeed, 0);
	tsw_dock_model model{};
	tsw_dock_model_init(&model);
	tsw_dock_view view{};
	tsw_dock_view_build(&model, &session, &c.status, &view);
	return view;
}

/* Zones in order, inside the dock; Stop and the card only where they belong. */
void checkZones(const TswDock &dock, const Case &c, const QString &label)
{
	int bottom = -1;
	for (const char *name : ZONES) {
		const auto *zone = dock.findChild<QWidget *>(name);
		if (zone == nullptr) {
			fail(label + ": no zone " + name);
			continue;
		}
		if (!zone->isVisible())
			continue;
		if (zone->geometry().top() <= bottom)
			fail(label + ": " + name + " is not below the zone before it");
		if (zone->geometry().right() >= dock.width())
			fail(label + ": " + name + " is wider than the dock");
		bottom = zone->geometry().bottom();
	}
	const bool recording = c.status.phase != TSW_PHASE_IDLE;
	const auto *pause = dock.findChild<QWidget *>("tswPause");
	const auto *stop = dock.findChild<QWidget *>("tswStop");
	if (stop->isVisible() != recording)
		fail(label + ": Stop shown when it should not be, or the other way round");
	if (pause->isVisible() != recording)
		fail(label + ": Pause shown when it should not be, or the other way round");
	/* Zone E is one row, Pause on the left and Stop on the right, as tall as each other; or, only when
	 * a word would not fit, Pause above Stop, as wide as each other. Never touching. */
	const bool row = pause->y() == stop->y() && pause->height() == stop->height() &&
			 pause->geometry().right() < stop->x() && pause->x() >= 0;
	const bool stacked = pause->x() == stop->x() && pause->width() == stop->width() &&
			     pause->geometry().bottom() < stop->y();
	if (recording && !row && !stacked)
		fail(label +
		     QString(": Pause (%1, %2, %3 x %4) and Stop (%5, %6, %7 x %8) are neither a row nor stacked")
			     .arg(pause->x())
			     .arg(pause->y())
			     .arg(pause->width())
			     .arg(pause->height())
			     .arg(stop->x())
			     .arg(stop->y())
			     .arg(stop->width())
			     .arg(stop->height()));
	if (dock.findChild<QWidget *>("tswCard")->isVisible() != (c.status.outcome != 0))
		fail(label + ": card shown when it should not be, or the other way round");
}

/*
 * Zone D keeps no empty room: its visible captions and rails follow each other with nothing between
 * them but the layout's spacing, from its top to its bottom. A hidden rail that kept its room would
 * show here as a hole (the reserve the recording states once had).
 */
void checkNoReserve(const TswDock &dock, const QString &label)
{
	const auto *speeds = dock.findChild<QWidget *>("tswSpeeds");
	if (!speeds->isVisible())
		return;
	std::vector<std::pair<int, int>> rows; /* top and bottom of each visible caption and rail */
	for (const QWidget *widget : speeds->findChildren<QWidget *>()) {
		const bool part = qobject_cast<const QLabel *>(widget) != nullptr ||
				  qobject_cast<const TswRail *>(widget) != nullptr;
		if (part && widget->isVisible()) {
			const int top = widget->mapTo(speeds, QPoint(0, 0)).y();
			rows.emplace_back(top, top + widget->height());
		}
	}
	std::sort(rows.begin(), rows.end());
	int bottom = 0;
	for (const auto &row : rows) {
		if (row.first - bottom > dock.theme().s4)
			fail(label + QString(": zone D has %1 px of empty room above y %2")
					     .arg(row.first - bottom)
					     .arg(row.first));
		bottom = std::max(bottom, row.second);
	}
	if (speeds->height() - bottom > 1)
		fail(label + QString(": zone D has %1 px of empty room at its bottom").arg(speeds->height() - bottom));
}

/* No text is cut: every widget that grows with its text has the height it needs (UX-121). */
void checkHeights(const TswDock &dock, const QString &label)
{
	for (const QWidget *widget : dock.findChildren<QWidget *>()) {
		if (!widget->isVisible() || !widget->hasHeightForWidth())
			continue;
		if (widget->height() < widget->heightForWidth(widget->width())) {
			const auto *text = qobject_cast<const QLabel *>(widget);
			fail(label + ": text cut in " + widget->metaObject()->className() + " \"" +
			     (text != nullptr ? text->text() : widget->accessibleName()) +
			     QString("\" (%1 px wide, %2 px tall, needs %3 px)")
				     .arg(widget->width())
				     .arg(widget->height())
				     .arg(widget->heightForWidth(widget->width())));
		}
	}
	const auto *status = dock.findChild<QWidget *>("tswStatus");
	if (status->property("tswClipped").toBool())
		fail(label + ": a painted text of the state block does not fit");
	const double floor = dock.theme().pointsAt(HERO_FLOOR);
	if (status->property("tswHeroPoints").toDouble() < floor)
		fail(label + QString(": hero %1 pt, below its floor of %2 pt")
				     .arg(status->property("tswHeroPoints").toDouble())
				     .arg(floor));
	/* The status row is reserved in every state; left empty it looks like a hole (UX-007). */
	if (status->property("tswStatusLine").toString().isEmpty())
		fail(label + ": the status line is empty, a hole in the status card");
	/* A word cannot wrap: every painted button must be at least as wide as its longest word, its
	 * glyph and its padding, or that word runs past its edge. */
	for (const TswButton *button : dock.findChildren<TswButton *>()) {
		if (button->isVisible() && button->width() < button->minimumSizeHint().width())
			fail(label + QString(": a word is cut in %1 \"%2\" (%3 px wide, its longest word needs %4 px)")
					     .arg(button->objectName(), button->accessibleName())
					     .arg(button->width())
					     .arg(button->minimumSizeHint().width()));
	}
	for (const TswRail *rail : dock.findChildren<TswRail *>()) {
		if (rail->isVisible() && rail->property("tswClipped").toBool())
			fail(label + ": a label is wider than its segment in " + rail->objectName());
	}
	const auto *main = dock.findChild<QWidget *>("tswMain");
	if (main->height() < dock.theme().L * 5 / 2)
		fail(label + ": the main button is shorter than 2.5 lines");
}

/* Nothing reaches past what is visible: a narrow OBS dock would cut it off (UX-012). */
void checkFit(const TswDock &dock, const QWidget &viewport, const QString &label)
{
	if (dock.minimumSizeHint().width() > MAX_MINIMUM_WIDTH)
		fail(label + QString(": the dock cannot shrink below %1 px").arg(dock.minimumSizeHint().width()));
	for (const QWidget *widget : dock.findChildren<QWidget *>()) {
		if (!widget->isVisible())
			continue;
		const int right = widget->mapTo(&viewport, QPoint(widget->width(), 0)).x();
		if (right > viewport.width())
			fail(label + QString(": %1 \"%2\" ends at %3 px, only %4 px show")
					     .arg(widget->metaObject()->className(), widget->objectName())
					     .arg(right)
					     .arg(viewport.width()));
	}
}

/* The contrast guard held: shapes 3:1 and texts 4.5:1 against what they sit on (UX-101). */
void checkContrast(const TswDock &dock, const QString &label)
{
	const TswTheme &t = dock.theme();
	const struct {
		const char *what;
		QColor color;
		QColor under;
		double needs;
	} pairs[] = {
		{"accent on the panel", t.accent, t.panel, SHAPE},
		{"danger on the panel", t.danger, t.panel, SHAPE},
		{"the timelapse frame on the panel", t.accent, t.panel, SHAPE},
		{"onAccent on accentFill", t.onAccent, t.accentFill, TEXT},
		{"text on the panel", t.text, t.panel, TEXT},
		{"muted on the panel", t.muted, t.panel, TEXT},
		{"text on accentTint", t.text, t.accentTint, TEXT},
		{"text on dangerTint", t.text, t.dangerTint, TEXT},
		{"accent words on accentTint", t.ink(t.accent, t.accentTint), t.accentTint, TEXT},
		{"the record dot on dangerTint", t.shape(t.danger, t.dangerTint), t.dangerTint, SHAPE},
		{"the focus ring on the panel", t.focus, t.panel, SHAPE},
		{"the real-time outline on the panel", t.frame, t.panel, SHAPE},
		{"the speed badge on the timelapse band", t.badgeFill, t.accentFill, SHAPE},
		{"the badge's number on the badge", t.badgeText, t.badgeFill, TEXT},
		{"the chevrons on accentTint", t.shape(t.accent, t.accentTint), t.accentTint, SHAPE},
		{"the paused frame and bars on the panel", t.shape(t.warning, t.panel), t.panel, SHAPE},
		{"the pause glyph on warningTint", t.shape(t.warning, t.warningTint), t.warningTint, SHAPE},
		{"text on warningTint", t.ink(t.text, t.warningTint), t.warningTint, TEXT},
	};
	for (const auto &pair : pairs) {
		const double contrast = tswContrast(pair.color, pair.under);
		if (contrast < pair.needs)
			fail(label + QString(": %1 is %2:1, needs %3:1")
					     .arg(pair.what)
					     .arg(contrast, 0, 'f', 2)
					     .arg(pair.needs));
	}
	/* Status colors color glyphs and frames, never words (UX-111). A theme without them (System)
	 * gives the text color, which is not a status color. */
	for (const QLabel *text : dock.findChildren<QLabel *>()) {
		const QColor color = text->palette().color(QPalette::WindowText);
		for (const QColor &status : {t.danger, t.warning, t.success}) {
			if (text->isVisible() && status != t.text && color == status)
				fail(label + ": the label \"" + text->text() + "\" is colored with a status color");
		}
	}
}

/* No text under 8 pt: at OBS's Font Size 8 a 0.75b caption would be 6 pt, unreadable even up close. */
void checkTextFloor(const TswDock &dock, const QString &label)
{
	for (const QLabel *text : dock.findChildren<QLabel *>()) {
		const double points = QFontInfo(text->font()).pointSizeF();
		if (text->isVisible() && !text->text().isEmpty() && points < TEXT_FLOOR)
			fail(label + QString(": \"%1\" is %2 pt, under 8 pt").arg(text->text()).arg(points));
	}
	const double painted = dock.findChild<QWidget *>("tswStatus")->property("tswSmallestPoints").toDouble();
	if (painted < TEXT_FLOOR)
		fail(label + QString(": the status card paints a %1 pt text, under 8 pt").arg(painted));
}

/* The card's pills share a row only while neither label wraps; else they stack, full width (3.10). */
void checkCardRows(const TswDock &dock, const QString &label)
{
	const auto *folder = dock.findChild<TswButton *>("tswOpenFolder");
	const auto *play = dock.findChild<TswButton *>("tswPlay");
	if (!folder->isVisible() || !play->isVisible() || folder->y() != play->y())
		return;
	if (folder->labelLines(folder->width()) > 1 || play->labelLines(play->width()) > 1)
		fail(label +
		     QString(": the card's pills sit side by side, but a label wraps (pill %1 px, card %2 px, dock %3 px, viewport %4 px)")
			     .arg(folder->width())
			     .arg(folder->parentWidget()->width())
			     .arg(dock.width())
			     .arg(dock.parentWidget()->width()));
}

/* The main button says what it does, never the state (UX-004); Tab follows the eye (UX-102). */
void checkWordsAndFocus(const TswDock &dock, const QString &label)
{
	const auto *main = dock.findChild<QAbstractButton *>("tswMain");
	for (const char *key : {"State.Hero.RealTime", "State.Hero.Timelapse", "State.Tally.Rec", "State.Tally.Ready",
				"State.Tally.Paused"}) {
		if (main->accessibleName().compare(QString::fromUtf8(lookup(key)), Qt::CaseInsensitive) == 0)
			fail(label + ": the main button is named like a state: " + main->accessibleName());
	}
	int top = -1;
	const QWidget *widget = main;
	for (int step = 0; step < MAX_FOCUS_STEPS; step++) {
		if (widget->isVisible() && widget->isEnabled() && (widget->focusPolicy() & Qt::TabFocus) &&
		    dock.isAncestorOf(widget)) {
			const int y = widget->mapTo(&dock, QPoint(0, 0)).y();
			if (y < top)
				fail(label + QString(": Tab goes back up to %1 \"%2\"")
						     .arg(widget->metaObject()->className(), widget->accessibleName()));
			top = y;
		}
		widget = widget->nextInFocusChain();
		if (widget == main)
			break;
	}
}

/* Where the main button, Pause and Stop are in one state, to compare across states (UX-002). */
struct Positions {
	std::map<std::string, int> main;
	std::map<std::string, int> pause;
	std::map<std::string, int> stop;
	std::map<std::string, int> rail; /* the height of zone D's rail: speeds, or "Resume in" */
};

void notePositions(const TswDock &dock, const Case &c, Positions *positions)
{
	positions->main[c.name] = dock.findChild<QWidget *>("tswMain")->y();
	if (c.status.phase != TSW_PHASE_IDLE) {
		positions->pause[c.name] = dock.findChild<QWidget *>("tswPause")->x();
		positions->stop[c.name] = dock.findChild<QWidget *>("tswStop")->y();
		const char *rail = c.status.phase == TSW_PHASE_PAUSED ? "tswResumeRail" : "tswSpeedRail";
		positions->rail[c.name] = dock.findChild<QWidget *>(rail)->height();
	}
}

void checkPositions(const Positions &positions, const QString &label)
{
	const auto same = [&label](const std::map<std::string, int> &where, const char *what) {
		for (const auto &state : where) {
			if (state.second != where.begin()->second)
				fail(label + QString(": %1 at y %2 in %3 but %4 in %5")
						     .arg(what)
						     .arg(state.second)
						     .arg(QString::fromStdString(state.first))
						     .arg(where.begin()->second)
						     .arg(QString::fromStdString(where.begin()->first)));
		}
	};
	same(positions.main, "the main button");
	same(positions.pause, "Pause (its x)");
	same(positions.rail, "the height of zone D's rail");
	same(positions.stop, "Stop, and Pause beside it,");
}

void checkLayout(const TswDock &dock, const QWidget &viewport, const Case &c, const QString &label)
{
	checkZones(dock, c, label);
	checkNoReserve(dock, label);
	checkHeights(dock, label);
	checkFit(dock, viewport, label);
	checkContrast(dock, label);
	checkTextFloor(dock, label);
	checkCardRows(dock, label);
	checkWordsAndFocus(dock, label);
}

/* The eleven states every picture set shows, in this order: the paused ones right after the
 * recording ones they come from, so the two read side by side. */
std::vector<Case> allCases()
{
	return {
		{"idle", status(TSW_PHASE_IDLE, 1, 0, 0), false},
		{"idle-timelapse", status(TSW_PHASE_IDLE, 1, 0, 0), true},
		{"real-time", status(TSW_PHASE_RECORDING, 1, 0, 0), false},
		{"timelapse", status(TSW_PHASE_RECORDING, 4, 0, 0), false},
		{"paused", pausedAt(1), false},
		{"paused-timelapse", pausedAt(4), false},
		/* Paused in real time, then the x16 key: it will resume in timelapse x16, the longest label. */
		{"paused-choice", pausedAt(1), false, TSW_INPUT_SPEED_KEY, 16},
		{"saving", status(TSW_PHASE_SAVING, 1, 0, 0), false},
		{"summary", status(TSW_PHASE_IDLE, 1, 1, 0), false},
		{"error-103", status(TSW_PHASE_IDLE, 1, 1, 103), false},
		{"error-001", startFailed(), false},
	};
}

/* The case of that name. */
Case caseNamed(const char *name)
{
	for (const Case &c : allCases()) {
		if (std::strcmp(c.name, name) == 0)
			return c;
	}
	fail(QString("no case named %1").arg(name));
	return allCases().front();
}

/* Lets Qt lay the widgets out; a narrow dock relays itself out once more. */
void settle()
{
	for (int pass = 0; pass < 3; pass++)
		QApplication::processEvents();
}

/* A dock in a scroll area of this width, showing a case, settled (as in OBS). */
QScrollArea *showCase(const Case &c, int width)
{
	auto *dock = new TswDock(lookup);
	QScrollArea *area = TswDock::inScrollArea(dock);
	area->setFixedWidth(width);
	area->resize(width, TALL);
	dock->showView(viewOf(c));
	area->show();
	settle();
	return area;
}

/*
 * A dock the user narrows (or widens) must look exactly like one made at the new width: nothing
 * kept from the old one (UX-012). OBS's own picture of the dock is made this way (theme-probe).
 */
void checkResize(const QString &locale)
{
	const std::vector<Case> cases = allCases();
	for (const Case &c : cases) {
		QScrollArea *resized = showCase(c, WIDTHS[0]);
		for (const int width : {WIDTHS[2], WIDTHS[0]}) {
			resized->setFixedWidth(width);
			settle();
			QScrollArea *fresh = showCase(c, width);
			const QImage before = resized->grab().toImage().convertToFormat(QImage::Format_RGB32);
			const QImage after = fresh->grab().toImage().convertToFormat(QImage::Format_RGB32);
			if (before != after)
				fail(QString("%1 %2 resized to %3 px: not as a dock made at that width")
					     .arg(locale, c.name)
					     .arg(width));
			delete fresh;
		}
		delete resized;
	}
}

/* A theme author's color replaces the theme's, and still passes the contrast guard (UX-015). */
void checkThemeHooks()
{
	const QColor green(0, 160, 0);
	QScrollArea *area = showCase(caseNamed("timelapse"), WIDTHS[0]);
	auto *dock = area->findChild<TswDock *>();
	dock->setProperty("accentColor", green);
	settle();
	const QColor accent = dock->theme().accent;
	if (std::abs(accent.hslHue() - green.hslHue()) > 2 || tswContrast(accent, dock->theme().panel) < SHAPE)
		fail(QString("accentColor %1 gave the accent %2").arg(green.name(), accent.name()));
	delete area;
}

/*
 * Reduce motion (UX-045): a new speed moves the thumb at once, and turning it on ends a slide under
 * way. The same change without it must slide, or this check would test nothing.
 */
void checkReduceMotion()
{
	const Case realTime = caseNamed("real-time");
	for (const bool reduce : {false, true}) {
		QScrollArea *area = showCase(realTime, WIDTHS[0]);
		auto *dock = area->findChild<TswDock *>();
		dock->setReduceMotion(reduce);
		tsw_dock_view view = viewOf(realTime);
		view.speed_selected = 8; /* the next speed changes: the thumb goes from x4 to x8 */
		dock->showView(view);
		const QWidget *rail = dock->findChild<QWidget *>("tswSpeed8")->parentWidget();
		const bool moving = rail->property("tswMoving").toBool();
		if (reduce && moving)
			fail("Reduce motion is on, but the thumb slides");
		if (!reduce && !moving)
			fail("the thumb did not slide, so the Reduce motion check tests nothing");
		dock->setReduceMotion(true);
		if (rail->property("tswMoving").toBool())
			fail("Reduce motion was turned on, but the slide under way goes on");
		delete area;
	}
}

/* A recorder as the dock sees it: the core session acts on each press at the time given, and the
 * status follows its commands, as tsw_controller_input does. */
struct QuickRecorder {
	tsw_session session{};
	tsw_dock_model model{};
	tsw_status status = caseNamed("timelapse").status;
	uint64_t now = 0;

	tsw_dock_view press(int input, unsigned speed)
	{
		const tsw_command command =
			tsw_session_input(&session, &status, static_cast<tsw_input>(input), speed, now);
		if (command.kind == TSW_COMMAND_PAUSE) {
			status.phase = TSW_PHASE_PAUSED;
			status.section_recorded_ns = 0;
		} else if (command.kind == TSW_COMMAND_RESUME) {
			status.phase = TSW_PHASE_RECORDING;
			status.speed = command.speed;
		}
		tsw_dock_view view{};
		tsw_dock_view_build(&model, &session, &status, &view);
		return view;
	}
};

/*
 * UX-133: Pause and Resume pressed quickly and many times. Each press the core takes shows at once
 * (the dock draws the new view right after the click, as dock-obs.cpp does); a second press within
 * 400 ms changes nothing; Pause, Stop and the main button never move, and no word is left behind.
 */
void checkQuickPauses(const QString &locale)
{
	QuickRecorder recorder;
	if (tsw_session_init(&recorder.session, 4, DEBOUNCE_NS) != TSW_OK)
		fail("session init");
	tsw_dock_model_init(&recorder.model);
	QScrollArea *area = showCase(caseNamed("timelapse"), WIDTHS[2]);
	auto *dock = area->findChild<TswDock *>();
	QObject::connect(dock, &TswDock::inputRequested, dock, [&recorder, dock](int input, unsigned speed) {
		dock->showView(recorder.press(input, speed));
	});
	auto *pause = dock->findChild<QAbstractButton *>("tswPause");
	const QWidget *main = dock->findChild<QWidget *>("tswMain");
	const QPoint where[] = {pause->pos(), dock->findChild<QWidget *>("tswStop")->pos(), main->pos()};
	for (int i = 0; i < QUICK_PRESSES; i++) {
		const bool doublePress = i % 4 == 3;
		recorder.now += doublePress ? DEBOUNCE_NS / 4 : DEBOUNCE_NS + 1;
		const bool wasPaused = recorder.status.phase == TSW_PHASE_PAUSED;
		pause->click();
		settle();
		const bool paused = recorder.status.phase == TSW_PHASE_PAUSED;
		const QString label = QString("%1 quick press %2").arg(locale).arg(i + 1);
		if (paused != (doublePress ? wasPaused : !wasPaused))
			fail(label + (doublePress ? ": a double press changed the state" : ": the press did nothing"));
		const QString expected = QString::fromUtf8(lookup(paused ? "A11y.Resume" : "A11y.Pause"));
		if (pause->accessibleName() != expected)
			fail(label + ": the button says \"" + pause->accessibleName() + "\", not \"" + expected + "\"");
		const QString tally = QString::fromUtf8(lookup(paused ? "State.Tally.Paused" : "A11y.Recording"));
		const QString state = dock->findChild<QWidget *>("tswStatus")->accessibleName();
		if (!state.startsWith(tally, Qt::CaseInsensitive))
			fail(label + ": the state block says \"" + state + "\"");
		const QPoint now[] = {pause->pos(), dock->findChild<QWidget *>("tswStop")->pos(), main->pos()};
		if (now[0] != where[0] || now[1] != where[1] || now[2] != where[2])
			fail(label + ": Pause, Stop or the main button moved");
	}
	delete area;
}

void checkLocale(const QString &locale, const QString &pictures)
{
	checkResize(locale);
	checkThemeHooks();
	checkReduceMotion();
	checkQuickPauses(locale);
	const std::vector<Case> cases = allCases();
	for (const int width : WIDTHS) {
		Positions positions;
		for (const Case &c : cases) {
			auto *dock = new TswDock(lookup);
			QScrollArea *area = TswDock::inScrollArea(dock); /* as in OBS */
			area->setFixedWidth(width);
			area->resize(width, 1000);
			dock->showView(viewOf(c));
			area->show();
			settle();
			const QString label = QString("%1 %2 px %3").arg(locale).arg(width).arg(c.name);
			checkLayout(*dock, *area->viewport(), c, label);
			notePositions(*dock, c, &positions);
			dock->grab().save(QString("%1/%2-%3-%4.png").arg(pictures, locale).arg(width).arg(c.name));
			delete area;
		}
		checkPositions(positions, QString("%1 %2 px").arg(locale).arg(width));
	}
}

/* --- The dock in OBS's themes (tests/ui/README.md) --- */

/* A font as OBS's own dock had it (theme-probe.cpp) against the one drawn here. */
void compareFont(const QJsonObject &obs, const QFont &font, const QString &label)
{
	const QFontInfo here(font);
	const QString mine = QString("%1 %2 pt %3 px%4")
				     .arg(here.family())
				     .arg(here.pointSizeF())
				     .arg(here.pixelSize())
				     .arg(here.bold() ? " bold" : "");
	const QString theirs = QString("%1 %2 pt %3 px%4")
				       .arg(obs["family"].toString())
				       .arg(obs["pointSize"].toDouble())
				       .arg(obs["pixelSize"].toInt())
				       .arg(obs["bold"].toBool() ? " bold" : "");
	if (mine != theirs)
		fail(label + ": " + mine + ", but OBS draws " + theirs);
}

/* Where two pictures of the same size differ: the number of pixels, and the first one. */
QString difference(const QImage &here, const QImage &obs)
{
	int count = 0;
	QString first;
	for (int y = 0; y < here.height(); y++) {
		for (int x = 0; x < here.width(); x++) {
			if (here.pixel(x, y) == obs.pixel(x, y))
				continue;
			if (count++ == 0)
				first = QString("first at (%1, %2): %3 here, %4 in OBS")
						.arg(x)
						.arg(y)
						.arg(here.pixelColor(x, y).name(), obs.pixelColor(x, y).name());
		}
	}
	return count == 0 ? QString() : QString("%1 pixels differ, %2").arg(count).arg(first);
}

/*
 * Is this the dock OBS draws? Idle, the dock shows the same words here and in OBS, so its content,
 * drawn at the size of OBS's picture (theme-probe.cpp), must match it pixel by pixel. The fonts
 * are compared too, because a font that differs explains a difference best.
 */
void compareWithObs(const QJsonObject &reference, const QString &folder, OBSBasic &window, int width, QScrollArea &area,
		    const QString &label)
{
	const QString name = reference[QString::number(width)].toString();
	if (name.isEmpty()) {
		std::printf("NOTE  %s: no picture of OBS's own dock to compare with\n", qPrintable(label));
		return;
	}
	const auto *dock = area.findChild<TswDock *>();
	compareFont(reference["dockFont"].toObject(), dock->font(), label + ", dock font");
	compareFont(reference["mainButtonFont"].toObject(), dock->findChild<QWidget *>("tswMain")->font(),
		    label + ", main button font");
	const QImage obs = QImage(folder + "/" + name).convertToFormat(QImage::Format_RGB32);
	window.resize(width + CENTRAL_WIDTH, window.height() - area.height() + obs.height());
	settle();
	checkHeights(*dock, label + ", as tall as OBS's picture"); /* a scroll bar may now take room */
	const QImage here = area.grab().toImage().convertToFormat(QImage::Format_RGB32);
	if (here.size() != obs.size())
		fail(label + QString(": drawn %1x%2, OBS's picture is %3x%4")
				     .arg(here.width())
				     .arg(here.height())
				     .arg(obs.width())
				     .arg(obs.height()));
	else if (const QString differ = difference(here, obs); !differ.isEmpty())
		fail(label + ": not as OBS draws it: " + differ);
	else
		std::printf("same  %s: pixel for pixel as OBS draws it\n", qPrintable(label));
}

/* The dock in an OBSDock docked on the right of an OBSBasic, as obs_frontend_add_dock_by_id does
 * once the user docks it, showing a case; the window is shown, tall. */
TswDock *dockInObs(OBSBasic &window, const Case &c, int width)
{
	window.setCentralWidget(new QWidget(&window));
	auto *frame = new OBSDock(&window);
	frame->setObjectName("timelapse-switch");
	frame->setWindowTitle(QString::fromUtf8(lookup("Dock.Title")));
	frame->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable |
			   QDockWidget::DockWidgetFloatable);
	auto *dock = new TswDock(lookup);
	frame->setWidget(TswDock::inScrollArea(dock));
	frame->setFixedWidth(width);
	window.addDockWidget(Qt::RightDockWidgetArea, frame);
	dock->showView(viewOf(c));
	window.resize(width + CENTRAL_WIDTH, TALL);
	window.show();
	settle();
	return dock;
}

/* Sizes the window to the dock's content plus some spare room, as a dock in OBS usually has, and
 * draws the dock with OBS's window around its rounded corners. */
QImage grabDock(OBSBasic &window, const QString &label)
{
	auto *frame = window.findChild<OBSDock *>();
	auto *area = qobject_cast<QScrollArea *>(frame->widget());
	const int chrome = frame->height() - area->viewport()->height();
	const int content = area->widget()->heightForWidth(area->viewport()->width());
	window.resize(frame->width() + CENTRAL_WIDTH, chrome + content + SPARE_ROOM);
	settle();
	if (area->verticalScrollBar()->isVisible())
		fail(label + ": the dock scrolls although it is taller than its content");
	return window.grab(frame->geometry()).toImage();
}

/*
 * One state in an OBS-like window (dockInObs). The checks run while the dock is tall; then it is
 * drawn as OBS shows it (grabDock).
 */
QImage renderInObs(const ObsTheme &theme, const QString &locale, const Case &c, int width, const QString &label,
		   Positions *positions)
{
	OBSBasic window;
	TswDock *dock = dockInObs(window, c, width);
	auto *area = qobject_cast<QScrollArea *>(dock->parentWidget()->parentWidget());
	checkLayout(*dock, *area->viewport(), c, label);
	notePositions(*dock, c, positions);
	/* In a narrow OBS dock the scroll area can make the dock wider than what shows: cut off. */
	if (dock->width() > area->viewport()->width())
		fail(label + QString(": the dock is %1 px wide but only %2 px show; the rest is cut off")
				     .arg(dock->width())
				     .arg(area->viewport()->width()));
	if (c.status.phase == TSW_PHASE_IDLE && c.status.outcome == 0 && !c.startInTimelapse)
		compareWithObs(theme.references.value(locale), theme.folder, window, width, *area, label);
	return grabDock(window, label);
}

/* --- What a mouse and a keyboard do to the controls --- */

enum class Act { Hover, Press, Focus, Hold, None };

struct Interaction {
	const char *label;
	const char *state;  /* a case of allCases() */
	const char *widget; /* object name, or nullptr for the state alone */
	Act act;
};

/* Every look a control can take that a still picture of a state never shows; focus also where it
 * meets an outline that means something (the armed thumb, the armed switch). */
const Interaction INTERACTIONS[] = {
	{"Record: hover", "idle", "tswMain", Act::Hover},
	{"Record: pressed", "idle", "tswMain", Act::Press},
	{"Record: keyboard focus", "idle", "tswMain", Act::Focus},
	{"start Timelapse: focus", "idle", "tswStartTimelapse", Act::Focus},
	{"armed x4: focus", "idle", "tswSpeed4", Act::Focus},
	{"x8: hover", "idle", "tswSpeed8", Act::Hover},
	{"x8: focus", "idle", "tswSpeed8", Act::Focus},
	{"armed switch: focus", "real-time", "tswMain", Act::Focus},
	{"engaged x4: focus", "timelapse", "tswSpeed4", Act::Focus},
	{"Pause: hover", "timelapse", "tswPause", Act::Hover},
	{"Pause: focus", "real-time", "tswPause", Act::Focus},
	{"Stop: hover", "timelapse", "tswStop", Act::Hover},
	{"Stop: held 60 %", "timelapse", "tswStop", Act::Hold},
	{"Resume: pressed", "paused", "tswPause", Act::Press},
	{"resume x8: focus", "paused", "tswResume8", Act::Focus},
	{"resume Real time: focus", "paused-timelapse", "tswResumeRealTime", Act::Focus},
	{"armed resume: focus", "paused-timelapse", "tswMain", Act::Focus},
	{"saving: disabled", "saving", nullptr, Act::None},
	{"Open folder: focus", "summary", "tswOpenFolder", Act::Focus},
	{"Close: hover", "summary", "tswDismiss", Act::Hover},
	{"Learn more: focus", "error-103", "tswLearnMore", Act::Focus},
};

/* Does to widget what a mouse or a keyboard would; returns false if it did not take. */
bool perform(OBSBasic &window, QWidget *widget, Act act)
{
	switch (act) {
	case Act::Hover: /* what Qt sets when the mouse enters a widget with WA_Hover */
		widget->setAttribute(Qt::WA_UnderMouse, true);
		widget->update();
		return true;
	case Act::Press:
		static_cast<QAbstractButton *>(widget)->setDown(true);
		return true;
	case Act::Focus:
		window.activateWindow();
		settle();
		widget->setFocus(Qt::TabFocusReason); /* as Tab does: the dock shows its focus ring */
		settle();
		return widget->hasFocus();
	case Act::Hold:
		static_cast<TswButton *>(widget)->setHoldProgress(HOLD);
		return true;
	case Act::None:
		break;
	}
	return true;
}

/* One row of the interaction sheet: every interaction, in the loaded language, at one width. */
QList<Shot> renderInteractions(const ObsTheme &theme, const QString &locale, int width)
{
	QList<Shot> row;
	for (const Interaction &interaction : INTERACTIONS) {
		const QString label =
			QString("%1 %2 %3 px %4").arg(theme.name, locale).arg(width).arg(interaction.label);
		OBSBasic window;
		TswDock *dock = dockInObs(window, caseNamed(interaction.state), width);
		QWidget *widget = interaction.widget != nullptr ? dock->findChild<QWidget *>(interaction.widget)
								: nullptr;
		if (interaction.widget != nullptr && (widget == nullptr || !perform(window, widget, interaction.act)))
			fail(label + ": could not do it, so its picture shows nothing");
		row.append(
			{QString("%1 (%2, %3 px)").arg(interaction.label, locale).arg(width), grabDock(window, label)});
	}
	return row;
}

/* The look of the contact sheet: OBS's window color, text color and font under the theme. */
SheetLook sheetLook()
{
	OBSBasic window;
	window.resize(16, 16);
	const QImage empty = window.grab().toImage();
	QLabel text(&window);
	text.ensurePolished();
	return {empty.pixelColor(8, 8), text.palette().color(QPalette::WindowText), text.font()};
}

/* The pictures of one theme in one language, and their contact sheet. */
void renderTheme(const ObsTheme &theme, const QString &locale, const QString &pictures)
{
	const QString folder = pictures + "/" + theme.name;
	QDir().mkpath(folder);
	const std::vector<Case> cases = allCases();
	QList<QList<Shot>> rows;
	for (const int width : WIDTHS) {
		QList<Shot> row;
		Positions positions;
		for (const Case &c : cases) {
			const QString label = QString("%1 %2 %3 px %4").arg(theme.name, locale).arg(width).arg(c.name);
			const QImage shot = renderInObs(theme, locale, c, width, label, &positions);
			shot.save(QString("%1/%2-%3-%4.png").arg(folder, locale).arg(width).arg(c.name));
			row.append({QString("%1 (%2 px)").arg(c.name).arg(width), shot});
		}
		checkPositions(positions, QString("%1 %2 %3 px").arg(theme.name, locale).arg(width));
		const QString obs = theme.references.value(locale)[QString::number(width)].toString();
		if (!obs.isEmpty())
			row.append({QString("OBS itself, idle (%1 px)").arg(width), QImage(theme.folder + "/" + obs)});
		rows.append(row);
	}
	/* Dark or light as drawn: OBS calls the System theme light even on a dark desktop. */
	const bool dark = theme.palette.color(QPalette::Window).lightnessF() < 0.5;
	const QString title = QString("%1 (%2, %3 palette), %4. Rows: %5, %6 and %7 px; last column: OBS's own dock")
				      .arg(theme.name, theme.id, dark ? "dark" : "light", locale)
				      .arg(WIDTHS[0])
				      .arg(WIDTHS[1])
				      .arg(WIDTHS[2]);
	QDir().mkpath(pictures + "/contact-sheets");
	contactSheet(title, rows, sheetLook())
		.save(QString("%1/contact-sheets/%2-%3.png").arg(pictures, theme.name, locale));
}

/* Every theme in the dumps folder, in both languages. */
void renderThemes(const QString &locales, const QString &pictures, const QString &dumps)
{
	const QStringList folders = QDir(dumps).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
	if (folders.isEmpty())
		fail("no OBS theme dumps in " + dumps + " (run tests/ui/obs-themes.sh)");
	for (const QString &folder : folders) {
		ObsTheme theme;
		QString error;
		if (!loadObsTheme(dumps + "/" + folder, &theme, &error) || !applyObsTheme(theme, &error)) {
			fail(error);
			continue;
		}
		const double dpi = QGuiApplication::primaryScreen()->logicalDotsPerInch();
		if (theme.logicalDpi != dpi)
			std::printf("NOTE  %s: OBS ran at %g dpi, these pictures at %g dpi\n", qPrintable(theme.name),
				    theme.logicalDpi, dpi);
		QList<QList<Shot>> interactions;
		for (const char *locale : LOCALES) {
			if (!loadLocale(locales + "/" + locale + ".ini")) {
				fail(QString("cannot read %1.ini").arg(locale));
				continue;
			}
			missing.clear();
			renderTheme(theme, locale, pictures);
			/* English at the widest width, Spanish at the narrowest: the two extremes. */
			const bool english = QString(locale) == LOCALES[0];
			interactions.append(renderInteractions(theme, locale, english ? WIDTHS[0] : WIDTHS[2]));
			for (const std::string &key : missing)
				fail(QString("%1.ini has no %2").arg(locale, QString::fromStdString(key)));
		}
		contactSheet(QString("%1: hover, pressed, keyboard focus, disabled and the Stop hold").arg(theme.name),
			     interactions, sheetLook())
			.save(QString("%1/contact-sheets/%2-interaction.png").arg(pictures, theme.name));
		std::printf("theme %s: pictures in %s/%s\n", qPrintable(theme.name), qPrintable(pictures),
			    qPrintable(theme.name));
	}
}

} // namespace

/* The core's checks report here: in this test any failed check is a failure. */
extern "C" void tsw_check_failed(const char *file, int line, const char *condition)
{
	std::printf("FAIL  check %s:%d: %s\n", file, line, condition);
	failures++;
}

int main(int argc, char **argv)
{
	QApplication app(argc, argv);
	if (argc < 3) {
		std::printf("usage: test-dock <locale folder> <pictures folder> [<OBS theme dumps folder>]\n");
		return 2;
	}
	const QString locales = QString::fromLocal8Bit(argv[1]);
	const QString pictures = QString::fromLocal8Bit(argv[2]);
	QDir().mkpath(pictures);
	if (argc >= 4) {
		renderThemes(locales, pictures, QString::fromLocal8Bit(argv[3]));
		std::printf("%s: %d failures\n", failures == 0 ? "PASS" : "FAIL", failures);
		return failures;
	}
	for (const char *locale : LOCALES) {
		if (!loadLocale(locales + "/" + locale + ".ini")) {
			fail(QString("cannot read %1.ini").arg(locale));
			continue;
		}
		missing.clear();
		checkLocale(locale, pictures);
		for (const std::string &key : missing)
			fail(QString("%1.ini has no %2").arg(locale, QString::fromStdString(key)));
	}
	std::printf("%s: %d failures; pictures in %s\n", failures == 0 ? "PASS" : "FAIL", failures,
		    qPrintable(pictures));
	return failures;
}
