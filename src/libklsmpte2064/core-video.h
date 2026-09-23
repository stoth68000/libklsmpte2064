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
KLSMPTE2064_API int klsmpte2064_video_push(void *hdl, const uint8_t *lumaplane);

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
KLSMPTE2064_API int klsmpte2064_video_get_wss_geometry(void *hdl,
	struct klsmpte2064_video_wss_geometry *geometry);

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
KLSMPTE2064_API int klsmpte2064_video_push_wss_luma(void *hdl,
	const uint8_t samples[KLSMPTE2064_WSS_ROWS][KLSMPTE2064_WSS_SAMPLES_PER_ROW]);

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
KLSMPTE2064_API int klsmpte2064_video_reset(void *hdl);

#ifdef __cplusplus
};
#endif

#endif /* _LIBKLSMPTE2064_CORE_VIDEO_H */
