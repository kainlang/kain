use std::ffi::OsStr;
use std::fs;
use std::path::{Component, Path, PathBuf};
use std::process::Command;
use std::time::{SystemTime, UNIX_EPOCH};

use crate::compute_residency::{
    write_compute_residency_sidecars, COMPUTE_RESIDENCY_ENV_VAR, COMPUTE_RESIDENCY_FILE_NAME,
};
use crate::{
    apply_active_world_selection_to_runtime_contract, resolve_root_component_name, DriverSession,
    RustBundleOutput, ShaderArtifactBundleOutput,
};
use kain_core::error::KainError;
use kain_core::{
    build_ui_output_from_source, realtime_app_bundle_to_json, runtime_contract_bundle_to_json,
    CompileTarget, RealtimeAppBundle, RealtimeAssetBinding, RuntimeCapability,
    RuntimeCompatibilityMetadata, RuntimeContractBundle, RuntimePlatformAvailabilityMetadata,
    RuntimeReflectionPayload, RuntimeServiceBinding, RuntimeVersionRecord, RuntimeWorldContract,
};
use kain_ui::{
    ui_runtime_bundle_from_output, ui_runtime_bundle_to_json, UiBuildOutput, UiRuntimeBundle,
    UiRuntimeMetadata,
};
use sha2::{Digest, Sha256};

const NATIVE_APP_RUNTIME_BUNDLE_FILE_NAME: &str = "native_app_bundle.json";
const NATIVE_APP_RUNTIME_CONTRACT_FILE_NAME: &str = "kain_runtime_contract.json";
const NATIVE_APP_RUNTIME_COMPATIBILITY_FILE_NAME: &str = "kain_runtime_compatibility.json";
const NATIVE_APP_REALTIME_BUNDLE_FILE_NAME: &str = "kain_realtime_app_bundle.json";
const NATIVE_APP_SHADER_BUNDLE_FILE_NAME: &str = "kain_shader_bundle.json";
const NATIVE_RUNTIME_VERSION_METADATA_FILE_NAME: &str = "kain_runtime_version.json";
const NATIVE_RUNTIME_REFLECTION_PAYLOAD_FILE_NAME: &str = "kain_reflection_payload.json";
const NATIVE_APP_C_FFI_PACKAGE_MANIFEST_FILE_NAME: &str = "kain_c_host_bridges.json";
const NATIVE_APP_CONFIG_DIR_NAME: &str = "config";
const NATIVE_APP_STATE_DIR_NAME: &str = "state";
const NATIVE_APP_MANIFEST_FILE_NAME: &str = "app_manifest.json";
const NATIVE_APP_RUNTIME_SNAPSHOT_FILE_NAME: &str = "runtime_snapshot.json";
const HOT_RELOAD_COMPATIBILITY_LANES: &[&str] = &[
    "cold-start",
    "noop",
    "presentation-only",
    "structural-migrate",
    "quiesce-and-migrate",
    "frame-boundary-gpu-swap",
    "restart-with-restore",
];

#[derive(Debug, Clone)]
pub struct NativeAppBundleConfig {
    pub app_name: Option<String>,
    pub window_title: Option<String>,
    pub root_component: Option<String>,
    pub source_file_name: Option<String>,
    pub source_root: Option<PathBuf>,
    pub initial_window_size: [f32; 2],
    pub include_spirv: bool,
}

impl Default for NativeAppBundleConfig {
    fn default() -> Self {
        Self {
            app_name: None,
            window_title: None,
            root_component: None,
            source_file_name: Some("app.kn".to_string()),
            source_root: None,
            initial_window_size: [1440.0, 920.0],
            include_spirv: true,
        }
    }
}

/// Runtime version metadata loaded from the native runtime manifest.
#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
pub struct RuntimeVersionMetadata {
    pub runtime_major: u32,
    pub runtime_minor: u32,
    pub runtime_patch: u32,
    pub abi_major: u32,
    pub abi_minor: u32,
    pub abi_patch: u32,
    pub runtime_version_string: String,
    pub abi_version_string: String,
    pub compatibility_class: String,
    pub runtime_lane: String,
    pub target_platforms: Vec<String>,
    pub active_platforms: Vec<String>,
}

impl RuntimeVersionMetadata {
    /// Load runtime version metadata from the canonical native runtime manifest.
    pub fn load_from_runtime_manifest() -> Result<Self, KainError> {
        let manifest_path = find_native_runtime_manifest()
            .ok_or_else(|| {
                KainError::runtime(
                    "Could not locate runtime/native_core_runtime.toml or a compatible runtime/native_runtime.toml"
                )
            })?;

        let manifest_source = fs::read_to_string(&manifest_path).map_err(|err| {
            KainError::runtime(format!(
                "Failed to read runtime manifest {}: {}",
                manifest_path.display(),
                err
            ))
        })?;

        #[derive(serde::Deserialize)]
        struct VersionSection {
            runtime_major: u32,
            runtime_minor: u32,
            runtime_patch: u32,
            abi_major: u32,
            abi_minor: u32,
            abi_patch: u32,
        }

        #[derive(serde::Deserialize)]
        struct MetadataSection {
            compatibility_class: String,
            runtime_lane: String,
            target_platforms: Vec<String>,
            active_platforms: Vec<String>,
        }

        #[derive(serde::Deserialize)]
        struct RuntimeManifest {
            version: VersionSection,
            metadata: MetadataSection,
        }

        let manifest: RuntimeManifest = toml::from_str(&manifest_source).map_err(|err| {
            KainError::runtime(format!(
                "Failed to parse runtime manifest {}: {}",
                manifest_path.display(),
                err
            ))
        })?;

        let runtime_version_string = format!(
            "{}.{}.{}",
            manifest.version.runtime_major,
            manifest.version.runtime_minor,
            manifest.version.runtime_patch
        );

        let abi_version_string = format!(
            "{}.{}.{}",
            manifest.version.abi_major, manifest.version.abi_minor, manifest.version.abi_patch
        );

        Ok(Self {
            runtime_major: manifest.version.runtime_major,
            runtime_minor: manifest.version.runtime_minor,
            runtime_patch: manifest.version.runtime_patch,
            abi_major: manifest.version.abi_major,
            abi_minor: manifest.version.abi_minor,
            abi_patch: manifest.version.abi_patch,
            runtime_version_string,
            abi_version_string,
            compatibility_class: manifest.metadata.compatibility_class,
            runtime_lane: manifest.metadata.runtime_lane,
            target_platforms: manifest.metadata.target_platforms,
            active_platforms: manifest.metadata.active_platforms,
        })
    }
}

fn find_native_runtime_manifest() -> Option<PathBuf> {
    // Prefer the CLI/runtime bundle env var, but keep the legacy driver name for compatibility.
    for env_var in ["KAIN_RUNTIME_MANIFEST_PATH", "KAIN_RUNTIME_MANIFEST"] {
        if let Ok(explicit) = std::env::var(env_var) {
            let candidate = PathBuf::from(explicit);
            if candidate.exists() {
                return Some(candidate);
            }
        }
    }

    // Try relative to current directory
    if let Ok(mut dir) = std::env::current_dir() {
        for _ in 0..10 {
            for suffix in native_runtime_manifest_candidate_suffixes() {
                let candidate = dir.join(suffix);
                if candidate.exists() {
                    return Some(candidate);
                }
            }
            match dir.parent() {
                Some(parent) => dir = parent.to_path_buf(),
                None => break,
            }
        }
    }

    // Try relative to executable
    if let Ok(exe_path) = std::env::current_exe() {
        if let Some(mut dir) = exe_path.parent().map(|p| p.to_path_buf()) {
            loop {
                for suffix in native_runtime_manifest_candidate_suffixes() {
                    let candidate = dir.join(suffix);
                    if candidate.exists() {
                        return Some(candidate);
                    }
                }
                if !dir.pop() {
                    break;
                }
            }
        }
    }

    None
}

fn native_runtime_manifest_candidate_suffixes() -> &'static [&'static str] {
    &[
        "runtime/native_core_runtime.toml",
        "runtime/native_runtime.toml",
        "runtime/native/runtime.toml",
    ]
}

#[derive(Debug, Clone)]
pub struct NativeAppMetadata {
    pub app_name: String,
    pub window_title: String,
    pub root_component: String,
    pub source_file_name: String,
    pub source_root: Option<PathBuf>,
    pub initial_window_size: [f32; 2],
}

#[derive(Debug, Clone)]
pub struct NativeAppBundle {
    pub metadata: NativeAppMetadata,
    pub runtime_contract: RuntimeContractBundle,
    pub realtime: RealtimeAppBundle,
    pub shader_bundle: Option<ShaderArtifactBundleOutput>,
    pub ui: UiBuildOutput,
    pub ui_runtime_bundle: UiRuntimeBundle,
    pub rust: RustBundleOutput,
    pub runtime_version: Option<RuntimeVersionMetadata>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum NativeAppRuntimeDependency {
    Path(PathBuf),
    Version(String),
}

#[derive(Debug, Clone)]
pub struct NativeAppMaterializationConfig {
    pub project_dir: PathBuf,
    pub runtime_crate_name: String,
    pub runtime_dependency: NativeAppRuntimeDependency,
    pub artifact_output_dir: PathBuf,
    pub build_executable: bool,
    pub release: bool,
    pub executable_output_dir: Option<PathBuf>,
    pub cargo_target_dir: Option<PathBuf>,
    pub gpu_runtime_cargo_target_dir: Option<PathBuf>,
    pub launcher_entrypoint: NativeAppLauncherEntrypoint,
    pub host_sidecars: Vec<NativeAppHostSidecarBinding>,
}

#[derive(Debug, Clone)]
pub struct NativeAppMaterializedPaths {
    pub project_dir: PathBuf,
    pub manifest_path: PathBuf,
    pub main_rs_path: PathBuf,
    pub source_copy_path: PathBuf,
    pub artifact_paths: Vec<PathBuf>,
    pub executable_path: Option<PathBuf>,
}

#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
struct NativeAppManifest {
    app_id: String,
    name: String,
    version: String,
    window_title: String,
    root_component: String,
    active_world: Option<String>,
    layout_id: String,
    required_runtime_capabilities: Vec<String>,
    target_outputs: Vec<String>,
    manifests: NativeAppManifestPaths,
    runtime_sidecars: NativeAppRuntimeSidecars,
    launcher: NativeAppLauncherMetadata,
    hot_reload: NativeAppHotReloadMetadata,
    host_bridges: Vec<kain_c_ffi::PackagedBridgeImport>,
    host_sidecars: Vec<NativeAppManifestHostSidecar>,
}

#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
struct NativeAppManifestPaths {
    host_bridges: String,
}

#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
struct NativeAppRuntimeSidecars {
    runtime_bundle: String,
    runtime_contract: String,
    runtime_compatibility: String,
    realtime_bundle: String,
    shader_bundle: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    reflection_payload: Option<String>,
    runtime_snapshot: String,
}

#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
struct NativeAppManifestHostSidecar {
    packaged_file_name: String,
    env_var: Option<String>,
}

#[derive(Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
struct NativeAppLauncherMetadata {
    kind: String,
    function_name: String,
}

#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
struct NativeAppHotReloadPolicy {
    product_mode_default: bool,
    devtools_opt_in: bool,
    preserve_focus: bool,
    preserve_selection: bool,
    preserve_docking: bool,
    preserve_overlays: bool,
    preserve_motion_policy: bool,
    preserve_animation_state: bool,
    preserve_signal_values: bool,
    preserve_session_state: bool,
}

#[derive(Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
struct NativeAppHotReloadIdentity {
    app_id: String,
    name: String,
    window_title: String,
    root_component: String,
    active_world: Option<String>,
    layout_id: String,
}

#[derive(Debug, Clone, Default, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
struct NativeAppHotReloadParticipantField {
    name: String,
    type_name: String,
}

#[derive(Debug, Clone, Default, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
struct NativeAppHotReloadWorldParticipant {
    name: String,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    state_fields: Vec<NativeAppHotReloadParticipantField>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    surface_kinds: Vec<String>,
    migration_mode: String,
}

#[derive(Debug, Clone, Default, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
struct NativeAppHotReloadActorParticipant {
    name: String,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    state_type: Option<String>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    state_fields: Vec<NativeAppHotReloadParticipantField>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    message_types: Vec<String>,
    migration_mode: String,
    quiesce_boundary: String,
    mailbox_transfer: String,
}

#[derive(Debug, Clone, Default, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
struct NativeAppHotReloadGpuHooks {
    swap_boundary: String,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    shader_bundle_role: Option<String>,
    resource_graph_reload: String,
}

#[derive(Debug, Clone, Default, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub(crate) struct NativeAppHotReloadParticipants {
    package_surface: String,
    default_state_migration: String,
    default_actor_quiesce: String,
    #[serde(default)]
    default_restart_mode: String,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    compatibility_lanes: Vec<String>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    worlds: Vec<NativeAppHotReloadWorldParticipant>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    actors: Vec<NativeAppHotReloadActorParticipant>,
    #[serde(default)]
    gpu_hooks: NativeAppHotReloadGpuHooks,
}

#[derive(Debug, Clone, Default, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
struct NativeAppHotReloadTransition {
    class: String,
    restart_required: bool,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    reasons: Vec<String>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    actions: Vec<String>,
}

#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
struct NativeAppHotReloadArtifact {
    role: String,
    path: String,
    fingerprint: String,
    byte_length: usize,
}

#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
struct NativeAppHotReloadMetadata {
    summary: String,
    launcher: NativeAppLauncherMetadata,
    policy: NativeAppHotReloadPolicy,
    identity: NativeAppHotReloadIdentity,
    #[serde(default)]
    participants: NativeAppHotReloadParticipants,
    #[serde(default)]
    transition: NativeAppHotReloadTransition,
    artifact_fingerprints: Vec<NativeAppHotReloadArtifact>,
    materialization_fingerprint: String,
    previous_materialization_fingerprint: Option<String>,
    changed_artifact_roles: Vec<String>,
    reload_compatible_with_previous: bool,
}

#[derive(Debug, Clone, serde::Deserialize)]
struct PreviousNativeAppManifestMetadata {
    launcher: NativeAppLauncherMetadata,
    hot_reload: NativeAppHotReloadMetadata,
}

#[derive(Debug, Clone)]
struct PackagedNativeAppHostSidecar {
    packaged_file_name: String,
    env_var: Option<String>,
}

#[derive(Debug, Clone)]
pub struct NativeAppHostSidecarBinding {
    pub source_path: PathBuf,
    pub packaged_file_name: Option<String>,
    pub env_var: Option<String>,
}

#[derive(Debug, Clone)]
pub enum NativeAppLauncherEntrypoint {
    RunBundledAppJson { function_name: String },
    RunNoArgFunction { function_name: String },
}

impl Default for NativeAppLauncherEntrypoint {
    fn default() -> Self {
        Self::RunBundledAppJson {
            function_name: "run_bundled_app_json".to_string(),
        }
    }
}

#[derive(Debug, Clone, serde::Serialize)]
struct NativeAppRuntimeSnapshot {
    app_id: String,
    name: String,
    version: String,
    window_title: String,
    root_component: String,
    active_world: Option<String>,
    layout_id: String,
    required_runtime_capabilities: Vec<String>,
    panels: Vec<NativeAppRuntimePanel>,
    commands: Vec<NativeAppRuntimeCommand>,
    providers: Vec<NativeAppRuntimeProvider>,
    tools: Vec<NativeAppRuntimeTool>,
    sessions: NativeAppRuntimeSessions,
    recent_sessions: Vec<NativeAppRuntimeRecentSession>,
    workspaces: Vec<NativeAppRuntimeWorkspace>,
    launcher: NativeAppLauncherMetadata,
    hot_reload: NativeAppHotReloadMetadata,
    #[serde(default)]
    reload: NativeAppHotReloadParticipants,
    updated_at: String,
}

#[derive(Debug, Clone, serde::Serialize)]
struct NativeAppRuntimePanel {
    id: String,
    title: String,
    dock: String,
    kind: String,
}

#[derive(Debug, Clone, serde::Serialize)]
struct NativeAppRuntimeCommand {
    id: String,
    label: String,
    surface: String,
    intent: String,
}

#[derive(Debug, Clone, serde::Serialize)]
struct NativeAppRuntimeProvider {
    id: String,
    label: String,
    transport: String,
    profile_kind: String,
    supports_tools: bool,
    supports_streaming: bool,
    active: bool,
    profile_configured: bool,
    profile_keys: Vec<String>,
}

#[derive(Debug, Clone, serde::Serialize)]
struct NativeAppRuntimeTool {
    id: String,
    label: String,
    capability: String,
    approval: String,
    decision: Option<String>,
    scope_decisions: Vec<String>,
}

#[derive(Debug, Clone, serde::Serialize)]
struct NativeAppRuntimeSessions {
    total_sessions: usize,
    active_provider: String,
    recent_session_id: String,
    recent_session_title: String,
}

#[derive(Debug, Clone, serde::Serialize)]
struct NativeAppRuntimeRecentSession {
    id: String,
    title: String,
    provider_id: String,
    status: String,
    workspace_root: String,
    updated_at: String,
    message_count: usize,
    last_message_role: String,
    last_message_preview: String,
}

#[derive(Debug, Clone, serde::Serialize)]
struct NativeAppRuntimeWorkspace {
    root: String,
    session_count: usize,
    recent_session_title: String,
}

impl DriverSession {
    pub fn compile_native_app_bundle(
        &self,
        source: &str,
        config: &NativeAppBundleConfig,
    ) -> Result<NativeAppBundle, KainError> {
        let source_file_name = normalized_source_file_name(config.source_file_name.as_deref());
        let source_root = normalized_source_root(config.source_root.as_deref());
        let source_name = source_file_name.as_str();
        let root_component = discover_native_app_root_component(
            source,
            config.root_component.as_deref(),
            source_name,
        )?
        .ok_or_else(|| {
            KainError::runtime(format!(
                "Native app bundle generation requires at least one component in {source_name}"
            ))
        })?;
        let base_name = source_stem(source_name);
        let app_name = sanitize_cargo_name(config.app_name.as_deref().unwrap_or(&base_name));
        let window_title = config
            .window_title
            .clone()
            .filter(|value| !value.trim().is_empty())
            .unwrap_or_else(|| root_component.clone());
        let prepared_ui_source = crate::prepare_c_ffi_source(source, CompileTarget::Rust)?;
        let mut runtime_contract =
            self.compile_runtime_contract_bundle(source, CompileTarget::Rust)?;
        let realtime = self
            .compile_realtime_app_bundle(source, CompileTarget::Rust, Some(&root_component))?
            .bundle;
        apply_active_world_selection_to_runtime_contract(
            &mut runtime_contract,
            realtime
                .active_world
                .as_ref()
                .map(|world| world.name.as_str()),
        )?;
        let shader_bundle = self.compile_shader_artifact_bundle(source).ok();
        let ui = build_ui_output_from_source(&prepared_ui_source, &root_component)?;
        let metadata = NativeAppMetadata {
            app_name,
            window_title,
            root_component,
            source_file_name,
            source_root,
            initial_window_size: config.initial_window_size,
        };
        let mut ui_runtime_metadata = UiRuntimeMetadata::default();
        ui_runtime_metadata.app_name = Some(metadata.app_name.clone());
        ui_runtime_metadata.window_title = metadata.window_title.clone();
        ui_runtime_metadata.root_component = metadata.root_component.clone();
        ui_runtime_metadata.source_file_name = Some(metadata.source_file_name.clone());
        ui_runtime_metadata.initial_window_size = metadata.initial_window_size;
        let ui_runtime_bundle = ui_runtime_bundle_from_output(ui_runtime_metadata, ui.clone());
        let rust = self.compile_rust_artifact_bundle(source, config.include_spirv)?;

        // Load runtime version metadata
        let runtime_version = RuntimeVersionMetadata::load_from_runtime_manifest().ok();
        if let Some(runtime_version) = &runtime_version {
            apply_runtime_compatibility_metadata(
                &mut runtime_contract.compatibility,
                runtime_version,
                &runtime_contract.target,
            );
        }
        let imported_c_libraries = kain_c_ffi::detect_c_library_imports(source);
        if !imported_c_libraries.is_empty() {
            ensure_native_c_ffi_runtime_contract_metadata(
                &mut runtime_contract,
                &imported_c_libraries,
            );
        }

        Ok(NativeAppBundle {
            metadata,
            runtime_contract,
            realtime,
            shader_bundle,
            ui,
            ui_runtime_bundle,
            rust,
            runtime_version,
        })
    }

    pub fn materialize_native_app_bundle(
        &self,
        source: &str,
        bundle: &NativeAppBundle,
        config: &NativeAppMaterializationConfig,
    ) -> Result<NativeAppMaterializedPaths, KainError> {
        let project_dir = &config.project_dir;
        fs::create_dir_all(project_dir.join("src"))
            .map_err(io_error("create native app source directory"))?;
        let config_dir = project_dir.join(NATIVE_APP_CONFIG_DIR_NAME);
        let state_dir = project_dir.join(NATIVE_APP_STATE_DIR_NAME);
        fs::create_dir_all(&config_dir).map_err(io_error("create native app config directory"))?;
        fs::create_dir_all(&state_dir).map_err(io_error("create native app state directory"))?;

        let source_copy_path = project_dir.join(&bundle.metadata.source_file_name);
        fs::write(&source_copy_path, source.as_bytes())
            .map_err(io_error("write embedded native app Kain source"))?;

        let artifact_root = if config.artifact_output_dir.is_absolute() {
            config.artifact_output_dir.clone()
        } else {
            project_dir.join(&config.artifact_output_dir)
        };
        fs::create_dir_all(&artifact_root)
            .map_err(io_error("create native app artifact directory"))?;
        let cargo_target_dir = config
            .cargo_target_dir
            .clone()
            .unwrap_or_else(|| project_dir.join(".kain").join("cargo-target"));
        let gpu_runtime_cargo_target_dir = config
            .gpu_runtime_cargo_target_dir
            .clone()
            .unwrap_or_else(|| project_dir.join(".kain").join("gpu-runtime-target"));

        let mut artifact_paths = Vec::new();
        let (materialized_realtime_bundle, packaged_realtime_asset_paths) =
            materialize_realtime_assets(
                &bundle.realtime,
                &artifact_root,
                bundle.metadata.source_root.as_deref(),
            )?;
        artifact_paths.extend(packaged_realtime_asset_paths.iter().cloned());

        let runtime_bundle_path = artifact_root.join(NATIVE_APP_RUNTIME_BUNDLE_FILE_NAME);
        let runtime_bundle_json = render_runtime_bundle_json(bundle)?;
        fs::write(&runtime_bundle_path, runtime_bundle_json.as_bytes())
            .map_err(io_error("write native app runtime bundle"))?;
        artifact_paths.push(runtime_bundle_path.clone());

        let runtime_contract_path = artifact_root.join(NATIVE_APP_RUNTIME_CONTRACT_FILE_NAME);
        let runtime_contract_json = render_runtime_contract_json(bundle)?;
        fs::write(&runtime_contract_path, runtime_contract_json.as_bytes())
            .map_err(io_error("write native app runtime contract"))?;
        artifact_paths.push(runtime_contract_path.clone());

        let runtime_compatibility_path =
            artifact_root.join(NATIVE_APP_RUNTIME_COMPATIBILITY_FILE_NAME);
        let runtime_compatibility_json = render_runtime_compatibility_json(bundle)?;
        fs::write(
            &runtime_compatibility_path,
            runtime_compatibility_json.as_bytes(),
        )
        .map_err(io_error("write native app runtime compatibility"))?;
        artifact_paths.push(runtime_compatibility_path.clone());

        let realtime_bundle_path = artifact_root.join(NATIVE_APP_REALTIME_BUNDLE_FILE_NAME);
        let realtime_bundle_json = render_realtime_bundle_json(&materialized_realtime_bundle)?;
        fs::write(&realtime_bundle_path, realtime_bundle_json.as_bytes())
            .map_err(io_error("write native app realtime bundle"))?;
        artifact_paths.push(realtime_bundle_path.clone());

        let compute_residency_paths =
            write_compute_residency_sidecars(&bundle.realtime, &artifact_root)?;
        artifact_paths.extend(compute_residency_paths.iter().cloned());
        if let Some(runtime_dll_path) = materialize_gpu_runtime_library(
            &artifact_root,
            config.release,
            Some(&gpu_runtime_cargo_target_dir),
        )? {
            artifact_paths.push(runtime_dll_path);
        }

        // Write runtime version metadata if available
        if let Some(runtime_version) = &bundle.runtime_version {
            let version_metadata_path =
                artifact_root.join(NATIVE_RUNTIME_VERSION_METADATA_FILE_NAME);
            let version_metadata_json =
                serde_json::to_string_pretty(runtime_version).map_err(|err| {
                    KainError::runtime(format!(
                        "Failed to serialize runtime version metadata: {}",
                        err
                    ))
                })?;
            fs::write(&version_metadata_path, version_metadata_json.as_bytes())
                .map_err(io_error("write native runtime version metadata"))?;
            artifact_paths.push(version_metadata_path);
        }

        let (reflection_payload_path, reflection_payload_json) = if let Some(reflection_payload) =
            &bundle.runtime_contract.reflection_payload
        {
            let reflection_payload_path =
                artifact_root.join(NATIVE_RUNTIME_REFLECTION_PAYLOAD_FILE_NAME);
            let reflection_payload_json = serde_json::to_string_pretty(reflection_payload)
                .map_err(|err| {
                    KainError::runtime(format!("Failed to serialize reflection payload: {}", err))
                })?;
            fs::write(&reflection_payload_path, reflection_payload_json.as_bytes())
                .map_err(io_error("write native runtime reflection payload"))?;
            artifact_paths.push(reflection_payload_path.clone());
            (Some(reflection_payload_path), Some(reflection_payload_json))
        } else {
            (None, None)
        };

        let (shader_bundle_path, shader_bundle_json) =
            if let Some(shader_bundle) = &bundle.shader_bundle {
                let path = artifact_root.join(NATIVE_APP_SHADER_BUNDLE_FILE_NAME);
                let json = shader_bundle.bundle_json.clone();
                fs::write(&path, json.as_bytes())
                    .map_err(io_error("write native app shader bundle"))?;
                artifact_paths.push(path.clone());
                (Some(path), Some(json))
            } else {
                (None, None)
            };
        let (packaged_c_ffi_imports, packaged_c_ffi_manifest_path, c_ffi_artifact_paths) =
            materialize_c_ffi_bridge_sidecars(source, &artifact_root)?;
        artifact_paths.extend(c_ffi_artifact_paths.iter().cloned());
        let (packaged_host_sidecars, packaged_host_sidecar_paths) =
            materialize_host_sidecars(&config.host_sidecars, &artifact_root)?;
        artifact_paths.extend(packaged_host_sidecar_paths.iter().cloned());

        let primary_path = artifact_root.join(&bundle.rust.bundle.primary.suggested_file_name);
        fs::write(
            &primary_path,
            bundle.rust.bundle.primary.contents.as_bytes(),
        )
        .map_err(io_error("write native app primary Rust artifact"))?;
        artifact_paths.push(primary_path);

        for artifact in &bundle.rust.bundle.supplemental {
            let path = artifact_root.join(&artifact.suggested_file_name);
            fs::write(&path, artifact.contents.as_bytes())
                .map_err(io_error("write native app supplemental artifact"))?;
            artifact_paths.push(path);
        }

        if let Some(spirv) = &bundle.rust.spirv {
            let spirv_path = artifact_root.join("kain_gpu.spv");
            fs::write(&spirv_path, spirv).map_err(io_error("write native app SPIR-V artifact"))?;
            artifact_paths.push(spirv_path);
        }

        let app_manifest_path = config_dir.join(NATIVE_APP_MANIFEST_FILE_NAME);
        let runtime_snapshot_path = state_dir.join(NATIVE_APP_RUNTIME_SNAPSHOT_FILE_NAME);
        let launcher_metadata = build_native_app_launcher_metadata(&config.launcher_entrypoint);
        let previous_manifest_metadata =
            read_previous_native_app_manifest_metadata(&app_manifest_path);
        let hot_reload = build_native_app_hot_reload_metadata(
            project_dir,
            &source_copy_path,
            source,
            bundle,
            &launcher_metadata,
            previous_manifest_metadata.as_ref(),
            &runtime_bundle_path,
            &runtime_bundle_json,
            &runtime_contract_path,
            &runtime_contract_json,
            &runtime_compatibility_path,
            &runtime_compatibility_json,
            &realtime_bundle_path,
            &realtime_bundle_json,
            shader_bundle_path.as_ref(),
            shader_bundle_json.as_deref(),
            reflection_payload_path.as_ref(),
            reflection_payload_json.as_deref(),
        );
        let app_manifest = build_native_app_manifest(
            bundle,
            &packaged_c_ffi_imports,
            &packaged_host_sidecars,
            packaged_c_ffi_manifest_path.as_deref(),
            &runtime_bundle_path,
            &runtime_contract_path,
            &runtime_compatibility_path,
            &realtime_bundle_path,
            shader_bundle_path.as_ref(),
            reflection_payload_path.as_deref(),
            &runtime_snapshot_path,
            launcher_metadata,
            hot_reload.clone(),
        );
        fs::write(
            &app_manifest_path,
            serde_json::to_string_pretty(&app_manifest).map_err(|err| {
                KainError::runtime(format!("Failed to serialize native app manifest: {err}"))
            })?,
        )
        .map_err(io_error("write native app manifest"))?;
        artifact_paths.push(app_manifest_path.clone());

        let runtime_snapshot =
            build_native_app_runtime_snapshot(bundle, &app_manifest, project_dir);
        fs::write(
            &runtime_snapshot_path,
            serde_json::to_string_pretty(&runtime_snapshot).map_err(|err| {
                KainError::runtime(format!(
                    "Failed to serialize native app runtime snapshot: {err}"
                ))
            })?,
        )
        .map_err(io_error("write native app runtime snapshot"))?;
        artifact_paths.push(runtime_snapshot_path.clone());

        let manifest_path = project_dir.join("Cargo.toml");
        let manifest = render_manifest(
            &bundle.metadata.app_name,
            &config.runtime_crate_name,
            &config.runtime_dependency,
            packaged_c_ffi_manifest_path.is_some(),
            project_dir,
        );
        fs::write(&manifest_path, manifest.as_bytes())
            .map_err(io_error("write native app Cargo manifest"))?;

        let main_rs_path = project_dir.join("src").join("main.rs");
        let runtime_bundle_include_path = relative_path_from_directory(
            main_rs_path.parent().unwrap_or(project_dir),
            &runtime_bundle_path,
        )
        .unwrap_or_else(|| runtime_bundle_path.clone());
        let runtime_bundle_file_name = runtime_bundle_path
            .file_name()
            .and_then(OsStr::to_str)
            .unwrap_or(NATIVE_APP_RUNTIME_BUNDLE_FILE_NAME);
        let realtime_bundle_file_name = realtime_bundle_path
            .file_name()
            .and_then(OsStr::to_str)
            .unwrap_or(NATIVE_APP_REALTIME_BUNDLE_FILE_NAME);
        let compute_residency_file_name = compute_residency_paths
            .first()
            .and_then(|path| path.file_name())
            .and_then(OsStr::to_str);
        let shader_bundle_file_name = shader_bundle_path.as_ref().and_then(|path| {
            path.file_name()
                .and_then(OsStr::to_str)
                .map(ToOwned::to_owned)
        });
        let packaged_c_ffi_manifest_file_name =
            packaged_c_ffi_manifest_path.as_ref().and_then(|path| {
                path.file_name()
                    .and_then(OsStr::to_str)
                    .map(ToOwned::to_owned)
            });
        let main_rs = render_main_rs(
            &runtime_bundle_include_path,
            &config.runtime_crate_name,
            &config.launcher_entrypoint,
            runtime_bundle_file_name,
            realtime_bundle_file_name,
            compute_residency_file_name,
            shader_bundle_file_name.as_deref(),
            packaged_c_ffi_manifest_file_name.as_deref(),
            &packaged_host_sidecars,
            packaged_c_ffi_manifest_path.is_some(),
        );
        fs::write(&main_rs_path, main_rs.as_bytes())
            .map_err(io_error("write native app entrypoint"))?;

        let executable_path = if config.build_executable {
            Some(build_native_app_executable(
                project_dir,
                &bundle.metadata.app_name,
                config.release,
                config.executable_output_dir.as_deref(),
                Some(&cargo_target_dir),
            )?)
        } else {
            None
        };

        if let (Some(executable_path), Some(output_dir)) = (
            executable_path.as_ref(),
            config.executable_output_dir.as_deref(),
        ) {
            let mut runtime_sidecar_file_names = vec![
                NATIVE_APP_RUNTIME_CONTRACT_FILE_NAME,
                NATIVE_APP_RUNTIME_COMPATIBILITY_FILE_NAME,
                NATIVE_APP_REALTIME_BUNDLE_FILE_NAME,
                COMPUTE_RESIDENCY_FILE_NAME,
                NATIVE_APP_SHADER_BUNDLE_FILE_NAME,
                NATIVE_RUNTIME_VERSION_METADATA_FILE_NAME,
                NATIVE_RUNTIME_REFLECTION_PAYLOAD_FILE_NAME,
                gpu_runtime_library_file_name(),
                NATIVE_APP_C_FFI_PACKAGE_MANIFEST_FILE_NAME,
            ];
            runtime_sidecar_file_names.extend(
                compute_residency_paths
                    .iter()
                    .filter_map(|path| path.file_name().and_then(OsStr::to_str))
                    .filter(|file_name| *file_name != COMPUTE_RESIDENCY_FILE_NAME),
            );
            runtime_sidecar_file_names.extend(
                packaged_realtime_asset_paths
                    .iter()
                    .filter_map(|path| path.file_name().and_then(OsStr::to_str))
                    .filter(|file_name| !file_name.is_empty()),
            );
            runtime_sidecar_file_names.extend(
                packaged_c_ffi_manifest_path
                    .iter()
                    .chain(c_ffi_artifact_paths.iter())
                    .filter_map(|path| path.file_name().and_then(OsStr::to_str))
                    .filter(|file_name| !file_name.is_empty()),
            );
            runtime_sidecar_file_names.extend(
                packaged_host_sidecar_paths
                    .iter()
                    .filter_map(|path| path.file_name().and_then(OsStr::to_str))
                    .filter(|file_name| !file_name.is_empty()),
            );
            copy_runtime_sidecars_to_executable_dir(
                executable_path,
                output_dir,
                &artifact_paths,
                &runtime_sidecar_file_names,
            )?;
        }

        Ok(NativeAppMaterializedPaths {
            project_dir: project_dir.clone(),
            manifest_path,
            main_rs_path,
            source_copy_path,
            artifact_paths,
            executable_path,
        })
    }
}

pub fn compile_native_app_bundle(
    source: &str,
    config: &NativeAppBundleConfig,
) -> Result<NativeAppBundle, KainError> {
    DriverSession::default().compile_native_app_bundle(source, config)
}

pub fn materialize_native_app_bundle(
    source: &str,
    bundle: &NativeAppBundle,
    config: &NativeAppMaterializationConfig,
) -> Result<NativeAppMaterializedPaths, KainError> {
    DriverSession::default().materialize_native_app_bundle(source, bundle, config)
}

pub fn discover_native_app_root_component(
    source: &str,
    configured_root: Option<&str>,
    source_name: &str,
) -> Result<Option<String>, KainError> {
    let typed = DriverSession::default().frontend_to_typed_program(source, CompileTarget::Rust)?;
    resolve_root_component_name(&typed, CompileTarget::Rust, configured_root).map_err(|error| {
        KainError::runtime(format!(
            "Failed to discover native app root for {}: {}",
            source_name, error
        ))
    })
}

fn render_manifest(
    app_name: &str,
    runtime_crate_name: &str,
    runtime_dependency: &NativeAppRuntimeDependency,
    include_c_ffi_runtime: bool,
    project_dir: &Path,
) -> String {
    let dependency = match runtime_dependency {
        NativeAppRuntimeDependency::Path(path) => {
            format!(r#"{{ path = "{}" }}"#, path_for_toml(path))
        }
        NativeAppRuntimeDependency::Version(version) => {
            format!(r#"{{ version = "{version}" }}"#)
        }
    };
    let c_ffi_dependency = if include_c_ffi_runtime {
        match resolve_workspace_crate_dependency(project_dir, "kain-c-ffi")
            .ok()
            .flatten()
        {
            Some(NativeAppRuntimeDependency::Path(path)) => {
                format!("\nkain-c-ffi = {{ path = \"{}\" }}", path_for_toml(&path))
            }
            Some(NativeAppRuntimeDependency::Version(version)) => {
                format!("\nkain-c-ffi = {{ version = \"{version}\" }}")
            }
            None => String::new(),
        }
    } else {
        String::new()
    };

    format!(
        "[package]\nname = \"{app_name}\"\nversion = \"0.1.0\"\nedition = \"2021\"\n\n[workspace]\n\n[dependencies]\n{runtime_crate_name} = {dependency}{c_ffi_dependency}\n"
    )
}

fn render_main_rs(
    runtime_bundle_include_path: &Path,
    runtime_crate_name: &str,
    launcher_entrypoint: &NativeAppLauncherEntrypoint,
    runtime_bundle_file_name: &str,
    realtime_bundle_file_name: &str,
    compute_residency_file_name: Option<&str>,
    shader_bundle_file_name: Option<&str>,
    c_ffi_manifest_file_name: Option<&str>,
    host_sidecars: &[PackagedNativeAppHostSidecar],
    include_c_ffi_runtime: bool,
) -> String {
    let runtime_bundle_include_path =
        rust_string_literal(&path_for_toml(runtime_bundle_include_path));
    let runtime_module_name = runtime_crate_name.replace('-', "_");
    let runtime_bundle_file_name = rust_string_literal(runtime_bundle_file_name);
    let realtime_bundle_file_name = rust_string_literal(realtime_bundle_file_name);
    let compute_residency_env = compute_residency_file_name.map(rust_string_literal);
    let shader_bundle_env = shader_bundle_file_name.map(rust_string_literal);
    let c_ffi_manifest_env = c_ffi_manifest_file_name.map(rust_string_literal);
    let host_sidecar_setters = host_sidecars
        .iter()
        .filter_map(|sidecar| {
            sidecar.env_var.as_ref().map(|env_var| {
                let file_name = rust_string_literal(&sidecar.packaged_file_name);
                let env_var = rust_string_literal(env_var);
                format!(
                    "    if let Some(path) = resolve_runtime_sidecar({file_name}) {{\n        std::env::set_var({env_var}, &path);\n    }}\n"
                )
            })
        })
        .collect::<String>();
    let shader_bundle_setter = shader_bundle_env
        .as_deref()
        .map(|file_name| {
            format!(
                "    if let Some(path) = resolve_runtime_sidecar({file_name}) {{\n        std::env::set_var(\"KAIN_UI_NATIVE_SHADER_BUNDLE\", &path);\n    }}\n"
            )
        })
        .unwrap_or_default();
    let compute_residency_setter = compute_residency_env
        .as_deref()
        .map(|file_name| {
            format!(
                "    if let Some(path) = resolve_runtime_sidecar({file_name}) {{\n        std::env::set_var(\"{COMPUTE_RESIDENCY_ENV_VAR}\", &path);\n    }}\n"
            )
        })
        .unwrap_or_default();
    let c_ffi_loader = if include_c_ffi_runtime {
        c_ffi_manifest_env
            .as_deref()
            .map(|file_name| {
                format!(
                    "    if let Some(path) = resolve_runtime_sidecar({file_name}) {{\n        kain_c_ffi::load_packaged_bridges_from_manifest(&path)?;\n    }}\n"
                )
            })
            .unwrap_or_default()
    } else {
        String::new()
    };
    let app_manifest_file_name = rust_string_literal("app_manifest.json");
    let app_snapshot_file_name = rust_string_literal(NATIVE_APP_RUNTIME_SNAPSHOT_FILE_NAME);
    let app_manifest_relative_path = rust_string_literal("../config/app_manifest.json");
    let app_snapshot_relative_path = rust_string_literal("../state/runtime_snapshot.json");
    let c_ffi_import = if include_c_ffi_runtime {
        "use kain_c_ffi;\n".to_string()
    } else {
        String::new()
    };
    let (entrypoint_import, runtime_bundle_constant, native_ui_env_setters, entrypoint_call) =
        match launcher_entrypoint {
            NativeAppLauncherEntrypoint::RunBundledAppJson { function_name } => {
                let function_name_literal = function_name.as_str();
                (
                    format!("use {runtime_module_name}::{function_name_literal};\n"),
                    format!(
                        "const KAIN_RUNTIME_BUNDLE: &str = include_str!({runtime_bundle_include_path});\n\n"
                    ),
                    format!(
                        "    if let Some(path) = resolve_runtime_sidecar({runtime_bundle_file_name}) {{\n        std::env::set_var(\"KAIN_UI_NATIVE_RUNTIME_BUNDLE\", &path);\n    }}\n    if let Some(path) = resolve_runtime_sidecar({realtime_bundle_file_name}) {{\n        std::env::set_var(\"KAIN_UI_NATIVE_REALTIME_BUNDLE\", &path);\n    }}\n{compute_residency_setter}{shader_bundle_setter}"
                    ),
                    format!("{function_name_literal}(KAIN_RUNTIME_BUNDLE)"),
                )
            }
            NativeAppLauncherEntrypoint::RunNoArgFunction { function_name } => {
                let function_name_literal = function_name.as_str();
                (
                    format!("use {runtime_module_name}::{function_name_literal};\n"),
                    String::new(),
                    String::new(),
                    format!("{function_name_literal}()"),
                )
            }
        };

    format!(
        "#![cfg_attr(all(target_os = \"windows\", not(debug_assertions)), windows_subsystem = \"windows\")]\n\nuse std::path::PathBuf;\n\n{c_ffi_import}{entrypoint_import}\n{runtime_bundle_constant}fn resolve_runtime_sidecar(file_name: &str) -> Option<PathBuf> {{\n    if let Some(current_exe_candidate) = std::env::current_exe().ok().and_then(|exe| {{\n        exe.parent().map(|dir| dir.join(file_name)).filter(|path| path.exists())\n    }}) {{\n        return Some(current_exe_candidate);\n    }}\n    let manifest_candidate = PathBuf::from(env!(\"CARGO_MANIFEST_DIR\")).join(\"generated\").join(file_name);\n    if manifest_candidate.exists() {{\n        return Some(manifest_candidate);\n    }}\n    None\n}}\n\nfn resolve_project_sidecar(file_name: &str, relative_source_path: &str) -> Option<PathBuf> {{\n    if let Some(runtime_sidecar) = resolve_runtime_sidecar(file_name) {{\n        return Some(runtime_sidecar);\n    }}\n    let project_candidate = PathBuf::from(env!(\"CARGO_MANIFEST_DIR\")).join(relative_source_path);\n    if project_candidate.exists() {{\n        return Some(project_candidate);\n    }}\n    None\n}}\n\nfn main() -> Result<(), Box<dyn std::error::Error>> {{\n{native_ui_env_setters}{host_sidecar_setters}{c_ffi_loader}    if let Some(path) = resolve_project_sidecar({app_manifest_file_name}, {app_manifest_relative_path}) {{\n        std::env::set_var(\"KAIN_UI_NATIVE_APP_MANIFEST\", &path);\n    }}\n    if let Some(path) = resolve_project_sidecar({app_snapshot_file_name}, {app_snapshot_relative_path}) {{\n        std::env::set_var(\"KAIN_UI_NATIVE_APP_SNAPSHOT\", &path);\n    }}\n    {entrypoint_call}\n}}\n"
    )
}

fn materialize_host_sidecars(
    host_sidecars: &[NativeAppHostSidecarBinding],
    artifact_root: &Path,
) -> Result<(Vec<PackagedNativeAppHostSidecar>, Vec<PathBuf>), KainError> {
    let mut packaged_sidecars = Vec::new();
    let mut packaged_paths = Vec::new();
    for binding in host_sidecars {
        if !binding.source_path.exists() {
            return Err(KainError::runtime(format!(
                "Native app host sidecar source '{}' does not exist",
                binding.source_path.display()
            )));
        }
        let packaged_file_name = binding
            .packaged_file_name
            .clone()
            .or_else(|| {
                binding
                    .source_path
                    .file_name()
                    .and_then(OsStr::to_str)
                    .map(ToOwned::to_owned)
            })
            .ok_or_else(|| {
                KainError::runtime(format!(
                    "Native app host sidecar '{}' does not resolve to a file name",
                    binding.source_path.display()
                ))
            })?;
        let destination = artifact_root.join(&packaged_file_name);
        if binding.source_path != destination {
            fs::copy(&binding.source_path, &destination)
                .map_err(io_error("copy native app host sidecar"))?;
        }
        packaged_paths.push(destination);
        packaged_sidecars.push(PackagedNativeAppHostSidecar {
            packaged_file_name,
            env_var: binding.env_var.clone(),
        });
    }
    Ok((packaged_sidecars, packaged_paths))
}

fn materialize_realtime_assets(
    realtime_bundle: &RealtimeAppBundle,
    artifact_root: &Path,
    source_root: Option<&Path>,
) -> Result<(RealtimeAppBundle, Vec<PathBuf>), KainError> {
    let mut materialized = realtime_bundle.clone();
    let mut packaged_paths = Vec::new();
    for asset in &mut materialized.assets {
        let source_path = resolve_realtime_asset_source_path(asset, source_root);
        if !source_path.exists() {
            let source_root_context = source_root
                .map(|root| format!(" resolved relative to source root '{}'", root.display()));
            return Err(KainError::runtime(format!(
                "Native app asset '{}' points to missing source '{}'{}",
                asset.key,
                asset.source,
                source_root_context.as_deref().unwrap_or("")
            )));
        }

        let packaged_file_name = packaged_realtime_asset_file_name(asset);
        let destination = artifact_root.join(&packaged_file_name);
        if source_path != destination {
            fs::copy(&source_path, &destination)
                .map_err(io_error("copy native app realtime asset"))?;
        }
        asset.source = packaged_file_name;
        packaged_paths.push(destination);
    }

    Ok((materialized, packaged_paths))
}

fn resolve_realtime_asset_source_path(
    asset: &RealtimeAssetBinding,
    source_root: Option<&Path>,
) -> PathBuf {
    let source_path = PathBuf::from(&asset.source);
    if source_path.is_absolute() {
        return source_path;
    }

    match source_root {
        Some(root) => root.join(&source_path),
        None => source_path,
    }
}

fn packaged_realtime_asset_file_name(asset: &RealtimeAssetBinding) -> String {
    let extension = Path::new(&asset.source)
        .extension()
        .and_then(OsStr::to_str)
        .filter(|value| !value.is_empty())
        .unwrap_or("bin");
    let sanitized_key = asset
        .key
        .chars()
        .map(|ch| {
            if ch.is_ascii_alphanumeric() {
                ch.to_ascii_lowercase()
            } else {
                '_'
            }
        })
        .collect::<String>()
        .trim_matches('_')
        .to_string();
    format!("kain_asset_{}.{}", sanitized_key, extension)
}

fn ensure_native_c_ffi_runtime_contract_metadata(
    runtime_contract: &mut RuntimeContractBundle,
    imported_c_libraries: &[String],
) {
    ensure_runtime_capability(
        &mut runtime_contract.required_capabilities,
        RuntimeCapability {
            key: "c.ffi".to_string(),
            source: "kain-c-ffi".to_string(),
            detail: Some(
                "Program imports C ABI libraries that must be packaged and loaded through the native host bridge."
                    .to_string(),
            ),
        },
    );
    ensure_runtime_capability(
        &mut runtime_contract.required_capabilities,
        RuntimeCapability {
            key: "host.bridge".to_string(),
            source: "kain-c-ffi".to_string(),
            detail: Some(
                "Native app materialization packages C bridge DLLs and shared libraries through the host bridge lane."
                    .to_string(),
            ),
        },
    );
    ensure_runtime_service_binding(
        &mut runtime_contract.service_bindings,
        RuntimeServiceBinding {
            service: "host.bridge".to_string(),
            provider: "kain-c-ffi".to_string(),
            lane: "rust-native".to_string(),
        },
    );
    for import_name in imported_c_libraries {
        ensure_runtime_service_binding(
            &mut runtime_contract.service_bindings,
            RuntimeServiceBinding {
                service: format!("c.{import_name}.bridge"),
                provider: "kain-c-ffi".to_string(),
                lane: "rust-native".to_string(),
            },
        );
    }
    runtime_contract
        .required_capabilities
        .sort_by(|left, right| left.key.cmp(&right.key));
    runtime_contract
        .service_bindings
        .sort_by(|left, right| left.service.cmp(&right.service));
}

fn ensure_runtime_capability(
    capabilities: &mut Vec<RuntimeCapability>,
    capability: RuntimeCapability,
) {
    if capabilities
        .iter()
        .any(|existing| existing.key == capability.key)
    {
        return;
    }
    capabilities.push(capability);
}

fn ensure_runtime_service_binding(
    bindings: &mut Vec<RuntimeServiceBinding>,
    binding: RuntimeServiceBinding,
) {
    if bindings
        .iter()
        .any(|existing| existing.service == binding.service)
    {
        return;
    }
    bindings.push(binding);
}

fn materialize_c_ffi_bridge_sidecars(
    source: &str,
    artifact_root: &Path,
) -> Result<
    (
        Vec<kain_c_ffi::PackagedBridgeImport>,
        Option<PathBuf>,
        Vec<PathBuf>,
    ),
    KainError,
> {
    let prepare = kain_c_ffi::PrepareContext {
        current_dir: std::env::current_dir().ok(),
        manifest_path: None,
    };
    let outputs = kain_c_ffi::import_libraries_for_source(
        source,
        &kain_c_ffi::ImportCOptions {
            mode: kain_c_ffi::ArtifactMode::Both,
            ..kain_c_ffi::ImportCOptions::default()
        },
        &prepare,
    )?;
    if outputs.is_empty() {
        return Ok((Vec::new(), None, Vec::new()));
    }

    let mut imports = Vec::new();
    let mut artifact_paths = Vec::new();
    for output in outputs {
        let bridge_dylib_path = output.dylib_path.as_ref().ok_or_else(|| {
            KainError::runtime(format!(
                "C FFI import '{}' did not produce a bridge library for native packaging",
                output.resolved.import_name
            ))
        })?;
        let bridge_destination =
            artifact_root.join(&output.packaged_bridge_manifest.bridge_library.file_name);
        fs::copy(bridge_dylib_path, &bridge_destination)
            .map_err(io_error("copy packaged C FFI bridge library"))?;
        artifact_paths.push(bridge_destination);

        let binding_manifest_destination = artifact_root.join(
            output
                .manifest_json_path
                .file_name()
                .and_then(OsStr::to_str)
                .unwrap_or("c_ffi_binding_manifest.json"),
        );
        fs::copy(&output.manifest_json_path, &binding_manifest_destination)
            .map_err(io_error("copy packaged C FFI binding manifest"))?;
        artifact_paths.push(binding_manifest_destination);

        let binding_report_destination = artifact_root.join(
            output
                .report_json_path
                .file_name()
                .and_then(OsStr::to_str)
                .unwrap_or("c_ffi_report.json"),
        );
        fs::copy(&output.report_json_path, &binding_report_destination)
            .map_err(io_error("copy packaged C FFI binding report"))?;
        artifact_paths.push(binding_report_destination);

        if let Some(shared_library) = output.packaged_bridge_manifest.shared_library.as_ref() {
            let shared_source = output.resolved.shared_lib_path.as_ref().ok_or_else(|| {
                KainError::runtime(format!(
                    "C FFI import '{}' is missing a resolved shared library path",
                    output.resolved.import_name
                ))
            })?;
            let shared_destination = artifact_root.join(&shared_library.file_name);
            fs::copy(shared_source, &shared_destination)
                .map_err(io_error("copy packaged C FFI shared library"))?;
            artifact_paths.push(shared_destination);
        }

        imports.push(output.packaged_bridge_manifest);
    }

    let packaged_manifest_path = artifact_root.join(NATIVE_APP_C_FFI_PACKAGE_MANIFEST_FILE_NAME);
    let packaged_manifest = kain_c_ffi::PackagedBridgeManifest {
        schema_version: "kain-c-ffi-runtime-v1".to_string(),
        lane: "c".to_string(),
        imports: imports.clone(),
    };
    fs::write(
        &packaged_manifest_path,
        serde_json::to_string_pretty(&packaged_manifest).map_err(|err| {
            KainError::runtime(format!(
                "Failed to serialize packaged C FFI manifest: {err}"
            ))
        })?,
    )
    .map_err(io_error("write packaged C FFI manifest"))?;
    artifact_paths.push(packaged_manifest_path.clone());

    Ok((imports, Some(packaged_manifest_path), artifact_paths))
}

fn build_native_app_manifest(
    bundle: &NativeAppBundle,
    packaged_c_ffi_imports: &[kain_c_ffi::PackagedBridgeImport],
    packaged_host_sidecars: &[PackagedNativeAppHostSidecar],
    packaged_c_ffi_manifest_path: Option<&Path>,
    runtime_bundle_path: &Path,
    runtime_contract_path: &Path,
    runtime_compatibility_path: &Path,
    realtime_bundle_path: &Path,
    shader_bundle_path: Option<&PathBuf>,
    reflection_payload_path: Option<&Path>,
    runtime_snapshot_path: &Path,
    launcher: NativeAppLauncherMetadata,
    hot_reload: NativeAppHotReloadMetadata,
) -> NativeAppManifest {
    let version = bundle
        .runtime_version
        .as_ref()
        .map(|value| value.runtime_version_string.clone())
        .unwrap_or_else(|| "0.1.0".to_string());
    let required_runtime_capabilities = bundle
        .runtime_contract
        .required_capabilities
        .iter()
        .map(|capability| capability.key.clone())
        .collect::<Vec<_>>();

    NativeAppManifest {
        app_id: bundle.metadata.app_name.replace('-', "."),
        name: bundle.metadata.window_title.clone(),
        version,
        window_title: bundle.metadata.window_title.clone(),
        root_component: bundle.metadata.root_component.clone(),
        active_world: bundle
            .realtime
            .active_world
            .as_ref()
            .map(|world| world.name.clone()),
        layout_id: format!("{}_shell", bundle.metadata.app_name.replace('-', "_")),
        required_runtime_capabilities,
        target_outputs: vec!["native-ui-bundle".to_string(), "native-exe".to_string()],
        manifests: NativeAppManifestPaths {
            host_bridges: packaged_c_ffi_manifest_path
                .map(sidecar_file_name)
                .unwrap_or_default(),
        },
        runtime_sidecars: NativeAppRuntimeSidecars {
            runtime_bundle: sidecar_file_name(runtime_bundle_path),
            runtime_contract: sidecar_file_name(runtime_contract_path),
            runtime_compatibility: sidecar_file_name(runtime_compatibility_path),
            realtime_bundle: sidecar_file_name(realtime_bundle_path),
            shader_bundle: shader_bundle_path.map(|path| sidecar_file_name(path)),
            reflection_payload: reflection_payload_path.map(sidecar_file_name),
            runtime_snapshot: sidecar_file_name(runtime_snapshot_path),
        },
        launcher,
        hot_reload,
        host_bridges: packaged_c_ffi_imports.to_vec(),
        host_sidecars: packaged_host_sidecars
            .iter()
            .map(|sidecar| NativeAppManifestHostSidecar {
                packaged_file_name: sidecar.packaged_file_name.clone(),
                env_var: sidecar.env_var.clone(),
            })
            .collect(),
    }
}

fn build_native_app_runtime_snapshot(
    bundle: &NativeAppBundle,
    app_manifest: &NativeAppManifest,
    project_dir: &Path,
) -> NativeAppRuntimeSnapshot {
    let updated_at = current_timestamp_string();
    let workspace_root = bundle
        .metadata
        .source_root
        .clone()
        .unwrap_or_else(|| project_dir.to_path_buf())
        .display()
        .to_string();
    NativeAppRuntimeSnapshot {
        app_id: app_manifest.app_id.clone(),
        name: app_manifest.name.clone(),
        version: app_manifest.version.clone(),
        window_title: bundle.metadata.window_title.clone(),
        root_component: bundle.metadata.root_component.clone(),
        active_world: app_manifest.active_world.clone(),
        layout_id: app_manifest.layout_id.clone(),
        required_runtime_capabilities: app_manifest.required_runtime_capabilities.clone(),
        panels: vec![NativeAppRuntimePanel {
            id: "runtime_surface".to_string(),
            title: bundle.metadata.window_title.clone(),
            dock: "center".to_string(),
            kind: "native-ui".to_string(),
        }],
        commands: vec![NativeAppRuntimeCommand {
            id: "runtime.reload".to_string(),
            label: "Reload Runtime".to_string(),
            surface: "titlebar".to_string(),
            intent: "runtime.reload".to_string(),
        }],
        providers: vec![NativeAppRuntimeProvider {
            id: "native_runtime".to_string(),
            label: "Native Runtime".to_string(),
            transport: "in-process".to_string(),
            profile_kind: "native-ui".to_string(),
            supports_tools: true,
            supports_streaming: false,
            active: true,
            profile_configured: true,
            profile_keys: vec![],
        }],
        tools: bundle
            .runtime_contract
            .required_capabilities
            .iter()
            .map(|capability| NativeAppRuntimeTool {
                id: capability.key.replace('.', "_"),
                label: capability.key.clone(),
                capability: capability.key.clone(),
                approval: "workspace".to_string(),
                decision: None,
                scope_decisions: vec![],
            })
            .collect(),
        sessions: NativeAppRuntimeSessions {
            total_sessions: 1,
            active_provider: "native_runtime".to_string(),
            recent_session_id: "native-app-session".to_string(),
            recent_session_title: bundle.metadata.window_title.clone(),
        },
        recent_sessions: vec![NativeAppRuntimeRecentSession {
            id: "native-app-session".to_string(),
            title: bundle.metadata.window_title.clone(),
            provider_id: "native_runtime".to_string(),
            status: "active".to_string(),
            workspace_root: workspace_root.clone(),
            updated_at: updated_at.clone(),
            message_count: 1,
            last_message_role: "system".to_string(),
            last_message_preview: "Native app materialization snapshot".to_string(),
        }],
        workspaces: vec![NativeAppRuntimeWorkspace {
            root: workspace_root,
            session_count: 1,
            recent_session_title: bundle.metadata.window_title.clone(),
        }],
        launcher: app_manifest.launcher.clone(),
        hot_reload: app_manifest.hot_reload.clone(),
        reload: app_manifest.hot_reload.participants.clone(),
        updated_at,
    }
}

pub(crate) fn build_native_app_reload_participants(
    bundle: &NativeAppBundle,
    shader_bundle_present: bool,
) -> NativeAppHotReloadParticipants {
    let mut worlds = bundle
        .runtime_contract
        .worlds
        .iter()
        .map(reload_world_participant_from_contract)
        .collect::<Vec<_>>();
    worlds.sort_by(|left, right| left.name.cmp(&right.name));

    let mut actors = bundle
        .runtime_contract
        .reflection_payload
        .as_ref()
        .map(reload_actor_participants_from_reflection)
        .unwrap_or_default();
    actors.sort_by(|left, right| left.name.cmp(&right.name));

    NativeAppHotReloadParticipants {
        package_surface: "std::reload".to_string(),
        default_state_migration: "auto-structural".to_string(),
        default_actor_quiesce: "turn-boundary".to_string(),
        default_restart_mode: "restart-with-snapshot-restore".to_string(),
        compatibility_lanes: HOT_RELOAD_COMPATIBILITY_LANES
            .iter()
            .map(|lane| (*lane).to_string())
            .collect(),
        worlds,
        actors,
        gpu_hooks: NativeAppHotReloadGpuHooks {
            swap_boundary: "frame-boundary".to_string(),
            shader_bundle_role: if shader_bundle_present {
                Some("shader_bundle".to_string())
            } else {
                None
            },
            resource_graph_reload: "planned".to_string(),
        },
    }
}

fn reload_role_affects_runtime_state(role: &str) -> bool {
    matches!(
        role,
        "runtime_bundle" | "runtime_contract" | "realtime_bundle" | "reflection_payload"
    )
}

fn reload_role_is_gpu_swap_only(role: &str) -> bool {
    matches!(role, "shader_bundle")
}

fn push_reload_action(actions: &mut Vec<String>, action: &str) {
    if !actions.iter().any(|existing| existing == action) {
        actions.push(action.to_string());
    }
}

fn build_native_app_hot_reload_transition(
    previous_manifest_metadata: Option<&PreviousNativeAppManifestMetadata>,
    launcher: &NativeAppLauncherMetadata,
    identity: &NativeAppHotReloadIdentity,
    participants: &NativeAppHotReloadParticipants,
    changed_artifact_roles: &[String],
) -> NativeAppHotReloadTransition {
    let semantic_roles = changed_artifact_roles
        .iter()
        .filter(|role| role.as_str() != "source_input")
        .cloned()
        .collect::<Vec<_>>();

    let Some(previous) = previous_manifest_metadata else {
        return NativeAppHotReloadTransition {
            class: "cold-start".to_string(),
            restart_required: false,
            reasons: vec!["first materialized reload baseline".to_string()],
            actions: vec!["launch-initial-generation".to_string()],
        };
    };

    let launcher_changed = previous.launcher != *launcher;
    let identity_changed = previous.hot_reload.identity != *identity;
    let participant_contract_changed = previous.hot_reload.participants != *participants;

    if launcher_changed || identity_changed || participant_contract_changed {
        let mut reasons = Vec::new();
        if launcher_changed {
            reasons.push("launcher contract changed".to_string());
        }
        if identity_changed {
            reasons.push("authored app identity changed".to_string());
        }
        if participant_contract_changed {
            reasons.push("std::reload participant contract changed".to_string());
        }
        return NativeAppHotReloadTransition {
            class: "restart-with-restore".to_string(),
            restart_required: true,
            reasons,
            actions: vec![
                "restart-process".to_string(),
                "restore-runtime-snapshot".to_string(),
            ],
        };
    }

    if semantic_roles.is_empty() {
        return NativeAppHotReloadTransition {
            class: "noop".to_string(),
            restart_required: false,
            reasons: vec!["source changed without a runtime-sidecar delta".to_string()],
            actions: vec!["preserve-ui-state".to_string()],
        };
    }

    let mut reasons = vec![format!(
        "changed runtime artifacts: {}",
        semantic_roles.join(", ")
    )];
    let mut actions = vec!["preserve-ui-state".to_string()];
    let has_worlds = !participants.worlds.is_empty();
    let has_actors = !participants.actors.is_empty();
    let runtime_state_roles = semantic_roles
        .iter()
        .any(|role| reload_role_affects_runtime_state(role));
    let gpu_only = semantic_roles
        .iter()
        .all(|role| reload_role_is_gpu_swap_only(role))
        && semantic_roles
            .iter()
            .any(|role| reload_role_is_gpu_swap_only(role));

    if gpu_only {
        reasons.push("shader/runtime graphics payload changed without schema drift".to_string());
        push_reload_action(&mut actions, "swap-gpu-at-frame-boundary");
        return NativeAppHotReloadTransition {
            class: "frame-boundary-gpu-swap".to_string(),
            restart_required: false,
            reasons,
            actions,
        };
    }

    if runtime_state_roles && has_actors {
        reasons.push("actor state remains structurally compatible".to_string());
        if has_worlds {
            reasons.push("world state remains structurally compatible".to_string());
        }
        push_reload_action(&mut actions, "quiesce-actors-at-turn-boundary");
        push_reload_action(&mut actions, "transfer-queued-actor-messages");
        if has_worlds {
            push_reload_action(&mut actions, "migrate-world-state-structurally");
        }
        return NativeAppHotReloadTransition {
            class: "quiesce-and-migrate".to_string(),
            restart_required: false,
            reasons,
            actions,
        };
    }

    if runtime_state_roles && has_worlds {
        reasons.push("world state remains structurally compatible".to_string());
        push_reload_action(&mut actions, "migrate-world-state-structurally");
        return NativeAppHotReloadTransition {
            class: "structural-migrate".to_string(),
            restart_required: false,
            reasons,
            actions,
        };
    }

    reasons.push("presentation/runtime surface can patch in place".to_string());
    push_reload_action(&mut actions, "patch-runtime-presentation");
    NativeAppHotReloadTransition {
        class: "presentation-only".to_string(),
        restart_required: false,
        reasons,
        actions,
    }
}

fn reload_world_participant_from_contract(
    world: &RuntimeWorldContract,
) -> NativeAppHotReloadWorldParticipant {
    let mut state_fields = world
        .state_slots
        .iter()
        .map(|slot| NativeAppHotReloadParticipantField {
            name: slot.name.clone(),
            type_name: slot.type_name.clone(),
        })
        .collect::<Vec<_>>();
    state_fields.sort_by(|left, right| left.name.cmp(&right.name));

    let mut surface_kinds = world
        .surfaces
        .iter()
        .map(|surface| surface.kind.clone())
        .collect::<Vec<_>>();
    surface_kinds.sort();

    NativeAppHotReloadWorldParticipant {
        name: world.name.clone(),
        state_fields,
        surface_kinds,
        migration_mode: "auto-structural".to_string(),
    }
}

fn reload_actor_participants_from_reflection(
    reflection: &RuntimeReflectionPayload,
) -> Vec<NativeAppHotReloadActorParticipant> {
    reflection
        .actors
        .iter()
        .map(|actor| {
            let mut message_types = actor.message_types.clone();
            message_types.sort();

            let mut state_fields = actor
                .state_type
                .as_ref()
                .and_then(|state_type| {
                    reflection
                        .types
                        .iter()
                        .find(|ty| ty.name == *state_type)
                        .map(|ty| {
                            ty.fields
                                .iter()
                                .map(|field| NativeAppHotReloadParticipantField {
                                    name: field.name.clone(),
                                    type_name: field.type_name.clone(),
                                })
                                .collect::<Vec<_>>()
                        })
                })
                .unwrap_or_default();
            state_fields.sort_by(|left, right| left.name.cmp(&right.name));

            NativeAppHotReloadActorParticipant {
                name: actor.name.clone(),
                state_type: actor.state_type.clone(),
                state_fields,
                message_types,
                migration_mode: "auto-structural".to_string(),
                quiesce_boundary: "turn-boundary".to_string(),
                mailbox_transfer: "preserve-queued-messages".to_string(),
            }
        })
        .collect()
}

fn build_native_app_launcher_metadata(
    launcher_entrypoint: &NativeAppLauncherEntrypoint,
) -> NativeAppLauncherMetadata {
    match launcher_entrypoint {
        NativeAppLauncherEntrypoint::RunBundledAppJson { function_name } => {
            NativeAppLauncherMetadata {
                kind: "run_bundled_app_json".to_string(),
                function_name: function_name.clone(),
            }
        }
        NativeAppLauncherEntrypoint::RunNoArgFunction { function_name } => {
            NativeAppLauncherMetadata {
                kind: "run_no_arg_function".to_string(),
                function_name: function_name.clone(),
            }
        }
    }
}

fn build_native_app_hot_reload_metadata(
    project_dir: &Path,
    source_copy_path: &Path,
    source_text: &str,
    bundle: &NativeAppBundle,
    launcher: &NativeAppLauncherMetadata,
    previous_manifest_metadata: Option<&PreviousNativeAppManifestMetadata>,
    runtime_bundle_path: &Path,
    runtime_bundle_json: &str,
    runtime_contract_path: &Path,
    runtime_contract_json: &str,
    runtime_compatibility_path: &Path,
    runtime_compatibility_json: &str,
    realtime_bundle_path: &Path,
    realtime_bundle_json: &str,
    shader_bundle_path: Option<&PathBuf>,
    shader_bundle_json: Option<&str>,
    reflection_payload_path: Option<&PathBuf>,
    reflection_payload_json: Option<&str>,
) -> NativeAppHotReloadMetadata {
    let hot_reload_plan = &bundle.ui.systems.hot_reload;
    let policy = NativeAppHotReloadPolicy {
        product_mode_default: true,
        devtools_opt_in: true,
        preserve_focus: hot_reload_plan.preserve_focus,
        preserve_selection: hot_reload_plan.preserve_selection,
        preserve_docking: hot_reload_plan.preserve_docking,
        preserve_overlays: hot_reload_plan.preserve_overlays,
        preserve_motion_policy: hot_reload_plan.preserve_motion_policy,
        preserve_animation_state: hot_reload_plan.preserve_animation_state,
        preserve_signal_values: hot_reload_plan.preserve_signal_values,
        preserve_session_state: hot_reload_plan.preserve_session_state,
    };
    let identity = NativeAppHotReloadIdentity {
        app_id: bundle.metadata.app_name.replace('-', "."),
        name: bundle.metadata.window_title.clone(),
        window_title: bundle.metadata.window_title.clone(),
        root_component: bundle.metadata.root_component.clone(),
        active_world: bundle
            .realtime
            .active_world
            .as_ref()
            .map(|world| world.name.clone()),
        layout_id: format!("{}_shell", bundle.metadata.app_name.replace('-', "_")),
    };
    let participants = build_native_app_reload_participants(bundle, shader_bundle_json.is_some());
    let mut artifact_fingerprints = vec![
        NativeAppHotReloadArtifact {
            role: "source_input".to_string(),
            path: reload_path_string(project_dir, source_copy_path),
            fingerprint: fingerprint_text(source_text),
            byte_length: source_text.len(),
        },
        NativeAppHotReloadArtifact {
            role: "runtime_bundle".to_string(),
            path: reload_path_string(project_dir, runtime_bundle_path),
            fingerprint: fingerprint_text(runtime_bundle_json),
            byte_length: runtime_bundle_json.len(),
        },
        NativeAppHotReloadArtifact {
            role: "runtime_contract".to_string(),
            path: reload_path_string(project_dir, runtime_contract_path),
            fingerprint: fingerprint_text(runtime_contract_json),
            byte_length: runtime_contract_json.len(),
        },
        NativeAppHotReloadArtifact {
            role: "runtime_compatibility".to_string(),
            path: reload_path_string(project_dir, runtime_compatibility_path),
            fingerprint: fingerprint_text(runtime_compatibility_json),
            byte_length: runtime_compatibility_json.len(),
        },
        NativeAppHotReloadArtifact {
            role: "realtime_bundle".to_string(),
            path: reload_path_string(project_dir, realtime_bundle_path),
            fingerprint: fingerprint_text(realtime_bundle_json),
            byte_length: realtime_bundle_json.len(),
        },
    ];

    if let (Some(path), Some(json)) = (shader_bundle_path, shader_bundle_json) {
        artifact_fingerprints.push(NativeAppHotReloadArtifact {
            role: "shader_bundle".to_string(),
            path: reload_path_string(project_dir, path),
            fingerprint: fingerprint_text(json),
            byte_length: json.len(),
        });
    }
    if let (Some(path), Some(json)) = (reflection_payload_path, reflection_payload_json) {
        artifact_fingerprints.push(NativeAppHotReloadArtifact {
            role: "reflection_payload".to_string(),
            path: reload_path_string(project_dir, path),
            fingerprint: fingerprint_text(json),
            byte_length: json.len(),
        });
    }

    let materialization_fingerprint = fingerprint_text(
        &artifact_fingerprints
            .iter()
            .map(|artifact| {
                format!(
                    "{}:{}:{}:{}",
                    artifact.role, artifact.path, artifact.fingerprint, artifact.byte_length
                )
            })
            .collect::<Vec<_>>()
            .join("|"),
    );

    let changed_artifact_roles = previous_manifest_metadata
        .map(|previous| {
            artifact_fingerprints
                .iter()
                .filter(|artifact| {
                    previous
                        .hot_reload
                        .artifact_fingerprints
                        .iter()
                        .find(|previous_artifact| previous_artifact.role == artifact.role)
                        .map(|previous_artifact| {
                            previous_artifact.fingerprint != artifact.fingerprint
                        })
                        .unwrap_or(true)
                })
                .map(|artifact| artifact.role.clone())
                .collect::<Vec<_>>()
        })
        .unwrap_or_default();
    let previous_materialization_fingerprint = previous_manifest_metadata
        .map(|previous| previous.hot_reload.materialization_fingerprint.clone());
    let reload_compatible_with_previous = previous_manifest_metadata
        .map(|previous| {
            previous.launcher == *launcher
                && previous.hot_reload.identity == identity
                && previous.hot_reload.participants == participants
        })
        .unwrap_or(false);
    let transition = build_native_app_hot_reload_transition(
        previous_manifest_metadata,
        launcher,
        &identity,
        &participants,
        &changed_artifact_roles,
    );
    let summary = if previous_manifest_metadata.is_some() {
        format!(
            "Product mode stays default and devtools stay opt-in. Transition lane: {}. Changed artifacts: {}. State-preserving reload remains {} when launcher, authored identity, and std::reload participant schemas still match. Actions: {}.",
            transition.class,
            if changed_artifact_roles.is_empty() {
                "none".to_string()
            } else {
                changed_artifact_roles.join(", ")
            },
            if reload_compatible_with_previous {
                "eligible"
            } else {
                "gated"
            },
            if transition.actions.is_empty() {
                "none".to_string()
            } else {
                transition.actions.join(", ")
            },
        )
    } else {
        format!(
            "Product mode stays default and devtools stay opt-in. This is the first materialized reload baseline for the packaged app. Transition lane: {}.",
            transition.class
        )
    };

    NativeAppHotReloadMetadata {
        summary,
        launcher: launcher.clone(),
        policy,
        identity,
        participants,
        transition,
        artifact_fingerprints,
        materialization_fingerprint,
        previous_materialization_fingerprint,
        changed_artifact_roles,
        reload_compatible_with_previous,
    }
}

fn read_previous_native_app_manifest_metadata(
    app_manifest_path: &Path,
) -> Option<PreviousNativeAppManifestMetadata> {
    let manifest_json = fs::read_to_string(app_manifest_path).ok()?;
    serde_json::from_str(&manifest_json).ok()
}

fn reload_path_string(project_dir: &Path, path: &Path) -> String {
    relative_path_from_directory(project_dir, path)
        .unwrap_or_else(|| path.to_path_buf())
        .display()
        .to_string()
}

fn fingerprint_text(value: &str) -> String {
    let mut hash = Sha256::new();
    hash.update(value.as_bytes());
    format!("{:x}", hash.finalize())
}

fn current_timestamp_string() -> String {
    let seconds = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_secs();
    format!("unix:{seconds}")
}

fn resolve_workspace_crate_dependency(
    project_dir: &Path,
    crate_name: &str,
) -> Result<Option<NativeAppRuntimeDependency>, KainError> {
    let workspace_root = resolve_driver_workspace_root()?;
    let dependency_root = workspace_root.join("crates").join(crate_name);
    if dependency_root.join("Cargo.toml").exists() {
        let path =
            relative_path_from_directory(project_dir, &dependency_root).unwrap_or(dependency_root);
        return Ok(Some(NativeAppRuntimeDependency::Path(path)));
    }
    Ok(None)
}

fn resolve_driver_workspace_root() -> Result<PathBuf, KainError> {
    let driver_manifest_dir = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    driver_manifest_dir
        .parent()
        .and_then(Path::parent)
        .map(Path::to_path_buf)
        .ok_or_else(|| {
            KainError::runtime(
                "Failed to derive the Kain workspace root from the kain-driver crate path",
            )
        })
}

fn sidecar_file_name(path: &Path) -> String {
    path.file_name()
        .and_then(OsStr::to_str)
        .unwrap_or_default()
        .to_string()
}

fn copy_runtime_sidecars_to_executable_dir(
    executable_path: &Path,
    output_dir: &Path,
    artifact_paths: &[PathBuf],
    file_names: &[&str],
) -> Result<(), KainError> {
    let Some(executable_dir) = executable_path.parent() else {
        return Ok(());
    };
    if executable_dir != output_dir {
        return Ok(());
    }
    for file_name in file_names {
        if let Some(source) = artifact_paths
            .iter()
            .find(|path| path.file_name().and_then(OsStr::to_str) == Some(*file_name))
        {
            let destination = executable_dir.join(file_name);
            fs::copy(source, &destination).map_err(io_error("copy native app runtime sidecar"))?;
        }
    }
    Ok(())
}

fn materialize_gpu_runtime_library(
    artifact_root: &Path,
    release: bool,
    cargo_target_dir: Option<&Path>,
) -> Result<Option<PathBuf>, KainError> {
    let Some(workspace_root) = find_workspace_root_with_gpu_runtime() else {
        return Ok(None);
    };
    let runtime_library_file_name = gpu_runtime_library_file_name();

    let mut command = Command::new("cargo");
    command.arg("build").arg("-p").arg("kain-gpu-runtime");
    if release {
        command.arg("--release");
    }
    if let Some(cargo_target_dir) = cargo_target_dir {
        fs::create_dir_all(cargo_target_dir)
            .map_err(io_error("create kain-gpu-runtime cargo target directory"))?;
        command.env("CARGO_TARGET_DIR", cargo_target_dir);
    }
    command.current_dir(&workspace_root);
    let output = command.output().map_err(|err| {
        KainError::runtime(format!(
            "Failed to invoke cargo to build kain-gpu-runtime at {}: {}",
            workspace_root.display(),
            err
        ))
    })?;
    if !output.status.success() {
        return Err(KainError::runtime(format!(
            "kain-gpu-runtime cargo build failed for {}:\n{}\n{}",
            workspace_root.display(),
            String::from_utf8_lossy(&output.stdout),
            String::from_utf8_lossy(&output.stderr)
        )));
    }

    let built_library = cargo_target_dir
        .map(Path::to_path_buf)
        .unwrap_or_else(|| workspace_root.join("target"))
        .join(if release { "release" } else { "debug" })
        .join(runtime_library_file_name);
    if !built_library.exists() {
        return Ok(None);
    }

    let destination = artifact_root.join(runtime_library_file_name);
    fs::copy(&built_library, &destination)
        .map_err(io_error("copy kain-gpu-runtime shared library"))?;
    Ok(Some(destination))
}

fn gpu_runtime_library_file_name() -> &'static str {
    if cfg!(windows) {
        "kain_gpu_runtime.dll"
    } else if cfg!(target_os = "macos") {
        "libkain_gpu_runtime.dylib"
    } else {
        "libkain_gpu_runtime.so"
    }
}

fn find_workspace_root_with_gpu_runtime() -> Option<PathBuf> {
    let mut roots = Vec::new();
    if let Ok(dir) = std::env::current_dir() {
        roots.push(dir);
    }
    if let Ok(exe) = std::env::current_exe() {
        if let Some(dir) = exe.parent() {
            roots.push(dir.to_path_buf());
        }
    }

    for mut dir in roots {
        for _ in 0..12 {
            if dir
                .join("crates")
                .join("kain-gpu-runtime")
                .join("Cargo.toml")
                .exists()
            {
                return Some(dir);
            }
            if !dir.pop() {
                break;
            }
        }
    }
    None
}

fn build_native_app_executable(
    project_dir: &Path,
    app_name: &str,
    release: bool,
    output_dir: Option<&Path>,
    cargo_target_dir: Option<&Path>,
) -> Result<PathBuf, KainError> {
    let mut command = Command::new("cargo");
    command.arg("build");
    if release {
        command.arg("--release");
    }
    if let Some(cargo_target_dir) = cargo_target_dir {
        fs::create_dir_all(cargo_target_dir)
            .map_err(io_error("create native app cargo target directory"))?;
        command.env("CARGO_TARGET_DIR", cargo_target_dir);
    }
    command.current_dir(project_dir);

    let output = command.output().map_err(|err| {
        KainError::runtime(format!(
            "Failed to invoke cargo to build native app at {}: {}",
            project_dir.display(),
            err
        ))
    })?;

    if !output.status.success() {
        return Err(KainError::runtime(format!(
            "Native app cargo build failed for {}:\n{}\n{}",
            project_dir.display(),
            String::from_utf8_lossy(&output.stdout),
            String::from_utf8_lossy(&output.stderr)
        )));
    }

    let built_executable = cargo_target_dir
        .map(Path::to_path_buf)
        .unwrap_or_else(|| project_dir.join("target"))
        .join(if release { "release" } else { "debug" })
        .join(binary_file_name(app_name));

    if !built_executable.exists() {
        return Err(KainError::runtime(format!(
            "Cargo reported success but no executable was found at {}",
            built_executable.display()
        )));
    }

    if let Some(output_dir) = output_dir {
        fs::create_dir_all(output_dir)
            .map_err(io_error("create native app executable output directory"))?;
        let copied_executable = output_dir.join(binary_file_name(app_name));
        fs::copy(&built_executable, &copied_executable)
            .map_err(io_error("copy native app executable"))?;
        return Ok(copied_executable);
    }

    Ok(built_executable)
}

fn binary_file_name(app_name: &str) -> String {
    if cfg!(windows) {
        format!("{app_name}.exe")
    } else {
        app_name.to_string()
    }
}

fn normalized_source_file_name(source_file_name: Option<&str>) -> String {
    source_file_name
        .and_then(|value| {
            let trimmed = value.trim();
            if trimmed.is_empty() {
                None
            } else {
                Some(trimmed.to_string())
            }
        })
        .unwrap_or_else(|| "app.kn".to_string())
}

fn normalized_source_root(source_root: Option<&Path>) -> Option<PathBuf> {
    source_root.and_then(|value| {
        value
            .components()
            .next()
            .is_some()
            .then(|| value.to_path_buf())
    })
}

fn source_stem(source_name: &str) -> String {
    Path::new(source_name)
        .file_stem()
        .and_then(OsStr::to_str)
        .filter(|value| !value.trim().is_empty())
        .unwrap_or("kain")
        .to_string()
}

fn sanitize_cargo_name(raw: &str) -> String {
    let mut sanitized = String::with_capacity(raw.len());
    let mut last_was_dash = false;

    for ch in raw.chars() {
        let mapped = match ch {
            'A'..='Z' => ch.to_ascii_lowercase(),
            'a'..='z' | '0'..='9' => ch,
            _ => '-',
        };

        if mapped == '-' {
            if !last_was_dash {
                sanitized.push(mapped);
                last_was_dash = true;
            }
        } else {
            sanitized.push(mapped);
            last_was_dash = false;
        }
    }

    let trimmed = sanitized.trim_matches('-');
    let mut result = if trimmed.is_empty() {
        "kain-ui-app".to_string()
    } else {
        trimmed.to_string()
    };

    if result
        .chars()
        .next()
        .is_some_and(|ch| !ch.is_ascii_alphabetic())
    {
        result.insert_str(0, "kain-ui-");
    }

    result
}

fn rust_string_literal(value: &str) -> String {
    format!("{value:?}")
}

fn render_runtime_bundle_json(bundle: &NativeAppBundle) -> Result<String, KainError> {
    ui_runtime_bundle_to_json(&bundle.ui_runtime_bundle).map_err(|err| {
        KainError::runtime(format!(
            "Failed to serialize native app runtime bundle for {}: {err}",
            bundle.metadata.app_name
        ))
    })
}

fn render_runtime_contract_json(bundle: &NativeAppBundle) -> Result<String, KainError> {
    runtime_contract_bundle_to_json(&bundle.runtime_contract).map_err(|err| {
        KainError::runtime(format!(
            "Failed to serialize runtime contract bundle for {}: {err}",
            bundle.metadata.app_name
        ))
    })
}

fn render_runtime_compatibility_json(bundle: &NativeAppBundle) -> Result<String, KainError> {
    serde_json::to_string_pretty(&bundle.runtime_contract.compatibility).map_err(|err| {
        KainError::runtime(format!(
            "Failed to serialize runtime compatibility bundle for {}: {err}",
            bundle.metadata.app_name
        ))
    })
}

fn apply_runtime_compatibility_metadata(
    compatibility: &mut RuntimeCompatibilityMetadata,
    runtime_version: &RuntimeVersionMetadata,
    runtime_target: &str,
) {
    compatibility.runtime_lane = Some(runtime_version.runtime_lane.clone());
    compatibility.compatibility_class = Some(runtime_version.compatibility_class.clone());
    compatibility.runtime_version = Some(RuntimeVersionRecord::new(
        runtime_version.runtime_major,
        runtime_version.runtime_minor,
        runtime_version.runtime_patch,
        runtime_version.runtime_version_string.clone(),
    ));
    compatibility.abi_version = Some(RuntimeVersionRecord::new(
        runtime_version.abi_major,
        runtime_version.abi_minor,
        runtime_version.abi_patch,
        runtime_version.abi_version_string.clone(),
    ));
    compatibility.platform_availability = Some(RuntimePlatformAvailabilityMetadata {
        schema_version: 1,
        target_platforms: runtime_version.target_platforms.clone(),
        active_platforms: runtime_version.active_platforms.clone(),
        runtime_platform: Some(std::env::consts::OS.to_string()),
        notes: vec![
            format!(
                "Runtime manifest declares target platforms: {}.",
                runtime_version.target_platforms.join(", ")
            ),
            format!(
                "Runtime manifest declares active platforms: {}.",
                runtime_version.active_platforms.join(", ")
            ),
            format!(
                "Materialization occurred on host platform '{}'.",
                std::env::consts::OS
            ),
        ],
    });
    if compatibility
        .migration_hints
        .iter()
        .all(|hint| !hint.contains("runtime manifest"))
    {
        compatibility.migration_hints.push(format!(
            "Runtime manifest metadata from {runtime_target} should stay in sync with the emitted bundle."
        ));
    }
}

fn render_realtime_bundle_json(realtime_bundle: &RealtimeAppBundle) -> Result<String, KainError> {
    realtime_app_bundle_to_json(realtime_bundle).map_err(|err| {
        KainError::runtime(format!(
            "Failed to serialize native app realtime bundle: {err}"
        ))
    })
}

fn relative_path_from_directory(from_dir: &Path, to_path: &Path) -> Option<PathBuf> {
    let from_components: Vec<_> = from_dir.components().collect();
    let to_components: Vec<_> = to_path.components().collect();

    if from_components.is_empty() || to_components.is_empty() {
        return None;
    }

    if !components_share_prefix(&from_components, &to_components) {
        return None;
    }

    let shared_prefix_len = from_components
        .iter()
        .zip(&to_components)
        .take_while(|(left, right)| left == right)
        .count();

    let mut relative = PathBuf::new();
    for _ in shared_prefix_len..from_components.len() {
        relative.push("..");
    }
    for component in &to_components[shared_prefix_len..] {
        relative.push(component.as_os_str());
    }

    if relative.as_os_str().is_empty() {
        None
    } else {
        Some(relative)
    }
}

fn components_share_prefix(left: &[Component<'_>], right: &[Component<'_>]) -> bool {
    match (left.first(), right.first()) {
        (Some(Component::Prefix(left_prefix)), Some(Component::Prefix(right_prefix))) => {
            left_prefix.kind() == right_prefix.kind()
        }
        (Some(Component::RootDir), Some(Component::RootDir)) => true,
        (Some(Component::Normal(left_normal)), Some(Component::Normal(right_normal))) => {
            left_normal == right_normal
        }
        (Some(Component::CurDir), Some(Component::CurDir)) => true,
        _ => false,
    }
}

fn path_for_toml(path: &Path) -> String {
    path.to_string_lossy().replace('\\', "/")
}

fn io_error(context: &'static str) -> impl Fn(std::io::Error) -> KainError {
    move |err| KainError::runtime(format!("Failed to {context}: {err}"))
}

#[cfg(test)]
mod tests {
    use super::*;
    use kain_core::{realtime_app_bundle_from_json, RuntimeReflectionPayload};
    use kain_ui::{ui_runtime_bundle_from_json, validate_ui_runtime_bundle};
    use serde_json::Value;
    use std::path::Path;
    use std::process::Command;
    use std::sync::Mutex;
    use tempfile::TempDir;

    static TEST_CWD_LOCK: Mutex<()> = Mutex::new(());

    fn native_ui_materialization_config(project_dir: PathBuf) -> NativeAppMaterializationConfig {
        NativeAppMaterializationConfig {
            project_dir,
            runtime_crate_name: "kain-ui-native".to_string(),
            runtime_dependency: NativeAppRuntimeDependency::Version("0.1.0".to_string()),
            artifact_output_dir: PathBuf::from("generated"),
            build_executable: false,
            release: false,
            executable_output_dir: None,
            cargo_target_dir: None,
            gpu_runtime_cargo_target_dir: None,
            launcher_entrypoint: NativeAppLauncherEntrypoint::default(),
            host_sidecars: Vec::new(),
        }
    }

    fn reloadable_native_ui_source(counter: i32, actor_extra_state: &str) -> String {
        format!(
            r#"
world Studio:
    state counter: Int = {counter}
    surface native_ui => App

actor ReloadDriver:
    state tick: Int = 0
{actor_extra_state}    on Ping(reply_to: P, step: Int):
        send reply_to.Reply(value = self.tick + step)

component App():
    render <panel title="Studio" />
"#
        )
    }

    #[test]
    fn discover_root_component_prefers_app_when_present() {
        let source = r#"
component Shell():
    render <panel />

component App():
    render <panel />
"#;

        let root = discover_native_app_root_component(source, None, "app.kn")
            .expect("component parse should succeed");
        assert_eq!(root.as_deref(), Some("App"));
    }

    #[test]
    fn discover_root_component_uses_single_world_native_ui_surface() {
        let source = r#"
world Studio:
    state counter: Int = 0
    surface native_ui => App
    surface viewport3d => "StudioPreview"
    surface web => App
    surface ue5 => "StudioBridge"

component App():
    render <panel />
"#;

        let root = discover_native_app_root_component(source, None, "app.kn")
            .expect("world root discovery should succeed");
        assert_eq!(root.as_deref(), Some("App"));
    }

    #[test]
    fn discover_root_component_requires_explicit_world_selection_when_multiple_worlds_exist() {
        let source = r#"
world Studio:
    state counter: Int = 0
    surface native_ui => App
    surface viewport3d => "StudioPreview"
    surface web => App
    surface ue5 => "StudioBridge"

world ShellWorld:
    state counter: Int = 0
    surface native_ui => Shell
    surface viewport3d => "ShellPreview"
    surface web => Shell
    surface ue5 => "ShellBridge"

component App():
    render <panel />

component Shell():
    render <panel />
"#;

        let error = discover_native_app_root_component(source, None, "app.kn")
            .expect_err("multiple worlds should require explicit selection");
        assert!(error
            .to_string()
            .contains("Multiple worlds declare native_ui surfaces"));
    }

    #[test]
    fn compile_native_app_bundle_collects_ui_and_rust_outputs() {
        let source = r#"
component App():
    render <panel title="Studio">
        <canvas title="Hero Surface" shader_ref="ui.hero_surface" />
    </panel>
"#;

        let bundle = compile_native_app_bundle(
            source,
            &NativeAppBundleConfig {
                app_name: Some("Studio Shell".to_string()),
                window_title: Some("Studio Shell".to_string()),
                root_component: None,
                source_file_name: Some("studio.kn".to_string()),
                source_root: None,
                initial_window_size: [1600.0, 900.0],
                include_spirv: true,
            },
        )
        .expect("native app bundle generation should succeed");

        assert_eq!(bundle.metadata.app_name, "studio-shell");
        assert_eq!(bundle.metadata.root_component, "App");
        assert_eq!(bundle.metadata.source_file_name, "studio.kn");
        assert_eq!(bundle.metadata.source_root, None);
        assert_eq!(bundle.runtime_contract.target, "rust");
        assert!(bundle
            .runtime_contract
            .required_capabilities
            .iter()
            .any(|capability| capability.key == "ui.runtime-bundle"));
        assert!(bundle.ui.tree.root.is_some());
        assert_eq!(bundle.ui_runtime_bundle.output, bundle.ui);
        assert!(bundle
            .ui_runtime_bundle
            .output
            .systems
            .surfaces
            .iter()
            .any(|surface| surface.gpu_backing_required));
        assert!(bundle.rust.bundle.primary.contents.contains("fn"));
    }

    #[test]
    fn compile_native_app_bundle_propagates_active_world_selection() {
        let source = r#"
world Studio:
    state counter: Int = 0
    surface native_ui => App
    surface viewport3d => "StudioPreview"
    surface web => App
    surface ue5 => "StudioBridge"

component App():
    render <panel title="Studio" />
"#;

        let bundle = compile_native_app_bundle(
            source,
            &NativeAppBundleConfig {
                app_name: Some("Studio Shell".to_string()),
                window_title: Some("Studio Shell".to_string()),
                source_file_name: Some("studio.kn".to_string()),
                ..Default::default()
            },
        )
        .expect("native app bundle generation should succeed");

        assert_eq!(
            bundle
                .realtime
                .active_world
                .as_ref()
                .map(|world| world.name.as_str()),
            Some("Studio")
        );
        assert_eq!(
            bundle
                .runtime_contract
                .active_world
                .as_ref()
                .map(|world| world.name.as_str()),
            Some("Studio")
        );
    }

    #[test]
    fn compile_native_app_bundle_supports_imported_world_and_entangle_modules() {
        let _guard = TEST_CWD_LOCK.lock().expect("cwd lock");
        let temp = TempDir::new().expect("temp dir");
        let main_dir = temp.path().join("src");
        let import_dir = temp.path().join("intentkit");
        let main_path = main_dir.join("main.kn");
        let import_path = import_dir.join("intents.kn");
        fs::create_dir_all(&main_dir).expect("main dir");
        fs::create_dir_all(&import_dir).expect("import dir");
        fs::write(
            &main_path,
            r#"
use intentkit::intents

component App():
    render <panel title="Studio" />
"#,
        )
        .expect("main source");
        fs::write(
            &import_path,
            r#"
world Physics:
    state hp: Int = 7
    surface native_ui => App

world Hud:
    state hp_display: Int = 7
    surface web => App

entangle Physics.hp <-> Hud.hp_display with single_writer
"#,
        )
        .expect("import source");

        let source = fs::read_to_string(&main_path).expect("read main source");
        let previous_dir = std::env::current_dir().expect("current dir");
        let result = (|| {
            std::env::set_current_dir(temp.path()).expect("set cwd");
            compile_native_app_bundle(
                &source,
                &NativeAppBundleConfig {
                    source_file_name: Some("main.kn".to_string()),
                    ..Default::default()
                },
            )
        })();
        std::env::set_current_dir(previous_dir).expect("restore cwd");

        let bundle = result.expect("native app bundle should accept imported entangle module");
        assert_eq!(bundle.metadata.root_component, "App");
        assert_eq!(
            bundle
                .realtime
                .active_world
                .as_ref()
                .map(|world| world.name.as_str()),
            Some("Physics")
        );
        assert_eq!(bundle.realtime.entanglements.len(), 1);
        assert_eq!(bundle.realtime.entanglements[0].authority, "Physics.hp");
        assert_eq!(bundle.realtime.entanglements[0].mirror, "Hud.hp_display");
    }

    #[test]
    fn materialize_native_app_bundle_writes_scaffold_and_artifacts() {
        let temp = TempDir::new().expect("temp dir");
        let project_dir = temp.path().join("native-app");
        let source = r#"
component App():
    render <panel title="Bundle Test" />
"#;
        let bundle = compile_native_app_bundle(
            source,
            &NativeAppBundleConfig {
                source_file_name: Some("bundle_test.kn".to_string()),
                ..Default::default()
            },
        )
        .expect("bundle should compile");

        let materialized = materialize_native_app_bundle(
            source,
            &bundle,
            &native_ui_materialization_config(project_dir.clone()),
        )
        .expect("materialization should succeed");

        assert_eq!(materialized.project_dir, project_dir);
        assert!(materialized.manifest_path.exists());
        assert!(materialized.main_rs_path.exists());
        assert!(materialized.source_copy_path.exists());
        assert!(!materialized.artifact_paths.is_empty());

        let manifest = fs::read_to_string(&materialized.manifest_path).expect("manifest");
        assert!(manifest.contains("kain-ui-native"));
        let main_rs = fs::read_to_string(&materialized.main_rs_path).expect("main.rs");
        assert!(main_rs.contains("run_bundled_app_json"));
        assert!(main_rs.contains("KAIN_UI_NATIVE_RUNTIME_BUNDLE"));
        assert!(main_rs.contains("KAIN_UI_NATIVE_REALTIME_BUNDLE"));
        assert!(project_dir
            .join("config")
            .join("app_manifest.json")
            .exists());
        assert!(project_dir
            .join("state")
            .join("runtime_snapshot.json")
            .exists());
        assert!(materialized
            .artifact_paths
            .iter()
            .any(|path| path.ends_with(NATIVE_APP_RUNTIME_BUNDLE_FILE_NAME)));
        assert!(materialized
            .artifact_paths
            .iter()
            .any(|path| path.ends_with(NATIVE_APP_RUNTIME_CONTRACT_FILE_NAME)));
    }

    #[test]
    fn materialize_native_app_bundle_emits_std_reload_participants_for_worlds_and_actors() {
        let temp = TempDir::new().expect("temp dir");
        let project_dir = temp.path().join("native-app-reload-contract");
        let source = reloadable_native_ui_source(0, "");
        let bundle = compile_native_app_bundle(
            &source,
            &NativeAppBundleConfig {
                source_file_name: Some("reload_contract.kn".to_string()),
                ..Default::default()
            },
        )
        .expect("bundle should compile");

        let materialized = materialize_native_app_bundle(
            &source,
            &bundle,
            &native_ui_materialization_config(project_dir.clone()),
        )
        .expect("materialization should succeed");

        let manifest_json =
            fs::read_to_string(project_dir.join("config").join("app_manifest.json"))
                .expect("app manifest");
        let manifest: Value = serde_json::from_str(&manifest_json).expect("manifest json");
        let participants = &manifest["hot_reload"]["participants"];
        assert_eq!(participants["package_surface"], "std::reload");
        assert_eq!(participants["default_state_migration"], "auto-structural");
        assert_eq!(
            participants["default_restart_mode"],
            "restart-with-snapshot-restore"
        );
        assert_eq!(participants["compatibility_lanes"][0], "cold-start");
        assert_eq!(
            participants["compatibility_lanes"][6],
            "restart-with-restore"
        );
        assert_eq!(participants["worlds"][0]["name"], "Studio");
        assert_eq!(
            participants["worlds"][0]["state_fields"][0]["name"],
            "counter"
        );
        assert_eq!(participants["actors"][0]["name"], "ReloadDriver");
        assert_eq!(participants["actors"][0]["state_fields"][0]["name"], "tick");
        assert_eq!(participants["gpu_hooks"]["swap_boundary"], "frame-boundary");
        assert_eq!(manifest["hot_reload"]["transition"]["class"], "cold-start");

        let runtime_snapshot_json =
            fs::read_to_string(project_dir.join("state").join("runtime_snapshot.json"))
                .expect("runtime snapshot");
        let runtime_snapshot: Value =
            serde_json::from_str(&runtime_snapshot_json).expect("snapshot json");
        assert_eq!(runtime_snapshot["reload"]["package_surface"], "std::reload");
        assert_eq!(
            runtime_snapshot["reload"]["default_restart_mode"],
            "restart-with-snapshot-restore"
        );
        assert_eq!(
            runtime_snapshot["reload"]["actors"][0]["name"],
            "ReloadDriver"
        );
        assert!(materialized
            .artifact_paths
            .iter()
            .any(|path| path.ends_with(NATIVE_RUNTIME_REFLECTION_PAYLOAD_FILE_NAME)));
    }

    #[test]
    fn native_app_hot_reload_uses_structural_participant_gating() {
        let temp = TempDir::new().expect("temp dir");
        let project_dir = temp.path().join("native-app-reload-compat");
        let source_v1 = reloadable_native_ui_source(0, "");
        let source_v2 = reloadable_native_ui_source(7, "");
        let source_v3 = reloadable_native_ui_source(7, "    state phase: Int = 0\n");

        let bundle_v1 = compile_native_app_bundle(
            &source_v1,
            &NativeAppBundleConfig {
                source_file_name: Some("reload_compat.kn".to_string()),
                ..Default::default()
            },
        )
        .expect("bundle v1 should compile");
        materialize_native_app_bundle(
            &source_v1,
            &bundle_v1,
            &native_ui_materialization_config(project_dir.clone()),
        )
        .expect("materialize v1");

        let bundle_v2 = compile_native_app_bundle(
            &source_v2,
            &NativeAppBundleConfig {
                source_file_name: Some("reload_compat.kn".to_string()),
                ..Default::default()
            },
        )
        .expect("bundle v2 should compile");
        materialize_native_app_bundle(
            &source_v2,
            &bundle_v2,
            &native_ui_materialization_config(project_dir.clone()),
        )
        .expect("materialize v2");
        let manifest_v2: Value = serde_json::from_str(
            &fs::read_to_string(project_dir.join("config").join("app_manifest.json"))
                .expect("manifest v2"),
        )
        .expect("manifest v2 json");
        assert_eq!(
            manifest_v2["hot_reload"]["reload_compatible_with_previous"],
            Value::Bool(true)
        );
        assert_eq!(
            manifest_v2["hot_reload"]["transition"]["class"],
            "quiesce-and-migrate"
        );

        let bundle_v3 = compile_native_app_bundle(
            &source_v3,
            &NativeAppBundleConfig {
                source_file_name: Some("reload_compat.kn".to_string()),
                ..Default::default()
            },
        )
        .expect("bundle v3 should compile");
        materialize_native_app_bundle(
            &source_v3,
            &bundle_v3,
            &native_ui_materialization_config(project_dir.clone()),
        )
        .expect("materialize v3");
        let manifest_v3: Value = serde_json::from_str(
            &fs::read_to_string(project_dir.join("config").join("app_manifest.json"))
                .expect("manifest v3"),
        )
        .expect("manifest v3 json");
        assert_eq!(
            manifest_v3["hot_reload"]["reload_compatible_with_previous"],
            Value::Bool(false)
        );
        assert_eq!(
            manifest_v3["hot_reload"]["transition"]["class"],
            "restart-with-restore"
        );
    }

    #[test]
    fn materialize_native_app_bundle_packages_shader_canvas_font_assets() {
        let temp = TempDir::new().expect("temp dir");
        let project_dir = temp.path().join("native-app-font-assets");
        let font_path = temp.path().join("hero_font.ttf");
        fs::write(&font_path, b"dummy-font-bytes").expect("font file");
        let source = format!(
            r#"
shader fragment hero_surface(uv: Vec2) -> Vec4:
    return vec4(uv.x, uv.y, 1.0, 1.0)

component App():
    render <panel>
        <canvas title="Hero Surface" text="Fast lane" shader_ref="hero_surface" shader_stage="fragment" shader_format="spirv" font_asset="{}" />
    </panel>
"#,
            path_for_toml(&font_path)
        );

        let bundle = compile_native_app_bundle(
            &source,
            &NativeAppBundleConfig {
                source_file_name: Some("font_asset_test.kn".to_string()),
                ..Default::default()
            },
        )
        .expect("bundle should compile");

        let materialized = materialize_native_app_bundle(
            &source,
            &bundle,
            &native_ui_materialization_config(project_dir.clone()),
        )
        .expect("materialization should succeed");

        let realtime_bundle_path = materialized
            .artifact_paths
            .iter()
            .find(|path| path.ends_with(NATIVE_APP_REALTIME_BUNDLE_FILE_NAME))
            .expect("realtime bundle sidecar");
        let realtime_bundle_json =
            fs::read_to_string(realtime_bundle_path).expect("realtime bundle");
        let realtime_bundle =
            realtime_app_bundle_from_json(&realtime_bundle_json).expect("parse realtime bundle");
        let font_asset = realtime_bundle
            .assets
            .iter()
            .find(|asset| asset.kind == "font")
            .expect("font asset binding");

        assert_eq!(
            font_asset.key,
            format!("font::{}", path_for_toml(&font_path))
        );
        assert_ne!(font_asset.source, path_for_toml(&font_path));
        assert!(font_asset.source.starts_with("kain_asset_font_"));
        assert!(materialized.artifact_paths.iter().any(|path| {
            path.file_name().and_then(OsStr::to_str) == Some(font_asset.source.as_str())
        }));
        assert_eq!(
            realtime_bundle.shader_canvases[0].font_atlases[0]
                .asset_key
                .as_deref(),
            Some(font_asset.key.as_str())
        );
    }

    #[test]
    fn materialize_native_app_bundle_resolves_relative_font_assets_from_source_root() {
        let _guard = TEST_CWD_LOCK.lock().expect("cwd lock");
        let temp = TempDir::new().expect("temp dir");
        let source_root = temp.path().join("source-root");
        let font_dir = source_root.join("fonts");
        fs::create_dir_all(&font_dir).expect("font dir");
        let font_path = font_dir.join("hero_font.ttf");
        fs::write(&font_path, b"dummy-font-bytes").expect("font file");
        let project_dir = temp.path().join("native-app-relative-font-assets");
        let source = r#"
shader fragment hero_surface(uv: Vec2) -> Vec4:
    return vec4(uv.x, uv.y, 1.0, 1.0)

component App():
    render <panel>
        <canvas title="Hero Surface" text="Fast lane" shader_ref="hero_surface" shader_stage="fragment" shader_format="spirv" font_asset="fonts/hero_font.ttf" />
    </panel>
"#;

        let bundle = compile_native_app_bundle(
            source,
            &NativeAppBundleConfig {
                source_file_name: Some("relative_font_asset_test.kn".to_string()),
                source_root: Some(source_root.clone()),
                ..Default::default()
            },
        )
        .expect("bundle should compile");

        assert_eq!(
            bundle.metadata.source_root.as_deref(),
            Some(source_root.as_path())
        );

        let unrelated_cwd = temp.path().join("other-cwd");
        fs::create_dir_all(&unrelated_cwd).expect("other cwd");
        let previous_dir = std::env::current_dir().expect("current dir");
        let result: Result<(), KainError> = (|| {
            std::env::set_current_dir(&unrelated_cwd).expect("set cwd");
            let materialized = materialize_native_app_bundle(
                source,
                &bundle,
                &native_ui_materialization_config(project_dir.clone()),
            )
            .expect("materialization should succeed");

            let realtime_bundle_path = materialized
                .artifact_paths
                .iter()
                .find(|path| path.ends_with(NATIVE_APP_REALTIME_BUNDLE_FILE_NAME))
                .expect("realtime bundle sidecar");
            let realtime_bundle_json =
                fs::read_to_string(realtime_bundle_path).expect("realtime bundle");
            let realtime_bundle = realtime_app_bundle_from_json(&realtime_bundle_json)
                .expect("parse realtime bundle");
            let font_asset = realtime_bundle
                .assets
                .iter()
                .find(|asset| asset.kind == "font")
                .expect("font asset binding");

            assert_eq!(font_asset.key, "font::fonts/hero_font.ttf");
            assert_ne!(font_asset.source, "fonts/hero_font.ttf");
            assert!(font_asset.source.starts_with("kain_asset_font_"));
            assert!(materialized.artifact_paths.iter().any(|path| {
                path.file_name().and_then(OsStr::to_str) == Some(font_asset.source.as_str())
            }));
            Ok(())
        })();
        std::env::set_current_dir(previous_dir).expect("restore cwd");
        result.expect("relative font asset packaging result");
    }

    #[test]
    fn materialize_native_app_bundle_includes_runtime_version_metadata() {
        let temp = TempDir::new().expect("temp dir");
        let project_dir = temp.path().join("native-app-with-version");
        let source = r#"
component App():
    render <panel title="Version Test" />
"#;
        let bundle = compile_native_app_bundle(
            source,
            &NativeAppBundleConfig {
                source_file_name: Some("version_test.kn".to_string()),
                ..Default::default()
            },
        )
        .expect("bundle should compile");

        // Check that runtime version metadata was loaded (if manifest is available)
        if bundle.runtime_version.is_some() {
            let version = bundle.runtime_version.as_ref().unwrap();
            assert_eq!(version.abi_major, 0);
            assert_eq!(version.abi_minor, 1);
            assert_eq!(version.runtime_major, 0);
            assert_eq!(version.runtime_minor, 1);
            assert_eq!(version.abi_version_string, "0.1.0");
            assert_eq!(version.runtime_version_string, "0.1.0");
            assert_eq!(version.compatibility_class, "experimental");
            assert_eq!(version.runtime_lane, "raw-native");
            assert!(version
                .target_platforms
                .iter()
                .any(|platform| platform == "windows"));
            assert!(version
                .target_platforms
                .iter()
                .any(|platform| platform == "linux"));
            assert!(version
                .active_platforms
                .iter()
                .any(|platform| platform == std::env::consts::OS));
            assert_eq!(
                bundle
                    .runtime_contract
                    .compatibility
                    .compatibility_class
                    .as_deref(),
                Some("experimental")
            );
            assert_eq!(
                bundle
                    .runtime_contract
                    .compatibility
                    .runtime_lane
                    .as_deref(),
                Some("raw-native")
            );
            assert!(bundle
                .runtime_contract
                .compatibility
                .runtime_version
                .is_some());
            assert!(bundle.runtime_contract.compatibility.abi_version.is_some());
            assert!(bundle
                .runtime_contract
                .compatibility
                .platform_availability
                .is_some());
            let platform = bundle
                .runtime_contract
                .compatibility
                .platform_availability
                .as_ref()
                .unwrap();
            assert_eq!(platform.schema_version, 1);
            assert!(platform
                .target_platforms
                .iter()
                .any(|platform| platform == "windows"));
            assert!(platform
                .target_platforms
                .iter()
                .any(|platform| platform == "linux"));
            assert!(platform
                .active_platforms
                .iter()
                .any(|platform| platform == std::env::consts::OS));
            assert_eq!(
                platform.runtime_platform.as_deref(),
                Some(std::env::consts::OS)
            );
        }

        let materialized = materialize_native_app_bundle(
            source,
            &bundle,
            &native_ui_materialization_config(project_dir.clone()),
        )
        .expect("materialization should succeed");

        // Check if runtime version metadata file was written (if metadata was available)
        if bundle.runtime_version.is_some() {
            let version_metadata_path = materialized
                .artifact_paths
                .iter()
                .find(|path| path.ends_with(NATIVE_RUNTIME_VERSION_METADATA_FILE_NAME));
            let compatibility_metadata_path = materialized
                .artifact_paths
                .iter()
                .find(|path| path.ends_with(NATIVE_APP_RUNTIME_COMPATIBILITY_FILE_NAME));

            assert!(
                version_metadata_path.is_some(),
                "Runtime version metadata file should be written when metadata is available"
            );
            assert!(
                compatibility_metadata_path.is_some(),
                "Runtime compatibility metadata file should be written when metadata is available"
            );

            if let Some(path) = version_metadata_path {
                assert!(path.exists(), "Runtime version metadata file should exist");
                let metadata_json = fs::read_to_string(path).expect("read version metadata");
                assert!(metadata_json.contains("abi_version_string"));
                assert!(metadata_json.contains("runtime_version_string"));
                assert!(metadata_json.contains("0.1.0"));
            }

            if let Some(path) = compatibility_metadata_path {
                assert!(
                    path.exists(),
                    "Runtime compatibility metadata file should exist"
                );
                let metadata_json = fs::read_to_string(path).expect("read compatibility metadata");
                assert!(metadata_json.contains("\"bundle_target\""));
                assert!(metadata_json.contains("\"runtime_lane\""));
                assert!(metadata_json.contains("\"compatibility_class\""));
                assert!(metadata_json.contains("\"platform_availability\""));
                assert!(metadata_json.contains("\"target_platforms\""));
                assert!(metadata_json.contains("\"active_platforms\""));
                assert!(metadata_json.contains("\"runtime_platform\""));
                assert!(metadata_json.contains("\"install\""));
                assert!(metadata_json.contains("\"update\""));
                assert!(metadata_json.contains("\"uninstall\""));
            }
        }
    }

    #[test]
    fn materialize_native_app_bundle_round_trips_emitted_sidecars() {
        let temp = TempDir::new().expect("temp dir");
        let project_dir = temp.path().join("native-app-end-to-end");
        let source = r#"
component App():
    render <panel title="End-to-End Bundle Proof" />

shader compute SampleCompute(id: UVec3) -> Vec4:
    uniform src: StorageBuffer<Vec4> @0
    uniform dst: StorageBuffer<Vec4> @1
    return vec4(1.0, 1.0, 1.0, 1.0)
"#;

        let bundle = compile_native_app_bundle(
            source,
            &NativeAppBundleConfig {
                app_name: Some("Bundle Proof".to_string()),
                window_title: Some("Bundle Proof".to_string()),
                source_file_name: Some("bundle_proof.kn".to_string()),
                ..Default::default()
            },
        )
        .expect("bundle should compile");

        assert!(bundle.runtime_contract.reflection_payload.is_some());
        assert!(bundle.runtime_version.is_some());

        let materialized = materialize_native_app_bundle(
            source,
            &bundle,
            &native_ui_materialization_config(project_dir.clone()),
        )
        .expect("materialization should succeed");

        let runtime_bundle_path = materialized
            .artifact_paths
            .iter()
            .find(|path| path.ends_with(NATIVE_APP_RUNTIME_BUNDLE_FILE_NAME))
            .expect("runtime bundle sidecar");
        let runtime_contract_path = materialized
            .artifact_paths
            .iter()
            .find(|path| path.ends_with(NATIVE_APP_RUNTIME_CONTRACT_FILE_NAME))
            .expect("runtime contract sidecar");
        let realtime_bundle_path = materialized
            .artifact_paths
            .iter()
            .find(|path| path.ends_with(NATIVE_APP_REALTIME_BUNDLE_FILE_NAME))
            .expect("realtime bundle sidecar");
        let reflection_payload_path = materialized
            .artifact_paths
            .iter()
            .find(|path| path.ends_with(NATIVE_RUNTIME_REFLECTION_PAYLOAD_FILE_NAME))
            .expect("reflection payload sidecar");
        let compute_residency_path = materialized
            .artifact_paths
            .iter()
            .find(|path| path.ends_with(COMPUTE_RESIDENCY_FILE_NAME))
            .expect("compute residency sidecar");
        let compute_residency_payload_path = materialized
            .artifact_paths
            .iter()
            .find(|path| {
                path.file_name()
                    .and_then(|value| value.to_str())
                    .is_some_and(|value| value.ends_with(".bin"))
            })
            .expect("compute residency payload sidecar");

        let runtime_bundle_json = fs::read_to_string(runtime_bundle_path).expect("runtime bundle");
        let runtime_bundle =
            ui_runtime_bundle_from_json(&runtime_bundle_json).expect("parse runtime bundle");
        validate_ui_runtime_bundle(&runtime_bundle).expect("validate runtime bundle");
        assert_eq!(
            runtime_bundle.metadata.app_name.as_deref(),
            Some("bundle-proof")
        );
        assert_eq!(runtime_bundle.metadata.window_title, "Bundle Proof");
        assert_eq!(runtime_bundle.metadata.root_component, "App");
        assert_eq!(runtime_bundle, bundle.ui_runtime_bundle);
        assert_eq!(runtime_bundle.output, bundle.ui);

        let runtime_contract_json =
            fs::read_to_string(runtime_contract_path).expect("runtime contract");
        let runtime_contract: RuntimeContractBundle =
            serde_json::from_str(&runtime_contract_json).expect("parse runtime contract");
        assert_eq!(runtime_contract.target, "rust");
        assert_eq!(runtime_contract, bundle.runtime_contract);

        let realtime_bundle_json = fs::read_to_string(realtime_bundle_path).expect("realtime");
        let realtime_bundle =
            realtime_app_bundle_from_json(&realtime_bundle_json).expect("parse realtime bundle");
        assert_eq!(realtime_bundle, bundle.realtime);

        let reflection_payload_json =
            fs::read_to_string(reflection_payload_path).expect("reflection payload");
        let reflection_payload: RuntimeReflectionPayload =
            serde_json::from_str(&reflection_payload_json).expect("parse reflection payload");
        assert_eq!(
            &reflection_payload,
            bundle
                .runtime_contract
                .reflection_payload
                .as_ref()
                .expect("bundle reflection payload")
        );

        let compute_residency_json =
            fs::read_to_string(compute_residency_path).expect("compute residency");
        let compute_residency: crate::compute_residency::ComputeResidencyBundle =
            serde_json::from_str(&compute_residency_json).expect("parse compute residency");
        assert_eq!(compute_residency.compute_shader_count, 1);
        assert_eq!(compute_residency.compute_shaders.len(), 1);
        assert!(compute_residency_json.contains("SampleCompute"));
        assert_eq!(compute_residency.compute_shaders[0].bindings.len(), 2);
        assert_eq!(
            compute_residency.compute_shaders[0].bindings[0].descriptor_kind,
            "storage_buffer"
        );
        assert_eq!(
            compute_residency.compute_shaders[0].bindings[0].payload_file,
            compute_residency_payload_path
                .file_name()
                .and_then(|value| value.to_str())
                .expect("payload file name")
        );

        let compute_residency_payload =
            fs::read(compute_residency_payload_path).expect("compute payload");
        assert_eq!(
            compute_residency_payload.len(),
            compute_residency.compute_shaders[0].bindings[0].byte_length
        );
        let main_rs = fs::read_to_string(&materialized.main_rs_path).expect("main.rs");
        assert!(main_rs.contains("KAIN_COMPUTE_RESIDENCY"));

        if let Some(version_path) = materialized
            .artifact_paths
            .iter()
            .find(|path| path.ends_with(NATIVE_RUNTIME_VERSION_METADATA_FILE_NAME))
        {
            let version_json = fs::read_to_string(version_path).expect("runtime version");
            let version: RuntimeVersionMetadata =
                serde_json::from_str(&version_json).expect("parse runtime version");
            let expected = bundle
                .runtime_version
                .as_ref()
                .expect("bundle runtime version");
            assert_eq!(version.runtime_major, expected.runtime_major);
            assert_eq!(version.runtime_minor, expected.runtime_minor);
            assert_eq!(version.runtime_patch, expected.runtime_patch);
            assert_eq!(version.abi_major, expected.abi_major);
            assert_eq!(version.abi_minor, expected.abi_minor);
            assert_eq!(version.abi_patch, expected.abi_patch);
            assert_eq!(
                version.runtime_version_string,
                expected.runtime_version_string
            );
            assert_eq!(version.abi_version_string, expected.abi_version_string);
            assert_eq!(version.compatibility_class, expected.compatibility_class);
            assert_eq!(version.runtime_lane, expected.runtime_lane);
            assert_eq!(version.target_platforms, expected.target_platforms);
            assert_eq!(version.active_platforms, expected.active_platforms);
        }
    }

    #[test]
    fn materialize_native_app_bundle_packages_c_ffi_bridges_for_native_ui() {
        let _guard = TEST_CWD_LOCK.lock().expect("cwd lock");
        let temp = TempDir::new().expect("temp dir");
        let project_root = temp.path();
        let project_dir = project_root.join("native-app-cffi");
        let native_dir = project_root.join("native");
        fs::create_dir_all(&native_dir).expect("native dir");

        let header_path = native_dir.join("beacon_math.h");
        let source_path = native_dir.join("beacon_math.c");
        let dll_path = if cfg!(target_os = "windows") {
            native_dir.join("beacon_math.dll")
        } else if cfg!(target_os = "macos") {
            native_dir.join("libbeacon_math.dylib")
        } else {
            native_dir.join("libbeacon_math.so")
        };
        fs::write(
            &header_path,
            "#if defined(_WIN32)\n#define BEACON_EXPORT __declspec(dllexport)\n#else\n#define BEACON_EXPORT\n#endif\nBEACON_EXPORT int beacon_add(int a, int b);\n",
        )
        .expect("header");
        fs::write(
            &source_path,
            "#include \"beacon_math.h\"\nint beacon_add(int a, int b) { return a + b; }\n",
        )
        .expect("source");
        compile_shared_library(&source_path, &dll_path);
        fs::write(
            project_root.join("KAIN.toml"),
            format!(
                "[c_ffi]\n\n[[c_ffi.libraries]]\nname = \"beacon_math\"\nheader = \"{}\"\nshared_lib = \"{}\"\n",
                header_path
                    .strip_prefix(project_root)
                    .unwrap()
                    .display()
                    .to_string()
                    .replace('\\', "/"),
                dll_path
                    .strip_prefix(project_root)
                    .unwrap()
                    .display()
                    .to_string()
                    .replace('\\', "/")
            ),
        )
        .expect("manifest");

        let previous_dir = std::env::current_dir().expect("current dir");
        let result: Result<(), KainError> = (|| {
            std::env::set_current_dir(project_root).expect("set cwd");
            let source = r#"
use c::beacon_math

component App():
    render <panel title="C FFI Native UI" />
"#;
            let bundle = compile_native_app_bundle(
                source,
                &NativeAppBundleConfig {
                    source_file_name: Some("cffi_app.kn".to_string()),
                    ..Default::default()
                },
            )
            .expect("bundle should compile");

            let materialized = materialize_native_app_bundle(
                source,
                &bundle,
                &native_ui_materialization_config(project_dir.clone()),
            )
            .expect("materialization should succeed");

            assert!(bundle
                .runtime_contract
                .required_capabilities
                .iter()
                .any(|capability| capability.key == "c.ffi"));
            assert!(bundle
                .runtime_contract
                .service_bindings
                .iter()
                .any(|binding| binding.service == "c.beacon_math.bridge"));
            assert!(materialized
                .artifact_paths
                .iter()
                .any(|path| path.ends_with(NATIVE_APP_C_FFI_PACKAGE_MANIFEST_FILE_NAME)));
            assert!(materialized.artifact_paths.iter().any(|path| {
                path.file_name()
                    .and_then(|value| value.to_str())
                    .is_some_and(|value| value.contains("kain_c_ffi_bridge_beacon_math"))
            }));
            let main_rs = fs::read_to_string(&materialized.main_rs_path).expect("main.rs");
            assert!(main_rs.contains("load_packaged_bridges_from_manifest"));
            let cargo_manifest =
                fs::read_to_string(&materialized.manifest_path).expect("cargo manifest");
            assert!(cargo_manifest.contains("kain-c-ffi"));
            Ok(())
        })();
        std::env::set_current_dir(previous_dir).expect("restore cwd");
        result.expect("c ffi native app packaging result");
    }

    #[test]
    fn materialize_native_app_bundle_supports_generic_host_sidecars_for_custom_runtime() {
        let temp = TempDir::new().expect("temp dir");
        let project_dir = temp.path().join("native-app-fast3d");
        let host_config_path = temp.path().join("fast3d_host.json");
        let scene_manifest_path = temp.path().join("scene_manifest_title_face.json");
        fs::write(
            &host_config_path,
            r#"{
  "action": "snapshot",
  "manifest_path": "scene_manifest_title_face.json",
  "output_path": "native_host_snapshot.png",
  "time_seconds": 0.0
}"#,
        )
        .expect("host config");
        fs::write(&scene_manifest_path, "{}").expect("scene manifest");
        let source = r#"
component App():
    render <panel title="Fast3D Host" />
"#;
        let bundle = compile_native_app_bundle(
            source,
            &NativeAppBundleConfig {
                source_file_name: Some("fast3d_host.kn".to_string()),
                ..Default::default()
            },
        )
        .expect("bundle should compile");

        let materialized = materialize_native_app_bundle(
            source,
            &bundle,
            &NativeAppMaterializationConfig {
                project_dir: project_dir.clone(),
                runtime_crate_name: "kain-fast3d-runtime".to_string(),
                runtime_dependency: NativeAppRuntimeDependency::Version("0.1.0".to_string()),
                artifact_output_dir: PathBuf::from("generated"),
                build_executable: false,
                release: false,
                executable_output_dir: None,
                cargo_target_dir: None,
                gpu_runtime_cargo_target_dir: None,
                launcher_entrypoint: NativeAppLauncherEntrypoint::RunNoArgFunction {
                    function_name: "run_fast3d_cli".to_string(),
                },
                host_sidecars: vec![
                    NativeAppHostSidecarBinding {
                        source_path: host_config_path.clone(),
                        packaged_file_name: Some("fast3d_host.json".to_string()),
                        env_var: Some("KAIN_FAST3D_CONFIG".to_string()),
                    },
                    NativeAppHostSidecarBinding {
                        source_path: scene_manifest_path.clone(),
                        packaged_file_name: Some("scene_manifest_title_face.json".to_string()),
                        env_var: None,
                    },
                ],
            },
        )
        .expect("materialization should succeed");

        let main_rs = fs::read_to_string(&materialized.main_rs_path).expect("main.rs");
        assert!(main_rs.contains("run_fast3d_cli"));
        assert!(main_rs.contains("KAIN_FAST3D_CONFIG"));
        assert!(!main_rs.contains("run_bundled_app_json(KAIN_RUNTIME_BUNDLE)"));

        let packaged_host_config = materialized
            .artifact_paths
            .iter()
            .find(|path| path.file_name().and_then(OsStr::to_str) == Some("fast3d_host.json"))
            .expect("packaged host config");
        let packaged_scene_manifest = materialized
            .artifact_paths
            .iter()
            .find(|path| {
                path.file_name().and_then(OsStr::to_str) == Some("scene_manifest_title_face.json")
            })
            .expect("packaged scene manifest");
        assert!(packaged_host_config.exists());
        assert!(packaged_scene_manifest.exists());

        let app_manifest_path = project_dir.join("config").join("app_manifest.json");
        let app_manifest_json = fs::read_to_string(app_manifest_path).expect("app manifest");
        assert!(app_manifest_json.contains("fast3d_host.json"));
        assert!(app_manifest_json.contains("KAIN_FAST3D_CONFIG"));
        assert!(app_manifest_json.contains("scene_manifest_title_face.json"));

        let cargo_manifest =
            fs::read_to_string(&materialized.manifest_path).expect("cargo manifest");
        assert!(cargo_manifest.contains("kain-fast3d-runtime"));
    }

    fn compile_shared_library(source: &Path, output: &Path) {
        let mut command = Command::new("clang");
        if cfg!(target_os = "windows") {
            command.args(["-shared", "-O2"]);
        } else {
            command.args(["-shared", "-fPIC", "-O2"]);
        }
        let status = command
            .arg(source)
            .arg("-o")
            .arg(output)
            .status()
            .expect("clang should launch for native-app c ffi test");
        assert!(status.success(), "clang should build C FFI shared library");
    }
}
