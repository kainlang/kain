# Native Process Stdio Fixture

This fixture proves that LLVM/direct-native Kain source can author real child-process and PTY flows through the `kain_native_process_*` ABI without falling back to host-only command helpers.

It validates:

- process specifications with argv wiring
- stdio piping and stdout capture
- stdin writes into a real child process
- exit waiting and exit-code reads
- PTY-backed interactive command exchange

Validate from the repo root:

```bash
./runtime/fixtures/validate_all.sh
```
