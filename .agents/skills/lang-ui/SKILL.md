---
name: lang-ui
description: Use when authoring Kain UI code, Kain-owned layout/widget flows, passive UI projections, or blade-level desktop experiences in `.kn` files, without taking ownership of native host generation, runtime UI substrate internals, or package framework internals.
---

# Lang UI

## Overview

This skill owns authored UI in Kain. Use it when a task is about Kain-side layout, component composition, action wiring, or UI-facing blades, and the work should stay in `.kn` source rather than drifting into host/runtime internals.

## Start Here

- Read the nearest authored UI lane first: `blades/kaintana/src/kaintana.kn`, `blades/kaintana-test/src/main.kn`, `blades/pong/src/main.kn`, and any UI-specific files in `blades/kain-example/src/`.
- Keep authored UI declarative where possible and let the runtime/package layer stay passive.
- Use real acceptance blades when the UI shape matters, not isolated toy fragments.

## Routing

- Stay here for Kain-authored UI components, layout trees, actions, and blade-level UI experiences.
- Switch to `package-kaintana` when the work changes the Kaintana framework surface itself.
- Switch to `runtime-stdlib` when the underlying UI host/session/runtime-backed stdlib behavior needs to change.
- Switch to `runtime-core` when the native UI substrate or ABI floor is the real blocker.
- Co-trigger `lang-gpu` when the UI surface is tightly coupled to graphics or shader work.

## Authoring Rules

- Keep Kain code responsible for the authored experience, not for reimplementing the host substrate in user space.
- If a UI blade reveals a host/runtime limitation, keep the authored layer clean and route the engine repair to `runtime-stdlib`, `runtime-core`, or `package-kaintana`.
- Strengthen one real blade or acceptance surface whenever you introduce a new authored UI pattern worth copying.
