# LLVM Actor Message Fixture

This fixture compiles to LLVM, links against the native runtime, executes the produced binary, and validates:

- actor spawn uses the actor-specific bootstrap entrypoint
- message sends lower through the mailbox push path
- the produced executable runs successfully on the native runtime lane

Expected result:

- The executable exits with code `0`
- The emitted LLVM IR contains `Printer_run`, `kain_actor_spawn`, `kain_actor_send`, and the canonical `%KainActorMessage` / `%KainActorSpawnConfig` ABI types
