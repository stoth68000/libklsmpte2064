/**
 * @file	core.h
 * @author	Steven Toth <stoth@kernellabs.com>
 * @copyright	Copyright (c) 2025 Kernel Labs Inc. All Rights Reserved.
 * @brief	Context allocation and destruction
 *
 */

/**
 * @page api_overview API Overview
 *
 * @section intro_sec Introduction
 *
 * A fast and efficient system for fingerprint creation.
 * See the project README.MD for a full description of features.
 * The source is written to closely follow the specification, so you should
 * easily be able to compare the library to the spec - for confirmation.
 *
 * @section use_Case_sec Typical Use Case
 * Your project might need to do something like this:
 *
 * @code{.sh}
 * void *hdl;
 * 
 * klsmpte2064_context_alloc(&hdl, COLORSPACE_V210, 1280, 720, strideBytes, 10);
 * 
 * while (frameArrived) {
 * 
 *   // Process Video - get the luma plane
 *   const uint8_t *videoplane = NULL; 
 *   videoframe->GetBytes((void **)&v);
 * 
 *   // Feed video into the framework
 *   klsmpte2064_video_push(hdl, videoplane);
 * 
 *   // Process Audio - We want a 5.1 discrete S312 fingerprint
 *   // Takes audio channels 1-6 in a SMPTE 312 channel format and fingerprint those.
 *   // Get the audio planes
 *   const int32_t *audiobytes = NULL;
 *   audioframe->GetBytes((void **)&audioplane);
 *   const int16_t *planes[] = { (int16_t *)audiobytes };
 * 
 *   // Feed audio into the framework
 *   klsmpte2064_audio_push(hdl, AUDIOTYPE_SMPTE312_S32_CH16_DECKLINK, 1001, 6000, &planes[0], 1, audioframe->GetSampleFrameCount());
 * 
 *   // Get the fingerprints, encapsulated as the spec mandates.
 *   uint8_t section[512];
 *   uint32_t usedLength = 0;
 *   klsmpte2064_encapsulation_pack(hdl, section, sizeof(section), &usedLength);
 * }
 * klsmpte2064_context_free(hdl);
 * @endcode
 *
 * @section license_sec License
 * This project is licensed under LGPL v2.1 License - see the LICENSE file for details.
 *
 * @section contact_sec Contact
 * Copyright (c) 2025 Kernel Labs Inc. All Rights Reserved.  
 * Author: Steven Toth <stoth@kernellabs.com>
 * GitHub: https://github.com/stoth68000/libklsmpte2064
 */
#ifndef _LIBKLSMPTE2064_CORE_H
#define _LIBKLSMPTE2064_CORE_H

#include <stdint.h>
#include <stdarg.h>
#include <sys/errno.h>

#include <libklsmpte2064/export.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Major version of the public API. */
#define KLSMPTE2064_VERSION_MAJOR 1
/** Minor version of the public API. */
#define KLSMPTE2064_VERSION_MINOR 0
/** Patch version of the public API. */
#define KLSMPTE2064_VERSION_PATCH 0

/** Direct 16x60 WSS luma input is available. */
#define KLSMPTE2064_CAP_DIRECT_WSS_LUMA (1u << 0)
/** YUV420P CPU WSS extractor is available. */
#define KLSMPTE2064_CAP_WSS_EXTRACT_YUV420P (1u << 1)
/** V210 CPU WSS extractor is available. */
#define KLSMPTE2064_CAP_WSS_EXTRACT_V210 (1u << 2)
/** Context, audio, and video reset APIs are available. */
#define KLSMPTE2064_CAP_RESET_APIS (1u << 3)
/** Format support probing APIs are available. */
#define KLSMPTE2064_CAP_FORMAT_PROBING (1u << 4)
/** Context status query API is available. */
#define KLSMPTE2064_CAP_STATUS_API (1u << 5)
/** Raw fingerprint query API is available. */
#define KLSMPTE2064_CAP_RAW_FINGERPRINT_API (1u << 6)

/**
 * @brief Video input format identifiers.
 */
enum klsmpte2064_colorspace_e
{
	COLORSPACE_UNDEFINED = 0,
	COLORSPACE_YUV420P,       /**< Most commonly used with 8 bit codecs. */
	COLORSPACE_V210,          /**< Most commonly used with Decklink SDI cards. */
	COLORSPACE_MAX,
};

/**
 * @brief Return the library version string.
 *
 * @return Static semantic version string for the linked library.
 */
KLSMPTE2064_API const char *klsmpte2064_version_string(void);

/**
 * @brief Return the library version components.
 *
 * Any output pointer may be NULL.
 *
 * @param[out] major Receives the major version.
 * @param[out] minor Receives the minor version.
 * @param[out] patch Receives the patch version.
 */
KLSMPTE2064_API void klsmpte2064_version(uint32_t *major,
	uint32_t *minor,
	uint32_t *patch);

/**
 * @brief Return supported integration capability flags.
 *
 * The returned mask uses KLSMPTE2064_CAP_* values and lets callers verify at
 * runtime that the linked library supports APIs needed by an integration.
 *
 * @return Bitmask of KLSMPTE2064_CAP_* flags.
 */
KLSMPTE2064_API uint32_t klsmpte2064_capabilities(void);

/**
 * @brief	    Allocate a unique handle for the framework, for use with further calls.
 *              The library supports all of the colorspace formats listed in the enum, a 8 or 10 bit depth
 *              packing. Most 8 bit codec typically take YUV420P, 8 bit. If you want higher levels of depth
 *              use V210.
 * @param[out] hdl Receives the allocated context handle.
 * @param[in] colorspace Typically COLORSPACE_YUV420P or COLORSPACE_V210.
 * @param[in] progressive Boolean. Currently only progressive video is supported.
 * @param[in] width Video width in pixels.
 * @param[in] height Video height in pixels.
 * @param[in] stride Size of each source video line in bytes.
 * @param[in] bitdepth Either 8 or 10. COLORSPACE_YUV420P is 8, V210 is 10.
 * @return      0 - Success
 * @return      < 0 - Error
 *
 * Threading: a context is not internally synchronized. Calls that operate on
 * the same context must be serialized by the caller. Separate contexts may be
 * used concurrently from different threads.
 */
KLSMPTE2064_API int klsmpte2064_context_alloc(void **hdl,
	enum klsmpte2064_colorspace_e colorspace,
	uint32_t progressive,
	uint32_t width,
	uint32_t height,
	uint32_t stride,
	uint32_t bitdepth);

/**
 * @brief	    Allocate a context for direct windowed-luma video input.
 *
 * This mode is intended for GPU or hardware pipelines that can efficiently
 * extract the SMPTE 2064 windowed sub-sampling set themselves and submit it via
 * klsmpte2064_video_push_wss_luma(). The context still owns video motion
 * history, audio fingerprinting, and encapsulation state, but it does not
 * allocate full-frame luma scratch buffers and does not accept
 * klsmpte2064_video_push().
 *
 * Use klsmpte2064_video_get_wss_geometry() after allocation to get the exact
 * source rows, columns, and prefilter taps required for the configured video
 * dimensions.
 *
 * @param[out] hdl Receives the allocated context handle.
 * @param[in] progressive Boolean. Currently only progressive video is supported.
 * @param[in] width Video width in pixels.
 * @param[in] height Video height in pixels.
 * @return 0 on success.
 * @return < 0 on error.
 *
 * Threading: a context is not internally synchronized. Calls that operate on
 * the same context must be serialized by the caller. Separate contexts may be
 * used concurrently from different threads.
 */
KLSMPTE2064_API int klsmpte2064_context_alloc_wss_luma(void **hdl,
	uint32_t progressive,
	uint32_t width,
	uint32_t height);


/**
 * @brief	    Raise (1) or lower (0) the overal level of console debug from the library.
 *              The default is zero, no console output under normal operating conditions.
 * @param[in] hdl A previously allocated context handle.
 * @param[in] level Verbosity level, normally 0 or 1.
 * @return      0 - Success
 * @return      < 0 - Error
 */
KLSMPTE2064_API int klsmpte2064_context_set_verbose(void *hdl, int level);

/**
 * @brief Reset all fingerprint state in a context.
 *
 * This clears video motion history, audio fingerprint data, cached audio
 * timebase selection, and encapsulation sequence state without reallocating the
 * context. Use this for source replacement or a major stream discontinuity.
 * This function performs no dynamic allocation.
 *
 * @param[in] hdl A previously allocated context handle.
 * @return 0 on success.
 * @return -EINVAL when hdl is NULL.
 */
KLSMPTE2064_API int klsmpte2064_context_reset(void *hdl);

/**
 * @brief	    Free a previously allocated handle.
 * @param[in] hdl A previously allocated context handle.
 */
KLSMPTE2064_API void klsmpte2064_context_free(void *hdl);

#ifdef __cplusplus
};
#endif

#endif /* _LIBKLSMPTE2064_CORE_H */
