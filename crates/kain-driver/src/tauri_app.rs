use std::ffi::OsStr;
use std::fs;
use std::path::{Component, Path, PathBuf};
use std::process::Command;
use std::time::{SystemTime, UNIX_EPOCH};

use crate::native_app::{
    build_native_app_reload_participants, NativeAppBundle, NativeAppBundleConfig,
    NativeAppHotReloadParticipants,
};
use crate::{DriverSession, HybridArtifactOutput};
use kain_core::error::KainError;
use kain_core::{
    realtime_app_bundle_to_json, runtime_contract_bundle_to_json, RealtimeAppBundle,
    RealtimeAssetBinding,
};
use kain_reflect::TypeRegistry;
use kain_ui::{ui_runtime_bundle_to_json, UiRuntimeBundle};
use kain_ui_tauri::{
    build_tauri_bridge_manifest, default_tauri_capability_presets, default_tauri_plugin_presets,
    patch_hybrid_wasm_reference, render_frontend_bridge_js, render_frontend_index_html,
    render_tauri_project_files, retarget_ui_runtime_bundle_for_tauri, TauriBridgeManifest,
    TauriBridgeManifestConfig, TauriCapabilityPreset, TauriFrontendAssetManifest,
    TauriPermissionPreset, TauriPluginPreset, TauriProjectRenderConfig,
    KAIN_TAURI_BRIDGE_MANIFEST_FILE_NAME, KAIN_TAURI_FRONTEND_BRIDGE_FILE_NAME,
    KAIN_TAURI_FRONTEND_DESCRIPTOR_FILE_NAME, KAIN_TAURI_FRONTEND_ENTRY_FILE_NAME,
};
use serde::Serialize;
use serde_json::json;
use sha2::{Digest, Sha256};

const TAURI_RUNTIME_BUNDLE_FILE_NAME: &str = "native_app_bundle.json";
const TAURI_RUNTIME_CONTRACT_FILE_NAME: &str = "kain_runtime_contract.json";
const TAURI_RUNTIME_COMPATIBILITY_FILE_NAME: &str = "kain_runtime_compatibility.json";
const TAURI_REALTIME_BUNDLE_FILE_NAME: &str = "kain_realtime_app_bundle.json";
const TAURI_SHADER_BUNDLE_FILE_NAME: &str = "kain_shader_bundle.json";
const TAURI_RUNTIME_VERSION_METADATA_FILE_NAME: &str = "kain_runtime_version.json";
const TAURI_RUNTIME_REFLECTION_PAYLOAD_FILE_NAME: &str = "kain_reflection_payload.json";
const TAURI_CONFIG_DIR_NAME: &str = "config";
const TAURI_STATE_DIR_NAME: &str = "state";
const TAURI_FRONTEND_DIR_NAME: &str = "frontend";
const TAURI_ARTIFACT_OUTPUT_DIR_NAME: &str = "generated";
const TAURI_APP_MANIFEST_FILE_NAME: &str = "app_manifest.json";
const TAURI_RUNTIME_SNAPSHOT_FILE_NAME: &str = "runtime_snapshot.json";
const TAURI_WASM_FILE_NAME: &str = "main.wasm";
const TAURI_JS_FILE_NAME: &str = "main.js";
const TAURI_TS_FILE_NAME: &str = "main.ts";

#[derive(Debug, Clone)]
pub struct TauriAppBundleConfig {
    pub native_app: NativeAppBundleConfig,
}

impl Default for TauriAppBundleConfig {
    fn default() -> Self {
        Self {
            native_app: NativeAppBundleConfig::default(),
        }
    }
}

#[derive(Debug, Clone)]
pub struct TauriAppBundle {
    pub metadata: crate::NativeAppMetadata,
    pub native_app: NativeAppBundle,
    pub ui_runtime_bundle: UiRuntimeBundle,
    pub hybrid: HybridArtifactOutput,
}

#[derive(Debug, Clone)]
pub struct TauriAppMaterializationConfig {
    pub project_dir: PathBuf,
    pub artifact_output_dir: PathBuf,
    pub build_executable: bool,
    pub release: bool,
    pub cargo_target_dir: Option<PathBuf>,
    pub bundle_identifier: Option<String>,
    pub window_label: Option<String>,
    pub cargo_package_name: Option<String>,
    pub plugin_presets: Vec<TauriPluginPreset>,
    pub capability_presets: Vec<TauriCapabilityPreset>,
    pub permission_presets: Vec<TauriPermissionPreset>,
    pub host_type_registry: TypeRegistry,
}

impl Default for TauriAppMaterializationConfig {
    fn default() -> Self {
        Self {
            project_dir: PathBuf::from("kain-tauri-app"),
            artifact_output_dir: PathBuf::from(TAURI_ARTIFACT_OUTPUT_DIR_NAME),
            build_executable: true,
            release: false,
            cargo_target_dir: None,
            bundle_identifier: None,
            window_label: None,
            cargo_package_name: None,
            plugin_presets: default_tauri_plugin_presets(),
            capability_presets: default_tauri_capability_presets(),
            permission_presets: Vec::new(),
            host_type_registry: TypeRegistry::default(),
        }
    }
}

#[derive(Debug, Clone)]
pub struct TauriAppMaterializedPaths {
    pub project_dir: PathBuf,
    pub src_tauri_manifest_path: PathBuf,
    pub src_tauri_main_rs_path: PathBuf,
    pub source_copy_path: PathBuf,
    pub bridge_manifest_path: PathBuf,
    pub frontend_dir: PathBuf,
    pub artifact_paths: Vec<PathBuf>,
    pub executable_path: Option<PathBuf>,
}

impl DriverSession {
    pub fn compile_tauri_app_bundle(
        &self,
        source: &str,
        config: &TauriAppBundleConfig,
    ) -> Result<TauriAppBundle, KainError> {
        let native_app = self.compile_native_app_bundle(source, &config.native_app)?;
        let ui_runtime_bundle = retarget_ui_runtime_bundle_for_tauri(&native_app.ui_runtime_bundle);
        let hybrid = self.compile_hybrid_artifacts(source)?;

        Ok(TauriAppBundle {
            metadata: native_app.metadata.clone(),
            native_app,
            ui_runtime_bundle,
            hybrid,
        })
    }

    pub fn materialize_tauri_app_bundle(
        &self,
        source: &str,
        bundle: &TauriAppBundle,
        config: &TauriAppMaterializationConfig,
    ) -> Result<TauriAppMaterializedPaths, KainError> {
        let project_dir = &config.project_dir;
        let src_tauri_dir = project_dir.join("src-tauri");
        let src_tauri_src_dir = src_tauri_dir.join("src");
        let frontend_dir = project_dir.join(TAURI_FRONTEND_DIR_NAME);
        let config_dir = project_dir.join(TAURI_CONFIG_DIR_NAME);
        let state_dir = project_dir.join(TAURI_STATE_DIR_NAME);
        let artifact_root = if config.artifact_output_dir.is_absolute() {
            config.artifact_output_dir.clone()
        } else {
            project_dir.join(&config.artifact_output_dir)
        };
        let cargo_target_dir = config
            .cargo_target_dir
            .clone()
            .unwrap_or_else(|| src_tauri_dir.join(".kain").join("cargo-target"));

        fs::create_dir_all(&src_tauri_src_dir)
            .map_err(io_error("create Tauri source directory"))?;
        fs::create_dir_all(src_tauri_dir.join("permissions"))
            .map_err(io_error("create Tauri permissions directory"))?;
        fs::create_dir_all(src_tauri_dir.join("capabilities"))
            .map_err(io_error("create Tauri capabilities directory"))?;
        fs::create_dir_all(&frontend_dir).map_err(io_error("create Tauri frontend directory"))?;
        fs::create_dir_all(&config_dir).map_err(io_error("create Tauri config directory"))?;
        fs::create_dir_all(&state_dir).map_err(io_error("create Tauri state directory"))?;
        fs::create_dir_all(&artifact_root).map_err(io_error("create Tauri artifact directory"))?;

        let source_copy_path = project_dir.join(&bundle.metadata.source_file_name);
        fs::write(&source_copy_path, source.as_bytes())
            .map_err(io_error("write embedded Tauri Kain source"))?;

        let mut artifact_paths = Vec::new();
        let (materialized_realtime_bundle, packaged_realtime_asset_paths) =
            materialize_realtime_assets(
                &bundle.native_app.realtime,
                &artifact_root,
                bundle.metadata.source_root.as_deref(),
            )?;
        artifact_paths.extend(packaged_realtime_asset_paths.iter().cloned());

        let runtime_bundle_path = artifact_root.join(TAURI_RUNTIME_BUNDLE_FILE_NAME);
        let runtime_bundle_json =
            ui_runtime_bundle_to_json(&bundle.ui_runtime_bundle).map_err(|err| {
                KainError::runtime(format!(
                    "Failed to serialize Tauri runtime bundle for {}: {err}",
                    bundle.metadata.app_name
                ))
            })?;
        fs::write(&runtime_bundle_path, runtime_bundle_json.as_bytes())
            .map_err(io_error("write Tauri runtime bundle"))?;
        artifact_paths.push(runtime_bundle_path.clone());

        let runtime_contract_path = artifact_root.join(TAURI_RUNTIME_CONTRACT_FILE_NAME);
        let runtime_contract_json = runtime_contract_bundle_to_json(
            &bundle.native_app.runtime_contract,
        )
        .map_err(|err| {
            KainError::runtime(format!(
                "Failed to serialize Tauri runtime contract for {}: {err}",
                bundle.metadata.app_name
            ))
        })?;
        fs::write(&runtime_contract_path, runtime_contract_json.as_bytes())
            .map_err(io_error("write Tauri runtime contract"))?;
        artifact_paths.push(runtime_contract_path.clone());

        let runtime_compatibility_path = artifact_root.join(TAURI_RUNTIME_COMPATIBILITY_FILE_NAME);
        let runtime_compatibility_json =
            serde_json::to_string_pretty(&bundle.native_app.runtime_contract.compatibility)
                .map_err(|err| {
                    KainError::runtime(format!(
                        "Failed to serialize Tauri runtime compatibility for {}: {err}",
                        bundle.metadata.app_name
                    ))
                })?;
        fs::write(
            &runtime_compatibility_path,
            runtime_compatibility_json.as_bytes(),
        )
        .map_err(io_error("write Tauri runtime compatibility"))?;
        artifact_paths.push(runtime_compatibility_path.clone());

        let realtime_bundle_path = artifact_root.join(TAURI_REALTIME_BUNDLE_FILE_NAME);
        let realtime_bundle_json = realtime_app_bundle_to_json(&materialized_realtime_bundle)
            .map_err(|err| {
                KainError::runtime(format!("Failed to serialize Tauri realtime bundle: {err}"))
            })?;
        fs::write(&realtime_bundle_path, realtime_bundle_json.as_bytes())
            .map_err(io_error("write Tauri realtime bundle"))?;
        artifact_paths.push(realtime_bundle_path.clone());

        if let Some(runtime_version) = &bundle.native_app.runtime_version {
            let version_metadata_path =
                artifact_root.join(TAURI_RUNTIME_VERSION_METADATA_FILE_NAME);
            let version_metadata_json =
                serde_json::to_string_pretty(runtime_version).map_err(|err| {
                    KainError::runtime(format!(
                        "Failed to serialize Tauri runtime version metadata: {err}"
                    ))
                })?;
            fs::write(&version_metadata_path, version_metadata_json.as_bytes())
                .map_err(io_error("write Tauri runtime version metadata"))?;
            artifact_paths.push(version_metadata_path);
        }

        let (reflection_payload_path, reflection_payload_json) = if let Some(reflection_payload) =
            &bundle.native_app.runtime_contract.reflection_payload
        {
            let reflection_payload_path =
                artifact_root.join(TAURI_RUNTIME_REFLECTION_PAYLOAD_FILE_NAME);
            let reflection_payload_json = serde_json::to_string_pretty(reflection_payload)
                .map_err(|err| {
                    KainError::runtime(format!(
                        "Failed to serialize Tauri reflection payload: {err}"
                    ))
                })?;
            fs::write(&reflection_payload_path, reflection_payload_json.as_bytes())
                .map_err(io_error("write Tauri reflection payload"))?;
            artifact_paths.push(reflection_payload_path.clone());
            (Some(reflection_payload_path), Some(reflection_payload_json))
        } else {
            (None, None)
        };

        let (shader_bundle_path, shader_bundle_json) =
            if let Some(shader_bundle) = &bundle.native_app.shader_bundle {
                let path = artifact_root.join(TAURI_SHADER_BUNDLE_FILE_NAME);
                fs::write(&path, shader_bundle.bundle_json.as_bytes())
                    .map_err(io_error("write Tauri shader bundle"))?;
                artifact_paths.push(path.clone());
                (Some(path), Some(shader_bundle.bundle_json.clone()))
            } else {
                (None, None)
            };

        let primary_rust_path =
            artifact_root.join(&bundle.native_app.rust.bundle.primary.suggested_file_name);
        fs::write(
            &primary_rust_path,
            bundle.native_app.rust.bundle.primary.contents.as_bytes(),
        )
        .map_err(io_error("write Tauri primary Rust artifact"))?;
        artifact_paths.push(primary_rust_path);
        for artifact in &bundle.native_app.rust.bundle.supplemental {
            let path = artifact_root.join(&artifact.suggested_file_name);
            fs::write(&path, artifact.contents.as_bytes())
                .map_err(io_error("write Tauri supplemental Rust artifact"))?;
            artifact_paths.push(path);
        }
        if let Some(spirv) = &bundle.native_app.rust.spirv {
            let spirv_path = artifact_root.join("kain_gpu.spv");
            fs::write(&spirv_path, spirv).map_err(io_error("write Tauri SPIR-V artifact"))?;
            artifact_paths.push(spirv_path);
        }

        let window_label = config
            .window_label
            .clone()
            .filter(|value| !value.trim().is_empty())
            .unwrap_or_else(|| "main".to_string());
        let bundle_identifier = config
            .bundle_identifier
            .clone()
            .filter(|value| !value.trim().is_empty())
            .unwrap_or_else(|| default_bundle_identifier(&bundle.metadata.app_name));
        let cargo_package_name = config
            .cargo_package_name
            .clone()
            .filter(|value| !value.trim().is_empty())
            .unwrap_or_else(|| bundle.metadata.app_name.clone());
        let cargo_package_version = bundle
            .native_app
            .runtime_version
            .as_ref()
            .map(|version| version.runtime_version_string.clone())
            .unwrap_or_else(|| "0.1.0".to_string());

        let frontend_manifest = TauriFrontendAssetManifest {
            root_dir: TAURI_FRONTEND_DIR_NAME.to_string(),
            entry_html: format!("{TAURI_FRONTEND_DIR_NAME}/{KAIN_TAURI_FRONTEND_ENTRY_FILE_NAME}"),
            descriptor_file: format!(
                "{TAURI_FRONTEND_DIR_NAME}/{KAIN_TAURI_FRONTEND_DESCRIPTOR_FILE_NAME}"
            ),
            js_bundle: format!("{TAURI_FRONTEND_DIR_NAME}/{TAURI_JS_FILE_NAME}"),
            ts_bundle: format!("{TAURI_FRONTEND_DIR_NAME}/{TAURI_TS_FILE_NAME}"),
            wasm_bundle: format!("{TAURI_FRONTEND_DIR_NAME}/{TAURI_WASM_FILE_NAME}"),
            bridge_js: format!("{TAURI_FRONTEND_DIR_NAME}/{KAIN_TAURI_FRONTEND_BRIDGE_FILE_NAME}"),
        };
        let runtime_sidecars = TauriRuntimeSidecarManifestJson {
            runtime_bundle: sidecar_file_name(&runtime_bundle_path),
            runtime_contract: sidecar_file_name(&runtime_contract_path),
            runtime_compatibility: sidecar_file_name(&runtime_compatibility_path),
            realtime_bundle: sidecar_file_name(&realtime_bundle_path),
            shader_bundle: shader_bundle_path
                .as_ref()
                .map(|path| sidecar_file_name(path)),
            reflection_payload: bundle
                .native_app
                .runtime_contract
                .reflection_payload
                .as_ref()
                .map(|_| TAURI_RUNTIME_REFLECTION_PAYLOAD_FILE_NAME.to_string()),
            runtime_snapshot: TAURI_RUNTIME_SNAPSHOT_FILE_NAME.to_string(),
            app_manifest: TAURI_APP_MANIFEST_FILE_NAME.to_string(),
        };
        let bridge_manifest = build_tauri_bridge_manifest(&TauriBridgeManifestConfig {
            app_id: bundle_identifier.clone(),
            app_name: bundle.metadata.window_title.clone(),
            window_label: window_label.clone(),
            window_title: bundle.metadata.window_title.clone(),
            frontend: frontend_manifest,
            runtime_sidecars: kain_ui_tauri::TauriRuntimeSidecarManifest {
                runtime_bundle: runtime_sidecars.runtime_bundle.clone(),
                runtime_contract: runtime_sidecars.runtime_contract.clone(),
                runtime_compatibility: runtime_sidecars.runtime_compatibility.clone(),
                realtime_bundle: runtime_sidecars.realtime_bundle.clone(),
                shader_bundle: runtime_sidecars.shader_bundle.clone(),
                reflection_payload: runtime_sidecars.reflection_payload.clone(),
                runtime_snapshot: format!(
                    "{TAURI_STATE_DIR_NAME}/{TAURI_RUNTIME_SNAPSHOT_FILE_NAME}"
                ),
                app_manifest: format!("{TAURI_CONFIG_DIR_NAME}/{TAURI_APP_MANIFEST_FILE_NAME}"),
            },
            plugin_presets: if config.plugin_presets.is_empty() {
                default_tauri_plugin_presets()
            } else {
                config.plugin_presets.clone()
            },
            capability_presets: if config.capability_presets.is_empty() {
                default_tauri_capability_presets()
            } else {
                config.capability_presets.clone()
            },
            permission_presets: config.permission_presets.clone(),
            commands: bundle
                .native_app
                .ui
                .systems
                .command_registry
                .snapshot
                .clone(),
            compiler_reflection_payload: bundle
                .native_app
                .runtime_contract
                .reflection_payload
                .clone(),
            host_type_registry: config.host_type_registry.clone(),
        })
        .map_err(KainError::runtime)?;

        let bridge_manifest_path = artifact_root.join(KAIN_TAURI_BRIDGE_MANIFEST_FILE_NAME);
        let bridge_manifest_json =
            serde_json::to_string_pretty(&bridge_manifest).map_err(|err| {
                KainError::runtime(format!("Failed to serialize Tauri bridge manifest: {err}"))
            })?;
        fs::write(&bridge_manifest_path, bridge_manifest_json.as_bytes())
            .map_err(io_error("write Tauri bridge manifest"))?;
        artifact_paths.push(bridge_manifest_path.clone());

        let rendered_project_files = render_tauri_project_files(
            &bridge_manifest,
            &TauriProjectRenderConfig {
                cargo_package_name: cargo_package_name.clone(),
                cargo_package_version: cargo_package_version.clone(),
                bundle_identifier: bundle_identifier.clone(),
                window_label: window_label.clone(),
                window_title: bundle.metadata.window_title.clone(),
                initial_window_size: bundle.metadata.initial_window_size,
                frontend_dist_relative_path: format!("../{TAURI_FRONTEND_DIR_NAME}"),
                bridge_manifest_relative_path: format!(
                    "../{TAURI_ARTIFACT_OUTPUT_DIR_NAME}/{KAIN_TAURI_BRIDGE_MANIFEST_FILE_NAME}"
                ),
            },
        )
        .map_err(KainError::runtime)?;
        for (relative_path, contents) in &rendered_project_files.files {
            let path = project_dir.join(relative_path);
            if let Some(parent) = path.parent() {
                fs::create_dir_all(parent)
                    .map_err(io_error("create generated Tauri project parent directory"))?;
            }
            fs::write(&path, contents.as_bytes())
                .map_err(io_error("write generated Tauri project file"))?;
            artifact_paths.push(path);
        }

        let frontend_descriptor_path = frontend_dir.join(KAIN_TAURI_FRONTEND_DESCRIPTOR_FILE_NAME);
        let frontend_descriptor_json = serde_json::to_string_pretty(&json!({
            "schema_version": 1,
            "entry_html": KAIN_TAURI_FRONTEND_ENTRY_FILE_NAME,
            "js_bundle": TAURI_JS_FILE_NAME,
            "ts_bundle": TAURI_TS_FILE_NAME,
            "wasm_bundle": TAURI_WASM_FILE_NAME,
            "bridge_js": KAIN_TAURI_FRONTEND_BRIDGE_FILE_NAME,
            "wasm_export_names": bundle.hybrid.wasm_export_names,
        }))
        .map_err(|err| {
            KainError::runtime(format!(
                "Failed to serialize Tauri frontend descriptor: {err}"
            ))
        })?;
        fs::write(
            &frontend_descriptor_path,
            frontend_descriptor_json.as_bytes(),
        )
        .map_err(io_error("write Tauri frontend descriptor"))?;
        artifact_paths.push(frontend_descriptor_path.clone());

        let frontend_html_path = frontend_dir.join(KAIN_TAURI_FRONTEND_ENTRY_FILE_NAME);
        fs::write(
            &frontend_html_path,
            render_frontend_index_html(
                &bundle.metadata.window_title,
                TAURI_JS_FILE_NAME,
                KAIN_TAURI_FRONTEND_BRIDGE_FILE_NAME,
            )
            .as_bytes(),
        )
        .map_err(io_error("write Tauri frontend index"))?;
        artifact_paths.push(frontend_html_path);

        let frontend_js_path = frontend_dir.join(TAURI_JS_FILE_NAME);
        fs::write(
            &frontend_js_path,
            patch_hybrid_wasm_reference(bundle.hybrid.js.clone(), TAURI_WASM_FILE_NAME).as_bytes(),
        )
        .map_err(io_error("write Tauri frontend JS bundle"))?;
        artifact_paths.push(frontend_js_path);

        let frontend_ts_path = frontend_dir.join(TAURI_TS_FILE_NAME);
        fs::write(&frontend_ts_path, bundle.hybrid.ts.as_bytes())
            .map_err(io_error("write Tauri frontend TS bundle"))?;
        artifact_paths.push(frontend_ts_path);

        let frontend_wasm_path = frontend_dir.join(TAURI_WASM_FILE_NAME);
        fs::write(&frontend_wasm_path, &bundle.hybrid.wasm)
            .map_err(io_error("write Tauri frontend WASM bundle"))?;
        artifact_paths.push(frontend_wasm_path);

        let frontend_bridge_path = frontend_dir.join(KAIN_TAURI_FRONTEND_BRIDGE_FILE_NAME);
        fs::write(
            &frontend_bridge_path,
            render_frontend_bridge_js(&bridge_manifest)
                .map_err(KainError::runtime)?
                .as_bytes(),
        )
        .map_err(io_error("write Tauri frontend bridge JS"))?;
        artifact_paths.push(frontend_bridge_path.clone());

        let launcher = TauriAppLauncherMetadata {
            kind: "tauri-cargo-run".to_string(),
            function_name: "cargo_run_src_tauri".to_string(),
        };
        let app_manifest_path = config_dir.join(TAURI_APP_MANIFEST_FILE_NAME);
        let previous_manifest_metadata = read_previous_manifest_metadata(&app_manifest_path);
        let hot_reload = build_tauri_hot_reload_metadata(
            project_dir,
            &bundle_identifier,
            &source_copy_path,
            source,
            bundle,
            &launcher,
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
            &bridge_manifest_path,
            &bridge_manifest_json,
            &frontend_descriptor_path,
            &frontend_descriptor_json,
            &frontend_bridge_path,
            &render_frontend_bridge_js(&bridge_manifest).map_err(KainError::runtime)?,
            frontend_dir.join(TAURI_JS_FILE_NAME).as_path(),
            &patch_hybrid_wasm_reference(bundle.hybrid.js.clone(), TAURI_WASM_FILE_NAME),
        );

        let app_manifest = TauriAppManifest {
            app_id: bundle_identifier.clone(),
            name: bundle.metadata.window_title.clone(),
            version: cargo_package_version.clone(),
            window_title: bundle.metadata.window_title.clone(),
            root_component: bundle.metadata.root_component.clone(),
            active_world: bundle
                .native_app
                .realtime
                .active_world
                .as_ref()
                .map(|world| world.name.clone()),
            layout_id: format!("{}_shell", bundle.metadata.app_name.replace('-', "_")),
            required_runtime_capabilities: bundle
                .native_app
                .runtime_contract
                .required_capabilities
                .iter()
                .map(|capability| capability.key.clone())
                .collect(),
            target_outputs: vec!["tauri-ui-bundle".to_string(), "tauri-desktop".to_string()],
            runtime_sidecars: runtime_sidecars.clone(),
            launcher: launcher.clone(),
            hot_reload: hot_reload.clone(),
            bridge_manifest: sidecar_file_name(&bridge_manifest_path),
        };
        fs::write(
            &app_manifest_path,
            serde_json::to_string_pretty(&app_manifest).map_err(|err| {
                KainError::runtime(format!("Failed to serialize Tauri app manifest: {err}"))
            })?,
        )
        .map_err(io_error("write Tauri app manifest"))?;
        artifact_paths.push(app_manifest_path.clone());

        let runtime_snapshot =
            build_tauri_runtime_snapshot(bundle, &app_manifest, &bridge_manifest);
        let runtime_snapshot_path = state_dir.join(TAURI_RUNTIME_SNAPSHOT_FILE_NAME);
        fs::write(
            &runtime_snapshot_path,
            serde_json::to_string_pretty(&runtime_snapshot).map_err(|err| {
                KainError::runtime(format!("Failed to serialize Tauri runtime snapshot: {err}"))
            })?,
        )
        .map_err(io_error("write Tauri runtime snapshot"))?;
        artifact_paths.push(runtime_snapshot_path);

        let src_tauri_manifest_path = src_tauri_dir.join("Cargo.toml");
        let src_tauri_main_rs_path = src_tauri_src_dir.join("main.rs");
        let executable_path = if config.build_executable {
            Some(build_tauri_app_executable(
                &src_tauri_manifest_path,
                &cargo_package_name,
                config.release,
                Some(&cargo_target_dir),
            )?)
        } else {
            None
        };

        Ok(TauriAppMaterializedPaths {
            project_dir: project_dir.clone(),
            src_tauri_manifest_path,
            src_tauri_main_rs_path,
            source_copy_path,
            bridge_manifest_path,
            frontend_dir,
            artifact_paths,
            executable_path,
        })
    }
}

pub fn compile_tauri_app_bundle(
    source: &str,
    config: &TauriAppBundleConfig,
) -> Result<TauriAppBundle, KainError> {
    DriverSession::default().compile_tauri_app_bundle(source, config)
}

pub fn materialize_tauri_app_bundle(
    source: &str,
    bundle: &TauriAppBundle,
    config: &TauriAppMaterializationConfig,
) -> Result<TauriAppMaterializedPaths, KainError> {
    DriverSession::default().materialize_tauri_app_bundle(source, bundle, config)
}

#[derive(Debug, Clone, Serialize)]
struct TauriRuntimeSidecarManifestJson {
    runtime_bundle: String,
    runtime_contract: String,
    runtime_compatibility: String,
    realtime_bundle: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    shader_bundle: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    reflection_payload: Option<String>,
    runtime_snapshot: String,
    app_manifest: String,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, serde::Deserialize)]
struct TauriAppLauncherMetadata {
    kind: String,
    function_name: String,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
struct TauriAppHotReloadIdentity {
    app_id: String,
    name: String,
    window_title: String,
    root_component: String,
    active_world: Option<String>,
    layout_id: String,
}

#[derive(Debug, Clone, Serialize)]
struct TauriAppHotReloadArtifact {
    role: String,
    path: String,
    fingerprint: String,
    byte_length: usize,
}

#[derive(Debug, Clone, Serialize)]
struct TauriAppHotReloadPolicy {
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

#[derive(Debug, Clone, Serialize)]
struct TauriAppHotReloadMetadata {
    summary: String,
    launcher: TauriAppLauncherMetadata,
    policy: TauriAppHotReloadPolicy,
    identity: TauriAppHotReloadIdentity,
    #[serde(default)]
    participants: NativeAppHotReloadParticipants,
    artifact_fingerprints: Vec<TauriAppHotReloadArtifact>,
    materialization_fingerprint: String,
    previous_materialization_fingerprint: Option<String>,
    changed_artifact_roles: Vec<String>,
    reload_compatible_with_previous: bool,
}

#[derive(Debug, Clone, serde::Deserialize)]
struct PreviousTauriAppManifestMetadata {
    launcher: TauriAppLauncherMetadata,
    hot_reload: PreviousTauriAppHotReloadMetadata,
}

#[derive(Debug, Clone, serde::Deserialize)]
struct PreviousTauriAppHotReloadMetadata {
    identity: TauriAppHotReloadIdentity,
    #[serde(default)]
    participants: NativeAppHotReloadParticipants,
    artifact_fingerprints: Vec<PreviousTauriAppHotReloadArtifact>,
    materialization_fingerprint: String,
}

#[derive(Debug, Clone, serde::Deserialize)]
struct PreviousTauriAppHotReloadArtifact {
    role: String,
    fingerprint: String,
}

#[derive(Debug, Clone, Serialize)]
struct TauriAppManifest {
    app_id: String,
    name: String,
    version: String,
    window_title: String,
    root_component: String,
    active_world: Option<String>,
    layout_id: String,
    required_runtime_capabilities: Vec<String>,
    target_outputs: Vec<String>,
    runtime_sidecars: TauriRuntimeSidecarManifestJson,
    launcher: TauriAppLauncherMetadata,
    hot_reload: TauriAppHotReloadMetadata,
    bridge_manifest: String,
}

#[derive(Debug, Clone, Serialize)]
struct TauriRuntimeSnapshot {
    app_id: String,
    name: String,
    version: String,
    window_title: String,
    root_component: String,
    active_world: Option<String>,
    layout_id: String,
    required_runtime_capabilities: Vec<String>,
    panels: Vec<TauriRuntimeSnapshotPanel>,
    commands: Vec<TauriRuntimeSnapshotCommand>,
    providers: Vec<TauriRuntimeSnapshotProvider>,
    tools: Vec<TauriRuntimeSnapshotTool>,
    launcher: TauriAppLauncherMetadata,
    hot_reload: TauriAppHotReloadMetadata,
    #[serde(default)]
    reload: NativeAppHotReloadParticipants,
    updated_at: String,
}

#[derive(Debug, Clone, Serialize)]
struct TauriRuntimeSnapshotPanel {
    id: String,
}

#[derive(Debug, Clone, Serialize)]
struct TauriRuntimeSnapshotCommand {
    id: String,
}

#[derive(Debug, Clone, Serialize)]
struct TauriRuntimeSnapshotProvider {
    id: String,
}

#[derive(Debug, Clone, Serialize)]
struct TauriRuntimeSnapshotTool {
    id: String,
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
                "Tauri asset '{}' points to missing source '{}'{}",
                asset.key,
                asset.source,
                source_root_context.as_deref().unwrap_or("")
            )));
        }

        let packaged_file_name = packaged_realtime_asset_file_name(asset);
        let destination = artifact_root.join(&packaged_file_name);
        if source_path != destination {
            fs::copy(&source_path, &destination).map_err(io_error("copy Tauri realtime asset"))?;
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

fn build_tauri_runtime_snapshot(
    bundle: &TauriAppBundle,
    app_manifest: &TauriAppManifest,
    bridge_manifest: &TauriBridgeManifest,
) -> TauriRuntimeSnapshot {
    let updated_at = current_timestamp_string();
    let mut commands = bridge_manifest
        .command_mappings
        .iter()
        .map(|mapping| TauriRuntimeSnapshotCommand {
            id: mapping.command_name.clone(),
        })
        .collect::<Vec<_>>();
    commands.push(TauriRuntimeSnapshotCommand {
        id: "runtime.reload".to_string(),
    });
    commands.sort_by(|left, right| left.id.cmp(&right.id));

    TauriRuntimeSnapshot {
        app_id: app_manifest.app_id.clone(),
        name: app_manifest.name.clone(),
        version: app_manifest.version.clone(),
        window_title: bundle.metadata.window_title.clone(),
        root_component: bundle.metadata.root_component.clone(),
        active_world: app_manifest.active_world.clone(),
        layout_id: app_manifest.layout_id.clone(),
        required_runtime_capabilities: app_manifest.required_runtime_capabilities.clone(),
        panels: vec![TauriRuntimeSnapshotPanel {
            id: "tauri_main_window".to_string(),
        }],
        commands,
        providers: vec![
            TauriRuntimeSnapshotProvider {
                id: "tauri_bridge".to_string(),
            },
            TauriRuntimeSnapshotProvider {
                id: "hybrid_runtime".to_string(),
            },
        ],
        tools: bundle
            .native_app
            .runtime_contract
            .required_capabilities
            .iter()
            .map(|capability| TauriRuntimeSnapshotTool {
                id: capability.key.replace('.', "_"),
            })
            .collect(),
        launcher: app_manifest.launcher.clone(),
        hot_reload: app_manifest.hot_reload.clone(),
        reload: app_manifest.hot_reload.participants.clone(),
        updated_at,
    }
}

#[allow(clippy::too_many_arguments)]
fn build_tauri_hot_reload_metadata(
    project_dir: &Path,
    bundle_identifier: &str,
    source_copy_path: &Path,
    source_text: &str,
    bundle: &TauriAppBundle,
    launcher: &TauriAppLauncherMetadata,
    previous_manifest_metadata: Option<&PreviousTauriAppManifestMetadata>,
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
    bridge_manifest_path: &Path,
    bridge_manifest_json: &str,
    frontend_descriptor_path: &Path,
    frontend_descriptor_json: &str,
    frontend_bridge_path: &Path,
    frontend_bridge_js: &str,
    frontend_js_path: &Path,
    frontend_js: &str,
) -> TauriAppHotReloadMetadata {
    let hot_reload_plan = &bundle.native_app.ui.systems.hot_reload;
    let policy = TauriAppHotReloadPolicy {
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
    let identity = TauriAppHotReloadIdentity {
        app_id: bundle_identifier.to_string(),
        name: bundle.metadata.window_title.clone(),
        window_title: bundle.metadata.window_title.clone(),
        root_component: bundle.metadata.root_component.clone(),
        active_world: bundle
            .native_app
            .realtime
            .active_world
            .as_ref()
            .map(|world| world.name.clone()),
        layout_id: format!("{}_shell", bundle.metadata.app_name.replace('-', "_")),
    };
    let participants =
        build_native_app_reload_participants(&bundle.native_app, shader_bundle_json.is_some());
    let mut artifact_fingerprints = vec![
        hot_reload_artifact(project_dir, "source_input", source_copy_path, source_text),
        hot_reload_artifact(
            project_dir,
            "runtime_bundle",
            runtime_bundle_path,
            runtime_bundle_json,
        ),
        hot_reload_artifact(
            project_dir,
            "runtime_contract",
            runtime_contract_path,
            runtime_contract_json,
        ),
        hot_reload_artifact(
            project_dir,
            "runtime_compatibility",
            runtime_compatibility_path,
            runtime_compatibility_json,
        ),
        hot_reload_artifact(
            project_dir,
            "realtime_bundle",
            realtime_bundle_path,
            realtime_bundle_json,
        ),
        hot_reload_artifact(
            project_dir,
            "bridge_manifest",
            bridge_manifest_path,
            bridge_manifest_json,
        ),
        hot_reload_artifact(
            project_dir,
            "frontend_descriptor",
            frontend_descriptor_path,
            frontend_descriptor_json,
        ),
        hot_reload_artifact(
            project_dir,
            "frontend_bridge_js",
            frontend_bridge_path,
            frontend_bridge_js,
        ),
        hot_reload_artifact(project_dir, "frontend_js", frontend_js_path, frontend_js),
    ];
    if let (Some(path), Some(json)) = (shader_bundle_path, shader_bundle_json) {
        artifact_fingerprints.push(hot_reload_artifact(
            project_dir,
            "shader_bundle",
            path,
            json,
        ));
    }
    if let (Some(path), Some(json)) = (reflection_payload_path, reflection_payload_json) {
        artifact_fingerprints.push(hot_reload_artifact(
            project_dir,
            "reflection_payload",
            path,
            json,
        ));
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
    let summary = if previous_manifest_metadata.is_some() {
        format!(
            "Tauri frontend and runtime sidecars changed: {}. Window reload remains {} while launcher, authored identity, and std::reload participant schemas stay stable.",
            if changed_artifact_roles.is_empty() {
                "none".to_string()
            } else {
                changed_artifact_roles.join(", ")
            },
            if reload_compatible_with_previous {
                "eligible"
            } else {
                "gated"
            }
        )
    } else {
        "This is the first materialized Tauri reload baseline for the generated app.".to_string()
    };

    TauriAppHotReloadMetadata {
        summary,
        launcher: launcher.clone(),
        policy,
        identity,
        participants,
        artifact_fingerprints,
        materialization_fingerprint,
        previous_materialization_fingerprint,
        changed_artifact_roles,
        reload_compatible_with_previous,
    }
}

fn hot_reload_artifact(
    project_dir: &Path,
    role: &str,
    path: &Path,
    contents: &str,
) -> TauriAppHotReloadArtifact {
    TauriAppHotReloadArtifact {
        role: role.to_string(),
        path: reload_path_string(project_dir, path),
        fingerprint: fingerprint_text(contents),
        byte_length: contents.len(),
    }
}

fn read_previous_manifest_metadata(
    app_manifest_path: &Path,
) -> Option<PreviousTauriAppManifestMetadata> {
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

fn default_bundle_identifier(app_name: &str) -> String {
    format!("ai.kain.{}", app_name.replace('-', "."))
}

fn build_tauri_app_executable(
    manifest_path: &Path,
    cargo_package_name: &str,
    release: bool,
    cargo_target_dir: Option<&Path>,
) -> Result<PathBuf, KainError> {
    let mut command = Command::new("cargo");
    command
        .arg("build")
        .arg("--manifest-path")
        .arg(manifest_path);
    if release {
        command.arg("--release");
    }
    if let Some(cargo_target_dir) = cargo_target_dir {
        fs::create_dir_all(cargo_target_dir)
            .map_err(io_error("create Tauri cargo target directory"))?;
        command.env("CARGO_TARGET_DIR", cargo_target_dir);
    }
    command.current_dir(
        manifest_path
            .parent()
            .and_then(Path::parent)
            .unwrap_or_else(|| Path::new(".")),
    );

    let output = command.output().map_err(|err| {
        KainError::runtime(format!(
            "Failed to invoke cargo to build Tauri app at {}: {}",
            manifest_path.display(),
            err
        ))
    })?;
    if !output.status.success() {
        return Err(KainError::runtime(format!(
            "Tauri app cargo build failed for {}:\n{}\n{}",
            manifest_path.display(),
            String::from_utf8_lossy(&output.stdout),
            String::from_utf8_lossy(&output.stderr)
        )));
    }

    let target_root = cargo_target_dir
        .map(Path::to_path_buf)
        .unwrap_or_else(|| {
            manifest_path
                .parent()
                .unwrap_or_else(|| Path::new("."))
                .join("target")
        })
        .join(if release { "release" } else { "debug" });
    let executable_path = target_root.join(binary_file_name(cargo_package_name));
    if !executable_path.exists() {
        return Err(KainError::runtime(format!(
            "Cargo reported success but no Tauri executable was found at {}",
            executable_path.display()
        )));
    }
    Ok(executable_path)
}

fn sidecar_file_name(path: &Path) -> String {
    path.file_name()
        .and_then(OsStr::to_str)
        .unwrap_or_default()
        .to_string()
}

fn binary_file_name(app_name: &str) -> String {
    if cfg!(windows) {
        format!("{app_name}.exe")
    } else {
        app_name.to_string()
    }
}

fn io_error(action: &'static str) -> impl Fn(std::io::Error) -> KainError {
    move |err| KainError::runtime(format!("Failed to {action}: {err}"))
}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::TempDir;

    fn sample_source() -> &'static str {
        r#"
component App():
    render <panel title="Studio" />
"#
    }

    #[test]
    fn tauri_bundle_materialization_writes_bridge_and_frontend_assets() {
        let temp = TempDir::new().expect("temp dir");
        let config = TauriAppBundleConfig::default();
        let bundle = compile_tauri_app_bundle(sample_source(), &config).expect("compile bundle");
        let result = materialize_tauri_app_bundle(
            sample_source(),
            &bundle,
            &TauriAppMaterializationConfig {
                project_dir: temp.path().join("tauri-app"),
                build_executable: false,
                bundle_identifier: Some("ai.kain.test".to_string()),
                ..Default::default()
            },
        )
        .expect("materialize bundle");

        assert!(result.project_dir.exists());
        assert!(result.src_tauri_manifest_path.exists());
        assert!(result.src_tauri_main_rs_path.exists());
        assert!(result.bridge_manifest_path.exists());
        assert!(result.frontend_dir.join("main.js").exists());
        assert!(result
            .artifact_paths
            .iter()
            .any(|path| path.ends_with(KAIN_TAURI_BRIDGE_MANIFEST_FILE_NAME)));

        let app_manifest = fs::read_to_string(
            result
                .project_dir
                .join(TAURI_CONFIG_DIR_NAME)
                .join(TAURI_APP_MANIFEST_FILE_NAME),
        )
        .expect("app manifest should exist");
        assert!(app_manifest.contains("ai.kain.test"));
        assert!(app_manifest.contains("\"std::reload\""));

        let runtime_snapshot = fs::read_to_string(
            result
                .project_dir
                .join(TAURI_STATE_DIR_NAME)
                .join(TAURI_RUNTIME_SNAPSHOT_FILE_NAME),
        )
        .expect("runtime snapshot should exist");
        assert!(runtime_snapshot.contains("ai.kain.test"));
        assert!(runtime_snapshot.contains("\"reload\""));
    }
}
