# Integration Guide

This page describes the preferred integration path for GPU-native video
applications that want to generate SMPTE 2064 fingerprints without pulling full
decoded luma frames back to the CPU.

## Recommended Flow

Use one `klsmpte2064_context` per independent source stream. The normal path is
to check capabilities, allocate a configured source context, query the sampler
plan once, copy that plan into the GPU pipeline, submit a 16x60 luma block for
each accepted frame, and pack a section only when the returned status says the
context is ready. Reset the relevant context state on source switches, seeks,
reconnects, or decode discontinuities.

The context is not internally synchronized. Calls that operate on the same
context must be serialized by the caller. Separate contexts may be used
concurrently from different threads.

## Capability Checks

At startup, verify that the linked library supports the complete GPU direct-WSS
surface:

```c
uint32_t major = 0;
uint32_t minor = 0;
uint32_t patch = 0;

klsmpte2064_version(&major, &minor, &patch);

if (!klsmpte2064_capabilities_satisfy(
        KLSMPTE2064_GPU_DIRECT_WSS_REQUIRED_CAPABILITIES)) {
    /* Disable direct-WSS integration or fail initialization. */
}
```

Build systems can also check the package version and capability mask:

```sh
pkg-config --atleast-version=1.0 libklsmpte2064
pkg-config --variable=gpu_direct_wss_required_capabilities libklsmpte2064
```

## Source Setup

`klsmpte2064_context_alloc_source()` is the preferred allocator for direct-WSS
integrations. It validates supported geometry, validates the frame-duration
timebase, derives picture-rate metadata when requested, and reserves the initial
audio work-buffer capacity before realtime processing starts.

```c
klsmpte2064_context *hdl = NULL;
struct klsmpte2064_source_config config = {0};
struct klsmpte2064_video_wss_sampler_plan plan = {0};
uint32_t section_size = 0;
int ret = 0;

config.size = sizeof(config);
config.version = KLSMPTE2064_STRUCT_VERSION_1;
config.progressive = 1;
config.width = width;
config.height = height;
config.timebase_num = 1001;
config.timebase_den = 60000;
config.max_audio_sample_count = 2300;

config.metadata_present = 1;
config.metadata.picture_rate = KLSMPTE2064_PICTURE_RATE_UNKNOWN;
config.metadata.id_present = 1;
config.metadata.id_length = 3;
config.metadata.id_data[0] = 'G';
config.metadata.id_data[1] = 'P';
config.metadata.id_data[2] = 'U';

ret = klsmpte2064_context_alloc_source(&hdl, &config);
if (ret < 0) {
    /* Unsupported geometry, unsupported timebase, bad metadata, or OOM. */
    return ret;
}

ret = klsmpte2064_video_get_wss_sampler_plan(hdl, &plan);
if (ret < 0) {
    klsmpte2064_context_free(hdl);
    return ret;
}

ret = klsmpte2064_encapsulation_max_size(hdl, &section_size);
if (ret < 0) {
    klsmpte2064_context_free(hdl);
    return ret;
}
```

If `metadata_present` is zero, the default ID payload is used and the picture
rate is derived from the timebase. If metadata is supplied with
`picture_rate == KLSMPTE2064_PICTURE_RATE_UNKNOWN`, only the picture-rate field
is derived and the rest of the supplied metadata is preserved.

`max_audio_sample_count` may be zero to use the library default. Set it to the
largest expected per-frame audio sample count when the realtime path must avoid
audio work-buffer growth.

## GPU Sampler Plan

The sampler plan is a flattened GPU-friendly form of the SMPTE 2064 windowed
sub-sampling geometry. Each `plan.samples[i]` entry describes one output sample
in `samples[i / 60][i % 60]`. The entry points at one or more absolute source
luma coordinates in `plan.taps[]`; average those taps to produce the output
8-bit luma sample.

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

Use `docs/GPU-SAMPLER-TEST-VECTORS.md` or
`klsmpte2064_video_make_wss_conformance_vector()` to validate an accelerated
sampler against deterministic expected 16x60 blocks before connecting it to
decoded frames.

## Per-Frame Video Path

After the GPU fills the compact 16x60 sample block, submit it and use the
returned status to decide whether a packed section can be emitted:

```c
struct klsmpte2064_video_push_result result = {0};
uint8_t samples[KLSMPTE2064_WSS_ROWS][KLSMPTE2064_WSS_SAMPLES_PER_ROW];
uint8_t section[KLSMPTE2064_ENCAPSULATION_MAX_BYTES];
uint32_t used = 0;

ret = klsmpte2064_video_push_wss_luma_result(hdl, samples, &result);
if (ret < 0) {
    return ret;
}

if (result.status.pack_ready) {
    ret = klsmpte2064_encapsulation_pack_if_ready(hdl,
        section,
        sizeof(section),
        &used);
    if (ret == 0) {
        /* section[0..used) contains the SMPTE 2064 fingerprint container. */
    }
}
```

The video fingerprint becomes packable after enough video history exists. A
not-ready state is reported as `-ENODATA` by
`klsmpte2064_encapsulation_pack_if_ready()`.

## Audio Path

Audio may be pushed before or after the matching video sample block; the library
stores the most recent fingerprint for each supported audio type. The timebase
passed to `klsmpte2064_audio_push()` must match the configured source timebase
after it has been established.

```c
const int16_t *planes[2] = {left, right};

ret = klsmpte2064_audio_push(hdl,
    AUDIOTYPE_STEREO_S16P,
    1001,
    60000,
    planes,
    2,
    sample_count);
```

`klsmpte2064_audio_push()` may allocate if `sample_count` exceeds the current
audio capacity. Use `max_audio_sample_count` in `klsmpte2064_source_config` to
reserve enough capacity during source allocation.

## Metadata

The source config can apply metadata during allocation. To update metadata after
allocation, use `klsmpte2064_encapsulation_set_metadata()`:

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

Set `id_present` to zero to omit the ID sub-container. The picture-rate code
can be derived from a frame-duration timebase:

```c
klsmpte2064_picture_rate_from_timebase(1001,
                                       60000,
                                       &metadata.picture_rate);
```

## Diagnostics

Callers that need telemetry or matching data without packing a SMPTE section can
query status and raw fingerprints:

```c
struct klsmpte2064_fingerprint fp = {0};

if (klsmpte2064_fingerprint_get(hdl, &fp) == 0 && fp.status.video_ready) {
    uint8_t video_fp = fp.video_fingerprint;
    uint8_t stereo_len = fp.audio_length[AUDIOTYPE_STEREO_S16P];
    const uint8_t *stereo_fp = fp.audio[AUDIOTYPE_STEREO_S16P];
}
```

`status.video_frames_pushed` reports how many video fingerprint updates have
been computed. `status.video_ready` and `status.pack_ready` become nonzero
after enough video history exists for encapsulation. `status.audio_ready_mask`
has bit `1 << type` set for each audio fingerprint type with current data.
`status.sequence_counter` is the sequence value that the next pack call will
write, and `status.motion` is the latest video motion score from 0.0 to 1.0.

## Reference Extractors

The CPU extractors are correctness oracles for accelerated samplers. They are
not the preferred production path when a decoded luma surface is already on the
GPU.

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

The lower-level geometry API remains available when callers need the SMPTE 2064
rows, columns, and horizontal prefilter offsets directly. For example, a
1920x1080 progressive source currently samples rows 178 through 898 and columns
399 through 1520 with horizontal offsets -1, 0, and 1.

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

`klsmpte2064_audio_push()` is allocation-free only while `sample_count` is less
than or equal to the context's current audio capacity.

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
