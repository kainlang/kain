"""
UE5 Engine Knowledge Scanner v2
Scans Unreal Engine headers and emits the rich EngineMetadata JSON format
consumed by KAIN's EngineKnowledge system.

Extracts: class hierarchy, includes, functions, properties, enums, structs,
          specifiers, modules, and type relationships.

Usage:
    python ue5_scanner.py <engine_source_path> <output_json>
    python ue5_scanner.py --legacy <engine_source_path> <output_json>  # old flat format
    python ue5_scanner.py --config <config_file>  # scan all configured installations
"""

import os
import re
import json
import sys
from pathlib import Path

class Ue5Scanner:
    def __init__(self):
        # UCLASS with optional API macro and inheritance
        self.class_pattern = re.compile(
            r'UCLASS\((.*?)\)\s+class\s+(?:[\w_]*?API\s+)?(\w+)\s*(?::\s*public\s+([\w:,\s]+?))?(?:\s*\{)',
            re.DOTALL
        )
        # USTRUCT
        self.struct_pattern = re.compile(
            r'USTRUCT\((.*?)\)\s+struct\s+(?:[\w_]*?API\s+)?(\w+)\s*(?::\s*public\s+([\w:,\s]+?))?(?:\s*\{)',
            re.DOTALL
        )
        # UENUM
        self.enum_pattern = re.compile(
            r'UENUM\((.*?)\)\s+enum\s+(?:class\s+)?(\w+)(?:\s*:\s*\w+)?\s*\{([^}]*)\}',
            re.DOTALL
        )
        # UFUNCTION
        self.func_pattern = re.compile(
            r'UFUNCTION\((.*?)\)\s+((?:virtual\s+)?(?:static\s+)?(?:[\w_]*?API\s+)?(?:const\s+)?([\w:<>&*\s]+?))\s+(\w+)\s*\((.*?)\)\s*(const)?',
            re.DOTALL
        )
        # UPROPERTY
        self.prop_pattern = re.compile(
            r'UPROPERTY\((.*?)\)\s+(?:[\w_]*?API\s+)?([\w:<>&*\s]+?)\s+(\w+)\s*(?:=\s*[^;]*)?\s*;',
            re.DOTALL
        )
        # #include detection
        self.include_pattern = re.compile(r'#include\s+"([^"]+)"')

    def clean_type(self, cpp_type):
        """Clean C++ type string, preserving it as-is (no KAIN mapping)."""
        cpp_type = re.sub(r'\b[\w_]*?API\b', '', cpp_type).strip()
        cpp_type = re.sub(r'\bTObjectPtr<([\w_]+)>', r'\1*', cpp_type)
        cpp_type = re.sub(r'\s+', ' ', cpp_type).strip()
        # Remove leading qualifiers for cleaner output but keep const/& for params
        return cpp_type

    def clean_return_type(self, full_decl):
        """Extract return type from a full function declaration prefix."""
        t = full_decl.strip()
        t = re.sub(r'\bvirtual\b', '', t).strip()
        t = re.sub(r'\bstatic\b', '', t).strip()
        t = re.sub(r'\b[\w_]*?API\b', '', t).strip()
        t = re.sub(r'\s+', ' ', t).strip()
        return t

    def parse_specifiers(self, meta_str):
        """Parse UCLASS/UFUNCTION/UPROPERTY specifier string into list."""
        if not meta_str or not meta_str.strip():
            return []
        specs = []
        depth = 0
        current = ""
        for ch in meta_str:
            if ch == '(':
                depth += 1
                current += ch
            elif ch == ')':
                depth -= 1
                current += ch
            elif ch == ',' and depth == 0:
                s = current.strip()
                if s:
                    specs.append(s)
                current = ""
            else:
                current += ch
        s = current.strip()
        if s:
            specs.append(s)
        return specs

    def extract_category(self, specifiers):
        """Extract Category value from specifier list."""
        for spec in specifiers:
            m = re.match(r'Category\s*=\s*"([^"]*)"', spec)
            if m:
                return m.group(1)
            m = re.match(r'Category\s*=\s*(\w+)', spec)
            if m:
                return m.group(1)
        return ""

    def parse_params(self, raw_params_str):
        """Parse function parameter string into list of {name, type, default_value?}."""
        raw = raw_params_str.replace('\n', ' ').strip()
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
                parts = p.split('=', 1)
                p = parts[0].strip()
                default_value = parts[1].strip()
            # Split type from name: last token is name
            tokens = p.split()
            if len(tokens) >= 2:
                p_name = tokens[-1].strip('*&')
                p_type = self.clean_type(' '.join(tokens[:-1]))
                # If name ended with * or &, it's part of the type
                if tokens[-1].startswith('*') or tokens[-1].startswith('&'):
                    p_type = self.clean_type(p)
                    p_name = ""
                entry = {"name": p_name, "type": p_type}
                if default_value:
                    entry["default_value"] = default_value
                result.append(entry)
        return result

    def guess_module(self, file_path, header_rel):
        """Guess the UE5 module from the file path."""
        parts = Path(file_path).parts
        for i, part in enumerate(parts):
            if part in ('Public', 'Private', 'Classes'):
                if i > 0:
                    return parts[i - 1]
        # Fallback: check header path
        if 'Niagara' in header_rel:
            return 'Niagara'
        if 'EnhancedInput' in header_rel:
            return 'EnhancedInput'
        return 'Engine'

    def compute_header_path(self, file_path, root_dir):
        """Compute the UE5-style include path relative to Public/ or Classes/."""
        rel = os.path.relpath(file_path, root_dir).replace('\\', '/')
        # Strip up to Public/ or Classes/
        for marker in ('Public/', 'Classes/'):
            idx = rel.find(marker)
            if idx != -1:
                return rel[idx + len(marker):]
        return os.path.basename(file_path)

    def scan_file(self, file_path, root_dir):
        """Scan a single header file and return classes, structs, enums."""
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()

        header_rel = self.compute_header_path(file_path, root_dir)
        module = self.guess_module(file_path, header_rel)

        classes = []
        structs = []
        enums = []

        # === CLASSES ===
        for match in self.class_pattern.finditer(content):
            meta = match.group(1).strip()
            name = match.group(2)
            parent_raw = match.group(3) or ""
            parent = parent_raw.split(',')[0].strip() if parent_raw else ""
            # Remove template params from parent
            parent = re.sub(r'<.*>', '', parent).strip()

            specifiers = self.parse_specifiers(meta)
            prefix = "A" if name.startswith("A") and len(name) > 1 and name[1].isupper() else "U"
            is_abstract = any("Abstract" in s for s in specifiers) or "ABSTRACT" in meta

            # Find class body for functions/properties
            class_start = match.end()
            funcs, props = self._extract_members(content, class_start)

            classes.append({
                "name": name,
                "parent": parent,
                "header": header_rel,
                "module": module,
                "prefix": prefix,
                "specifiers": specifiers,
                "functions": funcs,
                "properties": props,
                "is_abstract": is_abstract,
                "category": self.extract_category(specifiers),
            })

        # === STRUCTS ===
        for match in self.struct_pattern.finditer(content):
            meta = match.group(1).strip()
            name = match.group(2)
            parent_raw = match.group(3) or ""
            parent = parent_raw.split(',')[0].strip() if parent_raw else ""
            is_table_row = "FTableRowBase" in parent

            struct_start = match.end()
            _, fields = self._extract_members(content, struct_start)

            structs.append({
                "name": name,
                "header": header_rel,
                "module": module,
                "fields": fields,
                "is_table_row": is_table_row,
            })

        # === ENUMS ===
        for match in self.enum_pattern.finditer(content):
            meta = match.group(1).strip()
            name = match.group(2)
            body = match.group(3)
            is_flags = "Flags" in meta or "UMETA(Bitflags)" in body

            values = []
            for line in body.split('\n'):
                line = line.strip().rstrip(',')
                line = re.sub(r'//.*', '', line).strip()
                line = re.sub(r'UMETA\(.*?\)', '', line).strip()
                if not line or line.startswith('//') or '=' in line and line.split('=')[0].strip() == '':
                    continue
                val_name = line.split('=')[0].split('UMETA')[0].strip().rstrip(',')
                if val_name and not val_name.startswith('#') and val_name != '{' and val_name != '}':
                    values.append(val_name)

            # Filter out _MAX entries
            values = [v for v in values if not v.endswith('_MAX') and v]

            enums.append({
                "name": name,
                "header": header_rel,
                "module": module,
                "values": values,
                "is_flags": is_flags,
            })

        return classes, structs, enums

    def _extract_members(self, content, start_pos):
        """Extract UFUNCTION and UPROPERTY members from class body starting at start_pos."""
        # Find the matching closing brace
        depth = 1
        pos = start_pos
        while pos < len(content) and depth > 0:
            if content[pos] == '{':
                depth += 1
            elif content[pos] == '}':
                depth -= 1
            pos += 1
        body = content[start_pos:pos]

        funcs = []
        for match in self.func_pattern.finditer(body):
            meta = match.group(1).strip()
            full_decl = match.group(2)
            ret_type = self.clean_return_type(full_decl)
            func_name = match.group(4)
            raw_params = match.group(5)
            is_const = match.group(6) is not None
            is_virtual = 'virtual' in full_decl
            is_static = 'static' in full_decl

            specifiers = self.parse_specifiers(meta)
            params = self.parse_params(raw_params)

            funcs.append({
                "name": func_name,
                "return_type": ret_type,
                "params": params,
                "specifiers": specifiers,
                "is_const": is_const,
                "is_virtual": is_virtual,
                "is_static": is_static,
            })

        props = []
        for match in self.prop_pattern.finditer(body):
            meta = match.group(1).strip()
            prop_type = self.clean_type(match.group(2))
            prop_name = match.group(3)
            specifiers = self.parse_specifiers(meta)
            category = self.extract_category(specifiers)

            props.append({
                "name": prop_name,
                "type": prop_type,
                "specifiers": [s for s in specifiers if not s.startswith("Category")],
                "category": category,
            })

        return funcs, props

    def scan_directory(self, root_dir):
        """Scan all headers in a directory tree."""
        all_classes = []
        all_structs = []
        all_enums = []
        include_map = {}

        for root, dirs, files in os.walk(root_dir):
            for file in files:
                if not file.endswith('.h'):
                    continue
                path = os.path.join(root, file)
                try:
                    classes, structs, enums = self.scan_file(path, root_dir)
                    all_classes.extend(classes)
                    all_structs.extend(structs)
                    all_enums.extend(enums)

                    # Build include map from discovered types
                    header_rel = self.compute_header_path(path, root_dir)
                    for c in classes:
                        include_map[c["name"]] = header_rel
                    for s in structs:
                        include_map[s["name"]] = header_rel
                    for e in enums:
                        include_map[e["name"]] = header_rel
                except Exception as ex:
                    print(f"  Warning: Failed to scan {path}: {ex}")

        return all_classes, all_structs, all_enums, include_map

    def scan_to_legacy(self, root_dir):
        """Scan and emit legacy flat format for backward compat."""
        all_metadata = []
        for root, dirs, files in os.walk(root_dir):
            for file in files:
                if file.endswith('.h'):
                    path = os.path.join(root, file)
                    try:
                        with open(path, 'r', encoding='utf-8', errors='ignore') as f:
                            content = f.read()
                        results = {'classes': [], 'functions': [], 'properties': []}
                        for match in self.func_pattern.finditer(content):
                            ret_type = self.clean_return_type(match.group(2))
                            name = match.group(4)
                            params = self.parse_params(match.group(5))
                            results['functions'].append({
                                'name': name, 'return_type': ret_type,
                                'params': params, 'meta': match.group(1).strip()
                            })
                        for match in self.prop_pattern.finditer(content):
                            results['properties'].append({
                                'name': match.group(3),
                                'type': self.clean_type(match.group(2)),
                                'meta': match.group(1).strip()
                            })
                        if results['functions'] or results['classes']:
                            all_metadata.append({'file': file, 'content': results})
                    except Exception:
                        pass
        return all_metadata


def load_config(config_path):
    """Load UE5 installation paths from configuration file."""
    try:
        with open(config_path, 'r') as f:
            config = json.load(f)
        return config
    except Exception as e:
        print(f"Error loading config file {config_path}: {e}")
        sys.exit(1)


def find_valid_paths(config):
    """Find all valid UE5 installation paths from config."""
    valid_installations = []
    
    for installation in config.get('installations', []):
        if not installation.get('enabled', True):
            continue
            
        version = installation.get('version', 'unknown')
        paths = installation.get('paths', [])
        
        # Find first valid path for this version
        valid_path = None
        for path in paths:
            if os.path.exists(path):
                valid_path = path
                print(f"Found UE5 {version} at: {path}")
                break
        
        if valid_path:
            valid_installations.append({
                'version': version,
                'path': valid_path
            })
        else:
            print(f"Warning: No valid path found for UE5 {version}. Tried:")
            for path in paths:
                print(f"  - {path}")
    
    return valid_installations


def scan_from_config(config_path):
    """Scan all UE5 installations defined in config file."""
    config = load_config(config_path)
    installations = find_valid_paths(config)
    
    if not installations:
        print("Error: No valid UE5 installations found in config.")
        sys.exit(1)
    
    scanner = Ue5Scanner()
    output_dir = config.get('output_directory', '../metadata')
    output_template = config.get('output_filename_template', 'engine_{version}_scanned.json')
    
    # Create output directory if it doesn't exist
    os.makedirs(output_dir, exist_ok=True)
    
    results = []
    for installation in installations:
        version = installation['version']
        path = installation['path']
        
        print(f"\n{'='*60}")
        print(f"Scanning UE5 {version}")
        print(f"{'='*60}")
        
        try:
            classes, structs, enums, includes = scanner.scan_directory(path)
            
            metadata = {
                "engine_version": version,
                "classes": classes,
                "structs": structs,
                "enums": enums,
                "type_aliases": [],
                "include_map": includes,
            }
            
            output_file = os.path.join(output_dir, output_template.format(version=version))
            with open(output_file, 'w') as f:
                json.dump(metadata, f, indent=2)
            
            total = len(classes) + len(structs) + len(enums)
            print(f"\nExtraction complete: {total} types ({len(classes)} classes, {len(structs)} structs, {len(enums)} enums)")
            print(f"Include map: {len(includes)} entries")
            print(f"Saved to {output_file}")
            
            results.append({
                'version': version,
                'output': output_file,
                'types': total
            })
            
        except Exception as e:
            print(f"Error scanning UE5 {version}: {e}")
            import traceback
            traceback.print_exc()
    
    print(f"\n{'='*60}")
    print("Summary")
    print(f"{'='*60}")
    for result in results:
        print(f"UE5 {result['version']}: {result['types']} types -> {result['output']}")
    
    return results


def main():
    if len(sys.argv) < 2:
        print("Usage:")
        print("  python ue5_scanner.py <engine_source_path> <output_json>")
        print("  python ue5_scanner.py --legacy <engine_source_path> <output_json>")
        print("  python ue5_scanner.py --config <config_file>")
        print()
        print("  Scans UE5 headers and emits EngineMetadata JSON for KAIN's EngineKnowledge system.")
        print("  Use --legacy to emit the old flat format (backward compat with StdLibResolver).")
        print("  Use --config to scan all UE5 installations defined in a config file.")
        sys.exit(1)

    # Check for config mode
    if '--config' in sys.argv:
        config_idx = sys.argv.index('--config')
        if config_idx + 1 >= len(sys.argv):
            print("Error: --config requires a config file path")
            sys.exit(1)
        config_path = sys.argv[config_idx + 1]
        scan_from_config(config_path)
        return

    legacy = '--legacy' in sys.argv
    args = [a for a in sys.argv[1:] if a != '--legacy']
    
    if len(args) < 2:
        print("Error: Missing required arguments")
        print("Usage: python ue5_scanner.py [--legacy] <engine_source_path> [path2...] <output_json>")
        sys.exit(1)
    
    output_json = args[-1]
    input_paths = args[:-1]

    scanner = Ue5Scanner()

    if legacy:
        all_data = []
        for path in input_paths:
            if os.path.exists(path):
                print(f"Scanning (legacy) {path}...")
                data = scanner.scan_to_legacy(path)
                all_data.extend(data)
            else:
                print(f"Warning: Path not found: {path}")
        with open(output_json, 'w') as f:
            json.dump(all_data, f, indent=2)
        print(f"Legacy extraction complete. Found {len(all_data)} files.")
    else:
        all_classes = []
        all_structs = []
        all_enums = []
        all_includes = {}

        for path in input_paths:
            if os.path.exists(path):
                print(f"Scanning {path}...")
                classes, structs, enums, includes = scanner.scan_directory(path)
                all_classes.extend(classes)
                all_structs.extend(structs)
                all_enums.extend(enums)
                all_includes.update(includes)
                print(f"  Found {len(classes)} classes, {len(structs)} structs, {len(enums)} enums")
            else:
                print(f"Warning: Path not found: {path}")

        metadata = {
            "engine_version": "5.4",
            "classes": all_classes,
            "structs": all_structs,
            "enums": all_enums,
            "type_aliases": [],
            "include_map": all_includes,
        }

        with open(output_json, 'w') as f:
            json.dump(metadata, f, indent=2)

        total = len(all_classes) + len(all_structs) + len(all_enums)
        print(f"\nExtraction complete: {total} types ({len(all_classes)} classes, {len(all_structs)} structs, {len(all_enums)} enums)")
        print(f"Include map: {len(all_includes)} entries")
        print(f"Saved to {output_json}")

if __name__ == "__main__":
    main()
