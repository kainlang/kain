#!/usr/bin/env python3
"""Validate the widget_registry.json file."""

import json
import sys
from pathlib import Path

def main():
    registry_path = Path(__file__).parent.parent / "metadata" / "widget_registry.json"
    
    try:
        with open(registry_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
        
        print(f"✓ JSON is valid")
        print(f"  - Widgets: {len(data.get('widgets', {}))}")
        print(f"  - Delegates: {len(data.get('delegates', {}))}")
        print(f"  - Composition rules: {len(data.get('composition_rules', {}))}")
        print(f"  - Property constraints: {len(data.get('property_constraints', {}))}")
        
        # Check composition rules structure
        comp_rules = data.get('composition_rules', {})
        if comp_rules:
            print(f"\n  Composition rule categories:")
            for category, rules in comp_rules.items():
                print(f"    - {category}: {len(rules)} entries")
        
        return 0
    except json.JSONDecodeError as e:
        print(f"✗ JSON validation failed: {e}")
        return 1
    except Exception as e:
        print(f"✗ Error: {e}")
        return 1

if __name__ == "__main__":
    sys.exit(main())
