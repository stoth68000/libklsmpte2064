#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <ctype.h>

#include <libklsmpte2064/klsmpte2064.h>

struct tool_ctx_s
{
	int verbose;
	char *ivname;
	char *ianame;
	uint32_t width;
	uint32_t height;
	uint32_t progressive;
	uint32_t stride;
	uint32_t bitdepth;

	void *hdl;
};

static void usage(const char *program)
{
	printf("Version: %s\n", GIT_VERSION);
	printf("Read consecutive YUV420P frames of video from disk, generate video fingerprints.\n");
	printf("Usage: %s\n", program);
	printf("  -i yuv420p.yuv filename\n");
	printf("  -Y audioS32le.bin filename (interleaved only L / R / L / R)\n");
	printf("  -H pixel height\n");
	printf("  -W pixel width\n");
	printf("  -v increase level of verbosity\n");
	printf("\n");
	printf("  Eg. %s -i ../../dwts-master2880.yuv -W 1280 -H 720 -I ../../audio-ch2-s32-soccer.bin [-v]\n\n", program);
}

int main(int argc, char *argv[])
{
	struct tool_ctx_s *ctx = calloc(1, sizeof(*ctx));
	if (!ctx) {
		return -1;
	}
	ctx->bitdepth = 8;
	ctx->progressive = 1;

	int ch;

	while ((ch = getopt(argc, argv, "?hi:vB:H:I:S:W:Y:")) != -1) {
		switch (ch) {
		case 'i':
			if (ctx->ivname) {
				free(ctx->ivname);
				ctx->ivname = NULL;
			}
			ctx->ivname = strdup(optarg);
			break;
		case 'B':
			ctx->bitdepth = atoi(optarg);
			if (ctx->bitdepth != 8) {
				usage(argv[0]);
				exit(0);
			}
			break;
		case 'H':
			ctx->height = atoi(optarg);
			break;
		case 'I':
			if (ctx->ianame) {
				free(ctx->ianame);
				ctx->ianame = NULL;
			}
			ctx->ianame = strdup(optarg);
			break;
		case 'S':
			ctx->stride = atoi(optarg);
			break;
		case 'v':
			ctx->verbose++;
			break;
		case 'W':
			ctx->width = atoi(optarg);
			ctx->stride = ctx->width;
			break;
		default:
			usage(argv[0]);
			exit(1);
		}
	}

	if (ctx->ivname == NULL) {
		usage(argv[0]);
		exit(1);
	}

	int frame_size = (ctx->height * ctx->stride * 3) / 2;

	printf("Input Video: %s\n", ctx->ivname);
	printf("Framesize: %d\n", frame_size);
	if (ctx->ianame) {
		printf("Input Audio: %s\n", ctx->ianame);
	}

	if (!ctx->width || !ctx->height) {
		usage(argv[0]);
		exit(0);
	}

	int channels = 2;
	int aSampleCount = 800;
	int audioIsize = channels * sizeof(int32_t) * aSampleCount;
	int audioPsize = channels * sizeof(int16_t) * aSampleCount;
	int32_t *audioI = malloc(audioIsize);
	if (!audioI) {
		perror("malloc");
		exit(0);
	}
	int16_t *audioP = malloc(audioPsize);
	if (!audioP) {
		perror("malloc");
		exit(0);
	}

	uint8_t *lumaplane = malloc(frame_size);
	if (!lumaplane) {
		perror("malloc");
		exit(0);
	}

	FILE *fhv = fopen(ctx->ivname, "rb");
	if (!fhv) {
		perror("file open video");
		exit(0);
	}

	FILE *fha = fopen(ctx->ianame, "rb");
	if (!fha) {
		perror("file open audio");
		exit(0);
	}

	/* */
	if (klsmpte2064_context_alloc(&ctx->hdl,
		1, // uint32_t isYUV420p,
		1, // uint32_t progressive,
		ctx->width, // uint32_t width,
		ctx->height, // uint32_t height,
		ctx->stride, // uint32_t stride,
		ctx->bitdepth // uint32_t bitdepth);
	) < 0) {
		fprintf(stderr, "Unable to allocate SMPTE 2064 context, aborting\n");
		exit(0);
	}

	if (ctx->verbose) {
		klsmpte2064_context_set_verbose(ctx->hdl, ctx->verbose);
	}

	uint8_t section[512];

	while (1) {
		int l = fread(lumaplane, 1, frame_size, fhv);
		if (l < frame_size) {
			break;
		}
		if (feof(fhv)) {
			break;
		}
		l = fread(audioI, 1, audioIsize, fha);
		if (l < audioIsize) {
			break;
		}
		if (feof(fha)) {
			break;
		}

		/* VIDEO */
		if (klsmpte2064_video_push(ctx->hdl, lumaplane) < 0) {
			fprintf(stderr, "Unable to push luma plane, aborting\n");
			exit(0);
		}

		/* Audio */
		const int16_t *planes[2] = { &audioP[0], &audioP[aSampleCount] };

		/* Convert from interleaved to planar */
		for (int i = 0; i < aSampleCount; i++) {
			int32_t l = audioI[(i * 2) + 0] >> 16;
			audioP[i] = (int16_t)l;
		}
		for (int i = 0; i < aSampleCount; i++) {
			int32_t r = audioI[(i * 2) + 1] >> 16;
			audioP[aSampleCount + i] = (int16_t)r;
		}

		if (klsmpte2064_audio_push(ctx->hdl, AUDIOTYPE_STEREO_S16P, &planes[0], 2, aSampleCount) < 0) {
			fprintf(stderr, "Unable to push luma plane, aborting\n");
			exit(0);
		}

		uint32_t usedLength = 0;
		if (klsmpte2064_encapsulation_pack(ctx->hdl, section, sizeof(section), &usedLength) == 0) {
			printf("section %4d: ", usedLength);
			for (int i = 0; i < usedLength; i++) {
				printf("%02x ", section[i]);
			}
			printf("\n");
			printf("section %4d: ", usedLength);
			for (int i = 0; i < usedLength; i++) {
				printf("  %c", isprint(section[i]) ? section[i] : '.');
			}
			printf("\n");
		}

	}

	printf("Shutdown\n");

	if (fhv) {
		fclose(fhv);
	}
	if (fha) {
		fclose(fha);
	}

	klsmpte2064_context_free(ctx->hdl);
	free(audioP);
	free(audioI);
	free(lumaplane);
	free(ctx);
	return 0;
}