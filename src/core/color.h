/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * color.h: the color math behind the dock's theme engine (docs/design/visual-design.md, 3.5).
 *
 * An OBS theme is a box of paint tubes the user picked. The dock never brings its own paint: it
 * mixes what it needs from the theme's colors, and before it paints one it holds it next to the
 * color underneath and checks that the eye can tell them apart (the contrast guard):
 *
 *   theme color --> [ ensure contrast against the surface below ] --> the color that is painted
 *
 * Pure math with no OBS or Qt dependency, unit tested. Colors are sRGB channels from 0 to 1.
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct tsw_color {
	double red; /* sRGB, 0 to 1 */
	double green;
	double blue;
};

/* tsw_color_from_rgb8: a color from 8-bit channels (0 to 255). Memory: none allocated. */
struct tsw_color tsw_color_from_rgb8(uint8_t red, uint8_t green, uint8_t blue);

/* tsw_color_mix: fg laid over bg at amount opacity (0 to 1), per sRGB channel. */
struct tsw_color tsw_color_mix(struct tsw_color fg, struct tsw_color bg, double amount);

/* tsw_color_luminance: WCAG 2 relative luminance, 0 (black) to 1 (white). */
double tsw_color_luminance(struct tsw_color color);

/* tsw_color_contrast: WCAG 2 contrast ratio, 1 (same color) to 21 (black on white). */
double tsw_color_contrast(struct tsw_color a, struct tsw_color b);

/*
 * tsw_color_ensure_contrast: color, or the nearest color of the same hue and saturation that
 * reaches target (1 to 21) against bg. Lightness moves toward white on a dark bg and toward black
 * on a light one, in 1 % steps; the result is rounded to 8-bit channels before it is measured, so
 * what is painted is what was checked. Returns fallback if no step reaches target.
 * Memory: none allocated.
 */
struct tsw_color tsw_color_ensure_contrast(struct tsw_color color, struct tsw_color bg, double target,
					   struct tsw_color fallback);

#ifdef __cplusplus
}
#endif
