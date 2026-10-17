/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * obs-theme.cpp: see obs-theme.hpp. The dump's format is written by theme-probe.cpp.
 */

#include "obs-theme.hpp"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMetaEnum>
#include <QSet>
#include <QStyleFactory>

OBSBasic::OBSBasic()
{
	setObjectName("OBSBasic");
}

OBSDock::OBSDock(QWidget *parent) : QDockWidget(parent) {}

namespace {

bool readFile(const QString &path, QByteArray *bytes)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
		return false;
	*bytes = file.readAll();
	return true;
}

/* {"Active": {"Window": "#ff2b2e38", ...}, ...}: true only if every group has every role. */
bool readPalette(const QJsonObject &json, QPalette *palette)
{
	const QMetaEnum groups = QMetaEnum::fromType<QPalette::ColorGroup>();
	const QMetaEnum roles = QMetaEnum::fromType<QPalette::ColorRole>();
	int colors = 0;
	for (auto group = json.begin(); group != json.end(); ++group) {
		bool groupKnown = false;
		const int g = groups.keyToValue(group.key().toLatin1().constData(), &groupKnown);
		const QJsonObject byRole = group.value().toObject();
		for (auto role = byRole.begin(); role != byRole.end(); ++role) {
			bool roleKnown = false;
			const int r = roles.keyToValue(role.key().toLatin1().constData(), &roleKnown);
			const QColor color = QColor::fromString(role.value().toString());
			if (!groupKnown || !roleKnown || !color.isValid())
				return false;
			palette->setColor(static_cast<QPalette::ColorGroup>(g), static_cast<QPalette::ColorRole>(r),
					  color);
			colors++;
		}
	}
	return colors == QPalette::NColorGroups * (QPalette::NColorRoles - 1); /* all but NoRole */
}

/* url(:/res/images/down.svg) lives inside OBS's program; the dump has a copy of each one. */
QString pointToCopies(QString styleSheet, const QJsonObject &resources, const QDir &dump)
{
	for (auto resource = resources.begin(); resource != resources.end(); ++resource) {
		const QString copy = dump.absoluteFilePath(resource.value().toString());
		styleSheet.replace("url(" + resource.key() + ")", "url(\"" + copy + "\")");
	}
	return styleSheet;
}

} // namespace

bool loadObsTheme(const QString &folder, ObsTheme *theme, QString *error)
{
	const QDir dump(folder);
	QByteArray json;
	QByteArray styleSheet;
	if (!readFile(dump.filePath("theme.json"), &json) || !readFile(dump.filePath("stylesheet.qss"), &styleSheet)) {
		*error = folder + ": no theme.json or stylesheet.qss (run tests/ui/obs-themes.sh)";
		return false;
	}
	const QJsonObject root = QJsonDocument::fromJson(json).object();
	theme->folder = folder;
	theme->name = dump.dirName();
	theme->id = root["id"].toString();
	theme->dark = root["dark"].toBool();
	theme->style = root["style"].toString();
	theme->logicalDpi = root["logicalDpi"].toDouble();
	theme->styleSheet = pointToCopies(QString::fromUtf8(styleSheet), root["resources"].toObject(), dump);
	const QJsonObject paths = root["searchPaths"].toObject();
	for (auto path = paths.begin(); path != paths.end(); ++path) {
		for (const QJsonValue &place : path.value().toArray())
			theme->searchPaths[path.key()].append(place.toString());
	}
	const QStringList references = dump.entryList({"reference-*.json"}, QDir::Files);
	for (const QString &name : references) {
		QByteArray facts;
		if (readFile(dump.filePath(name), &facts)) /* reference-en-US.json --> en-US */
			theme->references[name.mid(10, name.size() - 15)] = QJsonDocument::fromJson(facts).object();
	}
	for (const QJsonValue &file : root["fonts"].toArray())
		theme->fontFiles.append(dump.absoluteFilePath(file.toString()));
	if (!theme->font.fromString(root["font"].toString()) ||
	    !readPalette(root["palette"].toObject(), &theme->palette)) {
		*error = folder + ": theme.json has no valid font or palette";
		return false;
	}
	return true;
}

bool applyObsTheme(const ObsTheme &theme, QString *error)
{
	/* 1. OBS's own fonts (obs-main.cpp loads them before anything else); once per file. */
	static QSet<QString> loaded;
	for (const QString &file : theme.fontFiles) {
		if (!loaded.contains(file) && QFontDatabase::addApplicationFont(file) < 0) {
			*error = theme.name + ": cannot load the font " + file;
			return false;
		}
		loaded.insert(file);
	}
	/* 2. Where "theme:Dark/close.svg" and the like are found (InitTheme). */
	for (auto path = theme.searchPaths.begin(); path != theme.searchPaths.end(); ++path)
		QDir::setSearchPaths(path.key(), path.value());
	/* 3. The style (InitTheme: Fusion inside a proxy that only changes how combo boxes scroll). */
	qApp->setStyleSheet(QString());
	QStyle *style = QStyleFactory::create(theme.style);
	if (style == nullptr) {
		*error = theme.name + ": no Qt style " + theme.style;
		return false;
	}
	QApplication::setStyle(style);
	/* 4. The desktop's font, which Qt gives OBS before OBS starts. */
	QApplication::setFont(theme.font);
	/* 5. Palette, then style sheet, as SetTheme does. */
	QApplication::setPalette(theme.palette);
	qApp->setStyleSheet(theme.styleSheet);
	return true;
}
