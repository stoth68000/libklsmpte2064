# GPU Sampler Test Vectors

These vectors help applications validate a GPU implementation of the direct
WSS luma path before wiring it into a realtime pipeline.

## 1920x1080 Progressive YUV420P

Create a 1920x1080 8-bit luma surface with this deterministic pattern:

```c
Y[y][x] = (uint8_t)((x * 3 + y * 5 + seed * 37) & 0xff);
```

Allocate a direct-WSS context for 1920x1080 progressive video and query the
geometry or flattened sampler plan:

```c
klsmpte2064_context_alloc_wss_luma(&hdl, 1, 1920, 1080);
klsmpte2064_video_get_wss_geometry(hdl, &geometry);
klsmpte2064_video_get_wss_sampler_plan(hdl, &plan);
```

The current geometry is:

```c
geometry.rows[0] = 178;
geometry.rows[15] = 898;
geometry.columns[0] = 399;
geometry.columns[59] = 1520;
geometry.prefilter_tap_count = 3;
geometry.prefilter_offsets = { -1, 0, 1 };
```

For each seed, extract the 16x60 block by averaging valid horizontal taps and
store each result in `samples[r][c]`.

| Seed | samples[0][0] | samples[0][1] | samples[15][59] | Byte sum | Byte xor |
|---:|---:|---:|---:|---:|---:|
| 1 | 76 | 133 | 127 | 122431 | 87 |
| 2 | 113 | 170 | 164 | 122368 | 12 |
| 3 | 150 | 207 | 201 | 122389 | 15 |

After pushing seed 1, seed 2, then seed 3 sample blocks with
`klsmpte2064_video_push_wss_luma()`, `klsmpte2064_fingerprint_get()` should
report:

```c
fingerprint.status.video_ready == 1
fingerprint.video_fingerprint == 234
```

The unit test `GPU sampler reference vector 1920x1080` validates this full
block, not only the representative values shown above.

Applications can also ask the library to generate the expected block directly:

```c
struct klsmpte2064_video_wss_conformance_vector vector;

klsmpte2064_video_make_wss_conformance_vector(hdl, seed, &vector);
```

`vector.samples` contains the expected 960-byte output, while
`vector.sample_sum` and `vector.sample_xor` provide quick diagnostics.
