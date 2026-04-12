use std::{error::Error, fmt};

use crate::no_egui_qt_host::launch_qt_quick_host;
use kain_core::{build_ui_output_from_source, KainError};
use kain_ui::{
    ui_runtime_bundle_from_json, ui_runtime_bundle_from_output, ui_runtime_bundle_to_json,
    validate_ui_runtime_bundle, UiBuildOutput, UiHostBackendKind, UiLayoutEngineKind,
    UiRenderEngineKind, UiRuntimeBundle, UiRuntimeMetadata, UI_RUNTIME_BUNDLE_SCHEMA_VERSION,
};

pub const KAIN_UI_NATIVE_RUNTIME_BUNDLE_SCHEMA_VERSION: u32 = UI_RUNTIME_BUNDLE_SCHEMA_VERSION;
pub type KainUiNativeRuntimeMetadata = UiRuntimeMetadata;
pub type KainUiNativeRuntimeBundle = UiRuntimeBundle;

#[derive(Clone, Debug)]
pub struct KainUiNativeBackendPlan {
    pub shell_host_backend: UiHostBackendKind,
    pub document_host_backend: UiHostBackendKind,
    pub devtools_host_backend: UiHostBackendKind,
    pub layout_engine: UiLayoutEngineKind,
    pub render_engine: UiRenderEngineKind,
    pub compatibility_host_backend: UiHostBackendKind,
    pub mixed_backend_session: bool,
}

impl Default for KainUiNativeBackendPlan {
    fn default() -> Self {
        Self {
            shell_host_backend: UiHostBackendKind::Qt,
            document_host_backend: UiHostBackendKind::RmlUi,
            devtools_host_backend: UiHostBackendKind::Imgui,
            layout_engine: UiLayoutEngineKind::Yoga,
            render_engine: UiRenderEngineKind::Wgpu,
            compatibility_host_backend: UiHostBackendKind::Qt,
            mixed_backend_session: true,
        }
    }
}

impl KainUiNativeBackendPlan {
    fn from_runtime_metadata(metadata: &UiRuntimeMetadata) -> Self {
        Self {
            shell_host_backend: metadata.preferred_shell_host_backend,
            document_host_backend: metadata.preferred_document_host_backend,
            devtools_host_backend: metadata.preferred_devtools_host_backend,
            layout_engine: metadata.preferred_layout_engine,
            render_engine: metadata.preferred_render_engine,
            compatibility_host_backend: metadata.compatibility_host_backend,
            mixed_backend_session: metadata.mixed_backend_session,
        }
    }
}

#[derive(Clone, Debug)]
pub struct KainUiNativeAppConfig {
    pub window_title: String,
    pub root_component: String,
    pub source: String,
    pub initial_window_size: [f32; 2],
    pub backend_plan: KainUiNativeBackendPlan,
}

pub type KainUiNativeDemoConfig = KainUiNativeAppConfig;

impl Default for KainUiNativeAppConfig {
    fn default() -> Self {
        Self {
            window_title: "KAIN UI Native".to_string(),
            root_component: "App".to_string(),
            source: String::new(),
            initial_window_size: [1440.0, 920.0],
            backend_plan: KainUiNativeBackendPlan::default(),
        }
    }
}

#[derive(Debug)]
struct UnsupportedNativeHostError {
    attempted_backend: &'static str,
}

impl fmt::Display for UnsupportedNativeHostError {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            formatter,
            "kain-ui-native default host routing only supports the Qt-backed path right now; `{}` does not have a live non-egui adapter in this build",
            self.attempted_backend
        )
    }
}

impl Error for UnsupportedNativeHostError {}

pub fn build_output(config: &KainUiNativeAppConfig) -> Result<UiBuildOutput, KainError> {
    build_ui_output_from_source(&config.source, &config.root_component)
}

pub fn build_runtime_bundle(
    config: &KainUiNativeAppConfig,
) -> Result<KainUiNativeRuntimeBundle, KainError> {
    let output = build_output(config)?;
    Ok(runtime_bundle_from_output(config, output))
}

pub fn runtime_bundle_from_output(
    config: &KainUiNativeAppConfig,
    output: UiBuildOutput,
) -> KainUiNativeRuntimeBundle {
    ui_runtime_bundle_from_output(
        KainUiNativeRuntimeMetadata {
            app_name: None,
            window_title: config.window_title.clone(),
            root_component: config.root_component.clone(),
            source_file_name: None,
            initial_window_size: config.initial_window_size,
            preferred_shell_host_backend: config.backend_plan.shell_host_backend,
            preferred_document_host_backend: config.backend_plan.document_host_backend,
            preferred_devtools_host_backend: config.backend_plan.devtools_host_backend,
            preferred_layout_engine: config.backend_plan.layout_engine,
            preferred_render_engine: config.backend_plan.render_engine,
            compatibility_host_backend: config.backend_plan.compatibility_host_backend,
            mixed_backend_session: config.backend_plan.mixed_backend_session,
        },
        output,
    )
}

pub fn runtime_bundle_to_json(
    bundle: &KainUiNativeRuntimeBundle,
) -> Result<String, serde_json::Error> {
    ui_runtime_bundle_to_json(bundle)
}

pub fn runtime_bundle_from_json(json: &str) -> Result<KainUiNativeRuntimeBundle, serde_json::Error> {
    ui_runtime_bundle_from_json(json)
}

pub fn build_demo_output(config: &KainUiNativeDemoConfig) -> Result<UiBuildOutput, KainError> {
    build_output(config)
}

pub fn run_app(config: KainUiNativeAppConfig) -> Result<(), Box<dyn Error>> {
    let bundle = build_runtime_bundle(&config)?;
    run_bundled_app(bundle)
}

pub fn run_bundled_app(bundle: KainUiNativeRuntimeBundle) -> Result<(), Box<dyn Error>> {
    validate_runtime_bundle(&bundle)?;
    let backend_plan = KainUiNativeBackendPlan::from_runtime_metadata(&bundle.metadata);
    match normalized_shell_backend(&backend_plan) {
        UiHostBackendKind::Qt => launch_qt_quick_host(&bundle, &backend_plan)
            .map_err(|error| Box::new(error) as Box<dyn Error>),
        _ => Err(Box::new(UnsupportedNativeHostError {
            attempted_backend: host_launch_label(&backend_plan),
        })),
    }
}

pub fn run_bundled_app_json(json: &str) -> Result<(), Box<dyn Error>> {
    let bundle = runtime_bundle_from_json(json)?;
    run_bundled_app(bundle)
}

pub fn run_demo(config: KainUiNativeDemoConfig) -> Result<(), Box<dyn Error>> {
    run_app(config)
}

fn validate_runtime_bundle(bundle: &KainUiNativeRuntimeBundle) -> Result<(), Box<dyn Error>> {
    validate_ui_runtime_bundle(bundle).map_err(|error| Box::new(error) as Box<dyn Error>)
}

fn host_launch_label(backend_plan: &KainUiNativeBackendPlan) -> &'static str {
    match normalized_shell_backend(backend_plan) {
        UiHostBackendKind::Auto => "auto",
        UiHostBackendKind::Native => "native",
        UiHostBackendKind::LegacyEgui => "legacy-egui",
        UiHostBackendKind::Imgui => "imgui",
        UiHostBackendKind::RmlUi => "rmlui",
        UiHostBackendKind::Slint => "slint",
        UiHostBackendKind::Qt => "qt",
        UiHostBackendKind::Cef => "cef",
    }
}

fn normalized_shell_backend(backend_plan: &KainUiNativeBackendPlan) -> UiHostBackendKind {
    match backend_plan.shell_host_backend {
        UiHostBackendKind::Auto | UiHostBackendKind::Native => UiHostBackendKind::Qt,
        backend => backend,
    }
}
