# Kain Native Runtime Test & Verification Pipeline

This pipeline spans **three verification layers** that complement each other:

| Layer | What it finds | Run time |
|-------|--------------|----------|
| **Sanitizer tests** (`make test`, `make stress`) | Memory errors, races, UB | Seconds |
| **Fuzz tests** (`make fuzz`) | Edge cases humans don't think of | Minutes |
| **CBMC verification** (`run_pipeline.py cbmc`) | **Formal proof** of pointer safety + invariants | Seconds–minutes |

The CBMC layer is the most powerful: it explores **every path** through your C code
within a bounded unwind depth and proves that no pointer dereference, arithmetic
overflow, or assertion violation is possible.

---

## Quick Start

```bash
# Sanitized compile-and-run of smoke + property tests
make test

# Formal verification of hand-written CBMC harnesses
python test/scripts/run_pipeline.py cbmc --list-harnesses
python test/scripts/run_pipeline.py cbmc --harness check_arena
python test/scripts/run_pipeline.py cbmc --harness check_actor

# Full pipeline (extract function catalog + run all auto-generated harnesses)
python test/scripts/run_pipeline.py extract
python test/scripts/run_pipeline.py cbmc
```

## Directory Layout

```
runtime/native/
├── src/core/            # Production runtime C (arena.c, actor.c, memory.c, …)
├── include/             # Headers
├── test/
│   ├── smoke/           # "Does it compile and do basic things?"
│   ├── property/        # "Does invariant X always hold?"
│   ├── fuzz/            # libFuzzer harnesses — coverage-guided chaos
│   ├── stress/          # Multi-threaded tsan hammer
│   ├── cbmc/            # ★ Hand-written CBMC verification harnesses
│   │   ├── check_arena.c   # 833 assertions, all pass
│   │   └── check_actor.c   # 5676 assertions, all pass
│   ├── scripts/         # Python pipeline
│   │   ├── run_pipeline.py   # Main entry point (extract, cbmc, esbmc, cross)
│   │   ├── _common.py        # Shared paths/helpers
│   │   ├── _find_cbmc_wsl.py # WSL CBMC detection
│   │   └── data/cbmc/harnesses/  # Auto-generated harnesses (fallback)
│   └── README.md
├── Makefile
└── .gitignore
```

---

# Layer 1: Makefile Tests (Sanitizer-Driven)

Built around `gcc -fsanitize=address,undefined` and GNU Make.

| Target | Sanitizer | What It Finds |
|--------|-----------|---------------|
| `make test` | ASan+UBSan | Use-after-free, buffer overflow, null deref, integer overflow, alignment |
| `make fuzz` | libFuzzer+ASan | Edge cases no human thinks of |
| `make stress` | TSan | Data races, deadlocks |

```bash
make test           # all smoke + property tests in one shot
make test TEST=smoke_arena  # run a single test
make fuzz           # build libFuzzer harnesses (run manually)
make stress         # tsan-compiled multi-threaded tests
make lib            # static library (_build/libkain.a)
make shared         # shared library (_build/libkain.so/.dll)
```

These are fast and great for regression, but they only test the paths your
test inputs happen to hit. They **cannot prove the absence of bugs**.

---

# Layer 2: Fuzz Tests

Fuzz harnesses live in `test/fuzz/` and use libFuzzer for coverage-guided
input generation.

```bash
make fuzz
_build/test/fuzz/fuzz_memory -max_len=4096 -runs=100000
```

Like sanitizer tests, fuzzing is path-coverage-based — it finds bugs fast
but cannot prove their absence.

---

# Layer 3: CBMC Formal Verification (★ The Power Tool)

CBMC (**C** **B**ounded **M**odel **C**hecker) converts your C code into a
SAT/SMT formula and asks a solver: *"is there any input, within bounded
loop unwinding, that violates any assertion or performs undefined behavior?"*

If CBMC says **VERIFICATION SUCCESSFUL**, your code is **provably safe**
for all paths within the unwind bound — no null dereference, no buffer
overflow, no use-after-free, no integer overflow, and every
`__CPROVER_assert` you wrote holds for every input.

## How to Run

```bash
# List available hand-written harnesses
python test/scripts/run_pipeline.py cbmc --list-harnesses

# Run a specific hand-written harness
python test/scripts/run_pipeline.py cbmc --harness check_arena
python test/scripts/run_pipeline.py cbmc --harness check_actor

# Run with deeper loop unwind
python test/scripts/run_pipeline.py cbmc --harness check_arena --unwind 10

# Run auto-generated harnesses (fallback — naive nondet pointers)
python test/scripts/run_pipeline.py cbmc
python test/scripts/run_pipeline.py cbmc --module arena
```

### What the output means

```
============================================================
  CBMC: check_arena.c [hand-written]
============================================================
    Harness: check_arena.c
    Source:   arena.c
  [OK] All 833 assertions verified

============================================================
  CBMC: check_actor.c [hand-written]
============================================================
    Harness: check_actor.c
    Source:   actor.c
  [OK] All 5676 assertions verified
```

- `[OK]` = VERIFICATION SUCCESSFUL — **every explored path is safe**
- `[FAIL] N violations` = some paths violate assertions or contain UB
  (false positives usually mean the harness constraints are too loose)

## Architecture

The pipeline is modular — **you never edit Python code to add a new harness**.

```
                    ┌─────────────────────────┐
                    │  run_pipeline.py cbmc     │
                    │  --harness <name>         │
                    └──────┬──────────────────┘
                           │
              ┌────────────┴────────────┐
              ▼                         ▼
     ┌─────────────────┐     ┌──────────────────────┐
     │ test/cbmc/       │     │ Auto-generated from  │
     │ check_<mod>.c    │     │ catalog (fallback)   │
     │ (hand-written)   │     │ data/cbmc/harnesses/ │
     └────────┬─────────┘     └──────────────────────┘
              │
     ┌────────┴────────┐
     ▼                 ▼
  WSL CBMC        GCC preprocess
  (Ubuntu 6.6.0)  + native CBMC
                   (6.9.0 fallback)
```

Hand-written harnesses take priority. If `test/cbmc/check_<name>.c` exists,
the pipeline uses it. Otherwise it falls back to auto-generated harnesses
(which call functions with completely uninitialized pointers — useful for
coverage but produces many false positives).

### WSL-first strategy

On Windows, CBMC runs through **WSL Ubuntu** by default (Linux GCC headers
are clean; MinGW/MSVC headers contain constructs that choke CBMC's parser).
If WSL is unavailable, the pipeline falls back to GCC preprocessing +
native CBMC.

---

# 🔑 How to Write a CBMC Harness

This is the core skill. A CBMC harness is a plain C file in
`test/cbmc/check_<module>.c` that:

1. **Creates valid data structures** backed by static buffers (for pointer
   provenance)
2. **Fills them with nondeterministic bytes** via `__CPROVER_havoc_object`
3. **Constrains** them with `__CPROVER_assume` to valid ranges
4. **Calls the real C functions** (including `static` ones — the combined
   source+harness is one translation unit)
5. **Asserts postconditions** with `__CPROVER_assert`

## The Golden Rule: Pointer Provenance

**`__CPROVER_havoc_object` makes every byte nondeterministic**, including
pointer fields. A havoc'd pointer is NOT a valid pointer — it could be
NULL, dangling, or point to deallocated memory. `__CPROVER_assume(p != NULL)`
only constrains non-nullness; it does **not** give CBMC pointer validity.

**Fix:** point all internal pointers at real static buffers.

```c
// ❌ BAD: havoc'd pointer with assume
static Widget* create_widget(void) {
    static Widget w;
    __CPROVER_havoc_object(&w);
    // w.data is a random pointer — every dereference is "invalid"
    return &w;
}

// ✅ GOOD: pointed at real static memory
static unsigned char backing_store[4096];

static Widget* create_valid_widget(void) {
    static Widget w;
    __CPROVER_havoc_object(&w);      // nondet CONTENTS
    __CPROVER_havoc_object(backing_store);  // nondet buffer contents

    // Valid provenance — pointer to a real allocated object
    w.data = &backing_store[0];
    w.size = sizeof(backing_store);
    return &w;
}
```

Now CBMC can verify every access through `w.data` against the bounds of
`backing_store[4096]`.

## Step-by-Step Recipe

### Step 1: Understand the module

Read the header and source. Identify:
- What are the key data structures?
- What are the public functions?
- What are the internal invariants?
- Which functions can be tested in isolation?

### Step 2: Create the harness file

```
touch test/cbmc/check_<module>.c
```

### Step 3: Include headers

```c
#include "module.h"     // primary header from include/
// #include "base.h"    // if the module depends on base types
// #include "other.h"   // as needed
```

### Step 4: Declare static backing buffers

```c
/* CBMC knows these are real allocated objects with valid provenance */
static KainModuleState g_state;
static unsigned char g_payload[4096];
```

### Step 5: Write a factory function

```c
static KainModuleState* create_valid_state(void) {
    KainModuleState* s = &g_state;
    __CPROVER_havoc_object(s);       // nondet all bytes
    __CPROVER_havoc_object(g_payload); // nondet backing contents

    /* ── Pointer provenance ── */
    s->buffer = &g_payload[0];
    s->buffer_size = sizeof(g_payload);

    /* ── Constrain fields to valid ranges ── */
    __CPROVER_assume(s->count <= s->buffer_size);
    __CPROVER_assume(s->offset >= 0 && s->offset <= s->buffer_size);
    __CPROVER_assume(s->mode >= MODE_MIN && s->mode <= MODE_MAX);

    return s;
}
```

### Step 6: Write test functions

Each test function should test **one property** clearly:

```c
void check_module_init(void) {
    KainModuleState* s = create_valid_state();
    unsigned char* pre_buffer = s->buffer;

    int rc = kain_module_init(s, g_payload, sizeof(g_payload));

    __CPROVER_assert(rc == 0 || rc == -1, "init returns 0 or -1");
    if (rc == 0) {
        __CPROVER_assert(s->buffer == pre_buffer, "buffer preserved");
        __CPROVER_assert(s->offset == 0, "offset reset to 0");
        __CPROVER_assert(s->count == 0, "count reset to 0");
    }
}

void check_module_op_null(void) {
    int rc = kain_module_op(NULL, NULL);
    __CPROVER_assert(rc == -1, "NULL args return -1");
}
```

### Step 7: Call everything from main

```c
int main(void) {
    check_module_init();
    check_module_op_null();
    // ... more tests
    return 0;
}
```

### Step 8: Run it

```bash
python test/scripts/run_pipeline.py cbmc --harness check_module
```

## Calling Static Functions

The combined file (source + harness) is a **single translation unit**. C's
`static` keyword means "internal linkage" — visible anywhere in the same
translation unit. So your harness CAN call `static` functions from the
source file.

Forward-declare them in your harness if needed:

```c
/* Forward declaration of static function from source.c */
static int module_internal_op(struct ModuleState* s, size_t n);

void test_internal_op(void) {
    // ...create valid state...
    int rc = module_internal_op(state, 42);
    __CPROVER_assert(rc >= 0, "internal op succeeds");
}
```

This is enormously powerful — you can test internal functions that aren't
part of the public API.

## Working with External Functions (OS Primitives)

CBMC treats undefined external functions (like `pthread_mutex_lock`,
`malloc`, `memcpy`) as **nondeterministic** — it considers any possible
return value and any possible memory side effect through pointer arguments.

This means:
- **`malloc`** — CBMC models heap allocation. It considers both success
  (returns valid memory) and failure (returns NULL). Your handling of OOM
  is automatically verified.
- **`memcpy`** — CBMC models the copy. Bounds checking on both src and
  dest is automatic.
- **`pthread_mutex_lock`** — CBMC treats it as "clobber the struct with
  nondet bytes." Since the code doesn't inspect the mutex internals, this
  is safe.
- **`free`** — CBMC models deallocation. Use-after-free is automatically
  detected.

You do NOT need to mock or stub these. Just call the real function and
let CBMC handle the modeling.

## Common Pitfalls

### 1. Havoc'd pointers without provenance

```c
// WRONG — every pointer deref will flag
KainWidget w;
__CPROVER_havoc_object(&w);
// w.data is an invalid pointer
widget_process(&w);

// RIGHT — point to real buffer
static unsigned char buf[256];
KainWidget w;
__CPROVER_havoc_object(&w);
w.data = &buf[0];
w.size = 256;
widget_process(&w);
```

### 2. Forgetting to call `__CPROVER_havoc_object`

```c
// WRONG — initializes all bytes to 0, which may miss bugs
KainWidget w = {0};

// RIGHT — nondet contents, constrained to valid state
KainWidget w;
__CPROVER_havoc_object(&w);
__CPROVER_assume(w.size > 0 && w.size <= 256);
```

### 3. Unwinding assertion failures

```
[FAIL] 1 violations
  - line 123 unwinding assertion loop 0
```

This means a loop iterates more times than your `--unwind N` bound allows.
Solutions:
- Increase unwind: `--unwind 10` or `--unwind 20`
- If the loop is bounded by a small constant in your code, use
  `--no-unwinding-assertions` (suppresses the unwinding check but
  preserves all real assertion checks)

The pipeline defaults to `--no-unwinding-assertions` for hand-written
harnesses since module-internal loops are typically bounded.

### 4. UTF-8 encoding in harness files

If your harness contains non-ASCII characters (like box-drawing Unicode
in comments), the pipeline reads both source and harness as UTF-8. This
is handled automatically, but if you see `UnicodeDecodeError` with
`cp1252`, the fix is already in the pipeline.

### 5. Nondet external return values

CBMC may choose a path where `malloc` returns NULL, or
`pthread_mutex_lock` returns nonzero. If your code doesn't check these,
CBMC still explores both branches. **This is a feature** — it verifies
your code handles failure correctly.

## Checking Your Results

Look for these in the CBMC output:

```
VERIFICATION SUCCESSFUL     ← all assertions hold, all paths safe
VERIFICATION FAILED          ← counterexample found (or false positive)
```

With a hand-written harness using proper static-buffer provenance,
"VERIFICATION FAILED" with pointer-safety violations almost always
means your `__CPROVER_assume` constraints are too loose. Tighten them
until only real bugs remain.

---

# Pipeline Reference

## `run_pipeline.py`

```
python test/scripts/run_pipeline.py <command> [options]
```

### Commands

| Command | Description |
|---------|-------------|
| `extract` | Scan `src/core/*.c`, extract function signatures + types → `data/catalog.json` |
| `cbmc`   | Run CBMC verification on catalog modules or specific harness |
| `cross`  | Cross-reference CBMC results vs. Z3 proofs in `src/core/z3/proofs/` |

### Options for `cbmc`

| Flag | Effect |
|------|--------|
| `--module <name>` | Auto-generated harness for one module (e.g. `--module arena`) |
| `--harness <name>` | Run hand-written harness from `test/cbmc/check_<name>.c` |
| `--harness check_<name>` | Also accepted (canonical form) |
| `--list-harnesses` | List available hand-written harnesses |
| `--unwind N` | Loop unwind bound (default: 3 for auto, 5 for hand-written) |

### Examples

```bash
# Extract function catalog
python test/scripts/run_pipeline.py extract

# Auto-generated harnesses for all modules
python test/scripts/run_pipeline.py cbmc

# Auto-generated for one module
python test/scripts/run_pipeline.py cbmc --module memory

# Hand-written harness
python test/scripts/run_pipeline.py cbmc --harness check_arena

# Hand-written with deeper unwind
python test/scripts/run_pipeline.py cbmc --harness check_actor --unwind 10

# List available
python test/scripts/run_pipeline.py cbmc --list-harnesses
```

---

# Examples

## `check_arena.c` — Proven Invariants

- `kain_arena_init` sets correct `start`, `end`, `low`, `high`, `depth`
- `kain_arena_reset` restores `low==start`, `high==end`, `depth==0`
- `kain_arena_available` never exceeds arena size
- `kain_arena_alloc_lo` returns aligned results in `[start,end)`, advances `low`
- `kain_frame_set_marker` stores correct offsets, depth unchanged
- `kain_frame_release_to_last_marker` restores `low`/`high` from marker
- `alloc_lo` + `alloc_hi` regions never overlap
- Allocation fits entirely within the buffer (no OOB writes)

*833 CBMC properties verified (pointer safety + assertions).*

## `check_actor.c` — Proven Invariants

- **Enqueue**: correct linked-list insertion in empty/non-empty mailboxes,
  capacity enforcement (-3), closed mailbox (-2), NULL safety (-1),
  OOM handling via malloc failure paths
- **Try_receive**: correct dequeue from linked list, FIFO ordering,
  empty detection, head/tail cleanup when queue drains
- **Accessors**: NULL-safe, correct field return, unbounded capacity
- **Spawn config init**: correct defaults, NULL-safety

*5676 CBMC properties verified (pointer safety + assertions).*

---

# LLM/Agent Workflow

If you are an LLM or coding agent with zero context, start here:

```
1.  read runtime/native/test/README.md              # this file
2.  ls runtime/native/test/cbmc/                     # see existing harnesses
3.  read runtime/native/test/cbmc/check_arena.c      # study the pattern
4.  cat runtime/native/src/core/<module>.c | head -100  # read the module
5.  cat runtime/native/include/<module>.h | head -100   # read the header
6.  grep -n "^static\|^int\|^void\|^size_t" src/core/<module>.c  # find all functions
7.  python test/scripts/run_pipeline.py cbmc --harness check_<module>  # run it
```

To add a new harness:
```
touch test/cbmc/check_<module>.c
# Write harness following the recipe above
python test/scripts/run_pipeline.py cbmc --harness check_<module>
```

The pipeline auto-discovers new harnesses — no Python code changes needed.

---

# Architecture Rationale

**Why static buffers for provenance instead of heap allocation?**

`malloc` is nondeterministic in CBMC — it can return NULL or valid memory.
For testing pointer safety of operations, a known-valid static buffer is
more predictable and avoids CBMC exploring OOM paths for the backing store
itself (which would mask real bugs in the module under test). The module's
OWN use of `malloc` (e.g., in the enqueue function) is still verified —
CBMC considers both success and failure of those allocations independently.

**Why WSL-first instead of native Windows CBMC?**

MinGW and MSVC headers on Windows contain `__attribute__((...))`,
`__declspec(...)`, and other GCC/MSVC extensions. When GCC-preprocessed,
these survive into the `.i` file and choke CBMC's C parser. WSL Ubuntu
uses standard Linux headers with no such extensions, so CBMC processes
them cleanly. The pipeline falls back to native CBMC with GCC preprocessing
if WSL is unavailable.

**Why hand-written harnesses over auto-generated?**

Auto-generated harnesses (from the function catalog) call each function
with completely uninitialized pointers — essentially "what happens if every
argument is garbage?" This produces thousands of violations because garbage
pointers are not valid. Hand-written harnesses with `__CPROVER_assume`
constraints filter out the garbage inputs and test only the valid domain.
They find real bugs in the actual logic, not noise from nonsensical inputs.

---

# Future Work

- `check_memory.c` — ownership/collapse/observe/decay tape model
- `check_scheduler.c` — bounded ring buffer enqueue/dequeue
- `esbmc` command for multi-threaded actor/scheduler verification
- Cross-reference CBMC results vs existing Z3 proofs
- CI integration: `cbmc --harness` in pre-merge gate
