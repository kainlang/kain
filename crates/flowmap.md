# Code/Kain/crates Flow Map

- Directory: `M:\Code\Kain\crates`
- Generated (UTC): `2026-03-28T20:00:22.324002+00:00`
- Languages: `JSON, JavaScript, Kain, Markdown, Rust, TOML, TypeScript`
- Entry files: `kain-ui-native/src/main.rs, kain-fast3d-runtime/src/main.rs, cli/src/main.rs, web/src/lib.rs`
- Manifests: `cargo, cargo, cargo, cargo, cargo, cargo, cargo, cargo, cargo, cargo, cargo, cargo`
- Additional manifests omitted from markdown: `32`

```mermaid
flowchart LR
  dir_dir["Code/Kain/crates"]
  dir_unreal["unreal"]
  dir_cli["cli"]
  dir_kain_core["kain-core"]
  dir_kain_asm["kain-asm"]
  dir_kain_3d["kain-3D"]
  dir_ue5_graphs["ue5-graphs"]
  file_kain_ui_native_src_main_rs["kain-ui-native/src/main.rs"]
  file_kain_fast3d_runtime_src_main_rs["kain-fast3d-runtime/src/main.rs"]
  file_cli_src_main_rs["cli/src/main.rs"]
  file_web_src_lib_rs["web/src/lib.rs"]
  file_cli_src_fabric_rs["cli/src/fabric.rs"]
  file_cli_src_import_asm_rs["cli/src/import_asm.rs"]
  file_cli_src_import_c_rs["cli/src/import_c.rs"]
  file_cli_src_import_crate_rs["cli/src/import_crate.rs"]
  file_cli_src_import_rust_rs["cli/src/import_rust.rs"]
  file_cli_src_import_typescript_rs["cli/src/import_typescript.rs"]
  lane_gpu_shader_lane["GPU / shader lane"]
  lane_unreal_lane["Unreal lane"]
  lane_web_wasm_lane["Web / WASM lane"]
  lane_python_host_lane["Python host lane"]
  lane_node_ts_host_lane["Node / TS host lane"]
  lane_interop_ffi_lane["Interop / FFI lane"]
  dir_dir -->|entrypoint| file_cli_src_main_rs
  dir_dir -->|entrypoint| file_kain_fast3d_runtime_src_main_rs
  dir_dir -->|entrypoint| file_kain_ui_native_src_main_rs
  dir_dir -->|entrypoint| file_web_src_lib_rs
  file_cli_src_import_c_rs -->|targets| lane_interop_ffi_lane
  file_cli_src_import_c_rs -->|targets| lane_web_wasm_lane
  file_cli_src_import_crate_rs -->|targets| lane_interop_ffi_lane
  file_cli_src_import_typescript_rs -->|targets| lane_gpu_shader_lane
  file_cli_src_import_typescript_rs -->|targets| lane_interop_ffi_lane
  file_cli_src_import_typescript_rs -->|targets| lane_node_ts_host_lane
  file_cli_src_main_rs -->|targets| lane_gpu_shader_lane
  file_cli_src_main_rs -->|targets| lane_interop_ffi_lane
  file_cli_src_main_rs -->|targets| lane_node_ts_host_lane
  file_cli_src_main_rs -->|targets| lane_python_host_lane
  file_cli_src_main_rs -->|targets| lane_unreal_lane
  file_cli_src_main_rs -->|targets| lane_web_wasm_lane
  file_web_src_lib_rs -->|targets| lane_web_wasm_lane
  file_cli_src_main_rs -->|imports| dir_cli
  file_cli_src_main_rs -->|imports| file_cli_src_fabric_rs
  file_cli_src_main_rs -->|imports| file_cli_src_import_asm_rs
  file_cli_src_main_rs -->|imports| file_cli_src_import_c_rs
  file_cli_src_main_rs -->|imports| file_cli_src_import_crate_rs
  file_cli_src_main_rs -->|imports| file_cli_src_import_rust_rs
  file_cli_src_main_rs -->|imports| file_cli_src_import_typescript_rs
  dir_dir -->|supports| file_cli_src_fabric_rs
  dir_dir -->|supports| file_cli_src_import_asm_rs
  dir_dir -->|supports| file_cli_src_import_c_rs
  dir_dir -->|supports| file_cli_src_import_crate_rs
```

## Manifest Summary
- `browser/Cargo.toml`: cargo, deps: 4
- `cli/Cargo.toml`: cargo, deps: 39
- `gpu/Cargo.toml`: cargo, deps: 4
- `kain-3D/Cargo.toml`: cargo, deps: 7
- `kain-asm/Cargo.toml`: cargo, deps: 12
- `kain-build/Cargo.toml`: cargo, deps: 1
- `kain-c-ffi/Cargo.toml`: cargo, deps: 12
- `kain-core/Cargo.toml`: cargo, deps: 13
- `kain-crate-ffi/Cargo.toml`: cargo, deps: 10
- `kain-driver/Cargo.toml`: cargo, deps: 16
- `kain-fast3d-runtime/Cargo.toml`: cargo, deps: 8
- `kain-gpu-runtime/Cargo.toml`: cargo, deps: 6

## Edge Legend
- `entrypoint`: root directory to a main entry file or manifest.
- `supports`: root or file contributes to a lane or helper surface.
- `imports`: file-to-file or file-to-subdir reference discovered from source text.
- `targets`: file participates in a platform lane such as Tauri, Unreal, GPU, or Python.
- `emits sidecar`: file materializes a named artifact or sidecar bundle.
