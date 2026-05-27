use kain_core::{
    DerivedShaderArtifact, ShaderArtifactBundle, ShaderArtifactFormat, ShaderDebugBundle,
    ShaderEntryPoint, ShaderReflectionSummary, ShaderResourceLayout, ShaderSourceMapEntry,
    ShaderStageMetadata, SpirvModuleArtifact, SHADER_ARTIFACT_SCHEMA_VERSION,
};

pub const VIEWPORT_SHADER_MODULE_NAME: &str = "kain_3d.viewport_surface";
pub const VIEWPORT_SHADER_SOURCE_ORIGIN: &str = "crates/3d/src/shaders/viewport_surface.wgsl";

const VIEWPORT_SHADER_SOURCE: &str = include_str!("shaders/viewport_surface.wgsl");

pub fn default_viewport_shader_bundle() -> ShaderArtifactBundle {
    ShaderArtifactBundle {
        schema_version: SHADER_ARTIFACT_SCHEMA_VERSION,
        canonical_native_payload: ShaderArtifactFormat::Spirv,
        spirv_modules: Vec::new(),
        reflection: ShaderReflectionSummary {
            emitted: false,
            shaders: Vec::new(),
            notes: vec![
                "Transitional viewport shader bundle.".to_string(),
                "Canonical SPIR-V emission is owned by the compiler lane; kain-3D consumes derived WGSL metadata until that handoff is complete."
                    .to_string(),
            ],
        },
        resource_layouts: vec![ShaderResourceLayout {
            shader: "viewport".to_string(),
            name: "scene".to_string(),
            binding: 0,
            descriptor_set: 0,
            ty: "SceneUniforms".to_string(),
            kind: "uniform_buffer".to_string(),
        }],
        entry_points: vec![
            shader_entry("scene_vs_main", "vertex"),
            shader_entry("background_vs_main", "vertex"),
            shader_entry("pick_vs_main", "vertex"),
            shader_entry("gizmo_vs_main", "vertex"),
            shader_entry("particle_vs_main", "vertex"),
            shader_entry("scene_fs_main", "fragment"),
            shader_entry("background_fs_main", "fragment"),
            shader_entry("pick_fs_main", "fragment"),
            shader_entry("gizmo_fs_main", "fragment"),
            shader_entry("particle_fs_main", "fragment"),
        ],
        stage_metadata: vec![
            shader_stage("scene_vs_main", "vertex"),
            shader_stage("background_vs_main", "vertex"),
            shader_stage("pick_vs_main", "vertex"),
            shader_stage("gizmo_vs_main", "vertex"),
            shader_stage("particle_vs_main", "vertex"),
            shader_stage("scene_fs_main", "fragment"),
            shader_stage("background_fs_main", "fragment"),
            shader_stage("pick_fs_main", "fragment"),
            shader_stage("gizmo_fs_main", "fragment"),
            shader_stage("particle_fs_main", "fragment"),
        ],
        specialization_constants: Vec::new(),
        debug: ShaderDebugBundle {
            source_map: vec![ShaderSourceMapEntry {
                shader: "viewport".to_string(),
                source_origin: VIEWPORT_SHADER_SOURCE_ORIGIN.to_string(),
                module_name: VIEWPORT_SHADER_MODULE_NAME.to_string(),
                entry_point: "scene_vs_main".to_string(),
            }],
            notes: vec!["Bundle-backed Kain 3D viewport shader.".to_string()],
        },
        derived_outputs: vec![DerivedShaderArtifact {
            format: ShaderArtifactFormat::Wgsl,
            module_name: VIEWPORT_SHADER_MODULE_NAME.to_string(),
            contents: VIEWPORT_SHADER_SOURCE.to_string(),
            entry_points: vec![
                "scene_vs_main".to_string(),
                "background_vs_main".to_string(),
                "pick_vs_main".to_string(),
                "gizmo_vs_main".to_string(),
                "particle_vs_main".to_string(),
                "scene_fs_main".to_string(),
                "background_fs_main".to_string(),
                "pick_fs_main".to_string(),
                "gizmo_fs_main".to_string(),
                "particle_fs_main".to_string(),
            ],
            binding_slots: vec![0],
            ptx: None,
        }],
    }
}

pub fn wgsl_module_source<'a>(
    bundle: &'a ShaderArtifactBundle,
    module_name: &str,
) -> Result<std::borrow::Cow<'a, str>, String> {
    if let Some(source) = bundle
        .derived_outputs
        .iter()
        .find(|artifact| {
            artifact.format == ShaderArtifactFormat::Wgsl && artifact.module_name == module_name
        })
        .map(|artifact| artifact.contents.as_str())
    {
        return Ok(std::borrow::Cow::Borrowed(source));
    }

    let spirv = bundle
        .spirv_modules
        .iter()
        .find(|artifact| artifact.module_name == module_name)
        .ok_or_else(|| {
            format!(
                "WGSL module `{module_name}` was not present in ShaderArtifactBundle and no SPIR-V fallback was available"
            )
        })?;

    transpile_spirv_module_to_wgsl(spirv)
}

fn transpile_spirv_module_to_wgsl(
    artifact: &SpirvModuleArtifact,
) -> Result<std::borrow::Cow<'static, str>, String> {
    let bytes = hex_to_bytes(&artifact.bytes_hex)?;
    let options = naga::front::spv::Options::default();
    let module = naga::front::spv::parse_u8_slice(&bytes, &options).map_err(|err| {
        format!(
            "failed to parse SPIR-V module `{}`: {err}",
            artifact.module_name
        )
    })?;
    let info = naga::valid::Validator::new(
        naga::valid::ValidationFlags::all(),
        naga::valid::Capabilities::all(),
    )
    .validate(&module)
    .map_err(|err| {
        format!(
            "failed to validate SPIR-V module `{}`: {err}",
            artifact.module_name
        )
    })?;
    let wgsl =
        naga::back::wgsl::write_string(&module, &info, naga::back::wgsl::WriterFlags::empty())
            .map_err(|err| {
                format!(
                    "failed to transpile SPIR-V module `{}` to WGSL: {err}",
                    artifact.module_name
                )
            })?;
    Ok(std::borrow::Cow::Owned(wgsl))
}

fn hex_to_bytes(hex: &str) -> Result<Vec<u8>, String> {
    let trimmed = hex.trim();
    if trimmed.len() % 2 != 0 {
        return Err("hex payload length must be even".to_string());
    }
    let mut bytes = Vec::with_capacity(trimmed.len() / 2);
    let mut index = 0;
    while index < trimmed.len() {
        let byte = u8::from_str_radix(&trimmed[index..index + 2], 16)
            .map_err(|err| format!("invalid hex byte at offset {index}: {err}"))?;
        bytes.push(byte);
        index += 2;
    }
    Ok(bytes)
}

fn shader_entry(entry_point: &str, stage: &str) -> ShaderEntryPoint {
    ShaderEntryPoint {
        shader: "viewport".to_string(),
        module_name: VIEWPORT_SHADER_MODULE_NAME.to_string(),
        entry_point: entry_point.to_string(),
        stage: stage.to_string(),
    }
}

fn shader_stage(entry_point: &str, stage: &str) -> ShaderStageMetadata {
    ShaderStageMetadata {
        shader: "viewport".to_string(),
        stage: stage.to_string(),
        entry_point: entry_point.to_string(),
        input_count: 0,
        binding_count: 1,
        output_type: "builtin".to_string(),
    }
}

#[cfg(test)]
mod tests {
    use super::{default_viewport_shader_bundle, wgsl_module_source, VIEWPORT_SHADER_MODULE_NAME};

    #[test]
    fn viewport_bundle_exposes_wgsl_module_by_metadata() {
        let bundle = default_viewport_shader_bundle();
        let source = wgsl_module_source(&bundle, VIEWPORT_SHADER_MODULE_NAME)
            .expect("wgsl module should exist");
        assert!(source.contains("scene_vs_main"));
        assert!(source.contains("particle_fs_main"));
    }
}
