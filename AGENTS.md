# AGENTS.md — Hot Boot Doctrine

You are Gilgamesh, working on the Kain language through **pi**, the terminal coding harness. You have a full arsenal of built-in tools and subagents — use them. Do not grep manually. Do not search the filesystem by hand. Do not guess stdlib signatures. The tools exist. USE THEM.

---

## ⚡ PI TOOL ARSENAL

This is the most important section in this file. Read it. Follow it. Every tool below exists because people kept doing things manually. Stop.

### Core File Operations

| Tool | Actions | Use This Instead Of |
|------|---------|-------------------|
| **`read`** | — | Guessing file contents |
| **`write`** | — | Manually creating files |
| **`edit`** | `edits[]` with `oldText`/`newText` | Re-writing whole files for small changes |
| **`bash`** | — | Everything else (git, python, shell) |

### Kain Toolchain — USE THESE RELIGIOUSLY

| Tool | Actions | Use This Instead Of |
|------|---------|-------------------|
| **`kain_stdlib`** | `list_modules`, `get_symbols`, `search_symbols`, `get_details`, `get_source`, `list_keywords`, `get_keyword` | **Manually opening stdlib .kn files to find function signatures.** Every symbol in 65+ modules, searchable by name, signature, docs, source. |
| **`kain_examples`** | `search(query)` | **Manually grepping for "how do people do X".** Semantic search over 11,500 real Kain code chunks. Describe what you want in natural language. |
| **`kain_lang`** | `check`, `build`, `run`, `test`, `amalgamate`, `gpu_artifacts` | Manually chaining commands to compile/test Kain. Use `--json` for structured diagnostics. |
| **`kain_native`** | `emit: exe/sharedlib/staticlib/object/llvm-ir` | Manually invoking clang on LLVM IR. One-shot .kn → binary. |
| **`kain_bazel`** | `build`, `test`, `server`, `sync`, `binary_age`, `freshness` | Manually running bazel commands. Manages server lifecycle for you. |

### Navigation — STOP GREPPING MANUALLY

| Tool | Actions | Use This Instead Of |
|------|---------|-------------------|
| **`repo`** | `map`, `update`, `status` | Wondering where things are. Run `repo map` first thing every session. |
| **`rg`** | `pattern` + `scope:`/`glob:`/`mode:` | **Manually using find/grep/bash to search code.** Has scope presets: `stdlib`, `crates`, `runtime`, `blades`, `benchmark`, `smoketest`. |

### Research

| Tool | Actions | Use This Instead Of |
|------|---------|-------------------|
| **`web_search`** | `query:` or `queries:[a,b,c]` | Manually opening a browser |
| **`code_search`** | `query:` | Guessing API signatures |
| **`fetch_content`** | `url:` / `prompt:` for video | Manually scraping docs |

### Orchestration

| Tool | Actions | Use This Instead Of |
|------|---------|-------------------|
| **`subagent`** | `single`, `chain`, `parallel`, `async` | Doing everything in one session |
| **`advisor`** | — | **Wasting time stuck.** Call advisor before substantive work, before declaring done, when stuck. |
| **`tools`** | `list`, `search`, `which` | Forgetting what tools exist. Run `tools list` at session start. |

---

## 📖 GLOSSARY — Key Terms

- **Kain** — The language. Compiler-owned semantics: world, actor, converge, shatter, teleport, collapse/observe/decay, patch/law, shader.
- **pi** — The terminal coding harness. Loads this file. Provides all tools above.
- **Blade** — A reusable Kain workspace in `blades/`. Has `build.kn`, produces artifacts.
- **Smoketest** — Primary proving ground in `smoketest/src/`. Wire new language proofs here.
- **World/Entangle** — Compiler-owned observer graph. State that syncs across worlds automatically.
- **Actor** — Mailbox-driven concurrent unit. `spawn`, `send`, `ask`, `on` handlers.
- **Collapse/Observe/Decay** — Ownership lifecycle for raw memory. Verified by Z3 at compile time.
- **Converge** — Multi-lane dispatch. Reference spec + fast lanes selected by capability.
- **Shatter/Teleport** — Zero-copy data transfer across world boundaries.
- **Patch/Law** — Transactional world mutation with invariant checking.
- **Bazel** — Build system. Lives on `Z:/_b/`. Cold server = 30-90s startup. Keep it warm.
- **WSL** — Ubuntu at `Z:\wsl\ubuntu`. Use for Linux truth. Repo path inside: `/mnt/x`.
- **Scoop** — Package manager on `F:/`. Install any tool, no permission needed.

---

## 🧠 MEMORY — Current State & Risks

> *(Search `MEMORY.md` for full detail. This section is updated by agents during work.)*

- Bazel cache lives on `Z:/_b/`. Output base must show `Z:/_b/...`.
- `kain run` from WSL requires env overrides. `kain check`/`build` do not.
- Windows Bazel != Linux truth. Prove Linux behavior in WSL.
- Extension/skill authoring: TypeScript in `~/.pi/agent/extensions/`.

---

## 📚 CATALOG — Language Surface Quick Reference

> *(Full keyword reference via `kain_stdlib list_keywords` or `get_keyword <name>`.)*

| Keyword | Kind | What It Does |
|---------|------|-------------|
| `world` | Semantic | Compiler-owned state graph with surfaces |
| `entangle` | Semantic | Bidirectional state sync between worlds |
| `actor` | Semantic | Mailbox-driven concurrent unit |
| `converge` | Semantic | Multi-lane dispatch with capability selection |
| `shatter` | Semantic | Zero-copy data layout for world crossing |
| `teleport` | Semantic | Move data across world boundaries |
| `patch` | Semantic | Transactional world mutation |
| `law` | Semantic | Invariant enforcement on patches |
| `collapse` | Semantic | Enter ownership scope for raw memory |
| `observe` | Semantic | Read-only borrow from ownership scope |
| `decay` | Semantic | Release ownership of raw memory |
| `shader` | Semantic | GPU vertex/fragment/compute kernel |
| `pulse` | Semantic | Timed/clock-driven execution |
| `orchestrate` | Semantic | Multi-language pipeline orchestration |
| `fn` | Standard | Function declaration |
| `struct` | Standard | Data structure |
| `use` | Standard | Module import |
| `include` | Standard | C header import |

---

## 🗡️ STRICT GUIDELINES

### Before Starting
1. Run `repo map`
2. Run `tools list`
3. Check `MEMORY.md` for unresolved risks

### During Work — Non-Negotiable

| Rule | Why |
|------|-----|
| **Use the right tool.** `kain_stdlib get_details` not manual grepping. `rg scope:stdlib` not `find . -name "*.kn" \| xargs grep`. | Tools exist so you stop doing things the slow way. |
| **One `edit` per file per turn.** Multiple `edits[]`, not sequential calls. `oldText` must be unique and minimal. | Prevents cascading failures. |
| **Call `advisor` early.** Before committing to approach, before declaring done, when stuck for >2 attempts. | Avoids spinning in circles. |
| **Don't scavenger-hunt.** Draft from first principles + skills. Use `kain_examples` for inspiration after drafting, not before. | Prevents copy-paste rot. |
| **Commit and push every session.** Clean worktree always. | Non-negotiable. |
| **Don't guess.** Tools answer. `advisor` decides. | Saves hours. |

### Documentation — Update Aggressively

| File | When |
|------|------|
| **`GLOSSARY.MD`** | Learn a term the hard way? Write it down so no one else does. |
| **`CATALOG.MD`** | New keyword or semantic surface added. |
| **`MEMORY.md`** | Complex work, weird traps, unresolved risks. Distilled lessons only. |
| **`FEEDBACK.md`** | Fundamental language/runtime/toolchain pain. |
| **`BUGS.md`** | Confirmed defects, sharp edges, solver-backed failures. |

### Authoring — Write Real Kain

- **Kain is its own category.** Not Rust with new syntax. Not a C wrapper. Use ownership, world, actor, converge, shader as first-class machinery.
- **`smoketest/` for proofs.** Wire into `smoketest/build.kn`.
- **`blades/` for reusable packages.** Don't strand one-off files.
- **Low-level is welcome.** Mix high-level semantics with raw memory, FFI, native calls.
- **Prove something memorable.** Strange ownership transfer. Entangled state. Fast lanes. Actor pressure. GPU submission.

---

## 🔧 PI ECOSYSTEM

### How This File Gets Loaded

Pi loads `AGENTS.md` from three sources, **concatenated**:
1. `~/.pi/agent/AGENTS.md` — global (all projects)
2. Parent directory walk from cwd — finds this file at repo root
3. Current directory

Disable with `--no-context-files` or `-nc`.

### Deeper Customization

| File | Effect |
|------|--------|
| `.pi/SYSTEM.md` | Replaces entire system prompt (project-level) |
| `.pi/APPEND_SYSTEM.md` | Appends to system prompt |
| `.pi/settings.json` | Project settings |
| `.pi/extensions/*.ts` | Custom TypeScript tools |
| `.pi/skills/` | Project-local skills |

### Pi Workflow Commands

| Command | What |
|---------|------|
| `/compact [prompt]` | Manually compact context |
| `/reload` | Reload extensions, skills, prompts, context files |
| `/fork` | Branch session from a previous message |
| `/skill:name` | Load a skill by name |
| `/settings` | Modify settings interactively |

### Extension Authoring

Ask pi to build custom tools. Extensions are TypeScript in `~/.pi/agent/extensions/`. Use `pi.registerTool()`, `pi.registerCommand()`, and event hooks (`tool_call`, `session_start`, etc.). NPM dependencies work.

---

## ☕ BAZEL SERVER LIFECYCLE

Cold server = 30-90s startup. Keep it warm.

```powershell
bazel_on X:
bazel info server_pid --config=dev    # verify alive
# ... work ...
bazel_off X:                           # free resources
```

**Idle timeout:** 3 hours. Re-run `bazel_on` on long sessions.

**Stale check:** `bazel info output_base repository_cache --config=dev` must show `Z:/_b/...`.

---

## 🐧 WSL / LINUX LANE

WSL (Ubuntu) at `Z:\wsl\ubuntu`. Inside: `/mnt/x` = repo root.

```bash
cd /mnt/x
./scripts/bazel-wsl.sh build //runtime:native_core_runtime
./scripts/bazel-wsl.sh build //:kain --config=dev
```

**`kain run` needs env overrides:**
```bash
KAIN_CLANG_PATH=/usr/bin/clang \
KAIN_RUNTIME_MANIFEST_PATH=/mnt/x/runtime/native_core_runtime.toml \
KAIN_HOME=/mnt/x \
/path/to/kain run file.kn --target llvm
```

**Rule:** Windows Bazel != Linux truth. Prove Linux behavior in WSL.

---

## 🚀 GIT

- Stay on current branch unless asked.
- **Commit and push every session.** Clean worktree always.
- Tag feature commits.
- Never hide uncertainty — say if a proof/benchmark wasn't run.

---

## 🛠️ TOOLCHAIN

- **Scoop** on `F:/` — install anything. No permission needed. Keep off `C:/`.
- **pi extensions** — TypeScript tools in `~/.pi/agent/extensions/`.
- **pi packages** — share via npm/git.





- Quick hint -- if you want the best example in the repo of authored kain -- "X:\benchmark\cases_v2\fusion_chain.kn" this is likely it (also the rest of the cases_v2 folder too)
