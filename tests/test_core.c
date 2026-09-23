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

static int test_context_alloc_validation(void)
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

	hdl = NULL;
	EXPECT_EQ_INT(0,
		klsmpte2064_context_alloc(&hdl,
			COLORSPACE_YUV420P,
			1,
			1280,
			720,
			1280,
			8));
	EXPECT_TRUE(hdl != NULL);
	klsmpte2064_context_free(hdl);
	klsmpte2064_context_free(NULL);

	return 0;
}

static int test_video_pack_readiness(void)
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
	EXPECT_EQ_INT(0,
		klsmpte2064_context_alloc(&hdl,
			COLORSPACE_YUV420P,
			1,
			width,
			height,
			stride,
			8));

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

	klsmpte2064_context_free(hdl);
	free(frame);
	return 0;
}

static int test_audio_validation_and_push(void)
{
	void *hdl = NULL;
	enum { SAMPLE_COUNT = 800 };
	int16_t left[SAMPLE_COUNT] = {0};
	int16_t right[SAMPLE_COUNT] = {0};
	const int16_t *planes[2] = { left, right };
	const int16_t *bad_planes[2] = { left, NULL };

	EXPECT_EQ_INT(0,
		klsmpte2064_context_alloc(&hdl,
			COLORSPACE_YUV420P,
			1,
			1280,
			720,
			1280,
			8));

	EXPECT_EQ_INT(-EINVAL,
		klsmpte2064_audio_push(NULL,
			AUDIOTYPE_STEREO_S16P,
			1001,
			60000,
			planes,
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
			planes,
			2,
			SAMPLE_COUNT));

	EXPECT_EQ_INT(0,
		klsmpte2064_audio_push(hdl,
			AUDIOTYPE_STEREO_S16P,
			1001,
			60000,
			planes,
			2,
			SAMPLE_COUNT));

	klsmpte2064_context_free(hdl);
	return 0;
}

int main(void)
{
	int ret = 0;

	ret = test_context_alloc_validation();
	if (ret != 0) {
		return ret;
	}

	ret = test_video_pack_readiness();
	if (ret != 0) {
		return ret;
	}

	ret = test_audio_validation_and_push();
	if (ret != 0) {
		return ret;
	}

	return 0;
}
