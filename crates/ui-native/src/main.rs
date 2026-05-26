fn main() -> Result<(), Box<dyn std::error::Error>> {
    kain_ui_native::run_app(kain_ui_native::KainUiNativeAppConfig::default())
}
