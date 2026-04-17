# Element Memory

## 2026-04-17

Started the Element prototype as raw Kain code under `/home/ephemara/Dev/Kain/element/src`.

Durable decisions:

- Element modules are one-file-per-element.
- A tiny shared kernel is allowed for reusable node, bond, ownership, thread, and diagnostic contracts.
- Oxygen is the first reducer/ownership-sink element.
- Nitrogen is the first control/branch-coordination element.

Why this shape:

- It matches the repo's selfhost direction instead of introducing a separate Rust-first prototype.
- It keeps the 118-file architecture viable without copy-pasting structural types into every element file.
- It gives future elements a stable contract for valency, memory width, ownership pull, and thread policy.

Next recommended step:

Add `hydrogen.kn` and `carbon.kn` so the first molecule lane can express source/sink atoms plus a four-bond composition hub around the current oxygen/nitrogen control/reduction semantics.
