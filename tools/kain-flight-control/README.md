# Kain Flight Control

`kain-flight-control` is a portable, data-driven MCP server for Kain development.

It is intentionally local-only and deterministic:

- no hardcoded checkout paths in repo-tracked config
- no generic shell proxy
- no remote services, embeddings, or RAG layer
- repo knowledge comes from declarative registries in `config/server.toml`

## Root Path Model

The server resolves the repo root from `KAIN_REPO_ROOT`.

If that variable is not set, the Python launcher falls back to the script location,
which keeps direct repo-local launches working.

## Launcher Behavior

`launcher.py` prefers a built binary in `tools/kain-flight-control/bin/`.

If no binary is present, it falls back to:

```bash
go run ./cmd/kain-flight-control --config tools/kain-flight-control/config/server.toml
```

## Local Build

```bash
cd tools/kain-flight-control
go build -o bin/kain-flight-control ./cmd/kain-flight-control
```

## Root Templates

The repo root includes:

- `mcp.json` for MCP clients that use JSON config
- `codex.config.toml` for Codex MCP registration

Both templates are env-driven and avoid machine-specific absolute repo paths.
