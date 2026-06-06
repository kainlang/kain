# Pi Tools for the Kain Repo

This directory contains the canonical tool registry and documentation
for all pi agent extensions in the Kain repo.

## Quick Start

Want to add a new capability? Here's the workflow:

### 1. Add to manifest

Edit `manifest.json` and add a new domain entry or add actions to an
existing domain. The manifest is the source of truth — the brain tool
(`tools.ts`) reads it to discover what exists.

### 2. Create or update the extension file

Each domain in the manifest maps to a `.ts` file under `.pi/extensions/`.
If your domain is new, create the file. If it's an existing domain,
add the action handler.

Each file registers **one router tool** that dispatches to multiple actions.
See existing files for the pattern.

### 3. Register in pi

```
# If pi is running
/reload

# If pi isn't running
pi
```

### 4. Test

Use the brain tool to verify it shows up:

→ `tools action:'list'`
→ `tools action:'search' query:'your tool'`

## Architecture Decision Records

### Why routers instead of flat tools?

Each tool in pi's system prompt costs ~150 tokens (name + description +
parameter schema). With flat tools, 30 operations = 30 tools = ~4500 tokens
in the prompt. With routers, 30 operations = 6 domains = 6 tools = ~900 tokens.

More importantly, the LLM is much better at selecting among 6 domains than
among 30 individual tools. Selection errors drop sharply.

### Why a manifest?

The manifest is the single source of truth for *what exists and what it does*.
The code files are the source of truth for *how it executes*. This separation:

- Makes it easy to audit available capabilities
- Enables the brain tool to provide discovery without parsing code
- Makes adding a new operation a data entry task + implementation task
- Keeps naming consistent (one place to enforce conventions)

## Files

| Path | Purpose |
|---|---|
| `manifest.json` | Canonical tool registry — all domains, actions, descriptions |
| `TAXONOMY.md` | Naming conventions, domain rules, tool count budget |
| `README.md` | This file |
| `../extensions/tools.ts` | Brain tool: discovery + search + which-tool-to-use |
| `../extensions/kain-tools.ts` | Kain stdlib router (8 actions) |
| `../extensions/kain-bazel-tools.ts` | Bazel build system router (6 actions) |
| `../extensions/kain-lang-tools.ts` | Kain language operations router (6 actions) |

## Tools

See `manifest.json` for the complete, authoritative tool listing.
