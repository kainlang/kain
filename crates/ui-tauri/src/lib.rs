use std::collections::{BTreeMap, BTreeSet};

use kain_core::runtime_contract::RuntimeReflectionPayload;
use kain_reflect::{TypeRegistry, TypeSchema};
#[cfg(test)]
use kain_ui::{UiBuildOutput, UiRuntimeMetadata};
use kain_ui::{
    UiCommandDescriptor, UiCommandEffectKind, UiHostBackendKind, UiLayoutEngineKind,
    UiRenderEngineKind, UiRuntimeBundle, UiSurfaceKind,
};
use serde::{Deserialize, Serialize};
use serde_json::{json, Value};

pub const KAIN_TAURI_BRIDGE_SCHEMA_VERSION: u32 = 1;
pub const KAIN_TAURI_BRIDGE_INVOKE_COMMAND: &str = "kain_bridge_dispatch";
pub const KAIN_TAURI_HOT_RELOAD_EVENT: &str = "kain://runtime/reload";
pub const KAIN_TAURI_BRIDGE_READY_EVENT: &str = "kain://bridge/ready";
pub const KAIN_TAURI_BRIDGE_MANIFEST_FILE_NAME: &str = "kain_tauri_bridge_manifest.json";
pub const KAIN_TAURI_FRONTEND_DESCRIPTOR_FILE_NAME: &str = "hybrid_bundle.json";
pub const KAIN_TAURI_FRONTEND_BRIDGE_FILE_NAME: &str = "kain-bridge.js";
pub const KAIN_TAURI_FRONTEND_ENTRY_FILE_NAME: &str = "index.html";
pub const KAIN_TAURI_MAIN_CAPABILITY_IDENTIFIER: &str = "kain-main";
pub const KAIN_TAURI_CUSTOM_PERMISSION_IDENTIFIER: &str = "kain-bridge";
pub const KAIN_TAURI_SUPPORTED_NAMESPACES: &[&str] = &[
    "app",
    "window",
    "webview",
    "event",
    "path",
    "fs",
    "dialog",
    "shell",
    "process",
    "menu",
    "tray",
    "clipboard",
    "notification",
    "opener",
    "store",
    "sql",
    "http",
    "updater",
    "global-shortcut",
    "host",
    "runtime",
];

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, Serialize, Deserialize)]
#[serde(rename_all = "kebab-case")]
pub enum TauriPluginPreset {
    App,
    Window,
    Webview,
    Event,
    Path,
    Fs,
    Dialog,
    Shell,
    Process,
    Menu,
    Tray,
    Clipboard,
    Notification,
    Opener,
    Store,
    Sql,
    Http,
    Updater,
    GlobalShortcut,
}

impl TauriPluginPreset {
    pub fn namespace(self) -> &'static str {
        match self {
            Self::App => "app",
            Self::Window => "window",
            Self::Webview => "webview",
            Self::Event => "event",
            Self::Path => "path",
            Self::Fs => "fs",
            Self::Dialog => "dialog",
            Self::Shell => "shell",
            Self::Process => "process",
            Self::Menu => "menu",
            Self::Tray => "tray",
            Self::Clipboard => "clipboard",
            Self::Notification => "notification",
            Self::Opener => "opener",
            Self::Store => "store",
            Self::Sql => "sql",
            Self::Http => "http",
            Self::Updater => "updater",
            Self::GlobalShortcut => "global-shortcut",
        }
    }

    pub fn global_object_name(self) -> &'static str {
        match self {
            Self::Clipboard => "clipboardManager",
            Self::GlobalShortcut => "globalShortcut",
            other => other.namespace(),
        }
    }

    pub fn dependency_line(self) -> Option<&'static str> {
        match self {
            Self::App
            | Self::Window
            | Self::Webview
            | Self::Event
            | Self::Path
            | Self::Menu
            | Self::Tray => None,
            Self::Fs => Some("tauri-plugin-fs = \"2\""),
            Self::Dialog => Some("tauri-plugin-dialog = \"2\""),
            Self::Shell => Some("tauri-plugin-shell = \"2\""),
            Self::Process => Some("tauri-plugin-process = \"2\""),
            Self::Clipboard => Some("tauri-plugin-clipboard-manager = \"2\""),
            Self::Notification => Some("tauri-plugin-notification = \"2\""),
            Self::Opener => Some("tauri-plugin-opener = \"2\""),
            Self::Store => Some("tauri-plugin-store = \"2\""),
            Self::Sql => Some("tauri-plugin-sql = { version = \"2\", features = [\"sqlite\"] }"),
            Self::Http => Some("tauri-plugin-http = \"2\""),
            Self::Updater => Some("tauri-plugin-updater = \"2\""),
            Self::GlobalShortcut => Some("tauri-plugin-global-shortcut = \"2\""),
        }
    }

    pub fn registration_line(self) -> Option<&'static str> {
        match self {
            Self::App | Self::Window | Self::Webview | Self::Event | Self::Path | Self::Menu
            | Self::Tray => None,
            Self::Fs => Some("let builder = builder.plugin(tauri_plugin_fs::init());"),
            Self::Dialog => Some("let builder = builder.plugin(tauri_plugin_dialog::init());"),
            Self::Shell => Some("let builder = builder.plugin(tauri_plugin_shell::init());"),
            Self::Process => Some("let builder = builder.plugin(tauri_plugin_process::init());"),
            Self::Clipboard => {
                Some("let builder = builder.plugin(tauri_plugin_clipboard_manager::init());")
            }
            Self::Notification => {
                Some("let builder = builder.plugin(tauri_plugin_notification::init());")
            }
            Self::Opener => Some("let builder = builder.plugin(tauri_plugin_opener::init());"),
            Self::Store => {
                Some("let builder = builder.plugin(tauri_plugin_store::Builder::default().build());")
            }
            Self::Sql => {
                Some("let builder = builder.plugin(tauri_plugin_sql::Builder::default().build());")
            }
            Self::Http => Some("let builder = builder.plugin(tauri_plugin_http::init());"),
            Self::Updater => {
                Some("let builder = builder.plugin(tauri_plugin_updater::Builder::new().build());")
            }
            Self::GlobalShortcut => {
                Some("let builder = builder.plugin(tauri_plugin_global_shortcut::Builder::new().build());")
            }
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum TauriCapabilityPreset {
    MainWindow,
    DevWindow,
}

impl TauriCapabilityPreset {
    pub fn identifier(self, window_label: &str) -> String {
        match self {
            Self::MainWindow => KAIN_TAURI_MAIN_CAPABILITY_IDENTIFIER.to_string(),
            Self::DevWindow => format!("kain-{}-dev", normalize_identifier_fragment(window_label)),
        }
    }

    pub fn description(self, window_label: &str) -> String {
        match self {
            Self::MainWindow => {
                format!("Capability for the generated Kain window '{window_label}'.")
            }
            Self::DevWindow => {
                format!("Development capability for the generated Kain window '{window_label}'.")
            }
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, Serialize, Deserialize)]
#[serde(rename_all = "kebab-case")]
pub enum TauriPermissionPreset {
    CoreDefault,
    FsDefault,
    DialogDefault,
    ShellAllowOpen,
    ShellAllowSpawn,
    ShellAllowExecute,
    ShellAllowKill,
    ShellAllowStdinWrite,
    ProcessDefault,
    ClipboardAllowReadText,
    ClipboardAllowWriteText,
    NotificationDefault,
    OpenerDefault,
    StoreDefault,
    SqlDefault,
    SqlAllowExecute,
    HttpDefault,
    UpdaterDefault,
    GlobalShortcutAllowIsRegistered,
    GlobalShortcutAllowRegister,
    GlobalShortcutAllowUnregister,
    KainBridge,
}

impl TauriPermissionPreset {
    pub fn identifier(self) -> &'static str {
        match self {
            Self::CoreDefault => "core:default",
            Self::FsDefault => "fs:default",
            Self::DialogDefault => "dialog:default",
            Self::ShellAllowOpen => "shell:allow-open",
            Self::ShellAllowSpawn => "shell:allow-spawn",
            Self::ShellAllowExecute => "shell:allow-execute",
            Self::ShellAllowKill => "shell:allow-kill",
            Self::ShellAllowStdinWrite => "shell:allow-stdin-write",
            Self::ProcessDefault => "process:default",
            Self::ClipboardAllowReadText => "clipboard-manager:allow-read-text",
            Self::ClipboardAllowWriteText => "clipboard-manager:allow-write-text",
            Self::NotificationDefault => "notification:default",
            Self::OpenerDefault => "opener:default",
            Self::StoreDefault => "store:default",
            Self::SqlDefault => "sql:default",
            Self::SqlAllowExecute => "sql:allow-execute",
            Self::HttpDefault => "http:default",
            Self::UpdaterDefault => "updater:default",
            Self::GlobalShortcutAllowIsRegistered => "global-shortcut:allow-is-registered",
            Self::GlobalShortcutAllowRegister => "global-shortcut:allow-register",
            Self::GlobalShortcutAllowUnregister => "global-shortcut:allow-unregister",
            Self::KainBridge => KAIN_TAURI_CUSTOM_PERMISSION_IDENTIFIER,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct TauriFrontendAssetManifest {
    pub root_dir: String,
    pub entry_html: String,
    pub descriptor_file: String,
    pub js_bundle: String,
    pub ts_bundle: String,
    pub wasm_bundle: String,
    pub bridge_js: String,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct TauriRuntimeSidecarManifest {
    pub runtime_bundle: String,
    pub runtime_contract: String,
    pub runtime_compatibility: String,
    pub realtime_bundle: String,
    pub shader_bundle: Option<String>,
    pub reflection_payload: Option<String>,
    pub runtime_snapshot: String,
    pub app_manifest: String,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct TauriCommandMapping {
    pub command_name: String,
    pub label: String,
    pub effect: String,
    pub namespace: String,
    pub method: String,
    pub transport: String,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub invoke_command: Option<String>,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct TauriReflectionMetadata {
    pub compiler_payload: Option<RuntimeReflectionPayload>,
    pub host_types: Vec<TypeSchema>,
    pub merged_type_names: Vec<String>,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct TauriBridgeManifest {
    pub schema_version: u32,
    pub app_id: String,
    pub app_name: String,
    pub window_label: String,
    pub window_title: String,
    pub frontend: TauriFrontendAssetManifest,
    pub runtime_sidecars: TauriRuntimeSidecarManifest,
    pub enabled_plugin_presets: Vec<TauriPluginPreset>,
    pub capability_presets: Vec<TauriCapabilityPreset>,
    pub permission_presets: Vec<TauriPermissionPreset>,
    pub capability_identifiers: Vec<String>,
    pub command_mappings: Vec<TauriCommandMapping>,
    pub reflection: TauriReflectionMetadata,
    pub supported_namespaces: Vec<String>,
    pub invoke_command: String,
    pub hot_reload_event: String,
}

#[derive(Debug, Clone)]
pub struct TauriBridgeManifestConfig {
    pub app_id: String,
    pub app_name: String,
    pub window_label: String,
    pub window_title: String,
    pub frontend: TauriFrontendAssetManifest,
    pub runtime_sidecars: TauriRuntimeSidecarManifest,
    pub plugin_presets: Vec<TauriPluginPreset>,
    pub capability_presets: Vec<TauriCapabilityPreset>,
    pub permission_presets: Vec<TauriPermissionPreset>,
    pub commands: Vec<UiCommandDescriptor>,
    pub compiler_reflection_payload: Option<RuntimeReflectionPayload>,
    pub host_type_registry: TypeRegistry,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct RenderedTauriCapability {
    pub identifier: String,
    pub relative_path: String,
    pub contents: String,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct RenderedTauriProjectFiles {
    pub files: BTreeMap<String, String>,
    pub capabilities: Vec<RenderedTauriCapability>,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct TauriProjectRenderConfig {
    pub cargo_package_name: String,
    pub cargo_package_version: String,
    pub bundle_identifier: String,
    pub window_label: String,
    pub window_title: String,
    pub initial_window_size: [f32; 2],
    pub frontend_dist_relative_path: String,
    pub bridge_manifest_relative_path: String,
}

pub fn default_tauri_plugin_presets() -> Vec<TauriPluginPreset> {
    vec![
        TauriPluginPreset::App,
        TauriPluginPreset::Window,
        TauriPluginPreset::Webview,
        TauriPluginPreset::Event,
        TauriPluginPreset::Path,
        TauriPluginPreset::Fs,
        TauriPluginPreset::Dialog,
        TauriPluginPreset::Shell,
        TauriPluginPreset::Process,
        TauriPluginPreset::Menu,
        TauriPluginPreset::Tray,
        TauriPluginPreset::Clipboard,
        TauriPluginPreset::Notification,
        TauriPluginPreset::Opener,
        TauriPluginPreset::Store,
        TauriPluginPreset::Sql,
        TauriPluginPreset::Http,
        TauriPluginPreset::Updater,
        TauriPluginPreset::GlobalShortcut,
    ]
}

pub fn default_tauri_capability_presets() -> Vec<TauriCapabilityPreset> {
    vec![TauriCapabilityPreset::MainWindow]
}

pub fn default_tauri_permission_presets(
    plugin_presets: &[TauriPluginPreset],
) -> Vec<TauriPermissionPreset> {
    let mut permissions = BTreeSet::new();
    permissions.insert(TauriPermissionPreset::KainBridge);

    for preset in plugin_presets {
        match preset {
            TauriPluginPreset::App
            | TauriPluginPreset::Window
            | TauriPluginPreset::Webview
            | TauriPluginPreset::Event
            | TauriPluginPreset::Path
            | TauriPluginPreset::Menu
            | TauriPluginPreset::Tray => {
                permissions.insert(TauriPermissionPreset::CoreDefault);
            }
            TauriPluginPreset::Fs => {
                permissions.insert(TauriPermissionPreset::FsDefault);
            }
            TauriPluginPreset::Dialog => {
                permissions.insert(TauriPermissionPreset::DialogDefault);
            }
            TauriPluginPreset::Shell => {
                permissions.insert(TauriPermissionPreset::ShellAllowOpen);
                permissions.insert(TauriPermissionPreset::ShellAllowSpawn);
                permissions.insert(TauriPermissionPreset::ShellAllowExecute);
                permissions.insert(TauriPermissionPreset::ShellAllowKill);
                permissions.insert(TauriPermissionPreset::ShellAllowStdinWrite);
            }
            TauriPluginPreset::Process => {
                permissions.insert(TauriPermissionPreset::ProcessDefault);
            }
            TauriPluginPreset::Clipboard => {
                permissions.insert(TauriPermissionPreset::ClipboardAllowReadText);
                permissions.insert(TauriPermissionPreset::ClipboardAllowWriteText);
            }
            TauriPluginPreset::Notification => {
                permissions.insert(TauriPermissionPreset::NotificationDefault);
            }
            TauriPluginPreset::Opener => {
                permissions.insert(TauriPermissionPreset::OpenerDefault);
            }
            TauriPluginPreset::Store => {
                permissions.insert(TauriPermissionPreset::StoreDefault);
            }
            TauriPluginPreset::Sql => {
                permissions.insert(TauriPermissionPreset::SqlDefault);
                permissions.insert(TauriPermissionPreset::SqlAllowExecute);
            }
            TauriPluginPreset::Http => {
                permissions.insert(TauriPermissionPreset::HttpDefault);
            }
            TauriPluginPreset::Updater => {
                permissions.insert(TauriPermissionPreset::UpdaterDefault);
            }
            TauriPluginPreset::GlobalShortcut => {
                permissions.insert(TauriPermissionPreset::GlobalShortcutAllowIsRegistered);
                permissions.insert(TauriPermissionPreset::GlobalShortcutAllowRegister);
                permissions.insert(TauriPermissionPreset::GlobalShortcutAllowUnregister);
            }
        }
    }

    permissions.into_iter().collect()
}

pub fn retarget_ui_runtime_bundle_for_tauri(bundle: &UiRuntimeBundle) -> UiRuntimeBundle {
    let mut retargeted = bundle.clone();
    retargeted.metadata.preferred_shell_host_backend = UiHostBackendKind::Tauri;
    retargeted.metadata.preferred_document_host_backend = UiHostBackendKind::Tauri;
    retargeted.metadata.preferred_devtools_host_backend = UiHostBackendKind::Tauri;
    retargeted.metadata.preferred_layout_engine = UiLayoutEngineKind::Yoga;
    retargeted.metadata.preferred_render_engine = UiRenderEngineKind::Browser;
    retargeted.metadata.compatibility_host_backend = UiHostBackendKind::Tauri;
    retargeted.metadata.mixed_backend_session = true;

    for surface in &mut retargeted.output.systems.surfaces {
        surface.preferred_host_backend = UiHostBackendKind::Tauri;
        surface.preferred_layout_engine = UiLayoutEngineKind::Yoga;
        let is_canvas_backed_surface = surface.gpu_backing_required
            || surface.shader.is_some()
            || matches!(
                surface.kind,
                UiSurfaceKind::Viewport2D | UiSurfaceKind::Viewport3D
            );
        if !is_canvas_backed_surface {
            surface.preferred_render_engine = UiRenderEngineKind::Browser;
        } else if matches!(
            surface.preferred_render_engine,
            UiRenderEngineKind::Auto | UiRenderEngineKind::Native | UiRenderEngineKind::Skia
        ) {
            surface.preferred_render_engine = UiRenderEngineKind::Browser;
        }
    }

    retargeted
}

pub fn build_tauri_command_mappings(
    commands: &[UiCommandDescriptor],
) -> Result<Vec<TauriCommandMapping>, String> {
    let mut mappings = Vec::with_capacity(commands.len());
    for command in commands {
        let mapping = match command.effect {
            UiCommandEffectKind::RuntimeMutation => TauriCommandMapping {
                command_name: command.name.clone(),
                label: command.label.clone(),
                effect: "runtime_mutation".to_string(),
                namespace: "runtime".to_string(),
                method: command.name.clone(),
                transport: "runtime".to_string(),
                invoke_command: None,
            },
            UiCommandEffectKind::ExternalEffect => {
                let (namespace, method) =
                    if let Some(remainder) = command.name.strip_prefix("tauri.") {
                        parse_tauri_namespace_and_method(remainder)?
                    } else {
                        ("host".to_string(), command.name.clone())
                    };
                if !is_supported_tauri_namespace(namespace.as_str()) {
                    return Err(format!(
                        "Unsupported Tauri bridge namespace '{}' in command '{}'",
                        namespace, command.name
                    ));
                }
                TauriCommandMapping {
                    command_name: command.name.clone(),
                    label: command.label.clone(),
                    effect: "external_effect".to_string(),
                    namespace,
                    method,
                    transport: "invoke".to_string(),
                    invoke_command: Some(KAIN_TAURI_BRIDGE_INVOKE_COMMAND.to_string()),
                }
            }
        };
        mappings.push(mapping);
    }
    mappings.sort_by(|left, right| left.command_name.cmp(&right.command_name));
    Ok(mappings)
}

pub fn build_tauri_reflection_metadata(
    compiler_payload: Option<&RuntimeReflectionPayload>,
    host_type_registry: &TypeRegistry,
) -> TauriReflectionMetadata {
    let mut host_types = host_type_registry.schemas().cloned().collect::<Vec<_>>();
    host_types.sort_by(|left, right| left.name.cmp(&right.name));

    let mut merged_type_names = BTreeSet::new();
    if let Some(payload) = compiler_payload {
        for ty in &payload.types {
            merged_type_names.insert(ty.name.clone());
        }
    }
    for schema in &host_types {
        merged_type_names.insert(schema.name.clone());
    }

    TauriReflectionMetadata {
        compiler_payload: compiler_payload.cloned(),
        host_types,
        merged_type_names: merged_type_names.into_iter().collect(),
    }
}

pub fn build_tauri_bridge_manifest(
    config: &TauriBridgeManifestConfig,
) -> Result<TauriBridgeManifest, String> {
    let enabled_plugin_presets = dedupe_presets(config.plugin_presets.clone());
    let capability_presets = dedupe_capability_presets(config.capability_presets.clone());
    let permission_presets = if config.permission_presets.is_empty() {
        default_tauri_permission_presets(&enabled_plugin_presets)
    } else {
        dedupe_permission_presets(config.permission_presets.clone())
    };
    let command_mappings = build_tauri_command_mappings(&config.commands)?;
    validate_enabled_command_namespaces(&command_mappings, &enabled_plugin_presets)?;
    let capability_identifiers = capability_presets
        .iter()
        .map(|preset| preset.identifier(&config.window_label))
        .collect::<Vec<_>>();

    Ok(TauriBridgeManifest {
        schema_version: KAIN_TAURI_BRIDGE_SCHEMA_VERSION,
        app_id: config.app_id.clone(),
        app_name: config.app_name.clone(),
        window_label: config.window_label.clone(),
        window_title: config.window_title.clone(),
        frontend: config.frontend.clone(),
        runtime_sidecars: config.runtime_sidecars.clone(),
        enabled_plugin_presets,
        capability_presets,
        permission_presets,
        capability_identifiers,
        command_mappings,
        reflection: build_tauri_reflection_metadata(
            config.compiler_reflection_payload.as_ref(),
            &config.host_type_registry,
        ),
        supported_namespaces: KAIN_TAURI_SUPPORTED_NAMESPACES
            .iter()
            .map(|value| value.to_string())
            .collect(),
        invoke_command: KAIN_TAURI_BRIDGE_INVOKE_COMMAND.to_string(),
        hot_reload_event: KAIN_TAURI_HOT_RELOAD_EVENT.to_string(),
    })
}

pub fn patch_hybrid_wasm_reference(source: String, wasm_file_name: &str) -> String {
    let wasm_url_expression = format!(
        "new URL('{wasm_file_name}', document.currentScript?.src ?? window.location.href).toString()"
    );

    source
        .replace("'main.wasm'", &wasm_url_expression)
        .replace("\"main.wasm\"", &wasm_url_expression)
}

pub fn render_tauri_project_files(
    bridge_manifest: &TauriBridgeManifest,
    render_config: &TauriProjectRenderConfig,
) -> Result<RenderedTauriProjectFiles, String> {
    let mut files = BTreeMap::new();
    files.insert(
        "src-tauri/Cargo.toml".to_string(),
        render_src_tauri_cargo_toml(bridge_manifest, render_config),
    );
    files.insert(
        "src-tauri/build.rs".to_string(),
        render_src_tauri_build_rs().to_string(),
    );
    files.insert(
        "src-tauri/src/main.rs".to_string(),
        render_src_tauri_main_rs().to_string(),
    );
    files.insert(
        "src-tauri/src/host.rs".to_string(),
        render_src_tauri_host_rs(bridge_manifest, render_config)?,
    );
    files.insert(
        "src-tauri/permissions/kain-bridge.toml".to_string(),
        render_kain_bridge_permission_toml().to_string(),
    );
    files.insert(
        "src-tauri/tauri.conf.json".to_string(),
        render_tauri_conf_json(bridge_manifest, render_config)?,
    );

    let capabilities = render_capability_files(bridge_manifest, render_config)?;
    for capability in &capabilities {
        files.insert(
            capability.relative_path.clone(),
            capability.contents.clone(),
        );
    }

    Ok(RenderedTauriProjectFiles {
        files,
        capabilities,
    })
}

pub fn render_frontend_index_html(
    window_title: &str,
    entry_js_file_name: &str,
    bridge_js_file_name: &str,
) -> String {
    format!(
        "<!doctype html>\n<html lang=\"en\">\n<head>\n  <meta charset=\"utf-8\">\n  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n  <title>{window_title}</title>\n</head>\n<body>\n  <main id=\"app\"></main>\n  <script src=\"./{bridge_js_file_name}\"></script>\n  <script type=\"module\" src=\"./{entry_js_file_name}\"></script>\n</body>\n</html>\n"
    )
}

pub fn render_frontend_bridge_js(bridge_manifest: &TauriBridgeManifest) -> Result<String, String> {
    let manifest_json = serde_json::to_string_pretty(bridge_manifest)
        .map_err(|err| format!("Failed to serialize embedded Tauri bridge manifest: {err}"))?;

    Ok(format!(
        "const bridgeManifest = {manifest_json};\n\nconst namespaceBindings = {{\n  app: () => window.__TAURI__?.app,\n  window: () => window.__TAURI__?.window,\n  webview: () => window.__TAURI__?.webview,\n  event: () => window.__TAURI__?.event,\n  path: () => window.__TAURI__?.path,\n  fs: () => window.__TAURI__?.fs,\n  dialog: () => window.__TAURI__?.dialog,\n  shell: () => window.__TAURI__?.shell,\n  process: () => window.__TAURI__?.process,\n  menu: () => window.__TAURI__?.menu,\n  tray: () => window.__TAURI__?.tray,\n  clipboard: () => window.__TAURI__?.clipboardManager,\n  notification: () => window.__TAURI__?.notification,\n  opener: () => window.__TAURI__?.opener,\n  store: () => window.__TAURI__?.store,\n  sql: () => window.__TAURI__?.sql,\n  http: () => window.__TAURI__?.http,\n  updater: () => window.__TAURI__?.updater,\n  'global-shortcut': () => window.__TAURI__?.globalShortcut,\n}};\n\nfunction resolveMethodTarget(root, methodPath) {{\n  if (!root || !methodPath) {{\n    return root;\n  }}\n  return methodPath.split('.').reduce((current, segment) => current?.[segment], root);\n}}\n\nfunction normalizeArgs(args) {{\n  if (args === undefined || args === null) {{\n    return [];\n  }}\n  if (Array.isArray(args)) {{\n    return args;\n  }}\n  return [args];\n}}\n\nasync function invokeRustBridge(request) {{\n  const invoke = window.__TAURI__?.core?.invoke;\n  if (typeof invoke !== 'function') {{\n    throw new Error('window.__TAURI__.core.invoke is unavailable');\n  }}\n  return invoke(bridgeManifest.invoke_command, {{ request }});\n}}\n\nasync function invokeNamespace(request) {{\n  const resolveNamespace = namespaceBindings[request.namespace];\n  if (!resolveNamespace) {{\n    throw new Error(`Unsupported Tauri namespace '${{request.namespace}}'`);\n  }}\n  const root = resolveNamespace();\n  const target = resolveMethodTarget(root, request.method);\n  if (typeof target !== 'function') {{\n    throw new Error(`Method '${{request.namespace}}.${{request.method}}' is unavailable`);\n  }}\n  return target(...normalizeArgs(request.args));\n}}\n\nasync function dispatch(request) {{\n  const normalized = request ?? {{}};\n  if (!normalized.namespace) {{\n    throw new Error('Kain Tauri bridge requests require a namespace');\n  }}\n  if (!normalized.method) {{\n    throw new Error('Kain Tauri bridge requests require a method');\n  }}\n  if (normalized.namespace === 'host' || normalized.namespace === 'runtime') {{\n    return invokeRustBridge(normalized);\n  }}\n  return invokeNamespace(normalized);\n}}\n\nasync function installHotReloadListener() {{\n  const listen = window.__TAURI__?.event?.listen;\n  if (typeof listen !== 'function') {{\n    return;\n  }}\n  await listen(bridgeManifest.hot_reload_event, (event) => {{\n    document.dispatchEvent(new CustomEvent('kain:tauri:runtime-reload', {{ detail: event.payload }}));\n    if (!event.payload || event.payload.strategy !== 'manual') {{\n      window.location.reload();\n    }}\n  }});\n}}\n\nwindow.__KAIN_TAURI_BRIDGE__ = {{\n  manifest: bridgeManifest,\n  dispatch,\n  invoke: dispatch,\n  call: dispatch,\n  hotReloadEvent: bridgeManifest.hot_reload_event,\n}};\nwindow.KainTauriBridge = window.__KAIN_TAURI_BRIDGE__;\ndocument.dispatchEvent(new CustomEvent('kain:tauri:bridge-ready', {{ detail: bridgeManifest }}));\ninstallHotReloadListener().catch((error) => {{\n  console.error('[kain-tauri-bridge] failed to install hot reload listener', error);\n}});\n"
    ))
}

fn render_src_tauri_cargo_toml(
    bridge_manifest: &TauriBridgeManifest,
    render_config: &TauriProjectRenderConfig,
) -> String {
    let mut dependency_lines = vec![
        "serde = { version = \"1\", features = [\"derive\"] }".to_string(),
        "serde_json = \"1\"".to_string(),
        "tauri = { version = \"2\" }".to_string(),
    ];

    for preset in &bridge_manifest.enabled_plugin_presets {
        if let Some(line) = preset.dependency_line() {
            dependency_lines.push(line.to_string());
        }
    }
    dependency_lines.sort();
    dependency_lines.dedup();

    let dependency_block = dependency_lines
        .iter()
        .map(|line| format!("{line}\n"))
        .collect::<String>();

    format!(
        "[package]\nname = \"{}\"\nversion = \"{}\"\nedition = \"2021\"\npublish = false\n\n[build-dependencies]\ntauri-build = {{ version = \"2\" }}\n\n[dependencies]\n{}\n",
        render_config.cargo_package_name,
        render_config.cargo_package_version,
        dependency_block,
    )
}

fn render_src_tauri_build_rs() -> &'static str {
    "fn main() {\n    tauri_build::try_build(\n        tauri_build::Attributes::new()\n            .app_manifest(tauri_build::AppManifest::new().commands(&[\"kain_bridge_dispatch\"])),\n    )\n    .expect(\"failed to build generated Kain Tauri host\");\n}\n"
}

fn render_src_tauri_main_rs() -> &'static str {
    "mod host;\n\nfn main() {\n    host::run();\n}\n"
}

fn render_src_tauri_host_rs(
    bridge_manifest: &TauriBridgeManifest,
    render_config: &TauriProjectRenderConfig,
) -> Result<String, String> {
    let plugin_registration_lines = bridge_manifest
        .enabled_plugin_presets
        .iter()
        .filter_map(|preset| preset.registration_line())
        .collect::<Vec<_>>();
    let plugin_registration_block = if plugin_registration_lines.is_empty() {
        String::new()
    } else {
        format!("{}\n", plugin_registration_lines.join("\n    "))
    };
    let manifest_json = serde_json::to_string_pretty(bridge_manifest)
        .map_err(|err| format!("Failed to serialize embedded generated bridge manifest: {err}"))?;

    Ok(format!(
        "use std::collections::BTreeMap;\nuse std::fs;\nuse std::path::{{Path, PathBuf}};\nuse std::thread;\nuse std::time::{{Duration, UNIX_EPOCH}};\n\nuse serde::{{Deserialize, Serialize}};\nuse serde_json::{{json, Value}};\nuse tauri::{{AppHandle, Emitter, WebviewWindow}};\n\nconst BRIDGE_MANIFEST_RELATIVE_PATH: &str = \"{}\";\nconst HOT_RELOAD_EVENT: &str = \"{}\";\nconst BRIDGE_READY_EVENT: &str = \"{}\";\nconst WATCH_DIRECTORIES: &[&str] = &[\"../frontend\", \"../generated\", \"../config\", \"../state\"];\nconst GENERATED_BRIDGE_MANIFEST: &str = r#\"{}\"#;\n\n#[derive(Debug, Clone, Serialize, Deserialize)]\nstruct BridgeRequest {{\n    namespace: String,\n    method: String,\n    #[serde(default)]\n    args: Value,\n    #[serde(default)]\n    correlation_id: Option<String>,\n}}\n\n#[tauri::command]\nfn kain_bridge_dispatch(\n    app: AppHandle,\n    window: WebviewWindow,\n    request: BridgeRequest,\n) -> Result<Value, String> {{\n    match request.namespace.as_str() {{\n        \"runtime\" => dispatch_runtime_request(&app, &request),\n        \"host\" => dispatch_host_request(&app, &window, &request),\n        other => Err(format!(\"Rust bridge only handles host/runtime namespaces, received '{{other}}'\")),\n    }}\n}}\n\npub fn run() {{\n    let builder = tauri::Builder::default();\n    {plugin_registration_block}    builder\n        .setup(|app| {{\n            start_hot_reload_watcher(app.handle().clone());\n            let payload = json!({{\n                \"window_label\": \"{}\",\n                \"window_title\": \"{}\",\n                \"bundle_identifier\": \"{}\",\n            }});\n            let _ = app.emit(BRIDGE_READY_EVENT, payload);\n            Ok(())\n        }})\n        .invoke_handler(tauri::generate_handler![kain_bridge_dispatch])\n        .run(tauri::generate_context!())\n        .expect(\"error while running generated Kain Tauri host\");\n}}\n\nfn dispatch_runtime_request(app: &AppHandle, request: &BridgeRequest) -> Result<Value, String> {{\n    match request.method.as_str() {{\n        \"reload\" | \"runtime.reload\" => {{\n            app.emit(HOT_RELOAD_EVENT, json!({{\"reason\": \"runtime-command\", \"strategy\": \"full-reload\"}}))\n                .map_err(|err| err.to_string())?;\n            Ok(json!({{ \"ok\": true, \"status\": \"reloaded\" }}))\n        }}\n        _ => Ok(json!({{\n            \"ok\": true,\n            \"namespace\": \"runtime\",\n            \"method\": request.method,\n            \"status\": \"accepted\",\n        }})),\n    }}\n}}\n\nfn dispatch_host_request(\n    app: &AppHandle,\n    window: &WebviewWindow,\n    request: &BridgeRequest,\n) -> Result<Value, String> {{\n    match request.method.as_str() {{\n        \"ping\" => Ok(json!({{\n            \"ok\": true,\n            \"namespace\": \"host\",\n            \"method\": \"ping\",\n            \"window_label\": \"{}\",\n        }})),\n        \"bridge.manifest\" => load_bridge_manifest(),\n        \"bridge.reflection\" => {{\n            let manifest = load_bridge_manifest()?;\n            Ok(manifest\n                .get(\"reflection\")\n                .cloned()\n                .unwrap_or(Value::Null))\n        }}\n        \"event.emit\" => {{\n            let event_name = request\n                .args\n                .get(\"event\")\n                .and_then(Value::as_str)\n                .ok_or_else(|| \"host.event.emit requires an 'event' string\".to_string())?;\n            let payload = request.args.get(\"payload\").cloned().unwrap_or(Value::Null);\n            if let Some(target) = request.args.get(\"target\").and_then(Value::as_str) {{\n                app.emit_to(target, event_name, payload.clone())\n                    .map_err(|err| err.to_string())?;\n            }} else {{\n                app.emit(event_name, payload.clone())\n                    .map_err(|err| err.to_string())?;\n            }}\n            Ok(json!({{ \"ok\": true, \"event\": event_name, \"payload\": payload }}))\n        }}\n        \"window.reload\" => {{\n            window\n                .eval(\"window.location.reload();\")\n                .map_err(|err| err.to_string())?;\n            Ok(json!({{ \"ok\": true, \"status\": \"window-reloaded\" }}))\n        }}\n        _ => Err(format!(\"Unsupported generated host bridge method '{{}}'\", request.method)),\n    }}\n}}\n\nfn load_bridge_manifest() -> Result<Value, String> {{\n    let manifest_path = PathBuf::from(env!(\"CARGO_MANIFEST_DIR\")).join(BRIDGE_MANIFEST_RELATIVE_PATH);\n    let manifest_source = fs::read_to_string(&manifest_path).unwrap_or_else(|_| GENERATED_BRIDGE_MANIFEST.to_string());\n    serde_json::from_str(&manifest_source).map_err(|err| {{\n        format!(\"Failed to parse generated bridge manifest {{}}: {{err}}\", manifest_path.display())\n    }})\n}}\n\nfn start_hot_reload_watcher(app: AppHandle) {{\n    thread::spawn(move || {{\n        let roots = watch_roots();\n        let mut previous = snapshot_watch_roots(&roots);\n        loop {{\n            thread::sleep(Duration::from_millis(350));\n            let current = snapshot_watch_roots(&roots);\n            if current != previous {{\n                let changed_paths = current\n                    .iter()\n                    .filter(|(path, fingerprint)| previous.get(*path) != Some(*fingerprint))\n                    .map(|(path, _)| path.clone())\n                    .collect::<Vec<_>>();\n                previous = current;\n                let _ = app.emit(\n                    HOT_RELOAD_EVENT,\n                    json!({{\n                        \"reason\": \"file-watch\",\n                        \"paths\": changed_paths,\n                        \"strategy\": \"full-reload\",\n                    }}),\n                );\n            }}\n        }}\n    }});\n}}\n\nfn watch_roots() -> Vec<PathBuf> {{\n    let manifest_dir = PathBuf::from(env!(\"CARGO_MANIFEST_DIR\"));\n    WATCH_DIRECTORIES\n        .iter()\n        .map(|relative| manifest_dir.join(relative))\n        .collect()\n}}\n\nfn snapshot_watch_roots(roots: &[PathBuf]) -> BTreeMap<String, u128> {{\n    let mut fingerprints = BTreeMap::new();\n    for root in roots {{\n        snapshot_path_recursive(root, &mut fingerprints);\n    }}\n    fingerprints\n}}\n\nfn snapshot_path_recursive(path: &Path, fingerprints: &mut BTreeMap<String, u128>) {{\n    let Ok(metadata) = fs::metadata(path) else {{\n        return;\n    }};\n\n    if metadata.is_file() {{\n        let fingerprint = metadata\n            .modified()\n            .ok()\n            .and_then(|modified| modified.duration_since(UNIX_EPOCH).ok())\n            .map(|duration| duration.as_millis())\n            .unwrap_or_default();\n        fingerprints.insert(path.display().to_string(), fingerprint);\n        return;\n    }}\n\n    if !metadata.is_dir() {{\n        return;\n    }}\n\n    let Ok(entries) = fs::read_dir(path) else {{\n        return;\n    }};\n    for entry in entries.flatten() {{\n        snapshot_path_recursive(&entry.path(), fingerprints);\n    }}\n}}\n",
        render_config.bridge_manifest_relative_path,
        bridge_manifest.hot_reload_event,
        KAIN_TAURI_BRIDGE_READY_EVENT,
        escape_rust_raw_string(&manifest_json),
        render_config.window_label,
        render_config.window_title,
        render_config.bundle_identifier,
        render_config.window_label,
    ))
}

fn render_kain_bridge_permission_toml() -> &'static str {
    "[[permission]]\nidentifier = \"kain-bridge\"\ndescription = \"Allows the generated Kain bridge dispatcher command.\"\ncommands.allow = [\"kain_bridge_dispatch\"]\n"
}

fn render_tauri_conf_json(
    bridge_manifest: &TauriBridgeManifest,
    render_config: &TauriProjectRenderConfig,
) -> Result<String, String> {
    let config = json!({
        "productName": bridge_manifest.app_name,
        "version": render_config.cargo_package_version,
        "identifier": render_config.bundle_identifier,
        "build": {
            "beforeBuildCommand": "",
            "beforeDevCommand": "",
            "frontendDist": render_config.frontend_dist_relative_path,
        },
        "app": {
            "withGlobalTauri": true,
            "windows": [
                {
                    "label": render_config.window_label,
                    "title": render_config.window_title,
                    "width": render_config.initial_window_size[0],
                    "height": render_config.initial_window_size[1],
                    "resizable": true,
                    "visible": true,
                }
            ]
        },
        "bundle": {
            "active": true,
            "targets": "all"
        }
    });

    serde_json::to_string_pretty(&config)
        .map_err(|err| format!("Failed to serialize generated tauri.conf.json: {err}"))
}

fn render_capability_files(
    bridge_manifest: &TauriBridgeManifest,
    render_config: &TauriProjectRenderConfig,
) -> Result<Vec<RenderedTauriCapability>, String> {
    let permission_entries = bridge_manifest
        .permission_presets
        .iter()
        .map(|preset| Value::String(preset.identifier().to_string()))
        .collect::<Vec<_>>();

    let mut capabilities = Vec::new();
    for preset in &bridge_manifest.capability_presets {
        let identifier = preset.identifier(&render_config.window_label);
        let capability_json = json!({
            "$schema": "../gen/schemas/desktop-schema.json",
            "identifier": identifier,
            "description": preset.description(&render_config.window_label),
            "windows": [render_config.window_label],
            "permissions": permission_entries,
        });
        let contents = serde_json::to_string_pretty(&capability_json).map_err(|err| {
            format!(
                "Failed to serialize generated Tauri capability '{}': {err}",
                identifier
            )
        })?;
        capabilities.push(RenderedTauriCapability {
            identifier: identifier.clone(),
            relative_path: format!("src-tauri/capabilities/{identifier}.json"),
            contents,
        });
    }
    Ok(capabilities)
}

fn dedupe_presets(mut presets: Vec<TauriPluginPreset>) -> Vec<TauriPluginPreset> {
    presets.sort();
    presets.dedup();
    presets
}

fn dedupe_capability_presets(
    mut presets: Vec<TauriCapabilityPreset>,
) -> Vec<TauriCapabilityPreset> {
    presets.sort();
    presets.dedup();
    presets
}

fn dedupe_permission_presets(
    mut presets: Vec<TauriPermissionPreset>,
) -> Vec<TauriPermissionPreset> {
    presets.sort();
    presets.dedup();
    presets
}

fn parse_tauri_namespace_and_method(value: &str) -> Result<(String, String), String> {
    let mut segments = value.splitn(2, '.');
    let namespace = segments
        .next()
        .map(normalize_tauri_namespace)
        .filter(|segment| !segment.is_empty())
        .ok_or_else(|| format!("Tauri bridge command '{value}' is missing a namespace"))?;
    let method = segments
        .next()
        .map(str::trim)
        .filter(|segment| !segment.is_empty())
        .ok_or_else(|| format!("Tauri bridge command '{value}' is missing a method"))?
        .to_string();
    Ok((namespace, method))
}

fn validate_enabled_command_namespaces(
    command_mappings: &[TauriCommandMapping],
    enabled_plugin_presets: &[TauriPluginPreset],
) -> Result<(), String> {
    let enabled_namespaces = enabled_namespace_set(enabled_plugin_presets);
    for mapping in command_mappings {
        if !enabled_namespaces.contains(mapping.namespace.as_str()) {
            return Err(format!(
                "Tauri command '{}' targets namespace '{}' but that namespace is not enabled for this scaffold",
                mapping.command_name, mapping.namespace
            ));
        }
    }
    Ok(())
}

fn enabled_namespace_set(enabled_plugin_presets: &[TauriPluginPreset]) -> BTreeSet<&'static str> {
    let mut namespaces = BTreeSet::from(["host", "runtime"]);
    for preset in enabled_plugin_presets {
        namespaces.insert(preset.namespace());
    }
    namespaces
}

fn is_supported_tauri_namespace(namespace: &str) -> bool {
    KAIN_TAURI_SUPPORTED_NAMESPACES
        .iter()
        .any(|candidate| *candidate == namespace)
}

fn normalize_tauri_namespace(value: &str) -> String {
    match value.trim().to_ascii_lowercase().replace('_', "-").as_str() {
        "clipboard-manager" => "clipboard".to_string(),
        "globalshortcut" => "global-shortcut".to_string(),
        normalized => normalized.to_string(),
    }
}

fn normalize_identifier_fragment(value: &str) -> String {
    value
        .chars()
        .map(|ch| match ch {
            'a'..='z' | '0'..='9' => ch,
            'A'..='Z' => ch.to_ascii_lowercase(),
            _ => '-',
        })
        .collect::<String>()
}

fn escape_rust_raw_string(value: &str) -> String {
    value.replace("\"#", "\\\"#")
}

#[cfg(test)]
mod tests {
    use super::*;
    use kain_ui::{
        UiRuntimeSystems, UiSurface, UiSurfaceCompositionMode, UiSurfaceRendererPreference,
    };

    fn sample_bridge_manifest() -> TauriBridgeManifest {
        build_tauri_bridge_manifest(&TauriBridgeManifestConfig {
            app_id: "ai.kain.sample".to_string(),
            app_name: "Sample".to_string(),
            window_label: "main".to_string(),
            window_title: "Sample".to_string(),
            frontend: TauriFrontendAssetManifest {
                root_dir: "frontend".to_string(),
                entry_html: "frontend/index.html".to_string(),
                descriptor_file: "frontend/hybrid_bundle.json".to_string(),
                js_bundle: "frontend/main.js".to_string(),
                ts_bundle: "frontend/main.ts".to_string(),
                wasm_bundle: "frontend/main.wasm".to_string(),
                bridge_js: "frontend/kain-bridge.js".to_string(),
            },
            runtime_sidecars: TauriRuntimeSidecarManifest {
                runtime_bundle: "generated/native_app_bundle.json".to_string(),
                runtime_contract: "generated/kain_runtime_contract.json".to_string(),
                runtime_compatibility: "generated/kain_runtime_compatibility.json".to_string(),
                realtime_bundle: "generated/kain_realtime_app_bundle.json".to_string(),
                shader_bundle: None,
                reflection_payload: Some("generated/kain_reflection_payload.json".to_string()),
                runtime_snapshot: "state/runtime_snapshot.json".to_string(),
                app_manifest: "config/app_manifest.json".to_string(),
            },
            plugin_presets: vec![
                TauriPluginPreset::App,
                TauriPluginPreset::Clipboard,
                TauriPluginPreset::GlobalShortcut,
            ],
            capability_presets: default_tauri_capability_presets(),
            permission_presets: Vec::new(),
            commands: vec![UiCommandDescriptor {
                name: "tauri.clipboard.readText".to_string(),
                label: "Read Clipboard".to_string(),
                description: None,
                keywords: Vec::new(),
                category: None,
                shortcut: None,
                effect: UiCommandEffectKind::ExternalEffect,
            }],
            compiler_reflection_payload: None,
            host_type_registry: TypeRegistry::default(),
        })
        .expect("bridge manifest should build")
    }

    #[test]
    fn retarget_ui_runtime_bundle_promotes_tauri_surface_hosts() {
        let mut build_output = UiBuildOutput::default();
        build_output.systems = UiRuntimeSystems::default();
        build_output.systems.surfaces = vec![
            UiSurface {
                id: "document".to_string(),
                kind: UiSurfaceKind::Canvas,
                node: kain_ui::UiNodeId(1),
                title: None,
                renderer_preference: UiSurfaceRendererPreference::Auto,
                composition_mode: UiSurfaceCompositionMode::Host,
                preferred_host_backend: UiHostBackendKind::Qt,
                preferred_layout_engine: UiLayoutEngineKind::Yoga,
                preferred_render_engine: UiRenderEngineKind::Native,
                gpu_backing_required: false,
                shader: None,
            },
            UiSurface {
                id: "viewport".to_string(),
                kind: UiSurfaceKind::Viewport3D,
                node: kain_ui::UiNodeId(2),
                title: None,
                renderer_preference: UiSurfaceRendererPreference::Wgpu,
                composition_mode: UiSurfaceCompositionMode::Viewport,
                preferred_host_backend: UiHostBackendKind::Qt,
                preferred_layout_engine: UiLayoutEngineKind::Yoga,
                preferred_render_engine: UiRenderEngineKind::Wgpu,
                gpu_backing_required: true,
                shader: None,
            },
        ];
        let bundle = UiRuntimeBundle {
            schema_version: 1,
            metadata: UiRuntimeMetadata::default(),
            output: build_output,
            native_projection: Default::default(),
        };

        let retargeted = retarget_ui_runtime_bundle_for_tauri(&bundle);
        assert_eq!(
            retargeted.metadata.preferred_shell_host_backend,
            UiHostBackendKind::Tauri
        );
        assert_eq!(
            retargeted.output.systems.surfaces[0].preferred_render_engine,
            UiRenderEngineKind::Browser
        );
        assert_eq!(
            retargeted.output.systems.surfaces[1].preferred_render_engine,
            UiRenderEngineKind::Wgpu
        );
        assert_eq!(
            retargeted.output.systems.surfaces[1].preferred_host_backend,
            UiHostBackendKind::Tauri
        );
    }

    #[test]
    fn command_mapping_rejects_unknown_tauri_namespaces() {
        let result = build_tauri_command_mappings(&[UiCommandDescriptor {
            name: "tauri.unknown_namespace.run".to_string(),
            label: "Broken".to_string(),
            description: None,
            keywords: Vec::new(),
            category: None,
            shortcut: None,
            effect: UiCommandEffectKind::ExternalEffect,
        }]);
        assert!(result.is_err());
    }

    #[test]
    fn bridge_manifest_derives_permissions_from_enabled_plugins() {
        let manifest = sample_bridge_manifest();
        assert!(manifest
            .permission_presets
            .contains(&TauriPermissionPreset::KainBridge));
        assert!(manifest
            .permission_presets
            .contains(&TauriPermissionPreset::CoreDefault));
        assert!(manifest
            .permission_presets
            .contains(&TauriPermissionPreset::ClipboardAllowReadText));
        assert!(manifest
            .permission_presets
            .contains(&TauriPermissionPreset::GlobalShortcutAllowRegister));
    }

    #[test]
    fn render_tauri_project_files_emits_host_scaffold() {
        let manifest = sample_bridge_manifest();
        let rendered = render_tauri_project_files(
            &manifest,
            &TauriProjectRenderConfig {
                cargo_package_name: "sample-tauri".to_string(),
                cargo_package_version: "0.1.0".to_string(),
                bundle_identifier: "ai.kain.sample".to_string(),
                window_label: "main".to_string(),
                window_title: "Sample".to_string(),
                initial_window_size: [1440.0, 920.0],
                frontend_dist_relative_path: "../frontend".to_string(),
                bridge_manifest_relative_path: "../generated/kain_tauri_bridge_manifest.json"
                    .to_string(),
            },
        )
        .expect("rendered project files");

        assert!(rendered.files.contains_key("src-tauri/Cargo.toml"));
        assert!(rendered.files.contains_key("src-tauri/src/host.rs"));
        assert!(rendered
            .files
            .contains_key("src-tauri/permissions/kain-bridge.toml"));
        assert!(rendered
            .files
            .contains_key("src-tauri/capabilities/kain-main.json"));
    }
}
