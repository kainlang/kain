---
title: "Shipping web without Rust"
summary: "Kain owns authored intent; Node FFI owns browser packaging. No cargo required."
published_at: "2026-03-26"
tags:
  - kain
  - node
  - ffi
  - templates
---

This template is designed around one simple promise:

- **Users should not need** `rustc` or `cargo`.
- **Kain still owns** authored intent, orchestration, and the semantic UI preview loop.
- **Node owns** the ecosystem lane: bundling, local serving, and actor-server glue.

## What to edit first

1. `manifests/content/*.json` for copy, sections, pricing, docs links, prompts, and forms.
2. `manifests/themes/*.json` for colors + typography.
3. `manifests/experiences/*.json` to compose section layouts.

Only touch `helpers/web_runtime.mjs` when you are adding genuinely new systems (new section kinds, new runtime APIs, or new artifact emitters).

## Why this architecture holds up

Because the build is **manifest-driven**, you can add experiences without cloning the whole site scaffold. The template treats “site modes” like data, not forks.
