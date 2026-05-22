---
name: wildcard-justwritebro
description: >-
  Use when Codex should author or repair Kain `.kn` code in a fast
  intuition-first mode instead of scanning large parts of the repo for
  examples. This is the wildcard anti-scavenger-hunt lane: load the core Kain
  authoring skills (`lang-semantics`, `lang-stdlib`, `lang-projects`,
  `lang-gpu`, and `lang-systems`), optionally add `lang-interop` or
  `lang-translation` only when the task explicitly needs them, then start
  writing immediately from first principles. Best for greenfield Kain files,
  speculative prototypes, creative semantic demos, and bold first passes where
  originality and speed matter more than matching existing repo patterns. Do
  not use it for compiler/runtime/bootstrap changes or when the user explicitly
  asks for repo-conforming integration research.
---

# Wildcard Just Write Bro

## Overview

Load the main Kain authoring field manuals, then commit to writing. Use this skill to stop the usual repo-wide example hunt and produce bold Kain from first principles while still validating the result.

## Required Loadout

Read these skills first and treat them as the whole starting context:

- `.agents/skills/lang-semantics/SKILL.md`
- `.agents/skills/lang-stdlib/SKILL.md`
- `.agents/skills/lang-projects/SKILL.md`
- `.agents/skills/lang-gpu/SKILL.md`
- `.agents/skills/lang-systems/SKILL.md`

Read one extra skill only when the task obviously requires it:

- `lang-interop` for C/native/foreign boundary work
- `lang-translation` for ports from Rust, C, C++, JS, TS, or Python into Kain

After that, stop loading repo context unless a concrete blocker forces a surgical lookup.

## Wildcard Contract

- Start authoring after the core skill read. Do not tour `blades/`, `benchmark/`, `smoketest/`, `library_of_kain/`, `ARCHITECTURE.md`, `MEMORY.md`, or broad stdlib maps just to get more examples.
- Prefer first-principles Kain. Invent the shape that best fits the problem instead of imitating the nearest existing file.
- Use strong Kain constructs when they fit: `world`, `entangle`, `patch`, `law`, `converge`, `orchestrate`, `actor`, `pulse`, `teleport`, `collapse`, `observe`, `decay`, shaders, raw memory lanes, and stdlib surfaces.
- Preserve ambition. The point of this mode is to let agents write surprising, high-ceiling Kain instead of collapsing to safe repo-pattern cargo cult.
- Keep extra repo reads exceptional, tiny, and justified by a real blocker.

## Write Loop

1. Infer the smallest believable contract from the user request and the target file or project.
2. Start writing Kain immediately.
3. Validate with the narrowest useful command, usually `kain check <entry.kn> --target llvm`, `kain run <entry.kn-or-blade> --target llvm`, or the closest project-local check loop.
4. Repair errors by iterating on the authored code first.
5. Escalate to a surgical repo lookup only if the error proves a missing exact symbol, syntax edge, or ownership boundary.

## Allowed Escalations

Break the no-scavenger rule only for one of these reasons:

- The user explicitly asks to match or integrate with an existing repo surface.
- The task crosses into compiler, runtime, or bootstrap ownership.
- Validation fails because an exact public symbol, file path, or package boundary is unknown.
- The task depends on a specific existing module, blade, or bridge the user already named.

When escalating, read only the minimum file or symbol needed, then return to writing.

## Anti-Patterns

- Do not begin with a repo-wide hunt for the best example.
- Do not read ten benchmark cases before drafting a simple `.kn` file.
- Do not flatten Kain into timid `fn` and `let` soup just because a nearby example did.
- Do not cite source anchors or internal compiler files unless the task genuinely becomes bootstrap or runtime work.
- Do not turn this wildcard lane into an excuse to skip validation.

## Output Standard

Deliver authored Kain that is:

- semantically bold
- locally coherent
- minimally validated
- not overfit to legacy repo patterns

If the first draft reveals a real compiler or runtime bug, keep the Kain design honest and hand the substrate issue to the owning bootstrap or runtime lane instead of sanding the idea down.
