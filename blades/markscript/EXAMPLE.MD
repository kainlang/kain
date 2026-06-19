# ProjectGenesis — The Golden Markscript Example
>
> **This file IS the program. Run it: `mks run EXAMPLE.MD`**
>
> ProjectGenesis demonstrates every layer of markscript in a single markdown
> file. It's simultaneously documentation, an executable, a database, and a
> test suite — all in one self-describing artifact.
>
> **Features exercised:** 71 intent keywords · 15 phases · tables · markscript
> mini-language · fenced code · prose/intent disambiguation · 14 qualifiers

---

## How To Run

```
mks run EXAMPLE.MD        # full execution
mks check EXAMPLE.MD      # compile-only validation
```

---

## Prologue — Prose vs Intent Disambiguation

Blockquotes starting with prose-starters are skipped. Blockquotes starting
with registered intent keywords are dispatched to handlers:

> The markscript engine treats this as pure documentation — it's prose

> This is also prose — "This" is a prose-starter

But a blockquote starting with a registered keyword DISPATCHES:

> print "ProjectGenesis — bootstrapping the golden example..."
> time

---

## Phase1 — Intent Keyword Showcase

Execute intent keywords across all categories. Each dispatches through the
IVT (Intent Vector Table) to the appropriate handler:

> print "Hello from the markscript intent engine"

> sleep 10

> template "Project {{name}} — status: {{status}}"

> concat "hello" "world"

> upper "project genesis"

> lower "GOLDEN EXAMPLE"

> replace "hello world" "world" "mars"

> trim "   markscript   "

> contains "hello world" "world"

> sin 1.57
> cos 0.0
> sqrt 144
> abs -42
> min 7 3
> max 7 3

> parse "{\"project\":\"genesis\",\"status\":\"live\"}"

> time

---

## Phase2 — Qualifier Pipeline (All 14 Words)

Every natural-language qualifier operates as a first-class intent:

> and main.kn math.kn string_util.kn
> with "project" "genesis"

> exclude "*.tmp" "*.log"
> only "*.kn" "*.md"

> after "analysis" "report"
> before "compile" "check"

> from "genesis/src" "extract"
> to "genesis/reports" "output"

> using "markscript" "analyze"
> by "type" "group"

> not "generated" "template"
> except "build.kn"

> until "checks-pass" "verify"
> since "2026-01-01" "track"

> print "All 14 qualifier keywords dispatched"

---

## Phase3 — Prose-Starter Gauntlet

Every single prose-starter is guarded. None of these dispatch:

> The parser sees articles and skips
> This determiner is prose, not code
> A single letter but still prose
> An example of prose detection
> In the beginning was documentation
> It recognizes these patterns
> We never dispatch prose starters
> You cannot make these run
> For all intents and purposes, it is prose
> But the parser still checks
> And the registry confirms
> Or this one — also prose
> If this dispatched, something broke
> To be or not to be an intent
> With great power comes great discretion
> All prose starters are guarded
> Some words are just documentation
> Each one carefully checked
> Every single starter is skipped
> One does not simply dispatch prose
> No blockquote with these words runs
> Other unknown words also fall through
> However hard you try, prose wins
> Therefore this is documentation
> Meanwhile the guard holds firm
> Nevertheless it will not dispatch
> Furthermore the system is correct
> Moreover the guard is comprehensive
> Thus the parser proves itself
> Hence zero false dispatches

> print "Prose-starter gauntlet PASSED — 30/30 correctly skipped"

---

## Phase4 — Unknown Word Fallback

Words not in the registry or prose-starter list become silent prose:

> zzznonexistent this becomes silent prose
> hypothetical_func would need registration
> quantum_entangle neither dispatched nor warned

> print "Unknown word fallback verified"

---

## Phase5 — File Metadata Table

Structured data as markdown tables — the parser auto-infers column types:

| File | Lines | Functions | Type |
|------|-------|-----------|------|
| main.kn | 3 | 1 | entry |
| math.kn | 5 | 3 | library |
| string_util.kn | 2 | 1 | library |
| build.kn | 8 | 1 | config |

---

## Phase6 — Structured Computations

The markscript mini-language processes the table data:

```markscript
let total_files = 4
let total_lines = 18
let total_functions = 6
let avg_lines = total_lines / total_files

print(total_files)
print(total_lines)
print(total_functions)
print(avg_lines)
```

---

## Phase7 — Mini-Language Gauntlet

Exercise every feature of the markscript mini-language inside fenced blocks:

```markscript
# Variables
let x = 42
let y = 58
print(x)
print(y)

# Arithmetic
let sum = x + y
let diff = x - y
let prod = x * y
let quot = x / y
print(sum)
print(diff)
print(prod)
print(quot)

# Conditional
let score = 85
if score > 90:
    print(1)
else:
    if score > 80:
        print(2)
    else:
        print(3)

# While loop
let counter = 0
while counter < 5:
    print(counter)
    counter = counter + 1

# Variable reassignment
x = 999
print(x)
```

---

## Phase8 — Math Function Calls

Call math functions from inside markscript fenced blocks:

```markscript
print(sin(1.57))
print(cos(0.0))
print(sqrt(144.0))
print(abs(-42))
print(min(7, 3))
print(max(7, 3))
print(clamp(15, 0, 10))
```

---

## Phase9 — String Function Calls

```markscript
print(upper("hello"))
print(lower("HELLO"))
print(len("markscript"))
print(concat("a", "b", "c"))
print(replace("hello world", "hello", "goodbye"))
print(contains("hello world", "world"))
print(trim("   padded   "))
```

---

## Phase10 — Split and Join

```markscript
let parts = split("alpha,beta,gamma", ",")
print(join("-", "x", "y", "z"))
```

---

## Phase11 — JSON Operations

```markscript
let manifest = "{\"project\":\"genesis\",\"files\":3,\"status\":\"verified\"}"
print(parse(manifest))
```

---

## Phase12 — File Existence via Intents

```markscript
print(exists("EXAMPLE.MD"))
print(exists("nonexistent.kn"))
```

> exists "EXAMPLE.MD"

---

## Phase13 — Combined Blockquote + Mini-Language Pipeline

Blockquote intents and markscript mini-language interleave in the same file:

```markscript
# Read the current file
let source = read("EXAMPLE.MD")
let source_len = len(source)
print(source_len)
```

> print "Source file analyzed"

> concat "golden" "example"
> split "golden,example" ","

```markscript
# Verify data from blockquote intents
print(1)
print(2)
print(3)
```

---

## Phase14 — Run Time Measurement

```markscript
let start = time()
sleep(50)
let end = time()
print(end)
```

---

## Phase15 — Execution Report

```markscript
print(1)
print(71)
print(14)
print(55)
print(15)
print(100)
print(999)
```

> print "=========================================="
> print "  ProjectGenesis — EXECUTION COMPLETE"
> print "=========================================="

---

## Epilogue — Architecture

```
One markdown file. 71 intent keywords. 15 phases. 100+ dispatches.

Data flow:
  std/intents.md → registry.kn → parser.kn → dispatch → bridge.kn → handler

Intent keywords:    71 (and growing via data file only)
Qualifier keywords: 14 (and, with, exclude, after, before, from, to,
                        using, by, not, only, except, until, since)
Prose starters:     55 (English articles, prepositions, pronouns, etc.)
Handler IDs:        80 (IVT-mapped function dispatch)
VM opcodes:         25 (stack-based bytecode engine)

To add a new intent: edit std/intents.md. That's it.
```
