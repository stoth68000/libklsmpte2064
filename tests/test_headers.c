#include <stdio.h>

#include <libklsmpte2064/core.h>
#include <libklsmpte2064/core-audio.h>
#include <libklsmpte2064/core-encapsulation.h>
#include <libklsmpte2064/core-fingerprint.h>
#include <libklsmpte2064/core-source.h>
#include <libklsmpte2064/core-video.h>

int main(void)
{
	klsmpte2064_context *hdl = NULL;
	struct klsmpte2064_source_config config = {0};
	struct klsmpte2064_video_wss_sampler_plan plan = {0};
	struct klsmpte2064_video_push_result result = {0};
	struct klsmpte2064_fingerprint fingerprint = {0};
	uint32_t max_size = 0;

	config.size = sizeof(config);
	config.version = KLSMPTE2064_STRUCT_VERSION_1;
	config.progressive = 1;
	config.width = 1920;
	config.height = 1080;
	config.timebase_num = 1001;
	config.timebase_den = 60000;

	(void)hdl;
	(void)plan;
	(void)result;
	(void)fingerprint;
	(void)max_size;

	if (!klsmpte2064_capabilities_satisfy(
			KLSMPTE2064_GPU_DIRECT_WSS_REQUIRED_CAPABILITIES)) {
		printf("Summary: total=1 passed=0 failed=1\n");
		return 1;
	}

	printf("Summary: total=1 passed=1 failed=0\n");
	return 0;
}
