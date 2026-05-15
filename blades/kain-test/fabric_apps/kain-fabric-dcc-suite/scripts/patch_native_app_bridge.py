#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path


APP_ROOT = Path(__file__).resolve().parent.parent
NATIVE_APP_ROOT = APP_ROOT / "native-app"


def patch_cargo_toml(path: Path) -> None:
    content = path.read_text(encoding="utf-8")
    additions = []
    if "\nserde = " not in content:
        additions.append('serde = { version = "1", features = ["derive"] }')
    if "\nserde_json = " not in content:
        additions.append('serde_json = "1"')
    if not additions:
        return
    content = content.rstrip() + "\n" + "\n".join(additions) + "\n"
    path.write_text(content, encoding="utf-8")


def patch_main_rs(path: Path) -> None:
    content = path.read_text(encoding="utf-8")
    if "mod bridge_contract;" in content and "spawn_live_bridge" in content:
        return

    content = content.replace(
        "use std::path::PathBuf;\n\n",
        "use std::path::PathBuf;\n\nmod bridge_contract;\nmod runtime_bridge;\n\nuse runtime_bridge::{spawn_live_bridge, LiveBridgePaths};\n\n",
        1,
    )

    marker = '    if let Some(path) = resolve_project_sidecar("runtime_snapshot.json", "../state/runtime_snapshot.json") {\n        std::env::set_var("KAIN_UI_NATIVE_APP_SNAPSHOT", &path);\n    }\n'
    injection = marker + """    if let Some(command_queue_path) = resolve_project_sidecar("command_queue.jsonl", "../state/command_queue.jsonl") {\n        std::env::set_var("KAIN_UI_NATIVE_COMMAND_BRIDGE", &command_queue_path);\n        let project_session_path = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../state/session_document.json");\n        let project_snapshot_path = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../state/runtime_snapshot.json");\n        let native_session_path = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("state/session_document.json");\n        let native_snapshot_path = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("state/runtime_snapshot.json");\n        spawn_live_bridge(LiveBridgePaths {\n            command_queue_path,\n            session_document_path: project_session_path,\n            runtime_snapshot_path: project_snapshot_path,\n            mirrored_session_document_paths: vec![native_session_path],\n            mirrored_runtime_snapshot_paths: vec![native_snapshot_path],\n        });\n    }\n"""
    if marker not in content:
        raise RuntimeError(f"Could not find runtime snapshot marker in {path}")
    content = content.replace(marker, injection, 1)
    path.write_text(content, encoding="utf-8")


def main() -> None:
    cargo_toml_path = NATIVE_APP_ROOT / "Cargo.toml"
    main_rs_path = NATIVE_APP_ROOT / "src" / "main.rs"
    if not cargo_toml_path.exists() or not main_rs_path.exists():
        raise SystemExit("Native app bundle has not been materialized yet.")

    patch_cargo_toml(cargo_toml_path)
    patch_main_rs(main_rs_path)
    print(f"Patched {cargo_toml_path}")
    print(f"Patched {main_rs_path}")


if __name__ == "__main__":
    main()
