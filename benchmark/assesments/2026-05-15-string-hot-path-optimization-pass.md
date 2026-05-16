# 2026-05-15 LLVM String Hot-Path Optimization Pass

## What landed

- File changed: `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
- New proof cases:
  - `crates/kain-sys-codegen/z3/proofs/control-char-at-byte-fast-path-preserves-empty-string-equality.yaml`
  - `crates/kain-sys-codegen/z3/proofs/memory-borrowed-string-param-call-keeps-refcount-neutral-without-escape.yaml`

The LLVM backend now:

- records string const metadata and direct known lengths
- caches `strlen` once per authored non-extern string parameter
- lowers `char_at(lhs, i) == char_at(rhs, j)` into validity checks plus direct byte loads
- treats internal string parameters as borrowed aliases instead of doing caller retain plus callee release on every helper call

## Benchmark delta

### `string_ops`

Session baseline after runtime debloat:

- Kain median: `136.6249 ms`
- Rust median: `10.2892 ms`
- JavaScript median: `51.2051 ms`

Final post-pass run:

- Kain median: `14.3292 ms`
- Rust median: `8.5250 ms`
- JavaScript median: `49.2466 ms`
- Python median: `310.2037 ms`

Result:

- Kain improved by about `9.53x` versus the session baseline
- Kain is now about `3.44x` faster than JavaScript on this case
- Kain is now about `1.68x` slower than Rust instead of over `13x` slower

## IR evidence

### `string_ops` is now mostly scalar

- `benchmark/out/build/string_ops/kain/string_ops.ll:9645`
  - `starts_with_at` hoists `strlen(text)` once
- `benchmark/out/build/string_ops/kain/string_ops.ll:9648`
  - `starts_with_at` hoists `strlen(needle)` once
- `benchmark/out/build/string_ops/kain/string_ops.ll:9683`
  - direct `getelementptr inbounds i8` byte load for `text[index + offset]`
- `benchmark/out/build/string_ops/kain/string_ops.ll:9685`
  - direct `getelementptr inbounds i8` byte load for `needle[offset]`
- `benchmark/out/build/string_ops/kain/string_ops.ll:9739`
  - `find_substring` now calls `starts_with_at` without RC retain ping-pong on the string args

## Remaining highest-payoff losers

### `struct_method`

Fresh targeted rerun:

- Kain median: `64.3651 ms`
- Rust median: `12.0470 ms`
- JavaScript median: `50.0848 ms`

Current cause:

- `benchmark/out/build/struct_method/kain/struct_method.ll:9587`
  - `make_pair` still returns `%BenchPair*`
- `benchmark/out/build/struct_method/kain/struct_method.ll:9593`
  - tiny two-field POD still calls `@KAIN_alloc`
- `benchmark/out/build/struct_method/kain/struct_method.ll:9650`
  - hot loop allocates through `make_pair`
- `benchmark/out/build/struct_method/kain/struct_method.ll:9654`
  - hot loop then immediately scores the heap object

Next move:

- scalarize non-escaping POD structs into SSA or entry-block stack slots

### `option_result`

Fresh targeted rerun:

- Kain median: `46.5871 ms`
- Rust median: `9.1133 ms`
- JavaScript median: `46.3393 ms`

Current cause:

- `benchmark/out/build/option_result/kain/option_result.ll:9586`
  - `maybe_value` still returns boxed `i8*`
- `benchmark/out/build/option_result/kain/option_result.ll:9596`
  - `None` path still allocates
- `benchmark/out/build/option_result/kain/option_result.ll:9611`
  - `Some(Int)` still allocates a tagged box
- `benchmark/out/build/option_result/kain/option_result.ll:9639`
  - `Err("skip")` still rebuilds the error string with `string_new`
- `benchmark/out/build/option_result/kain/option_result.ll:9643`
  - `Err` still allocates a tagged box
- `benchmark/out/build/option_result/kain/option_result.ll:9662`
  - `Ok(Int)` still allocates a tagged box

Next move:

- de-box thin `Option<Int>` / `Result<Int, String>` shapes into scalar tag + payload
- intern static error strings instead of rebuilding them

## Formal verification

- `mcp__z3_local__.run_proof_pack(path="D:/Kain-Lang/crates/kain-sys-codegen/z3", lane="llvm", report_name="llvm-string-ops-fast-path-and-borrowed-call")`
- Outcome: `16 proved`, `0 counterexamples`, `0 unknown`, `0 errors`
- Report:
  - `crates/kain-sys-codegen/z3/reports/20260515T213552Z-llvm-string-ops-fast-path-and-borrowed-call.json`
