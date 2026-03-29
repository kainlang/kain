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

- the universal pack now ships a bundled island lane (TypeScript + Preact) for app shells, chat, realtime, and WebGL scenes
- baseline hydration (metrics, filters, FAQ, forms, search) still lives in helper-authored browser scripts

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

- the pack now exposes a first real WebGL lane via a bundled Three.js scene island driven by the manifest scene descriptor
- the bundler is still helper-owned (Node + esbuild) and not yet a first-class Kain compiler/runtime surface

Requested upstream capability:

- a Kain-owned browser asset/runtime lane that can materialize static assets, bundle client modules, and bind scene contracts into real browser rendering backends from the same authored source family

### 8. Auth, payments, and persistent data contracts are still template-owned gaps

Current template approach:

- forms persist locally to JSONL during prototyping
- database, queue, secrets, and config stacks are manifest-only metadata until native runtime adapters land
- operator routes, docs requests, and handoff flows stop at local helper-owned persistence

Requested upstream capability:

- first-class Kain-side contracts for authentication, payment intents, and persistent typed storage so serious product sites do not have to invent these on top of the helper runtime

### 9. Streaming and socket-grade realtime flows are only lightly previewed

Current template approach:

- the helper runtime exposes SSE routes that tick continuously plus websocket endpoints (`/ws/realtime`, `/ws/chat`) for local previews
- the realtime/socket layer is still Node-owned; Kain inspects and authors the contract through manifests and FFI

Requested upstream capability:

- a durable Kain runtime surface for streaming/browser session state so chat-heavy and operator-heavy sites can graduate from helper-owned previews to real realtime orchestration

### 10. React-like component state and routing contracts are still helper-owned schemas

Current template approach:

- `ui.schema.json` and app module manifests describe component layout and workspace routes
- Preact islands provide a ready-to-go React/TypeScript-esque runtime, but the component model is still not authored semantically in Kain UI IR yet

Requested upstream capability:

- a first-class Kain-authored component and routing lane for the web so React/TypeScript-style application shells can be expressed semantically and lowered without helper-owned island code

### 11. Chat + LLM provider adapters are still template-owned stubs

Current template approach:

- chat routes return seeded responses and lightweight prompt routing
- no first-class model provider adapters or tool-calling surface exists inside Kain yet

Requested upstream capability:

- Kain-owned LLM adapter contracts (streaming, tool calls, guardrails) so chat-heavy sites can be powered without hand-authored Node glue

### 12. Durable storage and vector search are not first-class

Current template approach:

- form submissions, analytics, and uploads are persisted to local JSONL files
- search is a helper-owned in-memory index over manifest data

Requested upstream capability:

- Kain-side persistence contracts for relational + object storage plus vector search so docs/search hubs can scale beyond local previews

### 13. Deployment targets are still local-runtime focused

Current template approach:

- Node helper runtime assumes a local server process with filesystem access
- no dedicated serverless/edge packaging path is exposed from Kain

Requested upstream capability:

- first-class Kain deployment targets for serverless and edge runtimes so actor-server sites can ship without retooling the helper runtime

### 14. Messaging, scheduling, support ticketing, and privacy request providers are helper-owned

Current template approach:

- messaging, scheduling, ticket queues, and privacy request registries are manifest-driven
- the runtime exposes JSON endpoints, but no first-class provider adapters exist inside Kain

Requested upstream capability:

- Kain-owned adapters for email/SMS/push, scheduling calendars, ticketing systems, and privacy request workflows so these systems can move beyond helper-owned metadata

### 14. Voice + multimodal chat lanes are still browser/Node-only

Current template approach:

- voice stack and moderation registries live in manifest data
- any actual voice capture or streaming still relies on browser APIs + helper code

Requested upstream capability:

- first-class Kain-side voice/multimodal contracts and streaming adapters so chat-first sites can stay Kain-owned end to end

### 15. 3D web runtime execution is still helper-owned

Current template approach:

- 3D pipeline and render-stack metadata are manifest-only
- WebGL/Three.js islands and bundling live in the Node helper runtime

Requested upstream capability:

- a Kain-owned web 3D runtime lane that can consume scene contracts, render stacks, and device profiles without helper-side logic

### 16. React/Next/Astro framework parity is manifest-only

Current template approach:

- frontend stack registries describe React/Next/Astro/SvelteKit/Vue targets
- the actual runtime is the bundled Preact + island lane in the Node helper

Requested upstream capability:

- first-class Kain web targets or adapter lanes for major web frameworks so the registry entries can become runnable without retooling the helper runtime

### 17. Consent, verification, fraud, audit, export, and marketplace lanes are metadata-only

Current template approach:

- consent center, identity verification, fraud/risk, audit logs, data exports, marketplace, and syndication systems are declared as manifest data
- the helper runtime only exposes JSON endpoints for these lanes, without provider adapters or runtime enforcement

Requested upstream capability:

- first-class Kain-side contracts and adapter lanes for consent/cookies, identity verification, fraud/risk, audit logs, data exports, marketplace payouts, and content syndication so these systems can move beyond static metadata

### 18. Tenancy, SSO/SCIM, API keys, integration marketplace, event bus, and data pipelines are metadata-only

Current template approach:

- tenant management, SSO/SCIM, API key management, integration marketplace, event bus, data pipelines, compliance frameworks, and ops runbooks live in manifest data
- the helper runtime exposes JSON endpoints for these systems but does not enforce enterprise identity, provisioning, or event routing logic

Requested upstream capability:

- Kain-native enterprise identity/tenancy contracts plus event-bus and data-pipeline adapters so these systems can graduate from metadata to executable runtime lanes

### 19. PWA manifest + service worker emission remain helper-owned

Current template approach:

- the Node helper runtime emits `manifest.webmanifest`, `service-worker.js`, `offline/index.html`, and an installable SVG icon
- offline routing and precache lists are configured in `manifests/app.json`

Requested upstream capability:

- Kain-native web runtime surface for PWA manifests, offline caches, and install flows so installability is not helper-owned

### 20. Creative systems (copy decks, email templates, campaigns, icon/motion/illustration libraries) are helper-owned

Current template approach:

- creative systems live as manifest data and are rendered by the Node helper
- no first-class Kain authoring or adapter lane exists for email templates, asset libraries, or campaign workflows

Requested upstream capability:

- Kain-native creative system authoring + adapter lanes for email templates, asset libraries, and campaign automation so these systems can be compiled and validated without helper-owned rendering
