# JavaScript Bridge

`std::javascript::*` is the host-backed JavaScript and Node bridge for Kain.

It is meant for:

- importing Node modules and local `.js` / `.mjs` helpers
- driving web-adjacent asset generation from one `.kn` file
- using Kain as the orchestration layer while JavaScript handles ecosystem-specific work

The runtime lane is currently host-backed only:

- `interpret`
- `test`

Core wrapper module:

- `std::javascript::bridge`
- `std::javascript::web`

Typical flow:

```kain
use std::javascript::bridge
use std::javascript::web

fn main() -> String:
    let path = js_bridge_import("node:path")
    let basename = js_bridge_call_method(path, "basename", ["outputs/demo.html"])
    js_web_write_text("outputs/demo.txt", basename)
    return basename
```

Configuration is data-driven through `KAIN.toml`:

```toml
[node_ffi]
command = "node"
args = []
cwd = "."
```

If you want TypeScript-module interop, point `command` at a runtime that understands TS directly, such as `tsx`.

The current bridge also has buffer helpers for `Uint8Array`, `Buffer`, and `ArrayBuffer`:

- `js_bridge_buffer_info`
- `js_bridge_buffer_bytes`
- `js_web_buffer_info`
- `js_web_buffer_bytes`

It now also has higher-level payload adapters for web-facing artifacts:

- document payloads via `js_web_document_info`, `js_web_document_text`, and `js_web_document_write`
- image or canvas payloads via `js_web_image_info`, `js_web_image_bytes`, `js_web_image_write`, `js_web_canvas_info`, and `js_web_canvas_write`
- shared contract materialization via `js_web_shared_buffer(...)` and `js_web_shared_image(...)`

Those payload adapters are designed for helpers that return structured objects such as:

- `{ kind: "document", mime_type: "text/html", text: "<!doctype html>..." }`
- `{ kind: "canvas", mime_type: "image/svg+xml", width, height, text: "<svg...>", bytes: Uint8Array(...) }`

If you want a runtime-neutral payload that can move into Rust crate FFI or future C/C++ bridges, convert it into the shared contract first with `js_web_shared_buffer(...)` or `js_web_shared_image(...)`.

## Template-Oriented Helpers

The JavaScript stdlib layer now also includes two template-facing wrappers for
web application packs under `templates/Web`:

- `std::javascript::site_runtime`
- `std::javascript::site_actor`

They are intentionally thin wrappers over a local helper module, usually
`./helpers/web_runtime.mjs`, so Kain source can stay focused on orchestration
instead of repeating Node import, manifest loading, and artifact-writing glue.

Typical flow:

```kain
use std::javascript::site_runtime

fn main() -> String:
    let summary = js_site_write_matrix("manifests/app.json")
    println("built " + str(summary.experience_count) + " experiences")
    return summary.output_root
```

Those helpers are designed for no-Rust-required web templates where:

- Kain owns app orchestration, semantic UI preview, and template composition
- Node owns ecosystem-specific packaging, static serving, and actor-server glue
- manifests stay as the single source of truth for themes, content, scenes,
  experiences, and route surfaces
