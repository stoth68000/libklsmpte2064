/**
 * @file	core.h
 * @author	Steven Toth <stoth@kernellabs.com>
 * @copyright	Copyright (c) 2025 Kernel Labs Inc. All Rights Reserved.
 * @brief	A full SMPTE-2064 implementation based on SMPTE ST 2064-1:2015
 */

#ifndef _LIBKLSMPTE2064_CORE_H
#define _LIBKLSMPTE2064_CORE_H

#include <stdint.h>
#include <stdarg.h>
#include <sys/errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief	    Allocate a unique handle for the framework, for use with further calls.
 * @param[out]	void ** - handle
 * @return      0 - Success
 * @return      < 0 - Error
 */
int klsmpte2064_context_alloc(void **hdl,
	uint32_t isYUV420p,
	uint32_t progressive,
	uint32_t width,
	uint32_t height,
	uint32_t stride,
	uint32_t bitdepth);

int klsmpte2064_context_set_verbose(void *hdl, int level);

/**
 * @brief	    Free a previously allocated handle.
 * @param[out]	void * - handle
 */
void klsmpte2064_context_free(void *hdl);

#ifdef __cplusplus
};
#endif

#endif /* _LIBKLSMPTE2064_CORE_H */
