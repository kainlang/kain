#!/usr/bin/env python3
import json
from pathlib import Path

json_path = Path('unreal/metadata/shader_knowledge.json')
print(f'Loading {json_path}...')

with open(json_path, 'r', encoding='utf-8') as f:
    data = json.load(f)

print(f'Current sections: {list(data.keys())}')
print(f'Total intrinsics: {data["extraction_stats"]["total_intrinsics"]}')

# Add hlsl_types
data["hlsl_types"] = {
    "scalar_types": {
        "float": {"size_bytes": 4, "description": "32-bit floating point"},
        "int": {"size_bytes": 4, "description": "32-bit signed integer"},
        "uint": {"size_bytes": 4, "description": "32-bit unsigned integer"},
        "bool": {"size_bytes": 4, "description": "Boolean (stored as 32-bit)"},
        "half": {"size_bytes": 2, "description": "16-bit floating point"},
        "double": {"size_bytes": 8, "description": "64-bit floating point"},
        "min16float": {"size_bytes": 2, "description": "Minimum 16-bit float"},
        "min10float": {"size_bytes": 2, "description": "Minimum 10-bit float"},
        "min16int": {"size_bytes": 2, "description": "Minimum 16-bit signed int"},
        "min12int": {"size_bytes": 2, "description": "Minimum 12-bit signed int"},
        "min16uint": {"size_bytes": 2, "description": "Minimum 16-bit unsigned int"}
    },
    "vector_types": {
        "float2": {"base": "float", "components": 2, "size_bytes": 8},
        "float3": {"base": "float", "components": 3, "size_bytes": 12},
        "float4": {"base": "float", "components": 4, "size_bytes": 16},
        "int2": {"base": "int", "components": 2, "size_bytes": 8},
        "int3": {"base": "int", "components": 3, "size_bytes": 12},
        "int4": {"base": "int", "components": 4, "size_bytes": 16},
        "uint2": {"base": "uint", "components": 2, "size_bytes": 8},
        "uint3": {"base": "uint", "components": 3, "size_bytes": 12},
        "uint4": {"base": "uint", "components": 4, "size_bytes": 16},
        "bool2": {"base": "bool", "components": 2, "size_bytes": 8},
        "bool3": {"base": "bool", "components": 3, "size_bytes": 12},
        "bool4": {"base": "bool", "components": 4, "size_bytes": 16},
        "half2": {"base": "half", "components": 2, "size_bytes": 4},
        "half3": {"base": "half", "components": 3, "size_bytes": 6},
        "half4": {"base": "half", "components": 4, "size_bytes": 8}
    },
    "matrix_types": {
        "float2x2": {"base": "float", "rows": 2, "cols": 2, "size_bytes": 16},
        "float2x3": {"base": "float", "rows": 2, "cols": 3, "size_bytes": 24},
        "float2x4": {"base": "float", "rows": 2, "cols": 4, "size_bytes": 32},
        "float3x2": {"base": "float", "rows": 3, "cols": 2, "size_bytes": 24},
        "float3x3": {"base": "float", "rows": 3, "cols": 3, "size_bytes": 36},
        "float3x4": {"base": "float", "rows": 3, "cols": 4, "size_bytes": 48},
        "float4x2": {"base": "float", "rows": 4, "cols": 2, "size_bytes": 32},
        "float4x3": {"base": "float", "rows": 4, "cols": 3, "size_bytes": 48},
        "float4x4": {"base": "float", "rows": 4, "cols": 4, "size_bytes": 64},
        "int2x2": {"base": "int", "rows": 2, "cols": 2, "size_bytes": 16},
        "int3x3": {"base": "int", "rows": 3, "cols": 3, "size_bytes": 36},
        "int4x4": {"base": "int", "rows": 4, "cols": 4, "size_bytes": 64},
        "uint2x2": {"base": "uint", "rows": 2, "cols": 2, "size_bytes": 16},
        "uint3x3": {"base": "uint", "rows": 3, "cols": 3, "size_bytes": 36},
        "uint4x4": {"base": "uint", "rows": 4, "cols": 4, "size_bytes": 64}
    },
    "texture_types": {
        "Texture1D": {"dimensions": 1, "writable": False, "description": "1D texture"},
        "Texture2D": {"dimensions": 2, "writable": False, "description": "2D texture"},
        "Texture3D": {"dimensions": 3, "writable": False, "description": "3D texture"},
        "TextureCube": {"dimensions": 2, "writable": False, "description": "Cube texture"},
        "Texture1DArray": {"dimensions": 1, "writable": False, "description": "1D texture array"},
        "Texture2DArray": {"dimensions": 2, "writable": False, "description": "2D texture array"},
        "TextureCubeArray": {"dimensions": 2, "writable": False, "description": "Cube texture array"},
        "Texture2DMS": {"dimensions": 2, "writable": False, "description": "2D multisampled texture"},
        "Texture2DMSArray": {"dimensions": 2, "writable": False, "description": "2D multisampled texture array"},
        "RWTexture1D": {"dimensions": 1, "writable": True, "description": "Read-write 1D texture"},
        "RWTexture2D": {"dimensions": 2, "writable": True, "description": "Read-write 2D texture"},
        "RWTexture3D": {"dimensions": 3, "writable": True, "description": "Read-write 3D texture"},
        "RWTexture1DArray": {"dimensions": 1, "writable": True, "description": "Read-write 1D texture array"},
        "RWTexture2DArray": {"dimensions": 2, "writable": True, "description": "Read-write 2D texture array"}
    },
    "buffer_types": {
        "Buffer": {"writable": False, "structured": False, "description": "Typed buffer"},
        "RWBuffer": {"writable": True, "structured": False, "description": "Read-write typed buffer"},
        "StructuredBuffer": {"writable": False, "structured": True, "description": "Structured buffer"},
        "RWStructuredBuffer": {"writable": True, "structured": True, "description": "Read-write structured buffer"},
        "ByteAddressBuffer": {"writable": False, "structured": False, "description": "Raw byte buffer"},
        "RWByteAddressBuffer": {"writable": True, "structured": False, "description": "Read-write raw byte buffer"},
        "AppendStructuredBuffer": {"writable": True, "structured": True, "description": "Append-only structured buffer"},
        "ConsumeStructuredBuffer": {"writable": True, "structured": True, "description": "Consume-only structured buffer"}
    },
    "sampler_types": {
        "SamplerState": {"comparison": False, "description": "Standard sampler"},
        "SamplerComparisonState": {"comparison": True, "description": "Comparison sampler for shadow mapping"}
    }
}

print('Added hlsl_types section')

# Add hlsl_keywords
data["hlsl_keywords"] = {
    "control_flow": [
        "if", "else", "for", "while", "do", "switch", "case", "default",
        "break", "continue", "return", "discard"
    ],
    "type_qualifiers": [
        "const", "static", "uniform", "extern", "precise", "shared",
        "groupshared", "volatile", "row_major", "column_major"
    ],
    "parameter_qualifiers": [
        "in", "out", "inout", "nointerpolation", "linear", "centroid",
        "noperspective", "sample", "point", "line", "triangle",
        "lineadj", "triangleadj"
    ],
    "function_qualifiers": ["inline"],
    "shader_stages": ["vertex", "pixel", "geometry", "hull", "domain", "compute"],
    "semantics": {
        "vertex_input": [
            "POSITION", "NORMAL", "TANGENT", "BINORMAL", "TEXCOORD",
            "COLOR", "BLENDINDICES", "BLENDWEIGHT"
        ],
        "vertex_output": ["SV_Position", "SV_ClipDistance", "SV_CullDistance"],
        "pixel_input": [
            "SV_Position", "SV_IsFrontFace", "SV_SampleIndex",
            "SV_Coverage", "SV_InnerCoverage", "SV_PrimitiveID"
        ],
        "pixel_output": [
            "SV_Target", "SV_Target0", "SV_Target1", "SV_Target2",
            "SV_Target3", "SV_Target4", "SV_Target5", "SV_Target6",
            "SV_Target7", "SV_Depth", "SV_DepthGreaterEqual",
            "SV_DepthLessEqual", "SV_Coverage", "SV_StencilRef"
        ],
        "compute": [
            "SV_DispatchThreadID", "SV_GroupID", "SV_GroupIndex",
            "SV_GroupThreadID"
        ],
        "geometry": [
            "SV_GSInstanceID", "SV_OutputControlPointID",
            "SV_PrimitiveID", "SV_RenderTargetArrayIndex",
            "SV_ViewportArrayIndex"
        ],
        "tessellation": [
            "SV_DomainLocation", "SV_InsideTessFactor",
            "SV_OutputControlPointID", "SV_TessFactor"
        ]
    }
}

print('Added hlsl_keywords section')

# Add binding_rules
data["binding_rules"] = {
    "texture_slots": {
        "range": "t0-t127",
        "description": "Shader Resource Views (SRV) for textures and buffers",
        "ue5_convention": "Textures bound starting at t0, incrementing per resource"
    },
    "uav_slots": {
        "range": "u0-u63",
        "description": "Unordered Access Views (UAV) for read-write resources",
        "ue5_convention": "RW resources bound starting at u0, incrementing per resource"
    },
    "sampler_slots": {
        "range": "s0-s15",
        "description": "Sampler states",
        "ue5_convention": "Samplers bound starting at s0, shared across materials"
    },
    "cbuffer_slots": {
        "range": "b0-b13",
        "description": "Constant buffers",
        "ue5_convention": "View uniforms at b0, material parameters at b1+",
        "reserved_slots": {
            "b0": "View uniform buffer",
            "b1": "Primitive uniform buffer",
            "b2": "Material uniform buffer"
        }
    },
    "binding_best_practices": [
        "Always specify explicit register bindings in UE5 shaders",
        "Use KAIN @N syntax which maps to register(tN) for textures",
        "Avoid binding conflicts by checking existing slot usage",
        "Group related resources in adjacent slots for better cache locality",
        "Use permutations (CFG_*) to conditionally bind expensive resources"
    ],
    "ue5_specific": {
        "material_parameters": "Bound via FMaterialUniformExpression system",
        "scene_textures": "Bound via FSceneTextureUniformParameters",
        "global_shaders": "Use SHADER_PARAMETER macros for automatic binding"
    }
}

print('Added binding_rules section')

# Write back
print('\nWriting expanded shader_knowledge.json...')
with open(json_path, 'w', encoding='utf-8') as f:
    json.dump(data, f, indent=2)

file_size_kb = json_path.stat().st_size // 1024
print(f'✓ File written: {file_size_kb} KB')
print(f'✓ New sections: hlsl_types, hlsl_keywords, binding_rules')
print('\nExpansion complete!')
