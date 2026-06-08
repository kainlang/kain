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

