#include <libklsmpte2064/klsmpte2064.h>

#include "core-private.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int klsmpte2064_context_alloc(void **hdl,
	enum klsmpte2064_colorspace_e colorspace,
	uint32_t progressive,
	uint32_t width,
	uint32_t height,
	uint32_t stride,
	uint32_t bitdepth)
{
	struct ctx_s *ctx = NULL;
	int ret = 0;

	if (!hdl) {
		return -EINVAL;
	}
	*hdl = NULL;

	if (!colorspace || colorspace >= COLORSPACE_MAX ||
		!width || !height || !stride ||
		(bitdepth != 8 && bitdepth != 10) || progressive != 1) {
		return -EINVAL;
	}
	if ((colorspace == COLORSPACE_YUV420P && bitdepth != 8) ||
		(colorspace == COLORSPACE_V210 && bitdepth != 10)) {
		return -EINVAL;
	}

	ctx = calloc(1, sizeof(*ctx));
	if (!ctx) {
		return -ENOMEM;
	}

	ctx->ystride = width;
	ctx->y = malloc(width * height);
	if (!ctx->y) {
		ret = -ENOMEM;
		goto fail;
	}
	ctx->y_csc = malloc(width * height);
	if (!ctx->y_csc) {
		ret = -ENOMEM;
		goto fail;
	}

	ctx->colorspace = colorspace;
	ctx->width = width;
	ctx->height = height;
	ctx->bitdepth = bitdepth;
	ctx->inputstride = stride;
	ctx->progressive = progressive;
	ctx->per_pixel_motion_threshold = 32;
	ctx->audioMaxSampleCount = 2200;

	ctx->t1 = lookupTable1(progressive, width, height);
	if (!ctx->t1) {
		ret = -EINVAL;
		goto fail;
	}

	ctx->t2 = lookupTable2(progressive, width, height);
	if (!ctx->t2) {
		ret = -EINVAL;
		goto fail;
	}

	/* Progressive only - cache a list of line numbers in each frame.
	 * used for large algorithm acceleration.
	 */
	int gridv = ctx->t2->vstart_f1;
	for (int r = 0; r < KLSMPTE2064_WSS_ROWS; r++) {
		ctx->wss_lines[r] = gridv;
		gridv += ctx->t2->vstep;
	}
	/* Only these rows can affect the windowed 960-sample fingerprint. Converting
	 * and prefiltering them preserves the resulting fingerprints while avoiding
	 * full-frame work on rows that are never sampled.
	 */
	ctx->wss_line_count = KLSMPTE2064_WSS_ROWS;
	ctx->bs = klbs_alloc();
	if (!ctx->bs) {
		ret = -ENOMEM;
		goto fail;
	}

	ret = klsmpte2064_audio_alloc(ctx);
	if (ret < 0) {
		goto fail;
	}

	*hdl = ctx;
	return 0; /* Success */

fail:
	klsmpte2064_context_free(ctx);
	return ret;
}

void klsmpte2064_context_free(void *hdl)
{
	struct ctx_s *ctx = (struct ctx_s *)hdl;
	if (!ctx) {
		return;
	}

	klsmpte2064_audio_free(ctx);
	klbs_free(ctx->bs);
	free(ctx->y_csc);
	free(ctx->y);
	free(ctx);
}

int klsmpte2064_context_set_verbose(void *hdl, int level)
{
	struct ctx_s *ctx = (struct ctx_s *)hdl;
	if (!ctx) {
		return -EINVAL;
	}
	ctx->verbose = level;
	return 0;
}
