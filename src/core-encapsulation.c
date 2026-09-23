#include <libklsmpte2064/klsmpte2064.h>

#include "core-private.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int metadata_is_valid(const struct klsmpte2064_encapsulation_metadata *metadata)
{
	if (!metadata ||
		metadata->picture_rate > KLSMPTE2064_PICTURE_RATE_60 ||
		metadata->id_length > KLSMPTE2064_ENCAPSULATION_ID_MAX_BYTES ||
		(metadata->id_present && metadata->id_length == 0)) {
		return 0;
	}
	return 1;
}

int klsmpte2064_picture_rate_from_timebase(uint32_t timebase_num,
	uint32_t timebase_den,
	uint8_t *picture_rate)
{
	if (!picture_rate || !timebase_num || !timebase_den) {
		return -EINVAL;
	}

	if (timebase_num == 1001 && timebase_den == 24000) {
		*picture_rate = KLSMPTE2064_PICTURE_RATE_23976;
	} else if (timebase_num == 1 && timebase_den == 24) {
		*picture_rate = KLSMPTE2064_PICTURE_RATE_24;
	} else if (timebase_num == 1 && timebase_den == 25) {
		*picture_rate = KLSMPTE2064_PICTURE_RATE_25;
	} else if (timebase_num == 1001 && timebase_den == 30000) {
		*picture_rate = KLSMPTE2064_PICTURE_RATE_2997;
	} else if (timebase_num == 1 && timebase_den == 30) {
		*picture_rate = KLSMPTE2064_PICTURE_RATE_30;
	} else if (timebase_num == 1 && timebase_den == 50) {
		*picture_rate = KLSMPTE2064_PICTURE_RATE_50;
	} else if (timebase_num == 1001 && timebase_den == 60000) {
		*picture_rate = KLSMPTE2064_PICTURE_RATE_5994;
	} else if (timebase_num == 1 && timebase_den == 60) {
		*picture_rate = KLSMPTE2064_PICTURE_RATE_60;
	} else {
		*picture_rate = KLSMPTE2064_PICTURE_RATE_UNKNOWN;
		return -ENOTSUP;
	}

	return 0;
}

int klsmpte2064_encapsulation_set_metadata(klsmpte2064_context *hdl,
	const struct klsmpte2064_encapsulation_metadata *metadata)
{
	struct ctx_s *ctx = (struct ctx_s *)hdl;
	if (!ctx || !metadata_is_valid(metadata)) {
		return -EINVAL;
	}

	ctx->encapsulation_metadata = *metadata;
	ctx->encapsulation_metadata.id_present =
		ctx->encapsulation_metadata.id_present ? 1 : 0;
	if (!ctx->encapsulation_metadata.id_present) {
		ctx->encapsulation_metadata.id_length = 0;
		memset(ctx->encapsulation_metadata.id_data,
			0,
			sizeof(ctx->encapsulation_metadata.id_data));
	}
	return 0;
}

int klsmpte2064_encapsulation_get_metadata(klsmpte2064_context *hdl,
	struct klsmpte2064_encapsulation_metadata *metadata)
{
	struct ctx_s *ctx = (struct ctx_s *)hdl;
	if (!ctx || !metadata) {
		return -EINVAL;
	}

	*metadata = ctx->encapsulation_metadata;
	return 0;
}

/* 6.1 - Table 5 - Container structure */
int klsmpte2064_encapsulation_pack(klsmpte2064_context *hdl, uint8_t *data, uint32_t len, uint32_t *usedLength)
{
	struct ctx_s *ctx = (struct ctx_s *)hdl;
	if (!ctx || !data || !usedLength || len < 256) {
		return -EINVAL;
	}
	if (ctx->fingerprints_calculated < 3) {
		return -ENODATA;
	}

	const struct klsmpte2064_encapsulation_metadata *metadata =
		&ctx->encapsulation_metadata;
	int id_present_flag = metadata->id_present;
	int vfp_present_flag = 1;
	int afp_present_flag = 0;

	/* How many audio fingerprints do we have? */
	for (int i = 0; i < AUDIOTYPE_MAX; i++) {
		if (klbs_get_byte_count(&ctx->fp_bs[i]) > 0) {
			afp_present_flag++;
		}
	}

	/* Never actually called out if reserved means its 0 or 1, we'll go with the mpeg standard of 1 */
	uint32_t reserved = 0xffffffff;

	klbs_write_set_buffer(ctx->bs, data, len);
	klbs_write_bits(ctx->bs, 0x00, 8); /* FP_protocol_version */
	klbs_write_bits(ctx->bs, ctx->sequence_counter++, 8); /* Sequence_Counter */

/* SMPTE S253 Picture_Rate
| Value (binary) | Value (hex) | Frame Rate               |
| -------------- | ----------- | ------------------------ |
| 0000           | 0x0         | Unknown or not specified |
| 0001           | 0x1         | 23.976 fps               |
| 0010           | 0x2         | 24 fps                   |
| 0011           | 0x3         | 25 fps                   |
| 0100           | 0x4         | 29.97 fps                |
| 0101           | 0x5         | 30 fps                   |
| 0110           | 0x6         | 50 fps                   |
| 0111           | 0x7         | 59.94 fps                |
| 1000           | 0x8         | 60 fps                   |
| 1001–1111      | 0x9–0xF     | Reserved                 |
*/

	klbs_write_bits(ctx->bs, 0, 8); /* Length: Come back and update this. */

	klbs_write_bits(ctx->bs, metadata->picture_rate, 4); /* Picture_Rate */
	klbs_write_bits(ctx->bs, reserved, 1); /* Reserved */
	klbs_write_bits(ctx->bs, id_present_flag, 1); /* ID Present Flag */
	klbs_write_bits(ctx->bs, vfp_present_flag, 1); /* VFp Present Flag */
	klbs_write_bits(ctx->bs, afp_present_flag, 1); /* AFp Prsent Flag */

	if (id_present_flag) {
		klbs_write_bits(ctx->bs, reserved, 5); /* Reserved */
		klbs_write_bits(ctx->bs, 0, 3); /* SCType: 0 = ID Sub Container  */
		klbs_write_bits(ctx->bs, reserved, 3); /* Reserved */
		klbs_write_bits(ctx->bs, metadata->id_length, 5); /* Length of ID Data */
		for (uint8_t i = 0; i < metadata->id_length; i++) {
			klbs_write_bits(ctx->bs, metadata->id_data[i], 8);
		}
	}

	if (vfp_present_flag) {
		klbs_write_bits(ctx->bs, reserved, 3); /* Reserved */
		if (ctx->progressive) {
			klbs_write_bits(ctx->bs, 1, 2); /* VF Data Count - 6.3 */
			klbs_write_bits(ctx->bs, 1, 3); /* SCType: 1 = ID Video Fingerprint Container  */
			klbs_write_bits(ctx->bs, ctx->video_fingerprint_data_f4, 8); /* Video Fingerprint Data - Current frame */
		} else {
			klbs_write_bits(ctx->bs, 2, 2); /* VF Data Count - 6.3 */
			klbs_write_bits(ctx->bs, 1, 3); /* SCType: 1 = ID Video Fingerprint Container  */
			klbs_write_bits(ctx->bs, ctx->video_fingerprint_data_f4, 8); /* Video Fingerprint Data */
			klbs_write_bits(ctx->bs, ctx->video_fingerprint_data_f2, 8); /* Video Fingerprint Data */
		}
	}

	if (afp_present_flag) {
		/* Unsupported */

		/* Each call to klsmpte2064_audio_push() gets it own audio fingerprint.
		 * This enables callers to fingerprint the various audio channels
		 * in way they deem necessary:
		 * Eg. 1. English stereo downmix
		 *     2. Spanish stereo downmix.
		 *     3. English 5.1 downmix.
		 * 
		 * Callers are expected to call audio push as many times as they need for the same data,
		 * each getting its own fingerprint, each encapsulated sequentially.
		 * 
		 * The length of a fingerprint in bits is no more than ctx->t3->decimator_factor
		 * which the spec says is either 50 (6.25bytes) or 52 bits (6.5 bytes) exactly.
		 * 
		 */

		klbs_write_bits(ctx->bs, afp_present_flag, 5); /* Audio Fingerprint Count */
		klbs_write_bits(ctx->bs, 0x02, 3); /* SCType: 2 = ID Audio Fingerprint Container  */

		uint8_t audio_fingerprint_id = 0;
		for (int i = 0; i < AUDIOTYPE_MAX; i++) {
			uint32_t len = klbs_get_byte_count(&ctx->fp_bs[i]);
			if (len == 0) {
				continue;
			}

			klbs_write_bits(ctx->bs, audio_fingerprint_id++, 5);

			/* AudioMixType */
			switch (i) {
			case AUDIOTYPE_STEREO_S16P:
				klbs_write_bits(ctx->bs, 0x02, 3); /* Downmix from 2.0-channel audio */
				break;
			case AUDIOTYPE_STEREO_S32_CH16_DECKLINK:
				klbs_write_bits(ctx->bs, 0x02, 3); /* Downmix from 2.0-channel audio */
				break;
			case AUDIOTYPE_SMPTE312_S32_CH16_DECKLINK:
				klbs_write_bits(ctx->bs, 0x05, 3); /* Downmix from 5.1-channel audio */
				break;
			default:
				klbs_write_bits(ctx->bs, 0x0, 3); /* Reserved for future use by SMPTE */
			}

			klbs_write_bits(ctx->bs, len, 5); /* AFDataCount */
			klbs_write_bits(ctx->bs, reserved, 3); /* Reserved */

			for (uint32_t j = 0; j < len; j++) {
				klbs_write_bits(ctx->bs, ctx->fp_buffer[i][j], 8); /* Audio Fingerprint Data */
			}

		}
	}

	const uint32_t length_without_checksum = klbs_get_byte_count(ctx->bs);
	const uint32_t packed_length = length_without_checksum + 1;
	*(data + 2) = packed_length;

	uint32_t c = 0;
	for (uint32_t i = 0; i < length_without_checksum; i++) {
		c += *(data + i);
	}
	uint8_t checksum = (uint8_t)(-c & 0xFF);
	klbs_write_bits(ctx->bs, checksum, 8); /* Checksum */

	/* Verify it */
	c = 0;
	for (int i = 0; i < klbs_get_byte_count(ctx->bs); i++) {
		c += *(data + i);
	}
	if ((c & 0xFF) != 0) {
		fprintf(stderr, MODULE_PREFIX "warning, checksum failed verification. Continuing\n");
	}

	klbs_write_buffer_complete(ctx->bs);

	/* "length of the audio and video fingerprint container from the start of the FP_protocol_version
	 *  field to the end of the Checksum field (inclusive)."
	 */
	*usedLength = klbs_get_byte_count(ctx->bs);

	return 0;
}
