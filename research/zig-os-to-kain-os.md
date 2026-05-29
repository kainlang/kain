# Research: Zig `std.os` → Kain `std::os` Architecture Study

**Date**: 2026-05-29
**Reference**: `X:\reference\zigos` (windows.zig, linux.zig, wasi.zig, emscripten.zig, plan9.zig, uefi.zig + subsystem subdirs)
**Status**: Phase 1 shipped (core facade + os.path). Phase 2 pending (runtime ABI backfill).

---

## 1. The Gap That Existed

Kain's stdlib had 62 modules with 2,857 public symbols before this work. Every individual OS primitive existed:

| Capability | Existing module | Example function |
|---|---|---|
| Filesystem | `stdlib/fs.kn` | `fs_read_dir_paths`, `fs_metadata_text`, `fs_exists` |
| Process | `stdlib/process.kn` | `process_current_working_directory`, `process_environment`, `process_output_text` |
| Machine/CPU | `stdlib/machine.kn` | `cpu_logical_count`, `vm_page_size` |
| Path manipulation | `stdlib/path.kn` | `path_join`, `path_is_absolute`, `path_parent` |
| Platform detection | `stdlib/target.kn` | `target_current()` → `Target { arch, os, env, is_64bit }` |
| Platform ABI | `stdlib/platform.kn` | `native_platform_current_name`, library open/close |
| Time | `stdlib/time.kn` | `sleep_millis`, `now_millis` |

**The problem**: No unified facade. A developer wanting to do basic OS scripting had to import 6-7 modules and know exactly which function lived where. The "import os" Python ergonomic flow did not exist.

Every LLM that inspected the stdlib saw the individual modules working and concluded "covered." Nobody asked the meta-question: what's the single entry point?

---

## 2. Zig's `std.os` Architecture (Reference Analysis)

### 2.1 Layer Model

```
┌──────────────────────────────────────────────────┐
│  std.fs    std.process    std.net    std.thread  │  ← high-level stdlib
├──────────────────────────────────────────────────┤
│              std.posix (cross-platform)          │  ← POSIX-like wrappers
├──────────────────────────────────────────────────┤
│  windows.zig  │  linux.zig  │  wasi.zig  │  ...  │  ← platform-specific raw bindings
├──────────────────────────────────────────────────┤
│  kernel32.zig │  syscalls/  │  wasi ABI  │       │  ← subsystem DLLs / syscall tables
│  ntdll.zig    │  per-arch   │            │       │
│  ws2_32.zig   │             │            │       │
└──────────────────────────────────────────────────┘
```

### 2.2 Key Design Decisions in Zig's OS Layer

1. **Platform files are thin wrappers** — `windows.zig` just re-exports subsystem modules (`kernel32`, `ntdll`, `ws2_32`, `crypt32`, `nls`) and adds Windows-specific types (`HANDLE`, `DWORD`, `UNICODE_STRING`, `OBJECT_ATTRIBUTES`, etc.). All logic is "convert OS error codes → Zig errors."

2. **Linux layer is architecture-parameterized** — `linux.zig` selects per-arch syscall tables at comptime:
   ```zig
   const arch_bits = switch (native_arch) {
       .x86_64 => @import("linux/x86_64.zig"),
       .aarch64 => @import("linux/aarch64.zig"),
       // ... 20+ architectures
   };
   ```
   Each arch module exports `syscall0` through `syscall7`, `clone`, `restore_rt`, plus arch-specific constants (`SC`, `ARCH`, `VDSO`).

3. **WASI is a flat ABI import** — `wasi.zig` directly imports the `wasi_snapshot_preview1` module with typed externs like:
   ```zig
   pub extern "wasi_snapshot_preview1" fn fd_read(fd: fd_t, iovs: [*]const iovec_t, ...) errno_t;
   ```

4. **Subsystem modules are raw DLL imports** — `windows/kernel32.zig`, `windows/ntdll.zig`, `windows/ws2_32.zig` are pure `extern "kernel32" fn ...` declarations with Zig-typed wrappers.

5. **`std.posix` is the cross-platform layer** — sits on top of the platform files and provides POSIX-like names (`open`, `read`, `write`, `close`, `socket`, `fork`, `exec`, `waitpid`) that work across Windows/Linux/macOS/WASI.

### 2.3 What Kain Already Has vs. What's Missing

| Zig layer | Kain equivalent | Status |
|---|---|---|
| Subsystem DLLs (kernel32, ntdll) | `@extern` ABI functions in fs.kn, process.kn, etc. | ✅ Exists |
| Platform types (HANDLE, DWORD) | Wrapped as Int/String/ptr in Kain | ✅ Exists |
| Error code → typed error | `FsError`, `FsOpResult`, `FsTextResult` in fs.kn | ✅ Exists |
| Cross-platform posix layer | ❌ Not yet — this is what `std::os` provides | 🆕 Shipped |
| `os.path` equivalent | ❌ Was split across path.kn + fs.kn | 🆕 Shipped |
| Per-architecture syscall tables | ❌ Not needed (Kain targets LLVM, not raw syscalls) | N/A |
| Higher stdlib consuming os | fs.kn, process.kn exist but don't consume os yet | 🔮 Future |

---

## 3. What Was Built

### 3.1 `stdlib/os.kn` — Main OS Module (~450 lines)

Python-like `import os` equivalent. Pattern: `use std::os` → `os_getcwd()`, `os_listdir(".")`, etc.

**10 sections** covering:

| Section | Functions | Lines |
|---|---|---|
| 1. Platform Constants & Detection | `os_name()`, `os_platform_name()`, `os_arch_name()`, `os_is_windows/linux/macos/wasi()`, `os_is_64bit()`, `os_uname()` | ~80 |
| 2. Environment Variables | `os_getenv()`, `os_getenv_default()` | ~20 |
| 3. Process Identity | `os_getpid()` (+ stubbed getppid, getlogin, getuid, getgid) | ~15 |
| 4. Working Directory | `os_getcwd()` (+ stubbed chdir) | ~10 |
| 5. Filesystem Operations | `os_listdir()`, `os_scandir()`, `os_mkdir/makedirs()`, `os_remove/rmdir/removedirs()`, `os_rename/replace()`, `os_stat()`, `os_exists/isfile/isdir()`, `os_tmpfile/tmpdir()`, `os_read/write/append/atomic_write_text()` | ~130 |
| 6. Process Execution | `os_system()`, `os_popen_read()`, `os_popen_status()` | ~25 |
| 7. System Information | `os_cpu_count/core_count/package_count()`, `os_getpagesize()`, `os_get_terminal_size()` (+ stubbed urandom) | ~30 |
| 8. Time Utilities | `os_sleep_millis()`, `os_now_millis()` | ~10 |
| 9. File Descriptor Ops | `os_open/close/read/write/seek/tell/flush()` | ~40 |
| 10. Error Utilities | `os_last_error()` → OsError struct | ~10 |

### 3.2 `stdlib/os_path.kn` — Path Utilities (~500 lines)

Python `os.path` + pathlib equivalent. Pattern: `use std::os_path` → `os_path_join("a", "b")`.

**7 sections** covering:

| Section | Functions |
|---|---|
| 1. Constants | `os_path_sep()`, `os_path_altsep()`, `os_path_extsep()`, `os_path_pathsep()`, `os_path_devnull()` |
| 2. Assembly & Decomposition | `os_path_join()`, `split()`, `dirname()`, `basename()`, `splitext()`, `splitdrive()`, `commonpath()` |
| 3. Normalization | `os_path_normpath()`, `abspath()`, `relpath()`, `realpath()` |
| 4. Predicates | `os_path_isabs()`, `exists()`, `isfile()`, `isdir()`, `ismount()`, `islink()`, `samefile()` |
| 5. Metadata | `os_path_getsize()`, `getmtime()`, `getctime()`, `getatime()` |
| 6. Expansion | `os_path_expanduser()`, `expandvars()` |
| 7. Composition | `os_path_with_suffix()`, `with_name()`, `with_stem()` |

### 3.3 `smoketest/src/os_basics.kn` — Proving Surface

7 tests covering platform, process identity, filesystem listing, environment variables, system info, path operations, and process execution. **All pass** with `kain run --target llvm`.

---

## 4. What Remains (Phase 2)

### 4.1 Runtime ABI Backfill (C-side implementation needed)

These are all declared as `@extern` in comments within `os.kn`. Each one needs a C function in the native runtime, then the Kain wrapper can be uncommented:

| Function | Priority | Complexity | Notes |
|---|---|---|---|
| `abi_os_chdir(path)` | HIGH | Trivial | `chdir()` / `SetCurrentDirectoryW()` |
| `abi_os_setenv(key, value)` | HIGH | Trivial | `setenv()` / `SetEnvironmentVariableW()` |
| `abi_os_unsetenv(key)` | HIGH | Trivial | `unsetenv()` / `SetEnvironmentVariableW(key, NULL)` |
| `abi_os_getppid()` | MED | Trivial | `getppid()` / `GetParentProcessId()` via toolhelp |
| `abi_os_getlogin()` | MED | Trivial | `getlogin_r()` / `GetUserNameW()` |
| `abi_os_getuid()` | LOW | Trivial | `getuid()` / Windows: return 0 |
| `abi_os_getgid()` | LOW | Trivial | `getgid()` / Windows: return 0 |
| `abi_os_symlink(src, dst)` | MED | Moderate | `symlink()` / `CreateSymbolicLinkW()` (needs SeCreateSymbolicLinkPrivilege on Windows) |
| `abi_os_readlink(path)` | MED | Moderate | `readlink()` / `DeviceIoControl(FSCTL_GET_REPARSE_POINT)` |
| `abi_os_urandom(byte_count)` | HIGH | Trivial | `getentropy()` / `BCryptGenRandom()` / `/dev/urandom` |
| `abi_os_environ_keys()` | LOW | Moderate | Iterate `environ` / `GetEnvironmentStringsW()` |
| `abi_os_environ_len()` | LOW | Trivial | Count env vars |
| `abi_os_terminal_size()` | LOW | Trivial | `ioctl(TIOCGWINSZ)` / `GetConsoleScreenBufferInfo()` |
| `abi_os_hostname()` | LOW | Trivial | `gethostname()` / `GetComputerNameW()` |
| `abi_os_mkdir_single(path)` | MED | Trivial | `mkdir()` / `CreateDirectoryW()` (non-recursive version) |
| `abi_os_rmdir_single(path)` | MED | Trivial | `rmdir()` / `RemoveDirectoryW()` (non-recursive version) |

**Total**: 15 ABI functions, ~200 lines of C total. Most are single syscall wrappers.

### 4.2 `fs_metadata` Struct Parse Crash (Runtime Bug)

`fs_metadata(path)` returns `FsMetadata` struct but crashes (0xc0000005) in the LLVM lane. `fs_metadata_text(path)` works fine — it returns the raw `key=value\n` text.

**Workaround applied**: `os_stat()` and `os_scandir()` manually parse the metadata text using `_meta_field()` / `_meta_int()` helpers. This works but is slower than the struct path.

**Root cause**: Likely in `fs_parse_metadata_text()` or `fs_try_metadata()` in `stdlib/fs.kn` (~line 281). Needs investigation.

### 4.3 Interpreter Lane Support

`kain run` (interpreter) fails with `Undefined: abi_platform_current_kind`. The `--target llvm` lane works. The interpreter likely doesn't load the full ABI surface for all stdlib modules. This is a toolchain issue, not a code issue.

### 4.4 `std::os::path` Import Resolution

`use std::os::path` doesn't resolve — the compiler requires `use std::os_path` instead. The `graphics_shared.kn` → `std::graphics::shared` pattern exists, so the machinery is there, but the STDLIB_MAP may need regeneration after new files are added.

### 4.5 Missing Functions (Python compat gaps)

Functions from Python's `os` module not yet implemented:

| Python | Status |
|---|---|
| `os.walk()` | ❌ Can compose from `os_listdir` + recursion |
| `os.scandir()` | ✅ Done (`os_scandir`) |
| `os.fwalk()` | ❌ Needs fd-based walk |
| `os.sendfile()` | ❌ Needs `sendfile()` syscall |
| `os.truncate()` | ❌ Needs `ftruncate()` ABI |
| `os.utime()` | ❌ Needs `utimensat()` ABI |
| `os.chmod()` | ❌ Needs `chmod()` ABI |
| `os.chown()` | ❌ Needs `chown()` ABI |
| `os.link()` | ❌ Needs `link()` ABI |
| `os.mkfifo()` | ❌ POSIX only, unlikely priority |
| `os.sched_getaffinity()` | ❌ Needs platform-specific ABI |
| `os.getpriority()` / `os.setpriority()` | ❌ Needs platform-specific ABI |
| `os.times()` | ❌ Needs `times()` ABI |
| `os.cpu_count()` | ✅ Done |
| `os.getloadavg()` | ❌ Needs platform-specific ABI |
| `os.get_exec_path()` | ❌ Can build from `process_current_executable_path()` |
| `os.get_handle_inheritable()` | ❌ Windows-specific |
| `os.set_handle_inheritable()` | ❌ Windows-specific |

### 4.6 Stdlib Refactoring Opportunity

Currently `fs.kn`, `process.kn`, etc. operate independently. The long-term Zig-like architecture would have them consume `std::os`:

```
Current:
  fs.kn → raw @extern ABIs directly
  process.kn → raw @extern ABIs directly

Future (Zig pattern):
  std::os → raw @extern ABIs (single source of truth)
  fs.kn → wraps std::os calls
  process.kn → wraps std::os calls
```

This deduplicates ABI declarations and ensures one consistent error-handling path.

---

## 5. Kain Language Learnings (From This Work)

These are things future agents should know when authoring Kain:

| Gotcha | Rule |
|---|---|
| `match` is a reserved keyword | Use `matched` as variable name |
| `default` is a reserved keyword | Use `default_value` |
| `return` inside match arms | LLVM codegen rejects it; assign to a var then return after the match |
| Array indexing | Use `arr[i]` not `get(arr, i)` |
| Empty array init | Use `var x: Array<Foo> = []` not `var x: Array<Foo>` |
| Boolean negation | Use `x == false` not `not x` |
| Module-level `var` | Not accessible inside functions; no caching pattern |
| `len` as parameter name | Shadows the `len()` builtin; use `byte_count` instead |
| Duplicate `@extern` across modules | Linker error; import the owning module and use its wrapper |
| `elif` IS valid | Multi-branch conditionals use `if/elif/else` |
| `or` / `and` | Boolean operators work as expected |

---

## 6. Files Changed

```
Created:
  stdlib/os.kn                     — Main OS module (the "import os")
  stdlib/os_path.kn                — Path utilities (the "os.path")
  smoketest/src/os_basics.kn       — 7-test proving surface

Updated:
  MEMORY.md                        — Architecture notes and learnings
```

---

## 7. Validation

```powershell
# Both modules type-check clean
kain check stdlib/os.kn          # ✅ passed
kain check stdlib/os_path.kn     # ✅ passed

# Smoketest passes (LLVM native lane only; interpreter has ABI gaps)
kain run smoketest/src/os_basics.kn --target llvm   # ✅ ALL PASSED

# Output:
#   os_basics smoketest running...
#     platform ok: nt / windows / x86_64
#     process ok: pid=10116
#     fs ok: 17 entries in cwd
#     env ok
#     system ok: cpu=16 pagesize=4096
#     path ok
#     popen ok
#   os_basics smoketest: ALL PASSED
```

---

## 8. Next Steps (Recommended Order)

1. **Implement top-5 runtime ABIs** (chdir, setenv, unsetenv, urandom, getppid) — these unlock the stubbed functions and cover 80% of Python `os` usage
2. **Fix `fs_metadata` struct crash** — removes the workaround in os_stat/os_scandir, speeds up directory scanning
3. **Regenerate STDLIB_MAP** — so `use std::os::path` resolves natively
4. **Add `os.walk()`** — compose from existing os_listdir + recursion, high Python-compat value
5. **Refactor fs.kn/process.kn to consume std::os** — deduplicates ABI declarations, follows Zig architecture
