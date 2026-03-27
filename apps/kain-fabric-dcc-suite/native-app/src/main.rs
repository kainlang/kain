#![cfg_attr(all(target_os = "windows", not(debug_assertions)), windows_subsystem = "windows")]

use std::path::PathBuf;

use kain_ui_native::run_bundled_app_json;

mod runtime_bridge;

use runtime_bridge::{spawn_live_bridge, LiveBridgePaths};

const KAIN_RUNTIME_BUNDLE: &str = include_str!("../generated/native_app_bundle.json");

fn resolve_runtime_sidecar(file_name: &str) -> Option<PathBuf> {
    if let Some(current_exe_candidate) = std::env::current_exe().ok().and_then(|exe| {
        exe.parent().map(|dir| dir.join(file_name)).filter(|path| path.exists())
    }) {
        return Some(current_exe_candidate);
    }
    let manifest_candidate = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("generated").join(file_name);
    if manifest_candidate.exists() {
        return Some(manifest_candidate);
    }
    None
}

fn resolve_project_sidecar(file_name: &str, relative_source_path: &str) -> Option<PathBuf> {
    if let Some(runtime_sidecar) = resolve_runtime_sidecar(file_name) {
        return Some(runtime_sidecar);
    }
    let project_candidate = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join(relative_source_path);
    if project_candidate.exists() {
        return Some(project_candidate);
    }
    None
}

fn fallback_sidecar_path(relative_source_path: &str) -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR")).join(relative_source_path)
}

fn resolved_or_fallback_sidecar(file_name: &str, relative_source_path: &str) -> PathBuf {
    resolve_project_sidecar(file_name, relative_source_path)
        .unwrap_or_else(|| fallback_sidecar_path(relative_source_path))
}

fn existing_mirror_sidecars(primary_path: &std::path::Path, file_name: &str) -> Vec<PathBuf> {
    let mut mirrors = Vec::new();
    for candidate_path in [
        PathBuf::from(env!("CARGO_MANIFEST_DIR"))
            .join("state")
            .join(file_name),
        PathBuf::from(env!("CARGO_MANIFEST_DIR"))
            .join("../state")
            .join(file_name),
    ] {
        if candidate_path.exists() && candidate_path != primary_path {
            mirrors.push(candidate_path);
        }
    }
    mirrors
}

fn build_live_bridge_paths() -> LiveBridgePaths {
    let command_queue_path =
        resolved_or_fallback_sidecar("command_queue.jsonl", "../state/command_queue.jsonl");
    let session_document_path =
        resolved_or_fallback_sidecar("session_document.json", "../state/session_document.json");
    let runtime_snapshot_path =
        resolved_or_fallback_sidecar("runtime_snapshot.json", "../state/runtime_snapshot.json");

    LiveBridgePaths {
        mirrored_session_document_paths: existing_mirror_sidecars(
            &session_document_path,
            "session_document.json",
        ),
        mirrored_runtime_snapshot_paths: existing_mirror_sidecars(
            &runtime_snapshot_path,
            "runtime_snapshot.json",
        ),
        command_queue_path,
        session_document_path,
        runtime_snapshot_path,
    }
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    if let Some(path) = resolve_runtime_sidecar("native_app_bundle.json") {
        std::env::set_var("KAIN_UI_NATIVE_RUNTIME_BUNDLE", &path);
    }
    if let Some(path) = resolve_runtime_sidecar("kain_realtime_app_bundle.json") {
        std::env::set_var("KAIN_UI_NATIVE_REALTIME_BUNDLE", &path);
    }
    if let Some(path) = resolve_project_sidecar("app_manifest.json", "../config/app_manifest.json") {
        std::env::set_var("KAIN_UI_NATIVE_APP_MANIFEST", &path);
    }
    let live_bridge_paths = build_live_bridge_paths();
    std::env::set_var(
        "KAIN_UI_NATIVE_APP_SNAPSHOT",
        &live_bridge_paths.runtime_snapshot_path,
    );
    std::env::set_var(
        "KAIN_UI_NATIVE_COMMAND_BRIDGE",
        &live_bridge_paths.command_queue_path,
    );
    spawn_live_bridge(live_bridge_paths);
    run_bundled_app_json(KAIN_RUNTIME_BUNDLE)
}
