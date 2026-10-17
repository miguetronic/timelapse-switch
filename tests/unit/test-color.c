/*
 * Timelapse Switch
 * SPDX-FileCopyrightText: 2026 Miguel Parra (Miguetronic)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * test-color.c: the dock's color math (core/color.h, docs/design/visual-design.md 3.5).
 * Expected values are WCAG 2 figures worked out by hand, or the token values of the design
 * document, which were computed by a separate script.
 */

#include <math.h>

#include "core/color.h"
#include "test.h"

static const struct tsw_color BLACK = {0.0, 0.0, 0.0};
static const struct tsw_color WHITE = {1.0, 1.0, 1.0};

static void expect_near(double actual, double expected, double tolerance, int line)
{
	if (fabs(actual - expected) > tolerance) {
		printf("%s:%d: expected %.4f, got %.4f\n", __FILE__, line, expected, actual);
		tsw_test_failures++;
	}
}
#define EXPECT_NEAR(actual, expected, tolerance) expect_near(actual, expected, tolerance, __LINE__)

/* Compares with a #RRGGBB value, channel by channel, as 8-bit numbers. */
static void expect_rgb(struct tsw_color actual, unsigned expected, int line)
{
	const unsigned red = (unsigned)lround(actual.red * 255.0);
	const unsigned green = (unsigned)lround(actual.green * 255.0);
	const unsigned blue = (unsigned)lround(actual.blue * 255.0);
	const unsigned got = red << 16 | green << 8 | blue;
	if (got != expected) {
		printf("%s:%d: expected #%06X, got #%06X\n", __FILE__, line, expected, got);
		tsw_test_failures++;
	}
}
#define EXPECT_RGB(actual, expected) expect_rgb(actual, expected, __LINE__)

static void black_on_white_is_21_to_1_either_way(void)
{
	EXPECT_NEAR(tsw_color_contrast(BLACK, WHITE), 21.0, 1e-9);
	EXPECT_NEAR(tsw_color_contrast(WHITE, BLACK), 21.0, 1e-9);
	EXPECT_NEAR(tsw_color_contrast(WHITE, WHITE), 1.0, 1e-9);
}

/* Mid grey #808080: ((128 / 255 + 0.055) / 1.055) ^ 2.4 = 0.2159, worked out by hand. */
static void luminance_follows_the_srgb_curve(void)
{
	EXPECT_NEAR(tsw_color_luminance(tsw_color_from_rgb8(0x80, 0x80, 0x80)), 0.2159, 1e-4);
	EXPECT_NEAR(tsw_color_luminance(WHITE), 1.0, 1e-9);
	/* Below 0.04045 the curve is a straight line: 10 / 255 / 12.92 = 0.003035. */
	EXPECT_NEAR(tsw_color_luminance(tsw_color_from_rgb8(10, 10, 10)), 0.003035, 1e-6);
}

static void mix_lays_one_color_over_another_per_channel(void)
{
	const struct tsw_color quarter = tsw_color_mix(WHITE, BLACK, 0.25);
	EXPECT_NEAR(quarter.red, 0.25, 1e-9);
	EXPECT_NEAR(quarter.green, 0.25, 1e-9);
	EXPECT_NEAR(quarter.blue, 0.25, 1e-9);
	/* Default's surface: white at 6 % over #272A33 (design document, 3.5). */
	EXPECT_RGB(tsw_color_mix(WHITE, tsw_color_from_rgb8(0x27, 0x2A, 0x33), 0.06), 0x34373F);
}

static void a_color_that_already_passes_is_kept(void)
{
	const struct tsw_color red = tsw_color_from_rgb8(0xC0, 0x1C, 0x37);
	EXPECT_RGB(tsw_color_ensure_contrast(red, WHITE, 3.0, BLACK), 0xC01C37);
}

/* Light: its pale highlight #8CB5FF on the panel #E5E5E5 gets darker, keeping its blue (3.5). */
static void on_a_light_panel_a_color_gets_darker(void)
{
	const struct tsw_color panel = tsw_color_from_rgb8(0xE5, 0xE5, 0xE5);
	const struct tsw_color accent =
		tsw_color_ensure_contrast(tsw_color_from_rgb8(0x8C, 0xB5, 0xFF), panel, 3.0, BLACK);
	EXPECT_RGB(accent, 0x357DFF);
	TSW_EXPECT_TRUE(tsw_color_contrast(accent, panel) >= 3.0);
}

/* Classic: its dark highlight #19344C on the panel #3A393A gets lighter (3.5). */
static void on_a_dark_panel_a_color_gets_lighter(void)
{
	const struct tsw_color panel = tsw_color_from_rgb8(0x3A, 0x39, 0x3A);
	const struct tsw_color accent =
		tsw_color_ensure_contrast(tsw_color_from_rgb8(0x19, 0x34, 0x4C), panel, 3.0, WHITE);
	EXPECT_RGB(accent, 0x4688C2);
	TSW_EXPECT_TRUE(tsw_color_contrast(accent, panel) >= 3.0);
}

/* Text needs 4.5:1; the guard takes the first step that reaches it, not a far one. */
static void the_guard_stops_at_the_first_color_that_passes(void)
{
	const struct tsw_color panel = tsw_color_from_rgb8(0x27, 0x2A, 0x33);
	const struct tsw_color ink =
		tsw_color_ensure_contrast(tsw_color_from_rgb8(0x28, 0x4C, 0xB8), panel, 4.5, WHITE);
	const double contrast = tsw_color_contrast(ink, panel);
	TSW_EXPECT_TRUE(contrast >= 4.5 && contrast < 4.9);
}

/* No grey reaches 21:1 against mid grey: the fallback comes back. */
static void an_impossible_target_returns_the_fallback(void)
{
	const struct tsw_color grey = tsw_color_from_rgb8(0x80, 0x80, 0x80);
	const struct tsw_color fallback = tsw_color_from_rgb8(0x12, 0x34, 0x56);
	EXPECT_RGB(tsw_color_ensure_contrast(grey, grey, 21.0, fallback), 0x123456);
}

int main(void)
{
	black_on_white_is_21_to_1_either_way();
	luminance_follows_the_srgb_curve();
	mix_lays_one_color_over_another_per_channel();
	a_color_that_already_passes_is_kept();
	on_a_light_panel_a_color_gets_darker();
	on_a_dark_panel_a_color_gets_lighter();
	the_guard_stops_at_the_first_color_that_passes();
	an_impossible_target_returns_the_fallback();
	return tsw_test_failures;
}
