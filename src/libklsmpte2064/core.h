/**
 * @file	core.h
 * @author	Steven Toth <stoth@kernellabs.com>
 * @copyright	Copyright (c) 2025 Kernel Labs Inc. All Rights Reserved.
 * @brief	A full SMPTE-2064 implementation based on SMPTE ST 2064-1:2015
 *
 * Most likely use case:
 * void *hdl;
 * 
 * klsmpte2064_context_alloc(&hdl, COLORSPACE_V210, 1280, 720, strideBytes, 10);
 * 
 * while (frameArrived) {
 *   // Process Video
 *   const uint8_t *videoplane = NULL; 
 *   videoframe->GetBytes((void **)&v);
 *   klsmpte2064_video_push(hdl, videoplane);
 * 
 *   // Process Audio - We want a stereo finger print
 *   // Takes audio channels 1-2 and fingerprint those.
 *   const int32_t *audiobytes = NULL;
 *   audioframe->GetBytes((void **)&audioplane);
 *   const int16_t *planes[] = { (int16_t *)audiobytes };
 *   klsmpte2064_audio_push(hdl, AUDIOTYPE_STEREO_S32_CH16_DECKLINK, 1001, 6000, &planes[0], 1, audioframe->GetSampleFrameCount());
 *  
 *   // Process Audio - We want a 5.1 discrete S312 fingerprint
 *   // Takes audio channels 1-6 in a SMPTE 312 channel format and fingerprint those.
 *   const int32_t *audiobytes = NULL;
 *   audioframe->GetBytes((void **)&audioplane);
 *   const int16_t *planes[] = { (int16_t *)audiobytes };
 *   klsmpte2064_audio_push(hdl, AUDIOTYPE_SMPTE312_S32_CH16_DECKLINK, 1001, 6000, &planes[0], 1, audioframe->GetSampleFrameCount());
 * 
 *   // Get the fingerprints
 *   uint8_t section[512];
 *   uint32_t usedLength = 0;
 *   klsmpte2064_encapsulation_pack(hdl, section, sizeof(section), &usedLength);
 * }
 * klsmpte2064_context_free(hdl);
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
