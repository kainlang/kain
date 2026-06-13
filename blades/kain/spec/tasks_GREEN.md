# Stream GREEN: Ouroboros Pipeline

**Stream ID:** GREEN
**Role:** Wire the selfhost bootstrap pipeline: fix llvm_ffi.kn for headerless machines, complete selfhost handlers (205-208), enable multi-file compilation, fix KAIN.toml for ouroboros, make workspace discovery real.
**Effort:** 1-2 weeks
**Depends On:** none (can start parallel with RED)
**Requirements Covered:** FR-combine, FR-selfhost, FR-verify, FR-llvm-ffi, FR-multifile, FR-workspace
**Design Reference:** orchestrator.kn (handlers 205-208), compiler.kn (DriverSession), KAIN.toml ([source_order] + [selfhost])

---

## Context

The ouroboros pipeline is the end-game: kainc compiles its own source code and produces byte-identical LLVM IR. Phase 1 (source concatenation) already works — 23 files combined into 681KB `kainc_bootstrap.kn`. Phase 2 is blocked by two things:

1. **llvm_ffi.kn** — Uses `include <llvm-c/Core.h> as llvm` which requires LLVM dev headers not available on all machines. The combined source fails `kain build` because this include can't be resolved.
2. **Stub codegen** — The typechecker and codegen are at stub level, so even if the source compiles, the resulting binary can't do real work. This is RED + BLUE's job.

GREEN's job is to remove the Phase 2 blockers that are PURELY pipeline/infrastructure problems. RED + BLUE handle the semantic depth.

---

## Files You Own

### Files to Modify

| File | Region/Function | Change Description |
|------|-----------------|--------------------|
| `X:/blades/kain/src/llvm_ffi.kn` | Entire file | Make `include <llvm-c/...>` directives conditional — provide stub type definitions when LLVM-C headers aren't available |
| `X:/blades/kain/src/orchestrator.kn` | `handler_selfhost_phase1` (line 472-528) | Wire real source concatenation from KAIN.toml source_order |
| `X:/blades/kain/src/orchestrator.kn` | `handler_selfhost_phase2` (line 530-782) | Wire real self-compilation: compile combined source → diff LLVM IR |
| `X:/blades/kain/src/orchestrator.kn` | `handler_build_link` (line 383-436) | Wire clang/lld invocation to link .ll → .exe |
| `X:/blades/kain/src/orchestrator.kn` | `handler_build_package` (line 438-470) | Wire amalgamate + bundle |
| `X:/blades/kain/src/compiler.kn` | `discover_workspace` | Make real — directory ascent for KAIN.toml/build.kn/.git |
| `X:/blades/kain/src/compiler.kn` | `compile_workspace` (new) | Multi-file compilation: resolve `use` imports, read source files, aggregate |
| `X:/blades/kain/src/KAIN.toml` | Entire file | Add full [selfhost] section with paths, runtime manifest, outputs |

### Files to Create

| File | Purpose |
|------|---------|
| `X:/blades/kain/src/llvm_stub_types.kn` | Stub type definitions for LLVM-C opaque types when headers unavailable |

### Files You Must NOT Touch

| File | Reason |
|------|--------|
| `X:/blades/kain/src/types.kn` | Owned by Stream RED |
| `X:/blades/kain/src/codegen.kn` | Owned by Stream BLUE |
| `X:/blades/kain/src/monomorphize.kn` | Owned by Stream RED |

---

## Implementation Tasks

### GREEN-1: Fix llvm_ffi.kn — Make LLVM-C Includes Conditional

**Effort:** 1 day
**Objective:** Make the combined source compile on machines WITHOUT LLVM-C dev headers.

**Problem:** `llvm_ffi.kn` contains:
```kn
include <llvm-c/Core.h> as llvm
include <llvm-c/Types.h> as llvm_types
```
These fail on machines without LLVM installed. The combined source (`kainc_bootstrap.kn`) fails `kain build` because of these unresolved includes.

**Implementation Steps:**

1. Create `X:/blades/kain/src/llvm_stub_types.kn` with stub type definitions:
   ```
   // llvm_stub_types.kn — Fallback when LLVM-C headers are not available
   // These provide the minimal type surface needed for Path A (textual .ll) codegen.
   // Path B (LLVM-C API / OrcJIT) is disabled when using stubs.
   
   pub type LLVMContextRef = ptr<Byte>
   pub type LLVMModuleRef = ptr<Byte>
   pub type LLVMBuilderRef = ptr<Byte>
   pub type LLVMTypeRef = ptr<Byte>
   pub type LLVMValueRef = ptr<Byte>
   pub type LLVMBasicBlockRef = ptr<Byte>
   
   // Stub functions — return null/zero for all LLVM-C API calls
   pub fn llvm_context_create() -> LLVMContextRef with Unsafe:
       return int_to_ptr(0)
   // ... (all 70+ wrapper functions as null-returning stubs)
   ```

2. Modify `llvm_ffi.kn` to use conditional includes:
   - Add a `HAS_LLVM_HEADERS` constant (controls whether to use real includes or stubs)
   - When HAS_LLVM_HEADERS=0 (default): import from llvm_stub_types.kn instead
   - When HAS_LLVM_HEADERS=1: use the real `include <llvm-c/Core.h> as llvm` directives

3. The existing 70+ wrapper functions in llvm_ffi.kn should work with either source — they wrap the `llvm.` module (real) or stub functions (fallback).

4. Add to KAIN.toml:
   ```toml
   [c_ffi]
   enabled = false  # Set to true when LLVM dev headers are available
   ```

**Acceptance Criteria:**
- [ ] `kain check src/llvm_ffi.kn` passes WITHOUT LLVM headers installed (uses stubs)
- [ ] The combined source (`kainc_bootstrap.kn`) passes `kain check`
- [ ] Path A (textual .ll) codegen still works — it doesn't use LLVM-C API
- [ ] Path B (LLVM-C API) is stubbed and `jit_orc_available()` returns false

---

### GREEN-2: Wire handler_selfhost_phase1 — Source Concatenation

**Effort:** 1 day
**Objective:** Replace the "[HANDLER] Not yet implemented" stub with real source concatenation.

**Current state:** `handler_selfhost_phase1` prints diagnostics and returns 0. It does not actually concatenate files.

**Implementation Steps:**

1. Read KAIN.toml from `project_root/KAIN.toml` using `fs_read_text`
2. Parse the `[source_order]` section to get the ordered list of source files
3. For each file in source_order:
   - Read the file: `fs_read_text(project_root + "/src/" + filename)`
   - Append to combined source
   - Add a separator comment: `// ══ FILE: filename ══`
4. Write the combined source to the output path from `[selfhost.outputs]`:
   ```
   combined_source_path = "src/.selfhost/bootstrap/combined/kain_core_bootstrap.kn"
   ```
5. Verify: read back the combined file, check it has all expected markers (file separator comments)
6. Return 0 on success, non-zero on failure

**Kain code skeleton:**
```kn
pub fn handler_selfhost_phase1(project_root: String) -> Int with IO:
    let config: BuildConfig = load_build_config(project_root + "/KAIN.toml")
    let output_path: String = project_root + "/" + config.selfhost_combined_source_path
    let source_root: String = project_root + "/src/"
    
    let mut combined: String = "// ══ kainc bootstrap — combined source ══\n"
    combined = combined + "// Generated by handler_selfhost_phase1\n"
    combined = combined + "// Source files: " + str(len(config.source_order)) + "\n\n"
    
    var fi: Int = 0
    var total_lines: Int = 0
    while fi < len(config.source_order):
        let file_name: String = config.source_order[fi]
        let file_path: String = source_root + file_name
        let source: String = fs_read_text(file_path)
        combined = combined + "\n// ══ FILE: " + file_name + " ══\n\n"
        combined = combined + source + "\n"
        total_lines = total_lines + count_lines(source)
        fi = fi + 1
    
    fs_write_text(output_path, combined)
    println("[selfhost] Phase 1: Combined " + str(len(config.source_order)) + " files, " + str(total_lines) + " lines → " + output_path)
    return 0
```

**Acceptance Criteria:**
- [ ] Phase 1 produces combined source at correct path
- [ ] Combined source contains all 22 files in source_order
- [ ] Combined source has file separator comments
- [ ] `kain check` on the combined source passes
- [ ] Running `kain selfhost bootstrap --manifest src/KAIN.toml` completes Phase 1

---

### GREEN-3: Wire handler_selfhost_phase2 — Self-Compilation

**Effort:** 2 days
**Objective:** Replace the "[HANDLER] Not yet implemented" stub with real self-compilation + verification.

**Implementation Steps:**

1. **Run the bootstrap compiler** on the combined source:
   - Invoke `kain build combined_source --target llvm --output stage1.ll`
   - This produces `stage1.ll` (compiled by Rust bootstrap)
   - Build `stage1.exe` from stage1.ll via clang (or handler_build_link)

2. **Run kainc (stage1)** on the combined source:
   - Invoke `stage1/kainc.exe build combined_source --target llvm --output stage2.ll`
   - This produces `stage2.ll` (compiled by self-host compiler)

3. **Compare stage1.ll and stage2.ll**:
   - Read both files as text
   - Strip metadata differences (source locations, timestamps, `!` annotations)
   - Compare LLVM IR instructions structurally
   - Report: byte-identical = pass, any differences = mismatch with diff

4. **Report results**:
   ```kn
   pub fn handler_selfhost_phase2(project_root: String, verify: Bool) -> Int with IO:
       if !verify:
           println("[selfhost] Phase 2: Skipped (--verify-ouroboros not set)")
           return 0
       
       // ... compile stage1, compile stage2, compare ...
       
       if identical:
           println("[selfhost] ✅ OUROBOROS VERIFIED — stage1 and stage2 LLVM IR are byte-identical")
           return 0
       else:
           println("[selfhost] ❌ OUROBOROS FAILED — stage1 and stage2 LLVM IR differ")
           println("[selfhost] Differences:")
           // print diff
           return 1
   ```

**Dependency on BLUE:** This handler will produce meaningful results only after BLUE completes (real codegen). GREEN's job is to WIRE the infrastructure; the actual "byte-identical" result comes after RED+BLUE.

**Acceptance Criteria:**
- [ ] Phase 2 handler invokes the bootstrap compiler
- [ ] Phase 2 handler invokes kainc to produce stage2.ll
- [ ] Phase 2 handler compares stage1.ll and stage2.ll
- [ ] Metadata stripping works (removes !dbg, !DI*, source_filename differences)
- [ ] Handler returns 0 when IR matches, non-zero when it differs
- [ ] Running `kain selfhost bootstrap --manifest src/KAIN.toml --verify-ouroboros` works

---

### GREEN-4: Wire handler_build_link — Native Linking

**Effort:** 1 day
**Objective:** Replace the "[HANDLER] Not yet implemented" stub with real clang/lld invocation.

**Implementation Steps:**

1. Use `std::os` or `std::process` to invoke clang:
   - Input: the .ll file produced by codegen
   - Output: native executable (.exe on Windows)
   - Link against: `kain_runtime.lib` (from runtime manifest)
   - Flags: `-O2`, `-target <triple>`, `-o <output.exe>`

2. Handle platform differences:
   - Windows: `clang -target x86_64-pc-windows-msvc input.ll kain_runtime.lib -o output.exe`
   - Linux: `clang -target x86_64-pc-linux-gnu input.ll -lkain_runtime -o output`

3. Report success/failure with link error messages

**Kain code skeleton:**
```kn
pub fn handler_build_link(out_dir: String, target: String) -> Int with IO:
    let triple: String = target_triple_for_platform()
    let ll_file: String = out_dir + "/output.ll"
    let exe_file: String = out_dir + "/kainc.exe"
    let runtime_lib: String = runtime_manifest_path()  // from KAIN.toml [selfhost.runtime]
    
    let clang_cmd: String = "clang"
    let clang_args: Array<String> = [
        "-target", triple,
        "-O2",
        ll_file,
        runtime_lib,
        "-o", exe_file,
    ]
    
    // Invoke clang
    let result: Int = process_run(clang_cmd, clang_args)
    if result != 0:
        println("[link] ERROR: clang exited with code " + str(result))
        return 1
    
    println("[link] Produced " + exe_file)
    return 0
```

**Acceptance Criteria:**
- [ ] handler_build_link invokes clang with correct arguments
- [ ] Linker errors are reported
- [ ] Produced .exe is runnable
- [ ] `kainc.exe --version` prints version and exits 0

---

### GREEN-5: Wire handler_build_package — Amalgamate + Bundle

**Effort:** 0.5 day
**Objective:** Replace the "[HANDLER] Not yet implemented" stub.

**Implementation Steps:**

1. Use the existing `amalgamate` infrastructure if available, or produce a simple concatenation
2. Bundle source + binary + metadata into a distributable package
3. For now, produce a simple zip-like manifest

**Acceptance Criteria:**
- [ ] Handler returns structured output (doesn't just print stub)
- [ ] Package manifest includes source files, binary path, version

---

### GREEN-6: Make Workspace Discovery Real

**Effort:** 1 day
**Objective:** Replace `discover_workspace()` which always returns `""`.

**Implementation Steps:**

1. Implement directory ascent algorithm in compiler.kn:
   ```kn
   pub fn discover_workspace(start_dir: String) -> String:
       let mut dir: String = start_dir
       // Look for KAIN.toml, build.kn, platform.kn, or .git
       while dir != "" and dir != "/" and dir != "C:\\":
           let kain_toml: String = dir + "/KAIN.toml"
           let build_kn: String = dir + "/build.kn"
           let git_dir: String = dir + "/.git"
           
           if fs_exists(kain_toml) or fs_exists(build_kn):
               return dir
           if fs_is_dir(git_dir):
               return dir
           
           dir = dirname(dir)  // parent directory
       return ""
   ```

2. Wire into `compile_file` and `check_file` — if no explicit file path, discover workspace and use build.kn entry point.

3. Add `fs_exists`, `fs_is_dir`, `dirname` helpers (or use std::fs equivalents).

**Acceptance Criteria:**
- [ ] `discover_workspace("X:/blades/kain/src/")` returns `"X:/blades/kain/src/"`
- [ ] `discover_workspace("X:/blades/kain/")` returns `"X:/blades/kain/"`
- [ ] `discover_workspace("/tmp/")` returns `""`
- [ ] Wired into compile_file — `kainc check .` works from project root

---

### GREEN-7: Multi-File Compilation

**Effort:** 2 days
**Objective:** Enable compiling a workspace with multiple source files and `use` imports.

**Implementation Steps:**

1. In `compiler.kn`, add `compile_workspace(workspace_root: String)`:
   - Read `KAIN.toml` → get `source_order` or `source_root`
   - Read all source files in source_order
   - Resolve `use` imports: for `use token`, look up `token.kn` in source_order
   - Concatenate sources (or compile independently and link)
   - For Phase 1: simply concatenate in source_order (same as ouroboros combine)

2. Add `resolve_import(import_path: String, source_order: Array<String>) -> String`:
   - `use token` → looks for `token.kn` in source_order
   - `use src::token` → strip `src::` prefix, look for `token.kn`
   - `use std::fs` → resolve from stdlib (passed through to bootstrap)
   - Return the source file content or empty string if not found

3. Wire into DriverSession: before lex, resolve all imports and combine sources.

**Acceptance Criteria:**
- [ ] `use token` resolves to token.kn in same directory
- [ ] `use std::fs` resolves from stdlib
- [ ] All 23 files can be compiled as a workspace
- [ ] Import resolution errors reported as diagnostics

---

### GREEN-8: Complete KAIN.toml for Ouroboros

**Effort:** 0.5 day
**Objective:** Ensure `X:/blades/kain/src/KAIN.toml` has all required sections for ouroboros bootstrap.

**Implementation Steps:**

1. Verify/update `X:/blades/kain/src/KAIN.toml` with:

```toml
[package]
name = "kainc"
version = "0.1.0"
description = "Kain Self-Host Compiler"
authors = ["Kain Compiler Team"]

[build]
entry = "src/main.kn"
source_root = "src/"
output = "kainc"
target = "llvm"
profile = "debug"

[source_order]
files = [
    "token.kn",
    "error.kn",
    "span.kn",
    "ast.kn",
    "lexer.kn",
    "builtins.kn",
    "runtime.kn",
    "llvm_ffi.kn",
    "jit_metal.kn",
    "jit_x86.kn",
    "jit_orc.kn",
    "jit_cache.kn",
    "jit.kn",
    "parser.kn",
    "types.kn",
    "effects.kn",
    "monomorphize.kn",
    "codegen.kn",
    "orchestrator.kn",
    "compiler.kn",
    "cli.kn",
    "main.kn",
]

[selfhost]
mode = "llvm"

[selfhost.runtime]
manifest_path = "../../runtime/native_core_runtime.toml"
cache_root = ".selfhost/cache/runtime"

[selfhost.outputs]
combined_source_path = ".selfhost/bootstrap/combined/kain_core_bootstrap.kn"
llvm_output_path = ".selfhost/bootstrap/out/kain_core_bootstrap.ll"
native_output_path = ".selfhost/bootstrap/out/kainc.exe"
json_report_path = ".selfhost/reports/bootstrap_report.json"
markdown_report_path = ".selfhost/reports/bootstrap_report.md"
ouroboros_llvm_path = ".selfhost/ouroboros/kain_core_bootstrap.stage2.ll"

[ffi]
shared_libraries = []
link_libs = ["kain_runtime"]

[c_ffi]
enabled = false
```

2. Verify the file passes `kain check` (it's a TOML file, not .kn — but verify syntax).
3. Ensure all paths in `[source_order]` match actual files.

**Acceptance Criteria:**
- [ ] KAIN.toml exists at `X:/blades/kain/src/KAIN.toml`
- [ ] `[source_order]` lists all 22 source files in correct dependency order
- [ ] `[selfhost]` section has all required paths
- [ ] `[selfhost.runtime]` manifest_path points to actual runtime manifest

---

## Stream Conventions

- **Language:** Kain (.kn files)
- **Naming:** snake_case for functions, PascalCase for structs
- **Error handling:** Return non-zero Int on failure, 0 on success. Print diagnostics with println.
- **File I/O:** Use `std::fs` (`fs_read_text`, `fs_write_text`, `fs_exists`)
- **Process I/O:** Use `std::os` or `std::process` for clang/process invocation
- **Comments:** Mark new code with `// ── Stream GREEN ──`

---

## Stream Boundary — What You Do NOT Do

- ❌ Do NOT modify types.kn or codegen.kn
- ❌ Do NOT implement typechecking or codegen logic — RED and BLUE own that
- ❌ Do NOT change parser.kn or lexer.kn
- ❌ Do NOT try to make markscript VM work (Phase 4+)
- ❌ Do NOT modify the 9 IVT handler IDs (200-208)
- ❌ Do NOT remove the forward stubs in compiler.kn (they're needed for standalone check)

---

## Verification (After This Stream)

After completing all tasks, verify:

```bash
# All files check individually
kain check X:\blades\kain\src\

# Specifically verify llvm_ffi.kn passes without LLVM headers
kain check X:\blades\kain\src\llvm_ffi.kn

# Run ouroboros Phase 1
kain selfhost bootstrap --manifest X:\blades\kain\src\KAIN.toml

# Verify combined source exists and checks out
kain check X:\blades\kain\src\.selfhost\bootstrap\combined\kain_core_bootstrap.kn

# Build the compiler (should produce kainc.exe)
cd X:\blades\kain
kain build . --target llvm

# Verify binary runs
.\.kain\out\kainc.exe --version
.\.kain\out\kainc.exe --help
```

**Self-check:**
- [ ] llvm_ffi.kn passes `kain check` individually
- [ ] Combined source from Phase 1 passes `kain check`
- [ ] workspace discovery works from project root
- [ ] Multi-file compilation resolves `use` imports
- [ ] handler_selfhost_phase1 produces combined source
- [ ] handler_selfhost_phase2 compares stage1/stage2 (will fail until BLUE is done — that's expected)
- [ ] handler_build_link invokes clang
- [ ] KAIN.toml has all [source_order] and [selfhost] sections
- [ ] No files modified outside the declared region

---

## Completion Report

When done, report:
- Files created: <list with line counts>
- Files modified: <list with changes summary>
- Handlers made real (from stub): <count>
- Workspace discovery: working for which paths
- llvm_ffi.kn: passes check without LLVM headers?
- Phase 1: combined source produced + passes check?
- Any issues encountered: <list or "none">
- Anything the BLUE stream needs to know: <notes>
