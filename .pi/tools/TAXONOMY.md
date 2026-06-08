# Kain Pi Tools — Taxonomy

**Last updated:** 2026-06-08

This document defines the naming conventions, domain structure, and architectural
rules for all pi agent tools in the Kain repo. Keeping this consistent is how we
scale to hundreds of tools without melting the LLM.

---

## 1. Domain Structure

Every tool belongs to exactly one **domain**. A domain is a group of related
operations that the LLM can reason about as a unit. Each domain registers
**one router tool** that dispatches to **multiple actions**.

```
Domain: kain_bazel
  └── Tool: kain_bazel
        ├── action: build
        ├── action: test
        ├── action: server
        ├── action: sync
        ├── action: binary_age
        └── action: freshness
```

### Current domains

| Tool name | Domain | File | Actions |
|---|---|---|---|
| `kain_stdlib` | Standard Library | `kain-tools.ts` | list_modules, get_symbols, search_symbols, get_details, get_source, list_keywords, get_keyword |
| `kain_bazel` | Bazel Build System | `kain-bazel-tools.ts` | build, test, server, sync, binary_age, freshness |
| `kain_lang` | Kain Language Operations | `kain-lang-tools.ts` | check, build, run, test, amalgamate, gpu_artifacts |
| `kain_native` | Native Binary | `kain-native-tools.ts` | build |
| `kain_sync` | Binary Sync | `kain-sync.ts` | binary |
| `z3` | Z3 Solver | `z3-tools.ts` | analyze, extract, check, prove, admin, regress |
| `tools` | Tool Discovery | `tools.ts` | list, search, which |
| `git` | Git Repository | `git-tools.ts` | status, diff, log, recent |

### Adding a new domain

1. Add an entry to `.pi/tools/manifest.json` under `domains[]`
2. Create `X:/.pi/extensions/<domain-file>.ts`
3. In the extension file, register one router tool using `pi.registerTool()`
4. Add docs to this TAXONOMY.md

---

## 2. Naming Conventions

### Tool names (camelCase with domain prefix)

```
<domain>_<name>

kain_stdlib       ✓  (domain: kain, name: stdlib)
kain_bazel        ✓  (domain: kain, name: bazel)
kain_lang         ✓  (domain: kain, name: lang)
tools             ✓  (special: meta-domain)
```

**Rules:**
- Use underscores, not hyphens
- Prefix with domain for all Kain-specific tools (`kain_*`)
- The `tools` meta-tool is the only exception (no prefix)
- Keep names under 30 characters

### Action names (snake_case, imperative)

```
list_modules      ✓  (verb + noun)
search_symbols    ✓  (verb + noun)
get_details       ✓  (verb + noun)
server            ✓  (noun — status checks)
build             ✓  (verb — imperative)
```

**Rules:**
- Prefer `verb_noun` pattern: `search_symbols`, `list_modules`
- Single word is fine for common operations: `build`, `test`, `sync`
- Be consistent within a domain: don't mix `list_foo` and `fetch_bar`
- Keep under 25 characters

### File names (kebab-case domain extension)

```
kain-tools.ts       ✓  (<domain>-tools.ts)
kain-bazel-tools.ts ✓
tools.ts            ✓  (meta-tool, special case)
```

---

## 3. Schema: Manifest Entry

Every entry in `.pi/tools/manifest.json` must have:

```json
{
  "id": "kain_bazel",
  "name": "Kain Bazel",
  "file": "kain-bazel-tools.ts",
  "label": "Kain Bazel",
  "description": "Long-form description of what this domain does.",
  "promptSnippet": "One-liner for the LLM prompt snippet (max 120 chars)",
  "promptGuidelines": ["List of usage guidelines for the LLM."],
  "actions": {
    "action_id": {
      "label": "Human Label",
      "description": "What this action does (max 200 chars)"
    }
  }
}
```

---

## 4. File Layout

Each domain extension file should follow this structure:

```typescript
// === Imports ===
// === Manifest loader (reads manifest.json for description data) ===
// === Action dispatcher (executes subcommands) ===
// === Tool definition (single router tool) ===
// === Export (default function registers tools + commands) ===
```

---

## 5. Tool Count Budget

| Tier | Tools | Quality | Notes |
|---|---|---|---|
| 🟢 | 1–6 router tools | Excellent |  |
| 🟡 | 7–10 router tools | Manageable | We're here: 10 tools (at hard ceiling) |
| 🟠 | 11–15 router tools | Strained | Need aggressive dedup |
| 🔴 | 16+ router tools | Degraded | Split into more domains or merge |

Each router tool can have unlimited actions (subcommands) — the LLM never sees
these as separate tools, only as parameters. This is how we scale to hundreds
of operations.

**Hard ceiling: 10 router tools.** Beyond that, start merging domains or
introducing a second tier of meta-tools. We are AT the ceiling with the
addition of `z3` — any future tool must come from adding actions to an
existing domain, merging two domains, or introducing a meta-router
(e.g. `lang` for all language ops).
