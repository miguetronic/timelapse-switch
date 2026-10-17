/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * color.c: see color.h. The formulas are WCAG 2's (relative luminance and contrast ratio) and the
 * usual HSL model, which lets the guard change a color's lightness while its hue stays the theme's.
 */

#include "core/color.h"

#include <math.h>
#include <stdbool.h>

#include "core/check.h"

/* WCAG 2: below this luminance, white text has more contrast than black (the two meet at 0.179). */
static const double DARK_LUMINANCE = 0.179;
static const double LIGHTNESS_STEP = 0.01; /* 1 %: small enough that the guard does not overshoot */
enum { MAX_LIGHTNESS_STEPS = 100 };        /* from any lightness, 100 steps reach white or black */

struct hsl {
	double hue; /* 0 to 1, a fraction of the color wheel */
	double saturation;
	double lightness;
};

static bool is_channel(double value)
{
	return value >= 0.0 && value <= 1.0;
}

static bool is_color(struct tsw_color color)
{
	return is_channel(color.red) && is_channel(color.green) && is_channel(color.blue);
}

/* Rounds each channel to 8 bits: the color a screen really shows, and the one the guard measures. */
static struct tsw_color quantize(struct tsw_color color)
{
	return (struct tsw_color){round(color.red * 255.0) / 255.0, round(color.green * 255.0) / 255.0,
				  round(color.blue * 255.0) / 255.0};
}

struct tsw_color tsw_color_from_rgb8(uint8_t red, uint8_t green, uint8_t blue)
{
	return (struct tsw_color){red / 255.0, green / 255.0, blue / 255.0};
}

struct tsw_color tsw_color_mix(struct tsw_color fg, struct tsw_color bg, double amount)
{
	if (!TSW_CHECK(is_color(fg) && is_color(bg)) || !TSW_CHECK(is_channel(amount)))
		return bg;
	const double rest = 1.0 - amount;
	return (struct tsw_color){fg.red * amount + bg.red * rest, fg.green * amount + bg.green * rest,
				  fg.blue * amount + bg.blue * rest};
}

/* One sRGB channel to linear light (WCAG 2). */
static double linear(double channel)
{
	return channel <= 0.04045 ? channel / 12.92 : pow((channel + 0.055) / 1.055, 2.4);
}

double tsw_color_luminance(struct tsw_color color)
{
	if (!TSW_CHECK(is_color(color)))
		return 0.0;
	return 0.2126 * linear(color.red) + 0.7152 * linear(color.green) + 0.0722 * linear(color.blue);
}

double tsw_color_contrast(struct tsw_color a, struct tsw_color b)
{
	const double first = tsw_color_luminance(a);
	const double second = tsw_color_luminance(b);
	const double lighter = first > second ? first : second;
	const double darker = first > second ? second : first;
	const double ratio = (lighter + 0.05) / (darker + 0.05);
	if (!TSW_CHECK(ratio >= 1.0 && ratio <= 21.0 + 1e-9))
		return 1.0;
	return ratio;
}

static struct hsl to_hsl(struct tsw_color color)
{
	const double high = fmax(color.red, fmax(color.green, color.blue));
	const double low = fmin(color.red, fmin(color.green, color.blue));
	struct hsl out = {0.0, 0.0, (high + low) / 2.0};
	const double range = high - low;
	if (range <= 0.0)
		return out; /* a grey: no hue, no saturation */
	out.saturation = out.lightness > 0.5 ? range / (2.0 - high - low) : range / (high + low);
	if (high == color.red)
		out.hue = (color.green - color.blue) / range + (color.green < color.blue ? 6.0 : 0.0);
	else if (high == color.green)
		out.hue = (color.blue - color.red) / range + 2.0;
	else
		out.hue = (color.red - color.green) / range + 4.0;
	out.hue /= 6.0;
	return out;
}

/* One channel of an HSL color: the hue is shifted by +1/3 for red, 0 for green, -1/3 for blue. */
static double hue_channel(double low, double high, double hue)
{
	if (hue < 0.0)
		hue += 1.0;
	if (hue > 1.0)
		hue -= 1.0;
	if (hue < 1.0 / 6.0)
		return low + (high - low) * 6.0 * hue;
	if (hue < 0.5)
		return high;
	if (hue < 2.0 / 3.0)
		return low + (high - low) * (2.0 / 3.0 - hue) * 6.0;
	return low;
}

static struct tsw_color from_hsl(struct hsl color)
{
	if (color.saturation <= 0.0)
		return (struct tsw_color){color.lightness, color.lightness, color.lightness};
	const double high = color.lightness < 0.5
				    ? color.lightness * (1.0 + color.saturation)
				    : color.lightness + color.saturation - color.lightness * color.saturation;
	const double low = 2.0 * color.lightness - high;
	return (struct tsw_color){hue_channel(low, high, color.hue + 1.0 / 3.0), hue_channel(low, high, color.hue),
				  hue_channel(low, high, color.hue - 1.0 / 3.0)};
}

struct tsw_color tsw_color_ensure_contrast(struct tsw_color color, struct tsw_color bg, double target,
					   struct tsw_color fallback)
{
	if (!TSW_CHECK(is_color(color) && is_color(bg) && is_color(fallback)) ||
	    !TSW_CHECK(target >= 1.0 && target <= 21.0))
		return fallback;
	if (tsw_color_contrast(color, bg) >= target)
		return color;

	/* 1. Which way: toward white on a dark surface, toward black on a light one. */
	const double direction = tsw_color_luminance(bg) < DARK_LUMINANCE ? 1.0 : -1.0;
	struct hsl shade = to_hsl(color);
	const double start = shade.lightness;

	/* 2. Same hue and saturation, one lightness step at a time, until the eye can tell them apart. */
	for (int step = 1; step <= MAX_LIGHTNESS_STEPS; step++) {
		shade.lightness = fmin(1.0, fmax(0.0, start + direction * LIGHTNESS_STEP * step));
		const struct tsw_color candidate = quantize(from_hsl(shade));
		if (tsw_color_contrast(candidate, bg) >= target)
			return candidate;
	}
	return fallback;
}
