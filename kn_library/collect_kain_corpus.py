#!/usr/bin/env python3
"""
KAIN Corpus Collector v1
========================
Scans the entire KAIN repository for .kn files and builds a deduplicated library.

Features:
- Deduplication by content hash (SHA256)
- Quality filtering (min 10 lines of actual code)
- Metadata tracking (source path, size, line count, features)
- Categorization by content type (actor, shader, editor, etc.)
- Generates corpus_index.json for semantic analysis

Output Structure:
    kn_library/
    ├── collect_kain_corpus.py (this script)
    ├── corpus_index.json (metadata database)
    ├── actors/
    ├── shaders/
    ├── editor/
    ├── components/
    ├── utilities/
    └── examples/

Usage:
    python collect_kain_corpus.py
    python collect_kain_corpus.py --min-lines 20 --verbose
    python collect_kain_corpus.py --stats-only
"""

import os
import re
import json
import hashlib
import shutil
from pathlib import Path
from collections import defaultdict
from datetime import datetime


class KainCorpusCollector:
    """Collects and organizes KAIN source files into a deduplicated library."""
    
    def __init__(self, repo_root, library_root, min_lines=10):
        self.repo_root = Path(repo_root)
        self.library_root = Path(library_root)
        self.min_lines = min_lines
        
        # Deduplication tracking
        self.seen_hashes = set()
        self.seen_names = defaultdict(int)
        
        # Statistics
        self.stats = {
            'total_found': 0,
            'duplicates_skipped': 0,
            'too_small_skipped': 0,
            'collected': 0,
            'by_category': defaultdict(int),
        }
        
        # Corpus index (for semantic analysis later)
        self.corpus_index = []
        
        # Category patterns
        self.category_patterns = {
            'actors': [
                r'\bactor\s+\w+',
                r'@replicated',
                r'on\s+Server_',
                r'on\s+Client_',
                r'on\s+Multicast_',
            ],
            'shaders': [
                r'\bshader\s+(fragment|vertex|compute|surface)',
                r'\buniform\s+\w+',
                r'@permutation',
                r'\bSurfaceOutput\b',
            ],
            'editor': [
                r'@slate',
                r'@details',
                r'@viewport',
                r'@toolbar',
                r'@asset_editor',
                r'@editor_module',
            ],
            'components': [
                r'@component',
                r'\bstruct\s+\w+Component',
            ],
            'datatables': [
                r'@datatable',
                r'FTableRowBase',
            ],
            'utilities': [
                r'@blueprint',
                r'\bfn\s+\w+',
            ],
        }
    
    def compute_hash(self, content):
        """Compute SHA256 hash of file content for deduplication."""
        return hashlib.sha256(content.encode('utf-8')).hexdigest()
    
    def count_code_lines(self, content):
        """Count non-empty, non-comment lines."""
        lines = content.split('\n')
        code_lines = 0
        
        for line in lines:
            stripped = line.strip()
            # Skip empty lines and comments
            if stripped and not stripped.startswith('#') and not stripped.startswith('//'):
                code_lines += 1
        
        return code_lines
    
    def detect_category(self, content):
        """Detect the primary category of a KAIN file based on content."""
        scores = defaultdict(int)
        
        for category, patterns in self.category_patterns.items():
            for pattern in patterns:
                matches = len(re.findall(pattern, content, re.IGNORECASE))
                scores[category] += matches
        
        if not scores:
            return 'examples'
        
        # Return category with highest score
        return max(scores.items(), key=lambda x: x[1])[0]
    
    def extract_features(self, content):
        """Extract notable features from KAIN code for indexing."""
        features = []
        
        # Detect language features
        if re.search(r'\bactor\s+\w+', content):
            features.append('actor')
        if re.search(r'\bshader\s+', content):
            features.append('shader')
        if re.search(r'@replicated', content):
            features.append('networking')
        if re.search(r'@blueprint', content):
            features.append('blueprint')
        if re.search(r'@datatable', content):
            features.append('datatable')
        if re.search(r'@slate|@details|@viewport', content):
            features.append('editor')
        if re.search(r'\beffect\s+', content):
            features.append('effects')
        if re.search(r'\bcomptime\s+', content):
            features.append('comptime')
        
        # Detect UE5 integrations
        if re.search(r'\bUStaticMeshComponent\b', content):
            features.append('static_mesh')
        if re.search(r'\bUSkeletalMeshComponent\b', content):
            features.append('skeletal_mesh')
        if re.search(r'\bUMaterialInstanceDynamic\b', content):
            features.append('materials')
        
        return features
    
    def generate_unique_name(self, original_name):
        """Generate a unique filename if name collision occurs."""
        base_name = Path(original_name).stem
        extension = Path(original_name).suffix
        
        if base_name not in self.seen_names:
            self.seen_names[base_name] = 1
            return original_name
        
        # Add numeric suffix
        count = self.seen_names[base_name]
        self.seen_names[base_name] += 1
        return f"{base_name}_{count}{extension}"
    
    def collect_file(self, source_path):
        """Process and collect a single .kn file."""
        try:
            with open(source_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
        except Exception as e:
            print(f"  ⚠️  Failed to read {source_path}: {e}")
            return False
        
        # Check minimum line count
        code_lines = self.count_code_lines(content)
        if code_lines < self.min_lines:
            self.stats['too_small_skipped'] += 1
            return False
        
        # Check for duplicates by content hash
        content_hash = self.compute_hash(content)
        if content_hash in self.seen_hashes:
            self.stats['duplicates_skipped'] += 1
            return False
        
        self.seen_hashes.add(content_hash)
        
        # Detect category
        category = self.detect_category(content)
        
        # Extract features
        features = self.extract_features(content)
        
        # Generate unique filename
        original_name = source_path.name
        unique_name = self.generate_unique_name(original_name)
        
        # Create category directory
        category_dir = self.library_root / category
        category_dir.mkdir(parents=True, exist_ok=True)
        
        # Copy file to library
        dest_path = category_dir / unique_name
        shutil.copy2(source_path, dest_path)
        
        # Add to corpus index
        relative_source = source_path.relative_to(self.repo_root)
        self.corpus_index.append({
            'filename': unique_name,
            'category': category,
            'source_path': str(relative_source),
            'library_path': f"{category}/{unique_name}",
            'size_bytes': source_path.stat().st_size,
            'code_lines': code_lines,
            'content_hash': content_hash,
            'features': features,
            'collected_at': datetime.now().isoformat(),
        })
        
        self.stats['collected'] += 1
        self.stats['by_category'][category] += 1
        
        return True
    
    def scan_repository(self):
        """Recursively scan repository for .kn files."""
        print(f"🔍 Scanning repository: {self.repo_root}")
        print(f"📁 Library location: {self.library_root}")
        print(f"📏 Minimum lines: {self.min_lines}")
        print()
        
        # Find all .kn files
        kn_files = list(self.repo_root.rglob("*.kn"))
        self.stats['total_found'] = len(kn_files)
        
        print(f"📊 Found {len(kn_files)} .kn files")
        print()
        
        # Process each file
        for i, kn_file in enumerate(kn_files, 1):
            if i % 10 == 0:
                print(f"  Progress: {i}/{len(kn_files)} ({i*100//len(kn_files)}%)")
            
            self.collect_file(kn_file)
        
        print()
    
    def save_corpus_index(self):
        """Save corpus index as JSON for semantic analysis."""
        index_path = self.library_root / "corpus_index.json"
        
        index_data = {
            '_meta': {
                'generator': 'collect_kain_corpus.py',
                'generated_at': datetime.now().isoformat(),
                'repo_root': str(self.repo_root),
                'total_files': self.stats['collected'],
                'min_lines': self.min_lines,
            },
            'statistics': dict(self.stats),
            'files': self.corpus_index,
        }
        
        with open(index_path, 'w', encoding='utf-8') as f:
            json.dump(index_data, f, indent=2, ensure_ascii=False)
        
        print(f"💾 Saved corpus index: {index_path}")
        print(f"   Size: {index_path.stat().st_size / 1024:.1f} KB")
    
    def print_statistics(self):
        """Print collection statistics."""
        print()
        print("=" * 70)
        print("📊 KAIN Corpus Collection Statistics")
        print("=" * 70)
        print(f"Total .kn files found:     {self.stats['total_found']:,}")
        print(f"Duplicates skipped:        {self.stats['duplicates_skipped']:,}")
        print(f"Too small skipped:         {self.stats['too_small_skipped']:,}")
        print(f"Files collected:           {self.stats['collected']:,}")
        print()
        print("📁 By Category:")
        for category, count in sorted(self.stats['by_category'].items(), key=lambda x: -x[1]):
            print(f"   {category:20s} {count:4d} files")
        print()
        
        # Calculate total lines of code
        total_lines = sum(item['code_lines'] for item in self.corpus_index)
        print(f"📝 Total lines of code:    {total_lines:,}")
        print(f"📦 Total size:             {sum(item['size_bytes'] for item in self.corpus_index) / 1024:.1f} KB")
        print()
        
        # Feature distribution
        feature_counts = defaultdict(int)
        for item in self.corpus_index:
            for feature in item['features']:
                feature_counts[feature] += 1
        
        if feature_counts:
            print("🏷️  Feature Distribution:")
            for feature, count in sorted(feature_counts.items(), key=lambda x: -x[1])[:10]:
                print(f"   {feature:20s} {count:4d} files")
        
        print("=" * 70)
    
    def generate_readme(self):
        """Generate README.md for the library."""
        readme_path = self.library_root / "README.md"
        
        readme_content = f"""# KAIN Corpus Library

**Generated:** {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}  
**Total Files:** {self.stats['collected']}  
**Total Lines:** {sum(item['code_lines'] for item in self.corpus_index):,}

## Purpose

This library contains deduplicated, high-quality KAIN source files collected from the entire repository. It serves as:

1. **Training Data** - For future LLM fine-tuning on KAIN syntax
2. **Pattern Library** - For semantic analysis and code generation
3. **Example Repository** - For documentation and learning
4. **Corpus Analysis** - For language feature usage statistics

## Structure

```
kn_library/
├── corpus_index.json       # Metadata database (searchable)
├── actors/                 # Actor definitions with networking
├── shaders/                # Shader code (fragment, compute, etc.)
├── editor/                 # Editor tools (Slate, Details, etc.)
├── components/             # Reusable component definitions
├── datatables/             # DataTable structs
├── utilities/              # Blueprint functions and helpers
└── examples/               # Miscellaneous examples
```

## Statistics

| Category | Files | Description |
|----------|-------|-------------|
"""
        
        for category, count in sorted(self.stats['by_category'].items(), key=lambda x: -x[1]):
            readme_content += f"| {category} | {count} | KAIN {category} code |\n"
        
        readme_content += f"""
## Quality Filters

- **Minimum Lines:** {self.min_lines} lines of actual code
- **Deduplication:** Content-based (SHA256 hash)
- **Encoding:** UTF-8 with error handling

## Usage

### Search by Feature
```python
import json

with open('corpus_index.json') as f:
    index = json.load(f)

# Find all files with networking
networking_files = [
    f for f in index['files'] 
    if 'networking' in f['features']
]
```

### Search by Category
```python
# Find all shader files
shader_files = [
    f for f in index['files']
    if f['category'] == 'shaders'
]
```

### Get File Content
```python
# Read a specific file
file_info = index['files'][0]
with open(file_info['library_path']) as f:
    content = f.read()
```

## Maintenance

To update the corpus:
```bash
python collect_kain_corpus.py
```

To see statistics only:
```bash
python collect_kain_corpus.py --stats-only
```

## Future Enhancements

- [ ] Semantic search via embeddings
- [ ] AST-based similarity detection
- [ ] Feature extraction for ML training
- [ ] Code complexity metrics
- [ ] Dependency graph analysis
- [ ] Pattern mining for common idioms
"""
        
        with open(readme_path, 'w', encoding='utf-8') as f:
            f.write(readme_content)
        
        print(f"📄 Generated README: {readme_path}")


def main():
    import argparse
    
    parser = argparse.ArgumentParser(
        description='KAIN Corpus Collector - Build a deduplicated library of KAIN source files'
    )
    parser.add_argument('--min-lines', type=int, default=10,
                       help='Minimum lines of code (default: 10)')
    parser.add_argument('--stats-only', action='store_true',
                       help='Only print statistics, do not collect files')
    parser.add_argument('--verbose', '-v', action='store_true',
                       help='Verbose output')
    args = parser.parse_args()
    
    # Paths
    script_dir = Path(__file__).parent
    repo_root = script_dir.parent  # Go up one level from kn_library/
    library_root = script_dir
    
    print("=" * 70)
    print("🚀 KAIN Corpus Collector v1")
    print("=" * 70)
    print()
    
    collector = KainCorpusCollector(repo_root, library_root, args.min_lines)
    
    if not args.stats_only:
        collector.scan_repository()
        collector.save_corpus_index()
        collector.generate_readme()
    
    collector.print_statistics()
    
    print()
    print("✅ Collection complete!")
    print()


if __name__ == '__main__':
    main()
