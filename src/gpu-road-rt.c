/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * gpu-road-rt.c: road C on OBS's graphics thread (ADR-012), after each canvas is rendered. It runs on
 * every frame OBS renders, before the preview, so the whole file follows docs/coding-rules.md, B1,
 * checked by tools/check.sh: no allocation, no logging, no locks of our own, no waits. It never waits
 * for the copier (a frame that finds every slot busy is covered by the frame before, stage-ring.h),
 * nor for the card:
 *
 *   frame k:    convert it, ask for its copy (same place as OBS's own copy: before the preview)
 *               hand over frame k-2, whose copy has had two frames to finish
 *
 * The conversion is OBS's own, step by step (render_convert_texture and render_convert_plane in
 * libobs/obs-video.c, OBS 32.2.2): same effect, techniques, parameters and order, so the picture is
 * the one OBS would put in its memory feed. The verification mode checks it frame by frame.
 */

#include <graphics/graphics.h>
#include <util/platform.h>
#include <util/profiler.h>

#include "gpu-road-internal.h"

static const char *const PROFILE_NAME = "timelapse-switch: road C";

/* 1. The slots the copier finished come back: unmap them. */
static void reclaim_slots(struct tsw_gpu_road *road)
{
	size_t slot;
	bool mapped;
	while (tsw_stage_ring_reclaim(&road->ring, &slot, &mapped)) {
		if (!mapped)
			continue;
		for (size_t plane = 0; plane < tsw_gpu_road_planes(road); plane++)
			gs_stagesurface_unmap(road->surface[slot][plane]);
	}
}

/* One plane of the conversion: a triangle that covers the plane, through the plane's technique. */
static void convert_plane(const struct tsw_gpu_road *road, size_t set, size_t plane)
{
	const uint32_t width = road->plane[plane].width;
	const uint32_t height = road->plane[plane].height;
	gs_technique_t *technique = gs_effect_get_technique(road->effect, road->plane[plane].technique);
	gs_set_render_target(road->texture[set][plane], NULL);
	gs_ortho(0.0f, (float)width, 0.0f, (float)height, -100.0f, 100.0f);
	gs_set_viewport(0, 0, (int)width, (int)height);
	const size_t passes = gs_technique_begin(technique);
	for (size_t pass = 0; pass < passes; pass++) {
		gs_technique_begin_pass(technique, pass);
		gs_draw(GS_TRIS, 0, 3);
		gs_technique_end_pass(technique);
	}
	gs_technique_end(technique);
}

/* The parameters every plane after the first one also needs. */
static void set_plane_parameters(const struct tsw_gpu_road *road, gs_texture_t *frame, float sdr_white, float hdr_lw)
{
	gs_effect_set_texture(road->image, frame);
	gs_effect_set_float(road->width_i_param, road->width_i);
	gs_effect_set_float(road->height_i_param, road->height_i);
	gs_effect_set_float(road->sdr_white_nits_over_maximum, sdr_white);
	gs_effect_set_float(road->hdr_lw, hdr_lw);
}

/* 2a. The rendered frame in the output format, into one set of our planes, as OBS converts it. */
static void convert(const struct tsw_gpu_road *road, gs_texture_t *frame, size_t set)
{
	const float sdr_white = obs_get_video_sdr_white_level() / 10000.f;
	const float hdr_lw = obs_get_video_hdr_nominal_peak_level();
	gs_effect_set_texture(road->image, frame);
	gs_effect_set_vec4(road->color_vec0, &road->color_vec[0]);
	gs_effect_set_float(road->sdr_white_nits_over_maximum, sdr_white);
	gs_effect_set_float(road->hdr_lw, hdr_lw);
	convert_plane(road, set, 0);
	if (road->planes >= 2) {
		gs_effect_set_vec4(road->color_vec1, &road->color_vec[1]);
		if (road->planes == 2)
			gs_effect_set_vec4(road->color_vec2, &road->color_vec[2]);
		set_plane_parameters(road, frame, sdr_white, hdr_lw);
		convert_plane(road, set, 1);
	}
	if (road->planes == 3) {
		gs_effect_set_vec4(road->color_vec2, &road->color_vec[2]);
		set_plane_parameters(road, frame, sdr_white, hdr_lw);
		convert_plane(road, set, 2);
	}
}

/* 2b. Convert the frame into one set of our planes, leaving OBS's state as it was. */
static void convert_frame(const struct tsw_gpu_road *road, gs_texture_t *frame, size_t set)
{
	gs_texture_t *target = gs_get_render_target();
	gs_zstencil_t *zstencil = gs_get_zstencil_target();
	const enum gs_color_space space = gs_get_color_space();
	gs_viewport_push();
	gs_projection_push();
	gs_blend_state_push();
	gs_enable_blending(false);
	/* The triangle needs no vertices (the shader makes them); whoever draws next loads its own. */
	gs_load_vertexbuffer(NULL);
	gs_load_indexbuffer(NULL);

	convert(road, frame, set);

	gs_blend_state_pop();
	gs_projection_pop();
	gs_viewport_pop();
	gs_set_render_target_with_color_space(target, zstencil, space);
}

/* 2c. Ask the card to copy one set of our planes into the slot (it does not wait). */
static void stage_planes(const struct tsw_gpu_road *road, size_t slot, size_t set)
{
	for (size_t plane = 0; plane < tsw_gpu_road_planes(road); plane++)
		gs_stage_texture(road->surface[slot][plane], road->texture[set][plane]);
}

/* 3. A frame whose copy was asked for two frames ago, so the card is done: map it, hand it to the copier. */
static void hand_over(struct tsw_gpu_road *road, size_t slot)
{
	size_t mapped = 0;
	while (mapped < tsw_gpu_road_planes(road) &&
	       gs_stagesurface_map(road->surface[slot][mapped], &road->data[slot][mapped],
				   &road->linesize[slot][mapped]))
		mapped++;
	const bool complete = mapped == tsw_gpu_road_planes(road);
	while (!complete && mapped > 0) {
		mapped--;
		gs_stagesurface_unmap(road->surface[slot][mapped]);
	}
	tsw_stage_ring_publish(&road->ring, slot, complete);
	os_sem_post(road->wake);
}

/* Diagnostics for the summary line: how often our work on the graphics thread took long, and the longest. */
static void note_step(struct tsw_gpu_road *road, uint64_t started_ns)
{
	const uint64_t took_ns = os_gettime_ns() - started_ns;
	if (took_ns > TSW_GPU_SLOW_STEP_NS)
		os_atomic_set_long(&road->slow_steps, road->slow_steps + 1);
	if ((long)(took_ns / 1000) > road->longest_step_us)
		os_atomic_set_long(&road->longest_step_us, (long)(took_ns / 1000));
}

/* Whether OBS is rendering the main canvas now: its texture is the render target (OBS calls our callbacks
 * once per canvas). */
static gs_texture_t *main_canvas_frame(const struct tsw_gpu_road *road)
{
	gs_texture_t *frame = obs_get_main_texture();
	if (frame == NULL || gs_get_render_target() != frame)
		return NULL;
	if (gs_texture_get_width(frame) != road->width || gs_texture_get_height(frame) != road->height)
		return NULL; /* never expected: OBS refuses new video settings while an output records */
	return frame;
}

void tsw_gpu_road_rendered(void *param)
{
	struct tsw_gpu_road *road = param;
	gs_texture_t *frame = main_canvas_frame(road);
	if (frame == NULL)
		return;
	const uint64_t started_ns = os_gettime_ns();
	profile_start(PROFILE_NAME);
	reclaim_slots(road);

	/* This frame: convert it and ask for its copy right after the scene, where OBS asks for the copy of its
	 * own memory feed, so the card does it before the preview. */
	size_t slot;
	size_t previous;
	if (tsw_stage_ring_claim(&road->ring, obs_get_video_frame_time(), &slot, &previous)) {
		const size_t set = road->next_set;
		convert_frame(road, frame, set);
		stage_planes(road, slot, set);
		road->next_set = (set + 1) % TSW_GPU_PLANE_SETS;
		os_atomic_set_long(&road->staged, road->staged + 1);
	}

	/* The frame before now knows how many intervals it covers; it waits one more frame, so the one handed
	 * over is always two frames old and its copy done: mapping it never waits for the card. */
	if (previous != SIZE_MAX) {
		if (road->waiting != SIZE_MAX)
			hand_over(road, road->waiting);
		road->waiting = previous;
	}
	profile_end(PROFILE_NAME);
	note_step(road, started_ns);
}
