/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * recorder-rt.c: the recorder's real-time callbacks, what happens to every frame and audio
 * block. They run on OBS's video and audio threads, so the whole file follows
 * docs/coding-rules.md, B1, checked by tools/check.sh: no allocation, no logging, no locks
 * of our own, no I/O.
 *
 * video_output_lock_frame takes a mutex inside OBS for a few instructions, never during the
 * copy. It is OBS's only way to feed frames, and OBS's own recording does the same.
 */

#include <string.h>

#include <media-io/video-frame.h>
#include <util/platform.h>

#include "core/check.h"
#include "core/fingerprint.h"
#include "recorder-internal.h"

/* How many frames wait in our video feed for the encoder right now, kept if it is the most so far. The
 * feed holds 16 frames (VIDEO_CACHE_FRAMES in recorder.c); this tells whether that is too many or too
 * few (docs/performance.md, section 9). Frames sent minus frames OBS's feed thread has handed to the encoder; a lost frame is
 * still handed over (OBS repeats the previous picture), so it counts on both sides. */
static void note_feed_waiting(struct tsw_recorder *recorder)
{
	const long handed = (long)video_output_get_total_frames(recorder->video);
	const long waiting = recorder->frames_sent + recorder->frames_lost - handed;
	if (waiting > recorder->feed_peak)
		os_atomic_set_long(&recorder->feed_peak, waiting);
}

/* Smoothness diagnostics (summary line at the stop): a picture equal to the previous one, and in real time a
 * frame the gate sends twice or skips. Either shows in the file as a stutter. */
static void note_smoothness(struct tsw_recorder *recorder, const struct video_data *frame, uint32_t speed,
			    uint32_t copies)
{
	const uint64_t fingerprint =
		tsw_fingerprint(frame->data[0], frame->linesize[0], recorder->width, recorder->height);
	if (fingerprint != 0 && fingerprint == recorder->last_fingerprint)
		os_atomic_set_long(&recorder->input_repeats, recorder->input_repeats + 1);
	recorder->last_fingerprint = fingerprint;
	if (speed != 1 || os_atomic_load_bool(&recorder->flushing))
		return;
	if (copies == 0)
		os_atomic_set_long(&recorder->gate_skips, recorder->gate_skips + 1);
	else if (copies > 1)
		os_atomic_set_long(&recorder->gate_repeats, recorder->gate_repeats + (long)copies - 1);
}

/* OBS video thread, once per frame: send it 0, 1 or more times to our video feed. */
void tsw_recorder_receive_frame(void *param, struct video_data *frame)
{
	struct tsw_recorder *recorder = param;
	if (!os_atomic_load_bool(&recorder->capturing))
		return;
	/* Frames reach us a little more than a frame after they were drawn (about 38 ms at 30 fps). One
	 * drawn before the start would make the encoder start the file earlier than the audio (whose
	 * earlier blocks the audio gate drops). */
	if (frame->timestamp < recorder->start_ns)
		return;

	/* 1. Where this frame lands in the final video, and how many copies that needs. */
	const size_t sections = (size_t)os_atomic_load_long(&recorder->sections);
	const uint64_t output_ns = tsw_timeline_output_ns(&recorder->timeline, sections, frame->timestamp);
	const uint32_t speed = tsw_timeline_speed(&recorder->timeline, sections, frame->timestamp);
	uint32_t copies = 0; /* paused: nothing reaches the file (REQ-019) */
	if (os_atomic_load_bool(&recorder->flushing))
		copies = 1; /* stopping: keep the encoder moving (tsw_recorder_stop) */
	else if (speed != TSW_SPEED_PAUSED)
		copies = tsw_video_gate_copies(&recorder->video_gate, output_ns, speed);
	os_atomic_set_long(&recorder->frames_in, recorder->frames_in + 1);
	note_smoothness(recorder, frame, speed, copies);
	if (copies == 0)
		return;

	/* 2. Copy it into our feed. If the encoder is behind, OBS repeats the previous frame instead:
	 *    the frame count, and so the sync, is kept, but this picture is lost. */
	struct video_frame out;
	if (!video_output_lock_frame(recorder->video, &out, (int)copies, frame->timestamp)) {
		os_atomic_set_long(&recorder->frames_lost, recorder->frames_lost + (long)copies);
		note_feed_waiting(recorder);
		return;
	}
	struct video_frame in = {0};
	for (size_t plane = 0; plane < MAX_AV_PLANES; plane++) {
		in.data[plane] = frame->data[plane];
		in.linesize[plane] = frame->linesize[plane];
	}
	video_frame_copy(&out, &in, recorder->format, recorder->height);
	video_output_unlock_frame(recorder->video);
	if (recorder->first_frame_ns == 0) {
		recorder->first_frame_ns = frame->timestamp;
		recorder->frame_delay_ns = os_gettime_ns() - frame->timestamp;
	}
	os_atomic_set_long(&recorder->frames_sent, recorder->frames_sent + (long)copies);
	note_feed_waiting(recorder);
}

/* OBS audio thread, once per block of the recorded track: queue it for our audio thread. */
void tsw_recorder_receive_audio(void *param, size_t mix, struct audio_data *data)
{
	struct tsw_recorder *recorder = param;
	(void)mix;
	struct tsw_audio_in in = {0};
	for (uint32_t channel = 0; channel < recorder->ring.channels; channel++)
		in.channel[channel] = (const float *)data->data[channel];
	if (!tsw_audio_ring_push(&recorder->ring, &in, data->frames, data->timestamp))
		os_atomic_set_long(&recorder->ring_full, recorder->ring_full + 1);
}

/* Our audio thread, every 1024 samples: hand one block to the encoder, or skip it. */
bool tsw_recorder_provide_audio(void *param, uint64_t start_ts, uint64_t end_ts, uint64_t *new_ts, uint32_t mixers,
				struct audio_output_data *mixes)
{
	struct tsw_recorder *recorder = param;
	(void)start_ts;
	(void)end_ts;
	(void)mixers;
	if (!os_atomic_load_bool(&recorder->capturing))
		return false;

	/* 1. The oldest block OBS gave us, written straight into the encoder's buffer. */
	const uint32_t channels = recorder->ring.channels;
	struct tsw_audio_out out = {0};
	for (uint32_t channel = 0; channel < channels; channel++) {
		out.channel[channel] = mixes[0].data[channel];
		if (!TSW_CHECK(out.channel[channel] != NULL))
			return false; /* never expected: our audio feed has as many channels as the ring */
	}
	uint64_t timestamp;
	if (!tsw_audio_ring_pop(&recorder->ring, &out, &timestamp)) {
		os_atomic_set_long(&recorder->ring_empty, recorder->ring_empty + 1);
		return false; /* nothing yet; the block will be taken on a later turn */
	}

	/* 2. Send it if that keeps the audio in its place (gate.h), as silence in timelapse (REQ-006). */
	const size_t sections = (size_t)os_atomic_load_long(&recorder->sections);
	const uint64_t end_ns = timestamp + recorder->block_ns;
	const uint64_t output_end_ns = tsw_timeline_output_ns(&recorder->timeline, sections, end_ns);
	if (!tsw_audio_gate_pass(&recorder->audio_gate, output_end_ns, AUDIO_OUTPUT_FRAMES))
		return false;
	if (tsw_timeline_speed(&recorder->timeline, sections, timestamp) > 1 ||
	    tsw_timeline_speed(&recorder->timeline, sections, end_ns) > 1) {
		for (uint32_t channel = 0; channel < channels; channel++)
			memset(out.channel[channel], 0, AUDIO_OUTPUT_FRAMES * sizeof(float));
	}
	*new_ts = timestamp;
	if (recorder->first_block_ns == 0) {
		recorder->first_block_ns = timestamp;
		recorder->block_delay_ns = os_gettime_ns() - timestamp;
	}
	os_atomic_set_long(&recorder->blocks_sent, recorder->blocks_sent + 1);
	return true;
}
