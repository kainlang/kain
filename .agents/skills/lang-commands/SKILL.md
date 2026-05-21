---
name: lang-commands
description: Use when the task is about using Kain's user-facing command surface from the application side, such as choosing `kain`, `kn`, or `blade` workflows for a project, wiring authored command usage into docs or examples, or guiding project-local command execution without changing CLI router internals.
---

# Lang Commands

## Overview

This skill owns how authored projects use the command surface. Use it when the request is "how should this Kain project be run, built, checked, tested, or scaffolded from the user side?" rather than "change how the CLI itself works."

## Start Here

- Prefer the simplest user-facing command flow that matches the project shape.
- Keep project-local command guidance next to the blade, benchmark, or test lane being touched.
- If a request is really about build plumbing, Bazel drift, launcher binaries, or command registry internals, route it immediately instead of mixing concerns.

## Routing

- Stay here for user-facing command selection and project-local usage patterns.
- Switch to `tool-build-system` when the task changes `kain`/`kn`/`blade` internals, command manifests, router behavior, build adapters, launchers, generated BUILD state, or "how the repo builds Kain itself."
- Co-trigger `lang-blades` when the command flow is specific to a blade workspace.
- Co-trigger `test-harness`, `test-bench`, or `test-attrition` when the command work is really about using those proof lanes.

## Usage Rules

- Treat commands as operator surfaces, not ownership sinks. The skill should tell an agent how to use the command surface cleanly, not invite it to patch the command platform.
- Prefer documented, reproducible flows over one-off shell folklore.
- If the user asks for a new command or a changed CLI route, that is not `lang-commands`; hand it to `tool-build-system`.
