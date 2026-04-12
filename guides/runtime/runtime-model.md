# Runtime Model

Kain's runtime is a real execution engine, not only a validation pass.

## Core Runtime Objects

- `Env` stores scopes, functions, patches, laws, converges, worlds, components,
  actor definitions, patch history, and extension state.
- `Value` represents runtime values such as arrays, tuples, structs, closures,
  actor refs, futures, results, JSX nodes, and control-flow sentinels.
- `Message` is the actor-message payload type.
- `ActorRef` is the runtime handle that points at an actor mailbox.

## What `Env::new()` Does

The default environment registers:

- source-loaded stdlib functions
- network helpers
- JSON helpers
- the KOS/native bridge helpers
- any installed environment extensions

That means runtime behavior can be extended in a data-driven way instead of
hardcoded per call site.

## Runtime Registration

The runtime can register:

- ordinary functions
- patches, laws, converges, worlds, orchestrations
- components
- inline modules
- native functions
- typed programs

This is why Kain source can both execute directly and also produce structured
runtime contracts for downstream hosts.

## Execution Lanes

The interpreter currently supports:

- direct interpretation
- test execution
- actor/message semantics
- async/future semantics
- JSX/UI evaluation
- patch history and replay

## Contract Outputs

The runtime also feeds compiler-owned bundles:

- runtime contract bundles
- realtime app bundles
- shader and compute sidecars

Those emitted bundles are what downstream native and host runtimes consume.
