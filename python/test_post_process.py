"""Quick test for post-processor"""
import sys
from pathlib import Path

# Add parent dir to path
sys.path.insert(0, str(Path(__file__).parent))

from post_process import UE5PostProcessor

# Test with SlateTest3
plugin_path = Path(__file__).parent.parent.parent / "testing" / "Phase3" / "SlateTest3"
plugin_name = "SlateTest3"

print(f"Testing post-processor on: {plugin_path}")
print(f"Plugin name: {plugin_name}")
print()

processor = UE5PostProcessor(str(plugin_path), plugin_name, verbose=True)
result = processor.process_all()

print()
print("=" * 60)
print("RESULTS:")
print(f"Success: {result['success']}")
print(f"Fixes applied: {result['fixes_applied']}")
print(f"Errors: {len(result['errors'])}")

if result['fixes']:
    print("\nFixes:")
    for fix in result['fixes']:
        print(f"  - {fix}")

if result['errors']:
    print("\nErrors:")
    for error in result['errors']:
        print(f"  - {error}")
