---
name: lang-commands
description: Use when the task is about how to use Kain's command surface from the authored side, including file loops, blade loops, import loops, check/test flows, and project-local operator guidance without changing CLI internals.
---

# Lang Commands

Use this skill when the question is "which Kain commands should I run for this authored workflow?"

## Fast Loop

```powershell
kain check <file-or-dir> --target llvm
kain run <file-or-blade>
kain build <file.kn> --target llvm -o <output.exe>
kain blades build . --json
blade run <blade> --target auto -- <args>
kain test <file-or-dir>
kain import-c <input> -o <output.kn>
kain import-rust <input> -o <output.kn>
kain gpu-artifacts <shader.kn> --output <dir>
```

## Tiny Probe File

```kn
fn main() -> Int:
    println("command probe")
    return 0
```

Use that sort of file for quick check/run/build loops before scaling up.

## What To Do

- Use `kain check` first when you want fast syntax/type feedback.
- Use `kain run` for the quickest authored behavior loop.
- Use `kain build` when you need a real artifact.
- Use `kain blades ...` or `blade ...` when the unit of work is a blade rather than a loose file.
- Use the import commands when the fastest path is to pull foreign code into a Kain starting point instead of hand-porting from zero.

## Hand Off When

- Use `tool-build-system` when the task changes command routing, launcher behavior, Bazel/build plumbing, generated BUILD state, or how Kain itself is built.
- Co-trigger `lang-blades`, `test-harness`, `test-bench`, or `test-attrition` when the command surface is really in service of those lanes.
