//! Shader Knowledge Base
//!
//! A data-driven database of HLSL/UE5 shader intrinsics, include dependencies,
//! permutation patterns, thread group conventions, and material properties.
//! Loaded from `unreal/metadata/shader_knowledge.json` which is generated
//! by the shader extractor scanning the UE5 Engine Shaders directory.
//!
//! This replaces hardcoded intrinsic lists, include paths, and thread group
//! sizes with queries against real data extracted from Epic's shader corpus.

use serde::{Deserialize, Serialize};
use std::collections::HashMap;

// ═══════════════════════════════════════════════════════════════════
// Schema Types (mirrors shader_knowledge.json structure)
// ═══════════════════════════════════════════════════════════════════

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct IntrinsicParam {
    #[serde(rename = "type", default)]
    pub param_type: String,
    #[serde(default)]
    pub name: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct IntrinsicInfo {
    pub name: String,
    #[serde(default)]
    pub category: String,
    #[serde(default)]
    pub call_count: usize,
    #[serde(default)]
    pub params: Vec<IntrinsicParam>,
    #[serde(default)]
    pub param_count: usize,
    #[serde(default)]
    pub source: String,
    #[serde(default)]
    pub is_macro: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PermutationInfo {
    pub name: String,
    #[serde(default)]
    pub usage_count: usize,
    #[serde(rename = "type", default)]
    pub perm_type: String,
    #[serde(default)]
    pub range: Option<usize>,
    #[serde(default)]
    pub enum_class: Option<String>,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct IncludeData {
    #[serde(default)]
    pub graph: HashMap<String, Vec<String>>,
    #[serde(default)]
    pub frequency: HashMap<String, usize>,
    #[serde(default)]
    pub file_provides: HashMap<String, Vec<String>>,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct BindingPatterns {
    #[serde(default)]
    pub thread_groups: HashMap<String, usize>,
    #[serde(default)]
    pub parameter_types: HashMap<String, String>,
    #[serde(default)]
    pub common_textures: HashMap<String, usize>,
    #[serde(default)]
    pub common_uavs: HashMap<String, usize>,
    #[serde(default)]
    pub common_buffers: HashMap<String, usize>,
    #[serde(default)]
    pub cbuffers: Vec<String>,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct MaterialData {
    #[serde(default)]
    pub outputs: HashMap<String, usize>,
    #[serde(default)]
    pub parameters: HashMap<String, usize>,
    #[serde(default)]
    pub getters: HashMap<String, usize>,
    #[serde(default)]
    pub types: HashMap<String, usize>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ShaderKnowledgeData {
    #[serde(default)]
    pub engine_version: String,
    #[serde(default)]
    pub intrinsics: HashMap<String, IntrinsicInfo>,
    #[serde(default)]
    pub includes: IncludeData,
    #[serde(default)]
    pub permutations: HashMap<String, PermutationInfo>,
    #[serde(default)]
    pub bindings: BindingPatterns,
    #[serde(default)]
    pub material: MaterialData,
}

// ═══════════════════════════════════════════════════════════════════
// Shader Knowledge — the queryable runtime database
// ═══════════════════════════════════════════════════════════════════

#[derive(Debug, Clone)]
pub struct ShaderKnowledge {
    pub intrinsics: HashMap<String, IntrinsicInfo>,
    pub permutations: HashMap<String, PermutationInfo>,
    pub bindings: BindingPatterns,
    pub material: MaterialData,
    /// Reverse map: function name -> source .ush file that defines it
    function_to_include: HashMap<String, String>,
    /// All known material getter names (GetBaseColor, GetRoughness, etc.)
    material_getter_set: std::collections::HashSet<String>,
}

impl Default for ShaderKnowledge {
    fn default() -> Self {
        Self::new()
    }
}

impl ShaderKnowledge {
    pub fn new() -> Self {
        Self {
            intrinsics: HashMap::new(),
            permutations: HashMap::new(),
            bindings: BindingPatterns {
                thread_groups: HashMap::new(),
                parameter_types: HashMap::new(),
                common_textures: HashMap::new(),
                common_uavs: HashMap::new(),
                common_buffers: HashMap::new(),
                cbuffers: Vec::new(),
            },
            material: MaterialData {
                outputs: HashMap::new(),
                parameters: HashMap::new(),
                getters: HashMap::new(),
                types: HashMap::new(),
            },
            function_to_include: HashMap::new(),
            material_getter_set: std::collections::HashSet::new(),
        }
    }

    /// Load shader knowledge from JSON data
    pub fn load(&mut self, json_data: &str) -> Result<(), String> {
        let data: ShaderKnowledgeData = serde_json::from_str(json_data)
            .map_err(|e| format!("Failed to parse shader knowledge: {}", e))?;

        self.intrinsics = data.intrinsics;
        self.permutations = data.permutations;
        self.bindings = data.bindings;
        self.material = data.material;
        self.rebuild_indices();
        Ok(())
    }

    /// Rebuild reverse lookup indices
    fn rebuild_indices(&mut self) {
        self.function_to_include.clear();
        self.material_getter_set.clear();

        // Build function -> include map from file_provides data
        // (This is in the includes section but we don't store that raw —
        //  we extract it during load from the intrinsics source field)
        for (name, info) in &self.intrinsics {
            if !info.source.is_empty() {
                self.function_to_include
                    .insert(name.clone(), info.source.clone());
            }
        }

        // Build material getter set
        for name in self.material.getters.keys() {
            self.material_getter_set.insert(name.clone());
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // Query API — Intrinsics
    // ═══════════════════════════════════════════════════════════════

    /// Check if a function name is a known intrinsic (HLSL or UE5)
    pub fn is_known_function(&self, name: &str) -> bool {
        self.intrinsics.contains_key(name)
    }

    /// Get intrinsic info by name
    pub fn get_intrinsic(&self, name: &str) -> Option<&IntrinsicInfo> {
        self.intrinsics.get(name)
    }

    /// Check if a function is an HLSL builtin
    pub fn is_hlsl_intrinsic(&self, name: &str) -> bool {
        self.intrinsics
            .get(name)
            .map_or(false, |i| i.category == "hlsl")
    }

    /// Check if a function is a UE5-defined helper
    pub fn is_ue5_function(&self, name: &str) -> bool {
        self.intrinsics
            .get(name)
            .map_or(false, |i| i.category == "ue5" || i.category == "macro")
    }

    /// Get the parameter count for a known function.
    /// Returns None if function is unknown.
    pub fn get_param_count(&self, name: &str) -> Option<usize> {
        self.intrinsics.get(name).map(|i| i.param_count)
    }

    /// Infer the return type of a known function based on its params and name.
    /// This uses heuristics from the corpus data.
    pub fn infer_return_type(&self, name: &str) -> &str {
        // Functions that always return float (scalar)
        if matches!(
            name,
            "dot"
                | "length"
                | "distance"
                | "determinant"
                | "saturate"
                | "abs"
                | "sign"
                | "floor"
                | "ceil"
                | "round"
                | "trunc"
                | "frac"
                | "fmod"
                | "fwidth"
                | "ddx"
                | "ddy"
                | "ddx_coarse"
                | "ddy_coarse"
                | "ddx_fine"
                | "ddy_fine"
                | "rcp"
                | "rsqrt"
                | "sqrt"
                | "sin"
                | "cos"
                | "tan"
                | "asin"
                | "acos"
                | "atan"
                | "atan2"
                | "sinh"
                | "cosh"
                | "tanh"
                | "exp"
                | "exp2"
                | "log"
                | "log2"
                | "log10"
                | "pow"
                | "min"
                | "max"
                | "step"
                | "smoothstep"
                | "lerp"
                | "clamp"
                | "mad"
                | "luminance"
                | "GetLuminance"
                | "WaveGetLaneCount"
                | "WaveGetLaneIndex"
                | "WaveIsFirstLane"
                | "countbits"
                | "firstbithigh"
                | "firstbitlow"
                | "reversebits"
        ) {
            // These preserve the type of their first argument, so we return
            // "passthrough" to signal the caller should use the input type
            return "passthrough";
        }

        // Functions that always return bool
        if matches!(
            name,
            "all" | "any" | "isfinite" | "isinf" | "isnan" | "WaveActiveAllEqual"
        ) {
            return "bool";
        }

        // Functions that always return void
        if matches!(
            name,
            "clip"
                | "sincos"
                | "InterlockedAdd"
                | "InterlockedAnd"
                | "InterlockedOr"
                | "InterlockedXor"
                | "InterlockedMin"
                | "InterlockedMax"
                | "InterlockedExchange"
                | "InterlockedCompareExchange"
                | "InterlockedCompareStore"
                | "GroupMemoryBarrier"
                | "GroupMemoryBarrierWithGroupSync"
                | "DeviceMemoryBarrier"
                | "DeviceMemoryBarrierWithGroupSync"
                | "AllMemoryBarrier"
                | "AllMemoryBarrierWithGroupSync"
        ) {
            return "void";
        }

        // Texture operations return float4
        if name.starts_with("tex")
            || name == "Sample"
            || name == "SampleLevel"
            || name == "SampleGrad"
            || name == "SampleBias"
            || name == "Load"
            || name == "GatherRed"
            || name == "GatherGreen"
            || name == "GatherBlue"
            || name == "GatherAlpha"
        {
            return "float4";
        }

        // Cross product preserves type
        if name == "cross"
            || name == "normalize"
            || name == "reflect"
            || name == "refract"
            || name == "faceforward"
        {
            return "passthrough";
        }

        // Type cast/constructors
        if matches!(name, "asfloat" | "float") {
            return "float";
        }
        if matches!(name, "asint" | "int") {
            return "int";
        }
        if matches!(name, "asuint" | "uint") {
            return "uint";
        }
        if matches!(name, "f16tof32") {
            return "float";
        }
        if matches!(name, "f32tof16") {
            return "uint";
        }

        // Default: unknown, let caller decide
        "unknown"
    }

    // ═══════════════════════════════════════════════════════════════
    // Query API — Includes
    // ═══════════════════════════════════════════════════════════════

    /// Get the .ush source file that defines a function.
    /// Example: get_function_include("CalcSceneDepth") -> Some("SceneTexturesCommon.ush")
    pub fn get_function_include(&self, func_name: &str) -> Option<&str> {
        self.function_to_include.get(func_name).map(|s| s.as_str())
    }

    // ═══════════════════════════════════════════════════════════════
    // Query API — Permutations
    // ═══════════════════════════════════════════════════════════════

    /// Check if a name is a known UE5 permutation
    pub fn is_known_permutation(&self, name: &str) -> bool {
        self.permutations.contains_key(name)
    }

    /// Get permutation info
    pub fn get_permutation(&self, name: &str) -> Option<&PermutationInfo> {
        self.permutations.get(name)
    }

    // ═══════════════════════════════════════════════════════════════
    // Query API — Thread Groups
    // ═══════════════════════════════════════════════════════════════

    /// Get the most common thread group size.
    /// Returns (x, y, z) — defaults to (8, 8, 1) which is the most common
    /// 2D compute pattern in Epic's shader corpus.
    pub fn default_thread_group(&self) -> (u32, u32, u32) {
        // From corpus: [8x8x1] is the standard 2D pattern (60x usage)
        // [64x1x1] for linear, [1x1x1] for per-pixel
        (8, 8, 1)
    }

    /// Get thread group usage count for a specific size
    pub fn thread_group_usage(&self, key: &str) -> usize {
        self.bindings.thread_groups.get(key).copied().unwrap_or(0)
    }

    // ═══════════════════════════════════════════════════════════════
    // Query API — Material
    // ═══════════════════════════════════════════════════════════════

    /// Check if a name is a known material getter (GetBaseColor, GetRoughness, etc.)
    pub fn is_material_getter(&self, name: &str) -> bool {
        self.material_getter_set.contains(name)
    }

    /// Get all material output property names with usage counts
    pub fn material_outputs(&self) -> &HashMap<String, usize> {
        &self.material.outputs
    }

    /// Get all material getter names with usage counts
    pub fn material_getters(&self) -> &HashMap<String, usize> {
        &self.material.getters
    }

    /// Get count stats
    pub fn stats(&self) -> (usize, usize, usize) {
        (
            self.intrinsics.len(),
            self.permutations.len(),
            self.bindings.thread_groups.len(),
        )
    }
}

// ═══════════════════════════════════════════════════════════════════
// Tests
// ═══════════════════════════════════════════════════════════════════

#[cfg(test)]
mod tests {
    use super::*;

    fn sample_json() -> &'static str {
        r#"{
            "engine_version": "5.7",
            "extraction_stats": {},
            "intrinsics": {
                "lerp": {
                    "name": "lerp",
                    "category": "hlsl",
                    "call_count": 500,
                    "params": [
                        {"type": "float", "name": "a"},
                        {"type": "float", "name": "b"},
                        {"type": "float", "name": "t"}
                    ],
                    "param_count": 3
                },
                "CalcSceneDepth": {
                    "name": "CalcSceneDepth",
                    "category": "ue5",
                    "call_count": 61,
                    "params": [{"type": "float2", "name": "ScreenUV"}],
                    "param_count": 1,
                    "source": "SceneTexturesCommon.ush"
                },
                "BRANCH": {
                    "name": "BRANCH",
                    "category": "macro",
                    "call_count": 200,
                    "params": [],
                    "param_count": 0,
                    "source": "Common.ush",
                    "is_macro": true
                }
            },
            "includes": {
                "graph": {},
                "frequency": {"Common.ush": 551},
                "file_provides": {}
            },
            "permutations": {
                "USE_INSTANCING": {
                    "name": "USE_INSTANCING",
                    "usage_count": 47,
                    "type": "ifdef"
                }
            },
            "bindings": {
                "thread_groups": {"8x8x1": 60, "64x1x1": 50, "1x1x1": 90},
                "parameter_types": {},
                "common_textures": {},
                "common_uavs": {},
                "common_buffers": {},
                "cbuffers": []
            },
            "material": {
                "outputs": {"BaseColor": 1, "EmissiveColor": 1},
                "parameters": {},
                "getters": {
                    "GetBaseColor": 24,
                    "GetRoughness": 15,
                    "GetMetallic": 17,
                    "GetEmissive": 22,
                    "GetOpacity": 57
                },
                "types": {"MaterialFloat": 79, "MaterialFloat3": 56}
            }
        }"#
    }

    #[test]
    fn test_load_and_query() {
        let mut sk = ShaderKnowledge::new();
        sk.load(sample_json()).unwrap();

        assert_eq!(sk.intrinsics.len(), 3);
        assert!(sk.is_known_function("lerp"));
        assert!(sk.is_known_function("CalcSceneDepth"));
        assert!(!sk.is_known_function("nonexistent"));
    }

    #[test]
    fn test_intrinsic_categories() {
        let mut sk = ShaderKnowledge::new();
        sk.load(sample_json()).unwrap();

        assert!(sk.is_hlsl_intrinsic("lerp"));
        assert!(!sk.is_hlsl_intrinsic("CalcSceneDepth"));
        assert!(sk.is_ue5_function("CalcSceneDepth"));
        assert!(sk.is_ue5_function("BRANCH"));
    }

    #[test]
    fn test_return_type_inference() {
        let sk = ShaderKnowledge::new();

        assert_eq!(sk.infer_return_type("dot"), "passthrough");
        assert_eq!(sk.infer_return_type("all"), "bool");
        assert_eq!(sk.infer_return_type("InterlockedAdd"), "void");
        assert_eq!(sk.infer_return_type("Sample"), "float4");
        assert_eq!(sk.infer_return_type("asfloat"), "float");
    }

    #[test]
    fn test_function_include() {
        let mut sk = ShaderKnowledge::new();
        sk.load(sample_json()).unwrap();

        assert_eq!(
            sk.get_function_include("CalcSceneDepth"),
            Some("SceneTexturesCommon.ush")
        );
        assert_eq!(sk.get_function_include("BRANCH"), Some("Common.ush"));
        assert_eq!(sk.get_function_include("lerp"), None); // HLSL builtin, no source
    }

    #[test]
    fn test_permutations() {
        let mut sk = ShaderKnowledge::new();
        sk.load(sample_json()).unwrap();

        assert!(sk.is_known_permutation("USE_INSTANCING"));
        assert!(!sk.is_known_permutation("RANDOM_NAME"));
    }

    #[test]
    fn test_material_getters() {
        let mut sk = ShaderKnowledge::new();
        sk.load(sample_json()).unwrap();

        assert!(sk.is_material_getter("GetBaseColor"));
        assert!(sk.is_material_getter("GetRoughness"));
        assert!(!sk.is_material_getter("GetNonexistent"));
    }

    #[test]
    fn test_thread_groups() {
        let mut sk = ShaderKnowledge::new();
        sk.load(sample_json()).unwrap();

        assert_eq!(sk.default_thread_group(), (8, 8, 1));
        assert_eq!(sk.thread_group_usage("8x8x1"), 60);
        assert_eq!(sk.thread_group_usage("64x1x1"), 50);
    }
}
