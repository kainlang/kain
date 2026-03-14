#![cfg_attr(target_os = "windows", windows_subsystem = "windows")]

use kain_ui_native::run_bundled_app_json;

const KAIN_RUNTIME_BUNDLE: &str = include_str!("../generated/native_app_bundle.json");

fn main() -> Result<(), Box<dyn std::error::Error>> {
    run_bundled_app_json(KAIN_RUNTIME_BUNDLE)
}
