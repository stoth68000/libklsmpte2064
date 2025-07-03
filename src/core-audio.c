#include <libklsmpte2064/klsmpte2064.h>

#include "core-private.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Functions that implement 5.3.2, 5.3.3, 5.3.4 and 5.3.5
 * could easily be folded into a single one loop functon.
 * I've decided not to do that because the performance gain
 * isn't massive, and the current implementation better
 * reflects the spec doc, and is easy to read side
 * by side for other developers.
 */

/* In practise, the envelope detection and mean modeling
 * as describing in the spec never worked well at all.
 * The mean was always too far below the ES and resulted
 * in fingerprints containing all 11111, because ms was always < es.
 *
 * I ended up modelling this in excel with real data so I could visualize
 * what what happening to the calculations over time, as I adjusted
 * ES_KM, ES_KE and MS_KM. It didn't matter how much I applied gain
 * or attenuation to the absolute samples, the calculations were
 * relatively too far part to be useful.
 * It didn't matter if I adusted KM/KE/KM to decay faster, it never
 * seemed to.
 * The algorithm as listed in the spec (now in the spreadsheet)
 * never actually produced useful voice envelope detection capabiltiies.
 *
 * So, I experimented with better IIR filters in excel and found
 * something that atleast worked. KM/KE/KM were replaced with a
 * simpled alpha and beta multiplier. Memory/smoothing and decay
 * was still modelled, by the MS and ES were much closer together
 * and I was able to reliably produce fingerprints for voice
 * input.
 * 
 * Your milage may vary. The original algorithm is left here,
 * feel free to compile it in and experiement. If you think you
 * disagree with the above, or can shine any light as to why
 * the SMPTE2064 algorithm wasn't working as predicted, please
 * contain me.
 * Adjust ORIGINAL_SPEC_IMPLEMENTATION to 1 for a faithful
 * implementation and try your hand at it.
 */

#define ORIGINAL_SPEC_IMPLEMENTATION 0

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
static void _audio_decimator(struct ctx_s *ctx, enum klsmpte2064_audio_type_e type, uint32_t sampleCount, uint8_t *comp_bit, uint8_t *result)
{
	/* remove any prev fp */
	memset(&ctx->fp_buffer[type][0], 0, sizeof(ctx->fp_buffer[type]));
	klbs_init(&ctx->fp_bs[type]);
	klbs_write_set_buffer(&ctx->fp_bs[type], &ctx->fp_buffer[type][0], sizeof(ctx->fp_buffer[type]));

	memset(result, 0, sampleCount);

	/* decimate envelope/mean comparison */
	for (uint32_t i = 0; i < sampleCount; i += ctx->t3->decimator_factor) {
		result[i / ctx->t3->decimator_factor] = comp_bit[i];
		klbs_write_bit(&ctx->fp_bs[type], comp_bit[i]);
//		printf("%3d: bit %3d: = %d\n", i, i / ctx->t3->decimator_factor, comp_bit[i]);
	}
	klbs_write_buffer_complete(&ctx->fp_bs[type]);
	//printf("fp bytes used %d\n", klbs_get_byte_count(&ctx->fp_bs[type]));
}

/* 5.3.5 - Envelope/Mean Comparator - on two mono buffers */
static void _audio_envelope_mean_comparator(struct ctx_s *ctx, uint32_t sampleCount, float *Es, float *Ms, uint8_t *comp_bit)
{
	/* In a reliable fingerprinting system, you expect Es to briefly rise above Ms, then
	 * fall below or near it during steady-state voice or silence.
	 */

	/* Extract fingerprint by comparing envelope ES with local mean Ms */
	for (uint32_t i = 0; i < sampleCount; i++) {
#if ORIGINAL_SPEC_IMPLEMENTATION
		if (Ms[i] < Es[i]) {
#else
		/* threshold margin, raise the mean. We can probably avoid doing
		 * by adjusting the beta level up in the mean detector.
		 */ 
		const float delta = 0.015f;
		if ((Ms[i] + delta) < Es[i]) {
#endif
			comp_bit[i] = 1;
		} else {
			comp_bit[i] = 0;
		}
	}
}

/* 5.3.4 - Local Mean Detector - on a mono buffer */
/* It calculates a smoothed representation of the input signal a_wav[] and stores
 * the result in Ms[], using a simple leaky integrator filter. The goal is to extract the local
 * mean energy over time, which can later be used in envelope
 * tracking as part of the audio fingerprinting process.
 */
static void _audio_local_mean_detector(struct ctx_s *ctx, uint32_t sampleCount, float *a_wav, float *Ms)
{
#if ORIGINAL_SPEC_IMPLEMENTATION
	/* is the IIR filter coefficient (decay factor). A large Km causes slower decay and thus a longer memory window. */
	float Km = 8192; // local mean detector IIR filter coefficient

	/* Ms[] is the local mean of the signal over time, with smoothing controlled by Km. */
	Ms[0] = 0; // initialize first value to a known state

	/* Local mean IIR filter */
	for (uint32_t i = 1; i < sampleCount; i++) {
		/* The equivalent to a simple IIR low-pass filter but uses decay by subtracting
		 * a fractional part of the current value (Ms[i-1]/Km, rounded down).
		 */
		Ms[i] = a_wav[i] + Ms[i - 1] - floorf(Ms[i - 1] / Km);
	}
#else
	/* is the IIR filter coefficient (decay factor). A large Km causes slower decay and thus a longer memory window. */
	const float beta = 0.005f;

	/* Ms[] is the local mean of the signal over time, with smoothing controlled by Km. */
	Ms[0] = a_wav[0];

	/* Local mean IIR filter */
	for (uint32_t i = 1; i < sampleCount; i++) {
		/* IIR low-pass filter. Ms[i] = scaled_energy_contribution + decay percentage of previous_value,
		 * do more averaging for the mean basically.
		 */
		Ms[i] = beta * a_wav[i] + (1.0f - beta) * Ms[i - 1];
	}
#endif
}

/* 5.3.3 - Envelope Detector - on a mono buffer */
/* To compute the envelope of the input audio waveform a_wav[] using an IIR low-pass filter.
 * The output is stored in the buffer Es[], which represents the energy envelope of the input waveform.
 * It tracks the amplitude of the signal (a_wav) over time.
 * It has fast attack (quickly reacts to rising amplitude) due to the multiplication with Km.
 * It has slow decay (envelope fades gradually when signal drops), controlled by Ke.
 * 
 * Es[] is a smoothed, rectified version of the input signal’s amplitude — the envelope — used in
 * audio fingerprinting to detect energy bursts, silence, or motion (per SMPTE ST 2064-1).
 */
static void _audio_envelope_detector(struct ctx_s *ctx, uint32_t sampleCount, float *a_wav, float *Es)
{
#if ORIGINAL_SPEC_IMPLEMENTATION
	/* scaling factor (sometimes called gain), effectively increasing the contribution of the current sample. */
	float Km = 1024; // local mean detector IIR filter coefficient

	/* time constant of the IIR filter; higher value means slower envelope decay.
	 * Smaller Ke → larger fraction subtracted → faster decay
	 * Larger Ke → smaller fraction subtracted → slower decay
	 */
	float Ke = 256; // Envelope detector IIR filter coefficient

	Es[0] = 0;

	for (uint32_t i = 1; i < sampleCount; i++) {
		/* Es[i] = current_energy_contribution + previous_value - decay_term */
		Es[i] = (a_wav[i] * Km / Ke) + Es[i - 1] - floorf(Es[i - 1] / Ke);
	}
#else
	/*
	 * The variable alpha is the smoothing factor (or time constant) in a single-pole Infinite Impulse Response (IIR) low-pass filter.
	 * It controls how quickly the envelope Es[] responds to changes in the input audio.
	 * | `alpha` Value              | Behavior                             | Effect                                    |
     * | -------------------------- | ------------------------------------ | ----------------------------------------- |
	 * | **Close to 1** (`0.8–1.0`) | Very fast response                   | Es follows input tightly (less smoothing) |
	 * | **Moderate** (`0.2–0.5`)   | Balanced response                    | Good for speech onsets                    |
	 * | **Small** (`< 0.1`)        | Very slow response (heavy smoothing) | Es changes slowly (lags behind input)     |
	 */

	/* alpha controls how “agile” the envelope is:
	 * Bigger alpha = reacts faster
	 * Smaller alpha = smooths more
	 * 
	 * If alpha = 0.25:
	 *   Each new value of Es[i] is 25% from the current input and 75% from the past.
	 *   The envelope will rise quickly with loud input, then decay gently when input drops.
	 */
	const float alpha = 0.25f; /* How quickly the solution adapts to energy change */

	Es[0] = a_wav[0];

	for (uint32_t i = 1; i < sampleCount; i++) {
		/* Es[i] = scaled_energy_contribution + decay percentage of previous_value */
		Es[i] = alpha * a_wav[i] + (1.0f - alpha) * Es[i - 1];
	}
#endif
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
	if (sampleCount > ctx->audioMaxSampleCount) {
		klsmpte2064_audio_free(ctx);
		ctx->audioMaxSampleCount = sampleCount;
		if (klsmpte2064_audio_alloc(ctx) < 0) {
			return -ENOMEM;
		}
	}
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

	/* Reset the fingerprint for the type */
	klbs_init(&ctx->fp_bs[type]);

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
	_audio_decimator(ctx, type, sampleCount, ctx->comp_bit, ctx->result);

	if (ctx->verbose) {
		printf("a fp: ");
		for (int i = 0; i < ctx->t3->decimator_factor; i++) {
			printf("%d", ctx->result[i]);
		}
		printf("\n");
	}

	if (ctx->verbose > 1) {
		int x = 24;

		printf("As: ");
		for (int i = 0; i < x; i++) {
			printf("% 6.4f ", ctx->bufA[i]);
		}
		printf("\n");
		printf("Es: ");
		for (int i = 0; i < x; i++) {
			printf("% 6.4f ", ctx->Es[i]);
		}
		printf("\n");
		printf("Ms: ");
		for (int i = 0; i < x; i++) {
			printf("% 6.4f ", ctx->Ms[i]);
		}
		printf("\n");
		printf("MS: ");
		for (int i = 0; i < x; i++) {
			printf("% 6.4f ", ctx->Ms[i] + 0.015f);
		}
		printf("\n");
		printf(" ?: ");
		for (int i = 0; i < x; i++) {
			printf("% 6.4f ", ctx->Es[i] - (ctx->Ms[i] + 0.015f));
		}
		printf("\n");
		printf("CB: ");
		for (int i = 0; i < x; i++) {
			printf("% 6.4f ", (float)ctx->comp_bit[i]);
		}
		printf("\n");
	}

	return 0;
}
