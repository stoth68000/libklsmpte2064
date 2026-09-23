#include <libklsmpte2064/klsmpte2064.h>

#include "core-private.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *klsmpte2064_version_string(void)
{
	return VERSION;
}

void klsmpte2064_version(uint32_t *major, uint32_t *minor, uint32_t *patch)
{
	if (major) {
		*major = KLSMPTE2064_VERSION_MAJOR;
	}
	if (minor) {
		*minor = KLSMPTE2064_VERSION_MINOR;
	}
	if (patch) {
		*patch = KLSMPTE2064_VERSION_PATCH;
	}
}

uint32_t klsmpte2064_capabilities(void)
{
	return KLSMPTE2064_CAP_DIRECT_WSS_LUMA |
		KLSMPTE2064_CAP_WSS_EXTRACT_YUV420P |
		KLSMPTE2064_CAP_WSS_EXTRACT_V210 |
		KLSMPTE2064_CAP_RESET_APIS |
		KLSMPTE2064_CAP_FORMAT_PROBING;
}

static int context_alloc_common(void **hdl,
	enum klsmpte2064_colorspace_e colorspace,
	uint32_t progressive,
	uint32_t width,
	uint32_t height,
	uint32_t stride,
	uint32_t bitdepth,
	int direct_wss_luma)
{
	struct ctx_s *ctx = NULL;
	int ret = 0;

	if (!hdl) {
		return -EINVAL;
	}
	*hdl = NULL;

	if (!width || !height || progressive != 1) {
		return -EINVAL;
	}
	if (direct_wss_luma) {
		if (colorspace != COLORSPACE_UNDEFINED || stride != 0 || bitdepth != 8) {
			return -EINVAL;
		}
	} else {
		if (!colorspace || colorspace >= COLORSPACE_MAX ||
			!stride || (bitdepth != 8 && bitdepth != 10) ||
			(colorspace == COLORSPACE_YUV420P && bitdepth != 8) ||
			(colorspace == COLORSPACE_V210 && bitdepth != 10)) {
			return -EINVAL;
		}
	}

	ctx = calloc(1, sizeof(*ctx));
	if (!ctx) {
		return -ENOMEM;
	}

	ctx->colorspace = colorspace;
	ctx->direct_wss_luma = direct_wss_luma;
	ctx->width = width;
	ctx->height = height;
	ctx->bitdepth = bitdepth;
	ctx->inputstride = stride;
	ctx->progressive = progressive;
	ctx->per_pixel_motion_threshold = 32;
	ctx->audioMaxSampleCount = 2200;

	if (!direct_wss_luma) {
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
	}

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

int klsmpte2064_context_alloc(void **hdl,
	enum klsmpte2064_colorspace_e colorspace,
	uint32_t progressive,
	uint32_t width,
	uint32_t height,
	uint32_t stride,
	uint32_t bitdepth)
{
	return context_alloc_common(hdl,
		colorspace,
		progressive,
		width,
		height,
		stride,
		bitdepth,
		0);
}

int klsmpte2064_context_alloc_wss_luma(void **hdl,
	uint32_t progressive,
	uint32_t width,
	uint32_t height)
{
	return context_alloc_common(hdl,
		COLORSPACE_UNDEFINED,
		progressive,
		width,
		height,
		0,
		8,
		1);
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

int klsmpte2064_context_reset(void *hdl)
{
	struct ctx_s *ctx = (struct ctx_s *)hdl;
	if (!ctx) {
		return -EINVAL;
	}

	klsmpte2064_video_reset(hdl);
	for (int i = AUDIOTYPE_UNDEFINED + 1; i < AUDIOTYPE_MAX; i++) {
		klsmpte2064_audio_reset(hdl, (enum klsmpte2064_audio_type_e)i);
	}
	ctx->t3 = NULL;
	ctx->timebase_num = 0;
	ctx->timebase_den = 0;
	ctx->sequence_counter = 0;
	klbs_init(ctx->bs);
	return 0;
}
