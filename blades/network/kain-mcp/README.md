# `kain-mcp`

Canonical Kain-authored MCP blade for this checkout.

Entry points:

- `kain run blades/kain-mcp`
- `py -3 scripts/python/launch_kain_mcp.py`
- `powershell -ExecutionPolicy Bypass -File scripts/windows/sync-kain-source-of-truth.ps1`

Blade-owned data:

- `config/tools.json`: tool metadata, schemas, handler ids, write-safety flags
- `config/runtime_policy.json`: repo/env policy, binary lookup order, managed sync lock/cooldown/stamps, read limits, skip dirs, and validator runner settings

Source layout:

- `src/main.kn`: MCP router and request loop
- `src/runtime_settings.kn`: repo root, blade root, binary, and policy resolution
- `src/tool_registry.kn`: tool manifest loading plus initialize/tools metadata
- `src/protocol.kn`: MCP framing and JSON-RPC helpers
- `src/filesystem_tools.kn`: filesystem tool handlers
- `src/kain_tools.kn`: Kain CLI tool handlers
- `src/authoring_tools.kn`: docs/examples authoring helpers
