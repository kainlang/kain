#!/usr/bin/env python3
"""
KAIN Extension Scanner
Scans external UE5 plugins/modules (like MetaHuman, Niagara, PCG, etc.) and generates
extension metadata that can be dynamically loaded into the KAIN compiler.

Output: unreal/metadata/extensions/<extension_name>.json

Usage:
    python extension_scanner.py <plugin_dir> --name <extension_name>
    python extension_scanner.py Research/ReferenceCode/MetaHuman --name metahuman
    python extension_scanner.py "C:/Program Files/Epic Games/UE_5.4/Engine/Plugins/Runtime/Niagara" --name niagara

The generated JSON files are automatically loaded by the Rust backend via Ue5Context.
"""

import os
import re
import json
import sys
import time
from pathlib import Path
from collections import defaultdict


class ExtensionScanner:
    """Scans a UE5 plugin/module and extracts all relevant API information."""
    
    def __init__(self, extension_name: str):
        self.extension_name = extension_name
        
        # Patterns
        self.class_pattern = re.compile(
            r'UCLASS\((.*?)\)\s+class\s+(?:([\w_]*?API)\s+)?(\w+)\s*(?::\s*public\s+([\w:,\s]+?))?(?:\s*\{)',
            re.DOTALL
        )
        self.struct_pattern = re.compile(
            r'USTRUCT\((.*?)\)\s+struct\s+(?:([\w_]*?API)\s+)?(\w+)\s*(?::\s*public\s+([\w:,\s]+?))?(?:\s*\{)',
            re.DOTALL
        )
        self.enum_pattern = re.compile(
            r'UENUM\((.*?)\)\s+enum\s+(?:class\s+)?(\w+)(?:\s*:\s*\w+)?\s*\{([^}]*)\}',
            re.DOTALL
        )
        self.interface_pattern = re.compile(
            r'UINTERFACE\((.*?)\)\s+class\s+(?:([\w_]*?API)\s+)?(\w+)\s*:\s*public\s+UInterface',
            re.DOTALL
        )
        self.component_pattern = re.compile(
            r'class\s+(?:([\w_]*?API)\s+)?(\w+)\s*:\s*public\s+U(\w*Component)',
        )
        self.subsystem_pattern = re.compile(
            r'class\s+(?:([\w_]*?API)\s+)?(\w+)\s*:\s*public\s+U(\w*Subsystem)',
        )
        self.blueprint_func_pattern = re.compile(
            r'UFUNCTION\([^)]*BlueprintCallable[^)]*\)\s*(?:static\s+)?(?:[\w:]+\s+)?(\w+)\s*\(',
        )
        
        # Results
        self.classes = {}
        self.structs = {}
        self.enums = {}
        self.interfaces = {}
        self.components = []
        self.subsystems = []
        self.blueprint_functions = []
        self.include_map = {}
        self.module_map = {}
        self.modules = {}
        
        # Stats
        self.files_scanned = 0
        self.files_with_types = 0
    
    def scan_directory(self, root_dir: str):
        """Recursively scan all .h files in the plugin directory."""
        root_path = Path(root_dir)
        if not root_path.exists():
            print(f"❌ Path not found: {root_dir}")
            return False
        
        h_files = list(root_path.rglob("*.h"))
        total = len(h_files)
        print(f"📁 Found {total:,} header files")
        
        for i, header_path in enumerate(h_files):
            if i > 0 and i % 1000 == 0:
                print(f"📊 Progress: {i:,}/{total:,} files ({i*100//total}%)")
            
            self.scan_file(str(header_path), str(root_dir))
        
        # Scan Build.cs files for module dependencies
        build_files = list(root_path.rglob("*.Build.cs"))
        print(f"📁 Found {len(build_files)} Build.cs files")
        for build_file in build_files:
            self.scan_build_file(str(build_file))
        
        return True
    
    def scan_file(self, file_path: str, root_dir: str):
        """Scan a single header file."""
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
        except Exception as e:
            return
        
        self.files_scanned += 1
        found_any = False
        
        header_rel = self._compute_header_path(file_path, root_dir)
        module = self._guess_module(file_path, header_rel)
        
        # === CLASSES ===
        for match in self.class_pattern.finditer(content):
            meta = match.group(1).strip()
            api_export = match.group(2)
            name = match.group(3)
            parent_raw = match.group(4) or ""
            parent = parent_raw.split(',')[0].strip() if parent_raw else ""
            parent = re.sub(r'<.*>', '', parent).strip()
            
            if name in self.classes:
                continue
            
            prefix = "A" if name.startswith("A") and len(name) > 1 and name[1].isupper() else "U"
            is_abstract = "Abstract" in meta or "ABSTRACT" in meta
            is_blueprintable = "Blueprintable" in meta
            
            self.classes[name] = {
                "name": name,
                "parent": parent,
                "header": header_rel,
                "module": module,
                "prefix": prefix,
                "is_abstract": is_abstract,
                "is_blueprintable": is_blueprintable,
                "api_export": api_export,
            }
            self.include_map[name] = header_rel
            self.module_map[name] = module
            found_any = True
        
        # === STRUCTS ===
        for match in self.struct_pattern.finditer(content):
            meta = match.group(1).strip()
            api_export = match.group(2)
            name = match.group(3)
            parent_raw = match.group(4) or ""
            parent = parent_raw.split(',')[0].strip() if parent_raw else ""
            
            if name in self.structs:
                continue
            
            is_table_row = "FTableRowBase" in parent
            is_blueprint_type = "BlueprintType" in meta
            
            self.structs[name] = {
                "name": name,
                "parent": parent,
                "header": header_rel,
                "module": module,
                "is_table_row": is_table_row,
                "is_blueprint_type": is_blueprint_type,
                "api_export": api_export,
            }
            self.include_map[name] = header_rel
            self.module_map[name] = module
            found_any = True
        
        # === ENUMS ===
        for match in self.enum_pattern.finditer(content):
            meta = match.group(1).strip()
            name = match.group(2)
            body = match.group(3)
            
            if name in self.enums:
                continue
            
            is_flags = "Flags" in meta or "UMETA(Bitflags)" in body
            is_blueprint_type = "BlueprintType" in meta
            
            values = []
            for line in body.split('\n'):
                line = line.strip().rstrip(',')
                line = re.sub(r'//.*', '', line).strip()
                line = re.sub(r'UMETA\(.*?\)', '', line).strip()
                if not line or line.startswith('#'):
                    continue
                val_name = line.split('=')[0].split('UMETA')[0].strip().rstrip(',')
                if val_name and val_name not in ('{', '}', ''):
                    values.append(val_name)
            
            # Filter _MAX
            values = [v for v in values if not v.endswith('_MAX') and v]
            
            self.enums[name] = {
                "name": name,
                "header": header_rel,
                "module": module,
                "values": values,
                "is_flags": is_flags,
                "is_blueprint_type": is_blueprint_type,
            }
            self.include_map[name] = header_rel
            self.module_map[name] = module
            found_any = True
        
        # === INTERFACES ===
        for match in self.interface_pattern.finditer(content):
            meta = match.group(1).strip()
            api_export = match.group(2)
            name = match.group(3)
            
            if name in self.interfaces:
                continue
            
            self.interfaces[name] = {
                "name": name,
                "header": header_rel,
                "module": module,
                "api_export": api_export,
            }
            self.include_map[name] = header_rel
            self.module_map[name] = module
            found_any = True
        
        # === COMPONENTS ===
        for match in self.component_pattern.finditer(content):
            api_export = match.group(1)
            name = match.group(2)
            base_component = match.group(3)
            
            if any(c['name'] == name for c in self.components):
                continue
            
            self.components.append({
                "name": name,
                "base": f"U{base_component}",
                "header": header_rel,
                "module": module,
                "api_export": api_export,
            })
            found_any = True
        
        # === SUBSYSTEMS ===
        for match in self.subsystem_pattern.finditer(content):
            api_export = match.group(1)
            name = match.group(2)
            base_subsystem = match.group(3)
            
            if any(s['name'] == name for s in self.subsystems):
                continue
            
            self.subsystems.append({
                "name": name,
                "base": f"U{base_subsystem}",
                "header": header_rel,
                "module": module,
                "api_export": api_export,
            })
            found_any = True
        
        # === BLUEPRINT FUNCTIONS ===
        for match in self.blueprint_func_pattern.finditer(content):
            func_name = match.group(1)
            
            # Get context around the function
            func_start = match.start()
            context_start = max(0, func_start - 200)
            context = content[context_start:func_start + 200]
            
            self.blueprint_functions.append({
                "name": func_name,
                "file": header_rel,
                "module": module,
                "context": context[:300],  # Limit context size
            })
        
        if found_any:
            self.files_with_types += 1
    
    def scan_build_file(self, file_path: str):
        """Scan a Build.cs file for module dependencies."""
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
        except Exception:
            return
        
        # Extract module name
        module_match = re.search(r'public\s+class\s+(\w+)\s*:\s*ModuleRules', content)
        if not module_match:
            return
        
        module_name = module_match.group(1)
        
        # Extract public dependencies
        public_deps = re.findall(r'PublicDependencyModuleNames\.(?:Add|AddRange)\([^)]*"([^"]+)"', content)
        private_deps = re.findall(r'PrivateDependencyModuleNames\.(?:Add|AddRange)\([^)]*"([^"]+)"', content)
        
        self.modules[module_name] = {
            "name": module_name,
            "public_dependencies": list(set(public_deps)),
            "private_dependencies": list(set(private_deps)),
            "path": str(Path(file_path).parent),
        }
    
    def _compute_header_path(self, file_path: str, root_dir: str) -> str:
        """Compute UE5-style include path."""
        rel = os.path.relpath(file_path, root_dir).replace('\\', '/')
        for marker in ('Public/', 'Classes/'):
            idx = rel.find(marker)
            if idx != -1:
                return rel[idx + len(marker):]
        return os.path.basename(file_path)
    
    def _guess_module(self, file_path: str, header_rel: str) -> str:
        """Guess the module name from file path."""
        parts = Path(file_path).parts
        for i, part in enumerate(parts):
            if part in ('Public', 'Private', 'Classes'):
                if i > 0:
                    return parts[i - 1]
        
        # Fallback: use first part of header path
        if '/' in header_rel:
            return header_rel.split('/')[0]
        
        return self.extension_name.title()
    
    def generate_extension_json(self, output_dir: str) -> str:
        """Generate the extension JSON file."""
        output_path = Path(output_dir) / f"{self.extension_name}.json"
        
        extension_data = {
            "extension_name": self.extension_name,
            "version": "1.0.0",
            "description": f"Auto-generated extension metadata for {self.extension_name}",
            "generated_at": time.strftime("%Y-%m-%d %H:%M:%S"),
            
            "classes": list(self.classes.values()),
            "structs": list(self.structs.values()),
            "enums": list(self.enums.values()),
            "interfaces": list(self.interfaces.values()),
            "components": self.components,
            "subsystems": self.subsystems,
            "blueprint_functions": self.blueprint_functions[:100],  # Limit to first 100
            
            "include_map": self.include_map,
            "module_map": self.module_map,
            "modules": self.modules,
            
            "stats": {
                "files_scanned": self.files_scanned,
                "files_with_types": self.files_with_types,
                "total_classes": len(self.classes),
                "total_structs": len(self.structs),
                "total_enums": len(self.enums),
                "total_interfaces": len(self.interfaces),
                "total_components": len(self.components),
                "total_subsystems": len(self.subsystems),
                "total_blueprint_functions": len(self.blueprint_functions),
                "total_modules": len(self.modules),
            }
        }
        
        with open(output_path, 'w', encoding='utf-8') as f:
            json.dump(extension_data, f, indent=2, ensure_ascii=False)
        
        return str(output_path)
    
    def print_stats(self):
        """Print extraction statistics."""
        print(f"\n{'='*70}")
        print(f"📊 Extension: {self.extension_name}")
        print(f"{'='*70}")
        print(f"Files scanned:          {self.files_scanned:,}")
        print(f"Files with types:       {self.files_with_types:,}")
        print(f"Classes:                {len(self.classes):,}")
        print(f"Structs:                {len(self.structs):,}")
        print(f"Enums:                  {len(self.enums):,}")
        print(f"Interfaces:             {len(self.interfaces):,}")
        print(f"Components:             {len(self.components):,}")
        print(f"Subsystems:             {len(self.subsystems):,}")
        print(f"Blueprint Functions:    {len(self.blueprint_functions):,}")
        print(f"Modules:                {len(self.modules):,}")
        print(f"{'='*70}")


def main():
    import argparse
    
    parser = argparse.ArgumentParser(
        description='KAIN Extension Scanner - Extract API metadata from UE5 plugins'
    )
    parser.add_argument('plugin_dir', help='Plugin/module directory to scan')
    parser.add_argument('--name', '-n', required=True, help='Extension name (e.g., metahuman, niagara, pcg)')
    parser.add_argument('--output', '-o', default='Kain/unreal/metadata/extensions', 
                       help='Output directory (default: Kain/unreal/metadata/extensions)')
    
    args = parser.parse_args()
    
    print("="*70)
    print("🔍 KAIN Extension Scanner")
    print("="*70)
    print(f"📂 Plugin Directory: {args.plugin_dir}")
    print(f"🏷️  Extension Name:   {args.name}")
    print(f"📁 Output Directory: {args.output}")
    print()
    
    start_time = time.time()
    
    # Create output directory
    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # Scan the plugin
    scanner = ExtensionScanner(args.name)
    success = scanner.scan_directory(args.plugin_dir)
    
    if not success:
        sys.exit(1)
    
    # Print stats
    scanner.print_stats()
    
    # Generate JSON
    output_file = scanner.generate_extension_json(str(output_dir))
    file_size = os.path.getsize(output_file) / 1024
    
    elapsed = time.time() - start_time
    
    print(f"\n✅ Extension metadata generated!")
    print(f"📄 Output: {output_file} ({file_size:.1f} KB)")
    print(f"⏱️  Time: {elapsed:.1f}s")
    print(f"\n💡 The extension will be automatically loaded by the KAIN compiler.")
    print(f"   No manual integration needed!")


if __name__ == '__main__':
    main()
