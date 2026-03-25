#!/usr/bin/env python3
"""
KAIN Editor Attributes Extractor v1
Scans UE5 engine source to extract editor framework patterns and base classes.

Extracts:
  - Editor base classes (IDetailCustomization, SCompoundWidget, FAssetEditorToolkit, etc.)
  - Naming conventions (prefixes, suffixes)
  - Required includes and modules
  - Common patterns and boilerplate

Outputs:
  - editor_attributes.json (feeds into KAIN's editor codegen system)

Usage:
    python editor_attributes_extractor.py <ue5_engine_path> --output <output_dir>
    python editor_attributes_extractor.py D:\\Unreal\\UE_5.7\\Engine --output ../metadata
"""

import os
import re
import json
import sys
from pathlib import Path
from collections import defaultdict


class EditorAttributesExtractor:
    """
    Scans UE5 engine source for editor framework patterns.
    Focuses on UnrealEd, Slate, SlateCore, PropertyEditor, AssetTools modules.
    """

    def __init__(self):
        # Patterns for detecting editor base classes
        self.base_class_patterns = {
            # Slate widgets
            'SCompoundWidget': re.compile(r'class\s+(?:[\w_]*?API\s+)?(S\w+)\s*:\s*public\s+SCompoundWidget'),
            'SLeafWidget': re.compile(r'class\s+(?:[\w_]*?API\s+)?(S\w+)\s*:\s*public\s+SLeafWidget'),
            'SPanel': re.compile(r'class\s+(?:[\w_]*?API\s+)?(S\w+)\s*:\s*public\s+SPanel'),
            'SBorder': re.compile(r'class\s+(?:[\w_]*?API\s+)?(S\w+)\s*:\s*public\s+SBorder'),
            
            # Editor viewports
            'SEditorViewport': re.compile(r'class\s+(?:[\w_]*?API\s+)?(S\w+)\s*:\s*public\s+SEditorViewport'),
            'FEditorViewportClient': re.compile(r'class\s+(?:[\w_]*?API\s+)?(F\w+)\s*:\s*public\s+FEditorViewportClient'),
            
            # Details customization
            'IDetailCustomization': re.compile(r'class\s+(?:[\w_]*?API\s+)?(F\w+)\s*:\s*public\s+IDetailCustomization'),
            'IPropertyTypeCustomization': re.compile(r'class\s+(?:[\w_]*?API\s+)?(F\w+)\s*:\s*public\s+IPropertyTypeCustomization'),
            
            # Asset editors
            'FAssetEditorToolkit': re.compile(r'class\s+(?:[\w_]*?API\s+)?(F\w+)\s*:\s*public\s+FAssetEditorToolkit'),
            'FWorkflowCentricApplication': re.compile(r'class\s+(?:[\w_]*?API\s+)?(F\w+)\s*:\s*public\s+FWorkflowCentricApplication'),
            
            # Modules
            'IModuleInterface': re.compile(r'class\s+(?:[\w_]*?API\s+)?(F\w+)\s*:\s*public\s+IModuleInterface'),
            
            # Commands
            'TCommands': re.compile(r'class\s+(?:[\w_]*?API\s+)?(F\w+)\s*:\s*public\s+TCommands<\w+>'),
            
            # Asset types
            'UObject': re.compile(r'UCLASS\([^)]*\)\s+class\s+(?:[\w_]*?API\s+)?(U\w+)\s*:\s*public\s+UObject'),
        }
        
        # Track examples of each pattern
        self.examples = defaultdict(list)  # base_class -> [(class_name, header, module)]
        
        # Track naming patterns
        self.naming_patterns = defaultdict(lambda: {'prefixes': defaultdict(int), 'suffixes': defaultdict(int)})
        
        # Track required includes per base class
        self.includes_per_base = defaultdict(lambda: defaultdict(int))
        
        # Track module dependencies
        self.module_deps = defaultdict(lambda: defaultdict(int))
        
        # Stats
        self.files_scanned = 0
        self.files_with_patterns = 0

    def scan_directory(self, root_dir, target_modules=None):
        """
        Recursively scan headers in specific UE5 modules.
        
        Args:
            root_dir: UE5 Engine/Source path
            target_modules: List of module names to scan (default: editor-related modules)
        """
        if target_modules is None:
            target_modules = [
                'UnrealEd', 'Slate', 'SlateCore', 'PropertyEditor',
                'AssetTools', 'EditorStyle', 'InputCore', 'ToolMenus',
                'WorkspaceMenuStructure', 'ContentBrowser', 'LevelEditor',
                'DetailCustomizations', 'ComponentVisualizers',
                'Kismet', 'BlueprintGraph', 'GraphEditor',
            ]
        
        root_path = Path(root_dir)
        if not root_path.exists():
            print(f"  ⚠️  Path not found: {root_dir}")
            return
        
        # Find module directories
        module_paths = []
        for module_name in target_modules:
            # Check common locations
            for base in ['Runtime', 'Editor', 'Developer']:
                module_path = root_path / base / module_name
                if module_path.exists():
                    module_paths.append((module_name, module_path))
                    break
        
        print(f"  📁 Found {len(module_paths)} target modules")
        
        for module_name, module_path in module_paths:
            print(f"\n  🔍 Scanning module: {module_name}")
            h_files = list(module_path.rglob("*.h"))
            print(f"     {len(h_files)} headers")
            
            for header_path in h_files:
                self.scan_file(str(header_path), module_name, str(root_dir))
    
    def scan_file(self, file_path, module_name, root_dir):
        """Scan a single header file for editor patterns."""
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
        except Exception:
            return
        
        self.files_scanned += 1
        found_any = False
        
        header_rel = self._compute_header_path(file_path, root_dir)
        
        # Extract includes from this file
        includes = re.findall(r'#include\s+"([^"]+)"', content)
        
        # Check each base class pattern
        for base_class, pattern in self.base_class_patterns.items():
            for match in pattern.finditer(content):
                class_name = match.group(1)
                
                # Record example
                self.examples[base_class].append({
                    'name': class_name,
                    'header': header_rel,
                    'module': module_name,
                })
                
                # Analyze naming pattern
                prefix = class_name[0] if class_name else ''
                self.naming_patterns[base_class]['prefixes'][prefix] += 1
                
                # Extract suffix (e.g., "DetailsCustomization" from "FMyDetailsCustomization")
                suffix = self._extract_suffix(class_name, base_class)
                if suffix:
                    self.naming_patterns[base_class]['suffixes'][suffix] += 1
                
                # Record includes for this base class
                for inc in includes:
                    self.includes_per_base[base_class][inc] += 1
                
                # Record module dependency
                self.module_deps[base_class][module_name] += 1
                
                found_any = True
        
        if found_any:
            self.files_with_patterns += 1
    
    def _extract_suffix(self, class_name, base_class):
        """Extract common suffix from class name."""
        # Common suffixes to look for
        suffixes = [
            'DetailsCustomization', 'Details', 'Customization',
            'Toolkit', 'Editor', 'AssetEditor',
            'Viewport', 'ViewportClient',
            'Module', 'Commands', 'Extension',
            'Panel', 'Widget', 'Tab',
        ]
        
        for suffix in suffixes:
            if class_name.endswith(suffix):
                return suffix
        
        return None
    
    def _compute_header_path(self, file_path, root_dir):
        """Compute UE5-style include path."""
        rel = os.path.relpath(file_path, root_dir).replace('\\', '/')
        for marker in ('Public/', 'Classes/', 'Private/'):
            idx = rel.find(marker)
            if idx != -1:
                return rel[idx + len(marker):]
        return os.path.basename(file_path)
    
    def generate_attribute_definitions(self):
        """
        Generate editor_attributes.json from extracted patterns.
        
        Returns a dict mapping attribute names to their codegen rules.
        """
        attributes = {}
        
        # === SLATE WIDGETS ===
        if 'SCompoundWidget' in self.examples:
            attributes['slate'] = {
                'description': 'Generates a Slate UI widget (compound widget with child slots)',
                'base_class': 'SCompoundWidget',
                'class_prefix': 'S',
                'generates': 'slate_widget',
                'required_includes': self._top_includes('SCompoundWidget', 5),
                'required_modules': ['Slate', 'SlateCore'],
                'examples': self.examples['SCompoundWidget'][:3],
                'naming_convention': {
                    'prefix': 'S',
                    'pattern': 'S{Name}',
                },
                'boilerplate': {
                    'slate_begin_args': True,
                    'slate_end_args': True,
                    'construct_method': True,
                },
            }
        
        # === DETAILS CUSTOMIZATION ===
        if 'IDetailCustomization' in self.examples:
            top_suffix = self._most_common_suffix('IDetailCustomization')
            attributes['details'] = {
                'description': 'Generates a Details panel customization',
                'base_class': 'IDetailCustomization',
                'class_prefix': 'F',
                'class_suffix': top_suffix or 'DetailsCustomization',
                'generates': 'details_customization',
                'required_includes': self._top_includes('IDetailCustomization', 5),
                'required_modules': ['PropertyEditor', 'UnrealEd'],
                'examples': self.examples['IDetailCustomization'][:3],
                'naming_convention': {
                    'prefix': 'F',
                    'suffix': top_suffix or 'DetailsCustomization',
                    'pattern': 'F{Name}DetailsCustomization',
                },
                'boilerplate': {
                    'customize_details_method': True,
                    'detail_layout_builder': True,
                },
            }
        
        # === PROPERTY TYPE CUSTOMIZATION ===
        if 'IPropertyTypeCustomization' in self.examples:
            attributes['property_customization'] = {
                'description': 'Generates a property type customization',
                'base_class': 'IPropertyTypeCustomization',
                'class_prefix': 'F',
                'class_suffix': 'Customization',
                'generates': 'property_customization',
                'required_includes': self._top_includes('IPropertyTypeCustomization', 5),
                'required_modules': ['PropertyEditor'],
                'examples': self.examples['IPropertyTypeCustomization'][:3],
            }
        
        # === EDITOR VIEWPORT ===
        if 'SEditorViewport' in self.examples:
            attributes['viewport'] = {
                'description': 'Generates an editor viewport with viewport client',
                'base_class': 'SEditorViewport',
                'class_prefix': 'S',
                'class_suffix': 'Viewport',
                'generates': 'editor_viewport',
                'required_includes': self._top_includes('SEditorViewport', 5),
                'required_modules': ['UnrealEd', 'Slate'],
                'examples': self.examples['SEditorViewport'][:3],
                'requires_client': True,
                'client_base': 'FEditorViewportClient',
                'client_prefix': 'F',
                'client_suffix': 'ViewportClient',
                'naming_convention': {
                    'viewport_prefix': 'S',
                    'viewport_suffix': 'Viewport',
                    'client_prefix': 'F',
                    'client_suffix': 'ViewportClient',
                },
            }
        
        # === ASSET EDITOR TOOLKIT ===
        if 'FAssetEditorToolkit' in self.examples:
            attributes['asset_editor'] = {
                'description': 'Generates a full asset editor toolkit',
                'base_class': 'FAssetEditorToolkit',
                'class_prefix': 'F',
                'class_suffix': 'Toolkit',
                'generates': 'asset_editor_toolkit',
                'required_includes': self._top_includes('FAssetEditorToolkit', 5),
                'required_modules': ['UnrealEd', 'AssetTools'],
                'examples': self.examples['FAssetEditorToolkit'][:3],
                'naming_convention': {
                    'prefix': 'F',
                    'suffix': 'Toolkit',
                    'pattern': 'F{Name}Toolkit',
                },
                'boilerplate': {
                    'init_editor_method': True,
                    'toolkit_name_methods': True,
                    'register_tab_spawners': True,
                },
            }
        
        # === EDITOR MODULE ===
        if 'IModuleInterface' in self.examples:
            attributes['editor_module'] = {
                'description': 'Generates an editor module with IMPLEMENT_MODULE',
                'base_class': 'IModuleInterface',
                'class_prefix': 'F',
                'class_suffix': 'Module',
                'generates': 'editor_module',
                'required_includes': self._top_includes('IModuleInterface', 5),
                'required_modules': ['Core', 'CoreUObject'],
                'examples': self.examples['IModuleInterface'][:3],
                'naming_convention': {
                    'prefix': 'F',
                    'suffix': 'Module',
                    'pattern': 'F{Name}Module',
                },
                'boilerplate': {
                    'startup_module_method': True,
                    'shutdown_module_method': True,
                    'implement_module_macro': True,
                },
            }
        
        # === COMMANDS ===
        if 'TCommands' in self.examples:
            attributes['commands'] = {
                'description': 'Generates command palette entries',
                'base_class': 'TCommands',
                'class_prefix': 'F',
                'class_suffix': 'Commands',
                'generates': 'command_set',
                'required_includes': self._top_includes('TCommands', 5),
                'required_modules': ['Slate', 'InputCore'],
                'examples': self.examples['TCommands'][:3],
                'naming_convention': {
                    'prefix': 'F',
                    'suffix': 'Commands',
                    'pattern': 'F{Name}Commands',
                },
            }
        
        # === TOOLBAR (inferred from common patterns) ===
        attributes['toolbar'] = {
            'description': 'Generates a toolbar extension',
            'base_class': 'FToolBarBuilder',
            'class_prefix': 'F',
            'class_suffix': 'Extension',
            'generates': 'toolbar_extension',
            'required_includes': ['Framework/MultiBox/MultiBoxBuilder.h'],
            'required_modules': ['Slate'],
            'naming_convention': {
                'prefix': 'F',
                'suffix': 'Extension',
                'pattern': 'F{Name}Extension',
            },
        }
        
        # === MENU (inferred) ===
        attributes['menu'] = {
            'description': 'Generates a menu extension',
            'base_class': 'FMenuBuilder',
            'class_prefix': 'F',
            'class_suffix': 'MenuExtension',
            'generates': 'menu_extension',
            'required_includes': ['Framework/MultiBox/MultiBoxBuilder.h'],
            'required_modules': ['Slate'],
            'naming_convention': {
                'prefix': 'F',
                'suffix': 'MenuExtension',
                'pattern': 'F{Name}MenuExtension',
            },
        }
        
        return attributes
    
    def _top_includes(self, base_class, n=5):
        """Get top N most common includes for a base class."""
        if base_class not in self.includes_per_base:
            return []
        
        includes = self.includes_per_base[base_class]
        sorted_includes = sorted(includes.items(), key=lambda x: -x[1])
        return [inc for inc, _ in sorted_includes[:n]]
    
    def _most_common_suffix(self, base_class):
        """Get the most common suffix for a base class."""
        if base_class not in self.naming_patterns:
            return None
        
        suffixes = self.naming_patterns[base_class]['suffixes']
        if not suffixes:
            return None
        
        return max(suffixes.items(), key=lambda x: x[1])[0]
    
    def print_stats(self):
        """Print extraction statistics."""
        print(f"\n  📊 Extraction Results:")
        print(f"     Files scanned:        {self.files_scanned:,}")
        print(f"     Files with patterns:  {self.files_with_patterns:,}")
        print(f"\n  📋 Patterns Found:")
        
        for base_class, examples in sorted(self.examples.items()):
            print(f"     {base_class:30s} {len(examples):4d} examples")
            
            # Show naming pattern
            if base_class in self.naming_patterns:
                prefixes = self.naming_patterns[base_class]['prefixes']
                suffixes = self.naming_patterns[base_class]['suffixes']
                
                if prefixes:
                    top_prefix = max(prefixes.items(), key=lambda x: x[1])
                    print(f"       → Prefix: {top_prefix[0]} ({top_prefix[1]} uses)")
                
                if suffixes:
                    top_suffix = max(suffixes.items(), key=lambda x: x[1])
                    print(f"       → Suffix: {top_suffix[0]} ({top_suffix[1]} uses)")


def main():
    import argparse
    parser = argparse.ArgumentParser(
        description='KAIN Editor Attributes Extractor - Extract editor framework patterns from UE5'
    )
    parser.add_argument('engine_path', help='Path to UE5 Engine/Source directory')
    parser.add_argument('--output', '-o', default='.', help='Output directory for JSON file')
    parser.add_argument('--stats-only', action='store_true', help='Only print stats, no output file')
    parser.add_argument('--modules', nargs='+', help='Specific modules to scan (default: all editor modules)')
    args = parser.parse_args()
    
    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    print("=" * 70)
    print("🎨 KAIN Editor Attributes Extractor v1")
    print("=" * 70)
    print(f"📂 Engine path: {args.engine_path}")
    print(f"📁 Output: {output_dir}")
    print()
    
    extractor = EditorAttributesExtractor()
    
    print("━" * 70)
    print("🔍 Scanning UE5 Editor Modules")
    print("━" * 70)
    
    extractor.scan_directory(args.engine_path, args.modules)
    extractor.print_stats()
    
    if not args.stats_only:
        print(f"\n{'━' * 70}")
        print("📝 Generating editor_attributes.json")
        print("━" * 70)
        
        attributes = extractor.generate_attribute_definitions()
        
        output_data = {
            "_meta": {
                "generator": "editor_attributes_extractor.py",
                "source": args.engine_path,
                "total_attributes": len(attributes),
                "description": "Editor framework patterns extracted from UE5 engine source",
            },
            "attributes": attributes,
        }
        
        out_path = output_dir / "editor_attributes.json"
        with open(out_path, 'w', encoding='utf-8') as f:
            json.dump(output_data, f, indent=2, ensure_ascii=False)
        
        file_size_kb = os.path.getsize(out_path) / 1024
        print(f"\n  💾 Saved: {out_path} ({file_size_kb:.1f} KB)")
        print(f"  📊 Attributes defined: {len(attributes)}")
        
        # Print attribute summary
        print(f"\n  📋 Attributes:")
        for attr_name, attr_info in sorted(attributes.items()):
            base = attr_info.get('base_class', 'N/A')
            print(f"     @{attr_name:20s} → {base}")
    
    print(f"\n{'=' * 70}")
    print("✅ Extraction complete!")
    print(f"{'=' * 70}")


if __name__ == '__main__':
    main()
