# Mathematical Abstraction Patterns

Use this reference when the user has an idea in words and you need to turn it into solver-friendly structure.

## Extract The Right Variables

Model at least the variables that actually control the claim:

- state variables: counters, capacities, indices, ownership bits, scheduler state, cache levels
- environment variables: input size, timing windows, contention, hardware assumptions, failure rates
- cost variables: latency, throughput, instruction count, energy, memory traffic
- control variables: branch choices, strategy selectors, phase transitions, feature toggles

If the system is too large, slice the smallest seam that still determines success or failure.

## Common Claim Shapes

### Bounds

Use when the question is "can this ever overflow, underflow, exceed, or escape?"

- `0 <= x`
- `x < capacity`
- `used + growth + slack <= limit`

### Existence

Use when the question is "can a mechanism like this exist at all under these assumptions?"

- `exists state, control. constraints(state, control) and desired_effect(state, control)`

### Impossibility

Use when the user proposes something radical and you want to see whether the math kills it immediately.

- `constraints(state) -> not bad_state(state)`

### Equivalence

Use when comparing an alien implementation against a standard baseline.

- `impl_a(input, state) == impl_b(input, state)`

### Optimization

Use when the claim is about performance, not just correctness.

- maximize `throughput`
- minimize `latency + alpha * energy + beta * memory_traffic`
- enforce `candidate_cost <= baseline_cost - margin`

State the cost model explicitly. If the cost model is weak or guessed, say so.

### Reachability Or State Machines

Use when the question is "can the system ever enter this useful or catastrophic mode?"

- `Init(state0)`
- `Transition(state_i, state_i+1)`
- ask whether `Bad(state_n)` or `Goal(state_n)` is reachable

### Resource Accounting

Use when the idea depends on conservation, borrowing, or pressure balance.

- `resource_next = resource_now + produced - consumed`

## Translate Research Questions Into Math

- "Could this weird path beat the normal one?" -> define a baseline cost, candidate cost, and the constraints required for `candidate_cost < baseline_cost`.
- "Could this fault-like effect be harnessed safely?" -> define the desired effect, collateral bad states, and the assumptions separating the two.
- "Could this architecture converge without global locks?" -> model ownership, transitions, and a no-conflict invariant.
- "Could this memory layout dominate both locality and safety?" -> model address ranges, aliasing rules, and the cost function tied to locality.

## Z3 Habits For Research

- Start with the coarsest honest model that can falsify the idea.
- Add detail only when the coarse model survives.
- Prefer a witness over a debate. `sat` gives a construction; `unsat` kills a branch.
- Separate structural proof from cost proof. Something can be valid and still lose on performance.
- When optimization matters, compare against a named baseline instead of claiming "fast" in the abstract.
