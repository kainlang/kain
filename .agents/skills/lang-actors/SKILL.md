---
name: lang-actors
description: Use when authoring Kain actor code, including actor declarations, `spawn`, `send`, `ask`, supervision shape, actor-driven blade code, and message-oriented demos, without taking ownership of scheduler or mailbox internals.
---

# Lang Actors

Use this skill when the task is about how actors should be written in Kain.

## Fast Loop

```powershell
rg -n "actor |spawn |send |ask\\(" blades benchmark smoketest
kain check <entry.kn> --target llvm
kain run <entry.kn-or-blade>
```

## Kain Pattern

```kn
actor Counter:
    state value: Int = 0

    on Add(reply_to: P, amount: Int):
        self.value = self.value + amount
        send reply_to.Reply(value = self.value)

fn main() -> Int:
    let counter = spawn Counter()
    let total = ask(counter, "Add", 4)
    return total - 4
```

## What To Do

- Prefer named messages and typed-looking request/reply flows over anonymous fire-and-forget noise.
- Keep long-lived or message-driven behavior inside actors instead of rebuilding it with shared mutable globals.
- Use actors to prove something real: ask/reply, supervision, pressure, or state propagation.

## Hand Off When

- Use `lang-systems` when actors are part of backpressure, effects, ownership, raw memory, zero-copy, scheduler telemetry, or unsafe systems authoring.
- Use `bootstrap-actors` when actor semantics, lowering, or reflection truth changes.
- Use `runtime-core` when the issue is scheduler, mailbox, ABI glue, or crash behavior under native execution.
- Co-trigger `lang-semantics` when the actor lane is fused with `world`, `entangle`, `teleport`, or `pulse`.
