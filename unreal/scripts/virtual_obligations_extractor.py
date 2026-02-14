"""
UE5 Virtual Method Obligations Extractor

Scans all C++ headers in the Unreal Engine source tree to extract:
1. Pure virtual methods (= 0) per class/interface
2. Class inheritance chains
3. Computed "obligation sets" — for any base class, what pure virtuals
   must a concrete subclass implement?

This prevents the entire class of linker errors caused by missing
pure virtual overrides (e.g., the OnClose() bug in FAssetEditorToolkit).

Output: virtual_obligations.json — loaded by KAIN codegen to auto-generate
required method stubs when subclassing engine types.

Usage:
  python virtual_obligations_extractor.py <UE_SOURCE_DIR> [--focus-classes class1,class2,...]

Example:
  python virtual_obligations_extractor.py "D:/Unreal/UE_5.7/Engine/Source"
"""

import os
import re
import json
import sys
import argparse
from collections import defaultdict
from pathlib import Path


# ═══════════════════════════════════════════════════════════════════
# Regex patterns for C++ header parsing
# ═══════════════════════════════════════════════════════════════════

# Match class/struct declarations with optional API macro and inheritance
# Handles: class EDITOR_API FMyClass : public FBase, public IInterface {
CLASS_PATTERN = re.compile(
    r'(?:class|struct)\s+(?:[\w_]*?API\s+)?(\w+)\s*'
    r'(?:final\s*)?'
    r'(?::\s*((?:public|private|protected)\s+[\w:<>,\s]+(?:,\s*(?:public|private|protected)\s+[\w:<>,\s]+)*))?'
    r'\s*\{',
    re.DOTALL
)

# Match pure virtual method declarations
# Handles various forms:
#   virtual void OnClose() = 0;
#   virtual FName GetToolkitFName() const override = 0;
#   virtual TSharedRef<SWidget> GetWidget() = 0;
#   virtual const FString& GetName() const = 0;
PURE_VIRTUAL_PATTERN = re.compile(
    r'virtual\s+'                           # 'virtual' keyword
    r'([\w:<>&*\s,]+?)\s+'                  # return type (greedy but stops at name)
    r'(\w+)\s*'                             # method name
    r'\(([^)]*)\)\s*'                       # parameters
    r'(const)?\s*'                          # optional const
    r'(?:override\s*)?'                     # optional override
    r'(?:PURE_VIRTUAL\s*\([^)]*\)|=\s*0)\s*;'  # = 0 or PURE_VIRTUAL(...)
)

# Match UE5's PURE_VIRTUAL macro (used instead of = 0 in some cases)
# PURE_VIRTUAL(FMyClass::MyMethod, return;)
PURE_VIRTUAL_MACRO_PATTERN = re.compile(
    r'virtual\s+'
    r'([\w:<>&*\s,]+?)\s+'
    r'(\w+)\s*'
    r'\(([^)]*)\)\s*'
    r'(const)?\s*'
    r'(?:override\s*)?'
    r'PURE_VIRTUAL\s*\('
)

# Match non-pure virtual methods (these override/implement pure virtuals)
VIRTUAL_METHOD_PATTERN = re.compile(
    r'virtual\s+'
    r'([\w:<>&*\s,]+?)\s+'
    r'(\w+)\s*'
    r'\(([^)]*)\)\s*'
    r'(const)?\s*'
    r'(?:override)?\s*'
    r'(?:;|\{|//)'  # ends with ; or { or comment
)


def clean_type(t):
    """Clean a C++ type string."""
    t = re.sub(r'\b[\w_]*?API\b', '', t).strip()
    t = re.sub(r'\s+', ' ', t).strip()
    return t


def clean_params(raw):
    """Clean parameter string, preserving types and names."""
    raw = raw.replace('\n', ' ').replace('\t', ' ').strip()
    raw = re.sub(r'\s+', ' ', raw)
    return raw


def parse_params(raw_params):
    """Parse parameter string into list of {name, type, default_value?}."""
    raw = raw_params.replace('\n', ' ').strip()
    if not raw:
        return []
    params = []
    depth = 0
    current = ""
    for ch in raw:
        if ch in '(<':
            depth += 1
            current += ch
        elif ch in ')>':
            depth -= 1
            current += ch
        elif ch == ',' and depth == 0:
            params.append(current.strip())
            current = ""
        else:
            current += ch
    if current.strip():
        params.append(current.strip())

    result = []
    for p in params:
        p = p.strip()
        if not p:
            continue
        default_value = None
        if '=' in p:
            # Handle default values, but be careful with templates like TArray<T>=
            eq_idx = p.find('=')
            # Make sure it's not inside angle brackets
            depth = 0
            real_eq = -1
            for i, ch in enumerate(p):
                if ch in '<(': depth += 1
                elif ch in '>(': depth -= 1
                elif ch == '=' and depth == 0:
                    real_eq = i
                    break
            if real_eq >= 0:
                default_value = p[real_eq+1:].strip()
                p = p[:real_eq].strip()

        tokens = p.split()
        if len(tokens) >= 2:
            p_name = tokens[-1].strip('*&')
            p_type = clean_type(' '.join(tokens[:-1]))
            entry = {"name": p_name, "type": p_type}
            if default_value:
                entry["default_value"] = default_value
            result.append(entry)
        elif len(tokens) == 1:
            # Just a type, no name
            result.append({"name": "", "type": clean_type(tokens[0])})
    return result


def extract_parents(parent_str):
    """Extract parent class names from inheritance declaration."""
    if not parent_str:
        return []
    parents = []
    # Split by comma, handling templates
    depth = 0
    current = ""
    for ch in parent_str:
        if ch in '<(':
            depth += 1
            current += ch
        elif ch in '>)':
            depth -= 1
            current += ch
        elif ch == ',' and depth == 0:
            parents.append(current.strip())
            current = ""
        else:
            current += ch
    if current.strip():
        parents.append(current.strip())

    result = []
    for p in parents:
        # Remove access specifier
        p = re.sub(r'^(public|private|protected)\s+', '', p.strip())
        # Remove template args
        p = re.sub(r'<.*>', '', p).strip()
        if p and not p.startswith('//'):
            result.append(p)
    return result


def compute_header_path(file_path, root_dir):
    """Compute UE5-style include path relative to Public/ or Classes/."""
    rel = os.path.relpath(file_path, root_dir).replace('\\', '/')
    for marker in ('Public/', 'Classes/'):
        idx = rel.find(marker)
        if idx != -1:
            return rel[idx + len(marker):]
    return os.path.basename(file_path)


def guess_module(file_path):
    """Guess the UE5 module from the file path."""
    parts = Path(file_path).parts
    for i, part in enumerate(parts):
        if part in ('Public', 'Private', 'Classes'):
            if i > 0:
                return parts[i - 1]
    return ""


def guess_category(file_path):
    """Guess module category from path."""
    path_str = str(file_path).replace('\\', '/')
    if '/Editor/' in path_str:
        return "Editor"
    elif '/Developer/' in path_str:
        return "Developer"
    elif '/Runtime/' in path_str:
        return "Runtime"
    elif '/ThirdParty/' in path_str or '/ThirdParty/' in path_str:
        return "ThirdParty"
    return "Unknown"


def find_class_body(content, class_end_pos):
    """Find the body of a class starting from the opening brace."""
    depth = 1
    pos = class_end_pos
    while pos < len(content) and depth > 0:
        if content[pos] == '{':
            depth += 1
        elif content[pos] == '}':
            depth -= 1
        pos += 1
    return content[class_end_pos:pos - 1]


# ═══════════════════════════════════════════════════════════════════
# Main extraction
# ═══════════════════════════════════════════════════════════════════

def scan_file(file_path, root_dir):
    """Scan a single header file for classes and their pure virtual methods."""
    with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()

    header_rel = compute_header_path(file_path, root_dir)
    module = guess_module(file_path)
    category = guess_category(file_path)

    classes = []

    for match in CLASS_PATTERN.finditer(content):
        cls_name = match.group(1)
        parent_str = match.group(2) or ""
        parents = extract_parents(parent_str)

        # Find class body
        body = find_class_body(content, match.end())

        # Extract pure virtual methods (= 0)
        pure_virtuals = []
        for pv in PURE_VIRTUAL_PATTERN.finditer(body):
            ret_type = clean_type(pv.group(1))
            method_name = pv.group(2)
            raw_params = pv.group(3)
            is_const = pv.group(4) is not None
            params = parse_params(raw_params)

            pure_virtuals.append({
                "name": method_name,
                "return_type": ret_type,
                "params": params,
                "is_const": is_const,
                "raw_signature": f"virtual {ret_type} {method_name}({clean_params(raw_params)}){' const' if is_const else ''} = 0;",
            })

        # Also check for PURE_VIRTUAL macro
        for pv in PURE_VIRTUAL_MACRO_PATTERN.finditer(body):
            ret_type = clean_type(pv.group(1))
            method_name = pv.group(2)
            raw_params = pv.group(3)
            is_const = pv.group(4) is not None
            params = parse_params(raw_params)

            # Avoid duplicates
            if not any(p["name"] == method_name for p in pure_virtuals):
                pure_virtuals.append({
                    "name": method_name,
                    "return_type": ret_type,
                    "params": params,
                    "is_const": is_const,
                    "raw_signature": f"virtual {ret_type} {method_name}({clean_params(raw_params)}){' const' if is_const else ''} PURE_VIRTUAL;",
                })

        # Extract implemented virtual methods (to know what's already overridden)
        implemented_virtuals = set()
        for vm in VIRTUAL_METHOD_PATTERN.finditer(body):
            method_name = vm.group(2)
            # Only count if NOT pure virtual
            if method_name not in {pv["name"] for pv in pure_virtuals}:
                implemented_virtuals.add(method_name)

        classes.append({
            "name": cls_name,
            "parents": parents,
            "header": header_rel,
            "module": module,
            "category": category,
            "pure_virtuals": pure_virtuals,
            "implemented_virtuals": list(implemented_virtuals),
        })

    return classes


def scan_directory(root_dir):
    """Scan all headers in a directory tree."""
    all_classes = []
    file_count = 0
    error_count = 0

    for root, dirs, files in os.walk(root_dir):
        # Skip ThirdParty to avoid noise
        dirs[:] = [d for d in dirs if d not in ('ThirdParty', 'Intermediate', 'Binaries')]
        for f in files:
            if not f.endswith('.h'):
                continue
            path = os.path.join(root, f)
            try:
                classes = scan_file(path, root_dir)
                all_classes.extend(classes)
                file_count += 1
            except Exception as ex:
                error_count += 1

    return all_classes, file_count, error_count


def compute_obligations(all_classes):
    """
    Compute the obligation set for each class:
    For any class C, what pure virtual methods must a concrete subclass implement?

    This walks the inheritance chain:
    1. Collect all pure virtuals declared by C and its ancestors
    2. Subtract any that are implemented (non-pure virtual override) by C or ancestors
    3. The remainder is the obligation set
    """
    # Build lookup maps
    class_map = {}
    for cls in all_classes:
        name = cls["name"]
        # Keep the version with more pure virtuals if duplicates
        if name in class_map:
            existing = class_map[name]
            if len(cls["pure_virtuals"]) > len(existing["pure_virtuals"]):
                class_map[name] = cls
        else:
            class_map[name] = cls

    # For each class, compute full obligation set
    obligations = {}

    def get_all_pure_virtuals(cls_name, visited=None):
        """Recursively collect all pure virtuals from class and ancestors."""
        if visited is None:
            visited = set()
        if cls_name in visited:
            return {}, set()
        visited.add(cls_name)

        cls = class_map.get(cls_name)
        if not cls:
            return {}, set()

        # Start with this class's pure virtuals
        pvs = {}
        for pv in cls["pure_virtuals"]:
            key = pv["name"]  # Use method name as key
            pvs[key] = {
                "name": pv["name"],
                "return_type": pv["return_type"],
                "params": pv["params"],
                "is_const": pv["is_const"],
                "declared_in": cls_name,
                "raw_signature": pv.get("raw_signature", ""),
            }

        # Collect implemented (non-pure) virtuals
        implemented = set(cls.get("implemented_virtuals", []))

        # Walk parents
        for parent in cls.get("parents", []):
            parent_pvs, parent_impl = get_all_pure_virtuals(parent, visited)
            # Add parent's pure virtuals (if not already declared at this level)
            for key, pv in parent_pvs.items():
                if key not in pvs:
                    pvs[key] = pv
            implemented |= parent_impl

        return pvs, implemented

    for cls_name, cls in class_map.items():
        all_pvs, all_impl = get_all_pure_virtuals(cls_name)

        # Obligations = pure virtuals NOT implemented by this class or ancestors
        remaining = {}
        for key, pv in all_pvs.items():
            if key not in all_impl:
                remaining[key] = pv

        if remaining:
            obligations[cls_name] = {
                "class": cls_name,
                "parents": cls.get("parents", []),
                "header": cls.get("header", ""),
                "module": cls.get("module", ""),
                "category": cls.get("category", ""),
                "obligations": list(remaining.values()),
                "obligation_count": len(remaining),
            }

    return obligations, class_map


def build_default_stubs(obligations):
    """
    For each obligation, generate a sensible default C++ stub body.
    This is what codegen should emit when a KAIN class subclasses an engine type.
    """
    for cls_name, info in obligations.items():
        for ob in info["obligations"]:
            ret = ob["return_type"]
            name = ob["name"]
            is_const = ob["is_const"]

            # Generate default return based on type
            if ret == "void":
                ob["default_body"] = "{ }"
            elif ret == "bool":
                ob["default_body"] = "{ return false; }"
            elif ret in ("int", "int32", "int64", "uint32", "uint64", "float", "double"):
                ob["default_body"] = "{ return 0; }"
            elif ret == "FName":
                ob["default_body"] = '{ return FName(); }'
            elif ret == "FString":
                ob["default_body"] = '{ return FString(); }'
            elif ret == "FText":
                ob["default_body"] = '{ return FText::GetEmpty(); }'
            elif ret == "FLinearColor":
                ob["default_body"] = '{ return FLinearColor::White; }'
            elif ret.startswith("TSharedRef"):
                ob["default_body"] = "{ return SNullWidget::NullWidget; }"
            elif ret.startswith("TSharedPtr"):
                ob["default_body"] = "{ return nullptr; }"
            elif ret.endswith('*'):
                ob["default_body"] = "{ return nullptr; }"
            elif ret.endswith('&'):
                # Reference return — tricky, often needs a static local
                ob["default_body"] = f"{{ static {ret.rstrip('& ')} Default; return Default; }}"
            else:
                ob["default_body"] = f"{{ return {ret}(); }}"

            # Generate the full override signature
            param_str = ", ".join(
                f"{p['type']} {p['name']}" for p in ob["params"]
            )
            const_str = " const" if is_const else ""
            ob["override_declaration"] = f"virtual {ret} {name}({param_str}){const_str} override"
            ob["override_definition"] = f"{ret} {{CLASS}}::{name}({param_str}){const_str}\n{ob['default_body']}"


def main():
    parser = argparse.ArgumentParser(
        description="Extract pure virtual method obligations from UE5 headers"
    )
    parser.add_argument("source_dir", help="UE5 Engine/Source directory")
    parser.add_argument("--focus-classes", default="",
                        help="Comma-separated list of classes to highlight in output")
    parser.add_argument("--output", default=None,
                        help="Output JSON path (default: unreal/metadata/virtual_obligations.json)")
    args = parser.parse_args()

    source_dir = args.source_dir
    focus_classes = [c.strip() for c in args.focus_classes.split(",") if c.strip()] if args.focus_classes else []

    output_path = args.output or os.path.join(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
        "metadata", "virtual_obligations.json"
    )

    print(f"🔍 Scanning C++ headers in: {source_dir}")
    print()

    # Pass 1: Scan all headers
    print("📦 Pass 1: Scanning headers for classes and pure virtual methods...")
    all_classes, file_count, error_count = scan_directory(source_dir)

    classes_with_pvs = [c for c in all_classes if c["pure_virtuals"]]
    total_pvs = sum(len(c["pure_virtuals"]) for c in classes_with_pvs)
    print(f"   ✓ Scanned {file_count} headers ({error_count} errors)")
    print(f"   ✓ {len(all_classes)} classes/structs found")
    print(f"   ✓ {len(classes_with_pvs)} classes with pure virtual methods")
    print(f"   ✓ {total_pvs} total pure virtual declarations")

    # Pass 2: Compute obligation sets
    print()
    print("🔗 Pass 2: Computing obligation sets via inheritance chains...")
    obligations, class_map = compute_obligations(all_classes)
    print(f"   ✓ {len(obligations)} classes have unresolved obligations")

    # Pass 3: Generate default stubs
    print()
    print("🔧 Pass 3: Generating default stub implementations...")
    build_default_stubs(obligations)
    print(f"   ✓ Default stubs generated for all obligations")

    # ─── Focus classes report ───────────────────────────────────
    # Key classes that KAIN codegen commonly subclasses
    kain_relevant = [
        "FAssetEditorToolkit", "IDetailCustomization", "FEditorViewportClient",
        "IToolkitHost", "SCompoundWidget", "FGCObject", "FTickableGameObject",
        "IModuleInterface", "FGlobalShader", "IAssetEditorInstance",
        "FEdMode", "FEditorUndoClient", "FNotifyHook",
        "IPropertyTypeCustomization", "FTickableEditorObject",
        "SLeafWidget", "SPanel", "IPlugin",
    ]

    if focus_classes:
        kain_relevant = focus_classes + [c for c in kain_relevant if c not in focus_classes]

    print()
    print("🎯 Key classes for KAIN codegen:")
    for cls_name in kain_relevant:
        if cls_name in obligations:
            info = obligations[cls_name]
            ob_names = [ob["name"] for ob in info["obligations"]]
            print(f"   {cls_name}: {info['obligation_count']} obligations → {', '.join(ob_names[:5])}")
            if len(ob_names) > 5:
                print(f"      ... and {len(ob_names) - 5} more")
        elif cls_name in class_map:
            print(f"   {cls_name}: 0 obligations (all implemented)")
        # else: not found in scan

    # ─── Build output JSON ──────────────────────────────────────
    # kain_focus: full detail for key classes (with stubs)
    kain_focus = {}
    for cls_name in kain_relevant:
        if cls_name in obligations:
            kain_focus[cls_name] = obligations[cls_name]

    # all_obligations: compact format — only classes with ≤15 obligations
    # (classes with more are internal engine interfaces, never subclassed by users)
    # Strip verbose fields to keep size manageable
    compact_obligations = {}
    for cls_name, info in obligations.items():
        if info["obligation_count"] > 10:
            continue
        compact_obs = []
        for ob in info["obligations"]:
            compact_ob = {
                "name": ob["name"],
                "return_type": ob["return_type"],
                "params": ob["params"],
                "is_const": ob["is_const"],
                "declared_in": ob["declared_in"],
            }
            # Include default_body for codegen
            if "default_body" in ob:
                compact_ob["default_body"] = ob["default_body"]
            compact_obs.append(compact_ob)
        compact_obligations[cls_name] = {
            "parents": info["parents"],
            "header": info["header"],
            "module": info["module"],
            "category": info["category"],
            "obligation_count": info["obligation_count"],
            "obligations": compact_obs,
        }

    output = {
        "_meta": {
            "generator": "virtual_obligations_extractor.py",
            "source": source_dir,
            "total_classes_scanned": len(all_classes),
            "total_pure_virtual_declarations": total_pvs,
            "total_classes_with_obligations": len(obligations),
            "compact_classes_included": len(compact_obligations),
            "kain_focus_classes": len(kain_focus),
        },
        "kain_focus": kain_focus,
        "obligations": compact_obligations,
    }

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, 'w') as f:
        json.dump(output, f, indent=2)

    size_kb = os.path.getsize(output_path) / 1024
    print()
    print(f"✅ Written: {output_path}")
    print(f"   Size: {size_kb:.1f} KB")

    # Summary stats
    print()
    print("📊 Summary:")
    print(f"   Total classes scanned: {len(all_classes)}")
    print(f"   Classes with pure virtuals: {len(classes_with_pvs)}")
    print(f"   Classes with unresolved obligations: {len(obligations)}")
    print(f"   KAIN-relevant classes tracked: {len(kain_focus)}")

    # Top 10 by obligation count
    print()
    print("📋 Top 10 classes by obligation count:")
    sorted_obs = sorted(obligations.values(), key=lambda x: -x["obligation_count"])
    for info in sorted_obs[:10]:
        print(f"   {info['obligation_count']:3d} obligations: {info['class']}")


if __name__ == "__main__":
    main()
