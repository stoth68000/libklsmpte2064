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

enum klsmpte2064_colorspace_e
{
	COLORSPACE_UNDEFINED = 0,
	COLORSPACE_YUV420P,
	COLORSPACE_V210,
};

/**
 * @brief	    Allocate a unique handle for the framework, for use with further calls.
 * @param[out]	void ** - handle
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
 * @param[in]	int - verbosity level (0 or 1).
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
