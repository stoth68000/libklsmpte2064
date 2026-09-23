/**
 * @file	core-source.h
 * @author	Steven Toth <stoth@kernellabs.com>
 * @copyright	Copyright (c) 2025 Kernel Labs Inc. All Rights Reserved.
 * @brief	Source-level context configuration helpers
 */

#ifndef _LIBKLSMPTE2064_CORE_SOURCE_H
#define _LIBKLSMPTE2064_CORE_SOURCE_H

#include <stdint.h>
#include <sys/errno.h>

#include <libklsmpte2064/export.h>
#include <libklsmpte2064/core.h>
#include <libklsmpte2064/core-encapsulation.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Direct-WSS source configuration used to allocate a context.
 *
 * This groups the values a realtime integration normally knows at source
 * startup: video geometry, frame-duration timebase, initial audio capacity,
 * and optional encapsulation metadata. The resulting context is configured for
 * direct WSS luma input via klsmpte2064_video_push_wss_luma().
 *
 * Set size to sizeof(struct klsmpte2064_source_config) and version to
 * KLSMPTE2064_STRUCT_VERSION_1. If metadata_present is zero, the default
 * metadata is used and picture_rate is derived from the timebase. If
 * metadata_present is nonzero and metadata.picture_rate is UNKNOWN, only the
 * picture_rate field is derived from the timebase before validation.
 */
struct klsmpte2064_source_config {
	uint32_t size; /**< sizeof(struct klsmpte2064_source_config). */
	uint32_t version; /**< KLSMPTE2064_STRUCT_VERSION_1. */
	uint32_t progressive; /**< Boolean. Currently only progressive is supported. */
	uint32_t width; /**< Source width in pixels. */
	uint32_t height; /**< Source height in pixels. */
	uint32_t timebase_num; /**< Frame-duration timebase numerator. */
	uint32_t timebase_den; /**< Frame-duration timebase denominator. */
	uint32_t max_audio_sample_count; /**< Initial audio capacity, or zero for default. */
	uint32_t metadata_present; /**< Nonzero to apply metadata. */
	uint32_t reserved; /**< Reserved, set to zero. */
	struct klsmpte2064_encapsulation_metadata metadata; /**< Optional metadata. */
};

/**
 * @brief Allocate a direct-WSS source context from one validated configuration.
 *
 * This helper validates direct-WSS video geometry, validates the timebase for
 * audio/picture-rate use, allocates the context, configures the initial audio
 * capacity, and applies or derives encapsulation metadata.
 *
 * @param[out] hdl Receives the allocated context handle.
 * @param[in] config Source configuration.
 * @return 0 on success.
 * @return -EINVAL on invalid arguments or unsupported geometry/timebase.
 * @return -ENOMEM on allocation failure.
 */
KLSMPTE2064_API int klsmpte2064_context_alloc_source(
	klsmpte2064_context **hdl,
	const struct klsmpte2064_source_config *config);

#ifdef __cplusplus
};
#endif

#endif /* _LIBKLSMPTE2064_CORE_SOURCE_H */
