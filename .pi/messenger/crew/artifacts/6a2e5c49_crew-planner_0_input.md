# Task for crew-planner

Create a task breakdown for implementing this request.

## Request

# KAINOS Research Mission

Research and design the ultimate non-von-Neumann operating system kernel — one that beats macOS, Linux, and Windows by abandoning POSIX/Unix assumptions entirely. This is a pure research task. NO code implementation. Scholars debate, synthesize, and write multi-doc research output.

## Output
All documents go to `X:\research\KAINOS\` (NOT reference/ — that's input only).

## Reference Material (READ THESE FIRST)
All 12 files are in `X:\research\KAINOS\reference\` — 543 KB of Kain semantic reference:

| File | Content |
|------|---------|
| `KEYWORDS.MD` | All 110 Kain keywords with semantics |
| `RULEBOOK.md` | The decision ladder — which construct for which problem |
| `semantics.md` | Deep Kain semantic surface reference |
| `systems.md` | Systems programming patterns in Kain |
| `SYSTEMS_PROGRAMMING.MD` | Low-level Kain: atomics, fences, raw memory, SIMD |
| `OWNERSHIP.MD` | Collapse/observe/decay ownership semantics |
| `ACTOR.MD` | Actor system: spawn, mailbox, supervision, scheduling |
| `MARKSCRIPT.MD` | Markscript bytecode VM specification |
| `keyword_crucible.kn` | 108/110 keywords exercised in one file |
| `classic_systems.kn` | Actors, atomics, SIMD, packed wire pressure |
| `CRUSHER.kn` | Heavy systems Kain demo |
| `metal.kn` | Bare-metal Kain patterns |

## Research Angles — Assign one scholar per angle:

### Scholar 1: The Death of POSIX — Why Unix Must Die
Research the historical evolution of operating systems. Why POSIX became a local maximum. What architectural assumptions from the 1970s still constrain us today (file descriptors, fork/exec, signal model, synchronous I/O, hierarchical filesystem as universal namespace). Research modern alternatives: Plan 9, Inferno, Singularity, Barrelfish, Redox, Fuchsia. What did they get right? What did they miss? Conclude: what assumptions must a post-Unix OS discard?

### Scholar 2: Non-von-Neumann Architecture — The Semantic Stack as ISA
Kain's compiler-owned semantic stack (world, entangle, patch, law, converge, orchestrate, axiom, pulse, shatter, teleport, resonate, actor, collapse/observe/decay) is essentially a new instruction set architecture — but at the compiler level, not the hardware level. Research: how does this change OS design? If the compiler owns state authority (world), state integrity (law/patch), dispatch (converge), and memory layout (shatter), what does the kernel even DO anymore? Compare to: capability-based systems (CHERI, seL4), tagged architectures (Symbolics Lisp Machines, Burroughs B5000), dataflow architectures. Map each Kain semantic construct to a kernel function it replaces or transforms.

### Scholar 3: Single Address Space — Everything Is One World
Research single-address-space operating systems (SASOS): Opal, Mungi, Nemesis, Singularity, Midori. How does Kain's `world` construct map to protection domains? How does `teleport` replace copy-based IPC? How does `shatter struct` enable zero-copy shared data across protection domains without MMU page table manipulation? How do `axiom` and `converge` handle capability checking at compile time instead of runtime? Contrast with: traditional MMU-based protection, Intel SGX/TDX enclaves, ARM TrustZone. Map out: what does a KainOS memory model look like without an MMU in the critical path?

### Scholar 4: Actor-Based Kernel — Every Thread Is a Mailbox
Research: if every kernel primitive is an actor (scheduler actor, memory actor, device actor, syscall actor, network actor), what does the kernel architecture become? How does Kain's `orchestrate` construct handle interrupt dispatch as a multi-stage pipeline (ISR → deferred procedure → actor message)? How does `converge` provide fast-path syscalls with capability-gated fallbacks? How does `pulse` replace timer interrupts? How does `resonate` replace poll/epoll/kqueue? Compare to: L4 microkernels, seL4, HelenOS, Akka-based systems, Erlang VM (BEAM). Map the full actor supervision tree for a KainOS kernel.

### Scholar 5: Formal Verification Surface — Prove the Kernel Correct
Kain's `law` construct enables compiler-witnessable invariants. `orchestrate` provides typed multi-stage pipelines with residency and transfer guarantees. Z3 and CBMC can prove bounded correctness. Research: what subset of kernel correctness can be proven at compile time vs runtime? How do `law` predicates map to seL4-style formal verification? How does `axiom` handle hardware capability assumptions with fallback? How does `patch` with epoch counters enable journaled kernel state that's always recoverable? Design the proof architecture: which kernel properties are `law` invariants, which are `converge verify random(N)` fuzz targets, which are Z3 proof packs, which are CBMC exhaustive proofs?

### Scholar 6: The I/O Stack — From Interrupt to Application Without Copying
Research: how does Kain eliminate the traditional I/O copy chain (device DMA → kernel buffer → syscall copy → user buffer)? How does `shatter struct` enable device drivers to write directly into application-visible SoA layouts? How does `teleport` enable zero-copy handoff from device actor to application world? How does `orchestrate` model the full I/O pipeline (interrupt → DMA → shatter buffer → teleport → application world → GPU dispatch)? Compare to: io_uring, DPDK, SPDK, RDMA, NVMe CMB, GPU direct storage. Design the KainOS I/O stack.

## Scholar Behavior
- READ the reference files in `X:\research\KAINOS\reference\` — all 12 of them — before debating.
- Use `web_search` for external research (historical OS papers, modern alternatives, hardware capabilities).
- Use `z3` to prove any mathematical claims that arise in debate.
- DM each other. Challenge claims. Cross-reference findings. Build on peers' work.
- Write structured documents in `X:\research\KAINOS\` with clear filenames.
- Use Kain semantic constructs as the lens for all analysis — this is a Kain-native OS design.

## Final Deliverable
A multi-document research corpus in `X:\research\KAINOS\`:
- `README.md` — Unified vision: what is KainOS and why does it beat everything?
- Per-scholar documents with their angle analysis
- `BIBLIOGRAPHY.md` — All cited papers, systems, and references

## Available Skills

Workers can load these skills on demand during task execution. When creating tasks, you may include a `skills` array with relevant skill names to help workers prioritize which to read.

  vera — Semantic code search, regex pattern search, and symbol lookup across a local repository. Returns ranked markdown codeblocks with file path, line range, content, and optional symbol info. Use `vera search` for conceptual/behavioral queries (how a feature works, where logic lives, exploring unfamiliar code). Use `vera grep` for exact strings, regex patterns, imports, and TODOs. Use `vera references` to trace callers/callees. Use rg only for bulk find-and-replace or files outside the index.


You must follow this sequence strictly:
1) Understand the request
2) Review relevant code/docs/reference resources
3) Produce sequential implementation steps
4) Produce a parallel task graph

Return output in this exact section order and headings:
## 1. PRD Understanding Summary
## 2. Relevant Code/Docs/Resources Reviewed
## 3. Sequential Implementation Steps
## 4. Parallelized Task Graph

In section 4, include both:
- markdown task breakdown
- a `tasks-json` fenced block with task objects containing title, description, dependsOn, and optionally skills (array of skill names from the Available Skills list that are relevant to the task).