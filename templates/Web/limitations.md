# Web Template Limitations

This file tracks real gaps that the template currently has to route around.
Do not delete entries just because the template has a workaround.

## Language / Runtime Gaps To Address Upstream

### 1. Semantic Kain UI does not yet lower directly to a first-class web backend

Current template approach:

- use Kain UI for native semantic preview
- use Node FFI plus manifest-driven HTML runtime for browser output

Requested upstream capability:

- a real `kain-ui-web` lane that consumes the same semantic UI IR and patch
  stream family described in `crates/kain-ui/NORTH_STAR_SPEC.md`

### 2. Manifest-heavy authoring still leans on JSON + JS helper code

Current template approach:

- themes, content, scenes, and experiences live in JSON registries
- Node helper loads, validates, merges, and renders them

Requested upstream capability:

- first-class Kain data authoring ergonomics for rich object literals,
  schema-backed manifests, and data validation without routing through JS for
  every content-heavy web starter

### 3. Actor-server runtime is Node-hosted rather than Kain-native

Current template approach:

- Node `http` server owns the long-running process
- Kain generates, inspects, and reports on actor topology through FFI

Requested upstream capability:

- a durable actor/runtime server lane in Kain that can own long-lived web
  process orchestration directly while still exposing Node adapters when useful

### 4. Client islands are JS-authored, not semantic Kain-authored

Current template approach:

- the browser runtime hydrates metric, portfolio, chat, and actor widgets with
  a tiny JS island layer

Requested upstream capability:

- Kain-authored browser interaction modules that can target JS, KS, and future
  semantic UI web runtimes without hand-writing local browser scripts

### 5. Template scaffolding is registry-driven but not yet CLI-exposed

Current template approach:

- the pack is ready to copy and specialize
- `package.json` scripts and Kain entrypoints make it runnable once copied

Requested upstream capability:

- first-class `kain init web` or equivalent template selection that can choose
  archetypes and hydrate manifests without requiring manual folder copying
