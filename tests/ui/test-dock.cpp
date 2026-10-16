/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * test-dock.cpp: the dock drawn without OBS (Qt's offscreen platform), in every state, in English
 * and Spanish, at a normal width and at the narrowest one it must support. It checks:
 *   - every text the dock asks for exists in the locale file, so no raw key ever shows (UX-120);
 *   - the zones keep their order from top to bottom (UX-002);
 *   - nothing is wider than the dock, even at 220 px (UX-012);
 *   - the main button is at least 2.5 times the font height (UX-107);
 *   - no text is cut: every label is as tall as its wrapped lines need (UX-121);
 *   - Stop and the card appear only when they should.
 * It also saves a picture of each case, for a person to look at.
 *
 *   test-dock <folder with en-US.ini and es-ES.ini> <folder for the pictures>
 */

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <cstdio>
#include <map>
#include <set>
#include <string>

#include "core/check.h"
#include "core/dock-view.h"
#include "ui/dock.hpp"

namespace {

const uint64_t SECOND_NS = 1000000000;
const int WIDTHS[] = {300, 220};
const char *const ZONES[] = {"tswBanner", "tswCounters", "tswSection", "tswCard",
			     "tswMain",   "tswSpeeds",   "tswStop",    "tswContext"};

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
void checkLayout(const TswDock &dock, const Case &c, const QString &label)
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
	const auto *main = dock.findChild<QPushButton *>("tswMain");
	if (main->height() < main->fontMetrics().height() * 5 / 2)
		fail(label + ": the main button is shorter than 2.5 lines");
	if (dock.minimumSizeHint().width() > dock.width())
		fail(label + QString(": needs %1 px").arg(dock.minimumSizeHint().width()));
	for (const QLabel *text : dock.findChildren<QLabel *>()) {
		if (text->isVisible() && text->wordWrap() && text->height() < text->heightForWidth(text->width()))
			fail(label + ": text cut: \"" + text->text() + "\"");
	}
}

void checkLocale(const QString &locale, const QString &pictures)
{
	const Case cases[] = {
		{"idle", status(TSW_PHASE_IDLE, 1, 0, 0), false},
		{"idle-timelapse", status(TSW_PHASE_IDLE, 1, 0, 0), true},
		{"real-time", status(TSW_PHASE_RECORDING, 1, 0, 0), false},
		{"timelapse", status(TSW_PHASE_RECORDING, 4, 0, 0), false},
		{"saving", status(TSW_PHASE_SAVING, 1, 0, 0), false},
		{"summary", status(TSW_PHASE_IDLE, 1, 1, 0), false},
		{"error-103", status(TSW_PHASE_IDLE, 1, 1, 103), false},
		{"error-001", startFailed(), false},
	};
	for (const int width : WIDTHS) {
		for (const Case &c : cases) {
			auto *dock = new TswDock(lookup);
			QScrollArea *area = TswDock::inScrollArea(dock); /* as in OBS */
			area->setFixedWidth(width);
			area->resize(width, 1000);
			dock->showView(viewOf(c));
			area->show();
			for (int pass = 0; pass < 3; pass++) /* a narrow dock relays itself out once more */
				QApplication::processEvents();
			const QString label = QString("%1 %2 px %3").arg(locale).arg(width).arg(c.name);
			checkLayout(*dock, c, label);
			dock->grab().save(QString("%1/%2-%3-%4.png").arg(pictures, locale).arg(width).arg(c.name));
			delete area;
		}
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
		std::printf("usage: test-dock <locale folder> <pictures folder>\n");
		return 2;
	}
	const QString locales = QString::fromLocal8Bit(argv[1]);
	const QString pictures = QString::fromLocal8Bit(argv[2]);
	QDir().mkpath(pictures);
	for (const char *locale : {"en-US", "es-ES"}) {
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
