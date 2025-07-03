#include <libklsmpte2064/klsmpte2064.h>

#include "core-private.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

struct tbl3_s tbl3[] = {
	{ 23.98, 52, { 77, 16 }, 923, 1001, 24000, },
	{ 29.97, 52, { 77, 20 }, 923, 1001, 30000, },
#if 0
	/* No support for 48 / 1.001 */
	{ 47.95, 52, { 77, 32 }, 923, },
#endif
	{ 59.94, 52, { 77, 40 }, 923, 1001, 60000, },
	{    24, 50, { 80, 16 }, 960,    1,    24, },
	{    25, 50, { 96, 20 }, 960,    1,    25, },
	{    30, 50, { 80, 20 }, 960,    1,    30, },
	{    50, 50, { 96, 40 }, 960,    1,    50, },
	{    60, 50, { 80, 40 }, 960,    1,    60, },
};

const struct tbl3_s *lookupTable3(double video_frame_rate)
{
	for (int i = 0; i < (sizeof(tbl3) / sizeof(struct tbl3_s)); i++) {
		struct tbl3_s *t = &tbl3[i];
		if (video_frame_rate == t->video_frame_rate) {
			return t;
		}
	}
	return NULL; /* Failed */
}

const struct tbl3_s *lookupTable3Timebase(uint32_t num, uint32_t den)
{
	for (int i = 0; i < (sizeof(tbl3) / sizeof(struct tbl3_s)); i++) {
		struct tbl3_s *t = &tbl3[i];
		if ((num == t->timebase_num) && (den == t->timebase_den)) {
			return t;
		}
	}
	return NULL; /* Failed */
}

static inline float pcm16_to_float(int16_t sample)
{
    return sample >= 0 ? (float)sample / 32767.0f : (float)sample / 32768.0f;
}

/* 5.3.6 - Decimator - on one mono buffer */
static void _audio_decimator(struct ctx_s *ctx, uint32_t sampleCount, uint8_t *comp_bit, uint8_t *result)
{
	/* decimate envelope/mean comparison */
	for (uint32_t i = 0; i < sampleCount; i += ctx->t3->decimator_factor) {
		result[i / ctx->t3->decimator_factor] = comp_bit[i];
	}
}

/* 5.3.5 - Envelope/Mean Comparator - on two mono buffers */
static void _audio_envelope_mean_comparator(struct ctx_s *ctx, uint32_t sampleCount, float *Es, float *Ms, uint8_t *comp_bit)
{
	/* Extract fingerprint by comparing envelope with local mean */
	for (uint32_t i = 0; i < sampleCount; i++) {
		if (Ms[i] < Es[i]) {
			comp_bit[i] = 1;
		} else {
			comp_bit[i] = 0;
		}
	}
}

/* 5.3.4 - Local Mean Detector - on a mono buffer */
static void _audio_local_mean_detector(struct ctx_s *ctx, uint32_t sampleCount, float *a_wav, float *Ms)
{
	float Km = 8192; // local mean detector IIR filter coefficient

	Ms[0] = 0; // initialize first value to a known state

	/* Local mean IIR filter */
	for (uint32_t i = 1; i < sampleCount; i++) {
		Ms[i] = a_wav[i] + Ms[i - 1] - floor(Ms[i - 1] / Km);
	}
}

/* 5.3.3 - Envelope Detector - on a mono buffer */
static void _audio_envelope_detector(struct ctx_s *ctx, uint32_t sampleCount, float *a_wav, float *Es)
{
	float Km = 8192; // local mean detector IIR filter coefficient
	float Ke = 1024; // Envelope detector IIR filter coefficient

	Es[0] = 0;

	for (uint32_t i = 1; i < sampleCount; i++) {
		Es[i] = (a_wav[i] * Km / Ke) + Es[i - 1] - floor(Es[i - 1] / Ke);
	}
}

/* 5.3.2 - Pseudo Absolute Value - on a mono buffer */
static void _audio_pseudo_abs_value(struct ctx_s *ctx, uint32_t sampleCount, float *a_wav)
{
	for (uint32_t i = 0; i < sampleCount; i++) {
		a_wav[i] = fabs(a_wav[i]);
	}
}

/* 5.3.1 - Downmix - convert from S16 to float in the working buffer */
static int _audio_downmix_stereo(struct ctx_s *ctx,	const int16_t *planes[], uint32_t planeCount,
	uint32_t sampleCount, float *buf)
{
	const int16_t *lft = (const int16_t *)planes[0];
	const int16_t *rgt = (const int16_t *)planes[1];

	for (uint32_t i = 0; i < sampleCount; i++) {

		float ls = pcm16_to_float(lft[i]);
		float rs = pcm16_to_float(rgt[i]);

		buf[i] = ((ls * 0.7071) + (rs * 0.7071)) / 2;
	}

	return 0;
}
static int _audio_downmix_decklink_interleaved_stereo(struct ctx_s *ctx, const int16_t *planes[], uint32_t planeCount,
	uint32_t sampleCount, float *buf)
{
	/* Take ch0 and ch1 into the analyzers */
	const int32_t *s = (const int32_t *)planes[0];
	int audioInputChannels = 16;

	for (uint32_t i = 0; i < sampleCount; i++) {
		/* Stride is 16 samples in decklink */
		float ls = pcm16_to_float(s[ (i * audioInputChannels) + 0] >> 16);
		float rs = pcm16_to_float(s[ (i * audioInputChannels) + 1] >> 16);

		buf[i] = ((ls * 0.7071) + (rs * 0.7071)) / 2;
	}

	return 0;
}

static int _audio_downmix(struct ctx_s *ctx, enum klsmpte2064_audio_type_e type,
	const int16_t *planes[], uint32_t planeCount, uint32_t sampleCount, float *buf)
{
	switch (type) {
	case AUDIOTYPE_STEREO_S16P:
		return _audio_downmix_stereo(ctx, planes, planeCount, sampleCount, buf);
	case AUDIOTYPE_STEREO_S32_CH16_DECKLINK:
		return _audio_downmix_decklink_interleaved_stereo(ctx, planes, planeCount, sampleCount, buf);
	default:
		return -EINVAL;
	}
}

int klsmpte2064_audio_alloc(struct ctx_s *ctx)
{
	ctx->bufA = malloc(ctx->audioMaxSampleCount * sizeof(float));
	if (!ctx->bufA) {
		return -ENOMEM;
	}
	ctx->Es = malloc(ctx->audioMaxSampleCount * sizeof(float));
	if (!ctx->Es) {
		return -ENOMEM;
	}
	ctx->Ms = malloc(ctx->audioMaxSampleCount * sizeof(float));
	if (!ctx->Ms) {
		return -ENOMEM;
	}
	ctx->comp_bit = malloc(ctx->audioMaxSampleCount * sizeof(uint8_t));
	if (!ctx->comp_bit) {
		return -ENOMEM;
	}
	ctx->result = malloc(ctx->audioMaxSampleCount * sizeof(uint8_t));
	if (!ctx->result) {
		return -ENOMEM;
	}

	return 0;
}

void klsmpte2064_audio_free(struct ctx_s *ctx)
{
	if (ctx->bufA) {
		free(ctx->bufA);
		ctx->bufA = NULL;
	}
	if (ctx->Es) {
		free(ctx->Es);
		ctx->Es = NULL;
	}
	if (ctx->Ms) {
		free(ctx->Ms);
		ctx->Ms = NULL;
	}
	if (ctx->comp_bit) {
		free(ctx->comp_bit);
		ctx->comp_bit = NULL;
	}
	if (ctx->result) {
		free(ctx->result);
		ctx->result = NULL;
	}
}

int klsmpte2064_audio_push(void *hdl, enum klsmpte2064_audio_type_e type,
	uint32_t timebase_num, uint32_t timebase_den,
	const int16_t *planes[], uint32_t planeCount, uint32_t sampleCount)
{
	struct ctx_s *ctx = (struct ctx_s *)hdl;
	if (!ctx || !planeCount || type >= AUDIOTYPE_MAX) {
		return -EINVAL;
	}
	for (int i = 0; i < planeCount; i++) {
		if (!planes[i]) {
			return -EINVAL;
		}
	}
	if (ctx->t3 == NULL) {
		ctx->t3 = lookupTable3Timebase(timebase_num, timebase_den);
		if (ctx->t3 == NULL) {
			return -EINVAL;
		}
		ctx->timebase_num = timebase_num;
		ctx->timebase_den = timebase_den;
	}

	/* Section 5.3 - Audio Fingerprint Generation */

	/* Step 5.3.1 - Downmix */
	if (_audio_downmix(ctx, type, planes, planeCount, sampleCount, ctx->bufA) < 0) {
		return -EINVAL;
	}

	/* Step 5.3.2 - Pseudo Absolute Value */
	_audio_pseudo_abs_value(ctx, sampleCount, ctx->bufA);

	/* Step 5.3.3 - Envelope Detector */
	_audio_envelope_detector(ctx, sampleCount, ctx->bufA, ctx->Es);

	/* Step 5.3.4 - Local Mean Detector */
	_audio_local_mean_detector(ctx, sampleCount, ctx->bufA, ctx->Ms);

	/* Step 5.3.5 - Envelope/Mean Comparator */
	_audio_envelope_mean_comparator(ctx, sampleCount, ctx->Es, ctx->Ms, ctx->comp_bit);

	/* Step 5.3.6 - Decimator */
	_audio_decimator(ctx, sampleCount, ctx->comp_bit, ctx->result);

	if (ctx->verbose) {
		printf("a fp: ");
		for (int i = 0; i < ctx->t3->decimator_factor; i++) {
			printf("%d", ctx->result[i]);
		}
		printf("\n");
	}

	return 0;
}
