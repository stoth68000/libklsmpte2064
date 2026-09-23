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
 * the context's configured video format. A GPU implementation should produce
 * one output luma sample for every row/column pair by averaging the valid
 * source pixels at column + prefilter_offsets[n]. Offsets that would fall
 * outside the source image are ignored, matching klsmpte2064_video_push().
 */
struct klsmpte2064_video_wss_geometry {
	uint32_t row_count; /**< Always KLSMPTE2064_WSS_ROWS for progressive video. */
	uint32_t samples_per_row; /**< Always KLSMPTE2064_WSS_SAMPLES_PER_ROW. */
	uint32_t prefilter_tap_count; /**< Number of valid entries in prefilter_offsets. */
	int rows[KLSMPTE2064_WSS_ROWS]; /**< Source luma row for each WSS row. */
	int columns[KLSMPTE2064_WSS_SAMPLES_PER_ROW]; /**< Source luma column for each WSS sample. */
	int prefilter_offsets[KLSMPTE2064_VIDEO_PREFILTER_MAX_TAPS]; /**< Horizontal prefilter offsets. */
};

/**
 * @brief	    Push a video frame into the solution for processing.
 *              During context creation the width, height, depth etc was declared,
 *              pay attension and don't violate that.
 * @param[in] hdl A previously allocated context handle.
 * @param[in] lumaplane The source luma plane for COLORSPACE_YUV420P, or a V210
 *            frame buffer for COLORSPACE_V210.
 * @return      0 - Success
 * @return      < 0 - Error
 */
int klsmpte2064_video_push(void *hdl, const uint8_t *lumaplane);

/**
 * @brief	    Query the SMPTE 2064 sampling geometry for a context.
 *
 * The returned geometry describes the 16 by 60 sample centers and horizontal
 * prefilter taps used by klsmpte2064_video_push() for the context's configured
 * width, height, progressive flag, and format table selection. Callers can use
 * the geometry to extract equivalent prefiltered 8-bit luma samples from GPU
 * surfaces, then submit them through klsmpte2064_video_push_wss_luma().
 *
 * @param[in] hdl A previously allocated context handle.
 * @param[out] geometry Destination for the sampling geometry.
 * @return 0 on success.
 * @return -EINVAL when hdl or geometry is NULL.
 */
int klsmpte2064_video_get_wss_geometry(void *hdl,
	struct klsmpte2064_video_wss_geometry *geometry);

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
 *
 * @param[in] hdl A previously allocated context handle.
 * @param[in] samples Prefiltered 8-bit luma samples arranged as [16][60].
 * @return 0 on success.
 * @return -EINVAL when hdl or samples is NULL.
 */
int klsmpte2064_video_push_wss_luma(void *hdl,
	const uint8_t samples[KLSMPTE2064_WSS_ROWS][KLSMPTE2064_WSS_SAMPLES_PER_ROW]);

#ifdef __cplusplus
};
#endif

#endif /* _LIBKLSMPTE2064_CORE_VIDEO_H */
