#include <libklsmpte2064/klsmpte2064.h>

#include "core-private.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

/* From libklvanc */
#define av_le2ne32(x) (x)

#define READ_PIXELS_10b(a, b, c)         \
    do {                             \
        val  = av_le2ne32( *src++ ); \
        if (a) *a++ =  val & 0x3ff;         \
        if (b) *b++ = (val >> 10) & 0x3ff;  \
        if (c) *c++ = (val >> 20) & 0x3ff;  \
    } while (0)

/* Unpack a line of V210 4:2:2 10bit into three seperate planes.
 * Discard any Chroma, keep ont the luma.
 */
void v210_planar_line_unpack_c(const uint32_t *src, uint16_t *y, int width)
{
	uint32_t val;
	int16_t *x = NULL;

	for (int i = 0; i < width - 5; i += 6) {
			READ_PIXELS_10b(x, y, x);
			READ_PIXELS_10b(y, x, y);
			READ_PIXELS_10b(x, y, x);
			READ_PIXELS_10b(y, x, y);
	}
}

/* Unpack a V210 image into a single unpacket luma field, 10 bits */
void v210_planar_unpack_c_10b(const uint32_t *src, uint32_t src_stride, uint16_t *y, uint32_t y_stride, uint32_t width, uint32_t height)
{
	for (uint32_t i = 0; i < height; i++) {
		const uint32_t *srcline = src + (i * src_stride);
		uint16_t *dstline = y + (i * y_stride);
		v210_planar_line_unpack_c(srcline, dstline, width);
	}
}

#define READ_PIXELS_8b(a, b, c)         \
    do {                             \
        val  = av_le2ne32( *src++ ); \
        if (a) *a++ =  val & 0xff;         \
        if (b) *b++ = (val >> 10) & 0xff;  \
        if (c) *c++ = (val >> 20) & 0xff;  \
    } while (0)

/* Unpack a line of V210 4:2:2 10bit into three seperate planes.
 * Discard any Chroma, keep ont the luma. Decimate from 10 bit to 8bit.
 */
void v210_planar_line_unpack_c_to_8b(const uint32_t *src, uint8_t *y, int width)
{
	uint32_t val;
	int16_t *x = NULL;

	for (int i = 0; i < width - 5; i += 6) {
			READ_PIXELS_8b(x, y, x);
			READ_PIXELS_8b(y, x, y);
			READ_PIXELS_8b(x, y, x);
			READ_PIXELS_8b(y, x, y);
	}
}

void v210_planar_unpack_c_to_8b(const uint32_t *src, uint32_t src_stride, uint8_t *y, uint32_t y_stride, uint32_t width, uint32_t height)
{
	for (uint32_t i = 0; i < height; i++) {
		const uint32_t *srcline = src + (i * src_stride);
		uint8_t *dstline = y + (i * y_stride);
		v210_planar_line_unpack_c_to_8b(srcline, dstline, width);
	}
}

