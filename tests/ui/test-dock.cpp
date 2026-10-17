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
 *     every state, Stop in every recording state (fixed positions);
 *   - nothing is wider than what is visible, even at 220 px (UX-012), and the dock can shrink to
 *     200 px;
 *   - the main button is at least 2.5 lines tall (UX-107), and its words are a verb, never a state;
 *   - no text is cut: every label and painted block is as tall as its lines need (UX-121), and
 *     the painted hero never falls below its floor size;
 *   - the colors pass the contrast guard, and no label is colored with a status color (UX-111);
 *   - Tab visits the controls in visual order (UX-102);
 *   - Stop and the card appear only when they should.
 * It also saves a picture of each case, for a person to look at.
 *
 *   test-dock <folder with en-US.ini and es-ES.ini> <folder for the pictures> [<OBS theme dumps>]
 *
 * With a third folder (written by tests/ui/obs-themes.sh, one subfolder per OBS theme), it dresses
 * the dock in each theme exactly as OBS would (obs-theme.hpp), puts it in an OBS-like dock frame,
 * runs the same checks, compares fonts and background with OBS's own dock, and saves the pictures
 * in <pictures>/<theme>/ plus one contact sheet per theme and language in <pictures>/contact-sheets/.
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
#include <map>
#include <set>
#include <string>
#include <vector>

#include "contact-sheet.hpp"
#include "core/check.h"
#include "core/dock-view.h"
#include "obs-theme.hpp"
#include "ui/dock-theme.hpp"
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
	if (dock.findChild<QWidget *>("tswStop")->isVisible() != recording)
		fail(label + ": Stop shown when it should not be, or the other way round");
	if (dock.findChild<QWidget *>("tswCard")->isVisible() != (c.status.outcome != 0))
		fail(label + ": card shown when it should not be, or the other way round");
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
			     (text != nullptr ? text->text() : widget->accessibleName()) + "\"");
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

/* The main button says what it does, never the state (UX-004); Tab follows the eye (UX-102). */
void checkWordsAndFocus(const TswDock &dock, const QString &label)
{
	const auto *main = dock.findChild<QAbstractButton *>("tswMain");
	for (const char *key :
	     {"State.Hero.RealTime", "State.Hero.Timelapse", "State.Tally.Rec", "State.Tally.Ready"}) {
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

/* Where the main button and Stop are in one state, to compare across states (UX-002). */
struct Positions {
	std::map<std::string, int> main;
	std::map<std::string, int> stop;
};

void notePositions(const TswDock &dock, const Case &c, Positions *positions)
{
	positions->main[c.name] = dock.findChild<QWidget *>("tswMain")->y();
	if (c.status.phase != TSW_PHASE_IDLE)
		positions->stop[c.name] = dock.findChild<QWidget *>("tswStop")->y();
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
	same(positions.stop, "Stop");
}

void checkLayout(const TswDock &dock, const QWidget &viewport, const Case &c, const QString &label)
{
	checkZones(dock, c, label);
	checkHeights(dock, label);
	checkFit(dock, viewport, label);
	checkContrast(dock, label);
	checkWordsAndFocus(dock, label);
}

/* The eight states every picture set shows, in this order. */
std::vector<Case> allCases()
{
	return {
		{"idle", status(TSW_PHASE_IDLE, 1, 0, 0), false},
		{"idle-timelapse", status(TSW_PHASE_IDLE, 1, 0, 0), true},
		{"real-time", status(TSW_PHASE_RECORDING, 1, 0, 0), false},
		{"timelapse", status(TSW_PHASE_RECORDING, 4, 0, 0), false},
		{"saving", status(TSW_PHASE_SAVING, 1, 0, 0), false},
		{"summary", status(TSW_PHASE_IDLE, 1, 1, 0), false},
		{"error-103", status(TSW_PHASE_IDLE, 1, 1, 103), false},
		{"error-001", startFailed(), false},
	};
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
	QScrollArea *area = showCase(allCases()[3], WIDTHS[0]);
	auto *dock = area->findChild<TswDock *>();
	dock->setProperty("accentColor", green);
	settle();
	const QColor accent = dock->theme().accent;
	if (std::abs(accent.hslHue() - green.hslHue()) > 2 || tswContrast(accent, dock->theme().panel) < SHAPE)
		fail(QString("accentColor %1 gave the accent %2").arg(green.name(), accent.name()));
	delete area;
}

void checkLocale(const QString &locale, const QString &pictures)
{
	checkResize(locale);
	checkThemeHooks();
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

/*
 * One state in an OBS-like window: the dock in its scroll area, in an OBSDock docked on the right
 * of an OBSBasic, as obs_frontend_add_dock_by_id does once the user docks it. The checks run while
 * the dock is tall; then it is sized to its content plus some spare room, as a dock in OBS usually
 * has, and drawn with OBS's window around its rounded corners.
 */
QImage renderInObs(const ObsTheme &theme, const QString &locale, const Case &c, int width, const QString &label,
		   Positions *positions)
{
	OBSBasic window;
	window.setCentralWidget(new QWidget(&window));
	auto *frame = new OBSDock(&window);
	frame->setObjectName("timelapse-switch");
	frame->setWindowTitle(QString::fromUtf8(lookup("Dock.Title")));
	frame->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable |
			   QDockWidget::DockWidgetFloatable);
	auto *dock = new TswDock(lookup);
	QScrollArea *area = TswDock::inScrollArea(dock);
	frame->setWidget(area);
	frame->setFixedWidth(width);
	window.addDockWidget(Qt::RightDockWidgetArea, frame);
	dock->showView(viewOf(c));
	window.resize(width + CENTRAL_WIDTH, TALL);
	window.show();
	settle();
	checkLayout(*dock, *area->viewport(), c, label);
	notePositions(*dock, c, positions);
	/* In a narrow OBS dock the scroll area can make the dock wider than what shows: cut off. */
	if (dock->width() > area->viewport()->width())
		fail(label + QString(": the dock is %1 px wide but only %2 px show; the rest is cut off")
				     .arg(dock->width())
				     .arg(area->viewport()->width()));
	if (c.status.phase == TSW_PHASE_IDLE && c.status.outcome == 0 && !c.startInTimelapse)
		compareWithObs(theme.references.value(locale), theme.folder, window, width, *area, label);

	const int chrome = frame->height() - area->viewport()->height();
	const int content = dock->heightForWidth(area->viewport()->width());
	window.resize(width + CENTRAL_WIDTH, chrome + content + SPARE_ROOM);
	settle();
	if (area->verticalScrollBar()->isVisible())
		fail(label + ": the dock scrolls although it is taller than its content");
	return window.grab(frame->geometry()).toImage();
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
	const QString title = QString("%1 (OBS calls it %2), %3. Rows: %4, %5 and %6 px; last column: OBS's own dock")
				      .arg(theme.id, theme.dark ? "dark" : "light", locale)
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
		for (const char *locale : LOCALES) {
			if (!loadLocale(locales + "/" + locale + ".ini")) {
				fail(QString("cannot read %1.ini").arg(locale));
				continue;
			}
			missing.clear();
			renderTheme(theme, locale, pictures);
			for (const std::string &key : missing)
				fail(QString("%1.ini has no %2").arg(locale, QString::fromStdString(key)));
		}
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
