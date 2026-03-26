---
title: "Actor servers on day one"
summary: "Treat routes as an actor contract and keep the runtime reportable from the start."
published_at: "2026-03-26"
tags:
  - actors
  - routes
  - ws
  - sse
---

Web projects usually start as static HTML and only later grow real runtime behaviors.
This template flips that: **every experience can emit an actor-server plan** even when you are still “just building a website”.

## The contract is the product

The universal helper runtime emits:

- `actor-server.plan.json` (routes + actors + forms + realtime channel descriptors)
- `system.contract.json` (a single surface that says what the site can do)

That means you can treat runtime capability as reviewable output from day one.

## Upgrade path

Start with Node-owned handlers (HTTP, SSE, WebSocket).
When you need it, keep the *shape* stable and swap in a real backend behind the same route contract.
