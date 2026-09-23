/**
 * @file	core-video.h
 * @author	Steven Toth <stoth@kernellabs.com>
 * @copyright	Copyright (c) 2025 Kernel Labs Inc. All Rights Reserved.
 * @brief	Processing video content
 */

#ifndef _LIBKLSMPTE2064_CORE_VIDEO_H
#define _LIBKLSMPTE2064_CORE_VIDEO_H

#include <stdint.h>
#include <stdarg.h>
#include <sys/errno.h>

#include <libklsmpte2064/export.h>
#include <libklsmpte2064/core.h>
#include <libklsmpte2064/core-fingerprint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Number of SMPTE 2064 windowed sub-sampling rows per video frame. */
#define KLSMPTE2064_WSS_ROWS 16
/** Number of SMPTE 2064 luma samples in each windowed sub-sampling row. */
#define KLSMPTE2064_WSS_SAMPLES_PER_ROW 60
/** Total number of SMPTE 2064 windowed luma samples per video frame. */
#define KLSMPTE2064_WSS_SAMPLES_PER_FRAME \
	(KLSMPTE2064_WSS_ROWS * KLSMPTE2064_WSS_SAMPLES_PER_ROW)
/** Maximum number of horizontal luma taps in the SMPTE 2064 prefilter. */
#define KLSMPTE2064_VIDEO_PREFILTER_MAX_TAPS 6
/** Maximum number of flattened sampler-plan taps per frame. */
#define KLSMPTE2064_WSS_SAMPLER_PLAN_MAX_TAPS \
	(KLSMPTE2064_WSS_SAMPLES_PER_FRAME * \
	 KLSMPTE2064_VIDEO_PREFILTER_MAX_TAPS)

/**
 * @brief SMPTE 2064 window and prefilter geometry for external sampling.
 *
 * Callers that use klsmpte2064_video_push_wss_luma() can query this structure
 * to learn the exact source coordinates and horizontal prefilter offsets for
 * the context's configured video format.
 *
 * rows[r] is an absolute vertical luma pixel coordinate in the source image.
 * columns[c] is an absolute horizontal luma pixel coordinate in the source
 * image. prefilter_offsets[t] is added only to columns[c], never to rows[r].
 * For each output sample, the caller computes:
 *
 * @code{.c}
 * samples[r][c] = average_valid_taps(
 *     Y[rows[r]][columns[c] + prefilter_offsets[t]]);
 * @endcode
 *
 * Horizontal taps that would fall outside the source image are ignored,
 * matching klsmpte2064_video_push(). The resulting sample block is passed to
 * klsmpte2064_video_push_wss_luma() as samples[16][60].
 */
struct klsmpte2064_video_wss_geometry {
	uint32_t row_count; /**< Number of vertical coordinates in rows[]. */
	uint32_t samples_per_row; /**< Number of horizontal coordinates in columns[]. */
	uint32_t prefilter_tap_count; /**< Number of valid horizontal taps in prefilter_offsets[]. */
	int rows[KLSMPTE2064_WSS_ROWS]; /**< Absolute source luma Y coordinates. */
	int columns[KLSMPTE2064_WSS_SAMPLES_PER_ROW]; /**< Absolute source luma X coordinates. */
	int prefilter_offsets[KLSMPTE2064_VIDEO_PREFILTER_MAX_TAPS]; /**< Horizontal X offsets added to each columns[c]. */
};

/**
 * @brief One output sample in a flattened WSS sampler plan.
 *
 * The sample maps to samples[sample_index / 60][sample_index % 60]. A GPU
 * kernel reads tap_count entries starting at tap_start in
 * klsmpte2064_video_wss_sampler_plan::taps, averages those source coordinates,
 * and writes one 8-bit luma value to the matching output sample index.
 */
struct klsmpte2064_video_wss_sample_plan_entry {
	uint16_t sample_index; /**< Flattened 0..959 output sample index. */
	uint16_t tap_start; /**< First tap index in the plan taps[] array. */
	uint16_t tap_count; /**< Number of valid taps to average. */
	uint16_t reserved; /**< Reserved, written as zero. */
};

/** Absolute luma coordinate to read for one sampler-plan tap. */
struct klsmpte2064_video_wss_sampler_tap {
	int32_t x; /**< Absolute source luma X coordinate. */
	int32_t y; /**< Absolute source luma Y coordinate. */
};

/**
 * @brief GPU-ready flattened WSS sampler plan.
 *
 * This is a direct translation of klsmpte2064_video_wss_geometry into fixed
 * arrays that can be copied to a GPU buffer. The plan includes only valid taps,
 * so kernels do not need bounds checks for the configured source dimensions.
 *
 * The library fills size and version on output. Callers should zero-initialize
 * the struct before first use.
 */
struct klsmpte2064_video_wss_sampler_plan {
	uint32_t size; /**< sizeof(struct klsmpte2064_video_wss_sampler_plan). */
	uint32_t version; /**< KLSMPTE2064_STRUCT_VERSION_1. */
	uint32_t sample_count; /**< Number of valid entries in samples[]. */
	uint32_t tap_count; /**< Number of valid entries in taps[]. */
	uint32_t max_taps_per_sample; /**< Maximum tap_count for any one sample. */
	uint32_t reserved; /**< Reserved, written as zero. */
	/** Per-output-sample entries. Only sample_count entries are valid. */
	struct klsmpte2064_video_wss_sample_plan_entry
		samples[KLSMPTE2064_WSS_SAMPLES_PER_FRAME];
	/** Absolute source taps. Only tap_count entries are valid. */
	struct klsmpte2064_video_wss_sampler_tap
		taps[KLSMPTE2064_WSS_SAMPLER_PLAN_MAX_TAPS];
};

/**
 * @brief Result returned by klsmpte2064_video_push_wss_luma_result().
 *
 * This combines the per-frame push with the state that realtime integrations
 * commonly query immediately afterward.
 */
struct klsmpte2064_video_push_result {
	uint32_t size; /**< sizeof(struct klsmpte2064_video_push_result). */
	uint32_t version; /**< KLSMPTE2064_STRUCT_VERSION_1. */
	struct klsmpte2064_context_status status; /**< Context status after push. */
	uint8_t video_fingerprint; /**< Current video fingerprint byte. */
	uint8_t reserved[7]; /**< Reserved, written as zero. */
};

/**
 * @brief Deterministic WSS conformance vector for sampler validation.
 *
 * The generated samples use the same deterministic luma pattern as the test
 * suite: Y = (x * 3 + y * 5 + seed * 37) & 0xff. GPU samplers can compare
 * their 960-byte output against samples[] for a supported source geometry.
 */
struct klsmpte2064_video_wss_conformance_vector {
	uint32_t size; /**< sizeof(struct klsmpte2064_video_wss_conformance_vector). */
	uint32_t version; /**< KLSMPTE2064_STRUCT_VERSION_1. */
	uint32_t width; /**< Source width used by this vector. */
	uint32_t height; /**< Source height used by this vector. */
	uint8_t seed; /**< Deterministic source-pattern seed. */
	uint8_t sample_xor; /**< XOR of all samples for quick checks. */
	uint16_t reserved; /**< Reserved, written as zero. */
	uint32_t sample_sum; /**< Sum of all samples for quick checks. */
	uint8_t samples[KLSMPTE2064_WSS_ROWS]
		[KLSMPTE2064_WSS_SAMPLES_PER_ROW]; /**< Expected 960-byte block. */
};

/**
 * @brief Test whether a video format is supported by the frame-push API.
 *
 * @param[in] colorspace Source video colorspace.
 * @param[in] progressive Boolean. Currently only progressive video is supported.
 * @param[in] width Video width in pixels.
 * @param[in] height Video height in pixels.
 * @param[in] bitdepth Source bit depth.
 * @return 1 when supported, 0 when unsupported.
 */
KLSMPTE2064_API int klsmpte2064_video_format_supported(
	enum klsmpte2064_colorspace_e colorspace,
	uint32_t progressive,
	uint32_t width,
	uint32_t height,
	uint32_t bitdepth);

/**
 * @brief Test whether dimensions can use direct WSS luma input.
 *
 * This probes the geometry tables needed by klsmpte2064_context_alloc_wss_luma()
 * and klsmpte2064_video_get_wss_geometry().
 *
 * @param[in] progressive Boolean. Currently only progressive video is supported.
 * @param[in] width Video width in pixels.
 * @param[in] height Video height in pixels.
 * @return 1 when supported, 0 when unsupported.
 */
KLSMPTE2064_API int klsmpte2064_video_wss_luma_format_supported(
	uint32_t progressive,
	uint32_t width,
	uint32_t height);

/**
 * @brief	    Push a video frame into the solution for processing.
 *              During context creation the width, height, depth etc was declared,
 *              pay attension and don't violate that.
 * @param[in] hdl A previously allocated context handle.
 * @param[in] lumaplane The source luma plane for COLORSPACE_YUV420P, or a V210
 *            frame buffer for COLORSPACE_V210.
 * @return      0 - Success
 * @return      < 0 - Error
 *
 * Threading: calls on the same context must be serialized by the caller.
 */
KLSMPTE2064_API int klsmpte2064_video_push(klsmpte2064_context *hdl,
	const uint8_t *lumaplane);

/**
 * @brief	    Query the SMPTE 2064 sampling geometry for a context.
 *
 * The returned geometry describes the 16 by 60 sample centers and horizontal
 * prefilter taps used by klsmpte2064_video_push() for the context's configured
 * width, height, progressive flag, and format table selection. Callers can use
 * the geometry to extract equivalent prefiltered 8-bit luma samples from GPU
 * surfaces, then submit them through klsmpte2064_video_push_wss_luma().
 *
 * For a 1920x1080 progressive source, the current table returns 16 vertical
 * source rows from 178 through 898 and 60 horizontal source columns from 399
 * through 1520, with horizontal prefilter offsets -1, 0, and 1. For example,
 * samples[0][0] is the average of Y[178][398], Y[178][399], and Y[178][400],
 * and samples[15][59] is the average of Y[898][1519], Y[898][1520], and
 * Y[898][1521].
 *
 * This function performs no dynamic allocation.
 *
 * @param[in] hdl A previously allocated context handle.
 * @param[out] geometry Destination for the sampling geometry.
 * @return 0 on success.
 * @return -EINVAL when hdl or geometry is NULL.
 *
 * Threading: calls on the same context must be serialized by the caller.
 */
KLSMPTE2064_API int klsmpte2064_video_get_wss_geometry(klsmpte2064_context *hdl,
	struct klsmpte2064_video_wss_geometry *geometry);

/**
 * @brief Query a flattened, GPU-ready WSS sampler plan.
 *
 * The plan describes exactly how to produce the 16 by 60 sample block accepted
 * by klsmpte2064_video_push_wss_luma(). Each sample entry points to one or more
 * absolute luma taps to average.
 *
 * This function performs no dynamic allocation.
 *
 * @param[in] hdl A previously allocated context handle.
 * @param[out] plan Destination for the sampler plan.
 * @return 0 on success.
 * @return -EINVAL when hdl or plan is NULL.
 */
KLSMPTE2064_API int klsmpte2064_video_get_wss_sampler_plan(
	klsmpte2064_context *hdl,
	struct klsmpte2064_video_wss_sampler_plan *plan);

/**
 * @brief Extract windowed luma samples from an 8-bit YUV420P luma plane.
 *
 * This is a CPU reference helper for callers that implement their own fast
 * sampler, such as a GPU compute kernel. It applies the supplied geometry's
 * horizontal prefilter taps and writes the exact 16 by 60 8-bit luma sample
 * block accepted by klsmpte2064_video_push_wss_luma().
 *
 * @param[in] geometry Geometry returned by klsmpte2064_video_get_wss_geometry().
 * @param[in] lumaplane Source Y plane.
 * @param[in] width Source image width in pixels.
 * @param[in] stride Source Y plane stride in bytes.
 * @param[out] samples Receives prefiltered 8-bit luma samples as [16][60].
 * @return 0 on success.
 * @return -EINVAL on invalid arguments.
 *
 * This function performs no dynamic allocation.
 */
KLSMPTE2064_API int klsmpte2064_video_extract_wss_luma_yuv420p(
	const struct klsmpte2064_video_wss_geometry *geometry,
	const uint8_t *lumaplane,
	uint32_t width,
	uint32_t stride,
	uint8_t samples[KLSMPTE2064_WSS_ROWS][KLSMPTE2064_WSS_SAMPLES_PER_ROW]);

/**
 * @brief Extract windowed luma samples from a packed V210 frame.
 *
 * This CPU reference helper mirrors the V210 luma unpacking and horizontal
 * prefiltering used by klsmpte2064_video_push(). It is intended as a correctness
 * oracle for more efficient integrations, not as the fastest possible path.
 *
 * @param[in] geometry Geometry returned by klsmpte2064_video_get_wss_geometry().
 * @param[in] frame Source V210 frame.
 * @param[in] width Source image width in pixels.
 * @param[in] stride Source V210 frame stride in bytes.
 * @param[out] samples Receives prefiltered 8-bit luma samples as [16][60].
 * @return 0 on success.
 * @return -EINVAL on invalid arguments.
 *
 * This function performs no dynamic allocation.
 */
KLSMPTE2064_API int klsmpte2064_video_extract_wss_luma_v210(
	const struct klsmpte2064_video_wss_geometry *geometry,
	const uint8_t *frame,
	uint32_t width,
	uint32_t stride,
	uint8_t samples[KLSMPTE2064_WSS_ROWS][KLSMPTE2064_WSS_SAMPLES_PER_ROW]);

/**
 * @brief	    Push prefiltered SMPTE 2064 windowed luma samples.
 *
 * This entry point is intended for callers that can extract the SMPTE 2064
 * windowed sub-sampling set more efficiently than libklsmpte2064 can from a
 * packed CPU video plane. The caller supplies exactly 16 rows by 60 samples of
 * 8-bit luma, matching the samples that Section 5.2 motion detection consumes
 * after format-specific prefiltering and window sub-sampling.
 *
 * The library does not perform colorspace conversion, horizontal prefiltering,
 * or coordinate selection in this API. The caller is responsible for producing
 * samples equivalent to those operations for the context's configured video
 * format. This is useful for GPU pipelines that can read only the required
 * luma sample/tap positions from an existing video surface and avoid a full
 * frame CPU readback or full frame colorspace conversion.
 *
 * The samples are consumed immediately. The caller may reuse or release the
 * buffer after the function returns. The function updates the same motion
 * history and video fingerprint state as klsmpte2064_video_push().
 * This function performs no dynamic allocation.
 *
 * @param[in] hdl A previously allocated context handle.
 * @param[in] samples Prefiltered 8-bit luma samples arranged as [16][60].
 * @return 0 on success.
 * @return -EINVAL when hdl or samples is NULL.
 *
 * Threading: calls on the same context must be serialized by the caller.
 */
KLSMPTE2064_API int klsmpte2064_video_push_wss_luma(klsmpte2064_context *hdl,
	const uint8_t samples[KLSMPTE2064_WSS_ROWS][KLSMPTE2064_WSS_SAMPLES_PER_ROW]);

/**
 * @brief Push prefiltered WSS luma samples and return current fingerprint state.
 *
 * This is equivalent to klsmpte2064_video_push_wss_luma() followed by status
 * and raw video fingerprint queries, but it does the common realtime path in a
 * single call. The result is filled only when the push succeeds.
 * This function performs no dynamic allocation.
 *
 * @param[in] hdl A previously allocated context handle.
 * @param[in] samples Prefiltered 8-bit luma samples arranged as [16][60].
 * @param[out] result Destination for the post-push result.
 * @return 0 on success.
 * @return -EINVAL when hdl, samples, or result is NULL.
 */
KLSMPTE2064_API int klsmpte2064_video_push_wss_luma_result(
	klsmpte2064_context *hdl,
	const uint8_t samples[KLSMPTE2064_WSS_ROWS][KLSMPTE2064_WSS_SAMPLES_PER_ROW],
	struct klsmpte2064_video_push_result *result);

/**
 * @brief Build a deterministic WSS conformance vector for sampler validation.
 *
 * The vector is generated from the current context geometry without dynamic
 * allocation and is intended for tests that compare a GPU sampler against the
 * library's reference math.
 *
 * @param[in] hdl A previously allocated context handle.
 * @param[in] seed Deterministic source-pattern seed.
 * @param[out] vector Destination for expected samples and quick checks.
 * @return 0 on success.
 * @return -EINVAL when hdl or vector is NULL.
 */
KLSMPTE2064_API int klsmpte2064_video_make_wss_conformance_vector(
	klsmpte2064_context *hdl,
	uint8_t seed,
	struct klsmpte2064_video_wss_conformance_vector *vector);

/**
 * @brief Reset video motion history and video fingerprint state.
 *
 * Use this when a video stream has a discontinuity, source switch, seek, or
 * reconnect and the next frames should not be compared with pre-discontinuity
 * samples. Audio fingerprints and encapsulation sequence state are unchanged.
 *
 * @param[in] hdl A previously allocated context handle.
 * @return 0 on success.
 * @return -EINVAL when hdl is NULL.
 *
 * This function performs no dynamic allocation.
 *
 * Threading: calls on the same context must be serialized by the caller.
 */
KLSMPTE2064_API int klsmpte2064_video_reset(klsmpte2064_context *hdl);

#ifdef __cplusplus
};
#endif

#endif /* _LIBKLSMPTE2064_CORE_VIDEO_H */
