/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * plugin-main.c: what OBS calls when it loads and unloads the plugin.
 *
 * It creates the recorder, the controller and the dock, registers the hotkeys and the
 * frontend event callback (every callback is listed in docs/architecture.md, rule P10-9),
 * and keeps the settings in plugin_config/timelapse-switch/settings.json. Hotkeys run on
 * OBS's hotkey thread; the actions they trigger run on the UI thread, through the same
 * controller as the dock's buttons.
 */

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/platform.h>

#include "controller.h"
#include "core/timeline.h"
#include "plugin-support.h"
#include "recorder.h"
#include "ui/dock-obs.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")
OBS_MODULE_AUTHOR("Miguetronic")

/* How OBS lists the plugin (its plugin list and log). */
const char *obs_module_name(void)
{
	return "Timelapse Switch";
}

const char *obs_module_description(void)
{
	return obs_module_text("Module.Description");
}

enum {
	DEFAULT_TIMELAPSE_SPEED = 4,
	DEFAULT_STOP_HOLD_MS = 1000, /* stopping by hotkey needs a hold (UX-052) */
	SINGLE_KEYS = 8,
};

enum action {
	RECORD,
	STOP,
	TIMELAPSE,
	REAL_TIME,
	SPEED_2,
	SPEED_4,
	SPEED_8,
	SPEED_16,
	FASTER,
	SLOWER,
	CHAPTER,
	PAUSE,
	ACTIONS
};

/* Not const because OBS passes callback data as void *; never written. */
static enum action ACTION[ACTIONS] = {RECORD,  STOP,     TIMELAPSE, REAL_TIME, SPEED_2, SPEED_4,
				      SPEED_8, SPEED_16, FASTER,    SLOWER,    CHAPTER, PAUSE};
static const uint32_t KEY_SPEED[4] = {2, 4, 8, 16}; /* SPEED_2 to SPEED_16 */

/* Hotkeys that act on each press (the others come in pairs, see register_hotkeys). */
static const struct {
	const char *name;    /* OBS's id; never change it, OBS saves bindings under it */
	const char *text;    /* locale key of the name shown in Settings > Hotkeys */
	const char *setting; /* key of its binding in settings.json */
	enum action action;
} SINGLE_KEY[SINGLE_KEYS] = {
	{"TimelapseSwitch.Speed2", "Hotkey.Speed2", "speed_2", SPEED_2},
	{"TimelapseSwitch.Speed4", "Hotkey.Speed4", "speed_4", SPEED_4},
	{"TimelapseSwitch.Speed8", "Hotkey.Speed8", "speed_8", SPEED_8},
	{"TimelapseSwitch.Speed16", "Hotkey.Speed16", "speed_16", SPEED_16},
	{"TimelapseSwitch.Faster", "Hotkey.Faster", "faster", FASTER},
	{"TimelapseSwitch.Slower", "Hotkey.Slower", "slower", SLOWER},
	{"TimelapseSwitch.Chapter", "Hotkey.Chapter", "chapter", CHAPTER},
	{"TimelapseSwitch.Pause", "Hotkey.Pause", "pause", PAUSE},
};

/* The plugin's state: the only global object of the plugin (rule P10-6). */
static struct {
	struct tsw_recorder *recorder;
	struct tsw_controller *controller;
	obs_data_t *settings;
	uint64_t stop_hold_ns;
	uint64_t stop_pressed_ns; /* hotkey thread only */
	obs_hotkey_pair_id record_pair;
	obs_hotkey_pair_id timelapse_pair;
	obs_hotkey_id single_key[SINGLE_KEYS];
} plugin;

/* UI thread: what a hotkey asked for, through the controller like the dock's buttons (UX-053). */
static void run_action(void *param)
{
	const enum action action = *(const enum action *)param;
	switch (action) {
	case RECORD:
		tsw_controller_input(plugin.controller, TSW_INPUT_RECORD, 0);
		break;
	case STOP:
		tsw_controller_input(plugin.controller, TSW_INPUT_STOP, 0);
		break;
	case TIMELAPSE:
		tsw_controller_input(plugin.controller, TSW_INPUT_TIMELAPSE, 0);
		break;
	case REAL_TIME:
		tsw_controller_input(plugin.controller, TSW_INPUT_REAL_TIME, 0);
		break;
	case SPEED_2:
	case SPEED_4:
	case SPEED_8:
	case SPEED_16:
		tsw_controller_input(plugin.controller, TSW_INPUT_SPEED_KEY, KEY_SPEED[action - SPEED_2]);
		break;
	case FASTER:
		tsw_controller_input(plugin.controller, TSW_INPUT_FASTER, 0);
		break;
	case SLOWER:
		tsw_controller_input(plugin.controller, TSW_INPUT_SLOWER, 0);
		break;
	case CHAPTER:
		tsw_recorder_add_marker(plugin.recorder);
		break;
	case PAUSE:
		tsw_controller_input(plugin.controller, TSW_INPUT_PAUSE, 0);
		break;
	case ACTIONS:
		break;
	}
}

static void queue(enum action action)
{
	obs_queue_task(OBS_TASK_UI, run_action, &ACTION[action], false);
}

/* ---------------------------------------------------------------------------------------- */
/* Hotkey callbacks (hotkey thread). In a pair, returning false hands the key to the other   */
/* half, so one key can toggle (UX-050).                                                    */

static bool on_record(void *data, obs_hotkey_pair_id id, obs_hotkey_t *hotkey, bool pressed)
{
	(void)data;
	(void)id;
	(void)hotkey;
	if (!pressed || tsw_recorder_is_active(plugin.recorder))
		return false;
	queue(RECORD);
	return true;
}

static bool on_stop(void *data, obs_hotkey_pair_id id, obs_hotkey_t *hotkey, bool pressed)
{
	(void)data;
	(void)id;
	(void)hotkey;
	if (!tsw_recorder_is_active(plugin.recorder))
		return false;
	/* Stop needs a hold: act on release once the key was held long enough (or at once if hold is 0). */
	if (pressed) {
		plugin.stop_pressed_ns = os_gettime_ns();
		if (plugin.stop_hold_ns > 0)
			return true;
	} else if (plugin.stop_hold_ns == 0 || os_gettime_ns() - plugin.stop_pressed_ns < plugin.stop_hold_ns) {
		return true;
	}
	queue(STOP);
	return true;
}

static bool on_timelapse(void *data, obs_hotkey_pair_id id, obs_hotkey_t *hotkey, bool pressed)
{
	(void)data;
	(void)id;
	(void)hotkey;
	if (!pressed || tsw_controller_effective_speed(plugin.controller) > 1)
		return false; /* already in timelapse (or starting or resuming in it): the pair's other key */
	queue(TIMELAPSE);
	return true;
}

static bool on_real_time(void *data, obs_hotkey_pair_id id, obs_hotkey_t *hotkey, bool pressed)
{
	(void)data;
	(void)id;
	(void)hotkey;
	if (!pressed || tsw_controller_effective_speed(plugin.controller) == 1)
		return false; /* already in real time (or starting or resuming in it): the pair's other key */
	queue(REAL_TIME);
	return true;
}

static void on_single_key(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	(void)id;
	(void)hotkey;
	if (pressed)
		queue(*(const enum action *)data);
}

/* ---------------------------------------------------------------------------------------- */
/* Settings and hotkey bindings.                                                            */

static void register_hotkeys(void)
{
	plugin.record_pair = obs_hotkey_pair_register_frontend("TimelapseSwitch.Record",
							       obs_module_text("Hotkey.Record"), "TimelapseSwitch.Stop",
							       obs_module_text("Hotkey.Stop"), on_record, on_stop, NULL,
							       NULL);
	plugin.timelapse_pair = obs_hotkey_pair_register_frontend(
		"TimelapseSwitch.Timelapse", obs_module_text("Hotkey.Timelapse"), "TimelapseSwitch.RealTime",
		obs_module_text("Hotkey.RealTime"), on_timelapse, on_real_time, NULL, NULL);
	for (size_t i = 0; i < SINGLE_KEYS; i++)
		plugin.single_key[i] = obs_hotkey_register_frontend(SINGLE_KEY[i].name,
								    obs_module_text(SINGLE_KEY[i].text), on_single_key,
								    &ACTION[SINGLE_KEY[i].action]);
}

static void load_settings(void)
{
	char *path = obs_module_config_path("settings.json");
	plugin.settings = obs_data_create_from_json_file_safe(path, "bak");
	bfree(path);
	if (plugin.settings == NULL)
		plugin.settings = obs_data_create();
	obs_data_set_default_int(plugin.settings, "stop_hold_ms", DEFAULT_STOP_HOLD_MS);
	obs_data_set_default_int(plugin.settings, "timelapse_speed", DEFAULT_TIMELAPSE_SPEED);
	obs_data_set_default_bool(plugin.settings, "details_open", false); /* closed until opened (UX-135) */
	const long long hold_ms = obs_data_get_int(plugin.settings, "stop_hold_ms");
	plugin.stop_hold_ns = (hold_ms > 0 ? (uint64_t)hold_ms : 0) * 1000000;

	/* Hotkey bindings saved by save_settings (OBS does not save a plugin's frontend hotkeys). */
	obs_data_t *keys = obs_data_get_obj(plugin.settings, "hotkeys");
	obs_data_array_t *record = obs_data_get_array(keys, "record");
	obs_data_array_t *stop = obs_data_get_array(keys, "stop");
	obs_data_array_t *timelapse = obs_data_get_array(keys, "timelapse");
	obs_data_array_t *real_time = obs_data_get_array(keys, "real_time");
	obs_hotkey_pair_load(plugin.record_pair, record, stop);
	obs_hotkey_pair_load(plugin.timelapse_pair, timelapse, real_time);
	for (size_t i = 0; i < SINGLE_KEYS; i++) {
		obs_data_array_t *binding = obs_data_get_array(keys, SINGLE_KEY[i].setting);
		obs_hotkey_load(plugin.single_key[i], binding);
		obs_data_array_release(binding);
	}
	obs_data_array_release(record);
	obs_data_array_release(stop);
	obs_data_array_release(timelapse);
	obs_data_array_release(real_time);
	obs_data_release(keys);
}

static void save_settings(void)
{
	obs_data_t *keys = obs_data_create();
	obs_data_array_t *first = NULL;
	obs_data_array_t *second = NULL;
	obs_hotkey_pair_save(plugin.record_pair, &first, &second);
	obs_data_set_array(keys, "record", first);
	obs_data_set_array(keys, "stop", second);
	obs_data_array_release(first);
	obs_data_array_release(second);
	obs_hotkey_pair_save(plugin.timelapse_pair, &first, &second);
	obs_data_set_array(keys, "timelapse", first);
	obs_data_set_array(keys, "real_time", second);
	obs_data_array_release(first);
	obs_data_array_release(second);
	for (size_t i = 0; i < SINGLE_KEYS; i++) {
		obs_data_array_t *binding = obs_hotkey_save(plugin.single_key[i]);
		obs_data_set_array(keys, SINGLE_KEY[i].setting, binding);
		obs_data_array_release(binding);
	}
	obs_data_set_obj(plugin.settings, "hotkeys", keys);
	obs_data_release(keys);
	obs_data_set_int(plugin.settings, "timelapse_speed", tsw_controller_timelapse_speed(plugin.controller));
	bool details_open = false;
	if (tsw_dock_details_open(&details_open)) /* no dock: keep the choice saved before */
		obs_data_set_bool(plugin.settings, "details_open", details_open);

	char *folder = obs_module_config_path("");
	char *path = obs_module_config_path("settings.json");
	if (os_mkdirs(folder) == MKDIR_ERROR || !obs_data_save_json_safe(plugin.settings, path, "tmp", "bak"))
		tsw_log(LOG_WARNING, "TSW-E106 warning | settings not saved | file=\"%s\"", path);
	bfree(path);
	bfree(folder);
}

/* UI thread: OBS is closing. Save the file being recorded and the settings (UX-089), and remove the
 * dock while OBS's frontend still exists (it has already saved where the dock was). */
static void on_frontend_event(enum obs_frontend_event event, void *data)
{
	(void)data;
	if (event == OBS_FRONTEND_EVENT_EXIT) {
		tsw_recorder_close(plugin.recorder);
		save_settings();
		tsw_dock_destroy();
	}
}

bool obs_module_load(void)
{
	plugin.recorder = tsw_recorder_create();
	if (plugin.recorder == NULL) {
		tsw_log(LOG_ERROR, "TSW-E901 error | the recorder could not be created");
		return false;
	}
	register_hotkeys();
	load_settings();
	const long long speed = obs_data_get_int(plugin.settings, "timelapse_speed");
	plugin.controller = tsw_controller_create(
		plugin.recorder, (speed >= 2 && speed <= TSW_MAX_SPEED) ? (uint32_t)speed : DEFAULT_TIMELAPSE_SPEED);
	if (plugin.controller == NULL) {
		tsw_log(LOG_ERROR, "TSW-E901 error | the controller could not be created");
		return false;
	}
	tsw_dock_create(plugin.controller, obs_data_get_bool(plugin.settings, "details_open"));
	obs_frontend_add_event_callback(on_frontend_event, NULL);
	tsw_log(LOG_INFO, "plugin loaded (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(on_frontend_event, NULL);
	obs_hotkey_pair_unregister(plugin.record_pair);
	obs_hotkey_pair_unregister(plugin.timelapse_pair);
	for (size_t i = 0; i < SINGLE_KEYS; i++)
		obs_hotkey_unregister(plugin.single_key[i]);
	tsw_dock_destroy(); /* already gone if OBS closed normally */
	tsw_controller_destroy(plugin.controller);
	tsw_recorder_destroy(plugin.recorder);
	obs_data_release(plugin.settings);
	tsw_log(LOG_INFO, "plugin unloaded");
}
