#include <libklsmpte2064/klsmpte2064.h>

#include "core-private.h"

#include <string.h>

static void fill_status(struct ctx_s *ctx,
	struct klsmpte2064_context_status *status)
{
	memset(status, 0, sizeof(*status));
	status->video_frames_pushed = ctx->fingerprints_calculated;
	status->video_ready = ctx->fingerprints_calculated >= 3;
	status->pack_ready = status->video_ready;
	status->timebase_num = ctx->timebase_num;
	status->timebase_den = ctx->timebase_den;
	status->sequence_counter = ctx->sequence_counter;
	status->motion = ctx->motion;

	for (int i = AUDIOTYPE_UNDEFINED + 1; i < AUDIOTYPE_MAX; i++) {
		if (klbs_get_byte_count(&ctx->fp_bs[i]) > 0) {
			status->audio_ready_mask |= 1u << i;
		}
	}
}

int klsmpte2064_context_status(void *hdl,
	struct klsmpte2064_context_status *status)
{
	struct ctx_s *ctx = (struct ctx_s *)hdl;
	if (!ctx || !status) {
		return -EINVAL;
	}

	fill_status(ctx, status);
	return 0;
}

int klsmpte2064_fingerprint_get(void *hdl,
	struct klsmpte2064_fingerprint *fingerprint)
{
	struct ctx_s *ctx = (struct ctx_s *)hdl;
	if (!ctx || !fingerprint) {
		return -EINVAL;
	}

	memset(fingerprint, 0, sizeof(*fingerprint));
	fill_status(ctx, &fingerprint->status);
	fingerprint->video_fingerprint = ctx->video_fingerprint_data_f4;

	for (int i = AUDIOTYPE_UNDEFINED + 1; i < AUDIOTYPE_MAX; i++) {
		uint32_t len = klbs_get_byte_count(&ctx->fp_bs[i]);
		if (len > KLSMPTE2064_AUDIO_FINGERPRINT_MAX_BYTES) {
			len = KLSMPTE2064_AUDIO_FINGERPRINT_MAX_BYTES;
		}
		fingerprint->audio_length[i] = (uint8_t)len;
		if (len > 0) {
			memcpy(fingerprint->audio[i], ctx->fp_buffer[i], len);
		}
	}

	return 0;
}
