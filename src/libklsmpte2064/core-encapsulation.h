/**
 * @file	core-encapsulation.h
 * @author	Steven Toth <stoth@kernellabs.com>
 * @copyright	Copyright (c) 2025 Kernel Labs Inc. All Rights Reserved.
 * @brief	fingerprint packing into external format
 */

#ifndef _LIBKLSMPTE2064_CORE_ENCAPSULATION_H
#define _LIBKLSMPTE2064_CORE_ENCAPSULATION_H

#include <stdint.h>
#include <stdarg.h>
#include <sys/errno.h>

#include <libklsmpte2064/export.h>
#include <libklsmpte2064/core.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum ID payload bytes supported by the encapsulation ID sub-container. */
#define KLSMPTE2064_ENCAPSULATION_ID_MAX_BYTES 31

/** SMPTE ST 2064/S253 picture-rate codes used in packed fingerprint sections. */
enum klsmpte2064_picture_rate_e
{
	KLSMPTE2064_PICTURE_RATE_UNKNOWN = 0x0,
	KLSMPTE2064_PICTURE_RATE_23976 = 0x1,
	KLSMPTE2064_PICTURE_RATE_24 = 0x2,
	KLSMPTE2064_PICTURE_RATE_25 = 0x3,
	KLSMPTE2064_PICTURE_RATE_2997 = 0x4,
	KLSMPTE2064_PICTURE_RATE_30 = 0x5,
	KLSMPTE2064_PICTURE_RATE_50 = 0x6,
	KLSMPTE2064_PICTURE_RATE_5994 = 0x7,
	KLSMPTE2064_PICTURE_RATE_60 = 0x8,
};

/**
 * @brief Map a video frame timebase to a SMPTE 2064 picture-rate code.
 *
 * The helper accepts common frame-duration timebases such as 1001/60000 for
 * 59.94 fps and 1/60 for 60 fps.
 *
 * @param[in] timebase_num Frame-duration numerator.
 * @param[in] timebase_den Frame-duration denominator.
 * @param[out] picture_rate Receives enum klsmpte2064_picture_rate_e.
 * @return 0 on success.
 * @return -EINVAL when picture_rate is NULL or the timebase is zero.
 * @return -ENOTSUP when the timebase has no SMPTE 2064 picture-rate code.
 */
KLSMPTE2064_API int klsmpte2064_picture_rate_from_timebase(
	uint32_t timebase_num,
	uint32_t timebase_den,
	uint8_t *picture_rate);

/**
 * @brief Metadata written into encapsulated fingerprint sections.
 *
 * Defaults preserve the historical library behavior: picture_rate is
 * KLSMPTE2064_PICTURE_RATE_5994, id_present is 1, and id_data is "KL".
 */
struct klsmpte2064_encapsulation_metadata {
	uint8_t picture_rate; /**< enum klsmpte2064_picture_rate_e value, 0..8. */
	uint8_t id_present; /**< Nonzero to include an ID sub-container. */
	uint8_t id_length; /**< Number of valid bytes in id_data, 0..31. */
	uint8_t id_data[KLSMPTE2064_ENCAPSULATION_ID_MAX_BYTES]; /**< ID payload bytes. */
};

/**
 * @brief Configure metadata for future encapsulation pack calls.
 *
 * @param[in] hdl A previously allocated context handle.
 * @param[in] metadata Metadata to copy into the context.
 * @return 0 on success.
 * @return -EINVAL on invalid arguments.
 *
 * This function performs no dynamic allocation.
 */
KLSMPTE2064_API int klsmpte2064_encapsulation_set_metadata(klsmpte2064_context *hdl,
	const struct klsmpte2064_encapsulation_metadata *metadata);

/**
 * @brief Query the current encapsulation metadata configuration.
 *
 * @param[in] hdl A previously allocated context handle.
 * @param[out] metadata Destination for current metadata.
 * @return 0 on success.
 * @return -EINVAL on invalid arguments.
 *
 * This function performs no dynamic allocation.
 */
KLSMPTE2064_API int klsmpte2064_encapsulation_get_metadata(klsmpte2064_context *hdl,
	struct klsmpte2064_encapsulation_metadata *metadata);

/**
 * @brief	    Create a 'container' section describing all of the audio and video fingerprints.
 *              This is then typically embeded into a ISO13818-1 PES or other means of distribution.
 * @param[in] hdl A previously allocated context handle.
 * @param[out] data User-supplied output buffer, minimum 256 bytes.
 * @param[in] len Output buffer length in bytes.
 * @param[out] usedLength Number of output bytes used.
 * @return      0 - Success
 * @return      < 0 - Error
 *
 * This function writes into caller-supplied storage and performs no dynamic
 * allocation.
 */
KLSMPTE2064_API int klsmpte2064_encapsulation_pack(klsmpte2064_context *hdl,
	uint8_t *data,
	uint32_t len,
	uint32_t *usedLength);

#ifdef __cplusplus
};
#endif

#endif /* _LIBKLSMPTE2064_CORE_ENCAPSULATION_H */
