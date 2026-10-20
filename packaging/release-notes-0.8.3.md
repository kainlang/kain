## Kain v0.8.3 — first dual-platform dumb-build release

Portable `kain 0.8.3` compilers for **Windows x64** and **Linux x64**, both built
with `!_DUMB_BUILD_KAIN_COMPILER.py` (plain `cargo build --release` + direct
`clang` C runtime — no Bazel). Each archive was verified by extracting it fresh
and running `kain run hello.kn --target llvm` end-to-end (compile → link → run).

### Downloads

| File | Size | Type |
|------|------|------|
| `kain-0.8.3-windows-x64.zip` | 124.8 MB | Portable zip (Windows x64) |
| `kain-0.8.3-linux-x64.tar.gz` | 97.4 MB | Portable tarball (Linux x64) |

```
kain-0.8.3-windows-x64.zip  sha256 91675dc2aeafce46c811243849543f150ce6d61716d406defb4a461ce45bcaac
kain-0.8.3-linux-x64.tar.gz sha256 b9448671bbdad0d780afedaf64e61a9fc71283542758126a40b1a776493af348
```

### Quick start — Windows

1. Extract `kain-0.8.3-windows-x64.zip` anywhere.
2. Add its `bin/` to PATH.
3. Run `kain --version` (reports `kain 0.8.3`).

### Quick start — Linux

1. `tar xzf kain-0.8.3-linux-x64.tar.gz` anywhere.
2. Add its `bin/` to PATH.
3. Run `kain --version`.

### Prerequisites (not bundled)

- `clang` 21 + a C archiver (`ar`/`llvm-ar`) on PATH (needed at link time).
- Linux: `libasound2` for audio programs.
- Windows: MSVC Build Tools *or* MinGW-w64 headers (needed only to *rebuild*
  the runtime; using the prebuilt `lib/kain_runtime.lib` needs nothing extra).

### What's inside

- `bin/kain` — the compiler (+ `kn` alias, `blade`)
- `lib/kain_runtime.lib` / `lib/libkain_runtime.a` — precompiled native C runtime
- `runtime/` — manifest + native C sources (the compiler hashes them per build)
- `stdlib/` — 65+ stdlib modules (147 `.kn` files) + `stdlib.kn` bundle
- Windows `bin/*.dll` — `libclang`, `python312`, `vcruntime140` (no system deps)

### Fixes since v0.8.1/v0.8.2 (found dogfooding the Linux bring-up)

- **Linux runtime quarantine over** — `audio_system.c` (ALSA include order,
  `poll.h`, rawmidi mode), `vulkan_surface_shim.c` + `vulkan_stubs.c`
  (missing `stddef.h`), `cpu.c` (`_GNU_SOURCE`), `ui_widget.c` (Win32 cursor +
  key polling guarded; builds + runs headless).
- **Runtime source selection is manifest-driven** — the dumb script compiles
  exactly `runtime/native_core_runtime.toml` (`sources` + per-OS list, 84 files
  Windows / 81 Linux). The old glob subset silently dropped `cuda_runtime.c`,
  `ui/*.c`, `platform_library.c` → undefined symbols at user link time.
- **Freestanding stubs excluded** — `freestanding_stubs.c` (bare-metal
  `-nostdlib` lane) duplicated `core.c` symbols → `lld-link: duplicate symbol`.
- **Link order fixed** — IR input now precedes static archives
  (`crates/build/src/native_link.rs`); GNU ld resolves left-to-right, so the
  old order linked on Windows (lld) but failed everywhere else.
- **WSL-safe builds** — isolated `target/wsl` cargo dir (shared checkouts hit
  `E0514` cache poison), detached-launch wrappers, `lib*.a` verify glob.
- New tooling: `scripts/package-release.py` (stages + archives either platform),
  `scripts/wsl-setup-kain.sh` (apt LLVM 21 + rustup + ALSA).

Built from `master` @ `228ba1770`. `kain --version` → `kain 0.8.3` on both.
