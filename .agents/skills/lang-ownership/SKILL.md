---
name: lang-ownership
description: Use when authoring Kain ownership-state code, including `collapse`, `observe`, `decay`, memory-sensitive flows, and ownership-heavy demos or benchmarks, without changing the ownership model implementation underneath.
---

# Lang Ownership

Use this skill when the problem is about how ownership should be expressed in Kain.

## Fast Loop

```powershell
rg -n "collapse |observe |decay" blades benchmark smoketest
kain check <entry.kn> --target llvm
kain run <entry.kn-or-blade>
```

## Kain Pattern

```kn
fn fold_cells(cells: ptr<Int>, count: Int) -> Int:
    var i: Int = 0
    var acc: Int = 0
    while i < count:
        acc = acc + mem_load(ptr_offset(cells, i, "Int"), "Int")
        i = i + 1
    return acc

fn main() -> Int with Unsafe:
    let cells = alloc_zeroed(8, "Int")
    collapse cells:
        mem_store(ptr_offset(cells, 0, "Int"), 7, "Int")
        0
    let total = observe cells:
        fold_cells(cells, 8)
    decay cells
    return total - 7
```

## What To Do

- Use ownership constructs to make lifetime and exclusivity visible in the authored code.
- Prefer real ownership pressure over decorative syntax. The point is to prove movement, observation, or teardown.
- Keep the ownership-shaped code intact when an engine bug shows up; route the engine bug instead of deleting the ownership lane.

## Hand Off When

- Use `lang-systems` when ownership is part of actors, effects, raw pointers, zero-copy buffers, cache/bit lanes, scheduler pressure, or unsafe systems authoring.
- Use `bootstrap-ownership` when semantic, typing, or lowering truth changes.
- Use `runtime-core` when the native runtime helpers or heap behavior are wrong under authored ownership code.
- Co-trigger `lang-semantics` when ownership is fused with `world`, `entangle`, `teleport`, or `patch`.
