# Golden Reference Files — Kaintana Test Pipeline

## Format

Golden files are raw framebuffer dumps stored as flat binary files:

- **Format:** Byte array of `uint32_t` pixel values, stored in native little-endian byte order
- **Layout:** Row-major order, pixel (x,y) at index `y * width + x`
- **Pixel encoding:** Premultiplied ARGB (`0xAARRGGBB`), matching the null backend's `uint32_t* g_fb` output
- **Size:** `width * height * 4` bytes (e.g., an 80x24 golden = 80 * 24 * 4 = 7,680 bytes)

## Naming Convention

`golden/<test_name>.bin` — each test spec row in `specs/*.tsv` that has a golden comparison references its corresponding `.bin` file by this name. Tests with no golden (marked `-`) skip pixel verification.

## Generation

Goldens are produced by running a known-good build of the test runner against the null backend with the `--record` flag:

```
kaintana-test-runner.exe specs/core.tsv --record --golden-dir golden/
```

This executes each render test, captures the null backend framebuffer after `kt_end()`, and writes the raw pixel data to `golden/<test_name>.bin`.

## Verification

The test runner loads each golden file and performs a byte-for-byte comparison against the current framebuffer:

- **Size mismatch:** Golden file size != `width * height * 4` → fail (size mismatch)
- **Byte mismatch:** First differing byte index reported → pixel diff coordinates
- **Exact match:** All bytes identical → pass

## When to Regenerate

Regenerate golden files when the rendering pipeline changes *intentionally*:

- Color blending formula changes (new DIV255 coefficients, different premultiply strategy)
- Rounded rect SDF algorithm changes
- Default layout behavior changes (padding defaults, alignment defaults)
- New font rendering or text measurement logic
- Clip intersection math changes

Do NOT regenerate goldens to "fix" failing tests — investigate the root cause first. A failing golden test means the rendering pipeline changed in a way that affects pixel output, which may indicate a regression.

## Tooling

The `scripts/generate_goldens.py` script automates regeneration:

```
python scripts/generate_goldens.py --spec specs/core.tsv --bin kaintana-test-runner.exe --out golden/
```

See that script for full usage.

## Integrity

Every golden file should have its SHA-256 checksum recorded in the corresponding spec TSV header for integrity verification:

```
# name=core_smoke    width=80    height=24    golden=a1b2c3d4e5...
```
