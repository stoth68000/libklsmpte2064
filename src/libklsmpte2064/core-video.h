/**
 * @file	core-video.h
 * @author	Steven Toth <stoth@kernellabs.com>
 * @copyright	Copyright (c) 2025 Kernel Labs Inc. All Rights Reserved.
 * @brief	A full SMPTE-2064 implementation based on SMPTE ST 2064-1:2015
 */

#ifndef _LIBKLSMPTE2064_CORE_VIDEO_H
#define _LIBKLSMPTE2064_CORE_VIDEO_H

#include <stdint.h>
#include <stdarg.h>
#include <sys/errno.h>

#ifdef __cplusplus
extern "C" {
#endif

int klsmpte2064_video_push(void *hdl, const uint8_t *lumaplane);

#ifdef __cplusplus
};
#endif

#endif /* _LIBKLSMPTE2064_CORE_VIDEO_H */
