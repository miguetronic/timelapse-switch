/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * gpu-road-internal.h: what gpu-road.c and gpu-road-rt.c share. No other module includes it.
 */

#pragma once

#include <graphics/graphics.h>
#include <graphics/vec4.h>
#include <util/threading.h>

#include "core/stage-ring.h"
#include "gpu-road.h"

enum {
	TSW_GPU_PLANES = 3, /* at most: Y, U and V */
};

/* One plane of the output format, as OBS's own conversion writes it (libobs/obs.c). */
struct tsw_gpu_plane {
	const char *technique; /* in OBS's format_conversion.effect */
	uint32_t width;
	uint32_t height;
	enum gs_color_format format;
};

struct tsw_gpu_road {
	/* Fixed from create to destroy. */
	size_t planes;
	struct tsw_gpu_plane plane[TSW_GPU_PLANES];
	uint32_t width; /* of the main texture, which is also the output */
	uint32_t height;
	uint64_t interval_ns;
	struct vec4 color_vec[3]; /* OBS's color matrix rows, in the order OBS hands them to the shader */
	float width_i;            /* parameters of OBS's conversion for this format */
	float height_i;
	tsw_gpu_road_receive receive;
	void *param;

	/* Graphics thread only (UI thread while not registered). */
	gs_effect_t *effect; /* OBS's own, cached by OBS: never destroyed here */
	gs_eparam_t *image, *color_vec0, *color_vec1, *color_vec2, *width_i_param, *height_i_param;
	gs_eparam_t *sdr_white_nits_over_maximum, *hdr_lw;
	gs_texture_t *texture[TSW_GPU_PLANES];                    /* the converted frame, one per plane */
	gs_stagesurf_t *surface[TSW_STAGE_SLOTS][TSW_GPU_PLANES]; /* staging slots, one per plane */
	volatile long staged;                                     /* frames staged so far */

	/* Written by the graphics thread before a slot is published, read by the copier after. */
	uint8_t *data[TSW_STAGE_SLOTS][TSW_GPU_PLANES];
	uint32_t linesize[TSW_STAGE_SLOTS][TSW_GPU_PLANES];

	struct tsw_stage_ring ring; /* graphics thread to copier thread */

	/* The copier thread. */
	pthread_t copier;
	bool copier_started;
	os_sem_t *wake;     /* posted when a slot is handed over, and to stop */
	volatile bool quit; /* the copier ends at its next wake */
	bool registered;    /* the rendered callback is registered: UI thread only */
};

/* The rendered callback (gpu-road-rt.c): OBS calls it on its graphics thread after each canvas is rendered. */
void tsw_gpu_road_rendered(void *param);
