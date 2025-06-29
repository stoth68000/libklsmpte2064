/* Not for inclusion by user applications */

#ifndef _LIBKLSMPTE2064_PRIVATE_H
#define _LIBKLSMPTE2064_PRIVATE_H

#include "klbitstream_readwriter.h"

#define MODULE_PREFIX "libklsmpte2064: "

struct tbl1_s
{
	int progressive;
	int width;
	int height;
	int pfcount;
	int prefilter[6];
};
const struct tbl1_s *lookupTable1(int progressive, int width, int height);

struct tbl2_s
{
	int progressive;
	int width;
	int height;
	int hstart;
	int hstep;
	int hstop;
	int vstart_f1;
	int vstart_f2;
	int vstep;
	int vstop_f1;
	int vstop_f2;
};
const struct tbl2_s *lookupTable2(int progressive, int width, int height);

struct tbl3_s
{
	double video_frame_rate;
	int decimator_factor;
	int bytes_per_x_frames[2];
	int bitrate_per_second;
};
const struct tbl3_s *lookupTable3(double video_frame_rate);

struct ctx_s
{
    int verbose;
    
	/* */
	uint32_t colorspace; // 1 = YUV420P, 2 = V210
	uint8_t *y; 
	uint32_t width;
	uint32_t height;
	uint32_t stride;
	uint32_t bitdepth;
    uint32_t progressive;

	const struct tbl1_s *t1;
	const struct tbl2_s *t2;
	const struct tbl3_s *t3;

    /* 5.2.2 Windowing Sub-Sampling */
#define WSS_ROWS 16
#define WSS_SAMPLES_PER_ROW 60
#define WSS_SAMPLES_PER_FRAME (WSS_ROWS * WSS_SAMPLES_PER_ROW)
    uint8_t wss_f4[WSS_ROWS][WSS_SAMPLES_PER_ROW]; /* 5.2.3.1 - figure 5 - frame or field we compare against */
    uint8_t wss_f3[WSS_ROWS][WSS_SAMPLES_PER_ROW]; /* 5.2.3.1 - figure 5 - basic unusued */
    uint8_t wss_f2[WSS_ROWS][WSS_SAMPLES_PER_ROW]; /* 5.2.3.1 - figure 5 - always the current frame */

    int per_pixel_motion_threshold;
    uint8_t video_fingerprint_data_f4; /* 5.2.3.2 */
    uint8_t video_fingerprint_data_f3; /* 5.2.3.2 */
    uint8_t video_fingerprint_data_f2; /* 5.2.3.2 */
    uint64_t fingerprints_calculated;
    //
    double motion;

	/* Audio */
	/* Max samples per frame = (1000 / 23.97) × 48 = 2002.5 */
	/* We'll pre-allocate sample buffers of audioMaxSampleCount = 2200 */
	int audioMaxSampleCount;
	float *bufA;
	float *Es;
	float *Ms;
	uint8_t *comp_bit;
	uint8_t *result;

	double framerate;

    /* Encapsulation */
    struct klbs_context_s *bs;
    uint8_t sequence_counter;
};

int klsmpte2064_audio_alloc(struct ctx_s *ctx);
void klsmpte2064_audio_free(struct ctx_s *ctx);

#endif /* _LIBKLSMPTE2064_PRIVATE_H */
