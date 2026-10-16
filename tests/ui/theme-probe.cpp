/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * theme-probe.cpp: a small OBS module for the theme pictures only (tests/ui/README.md).
 * obs-themes.sh installs it in a sandbox OBS; it is never shipped.
 *
 * Think of it as a photographer's color chart: once OBS has finished loading, with its theme
 * applied, it writes down everything the dock's look depends on, so test-dock can dress the dock
 * in exactly the same clothes without OBS. It writes to the folder named by the environment
 * variable TSW_THEME_DUMP:
 *
 *   stylesheet.qss           the application style sheet OBS built from the theme files
 *   fonts/                   the fonts OBS carries inside its program (Open Sans)
 *   resources/               the pictures inside OBS that the style sheet points to (":/res/...")
 *   reference-<lang>-300.png the content of the plugin's own dock as this OBS draws it, idle, in
 *   reference-<lang>-220.png OBS's language, 300 and 220 px wide (only if the plugin is installed)
 *   reference-<lang>.json    those pictures' names and the fonts of that dock
 *   theme.json               the rest: theme id, dark or not, palette, font, search paths. Written
 *                            last and in one step: once it exists, the dump is complete and the
 *                            script may close OBS.
 *
 * The theme does not depend on the language, so a second run in another language rewrites the
 * same theme files and adds its own reference pictures.
 */

#include <QApplication>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileInfo>
#include <QFontInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMainWindow>
#include <QMetaEnum>
#include <QPalette>
#include <QPixmap>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScreen>
#include <QStyle>
#include <QTimer>
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/config-file.h>

OBS_DECLARE_MODULE()

namespace {

const char DUMP_VARIABLE[] = "TSW_THEME_DUMP";
const char PLUGIN_DOCK[] = "timelapse-switch"; /* the plugin's dock id (src/ui/dock-obs.cpp) */
const int REFERENCE_WIDTHS[] = {300, 220};     /* the widths of the UI test (tests/ui/test-dock.cpp) */
const int REFERENCE_HEIGHT = 640;              /* tall enough for the idle dock, with room below it */
const int MAX_STYLE_WRAPPERS = 4;              /* style sheet, OBS's proxy: two in practice */

bool writeFile(const QString &path, const QByteArray &bytes)
{
	QFile file(path);
	return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

/* Copies a file out of OBS's program (":/...") into the dump, with normal permissions. */
bool copyOut(const QString &from, const QDir &dump, const QString &to)
{
	QFile source(from);
	if (!source.open(QIODevice::ReadOnly))
		return false;
	return dump.mkpath(QFileInfo(to).path()) && writeFile(dump.filePath(to), source.readAll());
}

/* The real members of a Qt enum, by name: no aliases (Normal = Active), no counters, no NoRole. */
QList<QPair<QString, int>> members(const QMetaEnum &meta, int count, int skip)
{
	QList<QPair<QString, int>> found;
	QList<int> seen;
	for (int i = 0; i < meta.keyCount(); i++) {
		const int value = meta.value(i);
		if (value < 0 || value >= count || value == skip || seen.contains(value))
			continue;
		seen.append(value);
		found.append({QString::fromLatin1(meta.key(i)), value});
	}
	return found;
}

/* Every color of every group, by Qt's own names: {"Active": {"Window": "#ff2b2e38", ...}, ...}. */
QJsonObject paletteJson(const QPalette &palette)
{
	const auto groups = members(QMetaEnum::fromType<QPalette::ColorGroup>(), QPalette::NColorGroups, -1);
	const auto roles = members(QMetaEnum::fromType<QPalette::ColorRole>(), QPalette::NColorRoles, QPalette::NoRole);
	QJsonObject all;
	for (const auto &group : groups) {
		QJsonObject colors;
		for (const auto &role : roles) {
			const QColor color = palette.color(static_cast<QPalette::ColorGroup>(group.second),
							   static_cast<QPalette::ColorRole>(role.second));
			colors[role.first] = color.name(QColor::HexArgb);
		}
		all[group.first] = colors;
	}
	return all;
}

/* OBS's own fonts (inside its program, ":/fonts"), so test-dock can load the very same files. */
QJsonArray copyFonts(const QDir &dump)
{
	QJsonArray files;
	const QDir fonts(":/fonts");
	for (const QString &name : fonts.entryList(QDir::Files)) {
		const QString copy = "fonts/" + name;
		if (copyOut(fonts.filePath(name), dump, copy))
			files.append(copy);
	}
	return files;
}

/* The pictures inside OBS that the style sheet uses, such as url(:/res/images/down.svg): copied
 * under resources/, by the exact text the style sheet uses, so test-dock can point to the copies. */
QJsonObject copyResources(const QDir &dump, const QString &styleSheet)
{
	static const QRegularExpression url(R"(url\((:[^)]+)\))");
	QJsonObject copies;
	QRegularExpressionMatchIterator matches = url.globalMatch(styleSheet);
	while (matches.hasNext()) {
		const QString resource = matches.next().captured(1);
		QString copy = resource.mid(1);
		while (copy.startsWith('/'))
			copy.remove(0, 1);
		copy = "resources/" + copy;
		if (!copies.contains(resource) && copyOut(resource, dump, copy))
			copies[resource] = copy;
	}
	return copies;
}

/* Where "prefix:" paths of the style sheet are looked for; OBS sets "theme:" to its theme folders. */
QJsonObject searchPaths(const QString &styleSheet)
{
	static const QRegularExpression prefix(R"(url\(([A-Za-z][A-Za-z0-9]+):)");
	QStringList prefixes{"theme"};
	QRegularExpressionMatchIterator matches = prefix.globalMatch(styleSheet);
	while (matches.hasNext()) {
		const QString name = matches.next().captured(1);
		if (!prefixes.contains(name))
			prefixes.append(name);
	}
	QJsonObject paths;
	for (const QString &name : prefixes)
		paths[name] = QJsonArray::fromStringList(QDir::searchPaths(name));
	return paths;
}

/* The font a widget really draws with, after the style sheet: what test-dock must match. */
QJsonObject fontJson(const QFont &font)
{
	const QFontInfo info(font);
	return {{"family", info.family()},
		{"pointSize", info.pointSizeF()},
		{"pixelSize", info.pixelSize()},
		{"bold", info.bold()}};
}

/* Lets Qt lay out and redraw what changed; a narrow dock relays itself out once more. */
void settle()
{
	for (int pass = 0; pass < 3; pass++)
		QCoreApplication::processEvents();
}

/* The plugin's dock content as this OBS draws it, idle, in OBS's language, at each width (the
 * content exactly as wide as in test-dock), and the fonts it uses. OBS adds a plugin's dock hidden
 * and floating. Shown with WA_DontShowOnScreen, Qt lays it out and draws it like any visible
 * window, but the desktop never gets a window, so nothing appears on screen. Only the content is
 * kept: a floating dock's frame differs from a docked one's. Does nothing if the plugin is not
 * installed; returns false only if a file cannot be written. */
bool writeReference(const QDir &dump)
{
	auto *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	auto *dockWidget = main != nullptr ? main->findChild<QDockWidget *>(PLUGIN_DOCK) : nullptr;
	QWidget *area = dockWidget != nullptr ? dockWidget->widget() : nullptr;
	const QWidget *dock = area != nullptr ? area->findChild<QWidget *>("tswDock") : nullptr;
	const QWidget *mainButton = dock != nullptr ? dock->findChild<QWidget *>("tswMain") : nullptr;
	if (mainButton == nullptr)
		return true;
	const QString locale = QString::fromUtf8(obs_get_locale());
	dockWidget->setAttribute(Qt::WA_DontShowOnScreen);
	dockWidget->show();
	settle();
	const int frame = dockWidget->width() - area->width();
	QJsonObject facts{{"dockFont", fontJson(dock->font())}, {"mainButtonFont", fontJson(mainButton->font())}};
	bool saved = true;
	for (const int width : REFERENCE_WIDTHS) {
		dockWidget->resize(width + frame, REFERENCE_HEIGHT);
		settle();
		const QImage shot = area->grab().toImage();
		const QString name = QString("reference-%1-%2.png").arg(locale).arg(width);
		saved = saved && shot.width() == width && shot.save(dump.filePath(name));
		facts[QString::number(width)] = name;
	}
	dockWidget->hide();
	dockWidget->setAttribute(Qt::WA_DontShowOnScreen, false);
	return saved && writeFile(dump.filePath("reference-" + locale + ".json"), QJsonDocument(facts).toJson());
}

/* The Qt style under the wrappers: with a style sheet set, Qt wraps the application's style, and OBS
 * wraps Fusion in a proxy; each wrapper owns the style it wraps, as a child. */
QString styleName()
{
	const QStyle *style = QApplication::style();
	for (int depth = 0; depth < MAX_STYLE_WRAPPERS; depth++) {
		const auto *inner = style->findChild<const QStyle *>(QString(), Qt::FindDirectChildrenOnly);
		if (inner == nullptr)
			break;
		style = inner;
	}
	return style->name();
}

/* What test-dock needs besides the files: written last, in one step (QSaveFile). */
bool writeThemeJson(const QDir &dump, const QString &styleSheet)
{
	config_t *config = obs_frontend_get_user_config();
	const char *id = config_get_string(config, "Appearance", "Theme");
	const QScreen *screen = QGuiApplication::primaryScreen();
	QJsonObject theme{
		{"id", QString::fromUtf8(id != nullptr ? id : "")},
		{"dark", obs_frontend_is_theme_dark()},
		{"style", styleName()},
		{"font", QApplication::font().toString()},
		{"palette", paletteJson(QApplication::palette())},
		{"searchPaths", searchPaths(styleSheet)},
		{"fonts", copyFonts(dump)},
		{"resources", copyResources(dump, styleSheet)},
		{"fontScale", static_cast<qint64>(config_get_int(config, "Appearance", "FontScale"))},
		{"density", static_cast<qint64>(config_get_int(config, "Appearance", "Density"))},
		{"obsVersion", QString::fromUtf8(obs_get_version_string())},
		{"qtVersion", QString::fromLatin1(qVersion())},
		{"platform", QGuiApplication::platformName()},
		{"logicalDpi", screen != nullptr ? screen->logicalDotsPerInch() : 0.0},
		{"devicePixelRatio", screen != nullptr ? screen->devicePixelRatio() : 0.0},
	};
	QSaveFile file(dump.filePath("theme.json"));
	return file.open(QIODevice::WriteOnly) && file.write(QJsonDocument(theme).toJson()) >= 0 && file.commit();
}

bool writeDump(const QDir &dump)
{
	const QString styleSheet = qApp->styleSheet();
	return dump.mkpath(".") && writeFile(dump.filePath("stylesheet.qss"), styleSheet.toUtf8()) &&
	       writeReference(dump) && writeThemeJson(dump, styleSheet);
}

/* UI thread, from OBS's event loop. */
void dumpTheme()
{
	const QString folder = qEnvironmentVariable(DUMP_VARIABLE);
	if (writeDump(QDir(folder)))
		blog(LOG_INFO, "[tsw-theme-probe] theme written to %s", qPrintable(folder));
	else
		blog(LOG_WARNING, "[tsw-theme-probe] the theme could not be written to %s", qPrintable(folder));
}

/* UI thread. By the time OBS has finished loading, its theme is applied and the plugin's dock
 * exists (the plugin adds it while it loads). The dump waits until OBS is back in its event loop,
 * because the dock can only be laid out and drawn from that loop. */
void onFrontendEvent(enum obs_frontend_event event, void *data)
{
	(void)data;
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING)
		QTimer::singleShot(0, qApp, dumpTheme);
}

} // namespace

bool obs_module_load(void)
{
	if (qEnvironmentVariable(DUMP_VARIABLE).isEmpty()) {
		blog(LOG_WARNING, "[tsw-theme-probe] %s is not set: nothing to do", DUMP_VARIABLE);
		return false;
	}
	obs_frontend_add_event_callback(onFrontendEvent, nullptr);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(onFrontendEvent, nullptr);
}
