/**
 * @file	core-video.h
 * @author	Steven Toth <stoth@kernellabs.com>
 * @copyright	Copyright (c) 2025 Kernel Labs Inc. All Rights Reserved.
 * @brief	Processing video content
 */

#ifndef _LIBKLSMPTE2064_CORE_VIDEO_H
#define _LIBKLSMPTE2064_CORE_VIDEO_H

#include <stdint.h>
#include <stdarg.h>
#include <sys/errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief	    Push a video frame into the solution for processing.
 *              During context creation the width, height, depth etc was declared,
 *              pay attension and don't violate that.
 * @param[in]	void * - A previously allocated content/handle
 * @param[in]	const uint8_t * - The luma plane.
 * @return      0 - Success
 * @return      < 0 - Error
 */
int klsmpte2064_video_push(void *hdl, const uint8_t *lumaplane);

#ifdef __cplusplus
};
#endif

#endif /* _LIBKLSMPTE2064_CORE_VIDEO_H */
