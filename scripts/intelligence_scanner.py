#!/usr/bin/env python3
"""
KAIN Intelligence Scanner - Extract patterns from $1M+ UE5 plugins

This scanner analyzes your entire plugin collection and extracts:
- Class implementations
- Common patterns
- Best practices
- Error solutions
- Include dependencies

Output: kain_intelligence.lance (embedded in compiler)
"""

import os
import re
import sys
from pathlib import Path
from dataclasses import dataclass, asdict
from typing import List, Dict, Optional
import json

try:
    from sentence_transformers import SentenceTransformer
    import lancedb
    HAS_DEPS = True
except ImportError:
    HAS_DEPS = False
    print("⚠️  Missing dependencies. Install with:")
    print("   pip install sentence-transformers lancedb pyarrow")
    sys.exit(1)

# Embedding model (384-dim, fast)
model = SentenceTransformer('all-MiniLM-L6-v2')

@dataclass
class Property:
    name: str
    type: str
    specifiers: List[str]

@dataclass
class Function:
    name: str
    return_type: str
    params: List[Dict[str, str]]
    specifiers: List[str]

@dataclass
class ClassImplementation:
    class_name: str
    parent_class: Optional[str]
    source_plugin: str
    header_code: str
    source_code: str
    properties: List[Dict]
    functions: List[Dict]
    includes: List[str]
    modules: List[str]
    uses_replication: bool
    uses_networking: bool
    uses_timers: bool
    uses_animation: bool
    uses_physics: bool
    description: str
    embedding: List[float]
    line_count: int
    source_file: str

class PluginScanner:
    def __init__(self, output_db='kain_intelligence.lance'):
        self.output_db = output_db
        self.stats = {
            'plugins_scanned': 0,
            'classes_extracted': 0,
            'patterns_found': 0,
            'files_processed': 0,
            'errors': 0,
        }
    
    def scan_all_plugins(self, plugin_dirs: List[str]) -> List[Dict]:
        """Scan all plugin directories"""
        all_classes = []
        
        for plugin_dir in plugin_dirs:
            if not Path(plugin_dir).exists():
                print(f"⚠️  Directory not found: {plugin_dir}")
                continue
            
            print(f"\n📂 Scanning {plugin_dir}...")
            
            for plugin_path in Path(plugin_dir).iterdir():
                if not plugin_path.is_dir():
                    continue
                
                # Skip common non-plugin directories
                if plugin_path.name.startswith('.') or plugin_path.name in ['Binaries', 'Intermediate', 'Saved']:
                    continue
                
                print(f"  🔍 {plugin_path.name}...", end='', flush=True)
                classes = self.scan_plugin(plugin_path)
                all_classes.extend(classes)
                
                self.stats['plugins_scanned'] += 1
                print(f" {len(classes)} classes")
        
        return all_classes
    
    def scan_plugin(self, plugin_path: Path) -> List[Dict]:
        """Scan a single plugin"""
        classes = []
        
        # Find all .h/.cpp pairs in Source directory
        source_dir = plugin_path / 'Source'
        if not source_dir.exists():
            return classes
        
        for header_file in source_dir.rglob('*.h'):
            # Skip generated files
            if '.generated.h' in header_file.name:
                continue
            
            source_file = header_file.with_suffix('.cpp')
            
            self.stats['files_processed'] += 1
            
            if source_file.exists():
                try:
                    cls = self.extract_class(header_file, source_file, plugin_path.name)
                    if cls:
                        classes.append(cls)
                except Exception as e:
                    self.stats['errors'] += 1
                    # Silently continue - some files may be malformed
        
        return classes
    
    def extract_class(self, header_path: Path, source_path: Path, plugin_name: str) -> Optional[Dict]:
        """Extract complete class information"""
        try:
            header_code = header_path.read_text(encoding='utf-8', errors='ignore')
            source_code = source_path.read_text(encoding='utf-8', errors='ignore')
            
            # Must have UCLASS to be interesting
            if 'UCLASS' not in header_code:
                return None
            
            # Extract metadata
            class_name = self.extract_class_name(header_code)
            if not class_name:
                return None
            
            parent_class = self.extract_parent_class(header_code, class_name)
            properties = self.extract_properties(header_code)
            functions = self.extract_functions(header_code)
            includes = self.extract_includes(header_code)
            modules = self.extract_modules(header_code)
            
            # Detect patterns
            uses_replication = any('Replicated' in p.get('specifiers', []) for p in properties)
            uses_networking = any(f['name'].startswith(('Server_', 'Client_', 'Multicast_')) for f in functions)
            uses_timers = 'FTimerHandle' in header_code
            uses_animation = 'UAnimMontage' in header_code or 'UAnimInstance' in header_code
            uses_physics = 'SetSimulatePhysics' in source_code or 'AddForce' in source_code
            
            # Generate description
            description = f"{class_name} from {plugin_name}"
            if parent_class:
                description += f" (extends {parent_class})"
            
            # Generate embedding
            embedding_text = f"{class_name} {parent_class} {' '.join(p['name'] for p in properties)} {' '.join(f['name'] for f in functions)}"
            embedding = model.encode(embedding_text).tolist()
            
            return {
                'class_name': class_name,
                'parent_class': parent_class,
                'source_plugin': plugin_name,
                'header_code': header_code[:10000],  # Limit size
                'source_code': source_code[:10000],  # Limit size
                'properties': properties,
                'functions': functions,
                'includes': includes,
                'modules': modules,
                'uses_replication': uses_replication,
                'uses_networking': uses_networking,
                'uses_timers': uses_timers,
                'uses_animation': uses_animation,
                'uses_physics': uses_physics,
                'description': description,
                'embedding': embedding,
                'line_count': len(header_code.split('\n')) + len(source_code.split('\n')),
                'source_file': str(header_path),
            }
        
        except Exception as e:
            return None
    
    def extract_class_name(self, code: str) -> Optional[str]:
        """Extract UCLASS name"""
        # Look for UCLASS() followed by class declaration
        match = re.search(r'UCLASS\([^)]*\)\s*class\s+(?:\w+_API\s+)?(\w+)', code)
        if match:
            return match.group(1)
        return None
    
    def extract_parent_class(self, code: str, class_name: str) -> Optional[str]:
        """Extract parent class"""
        # Look for: class ClassName : public ParentClass
        pattern = rf'class\s+(?:\w+_API\s+)?{re.escape(class_name)}\s*:\s*public\s+(\w+)'
        match = re.search(pattern, code)
        if match:
            return match.group(1)
        return None
    
    def extract_properties(self, code: str) -> List[Dict]:
        """Extract UPROPERTY declarations"""
        properties = []
        
        # Find all UPROPERTY declarations
        for match in re.finditer(r'UPROPERTY\(([^)]+)\)\s*(\w+(?:<[^>]+>)?(?:\*)?)\s+(\w+);', code):
            specifiers_str = match.group(1)
            specifiers = [s.strip() for s in specifiers_str.split(',')]
            prop_type = match.group(2)
            prop_name = match.group(3)
            
            properties.append({
                'name': prop_name,
                'type': prop_type,
                'specifiers': specifiers,
            })
        
        return properties
    
    def extract_functions(self, code: str) -> List[Dict]:
        """Extract UFUNCTION declarations"""
        functions = []
        
        # Find all UFUNCTION declarations
        for match in re.finditer(r'UFUNCTION\(([^)]+)\)\s*(?:virtual\s+)?(\w+)\s+(\w+)\s*\(([^)]*)\)', code):
            specifiers_str = match.group(1)
            specifiers = [s.strip() for s in specifiers_str.split(',')]
            return_type = match.group(2)
            func_name = match.group(3)
            params_str = match.group(4)
            
            # Parse parameters
            params = []
            if params_str.strip():
                for param in params_str.split(','):
                    param = param.strip()
                    if param:
                        # Simple parsing - just get type and name
                        parts = param.rsplit(' ', 1)
                        if len(parts) == 2:
                            params.append({'type': parts[0], 'name': parts[1]})
            
            functions.append({
                'name': func_name,
                'return_type': return_type,
                'params': params,
                'specifiers': specifiers,
            })
        
        return functions
    
    def extract_includes(self, code: str) -> List[str]:
        """Extract #include statements"""
        includes = []
        for match in re.finditer(r'#include\s+"([^"]+)"', code):
            includes.append(match.group(1))
        return includes
    
    def extract_modules(self, code: str) -> List[str]:
        """Extract module dependencies from includes"""
        modules = set(['Engine', 'CoreUObject'])  # Always needed
        
        if 'Niagara' in code:
            modules.add('Niagara')
        if 'EnhancedInput' in code:
            modules.add('EnhancedInput')
        if 'UMG' in code or 'Widget' in code:
            modules.add('UMG')
        if 'AnimGraph' in code:
            modules.add('AnimGraphRuntime')
        if 'AIModule' in code:
            modules.add('AIModule')
        if 'NavigationSystem' in code:
            modules.add('NavigationSystem')
        
        return list(modules)
    
    def extract_common_patterns(self, all_classes: List[Dict]) -> List[Dict]:
        """Find patterns that appear frequently"""
        print("\n🔍 Extracting common patterns...")
        
        pattern_groups = {}
        
        for cls in all_classes:
            # Group by parent class + property count + features
            key = (
                cls['parent_class'] or 'None',
                len(cls['properties']),
                cls['uses_replication'],
                cls['uses_networking']
            )
            
            if key not in pattern_groups:
                pattern_groups[key] = []
            pattern_groups[key].append(cls)
        
        # Extract patterns that appear in 5+ plugins
        common_patterns = []
        for key, group in pattern_groups.items():
            if len(group) >= 5:
                pattern = {
                    'pattern_name': f"{key[0]}_with_{key[1]}_properties",
                    'pattern_type': 'actor_composition',
                    'frequency': len(group),
                    'parent_class': key[0],
                    'property_count': key[1],
                    'uses_replication': key[2],
                    'uses_networking': key[3],
                    'examples': [
                        {'plugin': cls['source_plugin'], 'class': cls['class_name']}
                        for cls in group[:5]
                    ],
                    'confidence': len(group) / len(all_classes),
                }
                common_patterns.append(pattern)
        
        self.stats['patterns_found'] = len(common_patterns)
        return common_patterns
    
    def save_to_database(self, all_classes: List[Dict], common_patterns: List[Dict]):
        """Save everything to LanceDB"""
        print("\n💾 Saving to database...")
        
        # Create database
        db = lancedb.connect(self.output_db)
        
        # Create tables
        if all_classes:
            db.create_table('class_implementations', all_classes, mode='overwrite')
            print(f"✅ Saved {len(all_classes)} classes")
        
        if common_patterns:
            db.create_table('common_patterns', common_patterns, mode='overwrite')
            print(f"✅ Saved {len(common_patterns)} patterns")
    
    def print_stats(self):
        """Print scanning statistics"""
        print("\n" + "="*60)
        print("📊 SCANNING COMPLETE")
        print("="*60)
        print(f"Plugins scanned:    {self.stats['plugins_scanned']}")
        print(f"Files processed:    {self.stats['files_processed']}")
        print(f"Classes extracted:  {self.stats['classes_extracted']}")
        print(f"Patterns found:     {self.stats['patterns_found']}")
        print(f"Errors encountered: {self.stats['errors']}")
        print("="*60)

def main():
    print("🚀 KAIN Intelligence Scanner")
    print("="*60)
    
    scanner = PluginScanner()
    
    # Your plugin directories - EDIT THESE!
    plugin_dirs = [
        "D:/UE5Plugins/Marketplace",
        "D:/UE5Plugins/Premium",
        "D:/UE5Plugins/Custom",
        # Add more directories here
    ]
    
    print(f"📂 Scanning {len(plugin_dirs)} directories...")
    
    # Scan everything
    all_classes = scanner.scan_all_plugins(plugin_dirs)
    scanner.stats['classes_extracted'] = len(all_classes)
    
    if not all_classes:
        print("\n⚠️  No classes found! Check your plugin directories.")
        return
    
    # Extract patterns
    common_patterns = scanner.extract_common_patterns(all_classes)
    
    # Save to database
    scanner.save_to_database(all_classes, common_patterns)
    
    # Print stats
    scanner.print_stats()
    
    print(f"\n✅ Database saved to: {scanner.output_db}")
    print(f"📊 Size: {Path(scanner.output_db).stat().st_size / 1024 / 1024:.1f} MB")
    print("\n🎉 Ready to embed in KAIN compiler!")

if __name__ == '__main__':
    main()
