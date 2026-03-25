# Kain Web Templates

This folder is the repo-backed source for the Windows path `K:\templates\web`.

The first serious pack in this lane is `universal/`, a no-Rust-required web
template built around four rules:

1. Kain owns authored app intent, semantic UI preview, and orchestration.
2. Node owns browser packaging, local serving, and actor-server runtime glue.
3. themes, content, scenes, and experiences are manifest-driven instead of
   copied into one-off starter code.
4. repeated website boilerplate belongs in reusable stdlib wrappers or helper
   runtimes, not in every template entrypoint.

## Current Pack

- `template-pack.toml`
  - data-driven registry for the template pack surface
- `universal/`
  - one starter that can emit business, portfolio, immersive 3D, chat-heavy,
    and actor-server-oriented sites from manifest switches

## What Universal Includes

- a Kain UI native preview entrypoint for semantic surface authoring
- a Kain entrypoint that builds the full experience matrix through Node FFI
- a Kain entrypoint that reports actor-server topology
- a Node helper runtime with zero third-party dependencies
- manifest registries for themes, content, scenes, and experiences
- a `package.json` script surface so users can build or serve without Rust or
  Cargo

## Intent

This folder is not meant to be a pile of isolated starters.
It is meant to become a reusable web systems layer for Kain:

- static marketing sites
- portfolio and case-study sites
- immersive 3D storytelling shells
- chat-first product surfaces
- actor-based local web servers and realtime dashboards
