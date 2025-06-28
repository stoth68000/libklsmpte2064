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
	char *iname;
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
	printf("  -H pixel height\n");
	printf("  -W pixel width\n");
	printf("  -v increase level of verbosity\n");
	printf("\n");
	printf("  Eg. %s -i ../../dwts-master2880.yuv\n\n", program);
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

	while ((ch = getopt(argc, argv, "?hi:vB:H:S:W:")) != -1) {
		switch (ch) {
		case 'i':
			if (ctx->iname) {
				free(ctx->iname);
				ctx->iname = NULL;
			}
			ctx->iname = strdup(optarg);
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

	if (ctx->iname == NULL) {
		usage(argv[0]);
		exit(1);
	}

	int frame_size = (ctx->height * ctx->stride * 3) / 2;

	printf("Input: %s\n", ctx->iname);
	printf("Framesize: %d\n", frame_size);
	
	if (!ctx->width || !ctx->height) {
		usage(argv[0]);
		exit(0);
	}
	
	uint8_t *lumaplane = malloc(frame_size);
	if (!lumaplane) {
		perror("malloc");
		exit(0);
	}

	FILE *fh = fopen(ctx->iname, "rb");
	if (!fh) {
		perror("file open");
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

	while (!feof(fh)) {
		int l = fread(lumaplane, 1, frame_size, fh);
		if (l < frame_size) {
			break;
		}

		if (klsmpte2064_video_push(ctx->hdl, lumaplane) < 0) {
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
	fclose(fh);

	klsmpte2064_context_free(ctx->hdl);
	free(lumaplane);
	free(ctx);
	return 0;
}