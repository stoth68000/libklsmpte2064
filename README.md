# libklsmpte2064

libklsmpte2064 generates SMPTE ST 2064-style audio and video fingerprints and
can pack them into the library's SMPTE 2064 fingerprint container format. The
library is written in C and exposes a small public API through:

```c
#include <libklsmpte2064/klsmpte2064.h>
```

The current implementation is aimed at progressive video workflows and
GPU-native integrations.

## Current Capabilities

- Progressive video fingerprinting for supported SMPTE 2064 geometry tables.
- CPU frame input for 8-bit YUV420P luma and packed 10-bit V210.
- Direct 16x60 windowed-subsampled luma input for GPU pipelines.
- Geometry queries that tell applications exactly which source luma pixels to
  sample.
- CPU reference extractors for validating GPU samplers.
- Audio fingerprinting for:
  - stereo signed 16-bit planar 48 kHz audio,
  - DeckLink-style 16-channel interleaved S32 stereo,
  - DeckLink-style 16-channel interleaved S32 SMPTE 312 5.1 downmix.
- Configurable encapsulation metadata, including picture-rate code and ID
  payload.
- Status and raw fingerprint query APIs for applications that do not always
  need a packed section.
- Reset APIs for source switches, seeks, reconnects, or discontinuities.
- Public symbol export macros and hidden-symbol builds so installed headers
  define the ABI surface.

Interlaced video is not currently supported by the public allocation APIs.

## GPU-Style Direct WSS Flow

The direct-WSS path is the preferred integration route for GPU based application. Instead of
bringing a full decoded luma frame back to the CPU, the application queries the
sampling geometry once, uses the GPU to extract only the required luma taps,
and pushes the resulting 16x60 8-bit sample block.

```c
void *hdl = NULL;
struct klsmpte2064_video_wss_geometry geometry;
uint8_t samples[KLSMPTE2064_WSS_ROWS][KLSMPTE2064_WSS_SAMPLES_PER_ROW];

klsmpte2064_context_alloc_wss_luma(&hdl, 1, width, height);
klsmpte2064_video_get_wss_geometry(hdl, &geometry);

/* GPU or CPU fills samples[r][c] from geometry.rows/columns/taps. */

klsmpte2064_video_push_wss_luma(hdl, samples);
```

See `docs/INTEGRATION.md` for the complete integration flow and
`docs/GPU-SAMPLER-TEST-VECTORS.md` for deterministic vectors that validate a
GPU sampler against the CPU reference extractor.

## Hot Path Behavior

After context allocation, these GPU integration calls perform no dynamic
allocation:

- `klsmpte2064_video_get_wss_geometry`
- `klsmpte2064_video_extract_wss_luma_yuv420p`
- `klsmpte2064_video_extract_wss_luma_v210`
- `klsmpte2064_video_push_wss_luma`
- `klsmpte2064_context_status`
- `klsmpte2064_fingerprint_get`
- `klsmpte2064_encapsulation_set_metadata`
- `klsmpte2064_encapsulation_get_metadata`
- `klsmpte2064_encapsulation_pack`
- `klsmpte2064_video_reset`
- `klsmpte2064_audio_reset`
- `klsmpte2064_context_reset`

`klsmpte2064_audio_push` may resize internal audio work buffers if a call uses
a `sampleCount` larger than the context's current audio capacity.

## Build

Initialize the autotools build, configure, and build:

```sh
./autogen.sh --build
./configure --enable-shared=no
make
```

The package installs a `libklsmpte2064.pc` file for `pkg-config`.

## Test

Run the unit and utility checks:

```sh
make check
```

`make check` prints each check it runs and a detailed total/pass/fail summary.

Run the lightweight local benchmark:

```sh
make bench
```

The benchmark covers direct WSS push, status/raw fingerprint queries,
encapsulation packing, and the YUV420P WSS CPU extractor.

## Documentation

Generate Doxygen documentation:

```sh
make docs
```

Open:

```sh
doxygen-generated/html/index.html
```

Useful source documents:

- `docs/INTEGRATION.md`
- `docs/GPU-SAMPLER-TEST-VECTORS.md`

## Compatibility Checks

Applications can check the feature set at runtime:

```c
if (!klsmpte2064_capabilities_satisfy(
        KLSMPTE2064_GPU_DIRECT_WSS_REQUIRED_CAPABILITIES)) {
    /* Disable direct WSS integration or fail initialization. */
}
```

Build systems can also check the package version and GPU capability mask:

```sh
pkg-config --atleast-version=1.0 libklsmpte2064
pkg-config --variable=gpu_direct_wss_required_capabilities libklsmpte2064
```

## License

LGPL v2.1. See `LICENSE` for the full license text.
