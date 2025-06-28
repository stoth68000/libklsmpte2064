/**
 * @file	core-encapsulation.h
 * @author	Steven Toth <stoth@kernellabs.com>
 * @copyright	Copyright (c) 2025 Kernel Labs Inc. All Rights Reserved.
 * @brief	A full SMPTE-2064 implementation based on SMPTE ST 2064-1:2015
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
 * @param[in]	void * - A previously allocated content/handle
 * @param[in]   uint8_t * - user supplied buffer a minimum of 256 bytes long
 * @param[in]   uint32_t - buffer length in bytes
 * @param[out]  uint32_t * - number of bufer bytes used
 * @return      0 - Success
 * @return      < 0 - Error
 */
int klsmpte2064_encapsulation_pack(void *hdl, uint8_t *data, uint32_t len, uint32_t *usedLength);

#ifdef __cplusplus
};
#endif

#endif /* _LIBKLSMPTE2064_CORE_ENCAPSULATION_H */
