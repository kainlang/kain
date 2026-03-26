# Web Template Limitations

This file tracks real gaps that the template currently has to route around.
Do not delete entries just because the template has a workaround.

## Language / Runtime Gaps To Address Upstream

### 1. Semantic Kain UI does not yet lower directly to a first-class web backend

Current template approach:

- use Kain UI for native semantic preview
- use Node FFI plus a manifest-driven HTML runtime for browser output

Requested upstream capability:

- a real `kain-ui-web` lane that consumes the same semantic UI IR and patch stream family described in `crates/kain-ui/NORTH_STAR_SPEC.md`

### 2. Manifest-heavy authoring still leans on JSON + JS helper code

Current template approach:

- themes, content, scenes, and experiences live in JSON registries
- Node helper loads, validates, merges, and renders them

Requested upstream capability:

- first-class Kain data authoring ergonomics for rich object literals, schema-backed manifests, and data validation without routing through JS for every content-heavy web starter

### 3. Actor-server runtime is Node-hosted rather than Kain-native

Current template approach:

- Node `http` server owns the long-running process
- Kain generates, inspects, and reports on actor topology through FFI

Requested upstream capability:

- a durable actor/runtime server lane in Kain that can own long-lived web process orchestration directly while still exposing Node adapters when useful

### 4. Client islands are JS-authored, not semantic Kain-authored

Current template approach:

- the browser runtime hydrates metric cards, filters, FAQ, chat, search, and forms with a small JS island layer

Requested upstream capability:

- Kain-authored browser interaction modules that can target JS, KS, and future semantic UI web runtimes without hand-writing local browser scripts

### 5. Template scaffolding is registry-driven but not yet CLI-exposed

Current template approach:

- the pack is ready to copy and specialize
- `package.json` scripts and Kain entrypoints make it runnable once copied

Requested upstream capability:

- first-class `kain init web` or equivalent template selection that can choose archetypes and hydrate manifests without requiring manual folder copying

### 6. Search, forms, feeds, and sitemap output are helper-owned rather than compiler-owned

Current template approach:

- the helper runtime synthesizes search APIs, intake capture, RSS, robots, and sitemap files from manifest data

Requested upstream capability:

- first-class Kain-side emitters or standard-library surfaces for search indexes, form contracts, feed generation, and crawl metadata so these do not stay trapped in a JS helper forever

### 7. Browser asset pipelines and richer scene execution are still externalized

Current template approach:

- the immersive lane uses scene descriptors and stylized HTML/CSS shells
- there is no first-class Kain-managed browser asset graph, bundler contract, or WebGPU/WebGL scene backend here yet

Requested upstream capability:

- a Kain-owned browser asset/runtime lane that can materialize static assets, bundle client modules, and bind scene contracts into real browser rendering backends from the same authored source family

### 8. Auth, payments, and persistent data contracts are still template-owned gaps

Current template approach:

- forms persist locally to JSONL during prototyping
- operator routes, docs requests, and handoff flows stop at local helper-owned persistence

Requested upstream capability:

- first-class Kain-side contracts for authentication, payment intents, and persistent typed storage so serious product sites do not have to invent these on top of the helper runtime

### 9. Streaming and socket-grade realtime flows are only lightly previewed

Current template approach:

- the helper runtime now exposes a basic server-sent-events preview route
- there is no first-class Kain-owned websocket or browser realtime contract here yet

Requested upstream capability:

- a durable Kain runtime surface for streaming/browser session state so chat-heavy and operator-heavy sites can graduate from helper-owned previews to real realtime orchestration

### 10. React-like component state and routing contracts are still helper-owned schemas

Current template approach:

- `ui.schema.json` and app module manifests describe component layout, form counts, and workspace routes
- browser interactivity still lives in helper-authored JS islands instead of Kain-authored browser components

Requested upstream capability:

- a first-class Kain-authored component and routing lane for the web so React/TypeScript-style application shells can be expressed semantically and lowered without helper-owned island code
