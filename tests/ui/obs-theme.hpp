/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * obs-theme.hpp: an OBS theme for the UI test (tests/ui/README.md). theme-probe.cpp writes it down
 * inside a sandbox OBS; this reads it back and applies it to the test program the way OBS applies
 * a theme to itself (OBSApp::InitTheme and OBSApp::SetTheme in OBS's frontend/OBSApp_Themes.cpp):
 *
 *   OBS's fonts, "theme:" paths --> Fusion style --> desktop font --> palette --> style sheet
 *
 * It also has the two OBS classes the themes style a dock through. Theme rules such as
 * "OBSDock > QWidget" pick widgets by class name, so the test's stand-ins carry the same names.
 */

#pragma once

#include <QDockWidget>
#include <QFont>
#include <QJsonObject>
#include <QMainWindow>
#include <QMap>
#include <QPalette>
#include <QString>
#include <QStringList>

/* OBS's main window (class and object name OBSBasic), for the theme's sake only. */
class OBSBasic : public QMainWindow {
	Q_OBJECT

public:
	OBSBasic();
};

/* The frame OBS puts a plugin's dock in (obs_frontend_add_dock_by_id). */
class OBSDock : public QDockWidget {
	Q_OBJECT

public:
	explicit OBSDock(QWidget *parent);
};

struct ObsTheme {
	QString folder;                         /* the dump, such as build_x86_64/obs-themes/Yami.Light */
	QString name;                           /* the dump folder's name: Yami.Light */
	QString id;                             /* com.obsproject.Yami.Light */
	bool dark = false;                      /* as OBS reports it (obs_frontend_is_theme_dark) */
	QString style;                          /* the Qt style under OBS's proxy: fusion on Linux */
	QFont font;                             /* the desktop's font; the style sheet overrides parts */
	QPalette palette;                       /* every group and role */
	QString styleSheet;                     /* with OBS's own pictures pointing to their copies */
	QMap<QString, QStringList> searchPaths; /* "theme" --> OBS's theme folders */
	QStringList fontFiles;                  /* OBS's own fonts, copied into the dump */
	QMap<QString, QJsonObject> references;  /* OBS's own dock, by language (theme-probe.cpp) */
	double logicalDpi = 0;                  /* of the screen OBS ran on */
};

/* Reads a dump folder. Returns false, with the reason in *error, if it is missing or incomplete. */
bool loadObsTheme(const QString &folder, ObsTheme *theme, QString *error);

/* Applies a theme to the whole application, in OBS's order; widgets created afterwards look as in
 * OBS. Returns false, with the reason in *error, if a font or the style cannot be loaded. */
bool applyObsTheme(const ObsTheme &theme, QString *error);
