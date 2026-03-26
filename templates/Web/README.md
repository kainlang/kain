# Kain Web Templates

This folder is the repo-backed source for the Windows path `K:\templates\web`.

The universal pack is the current serious starter for Kain web work. It is built
around four rules:

1. Kain owns authored app intent, semantic UI preview, and orchestration.
2. Node owns browser packaging, local serving, search APIs, and actor-server runtime glue.
3. themes, content, scenes, and experiences are manifest-driven instead of copied into one-off starter code.
4. repeated website boilerplate belongs in shared helper runtimes and stdlib wrappers, not in each new site.

## Current Pack

- `template-pack.toml`
  - data-driven registry for the template pack surface
- `universal/`
  - one starter that can emit business, portfolio, immersive 3D, chat, docs, operator, actor-server, and hybrid sites from manifest switches

## What Universal Includes

- Kain entrypoints for build orchestration, actor reporting, and semantic Kain UI preview
- a shared Node helper runtime for manifest loading, HTML rendering, search, forms, feed/sitemap output, and local serving
- manifest registries for themes, content, scenes, and experiences
- archetypes for business, portfolio, immersive 3D, chat, actor-server, and hybrid site modes
- website systems for docs links, search, FAQ, pricing, testimonials, prompt decks, capability matrices, blueprint grids, local form capture, actor routes, RSS, robots, and sitemap output
- runtime APIs for catalog, scene descriptors, forms, search documents, chat, streaming previews, and health probes
- a `package.json` script surface so users can build or serve without Rust or Cargo

## Intent

This folder is not meant to become a pile of isolated starters.
It is meant to become a reusable web systems layer for Kain:

- static marketing sites
- portfolio and case-study sites
- immersive 3D storytelling shells
- chat-first product surfaces
- docs, onboarding, and searchable knowledge hubs
- operator dashboards and command-center shells
- actor-based local web servers and realtime dashboards
- hybrid sites that combine several of those modes in one deployable shell
