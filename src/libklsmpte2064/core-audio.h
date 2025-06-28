/**
 * @file	core-audio.h
 * @author	Steven Toth <stoth@kernellabs.com>
 * @copyright	Copyright (c) 2025 Kernel Labs Inc. All Rights Reserved.
 * @brief	A full SMPTE-2064 implementation based on SMPTE ST 2064-1:2015
 */

#ifndef _LIBKLSMPTE2064_CORE_AUDIO_H
#define _LIBKLSMPTE2064_CORE_AUDIO_H

#include <stdint.h>
#include <stdarg.h>
#include <sys/errno.h>

#ifdef __cplusplus
extern "C" {
#endif

enum klsmpte2064_audio_type_e
{
    AUDIOTYPE_UNDEFINED = 0,
    AUDIOTYPE_STEREO_S16P,
};

/**
 * @brief	    Push an audio frame into the solution for processing.
 *              Support for 48KHz signed 
 * @param[in]	void * - A previously allocated content/handle
 * @param[in]	const uint16_t ** - list of points into audio planes.
 * @return      0 - Success
 * @return      < 0 - Error
 */
int klsmpte2064_audio_push(void *hdl, enum klsmpte2064_audio_type_e type, double framerate, const int16_t *planes[], uint32_t planeCount, uint32_t sampleCount);

#ifdef __cplusplus
};
#endif

#endif /* _LIBKLSMPTE2064_CORE_AUDIO_H */
