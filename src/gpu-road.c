/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * gpu-road.c: road C (ADR-012), its preparation, its copier thread and its end. What runs on OBS's
 * graphics thread for every frame is in gpu-road-rt.c.
 */

#include "gpu-road.h"

#include <string.h>

#include <graphics/matrix4.h>
#include <util/bmem.h>
#include <util/platform.h>

#include "gpu-road-internal.h"
#include "plugin-support.h"
#include "tsw-compiler.h"

/* The planes of each supported format, as OBS creates them for its memory feed (libobs/obs.c,
 * calc_gpu_conversion_sizes and obs_init_gpu_copy_surfaces). Returns false for any other format. */
static bool describe_planes(struct tsw_gpu_road *road, enum video_format format)
{
	const uint32_t width = road->width;
	const uint32_t height = road->height;
	const struct tsw_gpu_plane y = {"Planar_Y", width, height, GS_R8};
	road->width_i = 0.0f;
	road->height_i = 0.0f;
	switch (format) {
	case VIDEO_FORMAT_NV12:
		road->planes = 2;
		road->plane[0] = (struct tsw_gpu_plane){"NV12_Y", width, height, GS_R8};
		road->plane[1] = (struct tsw_gpu_plane){"NV12_UV", width / 2, height / 2, GS_R8G8};
		road->width_i = 1.0f / (float)width;
		return true;
	case VIDEO_FORMAT_I420:
		road->planes = 3;
		road->plane[0] = y;
		road->plane[1] = (struct tsw_gpu_plane){"Planar_U_Left", width / 2, height / 2, GS_R8};
		road->plane[2] = (struct tsw_gpu_plane){"Planar_V_Left", width / 2, height / 2, GS_R8};
		road->width_i = 1.0f / (float)width;
		return true;
	case VIDEO_FORMAT_I444:
		road->planes = 3;
		road->plane[0] = y;
		road->plane[1] = (struct tsw_gpu_plane){"Planar_U", width, height, GS_R8};
		road->plane[2] = (struct tsw_gpu_plane){"Planar_V", width, height, GS_R8};
		return true;
	default:
		return false;
	}
}

/* OBS's color matrix for the output (libobs/obs.c, set_video_matrix), as the rows OBS hands to the
 * shader (libobs/obs-video.c, render_convert_texture). */
static void set_color_matrix(struct tsw_gpu_road *road, const struct obs_video_info *ovi)
{
	struct matrix4 matrix;
	video_format_get_parameters_for_format(ovi->colorspace, ovi->range, ovi->output_format, (float *)&matrix, NULL,
					       NULL);
	matrix4_inv(&matrix, &matrix);
	const struct vec4 r_row = matrix.x; /* OBS swaps the R and G rows */
	matrix.x = matrix.y;
	matrix.y = r_row;
	float m[16];
	memcpy(m, &matrix, sizeof(m));
	vec4_set(&road->color_vec[0], m[4], m[5], m[6], m[7]);
	vec4_set(&road->color_vec[1], m[0], m[1], m[2], m[3]);
	vec4_set(&road->color_vec[2], m[8], m[9], m[10], m[11]);
}

const char *tsw_gpu_road_unsupported(const struct obs_video_info *ovi)
{
	if (ovi == NULL)
		return "no video settings";
	if (!TSW_GPU_ROAD_PLATFORM)
		return "road C is tested on Linux only so far";
	if (ovi->base_width != ovi->output_width || ovi->base_height != ovi->output_height)
		return "the output is scaled (base and output resolutions differ)";
	if (ovi->output_format != VIDEO_FORMAT_NV12 && ovi->output_format != VIDEO_FORMAT_I420 &&
	    ovi->output_format != VIDEO_FORMAT_I444)
		return "the output format is not NV12, I420 or I444";
	if (ovi->colorspace != VIDEO_CS_DEFAULT && ovi->colorspace != VIDEO_CS_601 && ovi->colorspace != VIDEO_CS_709 &&
	    ovi->colorspace != VIDEO_CS_SRGB)
		return "the color space is HDR";
	return NULL;
}

/* OBS's conversion effect and its parameters: OBS loaded it at startup, so this returns OBS's own copy. */
static bool find_effect(struct tsw_gpu_road *road)
{
	char *path = obs_find_data_file("format_conversion.effect");
	road->effect = path != NULL ? gs_effect_create_from_file(path, NULL) : NULL;
	bfree(path);
	if (road->effect == NULL)
		return false;
	road->image = gs_effect_get_param_by_name(road->effect, "image");
	road->color_vec0 = gs_effect_get_param_by_name(road->effect, "color_vec0");
	road->color_vec1 = gs_effect_get_param_by_name(road->effect, "color_vec1");
	road->color_vec2 = gs_effect_get_param_by_name(road->effect, "color_vec2");
	road->width_i_param = gs_effect_get_param_by_name(road->effect, "width_i");
	road->height_i_param = gs_effect_get_param_by_name(road->effect, "height_i");
	road->sdr_white_nits_over_maximum = gs_effect_get_param_by_name(road->effect, "sdr_white_nits_over_maximum");
	road->hdr_lw = gs_effect_get_param_by_name(road->effect, "hdr_lw");
	bool found = road->image != NULL && road->color_vec0 != NULL && road->color_vec1 != NULL &&
		     road->color_vec2 != NULL && road->width_i_param != NULL && road->height_i_param != NULL &&
		     road->sdr_white_nits_over_maximum != NULL && road->hdr_lw != NULL;
	for (size_t plane = 0; plane < tsw_gpu_road_planes(road); plane++)
		found = found && gs_effect_get_technique(road->effect, road->plane[plane].technique) != NULL;
	return found;
}

/* The converted planes and the staging slots, on the card. Graphics context entered. */
static bool create_surfaces(struct tsw_gpu_road *road)
{
	bool created = true;
	for (size_t plane = 0; plane < tsw_gpu_road_planes(road); plane++) {
		const struct tsw_gpu_plane *p = &road->plane[plane];
		road->texture[plane] = gs_texture_create(p->width, p->height, p->format, 1, NULL, GS_RENDER_TARGET);
		created = created && road->texture[plane] != NULL;
		for (size_t slot = 0; slot < TSW_STAGE_SLOTS; slot++) {
			road->surface[slot][plane] = gs_stagesurface_create(p->width, p->height, p->format);
			created = created && road->surface[slot][plane] != NULL;
		}
	}
	return created;
}

/* What the copier hands to the recorder for one slot: the frame, once per interval it covers, with
 * the times OBS's own feed would give the repeats (video-io.c adds one frame time per repeat). */
static void copy_slot(const struct tsw_gpu_road *road, size_t slot)
{
	const struct tsw_stage_slot *staged = &road->ring.slots[slot];
	if (!staged->mapped)
		return;
	struct video_data frame;
	memset(&frame, 0, sizeof(frame));
	for (size_t plane = 0; plane < tsw_gpu_road_planes(road); plane++) {
		frame.data[plane] = road->data[slot][plane];
		frame.linesize[plane] = road->linesize[slot][plane];
	}
	for (uint32_t repeat = 0; repeat < staged->count; repeat++) {
		frame.timestamp = staged->timestamp + repeat * road->interval_ns;
		road->receive(road->param, &frame);
	}
}

/* Our copier thread: woken when a slot is handed over, copies every slot ready, in order. */
static void *copier_main(void *param)
{
	struct tsw_gpu_road *road = param;
	os_set_thread_name("tsw-road-c");
	while (os_sem_wait(road->wake) == 0 && !os_atomic_load_bool(&road->quit)) {
		size_t slot;
		while (tsw_stage_ring_next(&road->ring, &slot)) {
			copy_slot(road, slot);
			tsw_stage_ring_done(&road->ring);
		}
	}
	return NULL;
}

struct tsw_gpu_road *tsw_gpu_road_create(const struct obs_video_info *ovi, tsw_gpu_road_receive receive, void *param)
{
	if (tsw_gpu_road_unsupported(ovi) != NULL || receive == NULL)
		return NULL;
	struct tsw_gpu_road *road = bzalloc(sizeof(*road));
	road->width = ovi->output_width;
	road->height = ovi->output_height;
	road->interval_ns = obs_get_frame_interval_ns();
	road->receive = receive;
	road->param = param;
	const char *failed = NULL;
	if (!describe_planes(road, ovi->output_format) || tsw_stage_ring_init(&road->ring, road->interval_ns) != TSW_OK)
		failed = "unsupported format";
	set_color_matrix(road, ovi);

	if (failed == NULL) {
		obs_enter_graphics();
		if (!find_effect(road))
			failed = "OBS's conversion effect was not found";
		else if (!create_surfaces(road))
			failed = "the graphics card memory could not be prepared";
		obs_leave_graphics();
	}
	if (failed == NULL && os_sem_init(&road->wake, 0) != 0)
		failed = "the copier's signal could not be created";
	if (failed == NULL && pthread_create(&road->copier, NULL, copier_main, road) != 0)
		failed = "the copier thread could not start";
	road->copier_started = failed == NULL;
	if (failed != NULL) {
		tsw_log(LOG_WARNING, "road C unavailable, road B used | cause=\"%s\"", failed);
		tsw_gpu_road_destroy(road);
		return NULL;
	}
	return road;
}

void tsw_gpu_road_begin(struct tsw_gpu_road *road)
{
	if (road == NULL || road->registered)
		return;
	obs_add_main_rendered_callback(tsw_gpu_road_rendered, road);
	road->registered = true;
}

void tsw_gpu_road_get_counts(const struct tsw_gpu_road *road, struct tsw_gpu_road_counts *counts)
{
	if (counts == NULL)
		return;
	memset(counts, 0, sizeof(*counts));
	if (road == NULL)
		return;
	counts->unstaged = os_atomic_load_long(&road->ring.unstaged);
	counts->unmapped = os_atomic_load_long(&road->ring.unmapped);
	counts->staged = os_atomic_load_long(&road->staged);
}

void tsw_gpu_road_destroy(struct tsw_gpu_road *road)
{
	if (road == NULL)
		return;
	/* 1. No more frames: the removal waits for a rendered callback in progress (OBS holds a mutex). */
	if (road->registered)
		obs_remove_main_rendered_callback(tsw_gpu_road_rendered, road);
	road->registered = false;

	/* 2. The copier ends after the slot it is copying. */
	if (road->copier_started) {
		os_atomic_set_bool(&road->quit, true);
		os_sem_post(road->wake);
		pthread_join(road->copier, NULL);
	}
	os_sem_destroy(road->wake);

	/* 3. Neither thread uses the ring now: unmap what is still mapped, then give the card memory back. */
	obs_enter_graphics();
	for (size_t slot = 0; slot < TSW_STAGE_SLOTS; slot++) {
		const bool mapped = tsw_stage_ring_still_mapped(&road->ring, slot);
		for (size_t plane = 0; plane < tsw_gpu_road_planes(road); plane++) {
			if (mapped)
				gs_stagesurface_unmap(road->surface[slot][plane]);
			gs_stagesurface_destroy(road->surface[slot][plane]);
		}
	}
	for (size_t plane = 0; plane < tsw_gpu_road_planes(road); plane++)
		gs_texture_destroy(road->texture[plane]);
	obs_leave_graphics(); /* the effect is OBS's own (cached by OBS): not destroyed here */
	bfree(road);
}
