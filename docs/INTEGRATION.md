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
