#include <libklsmpte2064/klsmpte2064.h>

#include "core-private.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

struct tbl3_s tbl3[] = {
	{ 23.98, 52, { 77, 16 }, 923, },
	{ 29.97, 52, { 77, 20 }, 923, },
#if 0
	/* No support for 48 / 1.001 */
	{ 47.95, 52, { 77, 32 }, 923, },
#endif
	{ 59.94, 52, { 77, 40 }, 923, },
	{    24, 50, { 80, 16 }, 960, },
	{    25, 50, { 96, 20 }, 960, },
	{    30, 50, { 80, 20 }, 960, },
	{    50, 50, { 96, 40 }, 960, },
	{    60, 50, { 80, 40 }, 960, },
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

static inline float pcm16_to_float(int16_t sample)
{
    return sample >= 0 ? sample / 32767.0f : sample / 32768.0f;
}

static inline float pseudo_abs_float(float sample)
{
    return (sample >= 0.0f) ? sample : (-sample - (1.0f / 32768.0f));
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
		a_wav[i] = pseudo_abs_float(a_wav[i]);
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

static int _audio_downmix(struct ctx_s *ctx, enum klsmpte2064_audio_type_e type,
	const int16_t *planes[], uint32_t planeCount, uint32_t sampleCount, float *buf)
{
	switch (type) {
	case AUDIOTYPE_STEREO_S16P:
		return _audio_downmix_stereo(ctx, planes, planeCount, sampleCount, buf);
	default:
		return -EINVAL;
	}
}

int klsmpte2064_audio_push(void *hdl, enum klsmpte2064_audio_type_e type, double framerate, 
	const int16_t *planes[], uint32_t planeCount, uint32_t sampleCount)
{
	struct ctx_s *ctx = (struct ctx_s *)hdl;
	if (!ctx || !planeCount) {
		return -EINVAL;
	}

	for (int i = 0; i < planeCount; i++) {
		if (!planes[i]) {
			return -EINVAL;
		}
	}

	if (ctx->t3 == NULL) {
		ctx->framerate = framerate;
		ctx->t3 = lookupTable3(ctx->framerate);
		if (ctx->t3 == NULL) {
			return -EINVAL;
		}
	};

	/* We need a working mono buffer to transform the audio. */
	float *bufA = malloc(sampleCount * sizeof(float));
	if (!bufA) {
		return -ENOMEM;
	}
	float *Es = malloc(sampleCount * sizeof(float));
	if (!Es) {
		return -ENOMEM;
	}
	float *Ms = malloc(sampleCount * sizeof(float));
	if (!Ms) {
		return -ENOMEM;
	}
	uint8_t *comp_bit = malloc(sampleCount * sizeof(uint8_t));
	if (!comp_bit) {
		return -ENOMEM;
	}
	uint8_t *result = malloc(sampleCount * sizeof(uint8_t));
	if (!result) {
		return -ENOMEM;
	}

	/* Section 5.3 - Audio Fingerprint Generation */

	/* Step 5.3.1 - Downmix */
	if (_audio_downmix(ctx, type, planes, planeCount, sampleCount, bufA) < 0) {
		return -EINVAL;
	}

	/* Step 5.3.2 - Pseudo Absolute Value */
	_audio_pseudo_abs_value(ctx, sampleCount, bufA);

	/* Step 5.3.3 - Envelope Detector */
	_audio_envelope_detector(ctx, sampleCount, bufA, Es);

	/* Step 5.3.4 - Local Mean Detector */
	_audio_local_mean_detector(ctx, sampleCount, bufA, Ms);

	/* Step 5.3.5 - Envelope/Mean Comparator */
	_audio_envelope_mean_comparator(ctx, sampleCount, Es, Ms, comp_bit);

	/* Step 5.3.6 - Decimator */
	_audio_decimator(ctx, sampleCount, comp_bit, result);

	if (ctx->verbose) {
		printf("a fp: ");
		for (int i = 0; i < ctx->t3->decimator_factor; i++) {
			printf("%d", result[i]);
		}
		printf("\n");
	}

	free(result);
	free(comp_bit);
	free(Ms);
	free(Es);
	free(bufA);
	return 0;
}
