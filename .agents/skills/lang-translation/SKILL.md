---
name: lang-translation
description: Use when translating Rust, C, C++, JavaScript, TypeScript, Python, or tooling code into idiomatic Kain, preserving intent while reshaping the design around Kain semantics instead of performing a mechanical line-by-line port.
---

# Lang Translation

Use this skill when the task starts in another language and should end as real Kain.

## Fast Loop

```powershell
kain import-c <input> -o <generated.kn>
kain import-rust <input> -o <generated.kn>
kain import-ts <input> -o <generated.kn>
py .agents\skills\lang-translation\scripts\select_translation_examples.py --repo . --top 10
kain check <translated.kn> --target llvm
```

## Translation Rule

Translate intent, not syntax.

```text
C/Rust loop + shared mutable state
    -> maybe Kain actor, world, entangle, or converge lane

manual buffer ownership
    -> maybe collapse / observe / decay

host callback soup
    -> maybe actor messages or package-owned bridge helpers
```

## Tiny Kain Result Shape

```kn
converge mix(value: Int) -> Int:
    spec reference:
        return (value * 31) + 7
    fast llvm_lane when target("llvm"):
        return (value * 31) + 7
    verify random(8)
```

## What To Do

- Use the importer commands to get a starting point when that is faster than hand-porting from zero.
- Then rewrite toward Kain semantics. Imported output is a draft, not the finished shape.
- Prefer Kain modules, stdlib, ownership, actors, worlds, and converge lanes where they make the design stronger.

## Hand Off When

- Co-trigger the specific authored domain skill once the translation clearly becomes `lang-stdlib`, `lang-semantics`, `lang-systems`, `lang-actors`, `lang-ownership`, `lang-ui`, `lang-gpu`, `lang-interop`, or `lang-c-abi-ffi`.
- Use `bootstrap-core` or `runtime-*` only when the translation is blocked by missing engine capability rather than authored design.
