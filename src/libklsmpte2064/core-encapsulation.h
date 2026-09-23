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

#ifdef __cplusplus
extern "C" {
#endif

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
int klsmpte2064_encapsulation_pack(void *hdl, uint8_t *data, uint32_t len, uint32_t *usedLength);

#ifdef __cplusplus
};
#endif

#endif /* _LIBKLSMPTE2064_CORE_ENCAPSULATION_H */
