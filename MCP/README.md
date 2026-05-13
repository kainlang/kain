# KAIN MCP Redirect

The canonical MCP server now lives in the real blade at `blades/kain-mcp`.

Use one of these entrypoints:

- `kain run blades/kain-mcp`
- `py -3 scripts/python/launch_kain_mcp.py`

Source of truth:

- Blade manifest: `blades/kain-mcp/KAIN.toml`
- Runtime policy: `blades/kain-mcp/config/runtime_policy.json`
- Tool registry: `blades/kain-mcp/config/tools.json`
- Server modules: `blades/kain-mcp/src/*.kn`

This `MCP/` folder is now redirect-only documentation. Do not add new live server logic here.
