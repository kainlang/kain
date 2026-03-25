#!/usr/bin/env python3
"""
Verify UE5 paths configuration file.

This script checks:
- Config file exists and is valid JSON
- At least one enabled installation exists
- Paths are accessible
- Output directory is writable
"""

import json
import os
import sys
from pathlib import Path


def verify_config(config_path="ue5_paths_config.json"):
    """Verify the UE5 paths configuration file."""
    
    print("=" * 60)
    print("KAIN Metadata Configuration Verification")
    print("=" * 60)
    print()
    
    # Check if config file exists
    if not os.path.exists(config_path):
        print(f"❌ ERROR: Config file not found: {config_path}")
        print()
        print("Please create ue5_paths_config.json with your UE5 installation paths.")
        print("See README.md for examples.")
        return False
    
    print(f"✓ Config file found: {config_path}")
    
    # Load and parse JSON
    try:
        with open(config_path, 'r') as f:
            config = json.load(f)
    except json.JSONDecodeError as e:
        print(f"❌ ERROR: Invalid JSON in config file: {e}")
        return False
    
    print("✓ Config file is valid JSON")
    
    # Check required fields
    if "installations" not in config:
        print("❌ ERROR: Config missing 'installations' field")
        return False
    
    installations = config["installations"]
    if not isinstance(installations, list) or len(installations) == 0:
        print("❌ ERROR: 'installations' must be a non-empty array")
        return False
    
    print(f"✓ Found {len(installations)} installation(s) in config")
    print()
    
    # Check each installation
    enabled_count = 0
    valid_count = 0
    
    for i, install in enumerate(installations):
        version = install.get("version", f"unknown-{i}")
        enabled = install.get("enabled", True)
        paths = install.get("paths", [])
        
        print(f"Installation {i+1}: UE5 {version}")
        print(f"  Enabled: {enabled}")
        
        if not enabled:
            print(f"  [SKIP] Installation disabled")
            print()
            continue
        
        enabled_count += 1
        
        if not paths:
            print(f"  ❌ ERROR: No paths specified")
            print()
            continue
        
        print(f"  Paths: {len(paths)} configured")
        
        # Try each path
        found_valid = False
        for j, path in enumerate(paths):
            path_obj = Path(path)
            exists = path_obj.exists()
            is_dir = path_obj.is_dir() if exists else False
            
            status = "✓" if (exists and is_dir) else "✗"
            print(f"    {status} Path {j+1}: {path}")
            
            if exists and is_dir:
                found_valid = True
                # Check if it looks like a UE5 source directory
                has_runtime = (path_obj / "Runtime").exists()
                has_editor = (path_obj / "Editor").exists()
                
                if has_runtime and has_editor:
                    print(f"      ✓ Valid UE5 source directory")
                else:
                    print(f"      ⚠ Warning: Doesn't look like UE5 source (missing Runtime/Editor)")
        
        if found_valid:
            valid_count += 1
            print(f"  ✓ At least one valid path found")
        else:
            print(f"  ❌ ERROR: No valid paths found for this installation")
        
        print()
    
    # Check output directory
    output_dir = config.get("output_directory", "../metadata")
    output_path = Path(output_dir)
    
    print(f"Output directory: {output_dir}")
    
    if not output_path.exists():
        print(f"  ⚠ Warning: Output directory doesn't exist, will be created")
        try:
            output_path.mkdir(parents=True, exist_ok=True)
            print(f"  ✓ Created output directory")
        except Exception as e:
            print(f"  ❌ ERROR: Cannot create output directory: {e}")
            return False
    else:
        print(f"  ✓ Output directory exists")
    
    # Check if writable
    test_file = output_path / ".write_test"
    try:
        test_file.touch()
        test_file.unlink()
        print(f"  ✓ Output directory is writable")
    except Exception as e:
        print(f"  ❌ ERROR: Output directory is not writable: {e}")
        return False
    
    print()
    print("=" * 60)
    print("Summary")
    print("=" * 60)
    print(f"Total installations: {len(installations)}")
    print(f"Enabled installations: {enabled_count}")
    print(f"Valid installations: {valid_count}")
    print()
    
    if valid_count == 0:
        print("❌ FAILED: No valid UE5 installations found")
        print()
        print("Please update ue5_paths_config.json with your actual UE5 installation paths.")
        print("Paths should point to the Engine/Source directory.")
        print()
        print("Example:")
        print('  "paths": [')
        print('    "C:/Program Files/Epic Games/UE_5.7/Engine/Source",')
        print('    "D:/UE_5.7/Engine/Source"')
        print('  ]')
        return False
    
    if valid_count < enabled_count:
        print("⚠ WARNING: Some enabled installations have no valid paths")
        print("Extraction will skip these installations.")
        print()
    
    print("✓ Configuration is valid and ready to use")
    print()
    print("Next steps:")
    print("  1. Run: refresh_all_metadata.bat (or .sh on Linux/Mac)")
    print("  2. Or run individual scripts with: python ue5_scanner.py --config ue5_paths_config.json")
    print()
    
    return True


if __name__ == "__main__":
    success = verify_config()
    sys.exit(0 if success else 1)
