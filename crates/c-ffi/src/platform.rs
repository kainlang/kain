use crate::config::{CFfiConfig, CInteropTier, CLibraryConfig};
use crate::extract::extract_binding_bundle;
use crate::generate::write_generated_artifacts;
use crate::model::{
    BindingReportEntry, FileFingerprint, ImportCOutput, PrepareContext, ResolvedCLibrary,
};
use kain_core::error::KainError;
use kain_fs as kfs;
use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};
use std::collections::{BTreeMap, BTreeSet};
use std::fs;
use std::path::{Path, PathBuf};

pub const PLATFORM_LOCK_SCHEMA_VERSION: &str = "kain-platform-lock-v1";
const MAX_PLATFORM_SCAN_FILES: usize = 20000;

#[derive(Debug, Clone)]
pub struct ImportPlatformOptions {
    pub package_name: Option<String>,
    pub provider: String,
    pub sdk_root: Option<PathBuf>,
    pub output_dir: Option<PathBuf>,
    pub target_triple: Option<String>,
    pub dry_run: bool,
    pub report_json: Option<PathBuf>,
    pub registry_path: Option<PathBuf>,
    pub header_path: Option<PathBuf>,
}

impl Default for ImportPlatformOptions {
    fn default() -> Self {
        Self {
            package_name: None,
            provider: "system".to_string(),
            sdk_root: None,
            output_dir: None,
            target_triple: None,
            dry_run: false,
            report_json: None,
            registry_path: None,
            header_path: None,
        }
    }
}

#[derive(Debug, Clone)]
pub struct PlatformImportOutput {
    pub lock: PlatformPackageLock,
    pub lock_path: PathBuf,
    pub generated_module_path: Option<PathBuf>,
    pub binding_report_path: Option<PathBuf>,
    pub c_ffi_output: Option<ImportCOutput>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PlatformPackageLock {
    pub schema_version: String,
    pub package_name: String,
    pub provider: String,
    pub target_triple: String,
    pub dispatch_model: String,
    pub roots_searched: Vec<String>,
    pub resolved_headers: Vec<PlatformResolvedFile>,
    pub resolved_libraries: Vec<PlatformResolvedFile>,
    pub resolved_import_libraries: Vec<PlatformResolvedFile>,
    pub registry_files: Vec<PlatformResolvedFile>,
    pub hashes: Vec<FileFingerprint>,
    pub discovered_symbols: Vec<PlatformSymbol>,
    pub chosen_symbol_source: String,
    pub dependency_closure: Vec<String>,
    pub generated_modules: Vec<String>,
    pub capability_tags: Vec<String>,
    pub blocked_symbols: Vec<PlatformBlockedSymbol>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PlatformResolvedFile {
    pub kind: String,
    pub path: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PlatformSymbol {
    pub symbol_name: String,
    pub emitted_symbol: Option<String>,
    pub source: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PlatformBlockedSymbol {
    pub symbol_name: String,
    pub reason: String,
}

struct PlatformDiscovery {
    roots: Vec<PathBuf>,
    header: Option<PathBuf>,
    registry: Option<PathBuf>,
    dynamic_libraries: Vec<PathBuf>,
    import_libraries: Vec<PathBuf>,
    blocked: Vec<PlatformBlockedSymbol>,
}

pub fn import_platform_package(
    package_or_path: &str,
    options: &ImportPlatformOptions,
    prepare: &PrepareContext,
) -> Result<PlatformImportOutput, KainError> {
    let current_dir = prepare
        .current_dir
        .clone()
        .unwrap_or_else(|| std::env::current_dir().unwrap_or_else(|_| PathBuf::from(".")));
    let package_path = resolve_candidate_path(&current_dir, package_or_path);
    let package_name = options
        .package_name
        .clone()
        .or_else(|| {
            package_path
                .exists()
                .then(|| path_stem_or_name(&package_path))
                .filter(|value| !value.is_empty())
        })
        .unwrap_or_else(|| package_or_path.to_string());
    let package_name = sanitize_package_name(&package_name);
    let target_triple = options
        .target_triple
        .clone()
        .unwrap_or_else(default_target_triple);
    let output_dir = options.output_dir.clone().unwrap_or_else(|| {
        current_dir
            .join(".kain")
            .join("platform")
            .join(&package_name)
            .join(&target_triple)
    });
    let lock_path = output_dir.join(format!("{package_name}.lock"));

    let discovery = discover_platform_package(
        &package_name,
        package_or_path,
        &package_path,
        &target_triple,
        options,
        &current_dir,
    )?;
    let is_vulkan = package_name.eq_ignore_ascii_case("vulkan");
    let dispatch_model = if is_vulkan {
        "vulkan-loader-dispatch"
    } else {
        "direct-dynamic-symbols"
    };

    let mut generated_modules = Vec::new();
    let mut discovered_symbols = Vec::new();
    let mut blocked_symbols = discovery.blocked.clone();
    let mut c_ffi_output = None;
    let mut generated_module_path = None;
    let mut binding_report_path = None;

    if !options.dry_run && discovery.header.is_some() && !discovery.dynamic_libraries.is_empty() {
        kfs::create_dir_all(&output_dir).map_err(fs_to_kain_error)?;
        let header_for_bindings = if is_vulkan {
            write_vulkan_loader_subset_header(&output_dir)?
        } else {
            discovery.header.clone().expect("checked header")
        };
        let resolved = resolved_library_for_platform_package(
            &package_name,
            &current_dir,
            &header_for_bindings,
            discovery.dynamic_libraries.first().cloned(),
            &discovery.roots,
        );
        let bundle = extract_binding_bundle(&resolved)?;
        for entry in &bundle.report_entries {
            collect_symbol_lock_entry(entry, &mut discovered_symbols, &mut blocked_symbols);
        }
        let cache_dir = output_dir.join("generated");
        let (_artifacts, output) =
            write_generated_artifacts(&resolved, &bundle, &cache_dir, Some(&output_dir))?;
        generated_modules.push(format!("platform::{package_name}"));
        generated_modules.push(render_lock_path(
            &current_dir,
            &output.canonical_module_path,
        ));
        generated_module_path = Some(output.canonical_module_path.clone());
        binding_report_path = Some(output.report_json_path.clone());
        c_ffi_output = Some(output);
    } else if discovery.header.is_none() {
        blocked_symbols.push(PlatformBlockedSymbol {
            symbol_name: "*".to_string(),
            reason: "no usable package header was discovered".to_string(),
        });
    } else if discovery.dynamic_libraries.is_empty() {
        blocked_symbols.push(PlatformBlockedSymbol {
            symbol_name: "*".to_string(),
            reason: "no dynamic library was discovered for target".to_string(),
        });
    }

    let chosen_symbol_source = if is_vulkan {
        if discovery.registry.is_some() {
            "vk.xml registry metadata plus generated loader dispatch thunks".to_string()
        } else {
            "vulkan loader dispatch thunks; registry metadata not found".to_string()
        }
    } else {
        "header declarations plus generated typed thunks".to_string()
    };

    sort_symbol_locks(&mut discovered_symbols);
    sort_blocked_locks(&mut blocked_symbols);

    let lock = PlatformPackageLock {
        schema_version: PLATFORM_LOCK_SCHEMA_VERSION.to_string(),
        package_name: package_name.clone(),
        provider: options.provider.clone(),
        target_triple: target_triple.clone(),
        dispatch_model: dispatch_model.to_string(),
        roots_searched: render_paths(&current_dir, &discovery.roots),
        resolved_headers: discovery
            .header
            .iter()
            .map(|path| resolved_file("header", &current_dir, path))
            .collect(),
        resolved_libraries: discovery
            .dynamic_libraries
            .iter()
            .map(|path| resolved_file("dynamic_library", &current_dir, path))
            .collect(),
        resolved_import_libraries: discovery
            .import_libraries
            .iter()
            .map(|path| resolved_file("import_library", &current_dir, path))
            .collect(),
        registry_files: discovery
            .registry
            .iter()
            .map(|path| resolved_file("registry", &current_dir, path))
            .collect(),
        hashes: collect_fingerprints(&current_dir, &discovery),
        discovered_symbols,
        chosen_symbol_source,
        dependency_closure: render_dependency_closure(&current_dir, &discovery),
        generated_modules,
        capability_tags: capability_tags_for_package(&package_name),
        blocked_symbols,
    };

    if !options.dry_run {
        kfs::create_dir_all(&output_dir).map_err(fs_to_kain_error)?;
        let lock_json = serde_json::to_string_pretty(&lock).map_err(|err| {
            KainError::runtime(format!("Failed to serialize platform package lock: {err}"))
        })?;
        kfs::atomic_write_text(&lock_path, &lock_json).map_err(fs_to_kain_error)?;
        if let Some(report_json) = &options.report_json {
            if let Some(parent) = report_json.parent() {
                kfs::create_dir_all(parent).map_err(fs_to_kain_error)?;
            }
            kfs::atomic_write_text(report_json, &lock_json).map_err(fs_to_kain_error)?;
        }
    }

    Ok(PlatformImportOutput {
        lock,
        lock_path,
        generated_module_path,
        binding_report_path,
        c_ffi_output,
    })
}

fn discover_platform_package(
    package_name: &str,
    package_or_path: &str,
    package_path: &Path,
    target_triple: &str,
    options: &ImportPlatformOptions,
    current_dir: &Path,
) -> Result<PlatformDiscovery, KainError> {
    let mut root_set = BTreeSet::new();
    if package_path.exists() {
        root_set.insert(canonical_or_self(package_path));
    }
    if let Some(sdk_root) = &options.sdk_root {
        root_set.insert(canonical_or_self(&resolve_path(current_dir, sdk_root)));
    }
    for env_key in sdk_env_keys(package_name) {
        if let Ok(value) = std::env::var(env_key) {
            if !value.trim().is_empty() {
                root_set.insert(canonical_or_self(&resolve_path(
                    current_dir,
                    Path::new(&value),
                )));
            }
        }
    }
    if root_set.is_empty() {
        root_set.insert(current_dir.to_path_buf());
    }
    let roots: Vec<PathBuf> = root_set.into_iter().collect();

    let header = if let Some(header_path) = &options.header_path {
        Some(resolve_path(current_dir, header_path))
    } else {
        find_platform_header(package_name, &roots)
    };
    let registry = if let Some(registry_path) = &options.registry_path {
        Some(resolve_path(current_dir, registry_path))
    } else {
        find_platform_registry(package_name, &roots)
    };
    let mut dynamic_libraries = find_platform_libraries(package_name, target_triple, &roots, false);
    let import_libraries = find_platform_libraries(package_name, target_triple, &roots, true);

    if package_name.eq_ignore_ascii_case("vulkan") {
        for fallback in vulkan_system_library_fallbacks(target_triple) {
            if fallback.exists() && !dynamic_libraries.iter().any(|path| path == &fallback) {
                dynamic_libraries.push(fallback);
            }
        }
    }
    dynamic_libraries.sort();
    dynamic_libraries.dedup();

    let mut blocked = Vec::new();
    if header.is_none() {
        blocked.push(PlatformBlockedSymbol {
            symbol_name: package_or_path.to_string(),
            reason: "header discovery failed".to_string(),
        });
    }
    if package_name.eq_ignore_ascii_case("vulkan") && registry.is_none() {
        blocked.push(PlatformBlockedSymbol {
            symbol_name: "vk.xml".to_string(),
            reason:
                "Vulkan registry metadata was not found; loader-only metadata will be generated"
                    .to_string(),
        });
    }

    Ok(PlatformDiscovery {
        roots,
        header,
        registry,
        dynamic_libraries,
        import_libraries,
        blocked,
    })
}

fn resolved_library_for_platform_package(
    package_name: &str,
    manifest_root: &Path,
    header_path: &Path,
    shared_lib_path: Option<PathBuf>,
    roots: &[PathBuf],
) -> ResolvedCLibrary {
    let include_paths = roots
        .iter()
        .flat_map(|root| [root.join("include"), root.join("Include")])
        .filter(|path| path.exists())
        .collect::<Vec<_>>();
    let config = CLibraryConfig {
        name: package_name.to_string(),
        header: header_path.to_path_buf(),
        shared_lib: shared_lib_path.clone(),
        symbols: BTreeMap::new(),
        include_paths,
        defines: Vec::new(),
        link_libs: Vec::new(),
        sources: Vec::new(),
        objects: Vec::new(),
        static_libs: Vec::new(),
        bitcode: Vec::new(),
        cpp_options: Vec::new(),
        cpp_command: None,
        tier: Some(CInteropTier::Dynamic),
        runtime_owned: false,
    };
    ResolvedCLibrary {
        import_name: package_name.to_string(),
        manifest_root: manifest_root.to_path_buf(),
        header_path: header_path.to_path_buf(),
        shared_lib_path,
        source_paths: Vec::new(),
        object_paths: Vec::new(),
        static_lib_paths: Vec::new(),
        bitcode_paths: Vec::new(),
        config,
        global_config: CFfiConfig::default(),
        tier: CInteropTier::Dynamic,
        runtime_owned: false,
    }
}

fn write_vulkan_loader_subset_header(output_dir: &Path) -> Result<PathBuf, KainError> {
    let header_path = output_dir.join("vulkan_loader_subset.h");
    let source = r#"
typedef void* VkInstance;
typedef void* VkDevice;
typedef unsigned int VkResult;
void* vkGetInstanceProcAddr(VkInstance instance, const char* pName);
void* vkGetDeviceProcAddr(VkDevice device, const char* pName);
VkResult vkEnumerateInstanceVersion(unsigned int* pApiVersion);
"#;
    kfs::atomic_write_text(&header_path, source).map_err(fs_to_kain_error)?;
    Ok(header_path)
}

fn collect_symbol_lock_entry(
    entry: &BindingReportEntry,
    discovered: &mut Vec<PlatformSymbol>,
    blocked: &mut Vec<PlatformBlockedSymbol>,
) {
    if let Some(emitted) = &entry.emitted_symbol {
        discovered.push(PlatformSymbol {
            symbol_name: entry.symbol_path.clone(),
            emitted_symbol: Some(emitted.clone()),
            source: "generated_typed_thunk".to_string(),
        });
    } else if let Some(reason) = &entry.reason {
        blocked.push(PlatformBlockedSymbol {
            symbol_name: entry.symbol_path.clone(),
            reason: stable_platform_block_reason(entry, reason),
        });
    }
}

fn stable_platform_block_reason(entry: &BindingReportEntry, detail: &str) -> String {
    let code = match entry.kind {
        crate::model::ItemKind::Callback => "type_only_callback_handle",
        crate::model::ItemKind::Struct => "opaque_struct_metadata_only",
        crate::model::ItemKind::Enum => "type_only_enum_metadata",
        crate::model::ItemKind::Typedef => "type_only_typedef_metadata",
        crate::model::ItemKind::Global => "unsupported_global_variable",
        crate::model::ItemKind::Function => {
            if detail.contains("by-value C") {
                "unsupported_by_value_aggregate"
            } else if detail.contains("K&R-style") {
                "unsupported_kr_function"
            } else {
                "unsupported_function_signature"
            }
        }
    };
    format!("{code}: {detail}")
}

fn find_platform_header(package_name: &str, roots: &[PathBuf]) -> Option<PathBuf> {
    let mut exact_candidates = Vec::new();
    for root in roots {
        if package_name.eq_ignore_ascii_case("vulkan") {
            exact_candidates.push(root.join("include").join("vulkan").join("vulkan.h"));
            exact_candidates.push(root.join("Include").join("vulkan").join("vulkan.h"));
        }
        exact_candidates.push(root.join("include").join(format!("{package_name}.h")));
        exact_candidates.push(root.join("Include").join(format!("{package_name}.h")));
    }
    for candidate in exact_candidates {
        if candidate.is_file() {
            return Some(canonical_or_self(&candidate));
        }
    }

    let wanted = if package_name.eq_ignore_ascii_case("vulkan") {
        "vulkan.h".to_string()
    } else {
        format!("{package_name}.h")
    };
    find_first_file_by_name(roots, &wanted)
}

fn find_platform_registry(package_name: &str, roots: &[PathBuf]) -> Option<PathBuf> {
    if !package_name.eq_ignore_ascii_case("vulkan") {
        return None;
    }
    let mut candidates = Vec::new();
    for root in roots {
        candidates.push(
            root.join("share")
                .join("vulkan")
                .join("registry")
                .join("vk.xml"),
        );
        candidates.push(root.join("registry").join("vk.xml"));
        candidates.push(root.join("vk.xml"));
    }
    for candidate in candidates {
        if candidate.is_file() {
            return Some(canonical_or_self(&candidate));
        }
    }
    find_first_file_by_name(roots, "vk.xml")
}

fn find_platform_libraries(
    package_name: &str,
    target_triple: &str,
    roots: &[PathBuf],
    import_library: bool,
) -> Vec<PathBuf> {
    let wanted = library_file_names(package_name, target_triple, import_library);
    let mut output = Vec::new();
    for file in collect_sdk_files(roots) {
        let Some(name) = file.file_name().and_then(|value| value.to_str()) else {
            continue;
        };
        if wanted
            .iter()
            .any(|candidate| file_name_eq(name, candidate, target_triple))
        {
            output.push(canonical_or_self(&file));
        }
    }
    output.sort();
    output.dedup();
    output
}

fn library_file_names(
    package_name: &str,
    target_triple: &str,
    import_library: bool,
) -> Vec<String> {
    let mut names = Vec::new();
    let is_windows = target_triple.contains("windows");
    let is_macos = target_triple.contains("apple") || target_triple.contains("darwin");
    if package_name.eq_ignore_ascii_case("vulkan") {
        if import_library {
            names.push("vulkan-1.lib".to_string());
        } else if is_windows {
            names.push("vulkan-1.dll".to_string());
        } else if is_macos {
            names.push("libvulkan.dylib".to_string());
            names.push("libMoltenVK.dylib".to_string());
            names.push("MoltenVK".to_string());
        } else {
            names.push("libvulkan.so.1".to_string());
            names.push("libvulkan.so".to_string());
        }
        return names;
    }

    if import_library {
        if is_windows {
            names.push(format!("{package_name}.lib"));
            names.push(format!("lib{package_name}.lib"));
        }
        return names;
    }
    if is_windows {
        names.push(format!("{package_name}.dll"));
        names.push(format!("lib{package_name}.dll"));
    } else if is_macos {
        names.push(format!("lib{package_name}.dylib"));
        names.push(format!("{package_name}.dylib"));
    } else {
        names.push(format!("lib{package_name}.so"));
        names.push(format!("{package_name}.so"));
    }
    names
}

fn vulkan_system_library_fallbacks(target_triple: &str) -> Vec<PathBuf> {
    if target_triple.contains("windows") {
        vec![PathBuf::from(r"C:\Windows\System32\vulkan-1.dll")]
    } else if target_triple.contains("apple") || target_triple.contains("darwin") {
        vec![
            PathBuf::from("/usr/local/lib/libvulkan.dylib"),
            PathBuf::from("/usr/local/lib/libMoltenVK.dylib"),
        ]
    } else {
        vec![
            PathBuf::from("/usr/lib/libvulkan.so.1"),
            PathBuf::from("/usr/lib/x86_64-linux-gnu/libvulkan.so.1"),
        ]
    }
}

fn collect_sdk_files(roots: &[PathBuf]) -> Vec<PathBuf> {
    let mut files = Vec::new();
    let mut stack = roots.to_vec();
    while let Some(path) = stack.pop() {
        if files.len() >= MAX_PLATFORM_SCAN_FILES {
            break;
        }
        let Ok(read_dir) = fs::read_dir(&path) else {
            continue;
        };
        for entry in read_dir.flatten() {
            let entry_path = entry.path();
            if should_skip_scan_path(&entry_path) {
                continue;
            }
            if entry_path.is_dir() {
                stack.push(entry_path);
            } else if entry_path.is_file() {
                files.push(entry_path);
            }
        }
    }
    files.sort();
    files
}

fn should_skip_scan_path(path: &Path) -> bool {
    path.file_name()
        .and_then(|value| value.to_str())
        .map(|name| matches!(name, ".git" | ".kain" | "target" | "node_modules"))
        .unwrap_or(false)
}

fn find_first_file_by_name(roots: &[PathBuf], wanted: &str) -> Option<PathBuf> {
    collect_sdk_files(roots)
        .into_iter()
        .find(|path| {
            path.file_name()
                .and_then(|value| value.to_str())
                .map(|name| name.eq_ignore_ascii_case(wanted))
                .unwrap_or(false)
        })
        .map(|path| canonical_or_self(&path))
}

fn collect_fingerprints(current_dir: &Path, discovery: &PlatformDiscovery) -> Vec<FileFingerprint> {
    let mut paths = Vec::new();
    paths.extend(discovery.header.iter().cloned());
    paths.extend(discovery.registry.iter().cloned());
    paths.extend(discovery.dynamic_libraries.iter().cloned());
    paths.extend(discovery.import_libraries.iter().cloned());
    paths.sort();
    paths.dedup();
    paths
        .into_iter()
        .filter_map(|path| fingerprint_file(current_dir, &path))
        .collect()
}

fn fingerprint_file(current_dir: &Path, path: &Path) -> Option<FileFingerprint> {
    let bytes = fs::read(path).ok()?;
    let mut hasher = Sha256::new();
    hasher.update(&bytes);
    Some(FileFingerprint {
        path: render_lock_path(current_dir, path),
        sha256: format!("{:x}", hasher.finalize()),
    })
}

fn render_dependency_closure(current_dir: &Path, discovery: &PlatformDiscovery) -> Vec<String> {
    let mut values = Vec::new();
    for path in &discovery.dynamic_libraries {
        values.push(render_lock_path(current_dir, path));
    }
    for path in &discovery.import_libraries {
        values.push(render_lock_path(current_dir, path));
    }
    values.sort();
    values.dedup();
    values
}

fn capability_tags_for_package(package_name: &str) -> Vec<String> {
    if package_name.eq_ignore_ascii_case("vulkan") {
        vec![
            "platform.library.dynamic".to_string(),
            "graphics.vulkan".to_string(),
            "vulkan.dispatch.instance_device".to_string(),
        ]
    } else {
        vec!["platform.library.dynamic".to_string()]
    }
}

fn sdk_env_keys(package_name: &str) -> Vec<String> {
    let normalized = package_name
        .chars()
        .map(|ch| {
            if ch.is_ascii_alphanumeric() {
                ch.to_ascii_uppercase()
            } else {
                '_'
            }
        })
        .collect::<String>();
    let mut keys = vec![format!("KAIN_PLATFORM_{normalized}_SDK")];
    if package_name.eq_ignore_ascii_case("vulkan") {
        keys.push("VULKAN_SDK".to_string());
    }
    keys
}

fn resolved_file(kind: &str, current_dir: &Path, path: &Path) -> PlatformResolvedFile {
    PlatformResolvedFile {
        kind: kind.to_string(),
        path: render_lock_path(current_dir, path),
    }
}

fn render_paths(current_dir: &Path, paths: &[PathBuf]) -> Vec<String> {
    paths
        .iter()
        .map(|path| render_lock_path(current_dir, path))
        .collect()
}

fn render_lock_path(current_dir: &Path, path: &Path) -> String {
    let anchor = canonical_or_self(current_dir);
    let canonical_path = canonical_or_self(path);
    if let Ok(relative) = canonical_path.strip_prefix(&anchor) {
        if relative.as_os_str().is_empty() {
            ".".to_string()
        } else {
            slash_path(relative)
        }
    } else {
        slash_path(&canonical_path)
    }
}

fn slash_path(path: &Path) -> String {
    path.to_string_lossy().replace('\\', "/")
}

fn sort_symbol_locks(symbols: &mut Vec<PlatformSymbol>) {
    symbols.sort_by(|left, right| {
        left.symbol_name
            .cmp(&right.symbol_name)
            .then(left.emitted_symbol.cmp(&right.emitted_symbol))
            .then(left.source.cmp(&right.source))
    });
    symbols.dedup_by(|left, right| {
        left.symbol_name == right.symbol_name
            && left.emitted_symbol == right.emitted_symbol
            && left.source == right.source
    });
}

fn sort_blocked_locks(symbols: &mut Vec<PlatformBlockedSymbol>) {
    symbols.sort_by(|left, right| {
        left.symbol_name
            .cmp(&right.symbol_name)
            .then(left.reason.cmp(&right.reason))
    });
    symbols.dedup_by(|left, right| {
        left.symbol_name == right.symbol_name && left.reason == right.reason
    });
}

fn resolve_candidate_path(current_dir: &Path, value: &str) -> PathBuf {
    resolve_path(current_dir, Path::new(value))
}

fn resolve_path(current_dir: &Path, path: &Path) -> PathBuf {
    if path.is_absolute() {
        path.to_path_buf()
    } else {
        current_dir.join(path)
    }
}

fn canonical_or_self(path: &Path) -> PathBuf {
    fs::canonicalize(path).unwrap_or_else(|_| path.to_path_buf())
}

fn path_stem_or_name(path: &Path) -> String {
    path.file_stem()
        .or_else(|| path.file_name())
        .and_then(|value| value.to_str())
        .unwrap_or("platform_package")
        .to_string()
}

fn sanitize_package_name(value: &str) -> String {
    let mut out = String::new();
    for ch in value.chars() {
        if ch.is_ascii_alphanumeric() {
            out.push(ch.to_ascii_lowercase());
        } else if ch == '_' || ch == '-' || ch == '.' {
            out.push('_');
        }
    }
    if out.is_empty() {
        "platform_package".to_string()
    } else {
        out
    }
}

fn file_name_eq(left: &str, right: &str, target_triple: &str) -> bool {
    if target_triple.contains("windows") {
        left.eq_ignore_ascii_case(right)
    } else {
        left == right
    }
}

fn default_target_triple() -> String {
    match (std::env::consts::ARCH, std::env::consts::OS) {
        ("x86_64", "windows") => "x86_64-pc-windows-msvc".to_string(),
        ("aarch64", "windows") => "aarch64-pc-windows-msvc".to_string(),
        ("x86_64", "linux") => "x86_64-unknown-linux-gnu".to_string(),
        ("aarch64", "linux") => "aarch64-unknown-linux-gnu".to_string(),
        ("x86_64", "macos") => "x86_64-apple-darwin".to_string(),
        ("aarch64", "macos") => "aarch64-apple-darwin".to_string(),
        (arch, os) => format!("{arch}-unknown-{os}"),
    }
}

fn fs_to_kain_error(error: kain_fs::FsError) -> KainError {
    KainError::runtime(format!("Filesystem error: {error}"))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn write(path: &Path, content: &str) {
        if let Some(parent) = path.parent() {
            fs::create_dir_all(parent).unwrap();
        }
        fs::write(path, content).unwrap();
    }

    fn write_tiny_math_sdk(root: &Path) -> PathBuf {
        let sdk = root.join("tiny_math");
        let header = sdk.join("include").join("tiny_math.h");
        let library = sdk
            .join("bin")
            .join(library_file_names("tiny_math", &default_target_triple(), false)[0].clone());
        write(
            &header,
            r#"
typedef struct TinyPair {
    int left;
    int right;
} TinyPair;
typedef struct TinyOpaque TinyOpaque;
typedef int (*tiny_math_callback)(int value);
int tiny_add(int left, int right);
double tiny_gain(double value);
int tiny_apply_callback(int value, tiny_math_callback callback);
TinyOpaque* tiny_context(void);
TinyPair tiny_make_pair(int left, int right);
"#,
        );
        write(
            &library,
            "fake dynamic library bytes for deterministic lock tests",
        );
        sdk
    }

    fn tiny_import_options(root: &Path) -> ImportPlatformOptions {
        ImportPlatformOptions {
            package_name: Some("tiny_math".to_string()),
            output_dir: Some(root.join(".kain").join("platform").join("tiny_math")),
            ..ImportPlatformOptions::default()
        }
    }

    #[test]
    fn platform_import_locks_tiny_sdk_with_generated_typed_thunks() {
        let temp = tempfile::tempdir().unwrap();
        let sdk = write_tiny_math_sdk(temp.path());

        let output = import_platform_package(
            sdk.to_str().unwrap(),
            &tiny_import_options(temp.path()),
            &PrepareContext {
                current_dir: Some(temp.path().to_path_buf()),
                manifest_path: None,
            },
        )
        .expect("platform import");

        assert_eq!(output.lock.package_name, "tiny_math");
        assert_eq!(output.lock.dispatch_model, "direct-dynamic-symbols");
        assert!(output.lock_path.ends_with("tiny_math.lock"));
        assert!(output.lock_path.exists());
        assert!(output.generated_module_path.as_ref().unwrap().exists());
        assert!(output
            .lock
            .discovered_symbols
            .iter()
            .any(|symbol| symbol.symbol_name.contains("tiny_add")));
        assert!(output
            .lock
            .blocked_symbols
            .iter()
            .any(|symbol| symbol.symbol_name.contains("tiny_make_pair")
                && symbol.reason.starts_with("unsupported_by_value_aggregate")));
        assert!(output
            .lock
            .blocked_symbols
            .iter()
            .any(|symbol| symbol.symbol_name.contains("tiny_math_callback")
                && symbol.reason.starts_with("type_only_callback_handle")));
        assert!(output
            .lock
            .blocked_symbols
            .iter()
            .any(|symbol| symbol.symbol_name.contains("TinyPair")
                && symbol.reason.starts_with("opaque_struct_metadata_only")));
    }

    #[test]
    fn platform_import_lock_and_report_are_deterministic_and_relocatable() {
        let temp = tempfile::tempdir().unwrap();
        let sdk = write_tiny_math_sdk(temp.path());
        let output_dir = temp.path().join(".kain").join("platform").join("tiny_math");
        let report_json = temp
            .path()
            .join(".kain")
            .join("reports")
            .join("tiny_math.lock.json");
        let options = ImportPlatformOptions {
            report_json: Some(report_json.clone()),
            ..tiny_import_options(temp.path())
        };
        let prepare = PrepareContext {
            current_dir: Some(temp.path().to_path_buf()),
            manifest_path: None,
        };

        import_platform_package(sdk.to_str().unwrap(), &options, &prepare).expect("first import");
        let lock_path = output_dir.join("tiny_math.lock");
        let first_lock = fs::read(&lock_path).expect("first lock");
        let first_report = fs::read(&report_json).expect("first report");
        import_platform_package(sdk.to_str().unwrap(), &options, &prepare).expect("second import");

        assert_eq!(first_lock, fs::read(&lock_path).expect("second lock"));
        assert_eq!(first_report, fs::read(&report_json).expect("second report"));

        let lock_text = String::from_utf8(first_lock).expect("lock utf8");
        let temp_prefix = temp.path().to_string_lossy().replace('\\', "/");
        assert!(
            !lock_text.contains(&temp_prefix),
            "lock should not bake local temp prefix: {temp_prefix}"
        );
        assert!(lock_text.contains("tiny_math/include/tiny_math.h"));
        assert!(lock_text.contains(".kain/platform/tiny_math/tiny_math.kn"));
        assert!(!lock_text.contains("call_typed"));
    }

    #[test]
    fn vulkan_import_prefers_registry_and_loader_dispatch_metadata() {
        let temp = tempfile::tempdir().unwrap();
        let sdk = temp.path().join("vulkan_sdk");
        write(
            &sdk.join("include").join("vulkan").join("vulkan.h"),
            "typedef void* VkInstance;\nvoid* vkGetInstanceProcAddr(VkInstance instance, const char* pName);\n",
        );
        write(
            &sdk.join("share")
                .join("vulkan")
                .join("registry")
                .join("vk.xml"),
            "<registry><commands></commands></registry>",
        );
        let library = sdk
            .join("bin")
            .join(library_file_names("vulkan", &default_target_triple(), false)[0].clone());
        write(&library, "fake vulkan loader");

        let output = import_platform_package(
            "vulkan",
            &ImportPlatformOptions {
                sdk_root: Some(sdk),
                output_dir: Some(temp.path().join(".kain").join("platform").join("vulkan")),
                ..ImportPlatformOptions::default()
            },
            &PrepareContext {
                current_dir: Some(temp.path().to_path_buf()),
                manifest_path: None,
            },
        )
        .expect("vulkan platform import");

        assert_eq!(output.lock.dispatch_model, "vulkan-loader-dispatch");
        assert!(output.lock.chosen_symbol_source.contains("vk.xml"));
        assert!(output
            .lock
            .capability_tags
            .contains(&"graphics.vulkan".to_string()));
        assert!(output
            .lock
            .discovered_symbols
            .iter()
            .any(|symbol| symbol.symbol_name.contains("vkGetInstanceProcAddr")));
    }
}
