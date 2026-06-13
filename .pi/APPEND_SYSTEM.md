# Advisor Protocol — When in Doubt, Call the Advisor

**Call `advisor()` early and often.** This isn't a last resort — it's your first instinct when:

- The task is **complex**, multi-step, or touches unfamiliar parts of the codebase
- You're **stuck** — same error twice, approach not converging, results that don't smell right
- You're about to **commit to an approach** — let the advisor sanity-check it first
- You think the work is **done** — call advisor before declaring victory to catch what you missed
- You're considering a **change of direction** — surface the conflict and let the advisor break the tie
- You have **conflicting evidence** — you found X, someone suggested Y, don't guess which is right

**Advisor sessions are free.** They take time but save hours of spinning. Use them.

### Verification After Complex Work

After any complex edit in **`/crates`** or **`/runtime`**, call the advisor to:

- **Check it's not a one-off fix** — is the change structural and correct, or a hardcoded patch that will rot?
- **Assess edge cases** — what breaks? What's the failure mode? Are the invariants still solid?
- **Confirm approach fits the architecture** — does this belong here, or is there a cleaner place?

### Kain Authoring — Sanity Check

When writing or debugging Kain `.kn` files and you hit a complex issue — weird types, ownership errors, actor wiring not working, shader compilation failing — call the advisor to verify you're writing **correct, idiomatic Kain** before digging deeper or changing the compiler.

### KAINC Self-Host Compiler — KN.MD Update Protocol

**After ANY work on `blades/kain/src/` (the kainc self-host compiler), update `blades/kain/KN.MD` BEFORE declaring done.** This is non-negotiable. The document is the single source of truth that maps kainc's implementation state against the Rust bootstrap (`crates/`). Stale KN.MD causes downstream agents to operate on wrong assumptions.

To update KN.MD efficiently:
- **§1 (State Dashboard):** Update the Real% and Verdict columns for any subsystem you touched
- **§4 (Stream Status):** Update PASS/PARTIAL/FAIL verdicts for tasks you completed
- **§5 (Known Blockers):** Add/remove blockers as they're found/fixed
- **§9 (File Manifest):** Update line counts and real% for modified files
- **§3 (Decision Ladder):** If you implemented a new construct, update its typecheck+codegen status

Use `kain_lang check` results as ground truth for what actually compiles. Use `oracle` for any binary verification. Do not guess percentages — count actual functions, expression kinds, or code paths.
