#![cfg_attr(all(target_os = "windows", not(debug_assertions)), windows_subsystem = "windows")]

use std::path::PathBuf;

use kain_ui_native::run_material_atrium_showcase;

const SMOKE_SOURCE: &str = include_str!("../../smoke.kn");

fn requested_renderer_backend() -> Option<String> {
    let backend = std::env::args()
        .skip(1)
        .find(|candidate| matches!(candidate.as_str(), "bgfx" | "filament" | "diligent" | "the-forge"))?;
    Some(backend)
}

fn resolve_showcase_sidecar(file_name: &str) -> Option<PathBuf> {
    if let Some(current_exe_candidate) = std::env::current_exe().ok().and_then(|exe| {
        exe.parent().map(|dir| dir.join(file_name)).filter(|path| path.exists())
    }) {
        return Some(current_exe_candidate);
    }

    let manifest_candidate = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("generated")
        .join(file_name);
    if manifest_candidate.exists() {
        return Some(manifest_candidate);
    }

    None
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    if std::env::var_os("KAIN_RUNTIME_RENDERER_BACKEND").is_none() {
        if let Some(backend) = requested_renderer_backend() {
            std::env::set_var("KAIN_RUNTIME_RENDERER_BACKEND", backend);
        } else {
            std::env::set_var("KAIN_RUNTIME_RENDERER_BACKEND", "bgfx");
        }
    }
    std::env::set_var("KAIN_NATIVE_SCENE_PROFILE", "material_atrium");

    if let Some(path) = resolve_showcase_sidecar("material_atrium_visual_example.png") {
        std::env::set_var("KAIN_UI_NATIVE_QT_VIEWPORT_IMAGE_PATH", &path);
    }

    run_material_atrium_showcase(SMOKE_SOURCE)
}
