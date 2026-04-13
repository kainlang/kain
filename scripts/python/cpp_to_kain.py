#!/usr/bin/env python3
"""
C++ → KAIN Decompiler
Extracts KAIN source from KAIN-generated C++ files with embedded markers.

Usage:
    python cpp_to_kain.py unreal_plugins/VoxelForgePro/VoxelForgePro/Source/ --output recovered.kn
    python cpp_to_kain.py unreal_plugins/VoxelForgePro/VoxelForgePro/Source/ --validate  # Round-trip test
"""

import re
import sys
import argparse
from pathlib import Path
from typing import List, Dict, Tuple

class KainExtractor:
    """Extracts KAIN source from C++ files with KAIN markers."""
    
    def __init__(self):
        self.kain_lines: List[str] = []
        self.stats = {
            'files_processed': 0,
            'actors_found': 0,
            'components_found': 0,
            'structs_found': 0,
            'enums_found': 0,
            'functions_found': 0,
            'shaders_found': 0,
        }
    
    def extract_from_file(self, cpp_path: Path) -> List[str]:
        """Extract KAIN source from a single C++ file."""
        content = cpp_path.read_text(encoding='utf-8', errors='ignore')
        extracted = []
        
        # Pattern 1: Block markers (KAIN_BEGIN/KAIN_END)
        block_pattern = r'// KAIN_BEGIN: (.+?)\n(.*?)// KAIN_END: \1'
        for match in re.finditer(block_pattern, content, re.DOTALL):
            item_type = match.group(1)
            block_content = match.group(2)
            
            # Extract KAIN lines from block
            kain_lines = []
            for line in block_content.split('\n'):
                if '// KAIN:' in line:
                    kain_line = line.split('// KAIN:', 1)[1].strip()
                    kain_lines.append(kain_line)
            
            if kain_lines:
                extracted.extend(kain_lines)
                extracted.append('')  # Blank line between items
                
                # Update stats
                if item_type.startswith('actor'):
                    self.stats['actors_found'] += 1
                elif item_type.startswith('@component'):
                    self.stats['components_found'] += 1
                elif item_type.startswith('struct'):
                    self.stats['structs_found'] += 1
                elif item_type.startswith('enum'):
                    self.stats['enums_found'] += 1
        
        # Pattern 2: Inline markers (single-line KAIN comments)
        inline_pattern = r'^\s*// KAIN: (.+)$'
        for line in content.split('\n'):
            match = re.match(inline_pattern, line)
            if match:
                kain_line = match.group(1).strip()
                if kain_line and kain_line not in extracted:
                    extracted.append(kain_line)
        
        if extracted:
            self.stats['files_processed'] += 1
        
        return extracted
    
    def extract_from_directory(self, source_dir: Path) -> str:
        """Extract KAIN source from all C++ files in directory."""
        all_kain_lines = []
        
        # Process headers first (declarations)
        for header_file in sorted(source_dir.rglob('*.h')):
            if 'Intermediate' in str(header_file) or 'Binaries' in str(header_file):
                continue
            
            print(f"Processing: {header_file.relative_to(source_dir)}")
            extracted = self.extract_from_file(header_file)
            all_kain_lines.extend(extracted)
        
        # Then process source files (implementations)
        for cpp_file in sorted(source_dir.rglob('*.cpp')):
            if 'Intermediate' in str(cpp_file) or 'Binaries' in str(cpp_file):
                continue
            
            print(f"Processing: {cpp_file.relative_to(source_dir)}")
            extracted = self.extract_from_file(cpp_file)
            all_kain_lines.extend(extracted)
        
        # Deduplicate while preserving order
        seen = set()
        unique_lines = []
        for line in all_kain_lines:
            if line not in seen:
                seen.add(line)
                unique_lines.append(line)
        
        return '\n'.join(unique_lines)
    
    def print_stats(self):
        """Print extraction statistics."""
        print("\n" + "="*60)
        print("Extraction Statistics")
        print("="*60)
        print(f"Files processed:  {self.stats['files_processed']}")
        print(f"Actors found:     {self.stats['actors_found']}")
        print(f"Components found: {self.stats['components_found']}")
        print(f"Structs found:    {self.stats['structs_found']}")
        print(f"Enums found:      {self.stats['enums_found']}")
        print(f"Functions found:  {self.stats['functions_found']}")
        print(f"Shaders found:    {self.stats['shaders_found']}")
        print("="*60)


def validate_round_trip(source_dir: Path, kain_binary: Path = Path("kain")) -> bool:
    """
    Validate round-trip compilation:
    1. Extract KAIN from C++
    2. Compile extracted KAIN
    3. Diff generated C++ with original
    """
    import subprocess
    import tempfile
    import difflib
    
    print("\n🔄 Starting round-trip validation...")
    
    # Step 1: Extract KAIN
    extractor = KainExtractor()
    kain_source = extractor.extract_from_directory(source_dir)
    
    if not kain_source.strip():
        print("❌ No KAIN source extracted! C++ files may not have markers.")
        return False
    
    # Step 2: Write to temp file and compile
    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir_path = Path(tmpdir)
        kain_file = tmpdir_path / "recovered.kn"
        kain_file.write_text(kain_source)
        
        print(f"\n📝 Extracted KAIN ({len(kain_source.splitlines())} lines)")
        print(f"   Saved to: {kain_file}")
        
        # Create minimal KAIN.toml
        toml_content = """
[plugin]
name = "RecoveredPlugin"
version = "1.0.0"
engine_version = "5.4"
category = "Recovered"

[build]
sources = ["recovered.kn"]
"""
        toml_file = tmpdir_path / "KAIN.toml"
        toml_file.write_text(toml_content)
        
        # Compile
        print(f"\n🔨 Compiling extracted KAIN...")
        result = subprocess.run(
            [str(kain_binary), "build", "--ue5"],
            cwd=tmpdir_path,
            capture_output=True,
            text=True
        )
        
        if result.returncode != 0:
            print(f"❌ Compilation failed!")
            print(result.stderr)
            return False
        
        print("✅ Compilation succeeded!")
        
        # Step 3: Diff generated C++ with original
        print("\n🔍 Comparing generated C++ with original...")
        
        generated_dir = tmpdir_path / "Source" / "RecoveredPlugin"
        if not generated_dir.exists():
            print(f"❌ Generated source not found at {generated_dir}")
            return False
        
        # Compare key files
        differences_found = False
        for gen_file in generated_dir.rglob('*.h'):
            # Find corresponding original file
            rel_name = gen_file.name
            orig_files = list(source_dir.rglob(rel_name))
            
            if not orig_files:
                continue
            
            orig_file = orig_files[0]
            gen_content = gen_file.read_text().splitlines()
            orig_content = orig_file.read_text().splitlines()
            
            # Remove KAIN markers and timestamps for comparison
            gen_clean = [l for l in gen_content if '// KAIN' not in l and 'Generated on' not in l]
            orig_clean = [l for l in orig_content if '// KAIN' not in l and 'Generated on' not in l]
            
            diff = list(difflib.unified_diff(orig_clean, gen_clean, lineterm=''))
            
            if diff:
                differences_found = True
                print(f"\n⚠️  Differences in {rel_name}:")
                for line in diff[:20]:  # Show first 20 diff lines
                    print(line)
        
        if not differences_found:
            print("\n✅ Perfect round-trip! Generated C++ matches original.")
            return True
        else:
            print("\n⚠️  Round-trip has differences (may be acceptable)")
            return False


def main():
    parser = argparse.ArgumentParser(
        description='Extract KAIN source from KAIN-generated C++ files'
    )
    parser.add_argument(
        'source_dir',
        type=Path,
        help='Directory containing C++ source files (e.g., unreal_plugins/VoxelForgePro/VoxelForgePro/Source/)'
    )
    parser.add_argument(
        '--output', '-o',
        type=Path,
        help='Output .kn file path (default: recovered.kn)'
    )
    parser.add_argument(
        '--validate',
        action='store_true',
        help='Validate round-trip compilation'
    )
    parser.add_argument(
        '--kain-binary',
        type=Path,
        default=Path('kain'),
        help='Path to kain binary (default: kain)'
    )
    
    args = parser.parse_args()
    
    if not args.source_dir.exists():
        print(f"❌ Source directory not found: {args.source_dir}")
        sys.exit(1)
    
    if args.validate:
        success = validate_round_trip(args.source_dir, args.kain_binary)
        sys.exit(0 if success else 1)
    
    # Extract KAIN source
    extractor = KainExtractor()
    kain_source = extractor.extract_from_directory(args.source_dir)
    
    if not kain_source.strip():
        print("ERROR: No KAIN source extracted!")
        print("   Make sure C++ files have KAIN markers (// KAIN: ...)")
        sys.exit(1)
    
    # Write output
    output_path = args.output or Path('recovered.kn')
    output_path.write_text(kain_source)
    
    print(f"\nExtracted KAIN source to: {output_path}")
    print(f"   Lines: {len(kain_source.splitlines())}")
    
    extractor.print_stats()


if __name__ == '__main__':
    main()
