#include <libklsmpte2064/klsmpte2064.h>

#include "core-private.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

static int _video_prefilter(struct ctx_s *ctx, const uint8_t *luma);
static int _video_window_subsampling_progressive(struct ctx_s *ctx);
static int _video_window_compute_motion(struct ctx_s *ctx);

/* Table 1 - Video Format Prefilter */
struct tbl1_s tbl1[] = {
	{ 1, 4096, 2160, 6, { -3, -2, -1,  0,  1,  2 } },
	{ 1, 3840, 2160, 6, { -3, -2, -1,  0,  1,  2 } },
	{ 1, 2048, 1080, 3, { -1,  0,  1,  0,  0,  0 } },
	{ 0, 1920, 1080, 3, { -1,  0,  1,  0,  0,  0 } },
	{ 1, 1920, 1080, 3, { -1,  0,  1,  0,  0,  0 } },
	{ 1, 1280,  720, 2, { -1,  0,  0,  0,  0,  0 } },
	{ 0,  720,  485, 0, {  0,  0,  0,  0,  0,  0 } },
	{ 1,  720,  576, 0, {  0,  0,  0,  0,  0,  0 } },
};

/* Table 2 - Windowing Coordinates per video format.
 * The subset of pixels shall consist of 16 sample rows of 60 pixel samples,
 * evenly spaced horizontally and vertically across the selected window. This yields
 * a total of 960 pixels that are used in the motion detection process for
 * fingerprint generation.
 */
struct tbl2_s tbl2[] = {
	{ 0,  720,  485, 123,  8,  595,  60, 323, 10,  210,  473, },
	{ 0,  720,  576, 123,  8,  595,  68, 381, 12,  248,  561, },
	{ 1, 1280,  720, 256, 13, 1023, 117,  -1, 32,  597,   -1, },
	{ 0, 1920, 1080, 399, 19, 1520,  89, 652, 24,  449, 1012, },
	{ 1, 1920, 1080, 399, 19, 1520, 178,  -1, 48,  898,   -1, },
	{ 1, 3840, 2160, 798, 38, 3040, 412,  -1, 92, 1792,   -1, },
	{ 1, 2048, 1080, 463, 19, 1584, 206,  -1, 46,  896,   -1, },
	{ 1, 4096, 2160, 926, 38, 3168, 412,  -1, 92, 1792,   -1, },
};

/*

Section 5.2 "Video Fingerprint Generation".

Fields or frames shall be processed sequentially.

Stages:

Pre-filter -> windowing and sub-sampling -> motion detection.

Step 1: Pre-filter

Prior to sampling, the video shall be filtered to reduce its bandwidth and to facilitate consistent results with
different video formats. Understand table 1 better.

Step 2: Windowing and Sub-Sampling

"a window is defined to focus the fingerprint generation on the
central area of the image and reduce the impact of such possible alterations to the video"
"The number of pixels in the window also is reduced by a sub-sampling process."

Step 3: Motion Detection

"The difference in the video content between the current and
a prior video field/frame shall be used to calculate a video fingerprint"

"5.2. Only the 8 most significant bits of the luminance samples shall be used in the calculation."

*/

const struct tbl1_s *lookupTable1(int progressive, int width, int height)
{
	for (int i = 0; i < (sizeof(tbl1) / sizeof(struct tbl1_s)); i++) {
		struct tbl1_s *t = &tbl1[i];

		if (progressive == t->progressive && width == t->width && height == t->height) {
			return t;
		}
	}

	return NULL; /* Failed */
}

const struct tbl2_s *lookupTable2(int progressive, int width, int height)
{
	for (int i = 0; i < (sizeof(tbl2) / sizeof(struct tbl2_s)); i++) {
		struct tbl2_s *t = &tbl2[i];

		if (progressive == t->progressive && width == t->width && height == t->height) {
			return t;
		}
	}

	return NULL; /* Failed */
}

int klsmpte2064_video_push(void *hdl, const uint8_t *lumaplane)
{
	struct ctx_s *ctx = (struct ctx_s *)hdl;
	if (!ctx || !lumaplane) {
		return -EINVAL;
	}

	/* Step 1: pre-filter */
	/* "The current field/frame shall be compared with the second preceding
	 * field/frame to calculate a difference used for further processing."
	 */
	int r = _video_prefilter(ctx, lumaplane);
	if (r < 0) {
		return -1;
	}

	/* Step 2: windowing */
	r = _video_window_subsampling_progressive(ctx);
	if (r < 0) {
		return -1;
	}

	/* Step 3: motion detect */
	r = _video_window_compute_motion(ctx);
	if (r < 0) {
		return -1;
	}

	return 0;
}

/* Clone the luma plane into our content, and apply -3-2/-1 prefilters
 * per format during the process
 */
static int _video_prefilter(struct ctx_s *ctx, const uint8_t *luma)
{
	for (int h = 0; h < ctx->height; h++) {
		for (int w = 0; w < ctx->width; w++) {

			uint8_t *dstline = ctx->y + (ctx->stride * h);
			uint8_t *srcline = (uint8_t *)luma + (ctx->stride * h);
			
			if (ctx->t1->pfcount == 0) {

				/* No filtering at all, probably better to memcpy this */
				dstline[w] = srcline[w];

			} else {

				int sum = 0;
				int samples = 0;
				for (int i = 0; i < ctx->t1->pfcount; i++) {
					int xx = w + ctx->t1->prefilter[i];
					if (xx >= 0 && xx < ctx->width) {
						sum += srcline[xx];
						samples++;
					}
				}
				dstline[w] = (uint8_t)(sum / samples);

			}

		}
	}

	return 0;
}

/* See 5.2.2 and Figure 3 */
static int _video_window_subsampling_progressive(struct ctx_s *ctx)
{
	if (!ctx->progressive) {
		return -1;
	}

	/* Current to prior, we'll need this later in motion detection.
	 * We never really use f3 but its clear to read this way.
	 */
	memcpy(&ctx->wss_f2[0][0], &ctx->wss_f3[0][0], sizeof(ctx->wss_f3));
	memcpy(&ctx->wss_f3[0][0], &ctx->wss_f4[0][0], sizeof(ctx->wss_f4));

	/* Subsample the prefiltered luma into a windowed sub-sample area */
	int gridv = ctx->t2->vstart_f1;

	for (int r = 0; r < WSS_ROWS; r++) {
		int gridh = ctx->t2->hstart;
		uint8_t *srcline = (ctx->y + (gridv * ctx->stride));
		//printf(MODULE_PREFIX "gridv %4d: ", gridv);

		for (int c = 0; c < WSS_SAMPLES_PER_ROW; c++) {
			//printf(" %4d", gridh);

			ctx->wss_f4[r][c] = srcline[gridh];

			gridh += ctx->t2->hstep;
		}
		//printf("\n");
		gridv += ctx->t2->vstep;

	}

	return 0;
}

/* Compute motion magnitude between two subsampled frames.
 * 5.2.3.2 Pixel Counting
 * "a pixel shall be considered changed if the difference between the current pixel
 * and the same pixel in the previous field/frame is equal to or greater than 32 when
 * using 8-bit video samples"
 */
static int _video_window_compute_motion(struct ctx_s *ctx)
{
	int above_threshold = 0;

	for (int r = 0; r < WSS_ROWS; r++) {
		for (int c = 0; c < WSS_SAMPLES_PER_ROW; c++) {
			int diff = (int)ctx->wss_f4[r][c] - (int)ctx->wss_f2[r][c];
			int adiff = abs(diff);
			if (adiff > ctx->per_pixel_motion_threshold) {
				above_threshold++;
			}
		}
	}
	ctx->fingerprints_calculated++;

	/* "the count of
	 * changed pixels shall be divided by 4 to obtain a result varying from 0 to 240"
	 */
	ctx->video_fingerprint_data_f2 = ctx->video_fingerprint_data_f3;
	ctx->video_fingerprint_data_f3 = ctx->video_fingerprint_data_f4;
	ctx->video_fingerprint_data_f4 = above_threshold / 4;

	if (ctx->verbose) {
		printf(MODULE_PREFIX "frame %8" PRIu64 " - video fp 0x%02x, pixels are above threshold %3d/%3d\n",
			ctx->fingerprints_calculated,
			ctx->video_fingerprint_data_f4,
			above_threshold, WSS_SAMPLES_PER_FRAME);
	}

	/* Compute the motion, save this, might be a useful side metric. */
	ctx->motion = (double)above_threshold / (double)WSS_SAMPLES_PER_FRAME;

	return 0;
}
