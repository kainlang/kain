# MarkScript Guide

> **Prose-native bytecode VM for pi-squared.**
> Markdown headings → domains. Blockquotes → intents. Tables → data matrices.

---

## 1. What is MarkScript?

MarkScript is a **lightweight bytecode VM** whose source format is Markdown. Every markdown element maps to a VM construct:

| Markdown | VM Construct | Description |
|----------|-------------|-------------|
| `# Heading` | Domain | Named execution context |
| `## Subheading` | Subdomain | Scoped namespace within a domain |
| `> quote` | Intent | Side-effect declaration (run, build, gen, shell) |
| Code block | Script body | Space-delimited bytecode tokens |
| Table | Data matrix | Typed key-value rows, compile-time encoded |
| `---` | Separator | Bytecode segment boundary |

The VM executes scripts from code blocks, dispatches intents through its interrupt vector table (IVT), and resolves variables across domain scopes.

---

## 2. Using mks.exe

The MarkScript VM binary lives at the project root as `mks.exe`.

### Basic Commands

```bash
# Run a markscript file end-to-end
mks run pi-squared.md

# Run a specific script file
mks run scripts/build.md

# Generate code from tables + templates
mks gen schemas/provider.kn

# Show available domains and subdomains
mks dump pi-squared.md

# Lint a markscript file (validate IVT handlers, scope, syntax)
mks lint scripts/dev.md

# Compile to standalone bytecode (.mbc)
mks compile pi-squared.md -o pi-squared.mbc
```

### Common Options

| Flag | Effect |
|------|--------|
| `--verbose` | Show bytecode trace during execution |
| `--dry-run` | Parse and validate without executing |
| `--domain NAME` | Start execution at a specific domain |
| `--sub NAME` | Start execution at a subdomain within domain |
| `--debug` | Enter interactive debugger (step, inspect, continue) |

---

## 3. Available Scripts

| Script | Domain | Purpose |
|--------|--------|---------|
| `scripts/build.md` | `build` | Check → compile → link pi-squared | 
| `scripts/test.md` | `test` | Run Kain test suite with compiletest directives |
| `scripts/dev.md` | `dev` | Watch mode: recheck on file change |
| `scripts/clean.md` | `clean` | Remove build artifacts, reset state |
| `scripts/help.md` | `help` | Print available commands and flags |

---

## 4. Configuration via Tables

MarkScript tables encode typed configuration. The main config file is `config.md`.

```markdown
## BuildConfig

| Key | Value | Type |
|-----|-------|------|
| target | llvm | string |
| optimize | true | bool |
| debug | false | bool |
| output_dir | ./out | path |
| entry | src/main.kn | path |
```

Tables support three types inferred from the `Type` column: `string`, `bool`, `path`, `int`, `float`.

Scripts reference config values with `config.build.target` syntax in bytecode:

```markscript
mks_config("build.target")
mks_config("build.optimize")
```

---

## 5. Code Generation with `mks gen`

`mks gen` transforms data tables into Kain source files using built-in templates.

### Template Mapping

```bash
mks gen schemas/provider.kn
```

This reads `schemas/provider.kn` (a Kain struct template with `{{placeholder}}` markers), resolves variables from a `## GenConfig` table in the same file or from `config.md`, and writes the expanded output to `src/providers/generated/`.

### GenConfig Table

```markdown
## GenConfig

| Key | Value |
|-----|-------|
| source | schemas/provider.kn |
| output | src/providers/generated/provider_types.kn |
| template | struct_provider |
| vars | providers: [openai, anthropic, google, mistral] |
```

### Variable Interpolation

Placeholders in templates use `{{path.to.value}}` syntax:

```kain
struct {{provider_name}}Provider:
    api_key: String
    base_url: String
    model: String
```

`mks gen` with `vars.providers = [openai, anthropic]` produces two structs:

```kain
struct OpenAIProvider:
    api_key: String
    base_url: String
    model: String

struct AnthropicProvider:
    api_key: String
    base_url: String
    model: String
```

---

## 6. IVT (Interrupt Vector Table)

The VM dispatches intents through a fixed IVT with 16 slots:

| Handler | Intent | Purpose |
|---------|--------|---------|
| 0 | `init` | Initialize VM state |
| 1 | `domain` | Enter a domain |
| 2 | `subdomain` | Enter a subdomain |
| 3 | `script` | Execute a code block |
| 4 | `run` | Execute a shell command |
| 5 | `build` | Invoke the Kain build |
| 6 | `gen` | Run code generation |
| 7 | `check` | Run Kain typecheck |
| 8 | `shell` | Execute inline shell script |
| 9–15 | reserved | Future expansion |

---

## 7. Example: `scripts/build.md`

```markdown
# build

> build

```markscript
mks_config("build.target")
mks_config("build.optimize")
mks_config("build.output_dir")
cmd("kain build --target llvm src/main.kn")
```

> check

```markscript
cmd("kain check src/")
```

This script first reads build configuration, then runs typecheck (`> check`) followed by native compilation (`> build`).
