/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * plugin-main.c: what OBS calls when it loads and unloads the plugin.
 *
 * It creates the recorder, registers the hotkeys and the frontend event callback (every
 * callback is listed in docs/architecture.md, rule P10-9), and keeps the settings in
 * plugin_config/timelapse-switch/settings.json. Hotkeys run on OBS's hotkey thread; the
 * actions they trigger run on the UI thread.
 */

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/platform.h>

#include "core/timeline.h"
#include "plugin-support.h"
#include "recorder.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

enum {
	DEFAULT_TIMELAPSE_SPEED = 4,
	DEFAULT_STOP_HOLD_MS = 1000, /* stopping by hotkey needs a hold (UX-052) */
	SPEED_KEYS = 4,
};

enum action { RECORD, STOP, TIMELAPSE, REAL_TIME, SPEED_2, SPEED_4, SPEED_8, SPEED_16, ACTIONS };

/* Not const because OBS passes callback data as void *; never written. */
static enum action ACTION[ACTIONS] = {RECORD, STOP, TIMELAPSE, REAL_TIME, SPEED_2, SPEED_4, SPEED_8, SPEED_16};
static const uint32_t KEY_SPEED[SPEED_KEYS] = {2, 4, 8, 16};
static const char *const KEY_SETTING[SPEED_KEYS] = {"speed_2", "speed_4", "speed_8", "speed_16"};

/* The plugin's state: the only global object of the plugin (rule P10-6). */
static struct {
	struct tsw_recorder *recorder;
	obs_data_t *settings;
	uint32_t timelapse_speed; /* speed used by "Switch to timelapse"; UI thread */
	uint64_t stop_hold_ns;
	uint64_t stop_pressed_ns; /* hotkey thread only */
	obs_hotkey_pair_id record_pair;
	obs_hotkey_pair_id timelapse_pair;
	obs_hotkey_id speed_key[SPEED_KEYS];
} plugin;

/* UI thread: what a hotkey asked for. Failures are already logged by the recorder. */
static void run_action(void *param)
{
	const enum action action = *(const enum action *)param;
	enum tsw_result result = TSW_OK;
	if (action == RECORD) {
		result = tsw_recorder_start(plugin.recorder, 1);
	} else if (action == STOP) {
		tsw_recorder_stop(plugin.recorder);
	} else if (action == TIMELAPSE) {
		result = tsw_recorder_set_speed(plugin.recorder, plugin.timelapse_speed);
	} else if (action == REAL_TIME) {
		result = tsw_recorder_set_speed(plugin.recorder, 1);
	} else {
		/* Speed keys choose the timelapse speed and, while recording, switch to it (UX spec 3.11). */
		plugin.timelapse_speed = KEY_SPEED[action - SPEED_2];
		result = tsw_recorder_set_speed(plugin.recorder, plugin.timelapse_speed);
	}
	if (result != TSW_OK)
		tsw_log(LOG_DEBUG, "action %d not applied (result %d)", (int)action, (int)result);
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
	if (!pressed || tsw_recorder_speed(plugin.recorder) > 1)
		return false;
	queue(TIMELAPSE);
	return true;
}

static bool on_real_time(void *data, obs_hotkey_pair_id id, obs_hotkey_t *hotkey, bool pressed)
{
	(void)data;
	(void)id;
	(void)hotkey;
	if (!pressed || tsw_recorder_speed(plugin.recorder) == 1)
		return false;
	queue(REAL_TIME);
	return true;
}

static void on_speed_key(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
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
	static const char *const NAMES[SPEED_KEYS] = {"TimelapseSwitch.Speed2", "TimelapseSwitch.Speed4",
						      "TimelapseSwitch.Speed8", "TimelapseSwitch.Speed16"};
	static const char *const TEXTS[SPEED_KEYS] = {"Hotkey.Speed2", "Hotkey.Speed4", "Hotkey.Speed8",
						      "Hotkey.Speed16"};
	for (size_t i = 0; i < SPEED_KEYS; i++)
		plugin.speed_key[i] = obs_hotkey_register_frontend(NAMES[i], obs_module_text(TEXTS[i]), on_speed_key,
								   &ACTION[SPEED_2 + i]);
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
	const long long hold_ms = obs_data_get_int(plugin.settings, "stop_hold_ms");
	const long long speed = obs_data_get_int(plugin.settings, "timelapse_speed");
	plugin.stop_hold_ns = (hold_ms > 0 ? (uint64_t)hold_ms : 0) * 1000000;
	plugin.timelapse_speed = (speed >= 2 && speed <= TSW_MAX_SPEED) ? (uint32_t)speed : DEFAULT_TIMELAPSE_SPEED;

	/* Hotkey bindings saved by save_settings (OBS does not save a plugin's frontend hotkeys). */
	obs_data_t *keys = obs_data_get_obj(plugin.settings, "hotkeys");
	obs_data_array_t *record = obs_data_get_array(keys, "record");
	obs_data_array_t *stop = obs_data_get_array(keys, "stop");
	obs_data_array_t *timelapse = obs_data_get_array(keys, "timelapse");
	obs_data_array_t *real_time = obs_data_get_array(keys, "real_time");
	obs_hotkey_pair_load(plugin.record_pair, record, stop);
	obs_hotkey_pair_load(plugin.timelapse_pair, timelapse, real_time);
	for (size_t i = 0; i < SPEED_KEYS; i++) {
		obs_data_array_t *binding = obs_data_get_array(keys, KEY_SETTING[i]);
		obs_hotkey_load(plugin.speed_key[i], binding);
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
	for (size_t i = 0; i < SPEED_KEYS; i++) {
		obs_data_array_t *binding = obs_hotkey_save(plugin.speed_key[i]);
		obs_data_set_array(keys, KEY_SETTING[i], binding);
		obs_data_array_release(binding);
	}
	obs_data_set_obj(plugin.settings, "hotkeys", keys);
	obs_data_release(keys);

	char *folder = obs_module_config_path("");
	char *path = obs_module_config_path("settings.json");
	if (os_mkdirs(folder) == MKDIR_ERROR || !obs_data_save_json_safe(plugin.settings, path, "tmp", "bak"))
		tsw_log(LOG_WARNING, "TSW-E106 warning | settings not saved | file=\"%s\"", path);
	bfree(path);
	bfree(folder);
}

/* UI thread: OBS is closing. Save the file being recorded and the hotkey bindings (UX-089). */
static void on_frontend_event(enum obs_frontend_event event, void *data)
{
	(void)data;
	if (event == OBS_FRONTEND_EVENT_EXIT) {
		tsw_recorder_close(plugin.recorder);
		save_settings();
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
	obs_frontend_add_event_callback(on_frontend_event, NULL);
	tsw_log(LOG_INFO, "plugin loaded (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(on_frontend_event, NULL);
	obs_hotkey_pair_unregister(plugin.record_pair);
	obs_hotkey_pair_unregister(plugin.timelapse_pair);
	for (size_t i = 0; i < SPEED_KEYS; i++)
		obs_hotkey_unregister(plugin.speed_key[i]);
	tsw_recorder_destroy(plugin.recorder);
	obs_data_release(plugin.settings);
	tsw_log(LOG_INFO, "plugin unloaded");
}
