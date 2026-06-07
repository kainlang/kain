# Kain Runtime Test Pipeline

## Quick Start

```bash
# From runtime/native/
make test          # ASan+UBSan on all smoke + property tests
make fuzz          # build libFuzzer harnesses
make stress        # TSan on concurrent tests
make lib           # static library
make shared        # shared library (.so/.dylib/.dll)
```

## Directory Layout

```
test/
├── smoke/         # "Does it compile and do basic things?" — one per module
├── property/      # "Does invariant X always hold?" — random-input validation
├── fuzz/          # libFuzzer harnesses — coverage-guided chaos
├── stress/        # Multi-threaded hammer — finds races under TSan
├── scripts/       # Python helpers for test generation
├── .archive/      # Old conformance test scripts (pre-Makefile era)
└── README.md
```

## Adding a New Test

### Smoke test (30 seconds)

1. Copy `_TEMPLATE.c` from the test type you want
2. Include your module header
3. Write 3-5 basic operations
4. `make test` — runs automatically

### Fuzz harness (2 minutes)

1. Read the module header — note all public functions
2. Copy `fuzz/_TEMPLATE.c` → `fuzz/fuzz_<module>.c`
3. In `LLVMFuzzerTestOneInput`, call each function with data-derived args
4. `make fuzz` — build only
5. `./_build/test/fuzz/fuzz_<module> -max_len=4096 -runs=100000`

### LLM-Agent Workflow

For an agent to add tests for a module:
```
1. read runtime/native/include/<module>.h      # get function signatures
2. read test/fuzz/_TEMPLATE.c                  # get fuzz harness pattern
3. write test/fuzz/fuzz_<module>.c             # fill in the body
4. make fuzz                                   # build
5. ./_build/test/fuzz/fuzz_<module> -runs=...  # run
```

## Sanitizer Matrix

| Target | Sanitizer | What It Finds |
|--------|-----------|---------------|
| `make test` | ASan+UBSan | Use-after-free, buffer overflow, null deref, integer overflow, alignment |
| `make stress` | TSan | Data races, deadlocks |
| `make fuzz` | libFuzzer+ASan | Edge cases no human thinks of |

## Platform Support

| Platform | Static Lib | Shared Lib | ASan | UBSan | TSan | libFuzzer |
|----------|-----------|------------|------|-------|------|-----------|
| Linux | ✅ .a | ✅ .so | ✅ | ✅ | ✅ | ✅ |
| macOS | ✅ .a | ✅ .dylib | ✅ | ✅ | ✅ | ✅ |
| Windows | ✅ .lib | ✅ .dll | ✅ | ✅ | ❌ | ❌ |

Windows note: use WSL for fuzzing. MSVC/clang-cl doesn't support libFuzzer directly.
