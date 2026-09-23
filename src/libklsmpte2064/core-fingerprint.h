/**
 * @file	core-fingerprint.h
 * @author	Steven Toth <stoth@kernellabs.com>
 * @copyright	Copyright (c) 2025 Kernel Labs Inc. All Rights Reserved.
 * @brief	Fingerprint status and raw fingerprint access
 */

#ifndef _LIBKLSMPTE2064_CORE_FINGERPRINT_H
#define _LIBKLSMPTE2064_CORE_FINGERPRINT_H

#include <stdint.h>
#include <stdarg.h>
#include <sys/errno.h>

#include <libklsmpte2064/export.h>
#include <libklsmpte2064/core.h>
#include <libklsmpte2064/core-audio.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum bytes currently stored for one audio fingerprint. */
#define KLSMPTE2064_AUDIO_FINGERPRINT_MAX_BYTES 8

/**
 * @brief Current context readiness and scheduling state.
 *
 * This lets realtime callers decide whether packing would succeed without
 * treating -ENODATA from klsmpte2064_encapsulation_pack() as routine control
 * flow. The fields are a snapshot of the context at the time of the call.
 */
struct klsmpte2064_context_status {
	uint64_t video_frames_pushed; /**< Number of video fingerprints computed. */
	uint32_t video_ready; /**< Nonzero when the video fingerprint is packable. */
	uint32_t audio_ready_mask; /**< Bit i is set when audio type i has data. */
	uint32_t pack_ready; /**< Nonzero when encapsulation can produce a section. */
	uint32_t timebase_num; /**< Cached audio/video timebase numerator, or 0. */
	uint32_t timebase_den; /**< Cached audio/video timebase denominator, or 0. */
	uint8_t sequence_counter; /**< Sequence counter that will be used by the next pack call. */
	double motion; /**< Last video motion score, from 0.0 to 1.0. */
};

/**
 * @brief Raw fingerprint snapshot for diagnostics or non-container workflows.
 *
 * This exposes the current video fingerprint byte, motion score, audio
 * fingerprint bytes, and readiness state without constructing a SMPTE 2064
 * container section. Audio entries are indexed by enum
 * klsmpte2064_audio_type_e; audio_length[type] gives the number of valid bytes
 * in audio[type].
 */
struct klsmpte2064_fingerprint {
	struct klsmpte2064_context_status status; /**< Readiness and metadata snapshot. */
	uint8_t video_fingerprint; /**< Current video fingerprint byte. */
	uint8_t audio_length[AUDIOTYPE_MAX]; /**< Valid byte count for each audio entry. */
	uint8_t audio[AUDIOTYPE_MAX][KLSMPTE2064_AUDIO_FINGERPRINT_MAX_BYTES]; /**< Raw audio fingerprint bytes. */
};

/**
 * @brief Query context readiness and scheduling state.
 *
 * @param[in] hdl A previously allocated context handle.
 * @param[out] status Destination for status information.
 * @return 0 on success.
 * @return -EINVAL when hdl or status is NULL.
 *
 * This function performs no dynamic allocation.
 */
KLSMPTE2064_API int klsmpte2064_context_status(klsmpte2064_context *hdl,
	struct klsmpte2064_context_status *status);

/**
 * @brief Query the current raw fingerprint state.
 *
 * @param[in] hdl A previously allocated context handle.
 * @param[out] fingerprint Destination for raw fingerprint information.
 * @return 0 on success.
 * @return -EINVAL when hdl or fingerprint is NULL.
 *
 * This function performs no dynamic allocation.
 */
KLSMPTE2064_API int klsmpte2064_fingerprint_get(klsmpte2064_context *hdl,
	struct klsmpte2064_fingerprint *fingerprint);

#ifdef __cplusplus
};
#endif

#endif /* _LIBKLSMPTE2064_CORE_FINGERPRINT_H */
