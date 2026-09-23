# Integration Guide

This page summarizes the integration path intended for GPU-native applications
such as Iris.

## Threading

A libklsmpte2064 context is not internally synchronized. Calls that operate on
the same context must be serialized by the caller. Separate contexts may be used
concurrently from different threads.

For a multi-source application, use one context per independent source stream.
Reset or replace only the context associated with the source that changed.

## Capability Checks

At startup, callers can verify the linked library version and feature set:

```c
uint32_t major = 0;
uint32_t minor = 0;
uint32_t patch = 0;
uint32_t caps = klsmpte2064_capabilities();

klsmpte2064_version(&major, &minor, &patch);

if ((caps & KLSMPTE2064_CAP_DIRECT_WSS_LUMA) == 0 ||
    (caps & KLSMPTE2064_CAP_FORMAT_PROBING) == 0) {
    /* Disable direct WSS integration or fail initialization. */
}
```

Use format probing before allocating a context:

```c
if (!klsmpte2064_video_wss_luma_format_supported(1, width, height)) {
    /* Unsupported dimensions for direct-WSS input. */
}
```

## Direct WSS Luma Flow

The direct-WSS path avoids full-frame CPU luma materialization. The caller
extracts exactly the SMPTE 2064 windowed and prefiltered 16 by 60 luma sample
block, then submits that block to the library.

```c
void *hdl = NULL;
struct klsmpte2064_video_wss_geometry geometry;
uint8_t samples[KLSMPTE2064_WSS_ROWS][KLSMPTE2064_WSS_SAMPLES_PER_ROW];

if (klsmpte2064_context_alloc_wss_luma(&hdl, 1, width, height) < 0) {
    /* Unsupported format or allocation failure. */
}

if (klsmpte2064_video_get_wss_geometry(hdl, &geometry) < 0) {
    /* Should not fail for a valid context. */
}

/*
 * Production GPU path:
 *   Use geometry.rows, geometry.columns, and geometry.prefilter_offsets to
 *   sample the decoded luma plane directly on the GPU. Average only valid taps
 *   and write the resulting 16x60 8-bit luma block into samples.
 */

klsmpte2064_video_push_wss_luma(hdl, samples);
```

## 1920x1080 Sampling Example

For a 1920x1080 progressive source, the geometry describes absolute luma pixel
coordinates in the decoded source image:

```c
geometry.row_count = 16;
geometry.samples_per_row = 60;
geometry.prefilter_tap_count = 3;

geometry.rows[0] = 178;
geometry.rows[1] = 226;
/* ... */
geometry.rows[15] = 898;

geometry.columns[0] = 399;
geometry.columns[1] = 418;
/* ... */
geometry.columns[59] = 1520;

geometry.prefilter_offsets[0] = -1;
geometry.prefilter_offsets[1] = 0;
geometry.prefilter_offsets[2] = 1;
```

The application produces one 8-bit output sample for each row/column pair and
stores it in the same position in the `samples[16][60]` block:

```c
for (uint32_t r = 0; r < geometry.row_count; r++) {
    int y = geometry.rows[r];

    for (uint32_t c = 0; c < geometry.samples_per_row; c++) {
        int x = geometry.columns[c];
        int sum = 0;
        int count = 0;

        for (uint32_t t = 0; t < geometry.prefilter_tap_count; t++) {
            int tap_x = x + geometry.prefilter_offsets[t];

            if (tap_x >= 0 && tap_x < 1920) {
                sum += source_luma[y][tap_x];
                count++;
            }
        }

        samples[r][c] = (uint8_t)(sum / count);
    }
}
```

For example:

- `samples[0][0]` comes from row `178`, column `399`, averaged with horizontal
  taps `398`, `399`, and `400`.
- `samples[0][1]` comes from row `178`, column `418`, averaged with horizontal
  taps `417`, `418`, and `419`.
- `samples[15][59]` comes from row `898`, column `1520`, averaged with
  horizontal taps `1519`, `1520`, and `1521`.

After the block is filled, the application submits it:

```c
klsmpte2064_video_push_wss_luma(hdl, samples);
```

For Iris, the same mapping can be implemented in a Metal kernel: the geometry
arrays are copied once for the source format, the kernel reads the luma texture
at those coordinates, writes the 960 averaged 8-bit values into `samples`, and
the CPU passes that compact block to libklsmpte2064.

## Hot Path Allocation Contract

After successful context allocation, these calls perform no dynamic allocation:

- `klsmpte2064_video_get_wss_geometry`
- `klsmpte2064_video_extract_wss_luma_yuv420p`
- `klsmpte2064_video_extract_wss_luma_v210`
- `klsmpte2064_video_push_wss_luma`
- `klsmpte2064_encapsulation_pack`
- `klsmpte2064_video_reset`
- `klsmpte2064_audio_reset`
- `klsmpte2064_context_reset`

This contract is covered by the unit tests so regressions are caught by
`make check`. `klsmpte2064_audio_push()` is intentionally not part of the
allocation-free guarantee because it may resize internal work buffers if
`sampleCount` exceeds the context's current audio capacity.

After at least three video frames have been pushed, fingerprints can be packed:

```c
uint8_t section[256];
uint32_t used = 0;

if (klsmpte2064_encapsulation_pack(hdl, section, sizeof(section), &used) == 0) {
    /* section[0..used) contains the SMPTE 2064 fingerprint container. */
}
```

## CPU Reference Extractors

The CPU extractors are intended as correctness oracles for accelerated
implementations.

```c
klsmpte2064_video_extract_wss_luma_yuv420p(&geometry,
                                           y_plane,
                                           width,
                                           y_stride,
                                           samples);

klsmpte2064_video_extract_wss_luma_v210(&geometry,
                                        v210_frame,
                                        width,
                                        v210_stride,
                                        samples);
```

Use these helpers in tests to verify that a GPU sampler produces byte-identical
16 by 60 luma blocks. Production GPU pipelines should usually call
klsmpte2064_video_push_wss_luma() with samples extracted directly from the
decoded surface.

## Discontinuities

Use reset APIs when stream history should not cross a source switch, seek,
transport reconnect, or decode discontinuity:

```c
klsmpte2064_video_reset(hdl);                 /* Video history only. */
klsmpte2064_audio_reset(hdl, AUDIOTYPE_STEREO_S16P);
klsmpte2064_context_reset(hdl);               /* Full context state. */
```

`klsmpte2064_context_reset()` clears video history, audio fingerprints, cached
audio timebase, and encapsulation sequence state without reallocating the
context.
