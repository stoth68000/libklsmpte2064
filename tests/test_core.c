#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libklsmpte2064/klsmpte2064.h>

extern uint64_t klsmpte2064_test_allocation_count(void);

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

static const uint8_t GOLDEN_YUV_VIDEO_SECTION[] = {
	0x00, 0x00, 0x0b, 0x7e, 0xf8, 0xe2, 0x4b, 0x4c, 0xe9, 0xf0, 0x2d,
};
static const uint8_t GOLDEN_YUV_VIDEO_SECTION_SEQ2[] = {
	0x00, 0x01, 0x0b, 0x7e, 0xf8, 0xe2, 0x4b, 0x4c, 0xe9, 0xf0, 0x2c,
};
static const uint8_t GOLDEN_YUV_AUDIO_SECTION[] = {
	0x00, 0x00, 0x18, 0x7f, 0xf8, 0xe2, 0x4b, 0x4c,
	0xe9, 0xf0, 0x1a, 0x02, 0x17, 0x00, 0x00, 0x0a,
	0x17, 0x00, 0x00, 0x15, 0x17, 0x00, 0x00, 0x9f,
};
static const uint8_t GOLDEN_YUV_AUDIO_SECTION_SEQ2[] = {
	0x00, 0x01, 0x18, 0x7f, 0xf8, 0xe2, 0x4b, 0x4c,
	0xe9, 0xf0, 0x1a, 0x02, 0x17, 0x00, 0x00, 0x0a,
	0x17, 0x00, 0x00, 0x15, 0x17, 0x00, 0x00, 0x9e,
};

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

static uint32_t sample_sum(const uint8_t samples[KLSMPTE2064_WSS_ROWS]
	[KLSMPTE2064_WSS_SAMPLES_PER_ROW])
{
	uint32_t sum = 0;

	for (int r = 0; r < KLSMPTE2064_WSS_ROWS; r++) {
		for (int c = 0; c < KLSMPTE2064_WSS_SAMPLES_PER_ROW; c++) {
			sum += samples[r][c];
		}
	}
	return sum;
}

static uint8_t sample_xor(const uint8_t samples[KLSMPTE2064_WSS_ROWS]
	[KLSMPTE2064_WSS_SAMPLES_PER_ROW])
{
	uint8_t value = 0;

	for (int r = 0; r < KLSMPTE2064_WSS_ROWS; r++) {
		for (int c = 0; c < KLSMPTE2064_WSS_SAMPLES_PER_ROW; c++) {
			value ^= samples[r][c];
		}
	}
	return value;
}

static int expect_bytes(const uint8_t *expected,
	uint32_t expected_length,
	const uint8_t *actual,
	uint32_t actual_length)
{
	EXPECT_EQ_INT((int)expected_length, (int)actual_length);
	if (memcmp(expected, actual, expected_length) != 0) {
		fprintf(stderr, "byte mismatch\nexpected:");
		for (uint32_t i = 0; i < expected_length; i++) {
			fprintf(stderr, " %02x", expected[i]);
		}
		fprintf(stderr, "\nactual:  ");
		for (uint32_t i = 0; i < actual_length; i++) {
			fprintf(stderr, " %02x", actual[i]);
		}
		fprintf(stderr, "\n");
		return 1;
	}
	return 0;
}

static int alloc_context(void **hdl,
	enum klsmpte2064_colorspace_e colorspace,
	uint32_t width,
	uint32_t height,
	uint32_t stride,
	uint32_t bitdepth)
{
	return klsmpte2064_context_alloc(hdl,
		colorspace,
		1,
		width,
		height,
		stride,
		bitdepth);
}

static int alloc_yuv_context(void **hdl)
{
	return alloc_context(hdl, COLORSPACE_YUV420P, 1280, 720, 1280, 8);
}

static int alloc_wss_context(void **hdl)
{
	return klsmpte2064_context_alloc_wss_luma(hdl, 1, 1280, 720);
}

static int push_three_video_frames(void *hdl,
	uint8_t *frame,
	size_t frame_size)
{
	memset(frame, 0x00, frame_size);
	EXPECT_EQ_INT(0, klsmpte2064_video_push(hdl, frame));
	EXPECT_EQ_INT(0, klsmpte2064_video_push(hdl, frame));
	memset(frame, 0xff, frame_size);
	EXPECT_EQ_INT(0, klsmpte2064_video_push(hdl, frame));
	return 0;
}

static void fill_luma_pattern(uint8_t *frame,
	uint32_t width,
	uint32_t height,
	uint32_t stride,
	uint8_t seed)
{
	for (uint32_t y = 0; y < height; y++) {
		uint8_t *line = frame + (y * stride);
		for (uint32_t x = 0; x < width; x++) {
			line[x] = (uint8_t)((x * 3 + y * 5 + seed * 37) & 0xff);
		}
		for (uint32_t x = width; x < stride; x++) {
			line[x] = 0xee;
		}
	}
}

static void fill_v210_pattern(uint8_t *frame,
	uint32_t width,
	uint32_t height,
	uint32_t stride,
	uint8_t seed)
{
	for (uint32_t y = 0; y < height; y++) {
		uint8_t *line = frame + (y * stride);
		memset(line, 0xee, stride);
		for (uint32_t x = 0; x + 5 < width; x += 6) {
			uint32_t *dst = (uint32_t *)(line + ((x / 6) * 16));
			uint32_t y10[6] = {0};

			for (uint32_t i = 0; i < 6; i++) {
				y10[i] = (uint32_t)
					(((x + i) * 3 + y * 5 + seed * 37) & 0xff) << 2;
			}
			dst[0] = pack_v210_word(512, y10[0], 512);
			dst[1] = pack_v210_word(y10[1], 512, y10[2]);
			dst[2] = pack_v210_word(512, y10[3], 512);
			dst[3] = pack_v210_word(y10[4], 512, y10[5]);
		}
	}
}

static void fill_wss_samples(uint8_t samples[KLSMPTE2064_WSS_ROWS]
	[KLSMPTE2064_WSS_SAMPLES_PER_ROW],
	uint8_t value)
{
	for (int r = 0; r < KLSMPTE2064_WSS_ROWS; r++) {
		for (int c = 0; c < KLSMPTE2064_WSS_SAMPLES_PER_ROW; c++) {
			samples[r][c] = value;
		}
	}
}

static int push_three_wss_sample_frames(void *hdl)
{
	uint8_t samples[KLSMPTE2064_WSS_ROWS]
		[KLSMPTE2064_WSS_SAMPLES_PER_ROW] = {{0}};

	fill_wss_samples(samples, 0x00);
	EXPECT_EQ_INT(0, klsmpte2064_video_push_wss_luma(hdl, samples));
	EXPECT_EQ_INT(0, klsmpte2064_video_push_wss_luma(hdl, samples));
	fill_wss_samples(samples, 0xff);
	EXPECT_EQ_INT(0, klsmpte2064_video_push_wss_luma(hdl, samples));
	return 0;
}

static int pack_section(void *hdl,
	uint8_t *section,
	uint32_t section_size,
	uint32_t *used_length)
{
	memset(section, 0, section_size);
	*used_length = 0;
	return klsmpte2064_encapsulation_pack(hdl,
		section,
		section_size,
		used_length);
}

static int fill_audio_fixture(int16_t *left,
	int16_t *right,
	int32_t *decklink,
	int sample_count,
	int decklink_channels)
{
	for (int i = 0; i < sample_count; i++) {
		left[i] = (int16_t)((i % 200) - 100);
		right[i] = (int16_t)(100 - (i % 200));
		decklink[(i * decklink_channels) + 0] = ((int32_t)left[i]) << 16;
		decklink[(i * decklink_channels) + 1] = ((int32_t)right[i]) << 16;
		decklink[(i * decklink_channels) + 2] = ((int32_t)left[i] / 2) << 16;
		decklink[(i * decklink_channels) + 4] = ((int32_t)right[i] / 2) << 16;
		decklink[(i * decklink_channels) + 5] = ((int32_t)left[i] / 3) << 16;
	}
	return 0;
}

static int push_all_current_audio_types(void *hdl)
{
	enum { SAMPLE_COUNT = 800, DECKLINK_CHANNELS = 16 };
	int16_t left[SAMPLE_COUNT] = {0};
	int16_t right[SAMPLE_COUNT] = {0};
	int32_t decklink[SAMPLE_COUNT * DECKLINK_CHANNELS] = {0};
	const int16_t *stereo_planes[2] = {left, right};
	const int16_t *decklink_planes[1] = {(const int16_t *)decklink};

	fill_audio_fixture(left,
		right,
		decklink,
		SAMPLE_COUNT,
		DECKLINK_CHANNELS);

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
	return 0;
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
			COLORSPACE_MAX,
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

	hdl = (void *)0x1;
	EXPECT_EQ_INT(-EINVAL,
		klsmpte2064_context_alloc(&hdl,
			COLORSPACE_V210,
			1,
			1280,
			720,
			1280 * 8 / 3,
			8));
	EXPECT_TRUE(hdl == NULL);

	hdl = NULL;
	EXPECT_EQ_INT(0, alloc_yuv_context(&hdl));
	EXPECT_TRUE(hdl != NULL);
	EXPECT_EQ_INT(0, klsmpte2064_context_set_verbose(hdl, 0));
	EXPECT_EQ_INT(0, klsmpte2064_context_set_verbose(hdl, 1));
	EXPECT_EQ_INT(-EINVAL, klsmpte2064_context_set_verbose(NULL, 1));
	klsmpte2064_context_free(hdl);
	klsmpte2064_context_free(NULL);

	EXPECT_EQ_INT(-EINVAL, klsmpte2064_context_alloc_wss_luma(NULL,
		1,
		1280,
		720));

	hdl = (void *)0x1;
	EXPECT_EQ_INT(-EINVAL, klsmpte2064_context_alloc_wss_luma(&hdl,
		0,
		1280,
		720));
	EXPECT_TRUE(hdl == NULL);

	hdl = NULL;
	EXPECT_EQ_INT(0, alloc_wss_context(&hdl));
	EXPECT_TRUE(hdl != NULL);
	EXPECT_EQ_INT(0, klsmpte2064_context_set_verbose(hdl, 0));
	klsmpte2064_context_free(hdl);

	hdl = NULL;
	EXPECT_EQ_INT(0,
		alloc_context(&hdl,
			COLORSPACE_V210,
			1280,
			720,
			1280 * 8 / 3,
			10));
	EXPECT_TRUE(hdl != NULL);
	klsmpte2064_context_free(hdl);

	return 0;
}

static int test_version_capabilities_and_format_probing(void)
{
	uint32_t major = 0;
	uint32_t minor = 0;
	uint32_t patch = 0;
	uint32_t caps = 0;

	EXPECT_TRUE(klsmpte2064_version_string() != NULL);
	klsmpte2064_version(&major, &minor, &patch);
	EXPECT_EQ_INT(KLSMPTE2064_VERSION_MAJOR, (int)major);
	EXPECT_EQ_INT(KLSMPTE2064_VERSION_MINOR, (int)minor);
	EXPECT_EQ_INT(KLSMPTE2064_VERSION_PATCH, (int)patch);
	klsmpte2064_version(NULL, NULL, NULL);

	caps = klsmpte2064_capabilities();
	EXPECT_TRUE((caps & KLSMPTE2064_CAP_DIRECT_WSS_LUMA) != 0);
	EXPECT_TRUE((caps & KLSMPTE2064_CAP_WSS_EXTRACT_YUV420P) != 0);
	EXPECT_TRUE((caps & KLSMPTE2064_CAP_WSS_EXTRACT_V210) != 0);
	EXPECT_TRUE((caps & KLSMPTE2064_CAP_RESET_APIS) != 0);
	EXPECT_TRUE((caps & KLSMPTE2064_CAP_FORMAT_PROBING) != 0);
	EXPECT_TRUE((caps & KLSMPTE2064_CAP_STATUS_API) != 0);
	EXPECT_TRUE((caps & KLSMPTE2064_CAP_RAW_FINGERPRINT_API) != 0);
	EXPECT_TRUE((caps & KLSMPTE2064_CAP_ENCAPSULATION_METADATA) != 0);
	EXPECT_EQ_INT(1,
		klsmpte2064_capabilities_satisfy(
			KLSMPTE2064_GPU_DIRECT_WSS_REQUIRED_CAPABILITIES));
	EXPECT_EQ_INT(0, klsmpte2064_capabilities_satisfy(1u << 31));

	EXPECT_EQ_INT(1,
		klsmpte2064_video_format_supported(COLORSPACE_YUV420P,
			1,
			1280,
			720,
			8));
	EXPECT_EQ_INT(1,
		klsmpte2064_video_format_supported(COLORSPACE_V210,
			1,
			1920,
			1080,
			10));
	EXPECT_EQ_INT(1,
		klsmpte2064_video_wss_luma_format_supported(1, 3840, 2160));
	EXPECT_EQ_INT(0,
		klsmpte2064_video_format_supported(COLORSPACE_YUV420P,
			1,
			1280,
			720,
			10));
	EXPECT_EQ_INT(0,
		klsmpte2064_video_format_supported(COLORSPACE_V210,
			1,
			1920,
			1080,
			8));
	EXPECT_EQ_INT(0,
		klsmpte2064_video_format_supported(COLORSPACE_UNDEFINED,
			1,
			1280,
			720,
			8));
	EXPECT_EQ_INT(0,
		klsmpte2064_video_format_supported(COLORSPACE_YUV420P,
			0,
			1280,
			720,
			8));
	EXPECT_EQ_INT(0,
		klsmpte2064_video_wss_luma_format_supported(1, 640, 360));

	return 0;
}

static int test_yuv420p_golden_video_sections(void)
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
	EXPECT_EQ_INT(0, push_three_video_frames(hdl, frame, frame_size));

	EXPECT_EQ_INT(0, pack_section(hdl, section, sizeof(section), &used_length));
	EXPECT_TRUE(verify_checksum(section, used_length));
	EXPECT_EQ_INT(0,
		expect_bytes(GOLDEN_YUV_VIDEO_SECTION,
			sizeof(GOLDEN_YUV_VIDEO_SECTION),
			section,
			used_length));

	EXPECT_EQ_INT(0, pack_section(hdl, section, sizeof(section), &used_length));
	EXPECT_TRUE(verify_checksum(section, used_length));
	EXPECT_EQ_INT(0,
		expect_bytes(GOLDEN_YUV_VIDEO_SECTION_SEQ2,
			sizeof(GOLDEN_YUV_VIDEO_SECTION_SEQ2),
			section,
			used_length));

	klsmpte2064_context_free(hdl);
	free(frame);
	return 0;
}

static int test_yuv420p_padded_stride_golden_video_section(void)
{
	void *hdl = NULL;
	const uint32_t width = 1280;
	const uint32_t height = 720;
	const uint32_t stride = width + 64;
	const size_t frame_size = (size_t)stride * height;
	uint8_t *frame = calloc(1, frame_size);
	uint8_t section[256] = {0};
	uint32_t used_length = 0;

	EXPECT_TRUE(frame != NULL);
	EXPECT_EQ_INT(0,
		alloc_context(&hdl,
			COLORSPACE_YUV420P,
			width,
			height,
			stride,
			8));
	EXPECT_EQ_INT(0, push_three_video_frames(hdl, frame, frame_size));
	EXPECT_EQ_INT(0, pack_section(hdl, section, sizeof(section), &used_length));
	EXPECT_TRUE(verify_checksum(section, used_length));
	EXPECT_EQ_INT(0,
		expect_bytes(GOLDEN_YUV_VIDEO_SECTION,
			sizeof(GOLDEN_YUV_VIDEO_SECTION),
			section,
			used_length));

	klsmpte2064_context_free(hdl);
	free(frame);
	return 0;
}

static int test_encapsulation_metadata_api(void)
{
	void *hdl = NULL;
	struct klsmpte2064_encapsulation_metadata metadata = {0};
	struct klsmpte2064_encapsulation_metadata current = {0};
	uint8_t section[256] = {0};
	uint32_t used_length = 0;

	EXPECT_EQ_INT(-EINVAL,
		klsmpte2064_encapsulation_get_metadata(NULL, &current));
	EXPECT_EQ_INT(-EINVAL,
		klsmpte2064_encapsulation_get_metadata((void *)0x1, NULL));
	EXPECT_EQ_INT(-EINVAL,
		klsmpte2064_encapsulation_set_metadata(NULL, &metadata));
	EXPECT_EQ_INT(-EINVAL,
		klsmpte2064_encapsulation_set_metadata((void *)0x1, NULL));

	EXPECT_EQ_INT(0, alloc_wss_context(&hdl));
	EXPECT_EQ_INT(0, klsmpte2064_encapsulation_get_metadata(hdl, &current));
	EXPECT_EQ_INT(KLSMPTE2064_PICTURE_RATE_5994, current.picture_rate);
	EXPECT_EQ_INT(1, current.id_present);
	EXPECT_EQ_INT(2, current.id_length);
	EXPECT_EQ_U8('K', current.id_data[0]);
	EXPECT_EQ_U8('L', current.id_data[1]);

	metadata.picture_rate = KLSMPTE2064_PICTURE_RATE_60;
	metadata.id_present = 1;
	metadata.id_length = 4;
	metadata.id_data[0] = 'I';
	metadata.id_data[1] = 'R';
	metadata.id_data[2] = 'I';
	metadata.id_data[3] = 'S';
	EXPECT_EQ_INT(0, klsmpte2064_encapsulation_set_metadata(hdl, &metadata));
	EXPECT_EQ_INT(0, push_three_wss_sample_frames(hdl));
	EXPECT_EQ_INT(0, pack_section(hdl, section, sizeof(section), &used_length));
	EXPECT_TRUE(verify_checksum(section, used_length));
	EXPECT_EQ_INT(0x0d, section[2]);
	EXPECT_EQ_U8(0x8e, section[3]);
	EXPECT_EQ_U8(0xe4, section[5]);
	EXPECT_EQ_U8('I', section[6]);
	EXPECT_EQ_U8('R', section[7]);
	EXPECT_EQ_U8('I', section[8]);
	EXPECT_EQ_U8('S', section[9]);

	metadata.picture_rate = KLSMPTE2064_PICTURE_RATE_24;
	metadata.id_present = 0;
	metadata.id_length = 3;
	metadata.id_data[0] = 'B';
	metadata.id_data[1] = 'A';
	metadata.id_data[2] = 'D';
	EXPECT_EQ_INT(0, klsmpte2064_encapsulation_set_metadata(hdl, &metadata));
	EXPECT_EQ_INT(0, klsmpte2064_encapsulation_get_metadata(hdl, &current));
	EXPECT_EQ_INT(KLSMPTE2064_PICTURE_RATE_24, current.picture_rate);
	EXPECT_EQ_INT(0, current.id_present);
	EXPECT_EQ_INT(0, current.id_length);
	EXPECT_EQ_INT(0, pack_section(hdl, section, sizeof(section), &used_length));
	EXPECT_TRUE(verify_checksum(section, used_length));
	EXPECT_EQ_U8(0x2a, section[3]);

	metadata.picture_rate = 0x9;
	EXPECT_EQ_INT(-EINVAL, klsmpte2064_encapsulation_set_metadata(hdl, &metadata));
	metadata.picture_rate = KLSMPTE2064_PICTURE_RATE_24;
	metadata.id_present = 1;
	metadata.id_length = 0;
	EXPECT_EQ_INT(-EINVAL, klsmpte2064_encapsulation_set_metadata(hdl, &metadata));
	metadata.id_length = KLSMPTE2064_ENCAPSULATION_ID_MAX_BYTES + 1;
	EXPECT_EQ_INT(-EINVAL, klsmpte2064_encapsulation_set_metadata(hdl, &metadata));

	klsmpte2064_context_free(hdl);
	return 0;
}

static int test_wss_luma_golden_video_sections(void)
{
	void *hdl = NULL;
	uint8_t samples[KLSMPTE2064_WSS_ROWS]
		[KLSMPTE2064_WSS_SAMPLES_PER_ROW] = {{0}};
	uint8_t section[256] = {0};
	uint32_t used_length = 0;

	EXPECT_EQ_INT(0, alloc_wss_context(&hdl));

	EXPECT_EQ_INT(-EINVAL, klsmpte2064_video_push_wss_luma(NULL, samples));
	EXPECT_EQ_INT(-EINVAL, klsmpte2064_video_push_wss_luma(hdl, NULL));
	EXPECT_EQ_INT(-EINVAL, klsmpte2064_video_push(hdl, &samples[0][0]));
	EXPECT_EQ_INT(0, push_three_wss_sample_frames(hdl));

	EXPECT_EQ_INT(0, pack_section(hdl, section, sizeof(section), &used_length));
	EXPECT_TRUE(verify_checksum(section, used_length));
	EXPECT_EQ_INT(0,
		expect_bytes(GOLDEN_YUV_VIDEO_SECTION,
			sizeof(GOLDEN_YUV_VIDEO_SECTION),
			section,
			used_length));

	EXPECT_EQ_INT(0, pack_section(hdl, section, sizeof(section), &used_length));
	EXPECT_TRUE(verify_checksum(section, used_length));
	EXPECT_EQ_INT(0,
		expect_bytes(GOLDEN_YUV_VIDEO_SECTION_SEQ2,
			sizeof(GOLDEN_YUV_VIDEO_SECTION_SEQ2),
			section,
			used_length));

	klsmpte2064_context_free(hdl);
	return 0;
}

static int test_wss_geometry_api(void)
{
	void *hdl = NULL;
	struct klsmpte2064_video_wss_geometry geometry = {0};

	EXPECT_EQ_INT(-EINVAL,
		klsmpte2064_video_get_wss_geometry(NULL, &geometry));

	EXPECT_EQ_INT(0, alloc_yuv_context(&hdl));
	EXPECT_EQ_INT(-EINVAL, klsmpte2064_video_get_wss_geometry(hdl, NULL));
	EXPECT_EQ_INT(0, klsmpte2064_video_get_wss_geometry(hdl, &geometry));

	EXPECT_EQ_INT(KLSMPTE2064_WSS_ROWS, (int)geometry.row_count);
	EXPECT_EQ_INT(KLSMPTE2064_WSS_SAMPLES_PER_ROW,
		(int)geometry.samples_per_row);
	EXPECT_EQ_INT(2, (int)geometry.prefilter_tap_count);
	EXPECT_EQ_INT(-1, geometry.prefilter_offsets[0]);
	EXPECT_EQ_INT(0, geometry.prefilter_offsets[1]);
	EXPECT_EQ_INT(117, geometry.rows[0]);
	EXPECT_EQ_INT(149, geometry.rows[1]);
	EXPECT_EQ_INT(597, geometry.rows[KLSMPTE2064_WSS_ROWS - 1]);
	EXPECT_EQ_INT(256, geometry.columns[0]);
	EXPECT_EQ_INT(269, geometry.columns[1]);
	EXPECT_EQ_INT(1023,
		geometry.columns[KLSMPTE2064_WSS_SAMPLES_PER_ROW - 1]);
	EXPECT_EQ_INT(-EINVAL,
		klsmpte2064_video_extract_wss_luma_yuv420p(NULL,
			(uint8_t *)&geometry,
			1280,
			1280,
			(uint8_t (*)[KLSMPTE2064_WSS_SAMPLES_PER_ROW])
				&geometry));
	EXPECT_EQ_INT(-EINVAL,
		klsmpte2064_video_extract_wss_luma_v210(NULL,
			(uint8_t *)&geometry,
			1920,
			1920 * 8 / 3,
			(uint8_t (*)[KLSMPTE2064_WSS_SAMPLES_PER_ROW])
				&geometry));

	klsmpte2064_context_free(hdl);
	return 0;
}

static int test_reset_apis(void)
{
	void *hdl = NULL;
	uint8_t frame[1280 * 720] = {0};
	uint8_t section[256] = {0};
	uint32_t used_length = 0;

	EXPECT_EQ_INT(-EINVAL, klsmpte2064_video_reset(NULL));
	EXPECT_EQ_INT(-EINVAL,
		klsmpte2064_audio_reset(NULL, AUDIOTYPE_STEREO_S16P));
	EXPECT_EQ_INT(-EINVAL,
		klsmpte2064_context_reset(NULL));

	EXPECT_EQ_INT(0, alloc_yuv_context(&hdl));
	EXPECT_EQ_INT(-EINVAL,
		klsmpte2064_audio_reset(hdl, AUDIOTYPE_UNDEFINED));
	EXPECT_EQ_INT(0, push_three_video_frames(hdl, frame, sizeof(frame)));
	EXPECT_EQ_INT(0, pack_section(hdl, section, sizeof(section), &used_length));
	EXPECT_EQ_INT(0, klsmpte2064_video_reset(hdl));
	EXPECT_EQ_INT(-ENODATA,
		klsmpte2064_encapsulation_pack(hdl,
			section,
			sizeof(section),
			&used_length));

	EXPECT_EQ_INT(0, push_three_video_frames(hdl, frame, sizeof(frame)));
	EXPECT_EQ_INT(0, pack_section(hdl, section, sizeof(section), &used_length));
	EXPECT_EQ_INT(0,
		expect_bytes(GOLDEN_YUV_VIDEO_SECTION_SEQ2,
			sizeof(GOLDEN_YUV_VIDEO_SECTION_SEQ2),
			section,
			used_length));
	klsmpte2064_context_free(hdl);

	hdl = NULL;
	EXPECT_EQ_INT(0, alloc_yuv_context(&hdl));
	EXPECT_EQ_INT(0, push_three_video_frames(hdl, frame, sizeof(frame)));
	EXPECT_EQ_INT(0, push_all_current_audio_types(hdl));
	EXPECT_EQ_INT(0, pack_section(hdl, section, sizeof(section), &used_length));
	EXPECT_EQ_INT(0, klsmpte2064_context_reset(hdl));
	EXPECT_EQ_INT(-ENODATA,
		klsmpte2064_encapsulation_pack(hdl,
			section,
			sizeof(section),
			&used_length));
	EXPECT_EQ_INT(0, push_three_video_frames(hdl, frame, sizeof(frame)));
	EXPECT_EQ_INT(0, push_all_current_audio_types(hdl));
	EXPECT_EQ_INT(0, pack_section(hdl, section, sizeof(section), &used_length));
	EXPECT_EQ_INT(0,
		expect_bytes(GOLDEN_YUV_AUDIO_SECTION,
			sizeof(GOLDEN_YUV_AUDIO_SECTION),
			section,
			used_length));

	EXPECT_EQ_INT(0,
		klsmpte2064_audio_reset(hdl, AUDIOTYPE_STEREO_S16P));
	klsmpte2064_context_free(hdl);
	return 0;
}

static int test_status_and_raw_fingerprint_apis(void)
{
	void *hdl = NULL;
	struct klsmpte2064_context_status status = {0};
	struct klsmpte2064_fingerprint fingerprint = {0};
	uint8_t section[256] = {0};
	uint32_t used_length = 0;

	EXPECT_EQ_INT(-EINVAL, klsmpte2064_context_status(NULL, &status));
	EXPECT_EQ_INT(-EINVAL, klsmpte2064_context_status((void *)0x1, NULL));
	EXPECT_EQ_INT(-EINVAL, klsmpte2064_fingerprint_get(NULL, &fingerprint));
	EXPECT_EQ_INT(-EINVAL, klsmpte2064_fingerprint_get((void *)0x1, NULL));

	EXPECT_EQ_INT(0, alloc_wss_context(&hdl));
	EXPECT_EQ_INT(0, klsmpte2064_context_status(hdl, &status));
	EXPECT_EQ_INT(0, (int)status.video_frames_pushed);
	EXPECT_EQ_INT(0, (int)status.video_ready);
	EXPECT_EQ_INT(0, (int)status.pack_ready);
	EXPECT_EQ_INT(0, (int)status.audio_ready_mask);
	EXPECT_EQ_INT(0, (int)status.sequence_counter);

	EXPECT_EQ_INT(0, push_three_wss_sample_frames(hdl));
	EXPECT_EQ_INT(0, klsmpte2064_context_status(hdl, &status));
	EXPECT_EQ_INT(3, (int)status.video_frames_pushed);
	EXPECT_EQ_INT(1, (int)status.video_ready);
	EXPECT_EQ_INT(1, (int)status.pack_ready);
	EXPECT_EQ_INT(0, (int)status.sequence_counter);
	EXPECT_TRUE(status.motion == 1.0);

	EXPECT_EQ_INT(0, klsmpte2064_fingerprint_get(hdl, &fingerprint));
	EXPECT_EQ_INT(240, (int)fingerprint.video_fingerprint);
	EXPECT_EQ_INT(0, (int)fingerprint.audio_length[AUDIOTYPE_STEREO_S16P]);

	EXPECT_EQ_INT(0, push_all_current_audio_types(hdl));
	EXPECT_EQ_INT(0, klsmpte2064_fingerprint_get(hdl, &fingerprint));
	EXPECT_TRUE((fingerprint.status.audio_ready_mask &
		(1u << AUDIOTYPE_STEREO_S16P)) != 0);
	EXPECT_TRUE((fingerprint.status.audio_ready_mask &
		(1u << AUDIOTYPE_STEREO_S32_CH16_DECKLINK)) != 0);
	EXPECT_TRUE((fingerprint.status.audio_ready_mask &
		(1u << AUDIOTYPE_SMPTE312_S32_CH16_DECKLINK)) != 0);
	EXPECT_TRUE(fingerprint.audio_length[AUDIOTYPE_STEREO_S16P] > 0);
	EXPECT_TRUE(fingerprint.audio_length[AUDIOTYPE_STEREO_S32_CH16_DECKLINK] > 0);
	EXPECT_TRUE(fingerprint.audio_length[AUDIOTYPE_SMPTE312_S32_CH16_DECKLINK] > 0);

	EXPECT_EQ_INT(0, pack_section(hdl, section, sizeof(section), &used_length));
	EXPECT_EQ_INT(0, klsmpte2064_context_status(hdl, &status));
	EXPECT_EQ_INT(1, (int)status.sequence_counter);

	klsmpte2064_context_free(hdl);
	return 0;
}

static int test_wss_luma_matches_yuv420p_for_supported_dimensions(void)
{
	struct format_case {
		uint32_t width;
		uint32_t height;
	};
	const struct format_case cases[] = {
		{1280, 720},
		{1920, 1080},
		{2048, 1080},
		{3840, 2160},
		{4096, 2160},
	};

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		void *yuv_hdl = NULL;
		void *wss_hdl = NULL;
		struct klsmpte2064_video_wss_geometry geometry = {0};
		uint8_t samples[KLSMPTE2064_WSS_ROWS]
			[KLSMPTE2064_WSS_SAMPLES_PER_ROW] = {{0}};
		uint8_t yuv_section[256] = {0};
		uint8_t wss_section[256] = {0};
		uint32_t yuv_used_length = 0;
		uint32_t wss_used_length = 0;
		const uint32_t width = cases[i].width;
		const uint32_t height = cases[i].height;
		const uint32_t stride = width + 32;
		const size_t frame_size = (size_t)stride * height;
		uint8_t *frame = calloc(1, frame_size);

		EXPECT_TRUE(frame != NULL);
		EXPECT_EQ_INT(0,
			alloc_context(&yuv_hdl,
				COLORSPACE_YUV420P,
				width,
				height,
				stride,
				8));
		EXPECT_EQ_INT(0,
			klsmpte2064_context_alloc_wss_luma(&wss_hdl,
				1,
				width,
				height));
		EXPECT_EQ_INT(0,
			klsmpte2064_video_get_wss_geometry(wss_hdl, &geometry));

		for (uint8_t seed = 1; seed <= 3; seed++) {
			fill_luma_pattern(frame, width, height, stride, seed);
			EXPECT_EQ_INT(0, klsmpte2064_video_push(yuv_hdl, frame));
			EXPECT_EQ_INT(0,
				klsmpte2064_video_extract_wss_luma_yuv420p(&geometry,
					frame,
					width,
					stride,
					samples));
			EXPECT_EQ_INT(0,
				klsmpte2064_video_push_wss_luma(wss_hdl, samples));
		}

		EXPECT_EQ_INT(0,
			pack_section(yuv_hdl,
				yuv_section,
				sizeof(yuv_section),
				&yuv_used_length));
		EXPECT_EQ_INT(0,
			pack_section(wss_hdl,
				wss_section,
				sizeof(wss_section),
				&wss_used_length));
		EXPECT_TRUE(verify_checksum(yuv_section, yuv_used_length));
		EXPECT_TRUE(verify_checksum(wss_section, wss_used_length));
		EXPECT_EQ_INT(0,
			expect_bytes(yuv_section,
				yuv_used_length,
				wss_section,
				wss_used_length));

		klsmpte2064_context_free(yuv_hdl);
		klsmpte2064_context_free(wss_hdl);
		free(frame);
	}

	return 0;
}

static int test_wss_luma_matches_v210_for_supported_dimensions(void)
{
	struct format_case {
		uint32_t width;
		uint32_t height;
	};
	const struct format_case cases[] = {
		{1920, 1080},
		{3840, 2160},
	};

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		void *v210_hdl = NULL;
		void *wss_hdl = NULL;
		struct klsmpte2064_video_wss_geometry geometry = {0};
		uint8_t samples[KLSMPTE2064_WSS_ROWS]
			[KLSMPTE2064_WSS_SAMPLES_PER_ROW] = {{0}};
		uint8_t v210_section[256] = {0};
		uint8_t wss_section[256] = {0};
		uint32_t v210_used_length = 0;
		uint32_t wss_used_length = 0;
		const uint32_t width = cases[i].width;
		const uint32_t height = cases[i].height;
		const uint32_t stride = width * 8 / 3;
		const size_t frame_size = (size_t)stride * height;
		uint8_t *frame = calloc(1, frame_size);

		EXPECT_TRUE(frame != NULL);
		EXPECT_EQ_INT(0,
			alloc_context(&v210_hdl,
				COLORSPACE_V210,
				width,
				height,
				stride,
				10));
		EXPECT_EQ_INT(0,
			klsmpte2064_context_alloc_wss_luma(&wss_hdl,
				1,
				width,
				height));
		EXPECT_EQ_INT(0,
			klsmpte2064_video_get_wss_geometry(wss_hdl, &geometry));

		for (uint8_t seed = 1; seed <= 3; seed++) {
			fill_v210_pattern(frame, width, height, stride, seed);
			EXPECT_EQ_INT(0, klsmpte2064_video_push(v210_hdl, frame));
			EXPECT_EQ_INT(0,
				klsmpte2064_video_extract_wss_luma_v210(&geometry,
					frame,
					width,
					stride,
					samples));
			EXPECT_EQ_INT(0,
				klsmpte2064_video_push_wss_luma(wss_hdl, samples));
		}

		EXPECT_EQ_INT(0,
			pack_section(v210_hdl,
				v210_section,
				sizeof(v210_section),
				&v210_used_length));
		EXPECT_EQ_INT(0,
			pack_section(wss_hdl,
				wss_section,
				sizeof(wss_section),
				&wss_used_length));
		EXPECT_TRUE(verify_checksum(v210_section, v210_used_length));
		EXPECT_TRUE(verify_checksum(wss_section, wss_used_length));
		EXPECT_EQ_INT(0,
			expect_bytes(v210_section,
				v210_used_length,
				wss_section,
				wss_used_length));

		klsmpte2064_context_free(v210_hdl);
		klsmpte2064_context_free(wss_hdl);
		free(frame);
	}

	return 0;
}

static int test_gpu_sampler_reference_vector_1920x1080(void)
{
	void *hdl = NULL;
	struct klsmpte2064_video_wss_geometry geometry = {0};
	struct klsmpte2064_fingerprint fingerprint = {0};
	uint8_t samples[KLSMPTE2064_WSS_ROWS]
		[KLSMPTE2064_WSS_SAMPLES_PER_ROW] = {{0}};
	const uint32_t width = 1920;
	const uint32_t height = 1080;
	const uint32_t stride = width;
	const size_t frame_size = (size_t)stride * height;
	uint8_t *frame = calloc(1, frame_size);

	EXPECT_TRUE(frame != NULL);
	EXPECT_EQ_INT(0,
		klsmpte2064_context_alloc_wss_luma(&hdl, 1, width, height));
	EXPECT_EQ_INT(0, klsmpte2064_video_get_wss_geometry(hdl, &geometry));
	EXPECT_EQ_INT(178, geometry.rows[0]);
	EXPECT_EQ_INT(898, geometry.rows[KLSMPTE2064_WSS_ROWS - 1]);
	EXPECT_EQ_INT(399, geometry.columns[0]);
	EXPECT_EQ_INT(1520,
		geometry.columns[KLSMPTE2064_WSS_SAMPLES_PER_ROW - 1]);
	EXPECT_EQ_INT(3, (int)geometry.prefilter_tap_count);

	fill_luma_pattern(frame, width, height, stride, 1);
	EXPECT_EQ_INT(0,
		klsmpte2064_video_extract_wss_luma_yuv420p(&geometry,
			frame,
			width,
			stride,
			samples));
	EXPECT_EQ_U8(76, samples[0][0]);
	EXPECT_EQ_U8(133, samples[0][1]);
	EXPECT_EQ_U8(127, samples[15][59]);
	EXPECT_EQ_INT(122431, (int)sample_sum(samples));
	EXPECT_EQ_U8(87, sample_xor(samples));
	EXPECT_EQ_INT(0, klsmpte2064_video_push_wss_luma(hdl, samples));

	fill_luma_pattern(frame, width, height, stride, 2);
	EXPECT_EQ_INT(0,
		klsmpte2064_video_extract_wss_luma_yuv420p(&geometry,
			frame,
			width,
			stride,
			samples));
	EXPECT_EQ_U8(113, samples[0][0]);
	EXPECT_EQ_U8(170, samples[0][1]);
	EXPECT_EQ_U8(164, samples[15][59]);
	EXPECT_EQ_INT(122368, (int)sample_sum(samples));
	EXPECT_EQ_U8(12, sample_xor(samples));
	EXPECT_EQ_INT(0, klsmpte2064_video_push_wss_luma(hdl, samples));

	fill_luma_pattern(frame, width, height, stride, 3);
	EXPECT_EQ_INT(0,
		klsmpte2064_video_extract_wss_luma_yuv420p(&geometry,
			frame,
			width,
			stride,
			samples));
	EXPECT_EQ_U8(150, samples[0][0]);
	EXPECT_EQ_U8(207, samples[0][1]);
	EXPECT_EQ_U8(201, samples[15][59]);
	EXPECT_EQ_INT(122389, (int)sample_sum(samples));
	EXPECT_EQ_U8(15, sample_xor(samples));
	EXPECT_EQ_INT(0, klsmpte2064_video_push_wss_luma(hdl, samples));

	EXPECT_EQ_INT(0, klsmpte2064_fingerprint_get(hdl, &fingerprint));
	EXPECT_EQ_INT(234, (int)fingerprint.video_fingerprint);

	klsmpte2064_context_free(hdl);
	free(frame);
	return 0;
}

static int test_yuv420p_golden_audio_section(void)
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
	EXPECT_EQ_INT(0, push_three_video_frames(hdl, frame, frame_size));
	EXPECT_EQ_INT(0, push_all_current_audio_types(hdl));

	EXPECT_EQ_INT(0, pack_section(hdl, section, sizeof(section), &used_length));
	EXPECT_TRUE(verify_checksum(section, used_length));
	EXPECT_EQ_INT(0,
		expect_bytes(GOLDEN_YUV_AUDIO_SECTION,
			sizeof(GOLDEN_YUV_AUDIO_SECTION),
			section,
			used_length));

	EXPECT_EQ_INT(0, pack_section(hdl, section, sizeof(section), &used_length));
	EXPECT_TRUE(verify_checksum(section, used_length));
	EXPECT_EQ_INT(0,
		expect_bytes(GOLDEN_YUV_AUDIO_SECTION_SEQ2,
			sizeof(GOLDEN_YUV_AUDIO_SECTION_SEQ2),
			section,
			used_length));

	klsmpte2064_context_free(hdl);
	free(frame);
	return 0;
}

static int test_video_api_yuv420p_and_encapsulation_validation(void)
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
	EXPECT_EQ_INT(0, push_three_video_frames(hdl, frame, frame_size));
	EXPECT_EQ_INT(0, pack_section(hdl, section, sizeof(section), &used_length));
	EXPECT_TRUE(verify_checksum(section, used_length));
	EXPECT_EQ_INT(used_length, section[2]);

	klsmpte2064_context_free(hdl);
	free(frame);
	return 0;
}

static int test_supported_dimensions(void)
{
	struct format_case {
		uint32_t width;
		uint32_t height;
	};
	const struct format_case cases[] = {
		{1280, 720},
		{1920, 1080},
		{2048, 1080},
		{3840, 2160},
		{4096, 2160},
	};

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		void *hdl = NULL;
		const uint32_t stride = cases[i].width;
		const size_t frame_size = (size_t)stride * cases[i].height;
		uint8_t *frame = calloc(1, frame_size);
		uint8_t section[256] = {0};
		uint32_t used_length = 0;

		EXPECT_TRUE(frame != NULL);
		EXPECT_EQ_INT(0,
			alloc_context(&hdl,
				COLORSPACE_YUV420P,
				cases[i].width,
				cases[i].height,
				stride,
				8));
		EXPECT_EQ_INT(0, push_three_video_frames(hdl, frame, frame_size));
		EXPECT_EQ_INT(0,
			pack_section(hdl, section, sizeof(section), &used_length));
		EXPECT_TRUE(verify_checksum(section, used_length));
		EXPECT_EQ_INT(0,
			expect_bytes(GOLDEN_YUV_VIDEO_SECTION,
				sizeof(GOLDEN_YUV_VIDEO_SECTION),
				section,
				used_length));

		klsmpte2064_context_free(hdl);
		free(frame);
	}

	return 0;
}

static int test_video_api_v210_golden_section(void)
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
		alloc_context(&hdl,
			COLORSPACE_V210,
			width,
			height,
			stride,
			10));

	EXPECT_EQ_INT(0, push_three_video_frames(hdl, frame, frame_size));
	EXPECT_EQ_INT(0, pack_section(hdl, section, sizeof(section), &used_length));
	EXPECT_TRUE(verify_checksum(section, used_length));
	EXPECT_EQ_INT(0,
		expect_bytes(GOLDEN_YUV_VIDEO_SECTION,
			sizeof(GOLDEN_YUV_VIDEO_SECTION),
			section,
			used_length));

	klsmpte2064_context_free(hdl);
	free(frame);
	return 0;
}

static int test_audio_api_current_use_cases_and_edges(void)
{
	void *hdl = NULL;
	enum { SAMPLE_COUNT = 800, LARGE_SAMPLE_COUNT = 2300, DECKLINK_CHANNELS = 16 };
	int16_t left[LARGE_SAMPLE_COUNT] = {0};
	int16_t right[LARGE_SAMPLE_COUNT] = {0};
	int32_t decklink[LARGE_SAMPLE_COUNT * DECKLINK_CHANNELS] = {0};
	const int16_t *stereo_planes[2] = {left, right};
	const int16_t *bad_planes[2] = {left, NULL};
	const int16_t *decklink_planes[1] = {(const int16_t *)decklink};

	fill_audio_fixture(left,
		right,
		decklink,
		LARGE_SAMPLE_COUNT,
		DECKLINK_CHANNELS);

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
			NULL,
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
			1001,
			60000,
			stereo_planes,
			1,
			SAMPLE_COUNT));
	EXPECT_EQ_INT(-EINVAL,
		klsmpte2064_audio_push(hdl,
			AUDIOTYPE_STEREO_S32_CH16_DECKLINK,
			1001,
			60000,
			stereo_planes,
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
	EXPECT_EQ_INT(-EINVAL,
		klsmpte2064_audio_push(hdl,
			AUDIOTYPE_STEREO_S16P,
			1001,
			60000,
			stereo_planes,
			2,
			0));
	EXPECT_EQ_INT(-EINVAL,
		klsmpte2064_audio_push(hdl,
			AUDIOTYPE_MAX,
			1001,
			60000,
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
	EXPECT_EQ_INT(0,
		klsmpte2064_audio_push(hdl,
			AUDIOTYPE_STEREO_S16P,
			1001,
			60000,
			stereo_planes,
			2,
			LARGE_SAMPLE_COUNT));

	klsmpte2064_context_free(hdl);
	return 0;
}

static int test_public_api_negative_matrix(void)
{
	void *hdl = NULL;
	struct klsmpte2064_video_wss_geometry geometry = {0};
	struct klsmpte2064_video_wss_geometry bad_geometry = {0};
	uint8_t samples[KLSMPTE2064_WSS_ROWS]
		[KLSMPTE2064_WSS_SAMPLES_PER_ROW] = {{0}};
	uint8_t section[256] = {0};
	uint32_t used_length = 0;
	uint8_t y_plane[1920 * 16] = {0};

	EXPECT_EQ_INT(0,
		klsmpte2064_context_alloc_wss_luma(&hdl, 1, 1920, 1080));
	EXPECT_EQ_INT(0, klsmpte2064_video_get_wss_geometry(hdl, &geometry));

	bad_geometry = geometry;
	bad_geometry.row_count = 0;
	EXPECT_EQ_INT(-EINVAL,
		klsmpte2064_video_extract_wss_luma_yuv420p(&bad_geometry,
			y_plane,
			1920,
			1920,
			samples));

	bad_geometry = geometry;
	bad_geometry.samples_per_row = 0;
	EXPECT_EQ_INT(-EINVAL,
		klsmpte2064_video_extract_wss_luma_yuv420p(&bad_geometry,
			y_plane,
			1920,
			1920,
			samples));

	bad_geometry = geometry;
	bad_geometry.prefilter_tap_count =
		KLSMPTE2064_VIDEO_PREFILTER_MAX_TAPS + 1;
	EXPECT_EQ_INT(-EINVAL,
		klsmpte2064_video_extract_wss_luma_yuv420p(&bad_geometry,
			y_plane,
			1920,
			1920,
			samples));

	bad_geometry = geometry;
	bad_geometry.rows[0] = -1;
	EXPECT_EQ_INT(-EINVAL,
		klsmpte2064_video_extract_wss_luma_yuv420p(&bad_geometry,
			y_plane,
			1920,
			1920,
			samples));

	bad_geometry = geometry;
	bad_geometry.columns[0] = -1;
	EXPECT_EQ_INT(-EINVAL,
		klsmpte2064_video_extract_wss_luma_yuv420p(&bad_geometry,
			y_plane,
			1920,
			1920,
			samples));

	EXPECT_EQ_INT(-EINVAL,
		klsmpte2064_video_extract_wss_luma_yuv420p(&geometry,
			y_plane,
			1920,
			1919,
			samples));
	EXPECT_EQ_INT(-EINVAL,
		klsmpte2064_video_extract_wss_luma_v210(&geometry,
			y_plane,
			1919,
			1920 * 8 / 3,
			samples));
	EXPECT_EQ_INT(-EINVAL,
		klsmpte2064_video_extract_wss_luma_v210(&geometry,
			y_plane,
			1920,
			1,
			samples));

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

	for (uint32_t width = 0; width < 1300; width += 257) {
		EXPECT_EQ_INT(0,
			klsmpte2064_video_wss_luma_format_supported(1, width, 720));
	}

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

static int test_hot_path_no_allocations(void)
{
	void *hdl = NULL;
	struct klsmpte2064_video_wss_geometry geometry = {0};
	struct klsmpte2064_context_status status = {0};
	struct klsmpte2064_fingerprint fingerprint = {0};
	struct klsmpte2064_encapsulation_metadata metadata = {0};
	uint8_t samples[KLSMPTE2064_WSS_ROWS]
		[KLSMPTE2064_WSS_SAMPLES_PER_ROW] = {{0}};
	uint8_t section[256] = {0};
	uint32_t used_length = 0;
	const uint32_t width = 1920;
	const uint32_t height = 1080;
	const uint32_t yuv_stride = width + 32;
	const uint32_t v210_stride = width * 8 / 3;
	const size_t yuv_size = (size_t)yuv_stride * height;
	const size_t v210_size = (size_t)v210_stride * height;
	uint8_t *yuv_frame = calloc(1, yuv_size);
	uint8_t *v210_frame = calloc(1, v210_size);
	uint64_t before = 0;
	uint64_t after = 0;

	EXPECT_TRUE(yuv_frame != NULL);
	EXPECT_TRUE(v210_frame != NULL);
	EXPECT_EQ_INT(0,
		klsmpte2064_context_alloc_wss_luma(&hdl, 1, width, height));
	EXPECT_EQ_INT(0, klsmpte2064_video_get_wss_geometry(hdl, &geometry));

	fill_luma_pattern(yuv_frame, width, height, yuv_stride, 7);
	fill_v210_pattern(v210_frame, width, height, v210_stride, 9);
	EXPECT_EQ_INT(0, push_three_wss_sample_frames(hdl));

	before = klsmpte2064_test_allocation_count();

	EXPECT_EQ_INT(0, klsmpte2064_video_get_wss_geometry(hdl, &geometry));
	EXPECT_EQ_INT(0,
		klsmpte2064_video_extract_wss_luma_yuv420p(&geometry,
			yuv_frame,
			width,
			yuv_stride,
			samples));
	EXPECT_EQ_INT(0,
		klsmpte2064_video_extract_wss_luma_v210(&geometry,
			v210_frame,
			width,
			v210_stride,
			samples));
	EXPECT_EQ_INT(0, klsmpte2064_video_push_wss_luma(hdl, samples));
	EXPECT_EQ_INT(0, klsmpte2064_context_status(hdl, &status));
	EXPECT_EQ_INT(0, klsmpte2064_fingerprint_get(hdl, &fingerprint));
	metadata.picture_rate = KLSMPTE2064_PICTURE_RATE_5994;
	metadata.id_present = 1;
	metadata.id_length = 2;
	metadata.id_data[0] = 'K';
	metadata.id_data[1] = 'L';
	EXPECT_EQ_INT(0, klsmpte2064_encapsulation_set_metadata(hdl, &metadata));
	EXPECT_EQ_INT(0, klsmpte2064_encapsulation_get_metadata(hdl, &metadata));
	EXPECT_EQ_INT(0, pack_section(hdl, section, sizeof(section), &used_length));
	EXPECT_EQ_INT(0, klsmpte2064_video_reset(hdl));
	EXPECT_EQ_INT(0,
		klsmpte2064_audio_reset(hdl, AUDIOTYPE_STEREO_S16P));
	EXPECT_EQ_INT(0, klsmpte2064_context_reset(hdl));

	after = klsmpte2064_test_allocation_count();
	EXPECT_TRUE(before == after);

	klsmpte2064_context_free(hdl);
	free(yuv_frame);
	free(v210_frame);
	return 0;
}

typedef int (*test_fn)(void);

struct test_case {
	const char *name;
	test_fn fn;
};

int main(void)
{
	const struct test_case tests[] = {
		{ "context API validation", test_context_api },
		{ "version capabilities and format probing",
			test_version_capabilities_and_format_probing },
		{ "YUV420P golden video sections", test_yuv420p_golden_video_sections },
		{ "YUV420P padded stride golden video section",
			test_yuv420p_padded_stride_golden_video_section },
		{ "encapsulation metadata API", test_encapsulation_metadata_api },
		{ "direct WSS luma golden video sections",
			test_wss_luma_golden_video_sections },
		{ "WSS geometry API", test_wss_geometry_api },
		{ "reset APIs", test_reset_apis },
		{ "status and raw fingerprint APIs",
			test_status_and_raw_fingerprint_apis },
		{ "direct WSS luma matches YUV420P supported dimensions",
			test_wss_luma_matches_yuv420p_for_supported_dimensions },
		{ "direct WSS luma matches V210 supported dimensions",
			test_wss_luma_matches_v210_for_supported_dimensions },
		{ "GPU sampler reference vector 1920x1080",
			test_gpu_sampler_reference_vector_1920x1080 },
		{ "YUV420P golden audio section", test_yuv420p_golden_audio_section },
		{ "YUV420P video and encapsulation validation",
			test_video_api_yuv420p_and_encapsulation_validation },
		{ "supported progressive dimensions", test_supported_dimensions },
		{ "V210 golden video section", test_video_api_v210_golden_section },
		{ "audio API current use cases and edges",
			test_audio_api_current_use_cases_and_edges },
		{ "public API negative matrix", test_public_api_negative_matrix },
		{ "V210 colorspace conversion helpers", test_csc_api },
		{ "hot path APIs do not allocate", test_hot_path_no_allocations },
	};
	int passed = 0;
	int failed = 0;

	printf("Running %zu libklsmpte2064 API checks\n",
		sizeof(tests) / sizeof(tests[0]));
	for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
		printf("CHECK %zu/%zu: %s ... ",
			i + 1,
			sizeof(tests) / sizeof(tests[0]),
			tests[i].name);
		fflush(stdout);

		const int ret = tests[i].fn();
		if (ret == 0) {
			printf("PASS\n");
			passed++;
		} else {
			printf("FAIL\n");
			failed++;
		}
	}

	printf("Summary: total=%d passed=%d failed=%d\n",
		passed + failed,
		passed,
		failed);

	return failed == 0 ? 0 : 1;
}
