/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * gpu-road.h: road C (ADR-012), frames taken from the graphics card by the plugin itself, off OBS's
 * graphics thread.
 *
 * On road B OBS copies every frame into memory on its graphics thread, the thread that then draws the
 * preview. On road C the graphics thread only asks the card to convert and copy the frame into a
 * staging slot; our copier thread copies it from there into the recorder, through the same callback
 * road B uses (tsw_recorder_receive_frame), with the same times and repeats OBS's feed would give.
 *
 *   OBS graphics thread  --rendered callback-->  convert and stage the frame, hand over the one staged
 *                                                two frames ago
 *   our copier thread    --------------------->  receive(param, frame), count times per frame
 *
 * Life of a road: create (before the recording starts: card memory and the copier thread), begin (the
 * moment the recording takes frames in), destroy (after the last frame is taken). UI thread.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <media-io/video-io.h>
#include <obs.h>

#include "tsw-compiler.h"

struct tsw_gpu_road;

/* Where a frame goes: called on the copier thread, once per frame interval, as OBS's feed would. */
typedef void (*tsw_gpu_road_receive)(void *param, struct video_data *frame);

/*
 * tsw_gpu_road_unsupported: why road C cannot record this video setup, or NULL if it can. Road C
 * supports unscaled output (base resolution equal to output resolution) in NV12, I420 or I444, SDR.
 */
const char *tsw_gpu_road_unsupported(const struct obs_video_info *ovi) TSW_NODISCARD;

/*
 * tsw_gpu_road_create: card memory for the conversion and the staging slots, and the copier thread.
 * Thread: UI. Returns NULL if anything could not be prepared (logged); then road B is used.
 */
struct tsw_gpu_road *tsw_gpu_road_create(const struct obs_video_info *ovi, tsw_gpu_road_receive receive,
					 void *param) TSW_NODISCARD;

/* tsw_gpu_road_begin: from the next frame OBS renders, stage frames. Thread: UI. */
void tsw_gpu_road_begin(struct tsw_gpu_road *road);

/*
 * tsw_gpu_road_destroy: stop staging (waits for a frame in progress), stop the copier, give the card
 * memory back. Thread: UI. Safe with NULL.
 */
void tsw_gpu_road_destroy(struct tsw_gpu_road *road);

/* What road C counted so far, for the summary line. Thread: UI, before tsw_gpu_road_destroy. */
struct tsw_gpu_road_counts {
	long unstaged;        /* frames rendered while every slot was busy: the frame before covered them */
	long unmapped;        /* frames whose slot could not be mapped: not copied */
	long staged;          /* frames staged */
	long slow_steps;      /* steps of ours on OBS's graphics thread longer than 2 ms */
	long longest_step_us; /* the longest of them, in microseconds */
};
void tsw_gpu_road_get_counts(const struct tsw_gpu_road *road, struct tsw_gpu_road_counts *counts);
