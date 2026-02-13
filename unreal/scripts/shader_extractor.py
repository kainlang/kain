#!/usr/bin/env python3
r"""KAIN Shader Knowledge Extractor

Scans UE5 Engine Shaders directory (usf/ush/h) to extract:
  Pass 1: Intrinsic function catalog (HLSL + UE5 builtins)
  Pass 2: Include graph & dependencies (.ush include chains)
  Pass 3: Permutation & binding patterns (thread groups, params, macros)
  Pass 4: Material & surface output patterns

Usage:
  python shader_extractor.py <shader_dir> [--output <dir>]

Example:
  python shader_extractor.py "D:\Unreal\UE_5.7\Engine\Shaders" --output unreal/metadata
"""

import os
import re
import sys
import json
import time
from collections import defaultdict, Counter
from pathlib import Path

# ═══════════════════════════════════════════════════════════════════
# Configuration
# ═══════════════════════════════════════════════════════════════════

SHADER_EXTENSIONS = {'.usf', '.ush', '.h', '.hlsl', '.glsl'}

# Known HLSL intrinsics (baseline — we'll discover more from the corpus)
KNOWN_HLSL_INTRINSICS = {
    # Math
    'abs', 'acos', 'all', 'any', 'asin', 'atan', 'atan2',
    'ceil', 'clamp', 'clip', 'cos', 'cosh', 'countbits',
    'cross', 'ddx', 'ddx_coarse', 'ddx_fine', 'ddy', 'ddy_coarse', 'ddy_fine',
    'degrees', 'determinant', 'distance', 'dot',
    'exp', 'exp2',
    'f16tof32', 'f32tof16', 'faceforward', 'firstbithigh', 'firstbitlow',
    'floor', 'fma', 'fmod', 'frac', 'frexp', 'fwidth',
    'isfinite', 'isinf', 'isnan',
    'ldexp', 'length', 'lerp', 'lit', 'log', 'log10', 'log2',
    'mad', 'max', 'min', 'modf', 'mul',
    'normalize',
    'pow',
    'radians', 'rcp', 'reflect', 'refract', 'reversebits', 'round', 'rsqrt',
    'saturate', 'sign', 'sin', 'sincos', 'sinh', 'smoothstep', 'sqrt', 'step',
    'tan', 'tanh', 'transpose', 'trunc',
    # Texture
    'tex2D', 'tex2Dlod', 'tex2Dgrad', 'tex2Dbias', 'tex2Dproj',
    'tex3D', 'texCUBE',
    # Wave intrinsics
    'WaveActiveAllEqual', 'WaveActiveBallot', 'WaveActiveBitAnd',
    'WaveActiveBitOr', 'WaveActiveBitXor', 'WaveActiveCountBits',
    'WaveActiveMax', 'WaveActiveMin', 'WaveActiveProduct', 'WaveActiveSum',
    'WaveGetLaneCount', 'WaveGetLaneIndex', 'WaveIsFirstLane',
    'WavePrefixCountBits', 'WavePrefixProduct', 'WavePrefixSum',
    'WaveReadLaneAt', 'WaveReadLaneFirst',
    # Atomic
    'InterlockedAdd', 'InterlockedAnd', 'InterlockedCompareExchange',
    'InterlockedCompareStore', 'InterlockedExchange', 'InterlockedMax',
    'InterlockedMin', 'InterlockedOr', 'InterlockedXor',
    # Barriers
    'GroupMemoryBarrier', 'GroupMemoryBarrierWithGroupSync',
    'DeviceMemoryBarrier', 'DeviceMemoryBarrierWithGroupSync',
    'AllMemoryBarrier', 'AllMemoryBarrierWithGroupSync',
    # Misc
    'asfloat', 'asint', 'asuint', 'D3DCOLORtoUBYTE4',
    'EvaluateAttributeAtCentroid', 'EvaluateAttributeAtSample',
    'EvaluateAttributeSnapped', 'GetRenderTargetSampleCount',
    'GetRenderTargetSamplePosition',
}

# ═══════════════════════════════════════════════════════════════════
# File Discovery
# ═══════════════════════════════════════════════════════════════════

def find_shader_files(root_dir):
    """Find all shader files recursively."""
    files = []
    for dirpath, _, filenames in os.walk(root_dir):
        for fn in filenames:
            ext = os.path.splitext(fn)[1].lower()
            if ext in SHADER_EXTENSIONS:
                files.append(os.path.join(dirpath, fn))
    return sorted(files)

def read_file_safe(path):
    """Read file with encoding fallback."""
    for enc in ('utf-8', 'utf-8-sig', 'latin-1', 'cp1252'):
        try:
            with open(path, 'r', encoding=enc) as f:
                return f.read()
        except (UnicodeDecodeError, UnicodeError):
            continue
    return None

# ═══════════════════════════════════════════════════════════════════
# Pass 1: Intrinsic Function Catalog
# ═══════════════════════════════════════════════════════════════════

# Regex: function call pattern — word followed by (
RE_FUNC_CALL = re.compile(r'\b([A-Za-z_][A-Za-z0-9_]*)\s*\(')
# Regex: function definition — type name(params) { or type name(params)\n{
RE_FUNC_DEF = re.compile(
    r'(?:^|\n)\s*'
    r'(?:inline\s+|static\s+|void\s+|float[234]?\s+|half[234]?\s+|int[234]?\s+|uint[234]?\s+|bool\s+|'
    r'FMaterialPixelParameters\s+|FMaterialAttributes\s+|MaterialFloat[234]?\s+|'
    r'[A-Za-z_][A-Za-z0-9_]*\s+)'
    r'([A-Za-z_][A-Za-z0-9_]*)\s*\('
    r'([^)]*)\)'
    r'\s*\{',
    re.MULTILINE
)
# Regex: macro definitions (#define NAME(...)
RE_MACRO_DEF = re.compile(r'#define\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(([^)]*)\)')

# Things that look like function calls but aren't
CALL_BLACKLIST = {
    'if', 'else', 'for', 'while', 'do', 'switch', 'case', 'return',
    'struct', 'class', 'enum', 'typedef', 'define', 'ifdef', 'ifndef',
    'endif', 'elif', 'include', 'pragma', 'error', 'warning',
    'DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER', 'IMPLEMENT_GLOBAL_SHADER',
    'SHADER_PARAMETER', 'SHADER_PARAMETER_RDG_TEXTURE', 'SHADER_PARAMETER_SAMPLER',
    'SHADER_PARAMETER_RDG_TEXTURE_UAV', 'SHADER_PARAMETER_STRUCT',
    'SHADER_PARAMETER_RDG_BUFFER', 'SHADER_PARAMETER_RDG_BUFFER_UAV',
    'SHADER_PARAMETER_STRUCT_REF', 'SHADER_PARAMETER_STRUCT_INCLUDE',
    'SHADER_PARAMETER_ARRAY', 'SHADER_PARAMETER_TEXTURE',
    'SHADER_PARAMETER_UAV', 'SHADER_PARAMETER_SRV',
    'BEGIN_SHADER_PARAMETER_STRUCT', 'END_SHADER_PARAMETER_STRUCT',
    'DECLARE_GLOBAL_SHADER', 'SHADER_USE_PARAMETER_STRUCT',
    'IMPLEMENT_MATERIAL_SHADER_TYPE', 'IMPLEMENT_SHADER_TYPE',
    'RDG_EVENT_NAME', 'SCOPE_CYCLE_COUNTER', 'check', 'checkSlow',
    'ensure', 'verify', 'TEXT', 'UE_LOG', 'sizeof', 'offsetof',
    'static_cast', 'const_cast', 'reinterpret_cast', 'dynamic_cast',
    'FPermutationDomain', 'TShaderPermutationDomain', 'TShaderMapRef',
    'SHADER_PERMUTATION_BOOL', 'SHADER_PERMUTATION_INT', 'SHADER_PERMUTATION_ENUM',
    'BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT', 'END_GLOBAL_SHADER_PARAMETER_STRUCT',
}

def extract_intrinsics(content, filepath):
    """Extract function calls and definitions from shader content."""
    calls = Counter()
    definitions = {}
    macros = {}
    
    # Strip comments
    clean = re.sub(r'//[^\n]*', '', content)
    clean = re.sub(r'/\*.*?\*/', '', clean, flags=re.DOTALL)
    
    # Find all function calls
    for m in RE_FUNC_CALL.finditer(clean):
        name = m.group(1)
        if name not in CALL_BLACKLIST and not name.startswith('_'):
            calls[name] += 1
    
    # Find function definitions
    for m in RE_FUNC_DEF.finditer(clean):
        name = m.group(1)
        params_str = m.group(2).strip()
        if name not in CALL_BLACKLIST:
            # Parse params
            params = []
            if params_str:
                for p in params_str.split(','):
                    p = p.strip()
                    if p:
                        parts = p.rsplit(None, 1)
                        if len(parts) == 2:
                            params.append({"type": parts[0].strip(), "name": parts[1].strip()})
                        else:
                            params.append({"type": p, "name": ""})
            
            rel_path = os.path.basename(filepath)
            # Keep definition with most params (most detailed signature)
            if name not in definitions or len(params) > len(definitions[name].get('params', [])):
                definitions[name] = {
                    "name": name,
                    "params": params,
                    "param_count": len(params),
                    "source": rel_path,
                }
    
    # Find macro definitions (many UE5 "functions" are actually macros)
    for m in RE_MACRO_DEF.finditer(content):
        name = m.group(1)
        params_str = m.group(2).strip()
        if name not in CALL_BLACKLIST:
            param_names = [p.strip() for p in params_str.split(',') if p.strip()] if params_str else []
            macros[name] = {
                "name": name,
                "params": param_names,
                "param_count": len(param_names),
                "source": os.path.basename(filepath),
                "is_macro": True,
            }
    
    return calls, definitions, macros

# ═══════════════════════════════════════════════════════════════════
# Pass 2: Include Graph
# ═══════════════════════════════════════════════════════════════════

RE_INCLUDE = re.compile(r'#include\s+"([^"]+)"')

def extract_includes(content, filepath, root_dir):
    """Extract #include directives and build dependency graph."""
    includes = []
    rel_path = os.path.relpath(filepath, root_dir).replace('\\', '/')
    
    for m in RE_INCLUDE.finditer(content):
        inc_path = m.group(1)
        includes.append(inc_path)
    
    return rel_path, includes

# ═══════════════════════════════════════════════════════════════════
# Pass 3: Permutation & Binding Patterns
# ═══════════════════════════════════════════════════════════════════

RE_NUMTHREADS = re.compile(r'\[numthreads\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)\s*\]')
RE_PERMUTATION_BOOL = re.compile(r'SHADER_PERMUTATION_BOOL\s*\(\s*"?([A-Za-z_][A-Za-z0-9_]*)"?\s*\)')
RE_PERMUTATION_INT = re.compile(r'SHADER_PERMUTATION_INT\s*\(\s*"?([A-Za-z_][A-Za-z0-9_]*)"?\s*,\s*(\d+)\s*\)')
RE_PERMUTATION_ENUM = re.compile(r'SHADER_PERMUTATION_ENUM_CLASS\s*\(\s*"?([A-Za-z_][A-Za-z0-9_]*)"?\s*,\s*([A-Za-z_][A-Za-z0-9_:]*)\s*\)')
RE_SHADER_PARAM = re.compile(r'SHADER_PARAMETER\s*\(\s*([^,]+)\s*,\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)')
RE_SHADER_PARAM_RDG_TEX = re.compile(r'SHADER_PARAMETER_RDG_TEXTURE\s*\(\s*([^,]+)\s*,\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)')
RE_SHADER_PARAM_UAV = re.compile(r'SHADER_PARAMETER_RDG_TEXTURE_UAV\s*\(\s*([^,]+)\s*,\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)')
RE_SHADER_PARAM_BUF = re.compile(r'SHADER_PARAMETER_RDG_BUFFER(?:_SRV|_UAV)?\s*\(\s*([^,]+)\s*,\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)')
RE_SHADER_PARAM_SAMPLER = re.compile(r'SHADER_PARAMETER_SAMPLER\s*\(\s*([^,]+)\s*,\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)')
RE_REGISTER_DECL = re.compile(r'((?:RW)?(?:Texture2D|Texture3D|TextureCube|Buffer|StructuredBuffer|ByteAddressBuffer)(?:<[^>]+>)?)\s+([A-Za-z_][A-Za-z0-9_]*)\s*(?::\s*register\s*\(\s*([a-z])(\d+)\s*\))?')
RE_GROUPSHARED = re.compile(r'groupshared\s+(\S+)\s+([A-Za-z_][A-Za-z0-9_]*)\s*\[([^\]]*)\]')
RE_CBUFFER = re.compile(r'cbuffer\s+([A-Za-z_][A-Za-z0-9_]*)')
# UE5 ifdef permutation usage in .usf files  
RE_IFDEF_PERM = re.compile(r'#(?:if|ifdef|elif)\s+(?:defined\s*\(\s*)?([A-Z][A-Z0-9_]*(?:_[A-Z0-9]+)+)\s*\)?')

def extract_bindings(content, filepath):
    """Extract shader parameter bindings, thread groups, permutations."""
    result = {
        "thread_groups": [],
        "permutations": [],
        "parameters": [],
        "textures": [],
        "uavs": [],
        "buffers": [],
        "samplers": [],
        "register_decls": [],
        "groupshared": [],
        "cbuffers": [],
    }
    
    rel = os.path.basename(filepath)
    
    # Thread groups
    for m in RE_NUMTHREADS.finditer(content):
        result["thread_groups"].append({
            "x": int(m.group(1)),
            "y": int(m.group(2)),
            "z": int(m.group(3)),
            "source": rel,
        })
    
    # Permutations (from .h/.cpp files with SHADER_PERMUTATION_*)
    for m in RE_PERMUTATION_BOOL.finditer(content):
        result["permutations"].append({"name": m.group(1), "type": "bool", "source": rel})
    for m in RE_PERMUTATION_INT.finditer(content):
        result["permutations"].append({"name": m.group(1), "type": "int", "range": int(m.group(2)), "source": rel})
    for m in RE_PERMUTATION_ENUM.finditer(content):
        result["permutations"].append({"name": m.group(1), "type": "enum", "enum_class": m.group(2), "source": rel})
    
    # Also find permutations used as #ifdef in .usf files
    for m in RE_IFDEF_PERM.finditer(content):
        name = m.group(1)
        # Filter to likely permutations
        if any(name.startswith(p) for p in ('USE_', 'ENABLE_', 'HAS_', 'WITH_', 'ALLOW_', 'SUPPORT_',
                                              'FEATURE_', 'IS_', 'NEEDS_', 'WANT_', 'OUTPUT_',
                                              'MATERIAL_', 'VERTEX_', 'PIXEL_', 'COMPUTE_',
                                              'STRATA_', 'SUBSTRATE_', 'VIRTUAL_', 'NANITE_',
                                              'LUMEN_', 'HAIR_', 'WATER_', 'CLOUD_',
                                              'RAYHIT_', 'RAYTRACING_', 'DIM_')):
            result["permutations"].append({"name": name, "type": "ifdef", "source": rel})
    
    # SHADER_PARAMETER(type, name)
    for m in RE_SHADER_PARAM.finditer(content):
        result["parameters"].append({"type": m.group(1).strip(), "name": m.group(2), "source": rel})
    
    # RDG textures
    for m in RE_SHADER_PARAM_RDG_TEX.finditer(content):
        result["textures"].append({"type": m.group(1).strip(), "name": m.group(2), "source": rel})
    
    # UAVs
    for m in RE_SHADER_PARAM_UAV.finditer(content):
        result["uavs"].append({"type": m.group(1).strip(), "name": m.group(2), "source": rel})
    
    # Buffers
    for m in RE_SHADER_PARAM_BUF.finditer(content):
        result["buffers"].append({"type": m.group(1).strip(), "name": m.group(2), "source": rel})
    
    # Samplers
    for m in RE_SHADER_PARAM_SAMPLER.finditer(content):
        result["samplers"].append({"type": m.group(1).strip(), "name": m.group(2), "source": rel})
    
    # Register declarations (in .usf files)
    for m in RE_REGISTER_DECL.finditer(content):
        entry = {"type": m.group(1), "name": m.group(2), "source": rel}
        if m.group(3):
            entry["register_type"] = m.group(3)
            entry["register_slot"] = int(m.group(4))
        result["register_decls"].append(entry)
    
    # Groupshared
    for m in RE_GROUPSHARED.finditer(content):
        result["groupshared"].append({
            "type": m.group(1),
            "name": m.group(2),
            "size_expr": m.group(3),
            "source": rel,
        })
    
    # cbuffers
    for m in RE_CBUFFER.finditer(content):
        result["cbuffers"].append({"name": m.group(1), "source": rel})
    
    return result

# ═══════════════════════════════════════════════════════════════════
# Pass 4: Material & Surface Patterns
# ═══════════════════════════════════════════════════════════════════

# Material output properties set via PixelMaterialInputs or MaterialParameters
RE_MATERIAL_OUTPUT = re.compile(r'(?:PixelMaterialInputs|MaterialInputs)\s*\.\s*([A-Za-z_][A-Za-z0-9_]*)\s*=')
RE_MATERIAL_PARAM = re.compile(r'(?:Parameters|MaterialParameters)\s*\.\s*([A-Za-z_][A-Za-z0-9_]*)')
RE_MATERIAL_FLOAT = re.compile(r'MaterialFloat([234]?)\s+([A-Za-z_][A-Za-z0-9_]*)')
RE_GET_MATERIAL = re.compile(r'Get(?:Material|Pixel)([A-Za-z_]+)\s*\(')
RE_CUSTOM_EXPRESSION = re.compile(r'CustomExpression\d+\s*\(')

def extract_material_patterns(content, filepath):
    """Extract material/surface shader patterns."""
    result = {
        "outputs": [],
        "param_accesses": [],
        "material_types": [],
        "material_getters": [],
    }
    
    rel = os.path.basename(filepath)
    
    # Material output assignments
    for m in RE_MATERIAL_OUTPUT.finditer(content):
        result["outputs"].append({"name": m.group(1), "source": rel})
    
    # Material parameter accesses  
    for m in RE_MATERIAL_PARAM.finditer(content):
        result["param_accesses"].append({"name": m.group(1), "source": rel})
    
    # MaterialFloat types
    for m in RE_MATERIAL_FLOAT.finditer(content):
        suffix = m.group(1) or ""
        result["material_types"].append({
            "type": f"MaterialFloat{suffix}",
            "name": m.group(2),
            "source": rel,
        })
    
    # Get* material helper functions
    for m in RE_GET_MATERIAL.finditer(content):
        result["material_getters"].append({"name": f"Get{m.group(1)}", "source": rel})
    
    return result

# ═══════════════════════════════════════════════════════════════════
# Main Extraction Pipeline
# ═══════════════════════════════════════════════════════════════════

def run_extraction(shader_dir, output_dir):
    print("=" * 70)
    print("🔍 KAIN Shader Knowledge Extractor v1")
    print("=" * 70)
    print(f"📂 Shader directory: {shader_dir}")
    print(f"📁 Output: {output_dir}")
    print()
    
    start = time.time()
    
    # Discover files
    files = find_shader_files(shader_dir)
    usf_files = [f for f in files if f.endswith('.usf')]
    ush_files = [f for f in files if f.endswith('.ush')]
    h_files = [f for f in files if f.endswith('.h')]
    other_files = [f for f in files if not any(f.endswith(e) for e in ('.usf', '.ush', '.h'))]
    
    print(f"📊 Files found: {len(files)} total")
    print(f"   .usf: {len(usf_files)}")
    print(f"   .ush: {len(ush_files)}")
    print(f"   .h:   {len(h_files)}")
    print(f"   other: {len(other_files)}")
    print()
    
    # ─── Pass 1: Intrinsics ───
    print("━" * 70)
    print("📚 PASS 1: Intrinsic Function Catalog")
    print("━" * 70)
    
    all_calls = Counter()
    all_definitions = {}
    all_macros = {}
    
    for i, fpath in enumerate(files):
        content = read_file_safe(fpath)
        if not content:
            continue
        
        calls, defs, macros = extract_intrinsics(content, fpath)
        all_calls.update(calls)
        all_definitions.update(defs)
        all_macros.update(macros)
        
        if (i + 1) % 200 == 0:
            print(f"  📊 Progress: {i+1}/{len(files)} files ({100*(i+1)//len(files)}%)")
    
    # Classify functions
    hlsl_intrinsics = {}
    ue5_functions = {}
    user_defined = {}
    
    for name, count in all_calls.most_common():
        entry = {
            "name": name,
            "call_count": count,
        }
        
        if name in all_definitions:
            entry.update(all_definitions[name])
        elif name in all_macros:
            entry.update(all_macros[name])
        
        if name in KNOWN_HLSL_INTRINSICS:
            entry["category"] = "hlsl"
            hlsl_intrinsics[name] = entry
        elif name in all_definitions:
            # Defined in the corpus = UE5 helper or user function
            entry["category"] = "ue5"
            ue5_functions[name] = entry
        elif name in all_macros:
            entry["category"] = "macro"
            ue5_functions[name] = entry
        else:
            # Called but not defined here — could be a type constructor, method, or external
            if count >= 3:  # Only track if called 3+ times
                entry["category"] = "external"
                ue5_functions[name] = entry
    
    print(f"\n  📊 Pass 1 Results:")
    print(f"     HLSL intrinsics found:  {len(hlsl_intrinsics)}")
    print(f"     UE5 functions/macros:   {len(ue5_functions)}")
    print(f"     Function definitions:   {len(all_definitions)}")
    print(f"     Macro definitions:      {len(all_macros)}")
    
    # ─── Pass 2: Include Graph ───
    print()
    print("━" * 70)
    print("📦 PASS 2: Include Graph & Dependencies")
    print("━" * 70)
    
    include_graph = {}  # file -> [includes]
    include_frequency = Counter()  # which files are included most
    
    for fpath in files:
        content = read_file_safe(fpath)
        if not content:
            continue
        
        rel, includes = extract_includes(content, fpath, shader_dir)
        if includes:
            include_graph[rel] = includes
            for inc in includes:
                include_frequency[inc] += 1
    
    # Build reverse map: what does each .ush provide?
    # Track which functions are defined in which files
    file_provides = {}
    for name, info in all_definitions.items():
        src = info.get("source", "")
        if src:
            if src not in file_provides:
                file_provides[src] = []
            file_provides[src].append(name)
    for name, info in all_macros.items():
        src = info.get("source", "")
        if src:
            if src not in file_provides:
                file_provides[src] = []
            file_provides[src].append(name)
    
    print(f"\n  📊 Pass 2 Results:")
    print(f"     Files with includes:    {len(include_graph)}")
    print(f"     Unique include paths:   {len(include_frequency)}")
    print(f"     Most included files:")
    for path, count in include_frequency.most_common(15):
        print(f"       {count:4d}x  {path}")
    
    # ─── Pass 3: Bindings & Permutations ───
    print()
    print("━" * 70)
    print("⚙️  PASS 3: Permutation & Binding Patterns")
    print("━" * 70)
    
    all_thread_groups = []
    all_permutations = Counter()  # name -> count
    all_permutation_details = {}
    all_parameters = Counter()
    all_param_types = {}  # name -> type
    all_textures = Counter()
    all_uavs = Counter()
    all_buffers = Counter()
    all_groupshared_patterns = []
    all_cbuffers = []
    thread_group_counter = Counter()
    
    for fpath in files:
        content = read_file_safe(fpath)
        if not content:
            continue
        
        bindings = extract_bindings(content, fpath)
        
        for tg in bindings["thread_groups"]:
            key = f"{tg['x']}x{tg['y']}x{tg['z']}"
            thread_group_counter[key] += 1
            all_thread_groups.append(tg)
        
        for perm in bindings["permutations"]:
            all_permutations[perm["name"]] += 1
            if perm["name"] not in all_permutation_details:
                all_permutation_details[perm["name"]] = perm
        
        for param in bindings["parameters"]:
            all_parameters[param["name"]] += 1
            all_param_types[param["name"]] = param["type"]
        
        for tex in bindings["textures"]:
            all_textures[tex["name"]] += 1
        
        for uav in bindings["uavs"]:
            all_uavs[uav["name"]] += 1
        
        for buf in bindings["buffers"]:
            all_buffers[buf["name"]] += 1
        
        all_groupshared_patterns.extend(bindings["groupshared"])
        all_cbuffers.extend(bindings["cbuffers"])
    
    print(f"\n  📊 Pass 3 Results:")
    print(f"     Thread group patterns:  {len(thread_group_counter)}")
    for key, count in thread_group_counter.most_common(10):
        print(f"       {count:4d}x  [{key}]")
    print(f"     Unique permutations:    {len(all_permutations)}")
    print(f"     Shader parameters:      {len(all_parameters)}")
    print(f"     Texture bindings:       {len(all_textures)}")
    print(f"     UAV bindings:           {len(all_uavs)}")
    print(f"     Buffer bindings:        {len(all_buffers)}")
    print(f"     Groupshared vars:       {len(all_groupshared_patterns)}")
    print(f"     cbuffers:               {len(all_cbuffers)}")
    
    # ─── Pass 4: Material Patterns ───
    print()
    print("━" * 70)
    print("🎨 PASS 4: Material & Surface Patterns")
    print("━" * 70)
    
    material_outputs = Counter()
    material_params = Counter()
    material_getters = Counter()
    material_types = Counter()
    
    for fpath in files:
        content = read_file_safe(fpath)
        if not content:
            continue
        
        mat = extract_material_patterns(content, fpath)
        
        for out in mat["outputs"]:
            material_outputs[out["name"]] += 1
        for p in mat["param_accesses"]:
            material_params[p["name"]] += 1
        for g in mat["material_getters"]:
            material_getters[g["name"]] += 1
        for t in mat["material_types"]:
            material_types[t["type"]] += 1
    
    print(f"\n  📊 Pass 4 Results:")
    print(f"     Material outputs:       {len(material_outputs)}")
    if material_outputs:
        print(f"     Top outputs:")
        for name, count in material_outputs.most_common(15):
            print(f"       {count:4d}x  {name}")
    print(f"     Material parameters:    {len(material_params)}")
    print(f"     Material getters:       {len(material_getters)}")
    if material_getters:
        print(f"     Top getters:")
        for name, count in material_getters.most_common(15):
            print(f"       {count:4d}x  {name}")
    print(f"     MaterialFloat types:    {len(material_types)}")
    
    # ═══════════════════════════════════════════════════════════════
    # Build Output JSON
    # ═══════════════════════════════════════════════════════════════
    
    os.makedirs(output_dir, exist_ok=True)
    
    # Build the intrinsics catalog
    intrinsics_catalog = {}
    
    # HLSL intrinsics
    for name, info in sorted(hlsl_intrinsics.items()):
        intrinsics_catalog[name] = {
            "name": name,
            "category": "hlsl",
            "call_count": info["call_count"],
            "params": info.get("params", []),
            "param_count": info.get("param_count", 0),
        }
    
    # UE5 functions (only include functions called 2+ times to reduce noise)
    for name, info in sorted(ue5_functions.items()):
        if info["call_count"] >= 2:
            intrinsics_catalog[name] = {
                "name": name,
                "category": info.get("category", "ue5"),
                "call_count": info["call_count"],
                "params": info.get("params", []),
                "param_count": info.get("param_count", 0),
                "source": info.get("source", ""),
                "is_macro": info.get("is_macro", False),
            }
    
    # Build include dependency data
    include_data = {
        "graph": include_graph,
        "frequency": {k: v for k, v in include_frequency.most_common()},
        "file_provides": {k: v[:20] for k, v in sorted(file_provides.items())},  # Cap at 20 per file
    }
    
    # Build permutation data
    permutation_data = {}
    for name, count in all_permutations.most_common():
        entry = {"name": name, "usage_count": count}
        if name in all_permutation_details:
            detail = all_permutation_details[name]
            entry["type"] = detail.get("type", "ifdef")
            if "range" in detail:
                entry["range"] = detail["range"]
            if "enum_class" in detail:
                entry["enum_class"] = detail["enum_class"]
        else:
            entry["type"] = "ifdef"
        permutation_data[name] = entry
    
    # Build binding patterns
    binding_patterns = {
        "thread_groups": {k: v for k, v in thread_group_counter.most_common()},
        "parameter_types": {name: all_param_types.get(name, "unknown") for name, _ in all_parameters.most_common(200)},
        "common_textures": {name: count for name, count in all_textures.most_common(100)},
        "common_uavs": {name: count for name, count in all_uavs.most_common(100)},
        "common_buffers": {name: count for name, count in all_buffers.most_common(100)},
        "groupshared_patterns": all_groupshared_patterns[:50],
        "cbuffers": list({cb["name"] for cb in all_cbuffers}),
    }
    
    # Build material data
    material_data = {
        "outputs": {name: count for name, count in material_outputs.most_common()},
        "parameters": {name: count for name, count in material_params.most_common(100)},
        "getters": {name: count for name, count in material_getters.most_common(100)},
        "types": {name: count for name, count in material_types.most_common()},
    }
    
    # Assemble final JSON
    shader_knowledge = {
        "engine_version": "5.7",
        "extraction_stats": {
            "total_files": len(files),
            "usf_files": len(usf_files),
            "ush_files": len(ush_files),
            "h_files": len(h_files),
            "total_intrinsics": len(intrinsics_catalog),
            "total_permutations": len(permutation_data),
            "total_thread_groups": len(thread_group_counter),
        },
        "intrinsics": intrinsics_catalog,
        "includes": include_data,
        "permutations": permutation_data,
        "bindings": binding_patterns,
        "material": material_data,
    }
    
    output_path = os.path.join(output_dir, "shader_knowledge.json")
    with open(output_path, 'w', encoding='utf-8') as f:
        json.dump(shader_knowledge, f, indent=2, default=str)
    
    size_kb = os.path.getsize(output_path) / 1024
    print(f"\n  💾 Saved: {output_path} ({size_kb:.0f} KB)")
    
    elapsed = time.time() - start
    print()
    print("=" * 70)
    print(f"✅ Extraction complete in {elapsed:.1f}s")
    print("=" * 70)

# ═══════════════════════════════════════════════════════════════════
# CLI
# ═══════════════════════════════════════════════════════════════════

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    
    shader_dir = sys.argv[1]
    output_dir = "unreal/metadata"
    
    # Parse --output flag
    if '--output' in sys.argv:
        idx = sys.argv.index('--output')
        if idx + 1 < len(sys.argv):
            output_dir = sys.argv[idx + 1]
    
    if not os.path.isdir(shader_dir):
        print(f"❌ Not a directory: {shader_dir}")
        sys.exit(1)
    
    run_extraction(shader_dir, output_dir)
