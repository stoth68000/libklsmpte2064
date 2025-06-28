#include <libklsmpte2064/klsmpte2064.h>

#include "core-private.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 6.1 - Table 5 - Container structure */
int klsmpte2064_encapsulation_pack(void *hdl, uint8_t *data, uint32_t len, uint32_t *usedLength)
{
	struct ctx_s *ctx = (struct ctx_s *)hdl;
	if (!ctx || !data || len < 256) {
		return -EINVAL;
	}
	if (ctx->fingerprints_calculated < 3) {
		return -ENODATA;
	}

	int id_present_flag = 1;
	int vfp_present_flag = 1;
	int afp_present_flag = 0;

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

	klbs_write_bits(ctx->bs, 7, 4); /* Picture_Rate - hardcoded to 59.94 */
	klbs_write_bits(ctx->bs, reserved, 1); /* Reserved */
	klbs_write_bits(ctx->bs, id_present_flag, 1); /* ID Present Flag */
	klbs_write_bits(ctx->bs, vfp_present_flag, 1); /* VFp Present Flag */
	klbs_write_bits(ctx->bs, afp_present_flag, 1); /* AFp Prsent Flag */

	if (id_present_flag) {
		klbs_write_bits(ctx->bs, reserved, 5); /* Reserved */
		klbs_write_bits(ctx->bs, 0, 3); /* SCType: 0 = ID Sub Container  */
		klbs_write_bits(ctx->bs, reserved, 3); /* Reserved */
		klbs_write_bits(ctx->bs, 2, 5); /* Length of ID Data */
		klbs_write_bits(ctx->bs, 'K', 8); /* Arbitrary data */
		klbs_write_bits(ctx->bs, 'L', 8); /* Arbitrary data */
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
	}

	uint32_t c = 0;
	for (int i = 0; i < klbs_get_byte_count(ctx->bs); i++) {
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

	/* Go back and patch the struct to reflext the packed length */
	/* "length of the audio and video fingerprint container from the start of the FP_protocol_version
	 *  field to the end of the Checksum field (inclusive)."
	 */
	*(data + 2) = klbs_get_byte_count(ctx->bs);
	*usedLength = klbs_get_byte_count(ctx->bs);

	return 0;
}
