"""
Example: How to add custom post-processing plugins

This shows how to extend the post-processor with your own fixes
"""

from pathlib import Path
from typing import Dict, List, Tuple
from post_process import PostProcessPlugin, UE5PostProcessor


# Example 1: Simple text replacement
class MyCustomFixPlugin(PostProcessPlugin):
    """Replace old API with new API"""
    
    def __init__(self):
        super().__init__("MyCustomFix", priority=25)
    
    def process_header(self, content: str, file_path: Path, context: Dict) -> Tuple[str, List[str]]:
        changes = []
        
        if "OldAPIName" in content:
            content = content.replace("OldAPIName", "NewAPIName")
            changes.append(f"{file_path.name}: Replaced OldAPIName with NewAPIName")
        
        return content, changes


# Example 2: Pattern-based fix
class FixBlueprintCallablePlugin(PostProcessPlugin):
    """Ensure all public functions have BlueprintCallable"""
    
    def __init__(self):
        super().__init__("FixBlueprintCallable", priority=35)
    
    def process_header(self, content: str, file_path: Path, context: Dict) -> Tuple[str, List[str]]:
        import re
        changes = []
        
        # Find public functions without UFUNCTION
        pattern = r'public:\s*\n\s*(\w+\s+\w+\([^)]*\))'
        matches = re.findall(pattern, content)
        
        for match in matches:
            if 'UFUNCTION' not in content[:content.find(match)]:
                # Add UFUNCTION(BlueprintCallable)
                content = content.replace(
                    match,
                    f'UFUNCTION(BlueprintCallable)\n\t{match}'
                )
                changes.append(f"{file_path.name}: Added BlueprintCallable to {match.split('(')[0]}")
        
        return content, changes


# Example 3: Validation plugin
class CheckNamingConventionPlugin(PostProcessPlugin):
    """Validate UE5 naming conventions"""
    
    def __init__(self):
        super().__init__("CheckNamingConvention", priority=5)
    
    def validate(self, content: str, file_path: Path, context: Dict) -> List[Dict]:
        issues = []
        
        # Check for lowercase class names
        if 'class ' in content:
            import re
            classes = re.findall(r'class\s+(\w+)', content)
            for cls in classes:
                if cls[0].islower():
                    issues.append({
                        'severity': 'warning',
                        'message': f"Class '{cls}' should start with uppercase"
                    })
        
        return issues


# Example 4: Context-aware fix
class ProjectSpecificFixPlugin(PostProcessPlugin):
    """Fix project-specific issues"""
    
    def __init__(self):
        super().__init__("ProjectSpecificFix", priority=60)
    
    def process_header(self, content: str, file_path: Path, context: Dict) -> Tuple[str, List[str]]:
        changes = []
        plugin_name = context["plugin_name"]
        
        # Example: Add project-specific includes
        if "MyProjectType" in content and f'#include "{plugin_name}Types.h"' not in content:
            # Add include after CoreMinimal
            content = content.replace(
                '#include "CoreMinimal.h"',
                f'#include "CoreMinimal.h"\n#include "{plugin_name}Types.h"'
            )
            changes.append(f"{file_path.name}: Added {plugin_name}Types.h include")
        
        return content, changes


# How to use custom plugins:
if __name__ == "__main__":
    # Create processor
    processor = UE5PostProcessor("path/to/plugin", "MyPlugin", verbose=True)
    
    # Register custom plugins
    processor.register_plugin(MyCustomFixPlugin())
    processor.register_plugin(FixBlueprintCallablePlugin())
    processor.register_plugin(CheckNamingConventionPlugin())
    processor.register_plugin(ProjectSpecificFixPlugin())
    
    # Run processing
    result = processor.process_all()
    
    print(f"Success: {result['success']}")
    print(f"Fixes: {result['fixes_applied']}")
    print(f"Warnings: {len(result.get('warnings', []))}")
    print(f"Errors: {len(result['errors'])}")
