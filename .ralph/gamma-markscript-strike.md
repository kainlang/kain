# GAMMA — MarkScript Companion Strike

Implement all GAMMA-lane features for markscript:
- G1: CLI Maturity — pipe, watch, build, test, clean
- G2: Process Lifecycle — PID tracking, spawn tracked, await, kill, pipe, env, cwd
- G3: Structured Build Definitions — rewrite std/build.md
- G4: --json output for all subcommands
- G5: Mksfile.md auto-discovery

## Files to modify:
1. src/types.kn — Add ProcessRecord struct
2. src/vm.kn — Add processes field to MarkScriptVM
3. src/bridge.kn — Add handlers 51-59
4. src/cli.kn — New subcommand constants, parse_args, auto-detection
5. src/main.kn — New subcommand handlers, --json, auto-discovery
6. std/build.md — Rewrite as canonical format
7. std/process.md — Update with lifecycle intents

## Handshake contracts:
- H2: Handler IDs 51-59
- H4: VM state extensions
- H5: Build definition contract
- H8: Testing contract