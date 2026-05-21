---
name: lang-ui
description: Use when authoring Kain-owned UI code, including components, layout flows, native-ui surfaces, blade-facing desktop experiences, and UI-side state composition without taking ownership of host/runtime internals.
---

# Lang UI

Use this skill when the job is "write the UI in Kain."

## Fast Loop

```powershell
rg -n "component |render <|surface native_ui|ui_" blades benchmark smoketest
kain check <entry.kn> --target llvm
kain build native-ui <entry.kn> --bundle-only
kain run <blade-or-entry>
```

## Kain Pattern

```kn
component StatusPanel():
    render <panel title="Status" />

world Dashboard:
    state health: Int = 100
    surface native_ui => StatusPanel
```

## What To Do

- Keep the authored lane declarative and Kain-owned.
- Make UI state readable in Kain instead of burying behavior in host glue.
- Use blade-level acceptance loops when visual behavior matters, not just syntax-only snippets.

## Hand Off When

- Use `package-kaintana` when the framework surface itself is changing.
- Use `runtime-stdlib` when the issue is in the runtime-backed UI host/session layer.
- Use `runtime-core` when the native ABI/substrate is the real blocker.
- Co-trigger `lang-gpu` when the UI surface is tightly coupled to graphics or shader work.
