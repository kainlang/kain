use kain_ui_native::{run_app, KainUiNativeAppConfig};

const KAIN_SOURCE: &str = include_str!("../native_bundle_smoke.kn");

fn main() -> Result<(), Box<dyn std::error::Error>> {
    run_app(KainUiNativeAppConfig {
        window_title: "native_bundle_smoke".to_string(),
        root_component: "App".to_string(),
        source: KAIN_SOURCE.to_string(),
        initial_window_size: [1440.0, 920.0],
    })
}
