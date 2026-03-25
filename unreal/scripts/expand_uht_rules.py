#!/usr/bin/env python3
"""
Expand uht_rules.json with additional validation rules, attribute compatibility rules,
and replication rules from uht_rules_expansion.json.

This script merges the expansion file into the main uht_rules.json file, adding new
rules while preserving existing ones.
"""

import json
import sys
from pathlib import Path
from typing import Dict, List, Any

def load_json(filepath: Path) -> Dict[str, Any]:
    """Load JSON file with error handling."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            return json.load(f)
    except FileNotFoundError:
        print(f"Error: File not found: {filepath}")
        sys.exit(1)
    except json.JSONDecodeError as e:
        print(f"Error: Invalid JSON in {filepath}: {e}")
        sys.exit(1)

def save_json(filepath: Path, data: Dict[str, Any]) -> None:
    """Save JSON file with pretty formatting."""
    with open(filepath, 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=2, ensure_ascii=False)
    print(f"✓ Saved: {filepath}")

def merge_validation_rules(base_rules: List[Dict], new_rules: List[Dict]) -> List[Dict]:
    """Merge validation rules, avoiding duplicates based on message."""
    existing_messages = {rule['message'] for rule in base_rules}
    merged = base_rules.copy()
    
    added_count = 0
    for rule in new_rules:
        if rule['message'] not in existing_messages:
            merged.append(rule)
            added_count += 1
    
    print(f"  Added {added_count} new validation rules")
    return merged

def merge_incompatible_combos(base_combos: List[Dict], new_combos: List[Dict]) -> List[Dict]:
    """Merge incompatible combinations, avoiding duplicates."""
    existing = {
        (combo['specifier_a'], combo.get('specifier_b'))
        for combo in base_combos
    }
    merged = base_combos.copy()
    
    added_count = 0
    for combo in new_combos:
        key = (combo['specifier_a'], combo.get('specifier_b'))
        if key not in existing:
            merged.append(combo)
            added_count += 1
    
    print(f"  Added {added_count} new incompatible combinations")
    return merged

def expand_uht_rules(base_path: Path, expansion_path: Path, output_path: Path) -> None:
    """Expand uht_rules.json with additional rules from expansion file."""
    print("Loading files...")
    base_data = load_json(base_path)
    expansion_data = load_json(expansion_path)
    
    print("\nMerging validation rules...")
    if 'additional_validation_rules' in expansion_data:
        base_data['validation_rules'] = merge_validation_rules(
            base_data.get('validation_rules', []),
            expansion_data['additional_validation_rules']
        )
    
    print("\nMerging incompatible combinations...")
    if 'additional_incompatible_combos' in expansion_data:
        base_data['incompatible_combos'] = merge_incompatible_combos(
            base_data.get('incompatible_combos', []),
            expansion_data['additional_incompatible_combos']
        )
    
    print("\nAdding new sections...")
    # Add replication rules section
    if 'replication_rules' in expansion_data:
        base_data['replication_rules'] = expansion_data['replication_rules']
        print("  Added replication_rules section")
    
    # Add attribute compatibility matrix
    if 'attribute_compatibility_matrix' in expansion_data:
        base_data['attribute_compatibility_matrix'] = expansion_data['attribute_compatibility_matrix']
        print("  Added attribute_compatibility_matrix section")
    
    # Add KAIN-specific rules
    if 'kain_specific_rules' in expansion_data:
        base_data['kain_specific_rules'] = expansion_data['kain_specific_rules']
        print("  Added kain_specific_rules section")
    
    # Update metadata
    print("\nUpdating metadata...")
    if '_meta' in base_data:
        base_data['_meta']['total_rules'] = len(base_data.get('validation_rules', []))
        base_data['_meta']['total_incompatible_combos'] = len(base_data.get('incompatible_combos', []))
        base_data['_meta']['expanded'] = True
        base_data['_meta']['expansion_version'] = expansion_data.get('_meta', {}).get('version', '1.0.0')
        print(f"  Total rules: {base_data['_meta']['total_rules']}")
        print(f"  Total incompatible combos: {base_data['_meta']['total_incompatible_combos']}")
    
    print(f"\nSaving expanded file to: {output_path}")
    save_json(output_path, base_data)
    
    print("\n✓ Expansion complete!")
    print(f"\nSummary:")
    print(f"  Validation rules: {len(base_data.get('validation_rules', []))}")
    print(f"  Specifiers: {len(base_data.get('specifiers', []))}")
    print(f"  Property types: {len(base_data.get('property_types', []))}")
    print(f"  Incompatible combos: {len(base_data.get('incompatible_combos', []))}")
    print(f"  Replication rules: {'Yes' if 'replication_rules' in base_data else 'No'}")
    print(f"  Attribute compatibility: {'Yes' if 'attribute_compatibility_matrix' in base_data else 'No'}")
    print(f"  KAIN-specific rules: {'Yes' if 'kain_specific_rules' in base_data else 'No'}")

def main():
    """Main entry point."""
    # Determine paths relative to script location
    script_dir = Path(__file__).parent
    metadata_dir = script_dir.parent / 'metadata'
    
    base_path = metadata_dir / 'uht_rules.json'
    expansion_path = metadata_dir / 'uht_rules_expansion.json'
    output_path = metadata_dir / 'uht_rules.json'
    
    # Allow command-line override
    if len(sys.argv) > 1:
        base_path = Path(sys.argv[1])
    if len(sys.argv) > 2:
        expansion_path = Path(sys.argv[2])
    if len(sys.argv) > 3:
        output_path = Path(sys.argv[3])
    
    print("=" * 70)
    print("UHT Rules Expansion Tool")
    print("=" * 70)
    print(f"Base file: {base_path}")
    print(f"Expansion file: {expansion_path}")
    print(f"Output file: {output_path}")
    print("=" * 70)
    
    expand_uht_rules(base_path, expansion_path, output_path)

if __name__ == '__main__':
    main()
