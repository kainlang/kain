# KAIN MCP

Experimental MCP server lane written in KAIN.

Entry point:

- `server.kn`

Transport contract:

- MCP JSON-RPC on `stdin` / `stdout`
- logs on `stderr`

Environment:

- `KAIN_REPO_ROOT` or `KAIN_MCP_REPO_ROOT` sets the workspace root
- `KAIN_MCP_KAIN_BIN` overrides the CLI binary used for `kain.*` commands
- `KAIN_MCP_ALLOW_WRITES` toggles filesystem write tools, default `true`
- `KAIN_MCP_INCLUDE_HIDDEN` includes hidden files in directory scans, default `false`
- `KAIN_MCP_SKIP_DIRS` is a comma-separated skip list for recursive scans

Tools:

- `fs.list_directory`
- `fs.read_file`
- `fs.write_file`
- `fs.make_directory`
- `fs.get_connections`
- `kain.build`
- `kain.import_rust`
- `kain.import_c`
- `kain.import_ts`
- `kain.omni_init`
- `kain.omni_build`
- `kain.fabric_init`
- `kain.fabric_validate`
- `kain.fabric_run`
- `workspace.init`

Current shape:

- The server is actor-backed through a router actor.
- Tool metadata is registry-driven from KAIN data structures.
- CLI and pipeline tools are thin wrappers around the native `command_run` builtin.
- Filesystem tools use the native `read_dir`, `read_file`, `write_file`, `create_dir_all`, and path helpers.
