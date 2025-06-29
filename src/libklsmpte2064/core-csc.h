/**
 * @file	core-csc.h
 * @author	Steven Toth <stoth@kernellabs.com>
 * @copyright	Copyright (c) 2025 Kernel Labs Inc. All Rights Reserved.
 * @brief	A full SMPTE-2064 implementation based on SMPTE ST 2064-1:2015
 */

#ifndef _LIBKLSMPTE2064_CORE_CSC_H
#define _LIBKLSMPTE2064_CORE_CSC_H

#include <stdint.h>
#include <stdarg.h>
#include <sys/errno.h>

#ifdef __cplusplus
extern "C" {
#endif

void v210_planar_unpack_c(const uint32_t *src, uint32_t src_stride, uint16_t *y, uint32_t y_stride, uint32_t width, uint32_t height);
void v210_planar_unpack_c_to_8b(const uint32_t *src, uint32_t src_stride, uint8_t *y, uint32_t y_stride, uint32_t width, uint32_t height, int *lines, int lineCount);

#ifdef __cplusplus
};
#endif

#endif /* _LIBKLSMPTE2064_CORE_CSC_H */
