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
} tbl3[] = {
	{ 23.98, 52, { 77, 16 }, 923, },
	{ 29.97, 52, { 77, 20 }, 923, },
	{    50, 52, { 77, 32 }, 923, },
	{ 59.94, 52, { 77, 40 }, 923, },
	{    24, 50, { 80, 16 }, 960, },
	{    25, 50, { 96, 20 }, 960, },
	{    30, 50, { 80, 20 }, 960, },
	{    50, 50, { 96, 40 }, 960, },
	{    60, 50, { 80, 40 }, 960, },
};
const struct tbl3_s *lookupTable3(double video_frame_rate);

struct ctx_s
{
    int verbose;
    
	/* */
	uint8_t *y; 
	uint32_t width;
	uint32_t height;
	uint32_t stride;
	uint32_t bitdepth;
    uint32_t progressive;

	const struct tbl1_s *t1;
	const struct tbl2_s *t2;

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

    /* Encapsulation */
    struct klbs_context_s *bs;
    uint8_t sequence_counter;
};


#endif /* _LIBKLSMPTE2064_PRIVATE_H */
