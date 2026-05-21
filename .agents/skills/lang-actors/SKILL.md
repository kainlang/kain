---
name: lang-actors
description: Use when authoring, extending, or debugging Kain actor usage in `.kn` source, including actor declarations, `spawn`, `send`, `ask`, mailbox-facing workflows, actor-driven demos, and actor-centric benchmark or blade code, without changing actor scheduler or runtime internals.
---

# Lang Actors

## Overview

This skill owns the authored side of Kain actors. Use it when a task is about how application or benchmark code should model work with actors, how messages should flow in Kain source, or how to dogfood actor semantics in blades and examples.

## Start Here

- Read the nearest actor example first: `blades/actor-ask-roundtrip/src/main.kn`, `benchmark/cases/quantumerlang/main.kn`, `blades/pong/src/main.kn`, or `blades/kain-example/src/main.kn`.
- Keep actor behavior in Kain unless the request is explicitly about runtime machinery.
- Prefer typed, named message flows and proof-oriented examples over anonymous fire-and-forget spaghetti.

## Routing

- Stay here for `.kn` actor declarations, `on` handlers, `spawn`, `send`, `ask`, world-to-actor coordination, and authored actor demos.
- Switch to `bootstrap-actors` when the task changes actor semantics in parser/type/lowering/compiler-owned lanes.
- Switch to `runtime-core` when the task changes the native actor substrate, scheduler, mailbox policy, ABI glue, or crash behavior below authored Kain.
- Co-trigger `lang-semantics` when actor behavior is fused with `world`, `entangle`, `teleport`, or `pulse`.

## Authoring Rules

- Make actors prove something real: request/reply shape, supervision flow, entangled state propagation, or pressure under benchmark load.
- If a user asks for "make the actor example work" and the blocker is engine-side, do not paper over it with weaker Kain code. Surface the authored layer separately and route the subsystem fix to `bootstrap-actors` or `runtime-core`.
- Keep examples close to production semantics. Avoid toy actors that teach bad habits about message naming, reply lanes, or state ownership.
