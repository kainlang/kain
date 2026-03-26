# Universal Web Template

`universal/` is the serious Kain web starter that assumes:

- users should not need `rustc` or `cargo`
- Kain is still the orchestration language
- Node FFI is the practical browser/runtime lane today
- Kain UI should still shape authoring and preview where it already has real semantic strength

## Entry Points

- `src/main.kn`
  - builds the full experience matrix into `outputs/sites`
- `src/native_preview.kn`
  - semantic Kain UI preview surface for the pack
- `src/actor_server.kn`
  - prints the actor report for the hybrid actor-oriented experience

## Runtime Surface

- `helpers/web_runtime.mjs`
  - manifest loader
  - HTML and client-island renderer
  - client bundler (Preact + Three.js) for React/TypeScript-esque islands
  - KainScript (`.ks`) support in the client bundle loader
  - local search, chat, prompt-deck, catalog, app/auth/commerce/data/realtime, session, uploads, analytics, form, and route APIs
  - static artifact writer
  - actor-aware local HTTP + SSE + WebSocket server
- `package.json`
  - Node scripts for build, inspect, and local serving

## Manifest Surface

- `manifests/app.json`
  - top-level app, output, SEO, and runtime configuration
- `manifests/themes/*.json`
  - visual systems
- `manifests/content/*.json`
  - copy, pricing, testimonials, docs links, prompts, forms, routes, and search documents
- `manifests/scenes/*.json`
  - immersive scene descriptors
- `manifests/experiences/*.json`
  - final archetype compositions and section layouts

## Archetypes Included

- `business_launch`
- `portfolio_signal`
- `immersive_luminous`
- `chat_orbit`
- `actor_mesh_foundry`
- `knowledge_atlas`
- `operator_foundry`
- `hybrid_command`
- `app_foundry`
- `commerce_signal`
- `realtime_constellation`

## Artifacts Emitted Per Experience

- `index.html`
- `blog/index.html` (when `content.blog_posts` is configured)
- `blog/<slug>/index.html` (markdown-driven post pages)
- `site.manifest.json`
- `actor-server.plan.json`
- `site.data.json`
- `system.contract.json`
- `ui.schema.json`
- `sitemap.xml`
- `robots.txt`
- `feed.xml`
- `social-card.svg`

## Shared Artifacts (Written Once Per Build)

- `../client/kain-client.bundle.js`
- `../client/kain-client.bundle.js.meta.json`

## Typical Usage

Install dependencies:

```powershell
npm install
```

Use the Kain entrypoint:

```powershell
kain run src/main.kn
```

Use the Node-only scripts:

```powershell
npm run build
npm run catalog
npm run serve:hybrid
npm run serve:docs
npm run serve:operator
npm run serve:app
npm run serve:commerce
npm run serve:realtime
npm run experience:hybrid
npm run actor:hybrid
npm run contract:hybrid
npm run ui:hybrid
```

Client bundle only:

```powershell
npm run bundle:client
```

KainScript support:

- `helpers/client/lib/kain_script_bridge.ks` is a sample KainScript module used by the client bundle.
- `.ks` files are bundled alongside TS/TSX without requiring Rust tooling.

Switch to a TypeScript-aware helper runtime by changing `[node_ffi]` in
`KAIN.toml` from `node` to `npx` with `tsx`.

New reusable section kinds in this pass:

- `prompt_deck`
- `process_steps`
- `capability_matrix`
- `blueprint_grid`
- `app_shell`
- `auth_panel`
- `auth_session`
- `commerce_stack`
- `integration_grid`
- `realtime_channels`
- `data_collections`
- `uploads_lab`
- `analytics_lab`
- `card_grid` with `content.chat_personas`
- `card_grid` with `content.chat_modes`
- `process_steps` with `content.actor_playbooks`
- `card_grid` with `content.actor_tools`
