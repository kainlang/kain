---
name: lang-translation
description: Use when translating Rust, C, C++, JavaScript, TypeScript, Python, or tooling code into idiomatic Kain source, choosing the right Kain semantics for a foreign design, and producing real `.kn` replacements or migrations rather than mechanical line-by-line ports.
---

# Lang Translation

## Overview

This skill owns language-to-Kain translation on the authored side. Use it when the task is to take an existing implementation or idea from another language and rewrite it into real Kain that feels native to the repo instead of preserving the donor language's shape.

## Start Here

- Read `references/example-atlas.md` and `references/translation-patterns.md` before starting a large translation.
- Use `references/benchmark-translation-compass.md` when the source problem smells like a benchmark or pressure lane rather than an application module.
- Run `scripts/select_translation_examples.py` if you want a fast pointer to nearby repo exemplars before authoring.

## Routing

- Stay here for authored `.kn` translation, semantic reshaping, module decomposition, and Kain-first API selection.
- Switch to `lang-stdlib`, `lang-semantics`, `lang-actors`, `lang-ownership`, `lang-ui`, or `lang-gpu` when the translation clearly lands in one of those domains.
- Switch to `bootstrap-core` when translation is blocked by missing language/compiler support rather than authored design.
- Switch to `runtime-core`, `runtime-stdlib`, or `runtime-gpu` when the translated design needs host/runtime capability that does not exist yet.

## Translation Rules

- Translate intent, not syntax. If the source code emulates actors, ownership, world state, or dataflow manually, replace that machinery with native Kain constructs.
- Avoid preserving donor-language package boundaries when Kain's module and blade model offers a cleaner split.
- If a translation exposes a real compiler/runtime deficit, do not flatten the design into a weaker Kain program just to get green lights. Preserve the intended authored shape and route the blocker.
