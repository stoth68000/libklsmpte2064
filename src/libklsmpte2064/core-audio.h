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
    AUDIOTYPE_STEREO_S32_CH16_DECKLINK,   /**< Decklink SDI input native format, 16 channels, interleaved S32le */
    AUDIOTYPE_SMPTE312_S32_CH16_DECKLINK,   /**< SMPTE312M discrete audio 5.1 downmix.  */
    AUDIOTYPE_MAX
};

/**
 * @brief	    Push an audio frame into the solution for processing.
 *              Support for 48KHz signed 
 * @param[in]	void * - A previously allocated content/handle
 * @param[in]	enum klsmpte2064_audio_type_e - Eg. AUDIOTYPE_STEREO_S16P
 * @param[in]	uint32_t - timebase_num. Eg. 1 or 1001
 * @param[in]	uint32_t - timebase_den. Eg. 60 or 60000
 * @param[in]	const uint16_t ** - planes. A list of points into audio planes.
 * @param[in]	uint32_t - planeCount
 * @param[in]	uint32_t - Samples (per channel) in the planes. Should never exceed 2200. Typically 800/801 for 59.94.
 * @return      0 - Success
 * @return      < 0 - Error
 */
int klsmpte2064_audio_push(void *hdl, enum klsmpte2064_audio_type_e type,
    uint32_t timebase_num, uint32_t timebase_den,
    const int16_t *planes[], uint32_t planeCount, uint32_t sampleCount);

#ifdef __cplusplus
};
#endif

#endif /* _LIBKLSMPTE2064_CORE_AUDIO_H */
