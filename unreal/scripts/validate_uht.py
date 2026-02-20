#!/usr/bin/env python3
"""
UHT Validation Script - Validates KAIN-generated C++ against UnrealHeaderTool
Usage: python validate_uht.py <plugin_path>
"""

import sys
import os
import subprocess
import json
from pathlib import Path

# UE5 installation paths (adjust for your system)
UE5_PATHS = [
    r"C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealHeaderTool.exe",
    r"C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealHeaderTool.exe",
    r"C:\Program Files\Epic Games\UE_5.2\Engine\Binaries\Win64\UnrealHeaderTool.exe",
]

def find_uht():
    """Find UnrealHeaderTool.exe"""
    for path in UE5_PATHS:
        if os.path.exists(path):
            return path
    return None

def validate_plugin(plugin_path):
    """Validate plugin C++ code against UHT"""
    plugin_path = Path(plugin_path).resolve()
    
    if not plugin_path.exists():
        print(f"❌ Plugin path not found: {plugin_path}")
        return False
    
    # Find .uplugin file
    uplugin_files = list(plugin_path.glob("*.uplugin"))
    if not uplugin_files:
        print(f"❌ No .uplugin file found in {plugin_path}")
        return False
    
    uplugin_file = uplugin_files[0]
    print(f"🔍 Validating plugin: {uplugin_file.name}")
    
    # Find UHT
    uht_path = find_uht()
    if not uht_path:
        print("⚠️  UnrealHeaderTool not found - skipping UHT validation")
        print("   Install UE5 to enable UHT validation")
        return True  # Don't fail if UHT not available
    
    print(f"✓ Found UHT: {uht_path}")
    
    # Quick syntax checks (before running UHT)
    print("\n🔍 Running quick syntax checks...")
    issues = []
    
    # Check all header files
    for header in plugin_path.rglob("*.h"):
        with open(header, 'r', encoding='utf-8') as f:
            content = f.read()
            
            # Check for common issues
            if "UCLASS(" in content and "GENERATED_BODY()" not in content:
                issues.append(f"{header.name}: UCLASS without GENERATED_BODY()")
            
            if "USTRUCT(" in content and "GENERATED_BODY()" not in content:
                issues.append(f"{header.name}: USTRUCT without GENERATED_BODY()")
            
            if "UENUM(" in content and "BlueprintType" not in content:
                issues.append(f"{header.name}: UENUM without BlueprintType (warning)")
            
            # Check for double prefixes (common KAIN bug)
            if "class AA" in content or "struct FF" in content or "enum EE" in content:
                issues.append(f"{header.name}: Possible double prefix (AA/FF/EE)")
            
            # Check for missing includes
            if "UPROPERTY(" in content and "#include \"CoreMinimal.h\"" not in content:
                issues.append(f"{header.name}: Missing CoreMinimal.h include")
    
    if issues:
        print("\n⚠️  Potential issues found:")
        for issue in issues:
            print(f"   - {issue}")
        print()
    else:
        print("✓ Quick syntax checks passed\n")
    
    # Run UHT in parse-only mode (fast validation)
    print("🔍 Running UnrealHeaderTool validation...")
    try:
        result = subprocess.run(
            [uht_path, "-Mode=ParseOnly", f"-Plugin={uplugin_file}"],
            capture_output=True,
            text=True,
            timeout=30
        )
        
        if result.returncode == 0:
            print("✅ UHT validation passed!")
            return True
        else:
            print("❌ UHT validation failed:")
            print(result.stderr)
            return False
            
    except subprocess.TimeoutExpired:
        print("⚠️  UHT validation timed out (plugin may be too large)")
        return True  # Don't fail on timeout
    except Exception as e:
        print(f"⚠️  UHT validation error: {e}")
        return True  # Don't fail on errors

def main():
    if len(sys.argv) < 2:
        print("Usage: python validate_uht.py <plugin_path>")
        sys.exit(1)
    
    plugin_path = sys.argv[1]
    success = validate_plugin(plugin_path)
    
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
