/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * test-sync.c: timeline and gates together, over a long recording with many switches.
 *
 * Simulates 10 minutes at 30 fps and 48 kHz (audio blocks 7 ms out of phase with the frames,
 * as in OBS), switching speed about every second through x1, x2, x3, x4, x5, x8 and x16.
 * Every frame sent must land within half a frame of where it ideally belongs; the audio sent
 * must never be more than half a block away from the ideal position (checked after every
 * block, including blocks that straddle a switch), and in real-time sections every block must
 * start within half a block of its ideal place. The ideal position is computed here
 * independently, in floating point, from the switch schedule.
 */

#include "test.h"

#include <math.h>

#include <core/gate.h>
#include <core/timeline.h>

enum { SWITCHES = 437 }; /* switches in 600 s with the schedule below (hand-counted) */

static const uint64_t SECOND = 1000000000;
static const uint64_t DURATION_NS = 600 * 1000000000ULL;
static const uint64_t AUDIO_PHASE_NS = 7000000;
static const uint32_t SPEEDS[10] = {4, 1, 8, 1, 2, 16, 1, 3, 1, 5};

static struct tsw_timeline timeline;

/* Switch i happens at i * 1.370 s plus (i mod 7) * 0.113 s: irregular, always increasing. */
static uint64_t switch_time_ns(uint64_t i)
{
	return i * 1370000000ULL + (i % 7) * 113000000ULL;
}

/* Ideal final-video position of OBS time t, in seconds: the sum of section lengths over speed. */
static double ideal_position_s(uint64_t t)
{
	double position = 0.0;
	uint64_t section_start = 0;
	uint32_t speed = 1;
	for (uint64_t i = 1; i <= SWITCHES; i++) {
		const uint64_t next = switch_time_ns(i);
		if (next > t)
			break;
		position += (double)(next - section_start) / 1e9 / speed;
		section_start = next;
		speed = SPEEDS[(i - 1) % 10];
	}
	return position + (double)(t - section_start) / 1e9 / speed;
}

/* Break caught: audio or video drifting from the ideal position after many switches (sync lost). */
static void many_switches_keep_audio_and_video_in_place(void)
{
	struct tsw_video_gate video;
	struct tsw_audio_gate audio;
	TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, 0, 0, 1), TSW_OK);
	TSW_EXPECT_EQ(tsw_video_gate_init(&video, 30, 1), TSW_OK);
	TSW_EXPECT_EQ(tsw_audio_gate_init(&audio, 48000), TSW_OK);

	const double half_frame_s = 0.5 / 30 + 1e-6; /* plus 1 microsecond for nanosecond rounding */
	const double half_block_s = 0.5 * 1024 / 48000 + 1e-6;
	size_t count = 1;
	uint64_t next_switch = 1, frame = 0, block = 0;
	uint64_t video_misplaced = 0, audio_misplaced = 0;
	double worst_video_s = 0.0, worst_audio_s = 0.0;

	for (;;) {
		const uint64_t frame_t = frame * SECOND / 30;
		const uint64_t block_t = AUDIO_PHASE_NS + block * 1024 * SECOND / 48000;
		const uint64_t t = (frame_t < block_t) ? frame_t : block_t;
		if (t >= DURATION_NS)
			break;
		while (next_switch <= SWITCHES && switch_time_ns(next_switch) <= t) {
			const uint32_t speed = SPEEDS[(next_switch - 1) % 10];
			TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, count, switch_time_ns(next_switch), speed),
				      TSW_OK);
			count++;
			next_switch++;
		}
		if (t == frame_t) {
			const uint64_t output = tsw_timeline_output_ns(&timeline, count, t);
			const uint32_t speed = tsw_timeline_speed(&timeline, count, t);
			if (tsw_video_gate_copies(&video, output, speed) > 0) {
				const double sent_at_s = (double)(video.frames_out - 1) / 30;
				const double error_s = fabs(sent_at_s - ideal_position_s(t));
				worst_video_s = fmax(worst_video_s, error_s);
				video_misplaced += (error_s > half_frame_s);
			}
			frame++;
		} else {
			/* The end of the block may fall after a switch that is not published yet: publish it first,
			 * as the plugin will (the audio thread always runs behind the switches). */
			const uint64_t end_t = t + 1024 * SECOND / 48000;
			while (next_switch <= SWITCHES && switch_time_ns(next_switch) <= end_t) {
				const uint32_t speed = SPEEDS[(next_switch - 1) % 10];
				TSW_EXPECT_EQ(tsw_timeline_switch(&timeline, count, switch_time_ns(next_switch), speed),
					      TSW_OK);
				count++;
				next_switch++;
			}
			const bool sent =
				tsw_audio_gate_pass(&audio, tsw_timeline_output_ns(&timeline, count, end_t), 1024);
			const double drift_s = fabs((double)audio.samples_out / 48000 - ideal_position_s(end_t));
			worst_audio_s = fmax(worst_audio_s, drift_s);
			audio_misplaced += (drift_s > half_block_s);
			if (sent && tsw_timeline_speed(&timeline, count, t) == 1 &&
			    tsw_timeline_speed(&timeline, count, end_t) == 1) {
				const double sent_at_s = (double)(audio.samples_out - 1024) / 48000;
				audio_misplaced += (fabs(sent_at_s - ideal_position_s(t)) > half_block_s);
			}
			block++;
		}
	}

	printf("    worst video error %.3f ms, worst audio error %.3f ms, %zu sections\n", worst_video_s * 1000,
	       worst_audio_s * 1000, count);
	TSW_EXPECT_EQ(count, SWITCHES + 1);
	TSW_EXPECT_EQ(video_misplaced, 0);
	TSW_EXPECT_EQ(audio_misplaced, 0);
}

int main(void)
{
	many_switches_keep_audio_and_video_in_place();
	return tsw_test_failures;
}
