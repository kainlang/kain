use serde::{Deserialize, Serialize};

pub const SHADER_ARTIFACT_SCHEMA_VERSION: u32 = 1;

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct ShaderArtifactBundle {
    pub schema_version: u32,
    pub canonical_native_payload: ShaderArtifactFormat,
    pub spirv_modules: Vec<SpirvModuleArtifact>,
    pub reflection: ShaderReflectionSummary,
    pub resource_layouts: Vec<ShaderResourceLayout>,
    pub entry_points: Vec<ShaderEntryPoint>,
    pub stage_metadata: Vec<ShaderStageMetadata>,
    pub specialization_constants: Vec<ShaderSpecializationConstant>,
    pub debug: ShaderDebugBundle,
    pub derived_outputs: Vec<DerivedShaderArtifact>,
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum ShaderArtifactFormat {
    Spirv,
    Wgsl,
    Hlsl,
    Usf,
    Ptx,
}

impl ShaderArtifactFormat {
    pub fn as_str(self) -> &'static str {
        match self {
            ShaderArtifactFormat::Spirv => "spirv",
            ShaderArtifactFormat::Wgsl => "wgsl",
            ShaderArtifactFormat::Hlsl => "hlsl",
            ShaderArtifactFormat::Usf => "usf",
            ShaderArtifactFormat::Ptx => "ptx",
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct SpirvModuleArtifact {
    pub module_name: String,
    pub byte_len: usize,
    pub bytes_hex: String,
    pub entry_points: Vec<String>,
    pub stage_hints: Vec<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct ShaderReflectionSummary {
    pub emitted: bool,
    pub shaders: Vec<ShaderReflectionShader>,
    pub notes: Vec<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct ShaderReflectionShader {
    pub shader: String,
    pub stage: String,
    pub entry_point: String,
    pub inputs: Vec<ShaderIoField>,
    pub bindings: Vec<ShaderResourceLayout>,
    pub output_type: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct ShaderIoField {
    pub name: String,
    pub ty: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct ShaderResourceLayout {
    pub shader: String,
    pub name: String,
    pub binding: u32,
    pub descriptor_set: u32,
    pub ty: String,
    pub kind: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct ShaderEntryPoint {
    pub shader: String,
    pub module_name: String,
    pub entry_point: String,
    pub stage: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct ShaderStageMetadata {
    pub shader: String,
    pub stage: String,
    pub entry_point: String,
    pub input_count: usize,
    pub binding_count: usize,
    pub output_type: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct ShaderSpecializationConstant {
    pub shader: String,
    pub name: String,
    pub binding: u32,
    pub descriptor_set: u32,
    pub ty: String,
    pub source_kind: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct ShaderDebugBundle {
    pub source_map: Vec<ShaderSourceMapEntry>,
    pub notes: Vec<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct ShaderSourceMapEntry {
    pub shader: String,
    pub source_origin: String,
    pub module_name: String,
    pub entry_point: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct DerivedShaderArtifact {
    pub format: ShaderArtifactFormat,
    pub module_name: String,
    pub contents: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct ShaderArtifactRef {
    pub shader: String,
    pub module_name: String,
    pub entry_point: String,
    pub stage: String,
}

pub fn shader_artifact_bundle_to_json(
    bundle: &ShaderArtifactBundle,
) -> Result<String, serde_json::Error> {
    serde_json::to_string_pretty(bundle)
}

pub fn shader_artifact_bundle_from_json(
    json: &str,
) -> Result<ShaderArtifactBundle, serde_json::Error> {
    serde_json::from_str(json)
}

pub fn bytes_to_hex(bytes: &[u8]) -> String {
    let mut hex = String::with_capacity(bytes.len() * 2);
    for byte in bytes {
        use std::fmt::Write as _;
        let _ = write!(&mut hex, "{byte:02x}");
    }
    hex
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn serializes_shader_bundle_with_spirv_payload() {
        let bundle = ShaderArtifactBundle {
            schema_version: SHADER_ARTIFACT_SCHEMA_VERSION,
            canonical_native_payload: ShaderArtifactFormat::Spirv,
            spirv_modules: vec![SpirvModuleArtifact {
                module_name: "viewport".to_string(),
                byte_len: 4,
                bytes_hex: bytes_to_hex(&[0x03, 0x02, 0x23, 0x07]),
                entry_points: vec!["main".to_string()],
                stage_hints: vec!["vertex".to_string()],
            }],
            reflection: ShaderReflectionSummary {
                emitted: true,
                shaders: vec![ShaderReflectionShader {
                    shader: "viewport".to_string(),
                    stage: "vertex".to_string(),
                    entry_point: "main".to_string(),
                    inputs: vec![ShaderIoField {
                        name: "position".to_string(),
                        ty: "Vec3".to_string(),
                    }],
                    bindings: Vec::new(),
                    output_type: "Vec4".to_string(),
                }],
                notes: vec!["Compiler-owned shader bundle.".to_string()],
            },
            resource_layouts: Vec::new(),
            entry_points: vec![ShaderEntryPoint {
                shader: "viewport".to_string(),
                module_name: "viewport".to_string(),
                entry_point: "main".to_string(),
                stage: "vertex".to_string(),
            }],
            stage_metadata: vec![ShaderStageMetadata {
                shader: "viewport".to_string(),
                stage: "vertex".to_string(),
                entry_point: "main".to_string(),
                input_count: 1,
                binding_count: 0,
                output_type: "Vec4".to_string(),
            }],
            specialization_constants: Vec::new(),
            debug: ShaderDebugBundle {
                source_map: vec![ShaderSourceMapEntry {
                    shader: "viewport".to_string(),
                    source_origin: "<test>".to_string(),
                    module_name: "viewport".to_string(),
                    entry_point: "main".to_string(),
                }],
                notes: vec!["No source map emitted yet.".to_string()],
            },
            derived_outputs: vec![
                DerivedShaderArtifact {
                    format: ShaderArtifactFormat::Hlsl,
                    module_name: "viewport".to_string(),
                    contents: "// hlsl".to_string(),
                },
                DerivedShaderArtifact {
                    format: ShaderArtifactFormat::Ptx,
                    module_name: "viewport".to_string(),
                    contents: "// ptx".to_string(),
                },
            ],
        };

        let json = shader_artifact_bundle_to_json(&bundle).expect("bundle should serialize");
        assert!(json.contains("\"canonical_native_payload\": \"spirv\""));
        assert!(json.contains("\"module_name\": \"viewport\""));
        assert!(json.contains("\"format\": \"hlsl\""));
        assert!(json.contains("\"format\": \"ptx\""));
    }

    #[test]
    fn deserializes_shader_bundle_with_spirv_payload() {
        let json = r#"{
  "schema_version": 1,
  "canonical_native_payload": "spirv",
  "spirv_modules": [
    {
      "module_name": "viewport",
      "byte_len": 4,
      "bytes_hex": "03022307",
      "entry_points": ["main"],
      "stage_hints": ["vertex"]
    }
  ],
  "reflection": {
    "emitted": false,
    "shaders": [],
    "notes": []
  },
  "resource_layouts": [],
  "entry_points": [],
  "stage_metadata": [],
  "specialization_constants": [],
  "debug": {
    "source_map": [],
    "notes": []
  },
  "derived_outputs": []
}"#;

        let bundle =
            shader_artifact_bundle_from_json(json).expect("bundle json should deserialize");
        assert_eq!(bundle.canonical_native_payload, ShaderArtifactFormat::Spirv);
        assert_eq!(bundle.spirv_modules[0].module_name, "viewport");
    }
}
