/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * sync-probe.c: a small OBS module for the integration test only (tests/integration/README.md).
 * run.sh installs it in the test OBS; it is never shipped.
 *
 * It watches OBS's picture and sound exactly as the plugin receives them, and writes to the OBS
 * log when each beep of the test media starts and when each frame whose number is a multiple of
 * 30 (the frames the beeps belong to) arrives. analyze.py compares those times with the
 * recording: any difference was added by the plugin.
 *
 * Exception to rule B1 (docs/coding-rules.md): it logs from the real-time threads. That is
 * acceptable here, in a test module, at two short lines per second.
 */

#include <math.h>

#include <obs-module.h>

OBS_DECLARE_MODULE()

enum {
	BARS = 16,              /* the test media spells each frame number in 16 bars, bit 0 on the left */
	BEEP_PERIOD = 30,       /* a beep starts on every frame whose number is a multiple of 30 */
	BAR_LEVEL = 125,        /* luma above this is a white bar (bit set) */
	QUIET_BEFORE_BEEP = 10, /* a beep follows at least 1/10 s of quiet */
};
static const float BEEP_LEVEL = 3000.0f / 32768.0f; /* same level as analyze.py */
static const uint64_t SECOND_NS = 1000000000;

/* The probe's state. Each part has a single writer: the video thread or the audio thread. */
static struct {
	uint32_t width, height;
	uint32_t samples_per_sec;
	uint32_t last_number; /* video thread */
	uint64_t quiet;       /* audio thread: samples since the last loud one */
} probe;

/* OBS video thread, once per frame: the frame number, read from the middle row of the picture. */
static void on_frame(void *param, struct video_data *frame)
{
	(void)param;
	const uint8_t *row = frame->data[0] + (size_t)frame->linesize[0] * (probe.height / 2);
	uint32_t number = 0;
	for (uint32_t bar = 0; bar < BARS; bar++) {
		if (row[(2 * bar + 1) * probe.width / (2 * BARS)] > BAR_LEVEL)
			number |= 1u << bar;
	}
	if (number % BEEP_PERIOD == 0 && number != probe.last_number)
		blog(LOG_INFO, "[tsw-sync-probe] frame %u at %llu", number, (unsigned long long)frame->timestamp);
	probe.last_number = number;
}

/* OBS audio thread, once per block of track 1: the time of each beep onset, to the sample. */
static void on_audio(void *param, size_t mix, struct audio_data *data)
{
	(void)param;
	(void)mix;
	const float *samples = (const float *)data->data[0];
	for (uint32_t i = 0; i < data->frames; i++) {
		if (fabsf(samples[i]) <= BEEP_LEVEL) {
			probe.quiet++;
			continue;
		}
		if (probe.quiet > probe.samples_per_sec / QUIET_BEFORE_BEEP) {
			const uint64_t onset = data->timestamp + (uint64_t)i * SECOND_NS / probe.samples_per_sec;
			blog(LOG_INFO, "[tsw-sync-probe] beep at %llu", (unsigned long long)onset);
		}
		probe.quiet = 0;
	}
}

bool obs_module_load(void)
{
	struct obs_video_info ovi;
	struct obs_audio_info oai;
	if (!obs_get_video_info(&ovi) || !obs_get_audio_info(&oai))
		return false;
	probe.width = ovi.output_width;
	probe.height = ovi.output_height;
	probe.samples_per_sec = oai.samples_per_sec;
	probe.last_number = UINT32_MAX;
	probe.quiet = UINT64_MAX / 2; /* as after a long quiet, so a beep at the very start counts */
	obs_add_raw_video_callback(NULL, on_frame, NULL);
	obs_add_raw_audio_callback(0, NULL, on_audio, NULL);
	blog(LOG_INFO, "[tsw-sync-probe] watching %ux%u, %u Hz", probe.width, probe.height, probe.samples_per_sec);
	return true;
}

void obs_module_unload(void)
{
	obs_remove_raw_video_callback(on_frame, NULL);
	obs_remove_raw_audio_callback(0, on_audio, NULL);
}
