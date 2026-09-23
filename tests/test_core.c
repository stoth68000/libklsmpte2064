#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libklsmpte2064/klsmpte2064.h>

#define EXPECT_TRUE(expr)                                                     \
	do {                                                                      \
		if (!(expr)) {                                                        \
			fprintf(stderr,                                                    \
				"%s:%d: expected true: %s\n",                                  \
				__FILE__,                                                     \
				__LINE__,                                                     \
				#expr);                                                       \
			return 1;                                                         \
		}                                                                     \
	} while (0)

#define EXPECT_EQ_INT(expected, actual)                                       \
	do {                                                                      \
		int expected_value = (expected);                                      \
		int actual_value = (actual);                                          \
		if (expected_value != actual_value) {                                 \
			fprintf(stderr,                                                    \
				"%s:%d: expected %d, got %d\n",                                \
				__FILE__,                                                     \
				__LINE__,                                                     \
				expected_value,                                                \
				actual_value);                                                 \
			return 1;                                                         \
		}                                                                     \
	} while (0)

#define EXPECT_EQ_U8(expected, actual)                                        \
	do {                                                                      \
		uint8_t expected_value = (uint8_t)(expected);                         \
		uint8_t actual_value = (uint8_t)(actual);                             \
		if (expected_value != actual_value) {                                 \
			fprintf(stderr,                                                    \
				"%s:%d: expected 0x%02x, got 0x%02x\n",                        \
				__FILE__,                                                     \
				__LINE__,                                                     \
				expected_value,                                                \
				actual_value);                                                 \
			return 1;                                                         \
		}                                                                     \
	} while (0)

static uint32_t pack_v210_word(uint32_t a, uint32_t b, uint32_t c)
{
	return (a & 0x3ff) | ((b & 0x3ff) << 10) | ((c & 0x3ff) << 20);
}

static int verify_checksum(const uint8_t *data, uint32_t used_length)
{
	uint32_t sum = 0;

	for (uint32_t i = 0; i < used_length; i++) {
		sum += data[i];
	}
	return (sum & 0xff) == 0;
}

static int alloc_yuv_context(void **hdl)
{
	return klsmpte2064_context_alloc(hdl,
		COLORSPACE_YUV420P,
		1,
		1280,
		720,
		1280,
		8);
}

static int test_context_api(void)
{
	void *hdl = (void *)0x1;

	EXPECT_EQ_INT(-EINVAL,
		klsmpte2064_context_alloc(NULL,
			COLORSPACE_YUV420P,
			1,
			1280,
			720,
			1280,
			8));

	EXPECT_EQ_INT(-EINVAL,
		klsmpte2064_context_alloc(&hdl,
			COLORSPACE_UNDEFINED,
			1,
			1280,
			720,
			1280,
			8));
	EXPECT_TRUE(hdl == NULL);

	hdl = (void *)0x1;
	EXPECT_EQ_INT(-EINVAL,
		klsmpte2064_context_alloc(&hdl,
			COLORSPACE_YUV420P,
			0,
			1280,
			720,
			1280,
			8));
	EXPECT_TRUE(hdl == NULL);

	hdl = (void *)0x1;
	EXPECT_EQ_INT(-EINVAL,
		klsmpte2064_context_alloc(&hdl,
			COLORSPACE_YUV420P,
			1,
			640,
			360,
			640,
			8));
	EXPECT_TRUE(hdl == NULL);

	hdl = (void *)0x1;
	EXPECT_EQ_INT(-EINVAL,
		klsmpte2064_context_alloc(&hdl,
			COLORSPACE_YUV420P,
			1,
			1280,
			720,
			1280,
			10));
	EXPECT_TRUE(hdl == NULL);

	hdl = NULL;
	EXPECT_EQ_INT(0, alloc_yuv_context(&hdl));
	EXPECT_TRUE(hdl != NULL);
	EXPECT_EQ_INT(0, klsmpte2064_context_set_verbose(hdl, 0));
	EXPECT_EQ_INT(0, klsmpte2064_context_set_verbose(hdl, 1));
	EXPECT_EQ_INT(-EINVAL, klsmpte2064_context_set_verbose(NULL, 1));
	klsmpte2064_context_free(hdl);
	klsmpte2064_context_free(NULL);

	hdl = NULL;
	EXPECT_EQ_INT(0,
		klsmpte2064_context_alloc(&hdl,
			COLORSPACE_V210,
			1,
			1280,
			720,
			1280 * 8 / 3,
			10));
	EXPECT_TRUE(hdl != NULL);
	klsmpte2064_context_free(hdl);

	return 0;
}

static int test_video_api_yuv420p_and_encapsulation(void)
{
	void *hdl = NULL;
	const uint32_t width = 1280;
	const uint32_t height = 720;
	const uint32_t stride = width;
	const size_t frame_size = (size_t)stride * height;
	uint8_t *frame = calloc(1, frame_size);
	uint8_t section[256] = {0};
	uint32_t used_length = 0;

	EXPECT_TRUE(frame != NULL);
	EXPECT_EQ_INT(0, alloc_yuv_context(&hdl));

	EXPECT_EQ_INT(-EINVAL, klsmpte2064_video_push(NULL, frame));
	EXPECT_EQ_INT(-EINVAL, klsmpte2064_video_push(hdl, NULL));
	EXPECT_EQ_INT(-EINVAL,
		klsmpte2064_encapsulation_pack(NULL,
			section,
			sizeof(section),
			&used_length));
	EXPECT_EQ_INT(-EINVAL,
		klsmpte2064_encapsulation_pack(hdl,
			NULL,
			sizeof(section),
			&used_length));
	EXPECT_EQ_INT(-EINVAL,
		klsmpte2064_encapsulation_pack(hdl,
			section,
			255,
			&used_length));
	EXPECT_EQ_INT(-EINVAL,
		klsmpte2064_encapsulation_pack(hdl,
			section,
			sizeof(section),
			NULL));
	EXPECT_EQ_INT(-ENODATA,
		klsmpte2064_encapsulation_pack(hdl,
			section,
			sizeof(section),
			&used_length));

	EXPECT_EQ_INT(0, klsmpte2064_video_push(hdl, frame));
	EXPECT_EQ_INT(0, klsmpte2064_video_push(hdl, frame));
	EXPECT_EQ_INT(-ENODATA,
		klsmpte2064_encapsulation_pack(hdl,
			section,
			sizeof(section),
			&used_length));

	memset(frame, 0xff, frame_size);
	EXPECT_EQ_INT(0, klsmpte2064_video_push(hdl, frame));
	EXPECT_EQ_INT(0,
		klsmpte2064_encapsulation_pack(hdl,
			section,
			sizeof(section),
			&used_length));
	EXPECT_TRUE(used_length > 0);
	EXPECT_EQ_INT(used_length, section[2]);
	EXPECT_TRUE(verify_checksum(section, used_length));

	klsmpte2064_context_free(hdl);
	free(frame);
	return 0;
}

static int test_video_api_v210(void)
{
	void *hdl = NULL;
	const uint32_t width = 1280;
	const uint32_t height = 720;
	const uint32_t stride = width * 8 / 3;
	const size_t frame_size = (size_t)stride * height;
	uint8_t *frame = calloc(1, frame_size);
	uint8_t section[256] = {0};
	uint32_t used_length = 0;

	EXPECT_TRUE(frame != NULL);
	EXPECT_EQ_INT(0,
		klsmpte2064_context_alloc(&hdl,
			COLORSPACE_V210,
			1,
			width,
			height,
			stride,
			10));

	EXPECT_EQ_INT(0, klsmpte2064_video_push(hdl, frame));
	EXPECT_EQ_INT(0, klsmpte2064_video_push(hdl, frame));
	memset(frame, 0xff, frame_size);
	EXPECT_EQ_INT(0, klsmpte2064_video_push(hdl, frame));
	EXPECT_EQ_INT(0,
		klsmpte2064_encapsulation_pack(hdl,
			section,
			sizeof(section),
			&used_length));
	EXPECT_TRUE(used_length > 0);
	EXPECT_TRUE(verify_checksum(section, used_length));

	klsmpte2064_context_free(hdl);
	free(frame);
	return 0;
}

static int test_audio_api_current_use_cases(void)
{
	void *hdl = NULL;
	enum { SAMPLE_COUNT = 800, DECKLINK_CHANNELS = 16 };
	int16_t left[SAMPLE_COUNT] = {0};
	int16_t right[SAMPLE_COUNT] = {0};
	int32_t decklink[SAMPLE_COUNT * DECKLINK_CHANNELS] = {0};
	const int16_t *stereo_planes[2] = { left, right };
	const int16_t *bad_planes[2] = { left, NULL };
	const int16_t *decklink_planes[1] = { (const int16_t *)decklink };

	for (int i = 0; i < SAMPLE_COUNT; i++) {
		left[i] = (int16_t)((i % 200) - 100);
		right[i] = (int16_t)(100 - (i % 200));
		decklink[(i * DECKLINK_CHANNELS) + 0] = ((int32_t)left[i]) << 16;
		decklink[(i * DECKLINK_CHANNELS) + 1] = ((int32_t)right[i]) << 16;
		decklink[(i * DECKLINK_CHANNELS) + 2] = ((int32_t)left[i] / 2) << 16;
		decklink[(i * DECKLINK_CHANNELS) + 4] = ((int32_t)right[i] / 2) << 16;
		decklink[(i * DECKLINK_CHANNELS) + 5] = ((int32_t)left[i] / 3) << 16;
	}

	EXPECT_EQ_INT(0, alloc_yuv_context(&hdl));

	EXPECT_EQ_INT(-EINVAL,
		klsmpte2064_audio_push(NULL,
			AUDIOTYPE_STEREO_S16P,
			1001,
			60000,
			stereo_planes,
			2,
			SAMPLE_COUNT));

	EXPECT_EQ_INT(-EINVAL,
		klsmpte2064_audio_push(hdl,
			AUDIOTYPE_STEREO_S16P,
			1001,
			60000,
			bad_planes,
			2,
			SAMPLE_COUNT));

	EXPECT_EQ_INT(-EINVAL,
		klsmpte2064_audio_push(hdl,
			AUDIOTYPE_STEREO_S16P,
			1,
			1000,
			stereo_planes,
			2,
			SAMPLE_COUNT));

	EXPECT_EQ_INT(0,
		klsmpte2064_audio_push(hdl,
			AUDIOTYPE_STEREO_S16P,
			1001,
			60000,
			stereo_planes,
			2,
			SAMPLE_COUNT));

	EXPECT_EQ_INT(0,
		klsmpte2064_audio_push(hdl,
			AUDIOTYPE_STEREO_S32_CH16_DECKLINK,
			1001,
			60000,
			decklink_planes,
			1,
			SAMPLE_COUNT));

	EXPECT_EQ_INT(0,
		klsmpte2064_audio_push(hdl,
			AUDIOTYPE_SMPTE312_S32_CH16_DECKLINK,
			1001,
			60000,
			decklink_planes,
			1,
			SAMPLE_COUNT));

	EXPECT_EQ_INT(-EINVAL,
		klsmpte2064_audio_push(hdl,
			AUDIOTYPE_MAX,
			1001,
			60000,
			stereo_planes,
			2,
			SAMPLE_COUNT));

	klsmpte2064_context_free(hdl);
	return 0;
}

static int test_csc_api(void)
{
	uint32_t src[8] = {0};
	uint16_t y10[12] = {0};
	uint8_t y8[12] = {0};
	int lines[1] = {1};
	const uint16_t expected_y10[12] = {
		100, 200, 300, 400, 500, 600,
		700, 800, 900, 1000, 1023, 0
	};
	const uint8_t expected_y8_line0[6] = {
		25, 50, 75, 100, 125, 150
	};
	const uint8_t expected_y8_line1[6] = {
		175, 200, 225, 250, 255, 0
	};

	src[0] = pack_v210_word(0, 100, 0);
	src[1] = pack_v210_word(200, 0, 300);
	src[2] = pack_v210_word(0, 400, 0);
	src[3] = pack_v210_word(500, 0, 600);
	src[4] = pack_v210_word(0, 700, 0);
	src[5] = pack_v210_word(800, 0, 900);
	src[6] = pack_v210_word(0, 1000, 0);
	src[7] = pack_v210_word(1023, 0, 0);

	v210_planar_unpack_c(src, 16, y10, 6, 6, 2);
	for (int i = 0; i < 12; i++) {
		EXPECT_EQ_INT(expected_y10[i], y10[i]);
	}

	memset(y8, 0, sizeof(y8));
	v210_planar_unpack_c_to_8b(src, 16, y8, 6, 6, 2, NULL, 0);
	for (int i = 0; i < 6; i++) {
		EXPECT_EQ_U8(expected_y8_line0[i], y8[i]);
		EXPECT_EQ_U8(expected_y8_line1[i], y8[6 + i]);
	}

	memset(y8, 0xaa, sizeof(y8));
	v210_planar_unpack_c_to_8b(src, 16, y8, 6, 6, 2, lines, 1);
	for (int i = 0; i < 6; i++) {
		EXPECT_EQ_U8(0xaa, y8[i]);
		EXPECT_EQ_U8(expected_y8_line1[i], y8[6 + i]);
	}

	return 0;
}

int main(void)
{
	int ret = 0;

	ret = test_context_api();
	if (ret != 0) {
		return ret;
	}

	ret = test_video_api_yuv420p_and_encapsulation();
	if (ret != 0) {
		return ret;
	}

	ret = test_video_api_v210();
	if (ret != 0) {
		return ret;
	}

	ret = test_audio_api_current_use_cases();
	if (ret != 0) {
		return ret;
	}

	ret = test_csc_api();
	if (ret != 0) {
		return ret;
	}

	return 0;
}
