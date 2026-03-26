---
title: "3D WebGL islands (Three.js)"
summary: "Embed real WebGL scenes as client islands while keeping the page shell manifest-driven."
published_at: "2026-03-26"
tags:
  - 3d
  - webgl
  - three
  - islands
---

3D pages are usually treated as special cases.
In this template, 3D is “just another section kind” with a **client-island mount point**.

## What ships

- A manifest-driven scene descriptor (so the authored intent is data).
- A bundled client runtime (Preact + Three.js) that hydrates only where needed.

## Why islands matter

You can keep the site mostly static (fast, cacheable, SEO-friendly), while still shipping a real interactive 3D viewport where it counts.
