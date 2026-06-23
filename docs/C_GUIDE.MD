# C Interop >> Usage Guide

> **File**: `docs/C_GUIDE.MD`
> **Read `C.MD` first for the full architecture.** This guide is purely about **writing fast C-interop Kain**.

______________________________________________________________________

## The One Rule

> **The fastest C call is the one you don't make.**

Every C bridge crossing costs *something* :: thunk setup, pointer marshaling, ABI classifier dispatch. The C runtime is fast (libclang extracts 6,294 functions from the real `<windows.h>`, direct `call` instructions for scalar args), but it's still crossing a language boundary.

This guide teaches you how to **cross fewer times, cross faster when you do, and eliminate crossing entirely for hot paths.**

______________________________________________________________________

## Performance Ladder

```
                  ┌─────────────────────────────────────────────────────┐
                  │  0 C calls per iteration                            │
                  │  World state mirror (entangle + read from mirror)   │── FASTEST
                  ├─────────────────────────────────────────────────────┤
                  │  1 C call at startup, 0 in hot loop                 │
                  │  Cache C result in world, read mirror in loop       │── ~5ns/op
                  ├─────────────────────────────────────────────────────┤
                  │  1 C call per iteration (inline scalar)             │
                  │  Native to C thunk via include alias                │── ~3ns/op
                  ├─────────────────────────────────────────────────────┤
                  │  1 C call per iteration (system header)             │
                  │  Registry-backed libclang extraction + link          │── ~5ns/op
                  ├─────────────────────────────────────────────────────┤
                  │  1 C call per iteration (large lib, e.g. SQLite)    │
                  │  Amalgamation auto-compiled, companion .c bound     │── ~10ns/op
                  ├─────────────────────────────────────────────────────┤
                  │  C call per iteration (buffer write)                │
                  │  Shared buffer / zero-copy pointer pass             │── ~1µs/op
                  ├─────────────────────────────────────────────────────┤
                  │  C call per iteration (large struct return)         │
                  │  alloc_zeroed + mem_store + pointer pass            │── depends on size
                  └─────────────────────────────────────────────────────┘
```

______________________________________________________________________

## Strategy 1 - Eliminate the Bridge Entirely

### 1.1 Cache C Results in World + Entangled Mirror

The absolute fastest pattern: **call C exactly once, cache the result in world state, then read from an entangled mirror at zero cost forever.**

```kain
include <math.h> as cmath

world CConstants:
    state pi: Int = 0
    state sqrt_2: Int = 0
    state sqrt_3: Int = 0
    state sqrt_5: Int = 0
    state sqrt_7: Int = 0

world CConstantsMirror:
    state pi_copy: Int = 0
    state sqrt_2_copy: Int = 0
    state sqrt_3_copy: Int = 0
    state sqrt_5_copy: Int = 0
    state sqrt_7_copy: Int = 0

entangle CConstants.pi <-> CConstantsMirror.pi_copy with single_writer
entangle CConstants.sqrt_2 <-> CConstantsMirror.sqrt_2_copy with single_writer
entangle CConstants.sqrt_3 <-> CConstantsMirror.sqrt_3_copy with single_writer
entangle CConstants.sqrt_5 <-> CConstantsMirror.sqrt_5_copy with single_writer
entangle CConstants.sqrt_7 <-> CConstantsMirror.sqrt_7_copy with single_writer

// ── Seed: ONE C bridge crossing per value, EVER ──
fn seed_c_constants():
    CConstants.pi = cmath_acos(-1.0) as Int
    CConstants.sqrt_2 = cmath_sqrt(2.0) as Int
    CConstants.sqrt_3 = cmath_sqrt(3.0) as Int
    CConstants.sqrt_5 = cmath_sqrt(5.0) as Int
    CConstants.sqrt_7 = cmath_sqrt(7.0) as Int

// ── Hot loop: ZERO C crossings ──
fn hotloop_cached(iterations: Int) -> Int:
    var acc: Int = 0
    var i: Int = 0
    while i < iterations:
        // Read from entangled mirror 〰 no C calls
        let pi_bias = CConstantsMirror.pi_copy
        let sqrt_bias = CConstantsMirror.sqrt_2_copy
        acc = (acc + pi_bias + sqrt_bias + (i % 29)) % 1000000007
        i = i + 1
    return acc
```

**Performance**: ~infinite speedup vs raw C per-iteration calls. The entangled mirror read is a struct field access |-> it compiles to a memory load.

### 1.2 Read Math Constants in Bulk

The benchmark at `benchmark/cases_v2/system_headers.kn` shows the full pattern for calling `<math.h>` functions through the system header registry:

```kain
include <math.h> as cmath

fn system_header_math_wave_checksum(iterations: Int, modulus: Int) -> Int:
    let acc = 0
    let index = 0
    while index < iterations:
        let lane = (index % 4096) + 1
        let angle = (lane % 720) as Float * 0.00872664625
        let root = cmath_sqrt(lane as Float)
        let wave = cmath_sin(angle) + cmath_cos(angle * 0.5)
        let scaled = cmath_floor((root + wave + 2.0) * 100000.0) as Int
        acc = (acc + scaled + ((index % 97) * 31)) % modulus
        index = index + 1
    return acc
```

This calls 4 C functions per iteration (120,000 iterations = 480,000 C calls in the benchmark). The system header registry auto-resolves linker dependencies ~> `<math.h>` links `-lm` on Linux, auto-resolves to the toolchain default CRT on Windows.

______________________________________________________________________

## Strategy 2 :: The Minimal Include

### 2.1 Local Header + Companion .c

This is the simplest possible C interop: one header, one `.c` source, and one Kain file. **No `KAIN.toml`, no tier, no `[c_ffi]` config.**

```kain
// src/main.kn
include native/math.h as m

fn main() -> Int:
    let sum = m_add(3, 4)
    let prod = m_mul(5, 6)
    if sum != 7:
        return -1
    if prod != 30:
        return -2
    return 0
```

The companion `.c` file sits next to its header:

```c
// src/native/math.h
#pragma once

int math_add(int a, int b);
int math_mul(int a, int b);
```

```c
// src/native/math.c
#include "math.h"

int math_add(int a, int b) {
    return a + b;
}

int math_mul(int a, int b) {
    return a * b;
}
```

### 2.2 How Companion Discovery Works

The compiler sees `include native/math.h as m`, resolves `native/math.h` relative to the source file, discovers `native/math.c` as a sibling, and automatically:

1. Parses `math.h` via libclang (or lang_c AST, or regex fallback)
1. Generates an alias extern surface with `m_` prefix (e.g. `m_add`, `m_mul`)
1. Compiles `math.c` as a translation unit into the native link
1. Binds the real C symbols via `@link_name` on the alias thunks

**This is the canonical shape for local C wrappers.** Zero manifest, zero ceremony.

### 2.3 Natural Include (Slightly More Complex)

From `blades/c/include-natural`:

```kain
include native/native_math.h as nm

fn main() -> Int:
    let mixed = nm_mix(7, 11)
    let folded = nm_fold(mixed, 3)
    if folded != 131:
        return folded
    println("include_native_ok")
    return 0
```

With its companion C:

```c
// native/native_math.h
#ifndef KAIN_INCLUDE_NATURAL_NATIVE_MATH_H
#define KAIN_INCLUDE_NATURAL_NATIVE_MATH_H

int native_math_mix(int a, int b);
int native_math_fold(int seed, int rounds);

#endif
```

```c
// native/native_math.c
#include "native_math.h"

int native_math_mix(int a, int b) {
    return (a * 9) + (b * 5) + 1;
}

int native_math_fold(int seed, int rounds) {
    int value = seed;
    for (int i = 0; i < rounds; ++i) {
        value = value + (i * 7) - 3;
    }
    return value;
}
```

The `nm_` prefix tells you which include alias owns each function. `nm_mix` → from `native/native_math.h` as `nm`.

### 2.4 File Layout Rules

Put your companion `.c` beside your `.h`:

```
my_blade/
  src/
    main.kn
    native/
      math.h          ← included by main.kn
      math.c          ← discovered automatically as sibling
```

The compiler discovers the `.c` by taking the header path, replacing `.h` with `.c`, and checking if the file exists. You can override this with `[c_ffi]` metadata for complex cases, but for the common case it Just Works.

______________________________________________________________________

## Strategy 3 === System Headers

### 3.1 What the Registry Knows

Angle-bracket includes (`include <header.h> as alias`) resolve through the system header registry at `crates/c-ffi/system_headers.toml`. The registry knows:

| Family | Headers | Platforms | Auto-Link |
|--------|---------|-----------|-----------|
| `c-runtime-math` | `math.h` | win/linux/mac | `-lm` (linux), CRT default (win) |
| `c-runtime` | `stdio.h`, `stdlib.h`, `string.h`, etc. | win/linux/mac | CRT default (win), `-lc` (linux) |
| `windows-sdk` | Everything in `<windows.h>` | windows | `user32`, `gdi32`, `kernel32`, etc. |
| `vulkan` | `vulkan/vulkan.h` | win/linux/mac | `vulkan-1.lib` (win), `-lvulkan` (linux) |
| `posix` | `sys/mman.h`, `pthread.h`, `unistd.h`, etc. | linux/mac | `-lpthread` (linux), etc. |
| `ffmpeg` | `libavutil/avutil.h`, `libavcodec/avcodec.h`, etc. | win/linux/mac | SDK-defined libs |

### 3.2 `<math.h>` ~~ C Runtime Math

From `benchmark/cases_v2/system_headers.kn`:

```kain
include <math.h> as cmath

fn probe_math():
    let root = cmath_sqrt(144.0)         // → 12.0
    let wave = cmath_sin(1.570796)        // → ~1.0
    let angle = cmath_cos(3.141592)        // → ~-1.0
    let base = cmath_floor(3.999)          // → 3.0
```

The registry resolves `<math.h>` through the `c-runtime-math` family, which:

- On **Windows**: auto-links through the toolchain CRT default
- On **Linux**: appends `-lm`
- On **macOS**: appends `-System`

### 3.3 `<windows.h>` - 6,294 Win32 Functions in One Line

**Libclang parses the REAL Windows SDK header.** No shim, no subset, no hand-holding.

```kain
include <windows.h> as win

fn main() -> Int with Unsafe:
    let result = win_MessageBoxA(
        0,
        "libclang parsed 6294 functions from windows.h!",
        "Kain Win32",
        0
    )
    return if result == 1: 0 else: 99
```

What you get from `include <windows.h> as win`:

- **6,294 function declarations** extracted – every Win32 API in `<windows.h>`
- **`win_` prefix** on every extern: `win_CreateWindowExA`, `win_GetDC`, `win_TextOutA`, `win_MessageBoxA`, etc.
- **Auto-linked** against `user32`, `gdi32`, `kernel32`, `advapi32`, `ole32`, `shell32`, `comdlg32`, `comctl32`, `uuid`, `imm32`, `ws2_32` >> all resolved by the Windows SDK family policy
- **Scalar parameters** work directly: `Int` → `int`, `Float` → `float`, `String` → `const char*`
- **`with Unsafe` required** :: you're calling into the OS, the compiler needs to know

**Known gap**: Windows SAL annotations (`_Check_return_opt_`, `_ACRTIMP`) can block some CRT functions from full extraction. Vulkan, POSIX, and most third-party headers are clean.

### 3.4 `<vulkan/vulkan.h>` * * * 755 Vulkan Functions

From `benchmark/cases_v2/vulkan_loader.kn`:

```kain
include <vulkan/vulkan.h> as vk

fn vulkan_probe() -> Int:
    let self0 = vk_GetInstanceProcAddr(0, "vkGetInstanceProcAddr")
    let create = vk_GetInstanceProcAddr(0, "vkCreateInstance")
    let exts = vk_GetInstanceProcAddr(0, "vkEnumerateInstanceExtensionProperties")
    let layers = vk_GetInstanceProcAddr(0, "vkEnumerateInstanceLayerProperties")
    let bogus = vk_GetInstanceProcAddr(0, "vkDefinitelyNotARealSymbol")

    var lane = 0
    if self0 != 0: lane = lane + 11
    if create != 0: lane = lane + 19
    if exts != 0: lane = lane + 31
    if layers != 0: lane = lane + 37
    if bogus == 0: lane = lane + 41
    return lane
```

The registry resolves `<vulkan/vulkan.h>` through the `vulkan` family:

- Locates the Vulkan SDK via `VULKAN_SDK`, `KAIN_PLATFORM_VULKAN_SDK`, or standard install paths
- Links against `vulkan-1.lib` (Windows), `-lvulkan` (Linux), or `MoltenVK` (macOS)
- All 755 callable Vulkan functions extracted with **zero unsupported entries**

### 3.5 System Header Syntax Summary

```kain
include <math.h> as cmath           // CRT math
include <stdio.h> as cstdio         // CRT stdio (note: SAL may block on Windows)
include <windows.h> as win          // Full Win32 SDK (Windows only)
include <vulkan/vulkan.h> as vk     // Full Vulkan SDK
include <sys/mman.h> as posix       // POSIX mmap (Linux/macOS)
include <pthread.h> as pthread      // POSIX threads (Linux/macOS)
```

______________________________________________________________________

## Strategy 4 -- Large C Libraries (SQLite Pattern)

### 4.1 The Zero-Manifest Amalgamation

SQLite ships as a single `sqlite3.c` amalgamation (9.1 MB) + `sqlite3.h` (674 KB). Kain consumes it with **zero manifest ceremony**:

```kain
include sqlite3.h as sql

fn main() -> Int:
    let version = sql_libversion_number()
    let threadsafe = sql_threadsafe()
    let complete = sql_complete("select 1;")

    if version < 3000000: return 10
    if threadsafe < 0: return 11
    if complete != 1: return 12

    println("sqlite_include_ok")
    return 0
```

### 4.2 How it works

```
sqlite/
  main.kn           ← "include sqlite3.h as sql"
  sqlite3.c         ← 9.1 MB amalgamation (discovered as companion)
  sqlite3.h         ← 674 KB header (parsed by libclang)
  sqlite3ext.h      ← extension API
  KAIN.toml         ← optional, not required for the include itself
```

The compiler:

1. Sees `include sqlite3.h as sql`
1. Discovers `sqlite3.c` as sibling
1. Parses `sqlite3.h` via libclang (full preprocessing, macro expansion)
1. Compiles `sqlite3.c` into the native link
1. Exposes a `sql_`-prefixed extern surface

### 4.3 Wrapping the C Surface in Kain Facades

From `smoketest/src/interop/sqlite_rally.kn`:

```kain
include "../../native/sqlite3.h" as sql
include "../../native/smoketest_sqlite_pingpong.h" as ping

pub fn smoke_sqlite_version() -> Int:
    return sql_libversion_number()

pub fn smoke_sqlite_threadsafe() -> Int:
    return sql_threadsafe()

pub fn smoke_sqlite_keyword_count() -> Int:
    return sql_keyword_count()

pub fn smoke_sqlite_complete(sql_text: String) -> Int:
    return sql_complete(sql_text)

pub fn smoke_sqlite_ping_score(seed: Int, rounds: Int) -> Int:
    return ping_score(seed, rounds)

pub fn smoke_sqlite_ping_signature(seed: Int, rounds: Int) -> String:
    return ping_signature(seed, rounds)
```

**Key insight**: The Kain facade uses the `sql_` alias prefix, but wraps in semantically-named Kain functions (`smoke_sqlite_version()`, not `sql_libversion_number()`). This gives you versioning control, documentation, and type safety.

### 4.4 When SQLite Has Complex Return Types

For functions that return `const char*` strings, the alias extern surface carries `@c_string_return` on the generated thunk. The LLVM lane materializes the raw `i8*` into owned Kain string storage automatically \_\_\_ so `ping_signature(seed, rounds)` returns a real Kain `String`, not a borrowed native pointer.

For functions that need custom setup (e.g. `sqlite3_open` with a `sqlite3**`), write a small C wrapper in a companion `.c` that does the pointer dance, and call the wrapper from Kain. This keeps the Kain side safe.

______________________________________________________________________

## Strategy 5 --> The Kain Facade Pattern

### 5.1 Never Leak Raw C Vocabulary

Always wrap the foreign vocabulary in Kain vocabulary:

```kain
include ../native/ffmpeg_bridge.h as ff
include ../native/editor_presenter.h as ui

// ── Kain facade: one bridge feels like one Kain capability ──
pub fn ffmpeg_open_media(path: String) -> Int:
    return ff_open_media(path)

pub fn ffmpeg_close_media(media: Int) -> Int:
    return ff_close_media(media)

pub fn ffmpeg_decoder_create(media: Int, stream: Int) -> Int:
    return ff_decoder_create(media, stream)

pub fn ffmpeg_decoder_destroy(decoder: Int) -> Int:
    return ff_decoder_destroy(decoder)

pub fn ffmpeg_decode_next(decoder: Int) -> Int:
    return ff_decoder_decode_next(decoder)

pub fn ffmpeg_last_error() -> String:
    return ff_last_error()

pub fn presenter_open(title: String, width: Int, height: Int) -> Int:
    return ui_open(title, width, height)

pub fn presenter_close(handle: Int) -> Int:
    return ui_close(handle)

pub fn presenter_present_rgba_words(
    handle: Int, words_address: Int,
    width: Int, height: Int, word_count: Int,
    playhead_ms: Int, frame_checksum: Int, clip_count: Int
) -> Int:
    return ui_present_rgba_words(handle, words_address, width, height,
                                 word_count, playhead_ms, frame_checksum, clip_count)
```

**Rules:**

- The Kain facade name should be different from the raw C symbol. Don't name your wrapper `ff_open_media` if the C function is `ff_open_media` === doing so can make the alias thunk call the Kain wrapper instead of the extern and recurse until the native lane crashes.
- One native package = one Kain capability module.
- Only the facade module imports the C header. The rest of the app imports the Kain module.

### 5.2 Strings: C → Kain Are Automatic

When a C function returns `const char*`, the generated alias extern carries `@c_string_return`. The LLVM lane materializes the raw pointer into owned Kain string storage:

```kain
include sqlite3.h as sql

fn probe_sqlite_version() -> String:
    // Returns a real Kain String, not a borrowed C pointer
    return sql_libversion()
```

You can concatenate, slice, and pass it around without worrying about C lifetime:

```kain
let version = sql_libversion()
let full_msg = "SQLite version: " + version   // safe -- owned copy
```

______________________________________________________________________

## Strategy 6 〰 Multi-Library Bridges (FFmpeg + Presenter Pattern)

### 6.1 When You Need More Than One C World

Real blades often wrap more than one native library. The FFmpeg editor gauntlet (`blades/c/ffmpeg`) is the repo truth for complex multi-library C ABI:

```toml
# KAIN.toml ~ explicit metadata for complex cases
[c_ffi]
include_paths = [
  "native",
  "${env:KAIN_PLATFORM_FFMPEG_SDK}/include",
]

[[c_ffi.libraries]]
name = "ffmpeg_bridge"
header = "native/ffmpeg_bridge.h"
sources = ["native/ffmpeg_bridge.c"]
include_paths = [
  "native",
  "${env:KAIN_PLATFORM_FFMPEG_SDK}/include",
]
static_libs = [
  "${env:KAIN_PLATFORM_FFMPEG_SDK}/lib/avformat.lib",
  "${env:KAIN_PLATFORM_FFMPEG_SDK}/lib/avcodec.lib",
  "${env:KAIN_PLATFORM_FFMPEG_SDK}/lib/avutil.lib",
  "${env:KAIN_PLATFORM_FFMPEG_SDK}/lib/swscale.lib",
]

[[c_ffi.libraries]]
name = "editor_presenter"
header = "native/editor_presenter.h"
sources = ["native/editor_presenter.c"]
include_paths = ["native"]
link_libs = ["user32", "gdi32"]
```

```kain
// ffmpeg_abi.kn <--> includes both bridge headers AND system FFmpeg headers
include "../native/ffmpeg_bridge.h" as ff
include "../native/editor_presenter.h" as ui
include <libavutil/avutil.h> as avu
include <libavcodec/avcodec.h> as avc
include <libavformat/avformat.h> as avf
include <libswscale/swscale.h> as sws

// Cross-validate bridge versions against direct FFmpeg system headers
pub struct FfmpegVersionReport:
    bridge_score: Int
    angle_score: Int
    avutil: Int
    avcodec: Int
    avformat: Int
    swscale: Int

pub fn ffmpeg_bridge_version_report() -> FfmpegVersionReport:
    return FfmpegVersionReport {
        bridge_score: (ff_avutil_version() + ff_avcodec_version()
                       + ff_avformat_version() + ff_swscale_version()
                      ) % 1000000007,
        angle_score: (avu_version() + avc_version()
                      + avf_version() + sws_version()
                     ) % 1000000007,
        avutil: ff_avutil_version(),
        avcodec: ff_avcodec_version(),
        avformat: ff_avformat_version(),
        swscale: ff_swscale_version()
    }
```

### 6.2 Key Takeaways

- **Multiple `[[c_ffi.libraries]]` entries** are normal when the blade owns more than one native surface
- **`tier = "inline"`** compiles the bridge-owned C sources into the native link (default when `sources` is specified)
- **`include_paths`** can reference env vars for vendor SDK roots
- **`static_libs`** links vendor import libraries; **`link_libs`** links OS system libraries
- **The Kain facade** stays thin: maps `ff_*` and `ui_*` aliases into semantically named Kain functions
- **Handles are plain integers** at the boundary; the C bridge owns the lookup tables and validation

### 6.3 The Natural Include + System Header Hybrid

You can mix local `include` with system `<angle-bracket>` includes freely:

```kain
include "../native/ffmpeg_bridge.h" as ff      // local C wrapper
include <libavutil/avutil.h> as avu              // system FFmpeg header
include <libavcodec/avcodec.h> as avc            // system FFmpeg header
```

Both get the same alias treatment ~> just with different resolution paths.

______________________________________________________________________

## Strategy 7 --- Inline Assembly

### 7.1 The Asm + Win32 Pattern

Kain's `asm()` keyword gives you raw assembly with full register constraint mapping. From `blades/kain/asm/src/main.kn`:

```kain
include <windows.h> as win

const MASK_32: Int = 4294967295

// rep stosd 〰 fast memory fill via string instruction
fn asm_fill(ptr: Int, cnt: Int, val: Int) with Unsafe:
    asm("rep stosd", ptr, cnt, val & MASK_32,
        constraints = "{rdi},{rcx},{rax}",
        clobbers = "rdi,rcx,rax", memory = true, intel = true)

// rdtsc --> CPU timestamp counter
fn asm_rdtsc(sc: ptr<Int>) with Unsafe:
    asm("rdtsc\nmov [rdi], eax", ptr_to_int(sc),
        constraints = "{rdi}", clobbers = "rax,rdx,rdi",
        memory = true, intel = true)

// mov dword ptr => pixel write at (addr + offset)
fn asm_pix(addr: Int, off: Int, col: Int) with Unsafe:
    asm("mov dword ptr [rdi+rcx], eax",
        addr, off, col & MASK_32,
        constraints = "{rdi},{rcx},{rax}",
        clobbers = "rdi,rcx,rax", memory = true, intel = true)
```

Then call into the asm from a function that also uses Win32 interop:

```kain
fn main() -> Int with Unsafe:
    // rep stosd: fill 4 dwords with 0xDEADBEEF
    let buf: ptr<Int> = alloc_zeroed(8, "Int")
    asm_fill(ptr_to_int(buf), 4, 3735928559)

    // rdtsc: read CPU cycle counter
    let sc: ptr<Int> = alloc_zeroed(2, "Int")
    asm_rdtsc(sc)

    // mov dword ptr: store 0xFF00FF00
    asm_pix(ptr_to_int(buf), 0, 4278255360)

    decay buf
    decay sc

    // Win32: paint proof on desktop
    let desktop = win_GetDesktopWindow()
    let hdc = win_GetDC(desktop)
    if hdc != 0:
        let _ = win_TextOutA(hdc, 50, 50, "ASM PROOFS PASSED", 20)
        let _ = win_ReleaseDC(desktop, hdc)
    return 0
```

### 7.2 Asm Parameter Reference

```kain
asm("<instruction>",
    arg1, arg2, ...,              // Kain values mapped to constraints
    constraints = "{reg1},{reg2}", // register binding (Intel syntax: {rdi})
    clobbers = "rax,rdx,rdi",     // registers asm may clobber
    memory = true,                // asm reads/writes memory
    intel = true)                 // use Intel syntax (default)
```

- **constraints**: `{rdi}`, `{rcx}`, `{rax}` bind Kain args to specific registers
- **clobbers**: comma-separated register names the asm modifies
- **memory = true**: compiler presumes any memory may be read/written
- **intel = true**: Intel syntax (default). Set to false for AT&T syntax
- **return value**: `asm()` can optionally return a value: `let ticks = asm("rdtsc", ...)`

______________________________________________________________________

## Strategy 8 => Manual Memory Management

### 8.1 Allocating Memory for C Functions

Many C APIs expect you to pass a pointer to a struct or buffer. Use `alloc_zeroed` to create Kain-owned memory, `mem_store` to fill it, and `ptr_to_int` to pass the address:

```kain
fn fill_rect(hdc: Int, x: Int, y: Int, w: Int, h: Int, color: Int) -> Int with Unsafe:
    let brush = win_CreateSolidBrush(color)
    let rect: ptr<Int> = alloc_zeroed(4, "Int")         // 4 Ints = RECT struct
    mem_store(ptr_offset(rect, 0, "Int"), x, "Int")       // left
    mem_store(ptr_offset(rect, 1, "Int"), y, "Int")       // top
    mem_store(ptr_offset(rect, 2, "Int"), x + w, "Int")   // right
    mem_store(ptr_offset(rect, 3, "Int"), y + h, "Int")   // bottom
    let result = win_FillRect(hdc, rect, brush)           // pass ptr to C
    decay rect                                              // free
    let _ = win_DeleteObject(brush)
    return result
```

### 8.2 collapse / observe / decay

For C interop memory, use Kain's ownership expressions:

```kain
fn fill_native_words(words: ptr<Int>, count: Int) -> Int with Unsafe:
    if count <= 0:
        return 0
    collapse words:
        let status = native_fill_words(words, count)  // C mutates memory
        if status != 0:
            return status
        return observe words:
            mem_load(words, "Int")                     // read after C wrote

// Standalone lifecycle:
let buf: ptr<Int> = alloc_zeroed(8, "Int")
// ... use buf ...
decay buf                                              // release
```

### 8.3 Pointer Casting

```kain
// Float-to-int reinterpret via pointer cast (fast inverse sqrt pattern)
fn fast_invsqrt(x: Float) -> Float with Unsafe:
    let half_x = x * 0.5
    let buf: ptr<Int> = alloc_zeroed(2, "Int")
    let float_ptr = int_to_ptr(ptr_to_int(buf), "ptr<Float>")
    mem_store(float_ptr, x, "Float")        // write as Float
    let i = mem_load(buf, "Int")            // read as Int (reinterpret)
    i = 0x5f3759df - (i >> 1)              // magic constant
    mem_store(buf, i, "Int")
    let y = mem_load(float_ptr, "Float")    // read as Float
    decay buf
    y = y * (1.5 - (half_x * y * y))       // Newton iteration
    return y
```

This is the famous Quake fast inverse square root, fully in Kain with raw memory reinterpretation.

### 8.4 Multi-Buffer Pattern for Complex C Calls

From the component_minimal Win32 app * * * the message pump uses a typed pointer buffer:

```kain
// MSG struct (6 Ints)
let msg_buf = alloc_zeroed(6, "Int")

while my_IsWindow(hwnd) != 0:
    var has_msg = my_PeekMessageA(msg_buf, 0, 0, 0, 1)
    while has_msg != 0:
        let msg_type = mem_load(ptr_offset(msg_buf, 1, "Int"), "Int") & 0xFFFFFFFF
        if msg_type == 0x0201:
            // WM_LBUTTONDOWN ‒ read lParam (x,y packed)
            let lparam = mem_load(ptr_offset(msg_buf, 4, "Int"), "Int")
            let cmx = lparam & 0xFFFF
            let cmy = (lparam >> 16) & 0xFFFF
        let _ = my_TranslateMessage(msg_buf)
        let _ = my_DispatchMessageA(msg_buf)
        has_msg = my_PeekMessageA(msg_buf, 0, 0, 0, 1)

decay msg_buf
```

______________________________________________________________________

## Strategy 9 ~~ @extern @link_name for Single Functions

### 9.1 When You Don't Need the Full Header

If you only need one or two functions from a C library, you can declare them directly:

```kain
@extern @link_name("CreateSolidBrush")
fn create_solid_brush(color: Int) -> Int

@extern @link_name("FillRect")
fn fill_rect(hdc: Int, rect: ptr<Int>, brush: Int) -> Int

@extern @link_name("PeekMessageA")
fn peek_message(lpMsg: ptr<Int>, hWnd: Int,
                wMsgFilterMin: Int, wMsgFilterMax: Int,
                wRemoveMsg: Int) -> Int
```

This is useful for:

- Prototyping before switching to `include <header.h>`
- Single-function dependencies where the full header is overkill
- Functions with complex signatures that need custom parameters

### 9.2 When to Use @extern vs include

| Use Case | Tool |
|----------|------|
| One or two functions | `@extern @link_name` |
| A whole API surface | `include <header.h> as alias` |
| A local C wrapper | `include native/wrapper.h as alias` |
| System headers with auto-link | `include <math.h> as cmath` |
| Complex bridge with static libs | `include` + `[c_ffi]` metadata |

______________________________________________________________________

## Strategy 10 |-> Handle Lifecycle

### 10.1 Open → Use → Close

Native libraries love handles. Make the lifecycle explicit:

```kain
include sqlite3.h as sql

fn probe_database(path: String) -> Int:
    // Open
    let db = sql_open(path, 0)
    if db <= 0:
        return -1

    // Use
    let version = sql_libversion_number()
    let _ = sql_exec(db, "SELECT 1")

    // Close
    let status = sql_close(db)
    return if status == 0: version else: -2
```

### 10.2 The C Bridge Owns State, Kain Owns Policy

From the FFmpeg gauntlet === handles are integers at the boundary, the C bridge owns lookup tables:

```kain
pub fn ffmpeg_open_media(path: String) -> Int:
    return ff_open_media(path)

pub fn ffmpeg_close_media(media: Int) -> Int:
    return ff_close_media(media)

pub fn ffmpeg_live_media_count() -> Int:
    return ff_live_media_count()

pub fn ffmpeg_last_error() -> String:
    return ff_last_error()
```

**Rules:**

- Prefer `open/create` plus `close/destroy` pairs over hidden global state
- Name handles as handles in Kain (`media`, `decoder`, `db`)
- A stale handle should be rejected by the native side
- Wrap the status check in Kain: `if handle <= 0: return -1`

______________________________________________________________________

## Strategy 11 – The Full Win32 GDI App Pattern

The most comprehensive C interop example in the repo is `blades/kain/component_minimal/src/app.kn` \_\_\_ a 250+ line Win32 GDI application with:

### 11.1 Window Creation

```kain
include <windows.h> as win

fn create_window() -> Int with Unsafe:
    let hinst = win_GetModuleHandleA("user32.dll")
    let style = 0x00CF0000 | 0x10000000
    let hwnd = win_CreateWindowExA(
        0, "STATIC", TITLE, style,
        60, 40, APP_W, APP_H,
        0, 0, hinst, 0
    )
    if hwnd != 0:
        let user32 = win_GetModuleHandleA("user32.dll")
        let def_wnd_proc = win_GetProcAddress(user32, "DefWindowProcA")
        let _ = win_SetWindowLongPtrA(hwnd, -4, def_wnd_proc)
    let _ = win_ShowWindow(hwnd, 5)
    let _ = win_UpdateWindow(hwnd)
    return hwnd
```

### 11.2 The Message Pump

```kain
let msg_buf = alloc_zeroed(6, "Int")

while my_IsWindow(hwnd) != 0:
    var has_msg = my_PeekMessageA(msg_buf, 0, 0, 0, 1)
    while has_msg != 0:
        let msg_type = mem_load(ptr_offset(msg_buf, 1, "Int"), "Int") & 0xFFFFFFFF
        if msg_type == 0x0201:     // WM_LBUTTONDOWN
            // ... handle click ...
        if msg_type == 0x0200:     // WM_MOUSEMOVE
            // ... read mouse position ...
        if msg_type == 15:         // WM_PAINT
            let _ = win_ValidateRect(hwnd, 0)
        let _ = my_TranslateMessage(msg_buf)
        let _ = my_DispatchMessageA(msg_buf)
        has_msg = my_PeekMessageA(msg_buf, 0, 0, 0, 1)

    // Render frame
    if frame % 30 == 0:
        fps = 30
    let hdc = win_GetDC(hwnd)
    let _ = render_frame(hdc, frame, clicks, errors, debug_mode, phase, sphere_anim, mx, my, fps)
    let _ = win_ReleaseDC(hwnd, hdc)

decay msg_buf
```

### 11.3 GDI Drawing (All Through include \<windows.h> as win)

```kain
fn fill_rect(hdc: Int, x: Int, y: Int, w: Int, h: Int, color: Int) -> Int with Unsafe:
    let brush = win_CreateSolidBrush(color)
    let rect: ptr<Int> = alloc_zeroed(4, "Int")
    mem_store(ptr_offset(rect, 0, "Int"), x, "Int")
    mem_store(ptr_offset(rect, 1, "Int"), y, "Int")
    mem_store(ptr_offset(rect, 2, "Int"), x + w, "Int")
    mem_store(ptr_offset(rect, 3, "Int"), y + h, "Int")
    let result = win_FillRect(hdc, rect, brush)
    decay rect
    let _ = win_DeleteObject(brush)
    return result

fn draw_text(hdc: Int, x: Int, y: Int, text: String, color: Int) -> Int with Unsafe:
    let _ = win_SetTextColor(hdc, color)
    let _ = win_SetBkMode(hdc, 1)
    return win_TextOutA(hdc, x, y, text, len(text))
```

This proves that a full Win32 application (window, GDI painting, message loop, user input, sphere rendering) can be written in Kain with **zero `use c::`, zero manual externs, zero manifest ceremony** \<--> just `include <windows.h> as win`.

______________________________________________________________________

## Strategy 12 ⁓ Platform Packages

### 12.1 Importing a Platform SDK

For SDK-based interop (Vulkan, FFmpeg, custom vendor SDKs), use the `kain import platform` command:

```bash
# Import Vulkan SDK
kain import platform vulkan --sdk "C:/VulkanSDK/1.3.296.0"

# Import full Windows SDK (auto-detect from system)
kain import platform windows

# Import FFmpeg SDK
kain import platform ffmpeg --sdk "F:/Scoop/apps/ffmpeg-shared"
```

These commands register the SDK paths in the project's build configuration, set the environment variables the system header registry looks for (`VULKAN_SDK`, `KAIN_PLATFORM_FFMPEG_SDK`), and make `include <vulkan/vulkan.h> as vk` resolve automatically.

### 12.2 std::platform for Dynamic Loading

For ad-hoc dynamic library loading without a full header include:

```kain
use std::platform

fn library_symbol_probe(path: String, symbol: String) -> Int:
    let handle = platform_library_open(path)
    if handle <= 0:
        return platform_library_last_status()

    let address = platform_library_resolve(handle, symbol)
    let close_status = platform_library_close(handle)
    if close_status != 0:
        return close_status
    if address == 0:
        return platform_library_last_status()
    return 0
```

______________________________________________________________________

## Strategy 13 ... Converge + C: Native as a Fast Lane

### 13.1 Multi-Lane Dispatch with a C Fallback

Use `converge` to pair a spec lane with a C fast lane, then verify correctness:

```kain
fn scalar_mix(value: Int) -> Int:
    return ((value * 31) + 7) % 1000000007

include native/mix_fast.h as mix

converge mix(value: Int) -> Int:
    spec reference:
        return scalar_mix(value)
    fast c_lane when target("llvm"):
        return mix_fast(value)
    verify random(8)
```

### 13.2 Orchestrate with C Stages

The `orchestrate` system supports C as a first-class stage kind. From the god pipeline pattern:

```kain
orchestrate preflight(seed: Int, signal: Int) -> Int:
    stage cpu_seed: cpu
        mix(seed + signal)
        when capability("cpu.scalar")
        residency host transfer none policy static

    stage c_shadow: c
        host_shadow(cpu_seed + epoch)
        after cpu_seed
        residency host fallback cpu_seed        // C FFI with Kain fallback
        policy telemetry_prefer_cpu

    stage converge_lane: converge
        mix(py_shadow + cpu_seed)
        deps [cpu_seed, c_shadow]
        residency shared transfer shared_view
        policy telemetry_balance_latency

    // ... GPU, law, patch stages ...
    if legal == false:
        return c_shadow
    return final_lane
```

______________________________________________________________________

## Anti-Patterns

### ❌ Calling C in a Tight Loop Instead of Caching Results

```kain
// BAD: 120,000 C calls
while i < 120000:
    let pi = cmath_acos(-1.0) as Int    // same result every time
    i = i + 1

// GOOD: 1 C call
let pi = cmath_acos(-1.0) as Int
while i < 120000:
    // use pi
    i = i + 1
```

### ❌ Manually Declaring Hundreds of `@extern fn` Instead of Using `include <header.h>`

```kain
// BAD: 20 lines of manual declarations
@extern @link_name("CreateSolidBrush") fn fn1(c: Int) -> Int
@extern @link_name("CreateWindowExA") fn fn2(...) -> Int
@extern @link_name("TextOutA") fn fn3(...) -> Int
// ... hundreds more ...

// GOOD: one line
include <windows.h> as win
// 6,294 functions available as win_*
```

### ❌ Forgetting `with Unsafe` on Functions That Touch Raw Memory

```kain
// BAD: compiles but may misbehave
fn fill_rect(hdc: Int, x: Int, y: Int, w: Int, h: Int, color: Int) -> Int:

// GOOD: tells the compiler memory operations are happening
fn fill_rect(hdc: Int, x: Int, y: Int, w: Int, h: Int, color: Int) -> Int with Unsafe:
```

### ❌ Not Decaying Allocated Memory

```kain
// BAD: memory leak
let buf: ptr<Int> = alloc_zeroed(4, "Int")
// use buf...
// return without decay → leak

// GOOD: explicit lifetime
let buf: ptr<Int> = alloc_zeroed(4, "Int")
// use buf...
decay buf
```

### ❌ Using C for Operations Kain Can Do Natively

```kain
// BAD: C bridge for integer arithmetic
include native/math.h as m
let result = m_add(3, 4)

// GOOD: Kain arithmetic – no crossing
let result = 3 + 4
```

### ❌ Returning Raw Pointers Without Length, Ownership, and Destroy Policy

If a C function returns a pointer, you need to know:

- How many elements? (length)
- Who owns the memory? (Kain or C?)
- How is it freed? (which destroy function?)

**Wrap it:** Create a Kain struct that captures the handle, length, and cleanup logic.

### ❌ Naming a Kain Wrapper the Same as the Raw C Symbol

```kain
// BAD: ff_open_media could resolve to itself and recurse
include native/ffmpeg_bridge.h as ff
pub fn ff_open_media(path: String) -> Int:   // WRONG -- same name as ff_open_media extern
    return ff_open_media(path)                // may call itself!

// GOOD: distinct Kain name
pub fn ffmpeg_open_media(path: String) -> Int:
    return ff_open_media(path)
```

______________________________________________________________________

## Quick Reference Card

### Which Pattern for Which Situation

| You Need To... | Use This |
|----------------|----------|
| Read `sqrt(2)` in a hot loop | `entangle` + read from mirror |
| Call 3-4 math functions per iteration | `include <math.h> as cmath` |
| Show a Win32 message box | `include <windows.h> as win` |
| Write a full Win32 GDI app | `include <windows.h> as win` + manual memory |
| Call Vulkan APIs | `include <vulkan/vulkan.h> as vk` |
| Embed SQLite | `include sqlite3.h as sql` + companion `sqlite3.c` |
| Wrap a local C function | `include native/foo.h as f` + `native/foo.c` |
| Link a multi-library SDK | `[c_ffi]` metadata with `static_libs` + `link_libs` |
| Write raw CPU instructions | `asm("rep stosd", ...)` with constraints/clobbers |
| Allocate a struct for C | `alloc_zeroed(N, "Type")` + `mem_store` |
| Read a C string return | Automatic via `@c_string_return` on externs |
| Open/close a handle | Kain facade with `open`/`close` pairs |
| Load a DLL dynamically | `std::platform` |
| One function, not a whole header | `@extern @link_name("FuncName")` |
| Fuse C + Kain in a pipeline | `orchestrate` with `stage c` + `fallback` |
| Multi-lane dispatch with C | `converge` with `fast c_lane` |

### Include Form Quick Reference

| Syntax | Resolution | Manifest Needed? | Example |
|--------|------------|------------------|---------|
| `include <header.h> as alias` | System header registry | No | `include <math.h> as cmath` |
| `include "header.h" as alias` | Local file (angle brackets) | No (but `.c` sibling) | `include sqlite3.h as sql` |
| `include path/to/header.h as alias` | Relative to source | No (but `.c` sibling) | `include native/math.h as m` |
| `[c_ffi]` metadata | Explicit bridge config | Yes (`KAIN.toml`) | FFmpeg, complex SDKs |
| `@extern @link_name` | Manual symbol binding | No | Single functions |

### Memory vs Speed Tradeoffs

| Feature | Memory Cost | Speed Benefit |
|---------|------------|---------------|
| Entangled cache in world | World struct fields | ~infinite vs per-call C crossing |
| Shared buffer (pointer pass) | No copy | Zero-copy for large data |
| alloc_zeroed + decay | Heap allocation | Full control over lifetime |
| System header auto-link | Static link overhead | Zero linker config |
| C amalgamation (SQLite) | ~9 MB compiled C | Full library available |
| `@extern @link_name` | No extra codegen | Minimal thunk weight |

### The One Command to Prove It Works

```bash
# Check a local C include project
kain build blades/c/minimal/src/main.kn --target llvm

# Check a system header include
kain build blades/c/platform/windows/src/main.kn --target llvm

# Check a large C library include
kain build blades/c/sqlite/main.kn --target llvm

# Run the system header benchmark
kain run benchmark/cases_v2/system_headers.kn --target llvm

# Run the Vulkan loader benchmark
kain run benchmark/cases_v2/vulkan_loader.kn --target llvm
```

### Debugging C Interop --- DWARF Metadata

Every one of these commands supports `--debug` (or `-g`) to emit **full DWARF debug metadata** into the native binary:

```bash
# Build with source-level debug info
kain build blades/c/sqlite/main.kn --target llvm --debug

# Run with DWARF >> LLDB backtraces show Kain source lines
kain run blades/c/platform/windows/src/main.kn --target llvm --debug
```

This is **insanely useful** for C interop debugging because:

- **`!DILocation` and `!DISubprogram`** annotations map every LLVM instruction back to the original `.kn` source line === you see *your* Kain code in the debugger, not generated IR.
- **Segfault or ABI mismatch in a C call?** LLDB backtraces show exactly which `include`-generated wrapper was called and at which Kain source line.
- **Null pointer from `win_GetDC` returned 0?** Set a breakpoint at the exact line in your `.kn` file -- the debugger lands there.
- **Works with `kain build` and `kain run`** => only meaningful for `--target llvm` (native binaries), not for interpreted or transpiled targets.

```bash
# Example: debug a Win32 MessageBox crash
kain build blades/c/platform/windows/src/main.kn --target llvm --debug
lldb ./blades/c/platform/windows/src/main.exe
(lldb) breakpoint set --file main.kn --line 7
(lldb) run
# Now you're stopped at `let result = win_MessageBoxA(...)` ___ inspect registers, step in.
```

______________________________________________________________________

## Appendix: Extraction Priority Chain

When Kain encounters `include <header.h> as alias` or `include "path/to/header.h" as alias`, the extraction follows this priority:

```
1. libclang (real Clang C frontend) * * * full preprocessing, macro expansion,
   type resolution, struct/enum/typedef collection. Handles ANY C header
   with clean types. This is the fast path.

2. lang_c AST (pure-Rust C parser) --> fallback when libclang is unavailable.
   Good for simple headers without complex SAL annotations.

3. Regex fallback ~ last resort. Matches function prototypes via regex.
   Use only for debugging or minimal headers.
```

**Key insight**: For complex vendor headers (`<windows.h>`, `<vulkan/vulkan.h>`, `<cuda.h>`), libclang is always the first priority. You do NOT need to write smaller wrapper headers. The shim headers in the registry exist as fallbacks for systems without libclang ⁓ not as the primary path.

### What Libclang Automatically Handles

- **Preprocessing**: `#include`, `#define`, `#ifdef`, `#pragma once`, `__declspec`
- **Macro expansion**: `#define NULL ((void*)0)` → resolved to typed value
- **Type resolution**: `typedef struct { ... } HWND__* HWND` → fully parsed
- **Struct/enum/typedef**: Full aggregate type collection
- **Callback detection**: Function pointer parameters are recognized
- **Kain keyword sanitization**: C parameter names that collide with Kain keywords (`sampler`, `input`, `type`, `world`) are auto-suffixed with `_`

### What Libclang Does NOT Handle Yet

- **SAL annotations** on Windows CRT headers (`_Check_return_opt_`, `_ACRTIMP`, `__cdecl`) --- these mostly affect `<stdio.h>` and family. Vulkan, POSIX, and third-party headers are clean.
- **Tagged-int leakage** for `void*`/handle params >> 0 encodes as 1 in some codegen paths. Use `uintptr_t` in shim headers as a workaround if needed.
- **Struct initialization** * * * no `&` address-of. Use `alloc_zeroed` + `mem_store`.

______________________________________________________________________

*This guide was written by analyzing: 7 blades at `blades/c/`, 5 blades at `blades/kain/`, 2 benchmark files at `benchmark/cases_v2/` (system_headers, vulkan_loader), `smoketest/src/interop/sqlite_rally.kn`, the complete C-FFI crate (`crates/c-ffi/`), the system header registry (`crates/c-ffi/system_headers.toml`), and the C ABI skill documentation. Every example is backed by real, checkable Kain code from the repository.*
