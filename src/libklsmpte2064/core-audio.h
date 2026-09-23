/**
 * @file	core-audio.h
 * @author	Steven Toth <stoth@kernellabs.com>
 * @copyright	Copyright (c) 2025 Kernel Labs Inc. All Rights Reserved.
 * @brief	Processing audio content
 */

#ifndef _LIBKLSMPTE2064_CORE_AUDIO_H
#define _LIBKLSMPTE2064_CORE_AUDIO_H

#include <stdint.h>
#include <stdarg.h>
#include <sys/errno.h>

#include <libklsmpte2064/export.h>
#include <libklsmpte2064/core.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Supported audio fingerprint input layouts.
 */
enum klsmpte2064_audio_type_e
{
    AUDIOTYPE_UNDEFINED = 0,
    AUDIOTYPE_STEREO_S16P,                /**< Stereo pair of signed 16bit in planar format. */
    AUDIOTYPE_STEREO_S32_CH16_DECKLINK,   /**< Decklink SDI input native format, 16 channels, interleaved S32le */
    AUDIOTYPE_SMPTE312_S32_CH16_DECKLINK, /**< SMPTE312M discrete audio 5.1 downmix.  */
    AUDIOTYPE_MAX
};

/**
 * @brief	    Push an audio frame into the solution for processing.
 *              Support for 48KHz signed.
 *              Sample count should never technically exceed 2200, it's yypically 800/801
 *              for 59.94 framerate video. The timebase 1001/60000 is important, don't abuse this.
 * @param[in] hdl A previously allocated context handle.
 * @param[in] type Audio input layout, for example AUDIOTYPE_STEREO_S16P.
 * @param[in] timebase_num Video timebase numerator, for example 1 or 1001.
 * @param[in] timebase_den Video timebase denominator, for example 60 or 60000.
 * @param[in] planes Array of audio plane pointers.
 * @param[in] planeCount Number of entries in planes.
 * @param[in] sampleCount Number of samples per channel in the planes.
 * @return      0 - Success
 * @return      < 0 - Error
 */
KLSMPTE2064_API int klsmpte2064_audio_push(klsmpte2064_context *hdl, enum klsmpte2064_audio_type_e type,
    uint32_t timebase_num, uint32_t timebase_den,
    const int16_t *planes[], uint32_t planeCount, uint32_t sampleCount);

/**
 * @brief Reset one audio fingerprint slot.
 *
 * Use this when an audio stream has a discontinuity, source switch, seek, or
 * reconnect and the next audio fingerprint should not reuse pre-discontinuity
 * state for the selected audio type.
 * This function performs no dynamic allocation.
 *
 * @param[in] hdl A previously allocated context handle.
 * @param[in] type Audio fingerprint type to reset.
 * @return 0 on success.
 * @return -EINVAL on invalid handle or type.
 */
KLSMPTE2064_API int klsmpte2064_audio_reset(klsmpte2064_context *hdl, enum klsmpte2064_audio_type_e type);

#ifdef __cplusplus
};
#endif

#endif /* _LIBKLSMPTE2064_CORE_AUDIO_H */
