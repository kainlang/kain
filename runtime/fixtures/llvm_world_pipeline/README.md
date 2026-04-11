# LLVM World Pipeline Fixture

This fixture compiles to LLVM, links against the native runtime, executes the produced binary, and validates:

- world initialization emits and executes the generated world bootstrap
- patch application and converge/orchestrate lowering survive into the linked binary
- the current compiler-owned-intent execution lane produces a deterministic result

Expected result:

- The executable exits with code `10`
- The emitted LLVM IR contains `__kain_init_world_Studio`, `choose_value`, and `stage_bias`
