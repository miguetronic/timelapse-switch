/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * recorder.c: see recorder.h.
 *
 * Settings from the OBS profile, then start, stop and the speed switch. What happens to
 * every frame and audio block is in recorder-rt.c.
 */

#include "recorder.h"

#include <string.h>

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/config-file.h>
#include <util/dstr.h>
#include <util/platform.h>

#include "core/check.h"
#include "fault.h"
#include "plugin-support.h"
#include "recorder-internal.h"

enum {
	VIDEO_CACHE_FRAMES = 16, /* frames our video feed holds while the encoder catches up */
	SAVE_TIMEOUT_MS = 10000, /* longest wait for the file to be finalized when OBS closes */
};

/* Stopping: wait for the encoder to hand over the frames it still holds, but never forever. */
static const uint64_t FLUSH_STALL_NS = 1000000000; /* no progress for 1 s: the encoder holds nothing more */
static const uint64_t FLUSH_LIMIT_NS = 5000000000; /* stop after 5 s whatever happens */

static const uint64_t SECOND_NS = 1000000000;

/* The recorder that exists, so a task queued before it was freed can tell (see finish). */
static struct tsw_recorder *live_recorder;

/* ---------------------------------------------------------------------------------------- */
/* Settings from the OBS profile.                                                            */

/* The CPU-frame variant of a GPU encoder: our feed gives frames in memory (ADR-003). */
static const char *cpu_frame_encoder(const char *id)
{
	/* shortcut: maps the "_tex" naming of the VAAPI encoders (Linux) only; add the NVENC,
	 * AMF and QSV names when the plugin is tested on Windows. */
	static char cpu_id[128];
	const size_t length = strlen(id);
	if (length > 4 && length < sizeof(cpu_id) && strcmp(id + length - 4, "_tex") == 0) {
		memcpy(cpu_id, id, length - 4);
		cpu_id[length - 4] = '\0';
		if (obs_encoder_get_display_name(cpu_id) != NULL)
			return cpu_id;
	}
	return id;
}

/* Recording format chosen in the profile (Settings > Output > Recording). */
static const char *profile_format(void)
{
	config_t *profile = obs_frontend_get_profile_config();
	const char *mode = config_get_string(profile, "Output", "Mode");
	const bool advanced = mode != NULL && strcmp(mode, "Advanced") == 0;
	const char *format = config_get_string(profile, advanced ? "AdvOut" : "SimpleOutput", "RecFormat2");
	return (format != NULL && *format != '\0') ? format : "hybrid_mp4";
}

/* A new file name in the profile's recording folder, ending in " tswitch" (UX spec, section 3). */
static char *new_recording_path(const char *extension)
{
	config_t *profile = obs_frontend_get_profile_config();
	const char *pattern = config_get_string(profile, "Output", "FilenameFormatting");
	struct dstr format = {0};
	dstr_printf(&format, "%s tswitch",
		    (pattern != NULL && *pattern != '\0') ? pattern : "%CCYY-%MM-%DD %hh-%mm-%ss");
	char *name = os_generate_formatted_filename(extension, true, format.array);
	char *folder = obs_frontend_get_current_record_output_path();
	struct dstr path = {0};
	dstr_printf(&path, "%s/%s", folder, name);
	if (os_mkdirs(folder) == MKDIR_ERROR)
		tsw_log(LOG_WARNING, "TSW-E003 warning | cannot create the folder | folder=\"%s\"", folder);
	bfree(folder);
	bfree(name);
	dstr_free(&format);
	return path.array;
}

/* ---------------------------------------------------------------------------------------- */
/* Start, stop, switch.                                                                     */

/* Opens our video and audio feeds, with the same layout as OBS's main picture and sound. */
static enum tsw_result open_feeds(struct tsw_recorder *recorder, const struct obs_video_info *ovi,
				  const struct obs_audio_info *oai)
{
	struct video_output_info video = {
		.name = "timelapse-switch video",
		.format = ovi->output_format,
		.fps_num = ovi->fps_num,
		.fps_den = ovi->fps_den,
		.width = ovi->output_width,
		.height = ovi->output_height,
		.cache_size = VIDEO_CACHE_FRAMES,
		.colorspace = ovi->colorspace,
		.range = ovi->range,
	};
	struct audio_output_info audio = {
		.name = "timelapse-switch audio",
		.samples_per_sec = oai->samples_per_sec,
		.format = AUDIO_FORMAT_FLOAT_PLANAR,
		.speakers = oai->speakers,
		.input_callback = tsw_recorder_provide_audio,
		.input_param = recorder,
	};
	const uint32_t channels = get_audio_channels(oai->speakers);
	recorder->ring_storage = bzalloc(tsw_audio_ring_storage_floats(channels, AUDIO_OUTPUT_FRAMES) * sizeof(float));
	if (tsw_audio_ring_init(&recorder->ring, recorder->ring_storage, channels, AUDIO_OUTPUT_FRAMES) != TSW_OK)
		return TSW_E_INVALID;
	if (video_output_open(&recorder->video, &video) != VIDEO_OUTPUT_SUCCESS)
		return TSW_E_START_FAILED;
	if (audio_output_open(&recorder->audio, &audio) != AUDIO_OUTPUT_SUCCESS)
		return TSW_E_START_FAILED;
	recorder->format = ovi->output_format;
	recorder->width = ovi->output_width;
	recorder->height = ovi->output_height;
	recorder->block_ns = AUDIO_OUTPUT_FRAMES * SECOND_NS / oai->samples_per_sec;
	return TSW_OK;
}

/* A JSON file of the current profile folder (OBS keeps encoder settings there), or empty settings. */
static obs_data_t *profile_json(const char *file)
{
	char *folder = obs_frontend_get_current_profile_path();
	struct dstr path = {0};
	dstr_printf(&path, "%s/%s", folder, file);
	obs_data_t *data = obs_data_create_from_json_file_safe(path.array, "bak");
	dstr_free(&path);
	bfree(folder);
	return data != NULL ? data : obs_data_create();
}

/* The recording encoders of the profile, read where OBS stores them (Settings > Output > Recording). */
struct encoder_choice {
	const char *video_id;
	obs_data_t *video_settings;
	const char *audio_id;
	obs_data_t *audio_settings;
	size_t mix;
};

static struct encoder_choice choose_encoders(void)
{
	config_t *profile = obs_frontend_get_profile_config();
	const char *mode = config_get_string(profile, "Output", "Mode");
	struct encoder_choice choice = {"obs_x264", obs_data_create(), "ffmpeg_aac", obs_data_create(), 0};
	if (mode == NULL || strcmp(mode, "Advanced") != 0) {
		/* shortcut: Simple mode's quality presets do not map 1:1 to an encoder; x264 + AAC until a user needs more. */
		obs_data_set_string(choice.video_settings, "rate_control", "CRF");
		obs_data_set_int(choice.video_settings, "crf", 20);
		obs_data_set_int(choice.audio_settings, "bitrate", 192);
		tsw_log(LOG_INFO, "TSW-E005 info | Simple output mode: recording with x264 and AAC instead");
		return choice;
	}

	/* Video: the recording encoder, or the streaming one when the recording uses "(Use stream encoder)". */
	const char *video = config_get_string(profile, "AdvOut", "RecEncoder");
	const bool same_as_stream = video == NULL || *video == '\0' || strcmp(video, "none") == 0;
	obs_data_release(choice.video_settings);
	choice.video_id = same_as_stream ? config_get_string(profile, "AdvOut", "Encoder") : video;
	choice.video_settings = profile_json(same_as_stream ? "streamEncoder.json" : "recordEncoder.json");

	/* Audio: the recording audio encoder, on the first track selected for recording. */
	const char *audio = config_get_string(profile, "AdvOut", "RecAudioEncoder");
	if (audio != NULL && *audio != '\0')
		choice.audio_id = audio;
	const uint64_t tracks = config_get_uint(profile, "AdvOut", "RecTracks");
	for (size_t track = 0; track < MAX_AUDIO_MIXES; track++) {
		if ((tracks >> track) & 1u) {
			choice.mix = track;
			break;
		}
	}
	static const char *const BITRATE_KEY[] = {"Track1Bitrate", "Track2Bitrate", "Track3Bitrate",
						  "Track4Bitrate", "Track5Bitrate", "Track6Bitrate"};
	_Static_assert(sizeof(BITRATE_KEY) / sizeof(BITRATE_KEY[0]) == MAX_AUDIO_MIXES, "one key per OBS audio track");
	const uint64_t bitrate = config_get_uint(profile, "AdvOut", BITRATE_KEY[choice.mix]);
	obs_data_set_int(choice.audio_settings, "bitrate", bitrate > 0 ? (long long)bitrate : 160);
	return choice;
}

/* Our encoders: the types and settings of OBS's recording, fed by our feeds. */
static enum tsw_result create_encoders(struct tsw_recorder *recorder)
{
	struct encoder_choice choice = choose_encoders();
	const char *video_id = choice.video_id != NULL ? cpu_frame_encoder(choice.video_id) : "";
	recorder->mix = choice.mix;
	if (obs_encoder_get_display_name(video_id) != NULL && obs_encoder_get_display_name(choice.audio_id) != NULL) {
		recorder->video_encoder =
			obs_video_encoder_create(video_id, "timelapse-switch video", choice.video_settings, NULL);
		recorder->audio_encoder = obs_audio_encoder_create(choice.audio_id, "timelapse-switch audio",
								   choice.audio_settings, 0, NULL);
	}
	obs_data_release(choice.video_settings);
	obs_data_release(choice.audio_settings);
	if (recorder->video_encoder == NULL || recorder->audio_encoder == NULL) {
		tsw_log(LOG_ERROR, "TSW-E002 error | the recording encoder is not available | video=%s audio=%s",
			video_id, choice.audio_id);
		return TSW_E_ENCODER_UNAVAILABLE;
	}
	obs_encoder_set_video(recorder->video_encoder, recorder->video);
	obs_encoder_set_audio(recorder->audio_encoder, recorder->audio);
	tsw_log(LOG_INFO, "encoders | video=%s audio=%s track=%zu", video_id, choice.audio_id, recorder->mix + 1);
	return TSW_OK;
}

/* Stops taking frames and audio in. The removal waits for a callback in progress. */
static void stop_capture(struct tsw_recorder *recorder)
{
	if (!os_atomic_set_bool(&recorder->capturing, false))
		return; /* was not capturing */
	obs_remove_raw_video_callback(tsw_recorder_receive_frame, recorder);
	obs_remove_raw_audio_callback(recorder->mix, tsw_recorder_receive_audio, recorder);
}

/* Releases everything start created, in reverse order. Safe on a half-started recorder. */
static void release_media(struct tsw_recorder *recorder)
{
	stop_capture(recorder);
	obs_output_release(recorder->output);
	obs_encoder_release(recorder->video_encoder);
	obs_encoder_release(recorder->audio_encoder);
	if (recorder->audio != NULL)
		audio_output_close(recorder->audio);
	if (recorder->video != NULL)
		video_output_close(recorder->video);
	bfree(recorder->ring_storage);
	bfree(recorder->path);
	recorder->output = NULL;
	recorder->video_encoder = NULL;
	recorder->audio_encoder = NULL;
	recorder->audio = NULL;
	recorder->video = NULL;
	recorder->ring_storage = NULL;
	recorder->path = NULL;
	os_atomic_set_long(&recorder->speed, 1);
	os_atomic_set_bool(&recorder->paused, false);
	recorder->stopping = false;
	os_atomic_set_bool(&recorder->active, false);
}

/* Graphics thread, once per frame while stopping: stop the file writer once it has every frame
 * sent before the stop, or when the encoder makes no more progress. */
static void flush_tick(void *param, float seconds)
{
	struct tsw_recorder *recorder = param;
	(void)seconds;
	if (!os_atomic_load_bool(&recorder->flushing) || os_atomic_load_bool(&recorder->forced))
		return;
	const long written = (long)obs_output_get_total_frames(recorder->output);
	const uint64_t now = os_gettime_ns();
	if (written != recorder->flush_seen) {
		recorder->flush_seen = written;
		recorder->flush_seen_ns = now;
	}
	const bool complete = written >= recorder->flush_target;
	const bool stalled = now - recorder->flush_seen_ns >= FLUSH_STALL_NS;
	const bool too_long = now - recorder->flush_start_ns >= FLUSH_LIMIT_NS;
	if (complete || stalled || too_long) {
		os_atomic_set_bool(&recorder->forced, true);
		obs_output_force_stop(recorder->output); /* stops at the next packet */
	}
}

/* How the last recording ended, for the dock (tsw_recorder_get_status). Takes ownership of path. */
static void record_outcome(struct tsw_recorder *recorder, uint32_t error_code, uint64_t recorded_ns, uint64_t final_ns,
			   char *path)
{
	recorder->outcome++;
	recorder->error_code = error_code;
	recorder->last_recorded_ns = recorded_ns;
	recorder->last_final_ns = final_ns;
	bfree(recorder->last_path);
	recorder->last_path = path;
}

/* UI thread, after the output stopped: report, then release. */
static void finish(void *param)
{
	if (param != live_recorder)
		return; /* the recorder was freed after this task was queued (OBS closing) */
	struct tsw_recorder *recorder = param;
	if (!os_atomic_load_bool(&recorder->active))
		return; /* already finished: tsw_recorder_close waited for it */
	const char *error = obs_output_get_last_error(recorder->output);
	const bool failed = error != NULL && *error != '\0';
	if (failed)
		tsw_log(LOG_ERROR, "TSW-E103 error | the file writer reported a problem | cause=\"%s\"", error);
	tsw_log(LOG_INFO, "recording saved: %s", recorder->path);
	tsw_fault_log_new();
	obs_remove_tick_callback(flush_tick, recorder);
	os_atomic_set_bool(&recorder->flushing, false);

	/* What was saved: up to the stop, or up to now if the file writer stopped by itself. */
	const uint64_t end_ns = recorder->stop_ns != 0 ? recorder->stop_ns : os_gettime_ns();
	const size_t sections = (size_t)os_atomic_load_long(&recorder->sections);
	const uint64_t final_ns = sections > 0 ? tsw_timeline_output_ns(&recorder->timeline, sections, end_ns) : 0;
	const uint64_t recorded_ns = sections > 0 ? tsw_timeline_recorded_ns(&recorder->timeline, sections, end_ns) : 0;
	char *path = recorder->path;
	recorder->path = NULL;
	release_media(recorder);
	record_outcome(recorder, failed ? 103 : 0, recorded_ns, final_ns, path);
}

/* Output thread, when the file writer stopped. */
static void output_stopped(void *param, calldata_t *data)
{
	struct tsw_recorder *recorder = param;
	(void)data;
	os_event_signal(recorder->saved);
	obs_queue_task(OBS_TASK_UI, finish, recorder, false);
}

/* The file writer of the profile's format, with our encoders. */
static enum tsw_result create_output(struct tsw_recorder *recorder)
{
	const char *format = profile_format();
	const bool hybrid = strcmp(format, "hybrid_mp4") == 0;
	recorder->path = new_recording_path(hybrid ? "mp4" : format);
	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "path", recorder->path);
	recorder->output =
		obs_output_create(hybrid ? "mp4_output" : "ffmpeg_muxer", "timelapse-switch output", settings, NULL);
	obs_data_release(settings);
	if (recorder->output == NULL)
		return TSW_E_START_FAILED;
	obs_output_set_video_encoder(recorder->output, recorder->video_encoder);
	obs_output_set_audio_encoder(recorder->output, recorder->audio_encoder, 0);
	signal_handler_connect(obs_output_get_signal_handler(recorder->output), "stop", output_stopped, recorder);
	return TSW_OK;
}

/* Hybrid MP4 only: a chapter in the UI language (REQ-007, UX-044, UX-054); "%1" in its name becomes number.
 * OBS places it on the first frame drawn after this call. */
static void add_chapter(struct tsw_recorder *recorder, const char *name_key, uint32_t number)
{
	struct dstr name = {0};
	struct dstr digits = {0};
	dstr_copy(&name, obs_module_text(name_key));
	dstr_printf(&digits, "%u", number);
	dstr_replace(&name, "%1", digits.array);
	dstr_free(&digits);
	calldata_t data;
	calldata_init(&data);
	calldata_set_string(&data, "chapter_name", name.array);
	const bool added = proc_handler_call(obs_output_get_proc_handler(recorder->output), "add_chapter", &data);
	calldata_free(&data);
	dstr_free(&name);
	if (!added && !recorder->chapters_unavailable) {
		recorder->chapters_unavailable = true;
		tsw_log(LOG_INFO, "TSW-E006 info | chapters are not available in this format | format=%s",
			profile_format());
	}
}

/* The TSW-Exxx number shown for a failed start. */
static uint32_t start_error_number(enum tsw_result result)
{
	if (result == TSW_E_START_FAILED)
		return 1;
	if (result == TSW_E_ENCODER_UNAVAILABLE)
		return 2;
	return 901;
}

/* First section of the timeline, gates at zero, counters at zero. */
static enum tsw_result reset_timeline(struct tsw_recorder *recorder, const struct obs_video_info *ovi,
				      const struct obs_audio_info *oai, uint32_t speed)
{
	recorder->start_ns = os_gettime_ns();
	recorder->stop_ns = 0;
	recorder->chapters_unavailable = false;
	if (tsw_timeline_switch(&recorder->timeline, 0, recorder->start_ns, speed) != TSW_OK)
		return TSW_E_INVALID;
	os_atomic_set_long(&recorder->sections, 1);
	os_atomic_set_long(&recorder->speed, (long)speed);
	if (tsw_video_gate_init(&recorder->video_gate, ovi->fps_num, ovi->fps_den) != TSW_OK ||
	    tsw_audio_gate_init(&recorder->audio_gate, oai->samples_per_sec) != TSW_OK)
		return TSW_E_INVALID;
	recorder->frames_in = recorder->frames_sent = recorder->frames_lost = 0;
	recorder->blocks_sent = recorder->ring_full = recorder->ring_empty = 0;
	recorder->feed_peak = 0;
	recorder->input_repeats = recorder->gate_repeats = recorder->gate_skips = 0;
	recorder->last_fingerprint = 0;
	tsw_disk_meter_reset(&recorder->disk);
	recorder->disk_checked_ns = 0;
	recorder->disk_free_bytes = 0;
	recorder->file_bytes = 0;
	recorder->first_frame_ns = recorder->first_block_ns = recorder->frame_delay_ns = recorder->block_delay_ns = 0;
	return TSW_OK;
}

enum tsw_result tsw_recorder_start(struct tsw_recorder *recorder, uint32_t speed)
{
	if (!TSW_CHECK(recorder != NULL && speed >= 1 && speed <= TSW_MAX_SPEED))
		return TSW_E_INVALID;
	if (os_atomic_load_bool(&recorder->active))
		return TSW_OK;

	/* 1. Same picture and sound layout as OBS; our feeds, encoders and file writer. */
	struct obs_video_info ovi;
	struct obs_audio_info oai;
	if (!obs_get_video_info(&ovi) || !obs_get_audio_info(&oai))
		return TSW_E_START_FAILED;
	os_atomic_set_bool(&recorder->active, true);
	os_event_reset(recorder->saved);
	enum tsw_result result = open_feeds(recorder, &ovi, &oai);
	if (result == TSW_OK)
		result = create_encoders(recorder);
	if (result == TSW_OK)
		result = create_output(recorder);

	/* 2. Start writing. Starting the encoders takes tens of milliseconds, so the timeline starts only
	 *    afterwards, right before frames and audio come in: with an earlier start the video gate would
	 *    fill the gap by repeating the first frame, and the video would trail the audio. */
	if (result == TSW_OK && !obs_output_start(recorder->output)) {
		const char *error = obs_output_get_last_error(recorder->output);
		tsw_log(LOG_ERROR, "TSW-E001 error | the recording did not start | cause=\"%s\"", error ? error : "");
		result = TSW_E_START_FAILED;
	}
	if (result == TSW_OK)
		result = reset_timeline(recorder, &ovi, &oai, speed);
	if (result != TSW_OK) {
		release_media(recorder);
		record_outcome(recorder, start_error_number(result), 0, 0, NULL);
		return result;
	}
	recorder->markers = 0;
	add_chapter(recorder, speed > 1 ? "Chapter.Timelapse" : "Chapter.RealTime", speed); /* before any frame: 0:00 */
	struct video_scale_info frames = {ovi.output_format, ovi.output_width, ovi.output_height, ovi.range,
					  ovi.colorspace};
	obs_add_raw_video_callback(&frames, tsw_recorder_receive_frame, recorder);
	obs_add_raw_audio_callback(recorder->mix, NULL, tsw_recorder_receive_audio, recorder);
	os_atomic_set_bool(&recorder->capturing, true);
	tsw_log(LOG_INFO, "recording started, speed x%u | file=\"%s\" | %ux%u at %u/%u fps", speed, recorder->path,
		ovi.output_width, ovi.output_height, ovi.fps_num, ovi.fps_den);
	return TSW_OK;
}

/* The most frames that waited in the feed, at most its size: while frames are being lost the raw count also
 * includes the repeats OBS owes, which says nothing more than "full". */
static long feed_peak(struct tsw_recorder *recorder)
{
	const long peak = os_atomic_load_long(&recorder->feed_peak);
	return peak < VIDEO_CACHE_FRAMES ? peak : VIDEO_CACHE_FRAMES;
}

/* Where the time of the recording went, up to the stop (the dock shows the same; tests compare it). */
static void log_totals(struct tsw_recorder *recorder)
{
	const size_t sections = (size_t)os_atomic_load_long(&recorder->sections);
	if (sections == 0)
		return;
	struct tsw_timeline_totals totals;
	tsw_timeline_get_totals(&recorder->timeline, sections, recorder->stop_ns, &totals);
	const uint64_t final_ns = tsw_timeline_output_ns(&recorder->timeline, sections, recorder->stop_ns);
	const uint64_t recorded_ns = totals.real_time_ns + totals.timelapse_ns;
	tsw_log(LOG_INFO,
		"totals: real time %.3f s, timelapse %.3f s, paused %.3f s | %u switches, %u pauses | final %.3f s, "
		"waiting skipped %.3f s",
		(double)totals.real_time_ns / 1e9, (double)totals.timelapse_ns / 1e9, (double)totals.paused_ns / 1e9,
		totals.switches, totals.pauses, (double)final_ns / 1e9,
		(double)(recorded_ns > final_ns ? recorded_ns - final_ns : 0) / 1e9);
}

/* A stop while paused resumes first, at the very moment of the stop and at the speed before the pause:
 * emptying the encoder needs frames AND audio flowing, because OBS's file writer interleaves them, and
 * with the audio held back by the pause it would wait for ever and never finish the file (found by the
 * "pauses" integration scenario). The final video does not change: the stop falls at that same moment.
 * A pause always leaves room for this section (tsw_timeline_pause). UI thread. */
static void resume_for_stop(struct tsw_recorder *recorder)
{
	pthread_mutex_lock(&recorder->switch_mutex);
	if (os_atomic_load_bool(&recorder->paused)) {
		const long sections = os_atomic_load_long(&recorder->sections);
		const uint32_t speed = (uint32_t)os_atomic_load_long(&recorder->speed);
		if (tsw_timeline_switch(&recorder->timeline, (size_t)sections, recorder->stop_ns, speed) == TSW_OK)
			os_atomic_set_long(&recorder->sections, sections + 1);
		else
			tsw_log(LOG_ERROR, "TSW-E901 error | a stop while paused could not resume to save the file");
		os_atomic_set_bool(&recorder->paused, false);
		tsw_log(LOG_INFO, "stop while paused: frames and audio flow again to finish the file");
	}
	pthread_mutex_unlock(&recorder->switch_mutex);
}

void tsw_recorder_stop(struct tsw_recorder *recorder)
{
	if (!TSW_CHECK(recorder != NULL))
		return;
	if (!os_atomic_load_bool(&recorder->capturing) || recorder->stopping)
		return;
	recorder->stopping = true;
	recorder->stop_ns = os_gettime_ns();

	/* 1. Summary of what came in and went out. */
	const double real_s = (double)(os_gettime_ns() - recorder->start_ns) / (double)SECOND_NS;
	const long wanted = recorder->frames_sent + recorder->frames_lost;
	const double lost = wanted > 0 ? 100.0 * (double)recorder->frames_lost / (double)wanted : 0.0;
	tsw_log(LOG_INFO,
		"recording stopped at %.6f s: frames in %ld, sent %ld, lost %.2f%% | audio blocks sent %ld, "
		"ring full %ld, ring empty %ld | feed: at most %ld of %d frames waiting | smoothness: pictures OBS "
		"repeated %ld, real-time frames sent twice %ld, skipped %ld",
		real_s, recorder->frames_in, recorder->frames_sent, lost, recorder->blocks_sent, recorder->ring_full,
		recorder->ring_empty, feed_peak(recorder), (int)VIDEO_CACHE_FRAMES,
		os_atomic_load_long(&recorder->input_repeats), os_atomic_load_long(&recorder->gate_repeats),
		os_atomic_load_long(&recorder->gate_skips));
	tsw_log(LOG_INFO,
		"start: first frame %+.1f ms, first audio block %+.1f ms | data arrives late by: frames %.1f ms, audio %.1f ms",
		((double)recorder->first_frame_ns - (double)recorder->start_ns) / 1e6,
		((double)recorder->first_block_ns - (double)recorder->start_ns) / 1e6,
		(double)recorder->frame_delay_ns / 1e6, (double)recorder->block_delay_ns / 1e6);
	log_totals(recorder);

	/* 2. Flush, then stop. Encoders hold a few frames (x264 about 40) and hand them over only as new
	 *    frames arrive, so frames keep flowing (one copy each) until the file writer has every frame sent
	 *    before the stop; flush_tick then stops it at its next packet. OBS's usual stop cannot be used: it
	 *    waits for a packet stamped after the real stop time, and in timelapse our packets carry the
	 *    slower time of the final video. finish() stops the capture once the file is saved. */
	resume_for_stop(recorder);
	recorder->flush_target = recorder->frames_sent;
	recorder->flush_start_ns = recorder->flush_seen_ns = os_gettime_ns();
	recorder->flush_seen = -1;
	os_atomic_set_bool(&recorder->forced, false);
	os_atomic_set_bool(&recorder->flushing, true);
	obs_add_tick_callback(flush_tick, recorder);
}

enum tsw_result tsw_recorder_set_speed(struct tsw_recorder *recorder, uint32_t speed)
{
	if (!TSW_CHECK(recorder != NULL && speed >= 1 && speed <= TSW_MAX_SPEED))
		return TSW_E_INVALID;

	if (!TSW_CHECK(!os_atomic_load_bool(&recorder->paused)))
		return TSW_E_INVALID; /* the session resumes with tsw_recorder_resume, never with a switch */

	pthread_mutex_lock(&recorder->switch_mutex);
	enum tsw_result result = TSW_OK;
	if (os_atomic_load_bool(&recorder->capturing) && os_atomic_load_long(&recorder->speed) != (long)speed) {
		/* Append the section, then publish it: the real-time threads only read published sections. */
		const long sections = os_atomic_load_long(&recorder->sections);
		const uint64_t now = os_gettime_ns();
		result = tsw_timeline_switch(&recorder->timeline, (size_t)sections, now, speed);
		if (result == TSW_OK) {
			os_atomic_set_long(&recorder->sections, sections + 1);
			os_atomic_set_long(&recorder->speed, (long)speed);
			tsw_log(LOG_INFO, "switch: speed x%u at %.6f s", speed,
				(double)(now - recorder->start_ns) / (double)SECOND_NS);
			add_chapter(recorder, speed > 1 ? "Chapter.Timelapse" : "Chapter.RealTime", speed);
		} else if (result == TSW_E_TIMELINE_FULL) {
			tsw_log(LOG_WARNING, "TSW-E105 warning | too many speed switches in one recording | max=%d",
				TSW_TIMELINE_CAPACITY);
		}
	}
	pthread_mutex_unlock(&recorder->switch_mutex);
	return result;
}

enum tsw_result tsw_recorder_pause(struct tsw_recorder *recorder)
{
	if (!TSW_CHECK(recorder != NULL))
		return TSW_E_INVALID;

	pthread_mutex_lock(&recorder->switch_mutex);
	enum tsw_result result = TSW_OK;
	if (os_atomic_load_bool(&recorder->capturing) && !recorder->stopping &&
	    !os_atomic_load_bool(&recorder->paused)) {
		/* A paused section: the final video stands still, so the gates let nothing through. The speed
		 * before the pause stays in recorder->speed: it is the one to resume at. */
		const long sections = os_atomic_load_long(&recorder->sections);
		const uint64_t now = os_gettime_ns();
		result = tsw_timeline_pause(&recorder->timeline, (size_t)sections, now);
		if (result == TSW_OK) {
			os_atomic_set_long(&recorder->sections, sections + 1);
			os_atomic_set_bool(&recorder->paused, true);
			tsw_log(LOG_INFO, "pause at %.6f s", (double)(now - recorder->start_ns) / (double)SECOND_NS);
		} else if (result == TSW_E_TIMELINE_FULL) {
			tsw_log(LOG_WARNING, "TSW-E105 warning | too many speed switches in one recording | max=%d",
				TSW_TIMELINE_CAPACITY);
		}
	}
	pthread_mutex_unlock(&recorder->switch_mutex);
	return result;
}

enum tsw_result tsw_recorder_resume(struct tsw_recorder *recorder, uint32_t speed)
{
	if (!TSW_CHECK(recorder != NULL && speed >= 1 && speed <= TSW_MAX_SPEED))
		return TSW_E_INVALID;

	pthread_mutex_lock(&recorder->switch_mutex);
	enum tsw_result result = TSW_OK;
	if (os_atomic_load_bool(&recorder->capturing) && !recorder->stopping &&
	    os_atomic_load_bool(&recorder->paused)) {
		const long sections = os_atomic_load_long(&recorder->sections);
		const uint64_t now = os_gettime_ns();
		result = tsw_timeline_switch(&recorder->timeline, (size_t)sections, now, speed);
		if (result == TSW_OK) {
			const bool new_speed = os_atomic_load_long(&recorder->speed) != (long)speed;
			os_atomic_set_long(&recorder->sections, sections + 1);
			os_atomic_set_long(&recorder->speed, (long)speed);
			os_atomic_set_bool(&recorder->paused, false);
			tsw_log(LOG_INFO, "resume: speed x%u at %.6f s", speed,
				(double)(now - recorder->start_ns) / (double)SECOND_NS);
			if (new_speed) /* same speed: the pause leaves no trace, not even a chapter */
				add_chapter(recorder, speed > 1 ? "Chapter.Timelapse" : "Chapter.RealTime", speed);
		} else if (result == TSW_E_TIMELINE_FULL) {
			tsw_log(LOG_WARNING, "TSW-E105 warning | too many speed switches in one recording | max=%d",
				TSW_TIMELINE_CAPACITY);
		}
	}
	pthread_mutex_unlock(&recorder->switch_mutex);
	return result;
}

bool tsw_recorder_is_active(struct tsw_recorder *recorder)
{
	return TSW_CHECK(recorder != NULL) && os_atomic_load_bool(&recorder->active);
}

void tsw_recorder_add_marker(struct tsw_recorder *recorder)
{
	if (!TSW_CHECK(recorder != NULL))
		return;
	if (!os_atomic_load_bool(&recorder->capturing) || recorder->stopping)
		return;
	recorder->markers++;
	add_chapter(recorder, "Chapter.Marker", recorder->markers);
	tsw_log(LOG_INFO, "marker %u at %.6f s", recorder->markers,
		(double)(os_gettime_ns() - recorder->start_ns) / (double)SECOND_NS);
}

/* The file's size and the free space where it is written, at most once per second: the dock asks ten times
 * per second, and the disk does not change that fast. UI thread. */
static void sample_disk(struct tsw_recorder *recorder, uint64_t now)
{
	if (recorder->path == NULL || now - recorder->disk_checked_ns < SECOND_NS)
		return;
	recorder->disk_checked_ns = now;
	const int64_t size = os_get_file_size(recorder->path);
	if (size >= 0) {
		recorder->file_bytes = (uint64_t)size;
		tsw_disk_meter_add(&recorder->disk, now, recorder->file_bytes);
	}
	struct dstr folder = {0};
	dstr_copy(&folder, recorder->path);
	const char *slash = strrchr(folder.array, '/');
	const char *backslash = strrchr(folder.array, '\\');
	const char *cut = (backslash != NULL && (slash == NULL || backslash > slash)) ? backslash : slash;
	if (cut != NULL) {
		dstr_resize(&folder, (size_t)(cut - folder.array));
		recorder->disk_free_bytes = os_get_free_disk_space(folder.array);
	}
	dstr_free(&folder);
}

/* What the dock's details and health need (ux-spec 3.17), from the timeline and the counters. UI thread. */
static void fill_monitoring(struct tsw_recorder *recorder, struct tsw_status *status, size_t sections, uint64_t now,
			    bool saving)
{
	tsw_timeline_get_totals(&recorder->timeline, sections, now, &status->totals);
	tsw_timeline_map(&recorder->timeline, sections, now, status->map);
	status->frames_lost = (uint64_t)os_atomic_load_long(&recorder->frames_lost);
	status->feed_size = VIDEO_CACHE_FRAMES;
	if (!saving && recorder->video != NULL) {
		const long waiting = os_atomic_load_long(&recorder->frames_sent) +
				     os_atomic_load_long(&recorder->frames_lost) -
				     (long)video_output_get_total_frames(recorder->video);
		/* While frames are being lost OBS owes repeats to the last slot, so the count can exceed the feed:
		 * it is reported as full, and the lost frames tell the rest. */
		status->feed_waiting =
			waiting <= 0 ? 0 : (waiting >= VIDEO_CACHE_FRAMES ? VIDEO_CACHE_FRAMES : (uint32_t)waiting);
	}
	if (!saving)
		sample_disk(recorder, now);
	status->file_bytes = recorder->file_bytes;
	status->disk_free_bytes = recorder->disk_free_bytes;
	status->bytes_per_second = tsw_disk_meter_rate(&recorder->disk);
	status->obs_recording_active = obs_frontend_recording_active();
}

void tsw_recorder_get_status(struct tsw_recorder *recorder, struct tsw_status *status)
{
	if (!TSW_CHECK(recorder != NULL && status != NULL))
		return;
	memset(status, 0, sizeof(*status));
	status->outcome = recorder->outcome;
	status->error_code = recorder->error_code;
	status->last_recorded_ns = recorder->last_recorded_ns;
	status->last_final_ns = recorder->last_final_ns;
	status->path = recorder->path != NULL ? recorder->path
					      : (recorder->last_path != NULL ? recorder->last_path : "");
	status->speed = 1;
	if (!os_atomic_load_bool(&recorder->active))
		return; /* idle */

	/* Recording, or saving: then the counters stay where stop was pressed. */
	const bool saving = recorder->stopping || !os_atomic_load_bool(&recorder->capturing);
	status->phase = saving ? TSW_PHASE_SAVING
			       : (os_atomic_load_bool(&recorder->paused) ? TSW_PHASE_PAUSED : TSW_PHASE_RECORDING);
	status->speed = (uint32_t)os_atomic_load_long(&recorder->speed);
	const size_t sections = (size_t)os_atomic_load_long(&recorder->sections);
	if (sections == 0)
		return;
	const uint64_t now = (saving && recorder->stop_ns != 0) ? recorder->stop_ns : os_gettime_ns();
	const struct tsw_segment *section = &recorder->timeline.segments[sections - 1];
	status->recorded_ns = tsw_timeline_recorded_ns(&recorder->timeline, sections, now);
	status->final_ns = tsw_timeline_output_ns(&recorder->timeline, sections, now);
	status->section_recorded_ns = now > section->start_ns ? now - section->start_ns : 0;
	status->section_final_ns = status->final_ns > section->output_ns ? status->final_ns - section->output_ns : 0;
	fill_monitoring(recorder, status, sections, now, saving);
}

struct tsw_recorder *tsw_recorder_create(void)
{
	struct tsw_recorder *recorder = bzalloc(sizeof(*recorder));
	if (pthread_mutex_init(&recorder->switch_mutex, NULL) != 0 ||
	    os_event_init(&recorder->saved, OS_EVENT_TYPE_MANUAL) != 0) {
		bfree(recorder);
		return NULL;
	}
	recorder->speed = 1;
	live_recorder = recorder;
	return recorder;
}

void tsw_recorder_close(struct tsw_recorder *recorder)
{
	if (!TSW_CHECK(recorder != NULL))
		return;
	if (!os_atomic_load_bool(&recorder->active))
		return;
	tsw_recorder_stop(recorder);
	if (os_event_timedwait(recorder->saved, SAVE_TIMEOUT_MS) != 0)
		tsw_log(LOG_ERROR, "TSW-E104 error | OBS closed before the file was finalized | file=\"%s\"",
			recorder->path);
	finish(recorder);
}

void tsw_recorder_destroy(struct tsw_recorder *recorder)
{
	if (recorder == NULL)
		return;
	tsw_recorder_close(recorder);
	live_recorder = NULL;
	bfree(recorder->last_path);
	os_event_destroy(recorder->saved);
	pthread_mutex_destroy(&recorder->switch_mutex);
	bfree(recorder);
}
