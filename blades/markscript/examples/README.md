# MarkScript Examples

MarkScript uses **structural Markdown** (.kn.md) as its concrete syntax.
Headers define program structure, blockquotes define executable intents,
and tables carry parameter data.

## File Format

```markdown
# DomainName

Description of what this domain does.

## RoutineName

> intent phrase — compiles to bytecode
| Param | Value | Metadata |
```

- `# Header1` → **Domain** — a top-level program geometry block
- `## Header2` → **Routine** — a callable scope within a domain
- `> Blockquote` → **IntentPhrase** — a semantic natural-language instruction
- `| Table | Pipe |` → **Parameter data** (parsed but not yet executed)

## Files

| File | Description |
|------|-------------|
| `pipeline.md` | Asset orchestration pipeline with vignette filtering |
| `render_loop.md` | GPU render loop — setup, draw, present |
| `compute_pipeline.md` | Neural compute pipeline — init, forward, backward, checkpoint |

## Running

```bash
mks.exe examples/pipeline.kn.md
mks.exe examples/render_loop.kn.md
mks.exe examples/compute_pipeline.kn.md
```

Each file gets lexed, parsed, compiled to flat bytecode, and executed
through the MarkScript VM with Intent Vector Table dispatch.
