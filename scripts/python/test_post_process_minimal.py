#!/usr/bin/env python3
"""
Test script to verify post-processor works in minimal mode
Run this after removing validation plugins to ensure no breakage
"""

import sys
from pathlib import Path

# Add parent directory to path
sys.path.insert(0, str(Path(__file__).parent))

from post_process import UE5PostProcessor, PostProcessPlugin

def test_plugin_loading():
    """Test that plugins load correctly in minimal mode"""
    print("Testing plugin loading...")
    
    processor = UE5PostProcessor(
        plugin_path=".",
        plugin_name="TestPlugin",
        verbose=True
    )
    
    # Check that we have the expected plugins
    plugin_names = [p.name for p in processor.plugins]
    
    expected_plugins = [
        "ModuleAPIFix",
        "DuplicateForwardDecl",
        "MissingIncludes",
        "DelegateGeneratedH",
        "EmptyLines"
    ]
    
    print(f"\nLoaded plugins: {plugin_names}")
    print(f"Expected plugins: {expected_plugins}")
    
    # Verify no validation plugins
    assert "UE5Validator" not in plugin_names, "❌ UE5Validator should be removed!"
    assert "ValidationRules" not in plugin_names, "❌ ValidationRules should be removed!"
    
    # Verify essential plugins are present
    for expected in expected_plugins:
        assert expected in plugin_names, f"❌ Missing essential plugin: {expected}"
    
    print("\n✅ All checks passed!")
    print(f"✅ Loaded {len(processor.plugins)} plugins (minimal mode)")
    print("✅ No validation plugins present")
    print("✅ All essential plugins loaded")
    
    return True

def test_module_api_fix():
    """Test that ModuleAPIFix plugin works"""
    print("\n\nTesting ModuleAPIFix plugin...")
    
    from post_process import ModuleAPIFixPlugin
    
    plugin = ModuleAPIFixPlugin()
    
    # Test content with GAME_API
    test_content = """
#pragma once

class GAME_API MyActor : public AActor
{
    GENERATED_BODY()
};
"""
    
    context = {
        "plugin_name": "TestPlugin",
        "module_api": "TESTPLUGIN_API"
    }
    
    result, changes = plugin.process_header(test_content, Path("test.h"), context)
    
    assert "GAME_API" not in result, "❌ GAME_API should be replaced!"
    assert "TESTPLUGIN_API" in result, "❌ TESTPLUGIN_API should be present!"
    assert len(changes) > 0, "❌ Should report changes!"
    
    print("✅ ModuleAPIFix works correctly")
    print(f"   Changes: {changes}")
    
    return True

def test_missing_includes():
    """Test that MissingIncludes plugin works"""
    print("\n\nTesting MissingIncludes plugin...")
    
    from post_process import MissingIncludesPlugin
    
    plugin = MissingIncludesPlugin()
    
    # Test content without CoreMinimal.h
    test_content = """#pragma once

class MyActor : public AActor
{
};
"""
    
    context = {"plugin_name": "TestPlugin"}
    
    result, changes = plugin.process_header(test_content, Path("test.h"), context)
    
    assert '#include "CoreMinimal.h"' in result, "❌ CoreMinimal.h should be added!"
    assert len(changes) > 0, "❌ Should report changes!"
    
    print("✅ MissingIncludes works correctly")
    print(f"   Changes: {changes}")
    
    return True

def main():
    """Run all tests"""
    print("=" * 60)
    print("POST-PROCESSOR MINIMAL MODE TEST")
    print("=" * 60)
    
    try:
        test_plugin_loading()
        test_module_api_fix()
        test_missing_includes()
        
        print("\n" + "=" * 60)
        print("✅ ALL TESTS PASSED!")
        print("=" * 60)
        print("\nPost-processor is working correctly in minimal mode.")
        print("Validation has been successfully moved to Oracle.")
        
        return 0
    
    except AssertionError as e:
        print(f"\n❌ TEST FAILED: {e}")
        return 1
    except Exception as e:
        print(f"\n❌ ERROR: {e}")
        import traceback
        traceback.print_exc()
        return 1

if __name__ == "__main__":
    sys.exit(main())
