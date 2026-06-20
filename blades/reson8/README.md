# reson8 — The Kain DAW

> The world's first Kain-native Digital Audio Workstation. Compiler-owned state, journaled undo, zero-copy metering, Python+ML interop, GPU-accelerated UI.

## Architecture

reson8's state is organized into four compiler-owned `world` blocks:

- **MixerWorld** — Transport, meters, tempo, loop, session, peak/rms
- **PluginWorld** — Plugin registry, slots, scan paths (3 lanes: Kain-native, VST3/CLAP, Python)
- **ThemeWorld** — 80+ color/text/spacing/animation properties, fully themable
- **ProjectWorld** — File path, history, undo metadata

All UI reads go through entangle mirrors — zero lock contention.
Patches journal every mutation — full undo/redo history.
Laws verify invariants at compile time.

## Infrastructure

- **C bridges:** `src/bridge/native/` — 3 bridge pairs (audio_device, vst3_host, clap_host)
- **Vendored SDKs:** `3rdparty/` — miniaudio, vst3_sdk, clap
- **Python plugins:** `python_plugins/` — Demucs, Matchering, RNNoise wrappers

## Build

```bash
kain check X:\reson8\src\ --json
kain build X:\reson8\ --target llvm
kain run X:\reson8\ --target llvm
```

## Development Streams

| Stream | Name | Status |
|--------|------|--------|
| STREAM1 | Foundation & State Authority | In progress |
| STREAM2 | Orchestrate & DSP | Pending |
| STREAM3 | UI Components | Pending |
| STREAM4 | Actors & System Integration | Pending |
