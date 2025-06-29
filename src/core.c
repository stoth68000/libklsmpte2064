#include <libklsmpte2064/klsmpte2064.h>

#include "core-private.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int klsmpte2064_context_alloc(void **hdl,
	uint32_t colorspace,
	uint32_t progressive,
	uint32_t width,
	uint32_t height,
	uint32_t stride,
	uint32_t bitdepth)
{
	if (!colorspace || !width || !height || !stride || (bitdepth != 8 && bitdepth != 10) || progressive != 1) {
		return -EINVAL;
	}

	struct ctx_s *ctx = calloc(1, sizeof(*ctx));
	if (!ctx) {
		return -ENOMEM;
	}

	ctx->ystride = width;
	ctx->y = malloc(width * height);
	if (!ctx->y) {
		fprintf(stderr, MODULE_PREFIX "unable to allocate luma frame, aborting.\n");
		exit(0);
	}
	ctx->y_csc = malloc(width * height);
	if (!ctx->y_csc) {
		fprintf(stderr, MODULE_PREFIX "unable to allocate luma frame, aborting.\n");
		exit(0);
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
		free(ctx);
		return -EINVAL;
	}

	ctx->t2 = lookupTable2(progressive, width, height);
	if (!ctx->t2) {
		free(ctx);
		return -EINVAL;
	}

	ctx->bs = klbs_alloc();

	klsmpte2064_audio_alloc(ctx);

	*hdl = ctx;
	return 0; /* Success */
}

void klsmpte2064_context_free(void *hdl)
{
	struct ctx_s *ctx = (struct ctx_s *)hdl;

	klsmpte2064_audio_free(ctx);
	klbs_free(ctx->bs);
	free(ctx->y_csc);
	free(ctx->y);
	free(ctx);
}

int klsmpte2064_context_set_verbose(void *hdl, int level)
{
	struct ctx_s *ctx = (struct ctx_s *)hdl;
	ctx->verbose = level;
	return 0;
}
