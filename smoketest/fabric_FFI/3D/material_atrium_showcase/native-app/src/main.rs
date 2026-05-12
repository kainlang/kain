#![cfg_attr(all(target_os = "windows", not(debug_assertions)), windows_subsystem = "windows")]

use std::path::PathBuf;

use kain_ui_native::run_bundled_app_json;

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

fn main() -> Result<(), Box<dyn std::error::Error>> {
    if let Some(path) = resolve_runtime_sidecar("native_app_bundle.json") {
        std::env::set_var("KAIN_UI_NATIVE_RUNTIME_BUNDLE", &path);
    }
    if let Some(path) = resolve_runtime_sidecar("kain_realtime_app_bundle.json") {
        std::env::set_var("KAIN_UI_NATIVE_REALTIME_BUNDLE", &path);
    }
    if let Some(path) = resolve_runtime_sidecar("generic_scene_visual_reference.png") {
        std::env::set_var("KAIN_UI_NATIVE_QT_VIEWPORT_IMAGE_PATH", &path);
    }
    if let Some(path) = resolve_project_sidecar("app_manifest.json", "../config/app_manifest.json") {
        std::env::set_var("KAIN_UI_NATIVE_APP_MANIFEST", &path);
    }
    if let Some(path) = resolve_project_sidecar("runtime_snapshot.json", "../state/runtime_snapshot.json") {
        std::env::set_var("KAIN_UI_NATIVE_APP_SNAPSHOT", &path);
    }
    run_bundled_app_json(KAIN_RUNTIME_BUNDLE)
}
