#!/usr/bin/env python3
"""
UE5 Documentation Extractor
Processes 150,000+ HTML files from scraped UE5 documentation into structured JSON.

Output Structure:
    Kain/unreal/extracted_docs/
    ├── blueprint_api_index.json          # Master index
    ├── cpp_api_index.json                # C++ API master index
    ├── types/
    │   ├── actors.json                   # All AActor types
    │   ├── components.json               # All UActorComponent types
    │   ├── structs.json                  # All USTRUCT types
    │   ├── enums.json                    # All UENUM types
    │   └── interfaces.json               # All UInterface types
    ├── functions/
    │   ├── by_category.json              # Grouped by category
    │   └── by_module.json                # Grouped by module
    └── metadata/
        ├── engine_knowledge_expansion.json  # Ready for KAIN Oracle
        └── validation_rules.json            # New Oracle rules

Usage:
    # Blueprint API only
    python extract_ue5_docs.py --input M:/Code/Research/OfficialDocs/BlueprintAPI --output ../extracted_docs --workers 16
    
    # C++ API only
    python extract_ue5_docs.py --input M:/Code/Kain/unreal/UE_API --output ../extracted_docs --workers 16 --api cpp
    
    # Both (run separately, results merge)
    python extract_ue5_docs.py --input M:/Code/Research/OfficialDocs/BlueprintAPI --output ../extracted_docs --workers 16 --api blueprint
    python extract_ue5_docs.py --input M:/Code/Kain/unreal/UE_API --output ../extracted_docs --workers 16 --api cpp
"""

import os
import sys
import json
import argparse
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple
from dataclasses import dataclass, asdict
from concurrent.futures import ProcessPoolExecutor, as_completed
from collections import defaultdict
import re

try:
    from bs4 import BeautifulSoup
    from bs4 import XMLParsedAsHTMLWarning
    import warnings
    # Suppress XML parsing warnings - these files are XHTML but lxml handles them fine
    warnings.filterwarnings("ignore", category=XMLParsedAsHTMLWarning)
except ImportError:
    print("ERROR: BeautifulSoup4 not installed. Run: pip install beautifulsoup4 lxml")
    sys.exit(1)


@dataclass
class UE5Type:
    """Represents a UE5 type (class, struct, enum, interface) - minimal version."""
    name: str
    type_category: str  # "Actor", "Component", "Struct", "Enum", "Interface", "Object"
    description: Optional[str] = None
    category: Optional[str] = None


@dataclass
class UE5Function:
    """Represents a UE5 Blueprint function - minimal version."""
    name: str
    category: str
    description: Optional[str] = None
    parameters: List[Dict[str, str]] = None
    return_type: Optional[str] = None
    
    def __post_init__(self):
        if self.parameters is None:
            self.parameters = []


class UE5DocExtractor:
    """Extracts structured data from UE5 HTML documentation."""
    
    # Type prefixes for classification
    ACTOR_PREFIXES = ['A']
    COMPONENT_PREFIXES = ['U', 'UActorComponent', 'USceneComponent']
    STRUCT_PREFIXES = ['F']
    ENUM_PREFIXES = ['E']
    INTERFACE_PREFIXES = ['I', 'UInterface']
    
    def __init__(self, api_type='blueprint'):
        self.api_type = api_type  # 'blueprint' or 'cpp'
        self.stats = {
            'total_files': 0,
            'processed': 0,
            'failed': 0,
            'types_found': 0,
            'functions_found': 0,
        }
        
    def extract_from_html(self, html_path: Path) -> Optional[Dict]:
        """Extract structured data from a single HTML file."""
        try:
            with open(html_path, 'r', encoding='utf-8', errors='ignore') as f:
                html_content = f.read()
            
            soup = BeautifulSoup(html_content, 'lxml')
            
            # Extract basic metadata
            title = self._extract_title(soup)
            category = self._extract_category(html_path)
            description = self._extract_description(soup)
            
            # Determine if this is a type or function
            is_type = self._is_type_page(soup, title)
            
            if is_type:
                return self._extract_type(soup, title, category, description, html_path)
            else:
                return self._extract_function(soup, title, category, description, html_path)
                
        except Exception as e:
            print(f"ERROR processing {html_path}: {e}")
            self.stats['failed'] += 1
            return None
    
    def _extract_title(self, soup: BeautifulSoup) -> str:
        """Extract page title."""
        # Try h1 first (most reliable)
        h1 = soup.find('h1', id='H1TitleId')
        if h1:
            return h1.get_text(strip=True)
        
        # Try title tag
        title_tag = soup.find('title')
        if title_tag:
            # Remove " | Unreal Engine X.X Documentation" suffix
            title = title_tag.text.strip()
            title = re.sub(r'\s*\|\s*Unreal Engine.*', '', title)
            return title
        
        return "Unknown"
    
    def _extract_category(self, html_path: Path) -> str:
        """Extract category from file path."""
        # Path structure: .../BlueprintAPI/Category/Subcategory/index.html
        # or: .../API/Runtime/Module/Class/index.html
        parts = html_path.parts
        
        if 'BlueprintAPI' in parts:
            idx = parts.index('BlueprintAPI')
            if idx + 1 < len(parts):
                return parts[idx + 1]
        elif 'API' in parts:
            idx = parts.index('API')
            # For C++ API: Runtime/Module or Plugins/PluginName
            if idx + 2 < len(parts):
                return f"{parts[idx + 1]}/{parts[idx + 2]}"
        
        return "Unknown"
    
    def _extract_description(self, soup: BeautifulSoup) -> Optional[str]:
        """Extract description from meta tags or content."""
        # Try meta description first
        meta_desc = soup.find('meta', attrs={'name': 'description'})
        if meta_desc and meta_desc.get('content'):
            desc = meta_desc['content'].strip()
            # Filter out generic descriptions
            if desc and desc != "Add Return Node..." and len(desc) > 10:
                return desc
        
        # Try to find description in content
        # Look for <p> tags in maincol div
        maincol = soup.find('div', id='maincol')
        if maincol:
            # Get first paragraph that's not empty
            for p in maincol.find_all('p', recursive=False):
                text = p.get_text(strip=True)
                if text and len(text) > 20:
                    return text
        
        return None
    
    def _is_type_page(self, soup: BeautifulSoup, title: str) -> bool:
        """Determine if this page describes a type (class/struct/enum) vs a function."""
        # For C++ API, check if it's in ClassHierarchy or has class/struct/enum keywords
        if self.api_type == 'cpp':
            # ClassHierarchy page is a special index, not a type
            if 'Class Hierarchy' in title:
                return False
            # Check for type prefixes
            if any(title.startswith(prefix) for prefix in ['A', 'U', 'F', 'E', 'I', 'T']):
                return True
        
        # For Blueprint API, check for type prefixes
        if any(title.startswith(prefix) for prefix in ['A', 'U', 'F', 'E', 'I']):
            return True
        
        # Check for class/struct/enum keywords in content
        content = soup.get_text().lower()
        if any(keyword in content[:1000] for keyword in ['class ', 'struct ', 'enum ', 'interface ']):
            return True
        
        return False
    
    def _classify_type(self, name: str, content: str) -> str:
        """Classify type as Actor, Component, Struct, Enum, Interface, or Object."""
        if name.startswith('A'):
            return "Actor"
        elif name.startswith('F'):
            return "Struct"
        elif name.startswith('E'):
            return "Enum"
        elif name.startswith('I') or 'interface' in content.lower():
            return "Interface"
        elif 'component' in content.lower() or 'UActorComponent' in content:
            return "Component"
        elif name.startswith('U'):
            return "Object"
        elif name.startswith('T'):
            return "Template"
        return "Unknown"
    
    def _extract_type(self, soup: BeautifulSoup, title: str, category: str, 
                     description: Optional[str], html_path: Path) -> Dict:
        """Extract type information - minimal version."""
        content = soup.get_text()
        type_category = self._classify_type(title, content)
        
        ue_type = UE5Type(
            name=title,
            type_category=type_category,
            description=description,
            category=category
        )
        
        self.stats['types_found'] += 1
        return {'type': 'class', 'data': asdict(ue_type)}
    
    def _extract_function(self, soup: BeautifulSoup, title: str, category: str,
                         description: Optional[str], html_path: Path) -> Dict:
        """Extract function information - minimal version."""
        # Extract parameters (if available)
        parameters = self._extract_parameters(soup)
        
        # Extract return type
        return_type = self._extract_return_type(soup)
        
        ue_function = UE5Function(
            name=title,
            category=category,
            description=description,
            parameters=parameters,
            return_type=return_type
        )
        
        self.stats['functions_found'] += 1
        return {'type': 'function', 'data': asdict(ue_function)}
    
    def _extract_parameters(self, soup: BeautifulSoup) -> List[Dict[str, str]]:
        """Extract function parameters from HTML structure."""
        parameters = []
        
        # Look for inputs section
        inputs_div = soup.find('div', id='inputs')
        if inputs_div:
            table = inputs_div.find('table')
            if table:
                for row in table.find_all('tr', class_='normal-row'):
                    param_name_cell = row.find('td', class_='name-cell')
                    param_desc_cell = row.find('td', class_='desc-cell')
                    
                    if param_name_cell:
                        # Extract name
                        name_link = param_name_cell.find('a')
                        param_name = name_link.get_text(strip=True) if name_link else ""
                        
                        # Extract type from arguments div
                        args_div = param_name_cell.find('div', class_='name-cell-arguments')
                        param_type = args_div.get_text(strip=True) if args_div else ""
                        
                        # Extract description
                        param_desc = param_desc_cell.get_text(strip=True) if param_desc_cell else ""
                        
                        if param_name:
                            parameters.append({
                                'name': param_name,
                                'type': param_type,
                                'description': param_desc
                            })
        
        return parameters
    
    def _extract_return_type(self, soup: BeautifulSoup) -> Optional[str]:
        """Extract function return type from outputs section."""
        outputs_div = soup.find('div', id='outputs')
        if outputs_div:
            table = outputs_div.find('table')
            if table:
                for row in table.find_all('tr', class_='normal-row'):
                    param_name_cell = row.find('td', class_='name-cell')
                    if param_name_cell:
                        args_div = param_name_cell.find('div', class_='name-cell-arguments')
                        if args_div:
                            return args_div.get_text(strip=True)
        return None


def process_file(args: Tuple[Path, int, int, str]) -> Optional[Dict]:
    """Process a single HTML file (for multiprocessing)."""
    html_path, file_num, total_files, api_type = args
    
    if file_num % 1000 == 0:
        print(f"Processing {file_num}/{total_files}: {html_path.name}")
    
    extractor = UE5DocExtractor(api_type=api_type)
    return extractor.extract_from_html(html_path)


def main():
    parser = argparse.ArgumentParser(description='Extract structured data from UE5 documentation')
    parser.add_argument('--input', required=True, help='Input directory (BlueprintAPI or UE_API folder)')
    parser.add_argument('--output', required=True, help='Output directory for extracted JSON')
    parser.add_argument('--workers', type=int, default=8, help='Number of parallel workers')
    parser.add_argument('--api', choices=['blueprint', 'cpp'], default='blueprint', help='API type to extract')
    parser.add_argument('--dry-run', action='store_true', help='Count files without processing')
    
    args = parser.parse_args()
    
    input_dir = Path(args.input)
    output_dir = Path(args.output)
    
    if not input_dir.exists():
        print(f"ERROR: Input directory not found: {input_dir}")
        sys.exit(1)
    
    # Find all index.html files
    print(f"Scanning for HTML files in {input_dir}...")
    html_files = list(input_dir.rglob('index.html'))
    total_files = len(html_files)
    print(f"Found {total_files:,} HTML files")
    
    if args.dry_run:
        print("Dry run complete.")
        return
    
    # Create output directory structure
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / 'types').mkdir(exist_ok=True)
    (output_dir / 'functions').mkdir(exist_ok=True)
    (output_dir / 'metadata').mkdir(exist_ok=True)
    
    # Process files in parallel
    print(f"\nProcessing with {args.workers} workers...")
    
    types_by_category = defaultdict(list)
    functions_by_category = defaultdict(list)
    functions_by_module = defaultdict(list)
    all_types = []
    all_functions = []
    
    processed = 0
    failed = 0
    
    with ProcessPoolExecutor(max_workers=args.workers) as executor:
        # Submit all tasks
        futures = {
            executor.submit(process_file, (html_path, i+1, total_files, args.api)): html_path 
            for i, html_path in enumerate(html_files)
        }
        
        # Collect results
        for future in as_completed(futures):
            result = future.result()
            processed += 1
            
            if result is None:
                failed += 1
                continue
            
            if result['type'] == 'class':
                data = result['data']
                all_types.append(data)
                types_by_category[data['type_category']].append(data)
            elif result['type'] == 'function':
                data = result['data']
                all_functions.append(data)
                functions_by_category[data['category']].append(data)
            
            if processed % 5000 == 0:
                print(f"Progress: {processed}/{total_files} ({processed/total_files*100:.1f}%)")
    
    print(f"\nProcessing complete!")
    print(f"  Processed: {processed:,}")
    print(f"  Failed: {failed:,}")
    print(f"  Types found: {len(all_types):,}")
    print(f"  Functions found: {len(all_functions):,}")
    
    # Write output files
    print("\nWriting output files...")
    
    # Master index
    index_filename = f"{args.api}_api_index.json"
    index = {
        'api_type': args.api,
        'total_types': len(all_types),
        'total_functions': len(all_functions),
        'types_by_category': {k: len(v) for k, v in types_by_category.items()},
        'functions_by_category': {k: len(v) for k, v in functions_by_category.items()},
        'functions_by_module': {k: len(v) for k, v in functions_by_module.items()},
        'source_directory': str(input_dir),
    }
    with open(output_dir / index_filename, 'w', encoding='utf-8') as f:
        json.dump(index, f, indent=2)
    print(f"  ✅ {index_filename}")
    
    # Types by category
    for category, types_list in types_by_category.items():
        filename = f"{category.lower()}s.json"
        with open(output_dir / 'types' / filename, 'w', encoding='utf-8') as f:
            json.dump(types_list, f, indent=2)
        print(f"  ✅ types/{filename} ({len(types_list):,} entries)")
    
    # Functions by category
    with open(output_dir / 'functions' / 'by_category.json', 'w', encoding='utf-8') as f:
        json.dump(functions_by_category, f, indent=2)
    print(f"  ✅ functions/by_category.json ({len(functions_by_category)} categories)")
    
    # Functions by module
    if functions_by_module:
        with open(output_dir / 'functions' / 'by_module.json', 'w', encoding='utf-8') as f:
            json.dump(functions_by_module, f, indent=2)
        print(f"  ✅ functions/by_module.json ({len(functions_by_module)} modules)")
    
    # Generate engine_knowledge_expansion.json for KAIN Oracle
    print("\nGenerating engine_knowledge_expansion.json for KAIN Oracle...")
    engine_knowledge = {
        'classes': sorted(list(set([t['name'] for t in all_types if t['type_category'] in ['Actor', 'Object', 'Component']]))),
        'structs': sorted(list(set([t['name'] for t in all_types if t['type_category'] == 'Struct']))),
        'enums': sorted(list(set([t['name'] for t in all_types if t['type_category'] == 'Enum']))),
        'interfaces': sorted(list(set([t['name'] for t in all_types if t['type_category'] == 'Interface']))),
        'source': args.api,
    }
    
    expansion_file = output_dir / 'metadata' / f'engine_knowledge_expansion_{args.api}.json'
    with open(expansion_file, 'w', encoding='utf-8') as f:
        json.dump(engine_knowledge, f, indent=2)
    print(f"  ✅ {expansion_file.name}")
    print(f"     - Classes: {len(engine_knowledge['classes']):,}")
    print(f"     - Structs: {len(engine_knowledge['structs']):,}")
    print(f"     - Enums: {len(engine_knowledge['enums']):,}")
    print(f"     - Interfaces: {len(engine_knowledge['interfaces']):,}")
    
    print(f"\n✅ Extraction complete! Output written to: {output_dir}")
    print(f"\nNext steps:")
    print(f"  1. Review: {output_dir / index_filename}")
    print(f"  2. Merge: {expansion_file} into Kain/unreal/metadata/engine_knowledge.json")
    print(f"  3. Update KAIN Oracle to use expanded type database")


if __name__ == '__main__':
    main()
