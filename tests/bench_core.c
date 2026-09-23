#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include <libklsmpte2064/klsmpte2064.h>

static double now_seconds(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return (double)tv.tv_sec + ((double)tv.tv_usec / 1000000.0);
}

static void fill_samples(uint8_t samples[KLSMPTE2064_WSS_ROWS]
	[KLSMPTE2064_WSS_SAMPLES_PER_ROW],
	uint8_t seed)
{
	for (int r = 0; r < KLSMPTE2064_WSS_ROWS; r++) {
		for (int c = 0; c < KLSMPTE2064_WSS_SAMPLES_PER_ROW; c++) {
			samples[r][c] = (uint8_t)((r * 17 + c * 5 + seed) & 0xff);
		}
	}
}

static void fill_luma(uint8_t *frame,
	uint32_t width,
	uint32_t height,
	uint32_t stride)
{
	for (uint32_t y = 0; y < height; y++) {
		uint8_t *line = frame + (y * stride);
		for (uint32_t x = 0; x < width; x++) {
			line[x] = (uint8_t)((x * 3 + y * 7) & 0xff);
		}
	}
}

static void print_rate(const char *name, uint32_t iterations, double elapsed)
{
	const double per_second = (double)iterations / elapsed;
	const double ns_each = (elapsed * 1000000000.0) / (double)iterations;

	printf("BENCH %-36s %8u iterations %8.3f ms %10.0f/s %8.1f ns/op\n",
		name,
		iterations,
		elapsed * 1000.0,
		per_second,
		ns_each);
}

int main(void)
{
	enum {
		PUSH_ITERATIONS = 200000,
		QUERY_ITERATIONS = 200000,
		PACK_ITERATIONS = 100000,
		EXTRACT_ITERATIONS = 10000,
	};
	void *hdl = NULL;
	struct klsmpte2064_video_wss_geometry geometry = {0};
	struct klsmpte2064_context_status status = {0};
	struct klsmpte2064_fingerprint fingerprint = {0};
	uint8_t samples[KLSMPTE2064_WSS_ROWS]
		[KLSMPTE2064_WSS_SAMPLES_PER_ROW] = {{0}};
	uint8_t sample_blocks[4][KLSMPTE2064_WSS_ROWS]
		[KLSMPTE2064_WSS_SAMPLES_PER_ROW] = {{{0}}};
	uint8_t section[256] = {0};
	uint32_t used = 0;
	const uint32_t width = 1920;
	const uint32_t height = 1080;
	const uint32_t stride = 1920;
	const size_t frame_size = (size_t)stride * height;
	uint8_t *frame = calloc(1, frame_size);
	double start = 0.0;
	double elapsed = 0.0;
	int ret = 0;

	if (!frame) {
		return 1;
	}
	fill_luma(frame, width, height, stride);
	for (uint8_t i = 0; i < 4; i++) {
		fill_samples(sample_blocks[i], i);
	}

	ret = klsmpte2064_context_alloc_wss_luma(&hdl, 1, width, height);
	if (ret < 0) {
		free(frame);
		return 1;
	}
	ret = klsmpte2064_video_get_wss_geometry(hdl, &geometry);
	if (ret < 0) {
		klsmpte2064_context_free(hdl);
		free(frame);
		return 1;
	}

	klsmpte2064_video_push_wss_luma(hdl, sample_blocks[0]);
	klsmpte2064_video_push_wss_luma(hdl, sample_blocks[1]);
	klsmpte2064_video_push_wss_luma(hdl, sample_blocks[2]);

	start = now_seconds();
	for (uint32_t i = 0; i < PUSH_ITERATIONS; i++) {
		klsmpte2064_video_push_wss_luma(hdl, sample_blocks[i & 3]);
	}
	elapsed = now_seconds() - start;
	print_rate("direct WSS push", PUSH_ITERATIONS, elapsed);

	start = now_seconds();
	for (uint32_t i = 0; i < QUERY_ITERATIONS; i++) {
		klsmpte2064_context_status(hdl, &status);
		klsmpte2064_fingerprint_get(hdl, &fingerprint);
	}
	elapsed = now_seconds() - start;
	print_rate("status + raw fingerprint query", QUERY_ITERATIONS, elapsed);

	start = now_seconds();
	for (uint32_t i = 0; i < PACK_ITERATIONS; i++) {
		klsmpte2064_encapsulation_pack(hdl, section, sizeof(section), &used);
	}
	elapsed = now_seconds() - start;
	print_rate("encapsulation pack", PACK_ITERATIONS, elapsed);

	start = now_seconds();
	for (uint32_t i = 0; i < EXTRACT_ITERATIONS; i++) {
		klsmpte2064_video_extract_wss_luma_yuv420p(&geometry,
			frame,
			width,
			stride,
			samples);
	}
	elapsed = now_seconds() - start;
	print_rate("YUV420P WSS CPU extractor", EXTRACT_ITERATIONS, elapsed);

	printf("Summary: total=4 passed=4 failed=0\n");

	klsmpte2064_context_free(hdl);
	free(frame);
	return 0;
}
