# LLVM Codegen Verification Pipeline

A data-driven extraction, verification, and proof-generation pipeline that
audits every function in the Kain LLVM codegen backend
(`crates/sys-codegen/src/codegen_llvm/mod.rs` — 21,300 lines, 554 functions).

**Goal:** Replace cargo-based Rust tests with Kain-native formal verification.
Extract every piece of logic from the LLVM codegen, write Kain verification
code against each function, define mathematical invariants, and generate
Z3 proof packs for 100% formal coverage.

## Architecture

Three parallel lanes, each owning a domain of the codegen:

```
┌─────────────────────────────────────────────────────────┐
│                  codegen_llvm/mod.rs                     │
│                  (21,300 lines Rust)                     │
└─────────────────────────────────────────────────────────┘
                           │
           ┌───────────────┼───────────────┐
           ▼               ▼               ▼
    ┌────────────┐  ┌────────────┐  ┌────────────┐
    │   ALPHA    │  │   BRAVO    │  │  CHARLIE   │
    │  Types /   │  │ Memory /   │  │ Runtime /  │
    │  Layout /  │  │ Ownership /│  │ Stones /   │
    │  Casts     │  │ Control    │  │ Specials   │
    └────────────┘  └────────────┘  └────────────┘
           │               │               │
           ▼               ▼               ▼
    ┌─────────────────────────────────────────────┐
    │         Z3 Proof Packs + Kain Tests          │
    │            (z3/proofs/generated/)            │
    └─────────────────────────────────────────────┘
```

### Lane Alpha — Types, Layout & Casts
**File:** `src/alpha_types.kn`
**Domain:** ~90 functions
- Type mapping (`map_type_from_ast`, `map_type_from_str`)
- Struct layout & alignment (`struct_storage_type`, `obvious_llvm_type_alignment`)
- Tuple structs (`register_tuple_struct`, `tuple_field_alias_index`)
- Integer/float casts (sext, zext, trunc, fptosi, sitofp)
- Tagged ABI encoding (Option/Result tags, immediate range)
- JSON/Any bridge (tag constants, passthrough tracking)
- Helper alloc layout (`helper_alloc_storage_layout_with_bindings`)

### Lane Bravo — Memory, Ownership & Control Flow
**File:** `src/bravo_memory.kn`
**Domain:** ~170 functions
- Ephemeral storage lowering (SSA vs alloca decisions)
- Heap allocation & RC management (retain/release pairing)
- Ownership transfer (`collapse`/`observe`/`decay`)
- Scope lifetime management (defers, cleanup ordering)
- Control flow (if/else/match/loop PHI nodes)
- Fixed arrays, shattered arrays, literal maps
- I64 literal tracking & loop analysis

### Lane Charlie — Runtime Bridge, Machine Stones & Specials
**File:** `src/charlie_runtime.kn`
**Domain:** ~160 functions
- Runtime symbol bridge (`runtime_symbol_for_stdlib_function`)
- Machine stones: `pulse`, `axiom`, `resonate`, `converge`, `orchestrate`
- World/entangle/patch/law compilation
- String intrinsics (`char_at`, `byte_at`, `find_substring`)
- Kain Map codegen hashing (`kain_map_codegen_mix_u64`)
- Python import bridge, debug metadata, inline ASM, atomics, C FFI

## Project Structure

```
blades/
├── build.kn              # Build authority — compiles all lanes
├── README.md             # This file
└── src/
    ├── data.kn           # Shared data structures (ExtractionTarget, FunctionMeta, Invariant, etc.)
    ├── cli.kn            # CLI argument parser (alpha/bravo/charlie/all subcommands)
    ├── main.kn           # Entry point — wires CLI → lane dispatch
    ├── alpha_types.kn    # Alpha: Types, Layout & Casts verification
    ├── bravo_memory.kn   # Bravo: Memory, Ownership & Control Flow verification
    └── charlie_runtime.kn # Charlie: Runtime, Stones & Specials verification
```

## Quick Start

```bash
# Build the pipeline
kain build

# Run Alpha lane (Types, Layout & Casts)
./codegen-verify.exe alpha ../src/codegen_llvm/mod.rs

# Run all three lanes
./codegen-verify.exe all ../src/codegen_llvm/mod.rs --json

# Show help
./codegen-verify.exe --help
```

## Data Flow

1. **Parse**: Each lane reads `codegen_llvm/mod.rs` and extracts function metadata
   using a lightweight Rust parser (signatures, params, line ranges, doc comments)

2. **Analyze**: Functions are categorized by domain. For each function category,
   mathematical invariants are defined (bounds, alignment, type safety, RC balance,
   ordering constraints)

3. **Prove**: Invariants are lowered to SMT-LIB v2 and wrapped in Z3 proof pack
   YAML files. Generated packs land in `z3/proofs/generated/`

4. **Report**: Each lane produces a `VerificationReport` with invariant counts,
   proof pack counts, errors, and warnings

## Invariant Categories

| Category | Description | Example |
|----------|-------------|---------|
| `bounds` | Index/offset within valid range | `char_at` index < string length |
| `alignment` | Memory access alignment | Alloca alignment is power of 2 |
| `types` | Type mapping completeness | Every AST type → valid LLVM type |
| `ownership` | RC balance, no use-after-free | retain/release count matches |
| `control` | PHI consistency, reachability | All if/else paths converge |
| `abi` | Calling convention, layout | Tag bits don't overlap payload |

## Reference

- **LLVM Codegen Source:** `crates/sys-codegen/src/codegen_llvm/mod.rs`
- **Existing Z3 Proofs:** `crates/sys-codegen/z3/proofs/*.yaml` (37 proofs)
- **Existing Tests:** `crates/sys-codegen/tests/llvm_codegen_test.rs` (3,953 lines)
- **CLI Template Pattern:** `blades/markscript/` (production Kain CLI)
- **Kain Authoring Canon:** `docs/KAIN_BY_EXAMPLE.md`
- **Decision Ladder:** `docs/RULEBOOK.md`

## Decision Ladder

This pipeline operates primarily on **Layer 0** (plain `fn`, `struct`, `let`) because
extraction and analysis are pure transformations. For stateful verification runs,
consider climbing to:
- **Layer 1 (world):** Global verification state across lanes
- **Layer 2 (patch/law):** Journaled mutation of proof pack artifacts
- **Layer 4 (orchestrate):** Staged pipeline: parse → analyze → prove → report
- **Layer 5 (pulse):** Periodic re-verification on codegen changes

## Exit Codes

| Code | Constant | Meaning |
|------|----------|---------|
| 0 | `EXIT_OK` | All invariants pass |
| 1 | `EXIT_HELP` | Help or version shown |
| 2 | `EXIT_ERROR` | Invariant failure or source error |
| 3 | `EXIT_UNKNOWN` | Unknown lane/subcommand |

## Requirements

- **Kain toolchain** (`kain build`, `kain check`)
- **Z3 solver** (for proof pack validation)
- **Native runtime** — auto-linked during build
- **Windows, Linux, or WSL** — targets x86_64

## License

Part of the Kain language project. Internal tooling — use for codegen
verification and proof generation.
