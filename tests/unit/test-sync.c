/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * test-sync.c: timeline and gates together, over long recordings with many switches and pauses.
 *
 * Simulates 10 minutes at 30 fps and 48 kHz (audio blocks 7 ms out of phase with the frames,
 * as in OBS). Every frame sent must land within half a frame of where it ideally belongs; the
 * audio sent must never be more than half a block away from the ideal position (checked after
 * every block, including blocks that straddle a switch or a pause), and in real-time sections
 * every block must start within half a block of its ideal place. Nothing that happens inside a
 * pause may be sent. The ideal position is computed here independently, in floating point, from
 * the list of events.
 *
 * Two recordings:
 *   - 437 speed switches, about one per second, through x1, x2, x3, x4, x5, x8 and x16;
 *   - 410 pauses, each resumed after about 0.7 s, alternating real time and timelapse (REQ-019).
 */

#include "test.h"

#include <math.h>

#include <core/gate.h>
#include <core/timeline.h>

enum {
	SWITCHES = 437,     /* switches in 600 s with the first schedule (hand-counted) */
	PAUSE_EVENTS = 820, /* event 820 falls at 599.42 s and 821 at 600.15 s: 410 pauses, 410 resumes */
	MAX_EVENTS = 1024,
};

static const uint64_t SECOND = 1000000000;
static const uint64_t DURATION_NS = 600 * 1000000000ULL;
static const uint64_t AUDIO_PHASE_NS = 7000000;
static const uint32_t SPEEDS[10] = {4, 1, 8, 1, 2, 16, 1, 3, 1, 5};
static const uint32_t RESUME_SPEEDS[5] = {1, 4, 1, 16, 2};

/* One change of the recording: a switch or resume to speed, or a pause (TSW_SPEED_PAUSED). */
struct event {
	uint64_t at_ns;
	uint32_t speed;
};

static struct tsw_timeline timeline;
static struct event events[MAX_EVENTS];

/* Switch i happens at i * 1.370 s plus (i mod 7) * 0.113 s: irregular, always increasing. */
static size_t make_switches(void)
{
	for (uint64_t i = 1; i <= SWITCHES; i++) {
		events[i - 1].at_ns = i * 1370000000ULL + (i % 7) * 113000000ULL;
		events[i - 1].speed = SPEEDS[(i - 1) % 10];
	}
	return SWITCHES;
}

/* Event i happens at i * 0.731 s plus (i mod 5) * 0.097 s: odd events pause, even events resume, each
 * resume at the next speed of RESUME_SPEEDS, so pauses fall in real time and in timelapse. */
static size_t make_pauses(void)
{
	size_t n = 0;
	for (uint64_t i = 1; n < MAX_EVENTS; i++) {
		const uint64_t at_ns = i * 731000000ULL + (i % 5) * 97000000ULL;
		if (at_ns >= DURATION_NS)
			break;
		events[n].at_ns = at_ns;
		events[n].speed = (i % 2 == 1) ? TSW_SPEED_PAUSED : RESUME_SPEEDS[(i / 2) % 5];
		n++;
	}
	return n;
}

/* Ideal final-video position of OBS time t, in seconds: the sum of section lengths over speed. */
static double ideal_position_s(size_t n, uint64_t t)
{
	double position = 0.0;
	uint64_t section_start = 0;
	uint32_t speed = 1;
	for (size_t i = 0; i < n && events[i].at_ns <= t; i++) {
		if (speed != TSW_SPEED_PAUSED)
			position += (double)(events[i].at_ns - section_start) / 1e9 / speed;
		section_start = events[i].at_ns;
		speed = events[i].speed;
	}
	if (speed == TSW_SPEED_PAUSED)
		return position;
	return position + (double)(t - section_start) / 1e9 / speed;
}

/* Appends to the timeline every event up to time t, as the plugin's UI thread would. */
static void publish_events(size_t n, uint64_t t, size_t *next, size_t *count)
{
	while (*next < n && events[*next].at_ns <= t) {
		const struct event *e = &events[*next];
		const enum tsw_result result = (e->speed == TSW_SPEED_PAUSED)
						       ? tsw_timeline_pause(&timeline, *count, e->at_ns)
						       : tsw_timeline_switch(&timeline, *count, e->at_ns, e->speed);
		TSW_EXPECT_EQ(result, TSW_OK);
		(*count)++;
		(*next)++;
	}
}

struct tally {
	uint64_t video_misplaced, audio_misplaced, sent_while_paused;
	double worst_video_s, worst_audio_s;
};

/* One frame at OBS time t: sent as the recorder does (never while paused), checked against the ideal. */
static void simulate_frame(struct tsw_video_gate *video, size_t n, size_t count, uint64_t t, struct tally *tally)
{
	const uint32_t speed = tsw_timeline_speed(&timeline, count, t);
	if (speed == TSW_SPEED_PAUSED)
		return; /* recorder-rt.c sends nothing while paused */
	const uint64_t output = tsw_timeline_output_ns(&timeline, count, t);
	if (tsw_video_gate_copies(video, output, speed) > 0) {
		const double sent_at_s = (double)(video->frames_out - 1) / 30;
		const double error_s = fabs(sent_at_s - ideal_position_s(n, t));
		tally->worst_video_s = fmax(tally->worst_video_s, error_s);
		tally->video_misplaced += (error_s > 0.5 / 30 + 1e-6); /* plus 1 us for nanosecond rounding */
	}
}

/* One audio block from OBS time t to end_t: sent if the gate says so, checked against the ideal. */
static void simulate_block(struct tsw_audio_gate *audio, size_t n, size_t count, uint64_t t, uint64_t end_t,
			   struct tally *tally)
{
	const double half_block_s = 0.5 * 1024 / 48000 + 1e-6;
	const bool sent = tsw_audio_gate_pass(audio, tsw_timeline_output_ns(&timeline, count, end_t), 1024);
	const double drift_s = fabs((double)audio->samples_out / 48000 - ideal_position_s(n, end_t));
	tally->worst_audio_s = fmax(tally->worst_audio_s, drift_s);
	tally->audio_misplaced += (drift_s > half_block_s);
	const uint32_t speed_start = tsw_timeline_speed(&timeline, count, t);
	const uint32_t speed_end = tsw_timeline_speed(&timeline, count, end_t);
	if (sent && speed_start == 1 && speed_end == 1) {
		const double sent_at_s = (double)(audio->samples_out - 1024) / 48000;
		tally->audio_misplaced += (fabs(sent_at_s - ideal_position_s(n, t)) > half_block_s);
	}
	tally->sent_while_paused += (sent && speed_start == TSW_SPEED_PAUSED && speed_end == TSW_SPEED_PAUSED);
}

/* Plays the n events over 10 minutes and checks every frame and every audio block. */
static void simulate(size_t n, const char *name)
{
	struct tsw_video_gate video;
	struct tsw_audio_gate audio;
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 0, 0, 1), TSW_OK);
	TSW_EXPECT_EQ(tsw_video_gate_init(&video, 30, 1), TSW_OK);
	TSW_EXPECT_EQ(tsw_audio_gate_init(&audio, 48000), TSW_OK);

	struct tally tally = {0};
	size_t count = 1, next = 0;
	uint64_t frame = 0, block = 0;
	for (;;) {
		const uint64_t frame_t = frame * SECOND / 30;
		const uint64_t block_t = AUDIO_PHASE_NS + block * 1024 * SECOND / 48000;
		const uint64_t t = (frame_t < block_t) ? frame_t : block_t;
		if (t >= DURATION_NS)
			break;
		publish_events(n, t, &next, &count);
		if (t == frame_t) {
			simulate_frame(&video, n, count, t, &tally);
			frame++;
		} else {
			/* The end of the block may fall after an event that is not published yet: publish it
			 * first, as the plugin will (the audio thread always runs behind the switches). */
			const uint64_t end_t = t + 1024 * SECOND / 48000;
			publish_events(n, end_t, &next, &count);
			simulate_block(&audio, n, count, t, end_t, &tally);
			block++;
		}
	}

	printf("    %s: worst video error %.3f ms, worst audio error %.3f ms, %zu sections\n", name,
	       tally.worst_video_s * 1000, tally.worst_audio_s * 1000, count);
	TSW_EXPECT_EQ(count, n + 1);
	TSW_EXPECT_EQ(tally.video_misplaced, 0);
	TSW_EXPECT_EQ(tally.audio_misplaced, 0);
	TSW_EXPECT_EQ(tally.sent_while_paused, 0);
}

/* Break caught: audio or video drifting from the ideal position after many switches (sync lost). */
static void many_switches_keep_audio_and_video_in_place(void)
{
	const size_t n = make_switches();
	TSW_EXPECT_EQ(n, SWITCHES);
	simulate(n, "437 switches");
}

/* Break caught: sync lost, or something from inside a pause recorded, after many quick pauses and
 * resumes in real time and in timelapse (REQ-019). */
static void many_quick_pauses_keep_audio_and_video_in_place(void)
{
	const size_t n = make_pauses();
	TSW_EXPECT_EQ(n, PAUSE_EVENTS);
	simulate(n, "410 pauses");
}

int main(void)
{
	many_switches_keep_audio_and_video_in_place();
	many_quick_pauses_keep_audio_and_video_in_place();
	return tsw_test_failures;
}
