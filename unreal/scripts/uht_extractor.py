#!/usr/bin/env python3
"""
UHT (Unreal Header Tool) Validation Rule Extractor
Scans Epic's EpicGames.UHT C# source to extract:
  1. All LogError/LogWarning validation rules with context
  2. All specifier definitions (UCLASS, USTRUCT, UENUM, UPROPERTY, UFUNCTION)
  3. Property type definitions and constraints
  4. Specifier compatibility rules (what can combine with what)

Output: uht_rules.json — loaded by KAIN oracle for pre-validation
"""

import os
import re
import json
import sys
from collections import defaultdict

# ============================================================================
# Pass 1: Extract all LogError / LogWarning calls with surrounding context
# ============================================================================

def extract_validation_rules(uht_dir):
    """Extract every LogError/LogWarning call from UHT source with context."""
    rules = []
    
    # Pattern to match LogError/LogWarning calls
    # Handles both simple string and interpolated string patterns
    log_pattern = re.compile(
        r'(?:MessageSite|specifierContext\.MessageSite|context\.MessageSite|'
        r'tokenReader|type|function|classObj|this|outerClass|'
        r'specifierContext\.Type|(?:\w+))\.Log(Error|Warning)\s*\(\s*'
        r'(?:'
        r'\$"([^"]*)"'          # Interpolated string $"..."
        r'|"([^"]*)"'           # Plain string "..."
        r'|`([^`]*)`'           # Verbatim string
        r')',
        re.MULTILINE
    )
    
    # Broader pattern to also catch LogError in different formats
    broad_pattern = re.compile(
        r'\.Log(Error|Warning)\s*\(\s*(?:\$"([^"]+)"|"([^"]+)")',
        re.MULTILINE
    )
    
    for root, dirs, files in os.walk(uht_dir):
        for fname in sorted(files):
            if not fname.endswith('.cs'):
                continue
            fpath = os.path.join(root, fname)
            rel_path = os.path.relpath(fpath, uht_dir)
            
            with open(fpath, 'r', encoding='utf-8-sig', errors='replace') as f:
                content = f.read()
                lines = content.split('\n')
            
            for match in broad_pattern.finditer(content):
                severity = match.group(1).lower()  # "error" or "warning"
                message = match.group(2) or match.group(3) or ""
                
                # Get line number
                line_num = content[:match.start()].count('\n') + 1
                
                # Get surrounding context (function name, class)
                context = extract_context(lines, line_num - 1)
                
                # Determine category from file path
                category = categorize_rule(rel_path, context)
                
                # Clean up interpolated string placeholders
                message_clean = re.sub(r'\{[^}]+\}', '{...}', message)
                
                rules.append({
                    "severity": severity,
                    "message": message_clean,
                    "message_raw": message,
                    "source_file": rel_path.replace('\\', '/'),
                    "line": line_num,
                    "context": context,
                    "category": category,
                })
    
    # Deduplicate by message
    seen = set()
    unique_rules = []
    for r in rules:
        key = r["message_clean"] if "message_clean" in r else r["message"]
        if key not in seen:
            seen.add(key)
            unique_rules.append(r)
    
    return unique_rules


def extract_context(lines, line_idx):
    """Walk backwards from a LogError line to find the enclosing method/class."""
    context = {
        "method": "",
        "class": "",
        "specifier_type": "",  # class, function, property, struct, enum
    }
    
    # Walk backwards to find the method signature
    for i in range(line_idx, max(line_idx - 50, -1), -1):
        line = lines[i].strip()
        
        # Match method signature
        method_match = re.match(
            r'(?:private|public|internal)?\s*static\s+void\s+(\w+)\s*\(',
            line
        )
        if method_match:
            context["method"] = method_match.group(1)
            break
        
        # Also match validator methods
        validator_match = re.match(
            r'(?:private|public|internal)?\s*static\s+void\s+(\w+Validator)\s*\(',
            line
        )
        if validator_match:
            context["method"] = validator_match.group(1)
            break
    
    # Walk backwards further to find the class
    for i in range(line_idx, max(line_idx - 200, -1), -1):
        line = lines[i].strip()
        class_match = re.match(r'public\s+static\s+class\s+(\w+)', line)
        if class_match:
            context["class"] = class_match.group(1)
            break
    
    # Infer specifier type from class name
    cls = context["class"]
    if "Class" in cls and "Interface" not in cls:
        context["specifier_type"] = "class"
    elif "Function" in cls:
        context["specifier_type"] = "function"
    elif "Property" in cls:
        context["specifier_type"] = "property"
    elif "ScriptStruct" in cls or "Struct" in cls:
        context["specifier_type"] = "struct"
    elif "Enum" in cls:
        context["specifier_type"] = "enum"
    elif "Interface" in cls:
        context["specifier_type"] = "interface"
    
    return context


def categorize_rule(rel_path, context):
    """Categorize a validation rule based on its source file and context."""
    path_lower = rel_path.lower()
    
    if 'specifier' in path_lower:
        if 'class' in path_lower:
            return "class_specifier"
        elif 'function' in path_lower:
            return "function_specifier"
        elif 'property' in path_lower:
            return "property_specifier"
        elif 'struct' in path_lower:
            return "struct_specifier"
        elif 'enum' in path_lower:
            return "enum_specifier"
        elif 'interface' in path_lower:
            return "interface_specifier"
        elif 'field' in path_lower:
            return "field_specifier"
        else:
            return "specifier"
    elif 'parser' in path_lower:
        if 'class' in path_lower:
            return "class_parser"
        elif 'function' in path_lower:
            return "function_parser"
        elif 'property' in path_lower:
            return "property_parser"
        elif 'struct' in path_lower:
            return "struct_parser"
        elif 'enum' in path_lower:
            return "enum_parser"
        elif 'header' in path_lower:
            return "header_parser"
        else:
            return "parser"
    elif 'properties' in path_lower:
        return "property_type"
    elif 'types' in path_lower:
        return "type_definition"
    elif 'export' in path_lower:
        return "exporter"
    else:
        return "other"


# ============================================================================
# Pass 2: Extract all specifier definitions
# ============================================================================

def extract_specifiers(uht_dir):
    """Extract all [UhtSpecifier] attribute definitions."""
    specifiers = []
    
    spec_dir = os.path.join(uht_dir, "Specifiers")
    if not os.path.isdir(spec_dir):
        print(f"  Warning: Specifiers directory not found at {spec_dir}")
        return specifiers
    
    # Pattern for [UhtSpecifier(Extends = ..., ValueType = ...)]
    spec_pattern = re.compile(
        r'\[UhtSpecifier\s*\('
        r'(?:.*?Extends\s*=\s*UhtTableNames\.(\w+))?'
        r'(?:.*?ValueType\s*=\s*UhtSpecifierValueType\.(\w+))?'
        r'(?:.*?Extends\s*=\s*UhtTableNames\.(\w+))?'  # Extends might come after ValueType
        r'.*?\)\]'
        r'\s*(?:\[.*?\]\s*)*'  # Skip any additional attributes
        r'private\s+static\s+void\s+(\w+)\s*\(',
        re.DOTALL
    )
    
    # Simpler approach: parse line by line
    for fname in sorted(os.listdir(spec_dir)):
        if not fname.endswith('.cs'):
            continue
        fpath = os.path.join(spec_dir, fname)
        
        with open(fpath, 'r', encoding='utf-8-sig', errors='replace') as f:
            content = f.read()
            lines = content.split('\n')
        
        i = 0
        while i < len(lines):
            line = lines[i].strip()
            
            # Look for [UhtSpecifier(...)
            if '[UhtSpecifier(' in line:
                # Collect the full attribute + method signature
                block = line
                j = i + 1
                while j < len(lines) and 'private static void' not in block:
                    block += ' ' + lines[j].strip()
                    j += 1
                
                # Extract extends
                extends_match = re.search(r'Extends\s*=\s*UhtTableNames\.(\w+)', block)
                # Extract value type
                value_match = re.search(r'ValueType\s*=\s*UhtSpecifierValueType\.(\w+)', block)
                # Extract method name (specifier name)
                method_match = re.search(r'private\s+static\s+void\s+(\w+)\s*\(', block)
                # Extract Name override if present
                name_match = re.search(r'Name\s*=\s*"(\w+)"', block)
                
                if method_match:
                    method_name = method_match.group(1)
                    # Convert "BlueprintCallableSpecifier" -> "BlueprintCallable"
                    spec_name = name_match.group(1) if name_match else method_name.replace('Specifier', '').replace('Validator', '')
                    
                    extends = extends_match.group(1) if extends_match else "Unknown"
                    value_type = value_match.group(1) if value_match else "Legacy"
                    
                    # Map extends to KAIN-friendly category
                    category_map = {
                        "Class": "class",
                        "Function": "function",
                        "PropertyMember": "property",
                        "PropertyArgument": "function_param",
                        "ScriptStruct": "struct",
                        "Enum": "enum",
                        "Interface": "interface",
                    }
                    
                    specifiers.append({
                        "name": spec_name,
                        "applies_to": category_map.get(extends, extends.lower()),
                        "value_type": value_type,
                        "source_file": fname,
                    })
                
                i = j
            else:
                i += 1
    
    return specifiers


# ============================================================================
# Pass 3: Extract property type definitions and constraints
# ============================================================================

def extract_property_types(uht_dir):
    """Extract UHT property type definitions and their constraints."""
    property_types = []
    
    props_dir = os.path.join(uht_dir, "Types", "Properties")
    if not os.path.isdir(props_dir):
        print(f"  Warning: Properties directory not found at {props_dir}")
        return property_types
    
    for fname in sorted(os.listdir(props_dir)):
        if not fname.endswith('.cs'):
            continue
        fpath = os.path.join(props_dir, fname)
        
        with open(fpath, 'r', encoding='utf-8-sig', errors='replace') as f:
            content = f.read()
        
        # Extract class name
        class_match = re.search(r'class\s+(Uht\w+Property)\s*:', content)
        if not class_match:
            continue
        
        class_name = class_match.group(1)
        
        # Extract the C++ type name from EngineClassName or similar
        engine_name_match = re.search(r'EngineClassName\s*(?:=>|=)\s*"(\w+)"', content)
        cpp_type_match = re.search(r'override\s+string\s+EngineClassName\s*(?:=>|{[^}]*return\s*"(\w+)")', content)
        
        engine_name = ""
        if engine_name_match:
            engine_name = engine_name_match.group(1)
        elif cpp_type_match:
            engine_name = cpp_type_match.group(1)
        
        # Check for container constraints
        is_container = 'ContainerProperty' in content or 'UhtContainerProperty' in content
        
        # Check for specific constraints via LogError in this file
        constraints = []
        for err_match in re.finditer(r'\.Log(?:Error|Warning)\s*\(\s*(?:\$"([^"]+)"|"([^"]+)")', content):
            msg = err_match.group(1) or err_match.group(2)
            msg_clean = re.sub(r'\{[^}]+\}', '{...}', msg)
            constraints.append(msg_clean)
        
        # Check if blueprint-compatible
        bp_compatible = 'BlueprintVisible' in content or 'BlueprintReadWrite' in content
        
        # Simple name mapping
        type_name = class_name.replace('Uht', '').replace('Property', '')
        
        property_types.append({
            "uht_class": class_name,
            "type_name": type_name,
            "engine_class_name": engine_name,
            "is_container": is_container,
            "constraints": constraints,
            "source_file": fname,
        })
    
    return property_types


# ============================================================================
# Pass 4: Extract incompatible specifier combinations
# ============================================================================

def extract_incompatible_combos(rules):
    """
    Analyze validation rules to extract incompatible specifier combinations.
    These are rules like "Cannot specify both X and Y" or "X cannot be used with Y".
    """
    combos = []
    
    # Patterns for incompatibility messages
    patterns = [
        re.compile(r'[Cc]annot.*both\s+(\w+)\s+and\s+(\w+)', re.IGNORECASE),
        re.compile(r'(\w+)\s+(?:cannot|can not|should not)\s+be\s+(?:used with|combined with|declared as|a)\s+(\w+)', re.IGNORECASE),
        re.compile(r'(\w+)\s+functions\s+cannot\s+be\s+(\w+)', re.IGNORECASE),
        re.compile(r'[Aa]\s+(\w+)\s+function\s+cannot\s+be\s+a\s+(\w+)', re.IGNORECASE),
        re.compile(r'Found more than one\s+(\w[\w\s/]+)\s+specifier.*only one', re.IGNORECASE),
    ]
    
    for rule in rules:
        msg = rule.get("message_raw", rule.get("message", ""))
        
        for pat in patterns:
            m = pat.search(msg)
            if m:
                groups = m.groups()
                if len(groups) >= 2:
                    combos.append({
                        "specifier_a": groups[0],
                        "specifier_b": groups[1],
                        "message": rule["message"],
                        "category": rule["category"],
                        "severity": rule["severity"],
                    })
                elif len(groups) == 1:
                    combos.append({
                        "specifier_a": groups[0],
                        "specifier_b": None,
                        "message": rule["message"],
                        "category": rule["category"],
                        "severity": rule["severity"],
                        "constraint": "only_one_allowed",
                    })
                break
    
    return combos


# ============================================================================
# Pass 5: Extract KAIN-relevant validation rules
# ============================================================================

def extract_kain_relevant_rules(rules):
    """
    Filter and categorize rules into ones that KAIN can actually check.
    Returns rules grouped by what KAIN construct triggers them.
    """
    kain_rules = {
        "actor": [],         # KAIN actor → UCLASS
        "struct": [],        # KAIN struct → USTRUCT  
        "enum": [],          # KAIN enum → UENUM
        "function": [],      # KAIN fn → UFUNCTION
        "property": [],      # KAIN state/field → UPROPERTY
        "delegate": [],      # KAIN delegate → delegate macros
        "replication": [],   # @replicated → network rules
        "blueprint": [],     # @blueprint_callable etc
        "general": [],       # Cross-cutting rules
    }
    
    # Keywords that map to KAIN constructs
    keyword_mapping = {
        "actor": ["UCLASS", "class ", "actor", "Actor", "AActor"],
        "struct": ["USTRUCT", "struct", "ScriptStruct", "FTableRowBase"],
        "enum": ["UENUM", "enum", "Enum"],
        "function": ["UFUNCTION", "function", "BlueprintCallable", "BlueprintPure", 
                      "BlueprintNativeEvent", "BlueprintImplementableEvent"],
        "property": ["UPROPERTY", "property", "EditAnywhere", "BlueprintReadWrite",
                      "VisibleAnywhere", "EditDefaultsOnly"],
        "replication": ["Replicated", "replicated", "RepNotify", "Net ", "NetMulticast",
                        "Server", "Client", "NetReliable"],
        "blueprint": ["Blueprint", "blueprint"],
    }
    
    for rule in rules:
        msg = rule.get("message", "")
        category = rule.get("category", "")
        ctx_type = rule.get("context", {}).get("specifier_type", "")
        
        placed = False
        
        # First try context-based placement
        if ctx_type in kain_rules:
            kain_rules[ctx_type].append(rule)
            placed = True
        
        # Then try keyword-based
        if not placed:
            for kain_cat, keywords in keyword_mapping.items():
                if any(kw in msg or kw in category for kw in keywords):
                    kain_rules[kain_cat].append(rule)
                    placed = True
                    break
        
        if not placed:
            kain_rules["general"].append(rule)
    
    return kain_rules


# ============================================================================
# Main
# ============================================================================

def main():
    if len(sys.argv) < 2:
        print("Usage: uht_extractor.py <UHT_SOURCE_DIR> [--output <dir>]")
        print("  UHT_SOURCE_DIR: Path to EpicGames.UHT directory")
        print("  Example: uht_extractor.py D:\\Unreal\\UE_5.7\\Engine\\Source\\Programs\\Shared\\EpicGames.UHT")
        sys.exit(1)
    
    uht_dir = sys.argv[1]
    output_dir = "unreal/metadata"
    
    if "--output" in sys.argv:
        idx = sys.argv.index("--output")
        if idx + 1 < len(sys.argv):
            output_dir = sys.argv[idx + 1]
    
    if not os.path.isdir(uht_dir):
        print(f"Error: UHT directory not found: {uht_dir}")
        sys.exit(1)
    
    print(f"=== UHT Validation Rule Extractor ===")
    print(f"Source: {uht_dir}")
    print()
    
    # Pass 1: Extract all validation rules
    print("Pass 1: Extracting validation rules (LogError/LogWarning)...")
    all_rules = extract_validation_rules(uht_dir)
    errors = [r for r in all_rules if r["severity"] == "error"]
    warnings = [r for r in all_rules if r["severity"] == "warning"]
    print(f"  Found {len(errors)} error rules + {len(warnings)} warning rules = {len(all_rules)} total")
    
    # Count by category
    cat_counts = defaultdict(int)
    for r in all_rules:
        cat_counts[r["category"]] += 1
    print(f"  Categories: {dict(cat_counts)}")
    
    # Pass 2: Extract specifier definitions  
    print("\nPass 2: Extracting specifier definitions...")
    specifiers = extract_specifiers(uht_dir)
    print(f"  Found {len(specifiers)} specifier definitions")
    
    # Count by applies_to
    spec_counts = defaultdict(int)
    for s in specifiers:
        spec_counts[s["applies_to"]] += 1
    print(f"  By type: {dict(spec_counts)}")
    
    # Pass 3: Extract property type definitions
    print("\nPass 3: Extracting property type definitions...")
    property_types = extract_property_types(uht_dir)
    print(f"  Found {len(property_types)} property type definitions")
    for pt in property_types:
        if pt["constraints"]:
            print(f"    {pt['type_name']}: {len(pt['constraints'])} constraints")
    
    # Pass 4: Extract incompatible specifier combinations
    print("\nPass 4: Extracting incompatible specifier combinations...")
    incompatible = extract_incompatible_combos(all_rules)
    print(f"  Found {len(incompatible)} incompatible combinations")
    
    # Pass 5: Categorize for KAIN
    print("\nPass 5: Categorizing rules for KAIN oracle...")
    kain_rules = extract_kain_relevant_rules(all_rules)
    for cat, cat_rules in kain_rules.items():
        if cat_rules:
            print(f"  {cat}: {len(cat_rules)} rules")
    
    # Build output
    output = {
        "_meta": {
            "generator": "uht_extractor.py",
            "source": uht_dir,
            "total_rules": len(all_rules),
            "total_specifiers": len(specifiers),
            "total_property_types": len(property_types),
            "total_incompatible_combos": len(incompatible),
        },
        "validation_rules": all_rules,
        "specifiers": specifiers,
        "property_types": property_types,
        "incompatible_combos": incompatible,
        "kain_rules": {k: v for k, v in kain_rules.items() if v},
    }
    
    # Write output
    os.makedirs(output_dir, exist_ok=True)
    out_path = os.path.join(output_dir, "uht_rules.json")
    with open(out_path, 'w', encoding='utf-8') as f:
        json.dump(output, f, indent=2, ensure_ascii=False)
    
    file_size = os.path.getsize(out_path)
    print(f"\n=== Output ===")
    print(f"  Written to: {out_path}")
    print(f"  Size: {file_size / 1024:.1f} KB")
    print(f"\n=== Summary ===")
    print(f"  {len(all_rules)} validation rules ({len(errors)} errors, {len(warnings)} warnings)")
    print(f"  {len(specifiers)} specifier definitions")
    print(f"  {len(property_types)} property type definitions")  
    print(f"  {len(incompatible)} incompatible combinations")
    
    # Print some example KAIN-relevant rules
    print(f"\n=== Sample KAIN-Relevant Rules ===")
    for cat in ["property", "function", "replication", "struct"]:
        rules = kain_rules.get(cat, [])
        if rules:
            print(f"\n  [{cat}] ({len(rules)} rules)")
            for r in rules[:3]:
                print(f"    • {r['message']}")


if __name__ == "__main__":
    main()
