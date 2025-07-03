/**
 * @file	core.h
 * @author	Steven Toth <stoth@kernellabs.com>
 * @copyright	Copyright (c) 2025 Kernel Labs Inc. All Rights Reserved.
 * @brief	Context allocation and destruction
 *
 */

/**
 * @mainpage A library to calculate SMPTE2064 audio and video fingerprints
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

#ifdef __cplusplus
extern "C" {
#endif

enum klsmpte2064_colorspace_e
{
	COLORSPACE_UNDEFINED = 0,
	COLORSPACE_YUV420P,       /**< Most commonly used with 8 bit codecs. */
	COLORSPACE_V210,          /**< Most commonly used with Decklink SDI cards. */
	COLORSPACE_MAX,
};

/**
 * @brief	    Allocate a unique handle for the framework, for use with further calls.
 *              The library supports all of the colorspace formats listed in the enum, a 8 or 10 bit depth
 *              packing. Most 8 bit codec typically take YUV420P, 8 bit. If you want higher levels of depth
 *              use V210.
 * @param[out]	void ** - handle
 * @param[in]	enum klsmpte2064_colorspace_e - Typically COLORSPACE_YUV420P
 * @param[in]	uint32_t progressive - Boolean. Is the video progressive?
 * @param[in]	uint32_t width - in pixels
 * @param[in]	uint32_t height - in pixels
 * @param[in]	uint32_t stride - Size of each line of video in bytes
 * @param[in]	uint32_t bitdepth - either 8 or 10 only. COLORSPACE_YUV420P is 8, V210 is 10.
 * @return      0 - Success
 * @return      < 0 - Error
 */
int klsmpte2064_context_alloc(void **hdl,
	enum klsmpte2064_colorspace_e colorspace,
	uint32_t progressive,
	uint32_t width,
	uint32_t height,
	uint32_t stride,
	uint32_t bitdepth);


/**
 * @brief	    Raise (1) or lower (0) the overal level of console debug from the library.
 *              The default is zero, no console output under normal operating conditions.
 * @param[in]	void * - A previously allocated content/handle
 * @param[in]	int level - verbosity level (0 or 1).
 * @return      0 - Success
 * @return      < 0 - Error
 */
int klsmpte2064_context_set_verbose(void *hdl, int level);

/**
 * @brief	    Free a previously allocated handle.
 * @param[in]	void * - A previously allocated content/handle
 */
void klsmpte2064_context_free(void *hdl);

#ifdef __cplusplus
};
#endif

#endif /* _LIBKLSMPTE2064_CORE_H */
