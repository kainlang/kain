#!/usr/bin/env python3
"""
UE5 Module Dependency Graph Extractor
Scans all .Build.cs files from the Unreal Engine source to extract:
  1. Module names and their category (Runtime, Editor, Developer, Program, ThirdParty)
  2. Public and private dependency module names
  3. Dynamically loaded modules
  4. Include path modules
  5. Cross-references with engine_scanned.json to map types → modules

Output: module_graph.json — loaded by KAIN codegen for auto-deriving Build.cs dependencies

Usage:
  python module_graph_extractor.py <UE_SOURCE_DIR> [--engine-scan <engine_scanned.json>]
  
Example:
  python module_graph_extractor.py "D:/Unreal/UE_5.7/Engine/Source" --engine-scan ../metadata/engine_5.7_scanned.json
"""

import os
import re
import json
import sys
import argparse
from collections import defaultdict
from pathlib import Path


# ============================================================================
# Pass 1: Parse all .Build.cs files for module metadata + dependencies
# ============================================================================

# Regex: class declaration → module name
RE_CLASS_DECL = re.compile(
    r'public\s+class\s+(\w+)\s*:\s*ModuleRules'
)

# Regex: .AddRange(new string[] { "A", "B", ... })
RE_ADD_RANGE = re.compile(
    r'(\w+)\.AddRange\s*\(\s*new\s+string\s*\[\s*\]\s*\{([^}]*)\}',
    re.DOTALL
)

# Regex: .Add("ModuleName")
RE_ADD_SINGLE = re.compile(
    r'(\w+)\.Add\s*\(\s*"([^"]+)"\s*\)'
)

# Known dependency list field names and their categories
DEP_FIELDS = {
    "PublicDependencyModuleNames":       "public_deps",
    "PrivateDependencyModuleNames":      "private_deps",
    "DynamicallyLoadedModuleNames":      "dynamic_deps",
    "PrivateIncludePathModuleNames":     "private_include_path_modules",
    "PublicIncludePathModuleNames":      "public_include_path_modules",
}


def infer_module_category(filepath, ue_source_dir):
    """Infer module category from its path under Engine/Source/."""
    rel = os.path.relpath(filepath, ue_source_dir).replace("\\", "/")
    parts = rel.split("/")
    if len(parts) >= 1:
        top = parts[0]
        if top == "Runtime":
            return "Runtime"
        elif top == "Editor":
            return "Editor"
        elif top == "Developer":
            return "Developer"
        elif top == "Programs":
            return "Program"
        elif top == "ThirdParty":
            return "ThirdParty"
    return "Unknown"


def extract_string_list(text):
    """Extract quoted strings from a C# string array initializer body."""
    return re.findall(r'"([^"]+)"', text)


def parse_build_cs(filepath, ue_source_dir):
    """Parse a single .Build.cs file and return module info."""
    with open(filepath, "r", encoding="utf-8-sig", errors="replace") as f:
        content = f.read()

    # Extract module name from class declaration
    m = RE_CLASS_DECL.search(content)
    if not m:
        return None
    module_name = m.group(1)

    category = infer_module_category(filepath, ue_source_dir)
    rel_path = os.path.relpath(filepath, ue_source_dir).replace("\\", "/")

    # Extract module directory (parent of the .Build.cs file)
    module_dir = os.path.dirname(filepath)

    info = {
        "name": module_name,
        "category": category,
        "path": rel_path,
        "public_deps": [],
        "private_deps": [],
        "dynamic_deps": [],
        "private_include_path_modules": [],
        "public_include_path_modules": [],
    }

    # Parse AddRange calls
    for match in RE_ADD_RANGE.finditer(content):
        field_name = match.group(1)
        body = match.group(2)
        strings = extract_string_list(body)
        if field_name in DEP_FIELDS:
            key = DEP_FIELDS[field_name]
            info[key].extend(strings)

    # Parse single .Add() calls
    for match in RE_ADD_SINGLE.finditer(content):
        field_name = match.group(1)
        value = match.group(2)
        if field_name in DEP_FIELDS:
            key = DEP_FIELDS[field_name]
            info[key].append(value)

    # Deduplicate while preserving order
    for key in DEP_FIELDS.values():
        seen = set()
        deduped = []
        for v in info[key]:
            if v not in seen:
                seen.add(v)
                deduped.append(v)
        info[key] = deduped

    return info


def scan_all_build_cs(ue_source_dir):
    """Walk the UE source tree and parse every .Build.cs file."""
    modules = {}
    skipped = 0

    for root, dirs, files in os.walk(ue_source_dir):
        for fname in sorted(files):
            if not fname.endswith(".Build.cs") and not fname.endswith(".build.cs"):
                continue
            fpath = os.path.join(root, fname)
            info = parse_build_cs(fpath, ue_source_dir)
            if info:
                modules[info["name"]] = info
            else:
                skipped += 1

    return modules, skipped


# ============================================================================
# Pass 2: Cross-reference with engine scan to build type → module mapping
# ============================================================================

def build_type_to_module_map(engine_scan_path, modules):
    """
    Cross-reference engine_scanned.json with module graph to create
    a comprehensive type → module mapping.
    
    The engine scan has classes/structs/enums with 'module' fields.
    We validate those against our extracted module graph.
    """
    type_to_module = {}
    
    if not engine_scan_path or not os.path.exists(engine_scan_path):
        return type_to_module
    
    print(f"  Loading engine scan: {engine_scan_path}")
    with open(engine_scan_path, "r", encoding="utf-8") as f:
        scan = json.load(f)
    
    # Extract type→module from classes
    for cls in scan.get("classes", []):
        name = cls.get("name", "")
        module = cls.get("module", "")
        if name and module:
            type_to_module[name] = module
    
    # Extract type→module from structs
    for st in scan.get("structs", []):
        name = st.get("name", "")
        module = st.get("module", "")
        if name and module:
            type_to_module[name] = module
    
    # Extract type→module from enums
    for en in scan.get("enums", []):
        name = en.get("name", "")
        module = en.get("module", "")
        if name and module:
            type_to_module[name] = module
    
    # Validate: only keep types whose module actually exists in our graph
    validated = {}
    unknown_modules = set()
    for type_name, module_name in type_to_module.items():
        if module_name in modules:
            validated[type_name] = module_name
        else:
            unknown_modules.add(module_name)
    
    if unknown_modules:
        print(f"  ⚠ {len(unknown_modules)} modules referenced in scan but not found in .Build.cs files")
        # Still include them — they might be plugin modules or platform-specific
        validated = type_to_module
    
    return validated


# ============================================================================
# Pass 3: Compute transitive dependency closure (for "what do I really need?")
# ============================================================================

def compute_transitive_deps(modules, max_depth=10):
    """
    For each module, compute the full transitive closure of public dependencies.
    This answers: "If I depend on RenderCore, what modules do I transitively get?"
    
    Only follows public_deps since those are the ones that propagate to dependents.
    """
    transitive = {}
    
    for mod_name in modules:
        visited = set()
        queue = list(modules[mod_name]["public_deps"])
        depth = 0
        while queue and depth < max_depth:
            next_queue = []
            for dep in queue:
                if dep not in visited:
                    visited.add(dep)
                    if dep in modules:
                        next_queue.extend(modules[dep]["public_deps"])
            queue = next_queue
            depth += 1
        transitive[mod_name] = sorted(visited)
    
    return transitive


# ============================================================================
# Pass 4: Build reverse index — "which modules export this API/header?"
# ============================================================================

def scan_public_headers(ue_source_dir, modules):
    """
    For each module, scan its Public/ directory for header files.
    This lets us answer: "FShaderMapResource.h is in RenderCore's Public/ → needs RenderCore"
    """
    header_to_module = {}
    module_headers = {}
    
    for mod_name, info in modules.items():
        build_cs_path = os.path.join(ue_source_dir, info["path"])
        module_dir = os.path.dirname(build_cs_path)
        
        # Check for Public/ subdirectory
        public_dir = os.path.join(module_dir, "Public")
        if not os.path.isdir(public_dir):
            # Some modules use Classes/ instead
            public_dir = os.path.join(module_dir, "Classes")
        if not os.path.isdir(public_dir):
            continue
        
        headers = []
        for root, dirs, files in os.walk(public_dir):
            for fname in files:
                if fname.endswith(".h"):
                    # Store relative path from Public/ dir
                    rel = os.path.relpath(os.path.join(root, fname), public_dir).replace("\\", "/")
                    headers.append(rel)
                    # Map header filename → module (for quick lookup)
                    header_to_module[fname] = mod_name
                    # Also map full relative path
                    header_to_module[rel] = mod_name
        
        if headers:
            module_headers[mod_name] = len(headers)
    
    return header_to_module, module_headers


# ============================================================================
# Pass 5: Identify key API functions per module (high-value exports)
# ============================================================================

# These are functions/symbols we know are commonly needed and cause linker errors
# when the wrong module is missing. We extract them from known headers.
KNOWN_API_EXPORTS = {
    "RenderCore": [
        "AllShaderSourceDirectoryMappings",
        "AddShaderSourceDirectoryMapping",
        "ResetAllShaderSourceDirectoryMappings",
        "GetShaderSourceDirectoryMapping",
        "FShader",
        "FGlobalShader",
        "FShaderType",
        "FShaderMapResource",
        "IMPLEMENT_GLOBAL_SHADER",
        "DECLARE_GLOBAL_SHADER",
    ],
    "RHI": [
        "FRHICommandList",
        "FRHIResource",
        "GDynamicRHI",
        "RHICreateVertexBuffer",
        "RHICreateIndexBuffer",
    ],
    "Renderer": [
        "FSceneRenderer",
        "FDeferredShadingSceneRenderer",
    ],
    "Slate": [
        "SCompoundWidget",
        "SLeafWidget",
        "SPanel",
        "SWidget",
        "SLATE_BEGIN_ARGS",
        "SLATE_END_ARGS",
        "SLATE_ARGUMENT",
        "SLATE_ATTRIBUTE",
        "SLATE_EVENT",
    ],
    "SlateCore": [
        "FSlateApplication",
        "FSlateStyleSet",
        "FSlateBrush",
        "FSlateColor",
        "FSlateIcon",
    ],
    "UnrealEd": [
        "FAssetEditorToolkit",
        "IDetailCustomization",
        "FEditorViewportClient",
        "FEdModeToolkit",
        "IDetailsView",
        "FPropertyEditorModule",
    ],
    "AssetTools": [
        "IAssetTools",
        "FAssetTypeActions_Base",
        "UAssetDefinition",
    ],
    "PropertyEditor": [
        "IDetailLayoutBuilder",
        "IDetailCategoryBuilder",
        "IPropertyHandle",
    ],
    "Projects": [
        "IPluginManager",
        "IPlugin",
    ],
    "InputCore": [
        "FKey",
        "EKeys",
    ],
    "CoreUObject": [
        "UObject",
        "UClass",
        "UPackage",
        "UField",
        "FObjectInitializer",
    ],
    "Engine": [
        "AActor",
        "APawn",
        "ACharacter",
        "APlayerController",
        "AGameModeBase",
        "UActorComponent",
        "USceneComponent",
        "UWorld",
        "UGameInstance",
    ],
}


def build_api_to_module_map(modules):
    """Build a reverse map from known API symbols to their module."""
    api_to_module = {}
    for mod_name, symbols in KNOWN_API_EXPORTS.items():
        if mod_name in modules:  # Only include modules that actually exist
            for sym in symbols:
                api_to_module[sym] = mod_name
    return api_to_module


# ============================================================================
# Main: Orchestrate all passes and write output
# ============================================================================

def main():
    parser = argparse.ArgumentParser(description="UE5 Module Dependency Graph Extractor")
    parser.add_argument("ue_source_dir", help="Path to UE5 Engine/Source directory")
    parser.add_argument("--engine-scan", help="Path to engine_scanned.json for type→module cross-reference")
    parser.add_argument("--output", "-o", default=None, help="Output path (default: ../metadata/module_graph.json)")
    args = parser.parse_args()

    ue_source_dir = args.ue_source_dir
    if not os.path.isdir(ue_source_dir):
        print(f"ERROR: {ue_source_dir} is not a directory")
        sys.exit(1)

    output_path = args.output or os.path.join(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
        "metadata", "module_graph.json"
    )

    print(f"🔍 Scanning .Build.cs files in: {ue_source_dir}")
    print()

    # Pass 1: Parse all .Build.cs files
    print("📦 Pass 1: Parsing .Build.cs files...")
    modules, skipped = scan_all_build_cs(ue_source_dir)
    print(f"   ✓ {len(modules)} modules extracted ({skipped} skipped)")

    # Category breakdown
    categories = defaultdict(int)
    for info in modules.values():
        categories[info["category"]] += 1
    for cat, count in sorted(categories.items()):
        print(f"     {cat}: {count}")
    print()

    # Pass 2: Cross-reference with engine scan
    print("🔗 Pass 2: Cross-referencing type → module mapping...")
    type_to_module = build_type_to_module_map(args.engine_scan, modules)
    print(f"   ✓ {len(type_to_module)} types mapped to modules")
    print()

    # Pass 3: Compute transitive deps
    print("🌐 Pass 3: Computing transitive dependency closure...")
    transitive_deps = compute_transitive_deps(modules)
    # Stats
    max_transitive = max((len(v) for v in transitive_deps.values()), default=0)
    avg_transitive = sum(len(v) for v in transitive_deps.values()) / max(len(transitive_deps), 1)
    print(f"   ✓ Max transitive depth: {max_transitive} deps")
    print(f"   ✓ Average transitive deps: {avg_transitive:.1f}")
    print()

    # Pass 4: Scan public headers
    print("📂 Pass 4: Scanning public headers per module...")
    header_to_module, module_header_counts = scan_public_headers(ue_source_dir, modules)
    print(f"   ✓ {len(header_to_module)} headers mapped to modules")
    print(f"   ✓ {len(module_header_counts)} modules have public headers")
    print()

    # Pass 5: Build API → module map
    print("🔧 Pass 5: Building API symbol → module map...")
    api_to_module = build_api_to_module_map(modules)
    print(f"   ✓ {len(api_to_module)} API symbols mapped")
    print()

    # Assemble output
    output = {
        "_meta": {
            "generator": "module_graph_extractor.py",
            "source": ue_source_dir,
            "engine_scan": args.engine_scan or "",
            "total_modules": len(modules),
            "total_types_mapped": len(type_to_module),
            "total_headers_mapped": len(header_to_module),
            "total_api_symbols": len(api_to_module),
            "description": "UE5 module dependency graph for auto-deriving Build.cs dependencies"
        },
        "modules": modules,
        "transitive_public_deps": transitive_deps,
        "type_to_module": type_to_module,
        "header_to_module": header_to_module,
        "api_to_module": api_to_module,
    }

    # Write output
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(output, f, indent=2, sort_keys=False)

    file_size = os.path.getsize(output_path)
    print(f"✅ Written: {output_path}")
    print(f"   Size: {file_size / 1024:.1f} KB")
    print()

    # Print some useful stats
    print("📊 Key module stats:")
    key_modules = ["Core", "CoreUObject", "Engine", "RenderCore", "RHI", 
                   "Slate", "SlateCore", "UnrealEd", "AssetTools", "Projects"]
    for mod in key_modules:
        if mod in modules:
            info = modules[mod]
            pub = len(info["public_deps"])
            priv = len(info["private_deps"])
            trans = len(transitive_deps.get(mod, []))
            print(f"   {mod}: {pub} public + {priv} private deps, {trans} transitive")
    
    print()
    print("🎯 Usage in KAIN codegen:")
    print("   1. type_to_module[\"FShader\"] → \"RenderCore\"")
    print("   2. modules[\"RenderCore\"][\"public_deps\"] → [\"RHI\", \"CoreUObject\"]")
    print("   3. header_to_module[\"ShaderCore.h\"] → \"RenderCore\"")
    print("   4. api_to_module[\"AddShaderSourceDirectoryMapping\"] → \"RenderCore\"")


if __name__ == "__main__":
    main()
