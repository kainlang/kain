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
  - local search, frontend stack + UI runtime, chat runtime, actor runtime, agent knowledge/memory/tool registries, chat (playbooks/tools/memory), prompt-deck, UI kit, catalog, app/auth/commerce/data/realtime, 3D scene assets/materials/lighting/cameras/animation/physics/audio/XR/shaders, growth/experiments/service catalog, support + feedback + survey + messaging + payments + scheduling + privacy lanes, actor jobs/schedules/hosts, runtime hosts + deployment targets, session, uploads, analytics, form, and route APIs
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
  - copy, pricing, testimonials, docs links, prompts, forms, routes, growth/experiment/service data, and search documents
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
- `process_steps` with `content.chat_playbooks`
- `card_grid` with `content.chat_tools`
- `card_grid` with `content.chat_memory`
- `process_steps` with `content.actor_playbooks`
- `card_grid` with `content.actor_tools`
- `actor_topology`
- `ui_kit` (UI components + layouts + tokens island)
- `metric_grid` with `content.actor_metrics`
- `status_board`
- `roadmap_timeline`
- `team_grid`
- `partner_grid`
- `press_kit`
- `careers_list`
- `support_grid`
- `security_grid`
- `growth_stack`
- `experiment_board`
- `service_catalog`
- `success_playbooks`
- `notification_matrix`
- `release_notes`
- `feature_flags`
- `incident_response`
- `crm_pipeline`
- `community_hub`
- `event_schedule`
- `newsletter_panel`
- `compliance_grid`
- `observability_stack`
- `infrastructure_stack`
- `localization_grid`
- `accessibility_grid`
- `performance_targets`
- `legal_links`

New system registries in this pass:

- identity providers, roles, and access policies
- identity verification lanes
- fraud and risk controls
- consent and preference center metadata
- audit log metadata
- data export pipelines
- billing plans, invoices, and tax metadata
- subscription tiers and entitlements
- CMS content types and editorial workflow
- media libraries and asset pipelines
- automation flows and webhook event contracts
- API reference registry + developer portal tools
- SEO targets and social metadata
- chat agent roster, tools, and workflows
- UI components, layouts, and design token registries
- frontend stack registry (TypeScript/React-like runtime, routing, state, data, build)
- UI runtime registry (schema, modules, islands, tokens, layouts)
- chat runtime registry (streaming, personas, playbooks, tools, memory, safety)
- actor runtime registry (routes, mesh, supervision, queues, metrics, tools)
- frontend framework targets (React/Next/Remix/Astro/SvelteKit/Vue/Solid compatibility planning)
- expanded UI component + layout recipes (portfolio, docs, commerce, realtime)
- expanded infrastructure + observability stack metadata (edge, serverless, data, logs, cost)
- expanded security controls and performance/accessibility targets
- actor policies and actor metrics
- scene pipeline, render stack, interaction modes, and device profiles
- model stack, voice stack, and moderation policies
- actor supervision and actor queue registries
- data governance and backup plan registries
- support tickets, feedback loops, and survey programs
- messaging, payments, scheduling, and privacy request registries
- enablement programs, onboarding flows, reliability SLOs, data retention policies, and incident history registries
- product catalog and inventory stack registries
- fulfillment, shipping, and returns policy registries
- loyalty and referral program registries
- paid acquisition and personalization stack registries
- customer portal registry
- data platform registry
- release notes and changelog entries
- feature flag registry
- incident response playbooks
- CRM pipeline stages
- scene asset, material, lighting, camera, animation, physics, spatial audio, XR, and shader registries
- streaming stack registry for chat + ops previews
- knowledge sources, memory stores, tool registry, and agent workflow registries
- actor job, schedule, and host registries
- runtime host and deployment target registries
- brand system registry (voice, identity, motion)
- social presence and channel registry
- content calendar registry
- release pipeline registry
- QA program registry
- domain + edge stack registry
- trust center registry
- edge runtime registry (edge execution + cache routing)
- worker runtime registry (cron + queue workers)
- API gateway registry (routing + validation)
- rate limit registry (route + actor throttling)
- cache stack registry (edge/session/search cache lanes)
- search stack registry (text + vector + hybrid)
- storage stack registry (artifacts, uploads, archives)
- session store registry (cookie + token + actor sessions)
- marketplace stack registry
- content syndication registry (rss, email, partner embeds)
