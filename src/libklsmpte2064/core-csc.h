/**
 * @file	core-csc.h
 * @author	Steven Toth <stoth@kernellabs.com>
 * @copyright	Copyright (c) 2025 Kernel Labs Inc. All Rights Reserved.
 * @brief	colorspace conversion
 */

#ifndef _LIBKLSMPTE2064_CORE_CSC_H
#define _LIBKLSMPTE2064_CORE_CSC_H

#include <stdint.h>
#include <stdarg.h>
#include <sys/errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Unpack V210 10-bit luma samples into a 16-bit luma plane.
 *
 * Chroma samples are discarded. The output values use the low 10 bits of each
 * uint16_t sample.
 *
 * @param[in] src Packed V210 source frame.
 * @param[in] src_stride Source stride in bytes.
 * @param[out] y Destination luma plane.
 * @param[in] y_stride Destination stride in samples.
 * @param[in] width Width in pixels.
 * @param[in] height Height in lines.
 */
void v210_planar_unpack_c(const uint32_t *src,
	uint32_t src_stride,
	uint16_t *y,
	uint32_t y_stride,
	uint32_t width,
	uint32_t height);

/**
 * @brief Unpack V210 luma samples into an 8-bit luma plane.
 *
 * Chroma samples are discarded and 10-bit luma is converted to 8-bit by
 * retaining the most significant 8 bits. If lines is non-NULL and lineCount is
 * nonzero, only those source line numbers are converted.
 *
 * @param[in] src Packed V210 source frame.
 * @param[in] src_stride Source stride in bytes.
 * @param[out] y Destination 8-bit luma plane.
 * @param[in] y_stride Destination stride in bytes.
 * @param[in] width Width in pixels.
 * @param[in] height Height in lines.
 * @param[in] lines Optional list of source line numbers to convert.
 * @param[in] lineCount Number of entries in lines.
 */
void v210_planar_unpack_c_to_8b(const uint32_t *src,
	uint32_t src_stride,
	uint8_t *y,
	uint32_t y_stride,
	uint32_t width,
	uint32_t height,
	int *lines,
	int lineCount);

#ifdef __cplusplus
};
#endif

#endif /* _LIBKLSMPTE2064_CORE_CSC_H */
