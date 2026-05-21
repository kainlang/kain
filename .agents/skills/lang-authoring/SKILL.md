---
name: lang-authoring
description: Use when writing or reshaping Kain source as Kain, including module shape, public imports, idiomatic control flow, blade-facing `.kn` code, and routing authored work into `lang-semantics`, `lang-systems`, `lang-interop`, stdlib, UI, GPU, C-ABI, translation, or blade/project lanes without changing compiler, runtime, or build internals.
---

# Lang Authoring

Use this skill when the job is "write Kain" rather than "fix the compiler/runtime."

## Fast Loop

```powershell
rg -n "world |actor |converge |patch |law |shader |collapse |observe |decay |ptr<|mem_load|use std::|use c::" library_of_kain blades benchmark smoketest stdlib
kain check <entry.kn> --target llvm
kain run <entry.kn-or-blade>
kain build <entry.kn> --target llvm -o <output.exe>
```

## Kain Shape

```kn
use std::runtime
use std::text

const EXIT_OK: Int = 0

fn score(seed: Int) -> Int:
    return (seed * 17) + 3

fn main() -> Int:
    let value = score(7)
    println(format("score={value}"))
    return EXIT_OK
```

## What To Do

- Search for a nearby pattern first, then author the smallest real Kain surface that proves the feature.
- Prefer root `std.*` imports, top-level constants, named helpers, and real language constructs over transliterated Rust/C++ structure.
- Keep authored behavior in Kain. Push OS, driver, ABI, package, and runtime substrate concerns behind sibling skills instead of bloating the `.kn` file.

## Hand Off When

- Use `lang-semantics` when first-class Kain language features are dominant.
- Use `lang-systems` when actors, effects, ownership, raw memory, zero-copy, unsafe, or backpressure systems code is dominant.
- Use `lang-interop` when authored Kain crosses native, C ABI, DLL, OS, vendor SDK, Rust crate, host bridge, or package boundary surfaces.
- Use `lang-ui`, `lang-gpu`, `lang-c-abi-ffi`, `lang-stdlib`, or `lang-translation` when one of those narrower domains is dominant.
- Use `bootstrap-core` if the blocker is parser, typing, lowering, imports, or compiler-owned semantics.
- Use `runtime-core` or `runtime-stdlib` if the missing capability lives below authored Kain.
