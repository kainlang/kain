use crate::model::{
    BindingBundle, BindingManifest, BindingReport, BridgeParam, BridgeType, CFunctionBinding,
    GeneratedArtifacts, ImportCOutput, ItemStatus, ResolvedCLibrary,
};
use heck::ToSnakeCase;
use kain_core::error::KainError;
use std::fs;
use std::path::{Path, PathBuf};

pub const BRIDGE_FORMAT_VERSION: &str = "c-ffi-v1";
pub const BRIDGE_SYMBOL_NAME: &[u8] = b"kain_register_bridge";
pub const BINDING_MANIFEST_SCHEMA_VERSION: &str = "kain-c-ffi-manifest-v1";

pub fn write_generated_artifacts(
    resolved: &ResolvedCLibrary,
    bundle: &BindingBundle,
    cache_dir: &Path,
    output_dir_override: Option<&Path>,
) -> Result<(GeneratedArtifacts, ImportCOutput), KainError> {
    let generated_dir = output_dir_override
        .map(Path::to_path_buf)
        .unwrap_or_else(|| cache_dir.to_path_buf());
    fs::create_dir_all(&generated_dir).map_err(KainError::Io)?;
    let bridge_dir = cache_dir.join("bridge");
    fs::create_dir_all(bridge_dir.join("src")).map_err(KainError::Io)?;

    let canonical_module_source = render_canonical_module_source(resolved, bundle);
    let prelude_source = render_prelude_source(resolved, bundle);
    let bridge_source = render_bridge_source(resolved, bundle);

    let canonical_module_path = generated_dir.join(format!("{}.kn", resolved.import_name));
    let prelude_path = generated_dir.join(format!("{}_prelude.kn", resolved.import_name));
    let report_json_path = generated_dir.join(format!("{}_report.json", resolved.import_name));
    let report_text_path = generated_dir.join(format!("{}_report.txt", resolved.import_name));
    let manifest_json_path =
        generated_dir.join(format!("{}_binding_manifest.json", resolved.import_name));
    let bridge_manifest_path = bridge_dir.join("Cargo.toml");
    let bridge_source_path = bridge_dir.join("src").join("lib.rs");
    let supported_targets = vec!["interpret".to_string(), "test".to_string()];
    let capabilities = collect_capabilities(bundle);

    let report = BindingReport {
        library_name: resolved.import_name.clone(),
        parser_backend: "kain-import.c + kain-c-ffi".to_string(),
        header_path: resolved.header_path.display().to_string(),
        shared_lib_path: resolved
            .shared_lib_path
            .as_ref()
            .map(|value| value.display().to_string()),
        cache_dir: cache_dir.display().to_string(),
        report_json_path: report_json_path.display().to_string(),
        report_text_path: report_text_path.display().to_string(),
        manifest_json_path: manifest_json_path.display().to_string(),
        supported_targets: supported_targets.clone(),
        capabilities: capabilities.clone(),
        entries: bundle.report_entries.clone(),
        source_fingerprints: bundle.source_fingerprints.clone(),
    };

    let binding_manifest = BindingManifest {
        schema_version: BINDING_MANIFEST_SCHEMA_VERSION.to_string(),
        library_name: resolved.import_name.clone(),
        parser_backend: report.parser_backend.clone(),
        supported_targets,
        capabilities,
        generated_module: canonical_module_path.display().to_string(),
        generated_prelude: prelude_path.display().to_string(),
        entries: bundle.report_entries.clone(),
    };

    let report_json = serde_json::to_string_pretty(&report).map_err(|err| {
        KainError::runtime(format!(
            "Failed to serialize C FFI binding report for '{}': {err}",
            resolved.import_name
        ))
    })?;
    let report_text = render_report_text(&report);
    let manifest_json = serde_json::to_string_pretty(&binding_manifest).map_err(|err| {
        KainError::runtime(format!(
            "Failed to serialize C FFI binding manifest for '{}': {err}",
            resolved.import_name
        ))
    })?;

    fs::write(&canonical_module_path, &canonical_module_source).map_err(KainError::Io)?;
    fs::write(&prelude_path, &prelude_source).map_err(KainError::Io)?;
    fs::write(&report_json_path, report_json).map_err(KainError::Io)?;
    fs::write(&report_text_path, &report_text).map_err(KainError::Io)?;
    fs::write(&manifest_json_path, &manifest_json).map_err(KainError::Io)?;
    fs::write(&bridge_manifest_path, render_bridge_manifest(resolved)).map_err(KainError::Io)?;
    fs::write(&bridge_source_path, &bridge_source).map_err(KainError::Io)?;

    let artifacts = GeneratedArtifacts {
        canonical_module_source: canonical_module_source.clone(),
        prelude_source: prelude_source.clone(),
        bridge_source,
        report,
        report_text,
        manifest_json,
    };

    let output = ImportCOutput {
        resolved: resolved.clone(),
        config_root: Some(resolved.manifest_root.clone()),
        c_ffi_config: Some(resolved.global_config.clone()),
        cache_dir: cache_dir.to_path_buf(),
        canonical_module_path,
        prelude_path,
        report_json_path,
        report_text_path,
        manifest_json_path,
        bridge_manifest_path,
        bridge_source_path,
        dylib_path: None,
        canonical_module_source,
        prelude_source,
        cache_hit: false,
    };

    Ok((artifacts, output))
}

fn render_canonical_module_source(resolved: &ResolvedCLibrary, bundle: &BindingBundle) -> String {
    let mut output = String::new();
    output.push_str(&format!(
        "# Generated by kain-c-ffi for library {}\n# Header: {}\n\n",
        resolved.import_name,
        resolved.header_path.display()
    ));
    output.push_str("mod c:\n");
    output.push_str(&format!("    mod {}:\n", resolved.import_name));
    for binding in &bundle.functions {
        for alias in &binding.exported_aliases {
            output.push_str(&format!("        fn {}(", alias));
            for (index, param) in binding.params.iter().enumerate() {
                if index > 0 {
                    output.push_str(", ");
                }
                output.push_str(&format!("{}: {}", param.name, param.ty.render_kain()));
            }
            output.push(')');
            if !matches!(binding.return_type, BridgeType::Unit) {
                output.push_str(&format!(" -> {}", binding.return_type.render_kain()));
            }
            output.push_str(":\n");
            output.push_str("            return ");
            output.push_str(binding.return_type.default_literal());
            output.push_str("\n\n");
        }
    }
    output
}

fn render_prelude_source(resolved: &ResolvedCLibrary, bundle: &BindingBundle) -> String {
    let mut output = String::new();
    output.push_str(&format!(
        "# Generated import shim for C library {}\n",
        resolved.import_name
    ));
    for binding in &bundle.functions {
        if let Some(prefixed) = binding.exported_aliases.get(1) {
            output.push_str(&format!(
                "use c::{}::{} as {}\n",
                resolved.import_name, prefixed, prefixed
            ));
        }
    }
    output
}

fn render_bridge_manifest(resolved: &ResolvedCLibrary) -> String {
    let repo_root = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .and_then(|value| value.parent())
        .map(Path::to_path_buf)
        .unwrap_or_else(|| resolved.manifest_root.clone());
    let crate_name = bridge_crate_name(&resolved.import_name);
    format!(
        "[package]\nname = {:?}\nversion = \"0.1.0\"\nedition = \"2021\"\n\n[lib]\ncrate-type = [\"cdylib\"]\n\n[dependencies]\nkain-core = {{ path = {:?} }}\nkain-host = {{ path = {:?}, default-features = false }}\nkain-interop = {{ path = {:?} }}\nlibloading = \"0.8\"\n\n[profile.dev]\ndebug = 0\npanic = \"unwind\"\n\n[workspace]\nresolver = \"2\"\n",
        crate_name,
        repo_root.join("crates").join("kain-core").display().to_string(),
        repo_root.join("crates").join("kain-host").display().to_string(),
        repo_root
            .join("crates")
            .join("kain-interop")
            .display()
            .to_string(),
    )
}

fn render_bridge_source(resolved: &ResolvedCLibrary, bundle: &BindingBundle) -> String {
    let shared_lib_path = resolved
        .shared_lib_path
        .as_ref()
        .map(|value| value.display().to_string())
        .unwrap_or_default();

    let mut output = String::new();
    output.push_str("use kain_core::error::KainError;\n");
    output.push_str("use kain_core::runtime::{Env, Value};\n");
    output.push_str("use kain_host::{FromKainValue, ToKainValue};\n");
    output.push_str("use libloading::{Library, Symbol};\n");
    output.push_str("use std::ffi::{c_void, CStr, CString};\n");
    output.push_str("use std::sync::{Arc, RwLock};\n\n");
    output.push_str(&format!(
        "const SHARED_LIB_PATH: &str = {:?};\n\n",
        shared_lib_path
    ));
    output.push_str(
        r#"#[derive(Clone)]
struct CAbiOpaqueHandle {
    pointee: String,
    mutable: bool,
    address: usize,
}

struct ByteBufferArg {
    bytes: Vec<u8>,
    writeback: Option<ByteBufferWriteback>,
}

enum ByteBufferWriteback {
    SharedBuffer(Value),
    SharedImage(Value),
}

impl ByteBufferArg {
    fn from_value(env: &mut Env, value: Value, mutable: bool) -> Result<Self, KainError> {
        if value.host_object_label() == Some("kain.shared.image") {
            let snapshot = env.call_named_function(
                "kain_shared_image_bytes",
                vec![value.clone()],
            )?;
            return Ok(Self {
                bytes: bytes_from_value(snapshot)?,
                writeback: if mutable {
                    Some(ByteBufferWriteback::SharedImage(value))
                } else {
                    None
                },
            });
        }
        if value.host_object_label() == Some("kain.shared.buffer") {
            let snapshot = env.call_named_function(
                "kain_shared_buffer_bytes",
                vec![value.clone()],
            )?;
            return Ok(Self {
                bytes: bytes_from_value(snapshot)?,
                writeback: if mutable {
                    Some(ByteBufferWriteback::SharedBuffer(value))
                } else {
                    None
                },
            });
        }
        Ok(Self {
            bytes: <Vec<u8> as FromKainValue>::from_kain_value(value)?,
            writeback: None,
        })
    }

    fn as_ptr(&self) -> *const u8 {
        self.bytes.as_ptr()
    }

    fn as_mut_ptr(&mut self) -> *mut u8 {
        self.bytes.as_mut_ptr()
    }

    fn commit(self, env: &mut Env) -> Result<(), KainError> {
        match self.writeback {
            Some(ByteBufferWriteback::SharedBuffer(buffer)) => {
                env.call_named_function(
                    "kain_shared_buffer_replace_bytes",
                    vec![buffer, bytes_to_value(&self.bytes)],
                )?;
                Ok(())
            }
            Some(ByteBufferWriteback::SharedImage(image)) => {
                env.call_named_function(
                    "kain_shared_image_replace_bytes",
                    vec![image, bytes_to_value(&self.bytes)],
                )?;
                Ok(())
            }
            None => Ok(()),
        }
    }
}

fn bytes_from_value(value: Value) -> Result<Vec<u8>, KainError> {
    <Vec<u8> as FromKainValue>::from_kain_value(value)
}

fn bytes_to_value(bytes: &[u8]) -> Value {
    Value::Array(Arc::new(RwLock::new(
        bytes.iter().map(|value| Value::Int(*value as i64)).collect(),
    )))
}

fn extract_c_handle(
    value: Value,
    expected_pointee: &str,
    allow_null: bool,
) -> Result<*mut c_void, KainError> {
    match value {
        Value::None if allow_null => Ok(std::ptr::null_mut()),
        other => {
            let handle = other
                .downcast_host_object::<CAbiOpaqueHandle>()
                .ok_or_else(|| KainError::runtime("expected a C ABI opaque handle".to_string()))?;
            if expected_pointee != "void" && handle.pointee != expected_pointee {
                return Err(KainError::runtime(format!(
                    "expected C ABI handle for {}, got {}",
                    expected_pointee, handle.pointee
                )));
            }
            Ok(handle.address as *mut c_void)
        }
    }
}

"#,
    );

    for binding in &bundle.functions {
        output.push_str(&render_bridge_wrapper(binding));
        output.push('\n');
    }

    output.push_str("fn register_all(env: &mut Env) {\n");
    for binding in &bundle.functions {
        let wrapper_name = wrapper_name(&binding.emitted_name);
        for alias in &binding.exported_aliases {
            output.push_str(&format!(
                "    env.register_native_fn({alias:?}, {wrapper_name});\n"
            ));
        }
    }
    output.push_str("}\n\n");
    output.push_str("#[no_mangle]\n");
    output.push_str("pub extern \"C\" fn kain_register_bridge(env: *mut Env) {\n");
    output.push_str("    let Some(env) = (unsafe { env.as_mut() }) else {\n");
    output.push_str("        return;\n");
    output.push_str("    };\n");
    output.push_str("    register_all(env);\n");
    output.push_str("}\n");
    output
}

fn render_bridge_wrapper(binding: &CFunctionBinding) -> String {
    let wrapper_name = wrapper_name(&binding.emitted_name);
    let mut output = String::new();
    let mut post_call = Vec::new();
    output.push_str(&format!(
        "fn {wrapper_name}(_env: &mut Env, args: Vec<Value>) -> Result<Value, KainError> {{\n"
    ));
    output.push_str(&format!(
        "    if args.len() != {} {{\n",
        binding.params.len()
    ));
    output.push_str(&format!(
        "        return Err(KainError::runtime(format!(\"{} expected {} argument(s), got {{}}\", args.len())));\n",
        binding.emitted_name,
        binding.params.len()
    ));
    output.push_str("    }\n");
    output.push_str(
        "    let library = unsafe { Library::new(SHARED_LIB_PATH) }\n        .map_err(|err| KainError::runtime(format!(\"Failed to load C shared library {}: {err}\", SHARED_LIB_PATH)))?;\n",
    );
    output.push_str("    let mut iter = args.into_iter();\n");
    for (index, param) in binding.params.iter().enumerate() {
        output.push_str(&render_param_conversion(index, param, &mut post_call));
    }
    let ffi_params = binding
        .params
        .iter()
        .map(|param| param.ty.render_rust_ffi())
        .collect::<Vec<_>>()
        .join(", ");
    let ffi_return = binding.return_type.render_rust_ffi();
    output.push_str(&format!(
        "    let symbol: Symbol<unsafe extern \"C\" fn({ffi_params}) -> {ffi_return}> = unsafe {{ library.get(&{:?}) }}\n        .map_err(|err| KainError::runtime(format!(\"Missing C symbol {{}} in {{}}: {{err}}\", {:?}, SHARED_LIB_PATH)))?;\n",
        format!("{}\0", binding.symbol_name).as_bytes(),
        binding.symbol_name
    ));
    let call_args = binding
        .params
        .iter()
        .map(|param| param.name.as_str())
        .collect::<Vec<_>>()
        .join(", ");
    output.push_str(&format!(
        "    let result = unsafe {{ symbol({call_args}) }};\n"
    ));
    for line in post_call {
        output.push_str(&line);
    }
    output.push_str(&render_return_conversion(&binding.return_type));
    output.push_str("}\n");
    output
}

fn render_param_conversion(
    _index: usize,
    param: &BridgeParam,
    post_call: &mut Vec<String>,
) -> String {
    match &param.ty {
        BridgeType::Unit => format!(
            "    let _{} = iter.next().expect(\"checked arg count\");\n",
            param.name
        ),
        BridgeType::Bool => format!(
            "    let {} = <bool as FromKainValue>::from_kain_value(iter.next().expect(\"checked arg count\"))?;\n",
            param.name
        ),
        BridgeType::SignedInt(name) => format!(
            "    let {} = <i64 as FromKainValue>::from_kain_value(iter.next().expect(\"checked arg count\"))? as {};\n",
            param.name, name
        ),
        BridgeType::UnsignedInt(name) => format!(
            "    let __{}_value = <i64 as FromKainValue>::from_kain_value(iter.next().expect(\"checked arg count\"))?;\n    if __{}_value < 0 {{ return Err(KainError::runtime(\"unsigned C ABI argument cannot be negative\".to_string())); }}\n    let {} = __{}_value as {};\n",
            param.name, param.name, param.name, param.name, name
        ),
        BridgeType::Float32 => format!(
            "    let {} = <f64 as FromKainValue>::from_kain_value(iter.next().expect(\"checked arg count\"))? as f32;\n",
            param.name
        ),
        BridgeType::Float64 => format!(
            "    let {} = <f64 as FromKainValue>::from_kain_value(iter.next().expect(\"checked arg count\"))?;\n",
            param.name
        ),
        BridgeType::CString => format!(
            "    let __{}_owned = <String as FromKainValue>::from_kain_value(iter.next().expect(\"checked arg count\"))?;\n    let __{}_cstring = CString::new(__{}_owned).map_err(|_| KainError::runtime(\"C string argument contained interior NUL\".to_string()))?;\n    let {} = __{}_cstring.as_ptr();\n",
            param.name, param.name, param.name, param.name, param.name
        ),
        BridgeType::ByteBuffer { mutable, .. } => {
            if *mutable {
                post_call.push(format!("    __{}_buffer.commit(_env)?;\n", param.name));
                format!(
                    "    let mut __{}_buffer = ByteBufferArg::from_value(_env, iter.next().expect(\"checked arg count\"), true)?;\n    let {} = __{}_buffer.as_mut_ptr();\n",
                    param.name, param.name, param.name
                )
            } else {
                format!(
                    "    let __{}_buffer = ByteBufferArg::from_value(_env, iter.next().expect(\"checked arg count\"), false)?;\n    let {} = __{}_buffer.as_ptr();\n",
                    param.name, param.name, param.name
                )
            }
        }
        BridgeType::OpaqueHandle { pointee, .. } => format!(
            "    let {} = extract_c_handle(iter.next().expect(\"checked arg count\"), {:?}, true)?;\n",
            param.name, pointee
        ),
    }
}

fn render_return_conversion(ty: &BridgeType) -> String {
    match ty {
        BridgeType::Unit => "    Ok(Value::Unit)\n".to_string(),
        BridgeType::Bool => "    Ok(ToKainValue::to_kain_value(result))\n".to_string(),
        BridgeType::SignedInt(_) => "    Ok(ToKainValue::to_kain_value(result as i64))\n".to_string(),
        BridgeType::UnsignedInt(_) => "    if (result as u128) > (i64::MAX as u128) { return Err(KainError::runtime(\"unsigned C ABI return overflowed Kain Int\".to_string())); }\n    Ok(ToKainValue::to_kain_value(result as i64))\n".to_string(),
        BridgeType::Float32 | BridgeType::Float64 => {
            "    Ok(ToKainValue::to_kain_value(result as f64))\n".to_string()
        }
        BridgeType::CString => "    if result.is_null() { return Err(KainError::runtime(\"C string return was null\".to_string())); }\n    let text = unsafe { CStr::from_ptr(result) }.to_string_lossy().into_owned();\n    Ok(ToKainValue::to_kain_value(text))\n".to_string(),
        BridgeType::ByteBuffer { .. } => "    Err(KainError::runtime(\"byte-buffer returns require explicit output metadata and are not supported yet\".to_string()))\n".to_string(),
        BridgeType::OpaqueHandle { mutable, pointee } => format!(
            "    if result.is_null() {{ return Ok(Value::None); }}\n    Ok(Value::host_object(\"kain.c.handle\", Arc::new(CAbiOpaqueHandle {{ pointee: {:?}.to_string(), mutable: {}, address: result as usize }})))\n",
            pointee,
            if *mutable { "true" } else { "false" }
        ),
    }
}

fn render_report_text(report: &BindingReport) -> String {
    let mut output = String::new();
    output.push_str(&format!(
        "library: {}\nheader: {}\nshared_lib: {}\ncache_dir: {}\n",
        report.library_name,
        report.header_path,
        report.shared_lib_path.as_deref().unwrap_or("<none>"),
        report.cache_dir
    ));
    output.push_str(&format!("parser_backend: {}\n", report.parser_backend));
    output.push_str(&format!(
        "supported_targets: {}\n",
        report.supported_targets.join(", ")
    ));
    output.push_str(&format!(
        "capabilities: {}\n\n",
        report.capabilities.join(", ")
    ));
    output.push_str("entries:\n");
    for entry in &report.entries {
        output.push_str(&format!(
            "- [{}] {:?} {}\n",
            match entry.status {
                ItemStatus::Callable => "callable",
                ItemStatus::TypeOnly => "type_only",
                ItemStatus::OpaqueHandle => "opaque_handle",
                ItemStatus::Stubbed => "stubbed",
                ItemStatus::Unsupported => "unsupported",
            },
            entry.kind,
            entry.symbol_path
        ));
        if let Some(reason) = &entry.reason {
            output.push_str(&format!("  reason: {reason}\n"));
        }
        if let Some(emitted) = &entry.emitted_symbol {
            output.push_str(&format!("  emitted: {emitted}\n"));
        }
    }
    output.push_str("\nsource_fingerprints:\n");
    for fingerprint in &report.source_fingerprints {
        output.push_str(&format!("- {} {}\n", fingerprint.sha256, fingerprint.path));
    }
    output
}

fn wrapper_name(emitted_name: &str) -> String {
    format!("__kain_c_bridge_{}", emitted_name.to_snake_case())
}

fn collect_capabilities(bundle: &BindingBundle) -> Vec<String> {
    let mut capabilities = vec!["binding-report".to_string(), "host-backed".to_string()];
    if bundle.functions.iter().any(|binding| {
        binding
            .params
            .iter()
            .any(|param| matches!(param.ty, BridgeType::OpaqueHandle { .. }))
            || matches!(binding.return_type, BridgeType::OpaqueHandle { .. })
    }) {
        capabilities.push("opaque-handle".to_string());
    }
    if bundle.functions.iter().any(|binding| {
        binding
            .params
            .iter()
            .any(|param| matches!(param.ty, BridgeType::ByteBuffer { .. }))
    }) {
        capabilities.push("shared-buffer".to_string());
        capabilities.push("shared-image".to_string());
    }
    if bundle
        .report_entries
        .iter()
        .any(|entry| matches!(entry.status, ItemStatus::Unsupported | ItemStatus::Stubbed))
    {
        capabilities.push("classification".to_string());
    }
    capabilities.sort();
    capabilities.dedup();
    capabilities
}

pub fn bridge_crate_name(import_name: &str) -> String {
    format!("kain_c_ffi_bridge_{}", import_name.to_snake_case())
}
