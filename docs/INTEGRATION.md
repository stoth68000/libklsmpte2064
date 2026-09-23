# Integration Guide

This page summarizes the integration path intended for GPU-native video
applications.

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

klsmpte2064_version(&major, &minor, &patch);

if (!klsmpte2064_capabilities_satisfy(
        KLSMPTE2064_GPU_DIRECT_WSS_REQUIRED_CAPABILITIES)) {
    /* Disable direct WSS integration or fail initialization. */
}
```

Build systems can check the package version and inspect the GPU direct-WSS
integration capability mask through `pkg-config`:

```sh
pkg-config --atleast-version=1.0 libklsmpte2064
pkg-config --variable=gpu_direct_wss_required_capabilities libklsmpte2064
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
klsmpte2064_context *hdl = NULL;
struct klsmpte2064_source_config config = {0};
struct klsmpte2064_video_wss_geometry geometry;
struct klsmpte2064_video_wss_sampler_plan plan;
struct klsmpte2064_video_push_result result;
uint8_t samples[KLSMPTE2064_WSS_ROWS][KLSMPTE2064_WSS_SAMPLES_PER_ROW];

config.size = sizeof(config);
config.version = KLSMPTE2064_STRUCT_VERSION_1;
config.progressive = 1;
config.width = width;
config.height = height;
config.timebase_num = 1001;
config.timebase_den = 60000;
config.max_audio_sample_count = 2300;

if (klsmpte2064_context_alloc_source(&hdl, &config) < 0) {
    /* Unsupported geometry, unsupported timebase, or allocation failure. */
}

if (klsmpte2064_video_get_wss_geometry(hdl, &geometry) < 0) {
    /* Should not fail for a valid context. */
}

if (klsmpte2064_video_get_wss_sampler_plan(hdl, &plan) < 0) {
    /* Should not fail for a valid context. */
}

/*
 * Production GPU path:
 *   Copy plan to a GPU buffer. For each plan.samples[i], average
 *   plan.taps[tap_start .. tap_start + tap_count) from the decoded luma plane
 *   and write the result to samples[i / 60][i % 60].
 */

klsmpte2064_video_push_wss_luma_result(hdl, samples, &result);
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

For GPU pipelines, prefer the flattened sampler plan:

```c
for (uint32_t i = 0; i < plan.sample_count; i++) {
    const struct klsmpte2064_video_wss_sample_plan_entry *entry =
        &plan.samples[i];
    uint32_t sum = 0;

    for (uint32_t t = 0; t < entry->tap_count; t++) {
        const struct klsmpte2064_video_wss_sampler_tap *tap =
            &plan.taps[entry->tap_start + t];

        sum += source_luma[tap->y][tap->x];
    }

    samples[i / KLSMPTE2064_WSS_SAMPLES_PER_ROW]
           [i % KLSMPTE2064_WSS_SAMPLES_PER_ROW] =
        (uint8_t)(sum / entry->tap_count);
}
```

The plan contains only valid absolute luma coordinates for the configured
source dimensions, so GPU kernels do not need per-tap edge checks.

Use `docs/GPU-SAMPLER-TEST-VECTORS.md` to validate a GPU sampler against a
deterministic 1920x1080 luma surface before connecting it to decoded frames.

## Hot Path Allocation Contract

After successful context allocation, these calls perform no dynamic allocation:

- `klsmpte2064_video_get_wss_geometry`
- `klsmpte2064_video_get_wss_sampler_plan`
- `klsmpte2064_video_extract_wss_luma_yuv420p`
- `klsmpte2064_video_extract_wss_luma_v210`
- `klsmpte2064_video_push_wss_luma`
- `klsmpte2064_video_push_wss_luma_result`
- `klsmpte2064_video_make_wss_conformance_vector`
- `klsmpte2064_context_status`
- `klsmpte2064_fingerprint_get`
- `klsmpte2064_encapsulation_set_metadata`
- `klsmpte2064_encapsulation_get_metadata`
- `klsmpte2064_encapsulation_max_size`
- `klsmpte2064_encapsulation_pack`
- `klsmpte2064_encapsulation_pack_if_ready`
- `klsmpte2064_video_reset`
- `klsmpte2064_audio_reset`
- `klsmpte2064_context_reset`

This contract is covered by the unit tests so regressions are caught by
`make check`. `klsmpte2064_audio_push()` is intentionally not part of the
allocation-free guarantee because it may resize internal work buffers if
`sampleCount` exceeds the context's current audio capacity.

After at least three video frames have been pushed, fingerprints can be packed.
Query the maximum required output size instead of hard-coding a buffer length:

```c
uint32_t max_section_size = 0;
uint8_t section[KLSMPTE2064_ENCAPSULATION_MAX_BYTES];
uint32_t used = 0;

klsmpte2064_encapsulation_max_size(hdl, &max_section_size);

if (klsmpte2064_encapsulation_pack_if_ready(hdl,
                                            section,
                                            max_section_size,
                                            &used) == 0) {
    /* section[0..used) contains the SMPTE 2064 fingerprint container. */
}
```

## Encapsulation Metadata

By default, packed sections preserve the original library behavior:

- Picture rate: `KLSMPTE2064_PICTURE_RATE_5994`
- ID sub-container: present
- ID payload: `KL`

Applications should configure this per source before packing so the section metadata
matches the actual source:

```c
struct klsmpte2064_encapsulation_metadata metadata = {0};

metadata.picture_rate = KLSMPTE2064_PICTURE_RATE_60;
metadata.id_present = 1;
metadata.id_length = 3;
metadata.id_data[0] = 'G';
metadata.id_data[1] = 'P';
metadata.id_data[2] = 'U';

klsmpte2064_encapsulation_set_metadata(hdl, &metadata);
```

The picture-rate code can be derived from a frame-duration timebase:

```c
klsmpte2064_picture_rate_from_timebase(1001,
                                       60000,
                                       &metadata.picture_rate);
```

Set `id_present` to zero to omit the ID sub-container. Metadata set/get calls
perform no dynamic allocation.

## Status and Raw Fingerprints

Applications can query readiness before packing:

```c
struct klsmpte2064_context_status status;

if (klsmpte2064_context_status(hdl, &status) == 0 && status.pack_ready) {
    klsmpte2064_encapsulation_pack(hdl, section, sizeof(section), &used);
}
```

`status.video_frames_pushed` reports how many video fingerprint updates have
been computed. `status.video_ready` and `status.pack_ready` become nonzero
after enough video history exists for encapsulation. `status.audio_ready_mask`
has bit `1 << type` set for each audio fingerprint type with current data.
`status.sequence_counter` is the sequence value that the next pack call will
write, and `status.motion` is the latest video motion score from 0.0 to 1.0.

For diagnostics, matching, or application-internal telemetry that does not need a
packed SMPTE section, callers can fetch the raw fingerprint snapshot:

```c
struct klsmpte2064_fingerprint fp;

if (klsmpte2064_fingerprint_get(hdl, &fp) == 0 && fp.status.video_ready) {
    uint8_t video_fp = fp.video_fingerprint;
    uint8_t stereo_len = fp.audio_length[AUDIOTYPE_STEREO_S16P];
    const uint8_t *stereo_fp = fp.audio[AUDIOTYPE_STEREO_S16P];
}
```

Status and raw fingerprint queries perform no dynamic allocation.

For the direct-WSS hot path, callers can push a sample block and collect the
post-push state with one call:

```c
struct klsmpte2064_video_push_result result;

if (klsmpte2064_video_push_wss_luma_result(hdl, samples, &result) == 0 &&
    result.status.pack_ready) {
    klsmpte2064_encapsulation_pack_if_ready(hdl,
                                            section,
                                            sizeof(section),
                                            &used);
}
```

`klsmpte2064_context_alloc_source()` can set the initial audio work-buffer
capacity. Choose a `max_audio_sample_count` large enough for the integration's
expected frame cadence so `klsmpte2064_audio_push()` does not need to resize
buffers on the realtime path.

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

## ABI and Symbol Visibility

Public functions are marked with `KLSMPTE2064_API` from
`libklsmpte2064/export.h`. The library is built with hidden symbol visibility,
so consumers should treat declarations in installed `libklsmpte2064/*.h`
headers as the supported ABI surface. Internal helpers, private structs, and
test-only symbols are not part of the compatibility contract.

Context handles use the opaque `klsmpte2064_context` typedef. New extensible
output structs include `size` and `version` fields; the library writes
`sizeof(struct)` and `KLSMPTE2064_STRUCT_VERSION_1` when filling them.

Applications should include the umbrella header unless they need a narrower
compile boundary:

```c
#include <libklsmpte2064/klsmpte2064.h>
```

## Benchmarking

Run the lightweight benchmark target after performance-sensitive changes:

```sh
make bench
```

The benchmark reports timing for direct WSS push, status/raw fingerprint
queries, encapsulation packing, and the YUV420P CPU reference extractor. These
numbers are intended as a local regression guard, not as a cross-machine
performance contract.
