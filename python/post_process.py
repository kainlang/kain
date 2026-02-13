"""
KAIN UE5 Post-Processor - The Auto-Healer Hub (Minimal Mode)
Runs after Rust codegen to fix edge cases and ensure production-ready output

⚠️  IMPORTANT: This is now a MINIMAL post-processor!
    Most validation has been moved to Oracle (kain/crates/ue5/src/ue5/oracle.rs)
    which runs BEFORE C++ generation with structured error codes.

This hub now focuses on:
- Safety net fixes (module API, missing includes)
- Code cleanup (duplicate declarations, empty lines)
- Formatting (not validation)

REMOVED PLUGINS (now in Oracle):
- UE5ValidatorPlugin: Oracle validates header syntax before codegen
- ValidationRulesPlugin: Oracle validates all rules with structured error codes

This is the HUB - it orchestrates remaining post-processing plugins:
- validation_rules.py - DEPRECATED (use Oracle instead)
- ue5_validator.py - DEPRECATED (use Oracle instead)
- header_fixer.py - Header auto-fixes (from mcp/utils)
- Custom plugins can be added dynamically
"""

import sys
import json
from pathlib import Path
from typing import Dict, List, Tuple, Callable
import re

# Import our existing tools
try:
    from validation_rules import RuleRegistry, ValidationIssue
    HAS_VALIDATION_RULES = True
except ImportError:
    HAS_VALIDATION_RULES = False
    print("[PostProcess] Warning: validation_rules.py not found")

try:
    from ue5_validator import UE5Validator
    HAS_UE5_VALIDATOR = True
except ImportError:
    HAS_UE5_VALIDATOR = False
    print("[PostProcess] Warning: ue5_validator.py not found")

try:
    import sys
    sys.path.insert(0, str(Path(__file__).parent.parent / "mcp" / "utils"))
    from header_fixer import HeaderFixer
    HAS_HEADER_FIXER = True
except ImportError:
    HAS_HEADER_FIXER = False
    print("[PostProcess] Warning: header_fixer.py not found")


class PostProcessPlugin:
    """Base class for post-processing plugins"""
    
    def __init__(self, name: str, priority: int = 50):
        self.name = name
        self.priority = priority  # Lower = runs first
    
    def process_header(self, content: str, file_path: Path, context: Dict) -> Tuple[str, List[str]]:
        """Process a header file. Return (modified_content, list_of_changes)"""
        return content, []
    
    def process_source(self, content: str, file_path: Path, context: Dict) -> Tuple[str, List[str]]:
        """Process a source file. Return (modified_content, list_of_changes)"""
        return content, []
    
    def validate(self, content: str, file_path: Path, context: Dict) -> List[Dict]:
        """Validate file and return list of issues"""
        return []


class UE5PostProcessor:
    """Post-processes generated UE5 C++ code with auto-fixes"""
    
    def __init__(self, plugin_path: str, plugin_name: str, verbose: bool = False):
        self.plugin_path = Path(plugin_path)
        self.plugin_name = plugin_name
        self.verbose = verbose
        self.fixes_applied = []
        self.errors = []
        self.warnings = []
        
        # Plugin registry
        self.plugins: List[PostProcessPlugin] = []
        
        # Context shared across all plugins
        self.context = {
            "plugin_name": plugin_name,
            "plugin_path": self.plugin_path,
            "module_api": f"{plugin_name.upper()}_API"
        }
        
        # Load plugins
        self._load_plugins()
    
    def _load_plugins(self):
        """Load essential post-processing plugins (validation moved to Oracle)"""
        # ESSENTIAL PLUGINS - Safety nets and cleanup only
        self.plugins.append(ModuleAPIFixPlugin())
        self.plugins.append(DuplicateForwardDeclPlugin())
        self.plugins.append(MissingIncludesPlugin())
        self.plugins.append(DelegateGeneratedHPlugin())
        self.plugins.append(EmptyLinesPlugin())
        
        # Advanced plugins (if available)
        if HAS_HEADER_FIXER:
            self.plugins.append(HeaderFixerPlugin())
        
        # REMOVED: Validation plugins (now handled by Oracle in Rust)
        # - UE5ValidatorPlugin: Oracle validates header syntax before codegen
        # - ValidationRulesPlugin: Oracle validates all rules with structured error codes
        # These were causing redundant validation passes and potential conflicts
        
        # Sort by priority
        self.plugins.sort(key=lambda p: p.priority)
        
        self.log(f"Loaded {len(self.plugins)} post-processing plugins (minimal mode)")
        for plugin in self.plugins:
            self.log(f"  - {plugin.name} (priority: {plugin.priority})")
    
    def register_plugin(self, plugin: PostProcessPlugin):
        """Register a custom plugin"""
        self.plugins.append(plugin)
        self.plugins.sort(key=lambda p: p.priority)
        self.log(f"Registered plugin: {plugin.name}")
    
    def log(self, msg: str):
        """Log message if verbose"""
        if self.verbose:
            print(f"[PostProcess] {msg}")
    
    def process_all(self) -> Dict:
        """Run all post-processing steps"""
        self.log(f"Processing plugin: {self.plugin_name}")
        
        # Find all generated files
        source_dir = self.plugin_path / "Source"
        if not source_dir.exists():
            return {"success": False, "error": "Source directory not found"}
        
        headers = list(source_dir.rglob("*.h"))
        sources = list(source_dir.rglob("*.cpp"))
        
        self.log(f"Found {len(headers)} headers, {len(sources)} source files")
        
        # Phase 1: Validation (detect issues)
        self.log("Phase 1: Validation")
        for header in headers:
            self.validate_file(header)
        for source in sources:
            self.validate_file(source)
        
        # Phase 2: Auto-fix (apply fixes)
        self.log("Phase 2: Auto-fixing")
        for header in headers:
            self.process_file(header, is_header=True)
        for source in sources:
            self.process_file(source, is_header=False)
        
        return {
            "success": len(self.errors) == 0,
            "fixes_applied": len(self.fixes_applied),
            "fixes": self.fixes_applied,
            "warnings": self.warnings,
            "errors": self.errors
        }
    
    def validate_file(self, file_path: Path):
        """Run validation plugins on a file"""
        try:
            content = file_path.read_text(encoding='utf-8')
            
            for plugin in self.plugins:
                issues = plugin.validate(content, file_path, self.context)
                for issue in issues:
                    severity = issue.get('severity', 'warning')
                    message = f"{file_path.name}: {issue.get('message', 'Unknown issue')}"
                    
                    if severity == 'error':
                        self.errors.append(message)
                    else:
                        self.warnings.append(message)
        
        except Exception as e:
            self.errors.append(f"Error validating {file_path.name}: {e}")
    
    def process_file(self, file_path: Path, is_header: bool):
        """Process a file through all plugins"""
        try:
            content = file_path.read_text(encoding='utf-8')
            original = content
            
            # Run through all plugins
            for plugin in self.plugins:
                if is_header:
                    content, changes = plugin.process_header(content, file_path, self.context)
                else:
                    content, changes = plugin.process_source(content, file_path, self.context)
                
                self.fixes_applied.extend(changes)
            
            # Write back if changed
            if content != original:
                file_path.write_text(content, encoding='utf-8')
                self.log(f"Modified: {file_path.name}")
        
        except Exception as e:
            self.errors.append(f"Error processing {file_path.name}: {e}")
    
    def log(self, msg: str):
        """Log message if verbose"""
        if self.verbose:
            print(f"[PostProcess] {msg}")


# ============================================================================
# BUILT-IN PLUGINS
# ============================================================================

class ModuleAPIFixPlugin(PostProcessPlugin):
    """Fix GAME_API -> PLUGINNAME_API"""
    
    def __init__(self):
        super().__init__("ModuleAPIFix", priority=10)
    
    def process_header(self, content: str, file_path: Path, context: Dict) -> Tuple[str, List[str]]:
        changes = []
        module_api = context["module_api"]
        
        if "GAME_API" in content:
            content = content.replace("GAME_API", module_api)
            changes.append(f"{file_path.name}: Fixed module API macro (GAME_API → {module_api})")
        
        return content, changes


class DuplicateForwardDeclPlugin(PostProcessPlugin):
    """Remove duplicate forward declarations"""
    
    def __init__(self):
        super().__init__("DuplicateForwardDecl", priority=20)
    
    def process_header(self, content: str, file_path: Path, context: Dict) -> Tuple[str, List[str]]:
        lines = content.split('\n')
        seen_decls = set()
        new_lines = []
        removed = 0
        
        for line in lines:
            stripped = line.strip()
            
            # Check if it's a forward declaration
            if (stripped.startswith('class ') or 
                stripped.startswith('struct ') or 
                stripped.startswith('enum class ')) and stripped.endswith(';'):
                
                if stripped in seen_decls:
                    removed += 1
                    continue
                seen_decls.add(stripped)
            
            new_lines.append(line)
        
        changes = []
        if removed > 0:
            changes.append(f"{file_path.name}: Removed {removed} duplicate forward declarations")
            return '\n'.join(new_lines), changes
        
        return content, changes


class MissingIncludesPlugin(PostProcessPlugin):
    """Add missing common includes - SAFETY NET ONLY (EngineKnowledge should handle most)"""
    
    def __init__(self):
        super().__init__("MissingIncludes", priority=30)
    
    def process_header(self, content: str, file_path: Path, context: Dict) -> Tuple[str, List[str]]:
        changes = []
        added = []
        
        # Check for missing CoreMinimal.h - SAFETY NET
        # EngineKnowledge should be adding this, but we keep as fallback
        if '#include "CoreMinimal.h"' not in content:
            if '#pragma once' in content:
                content = content.replace(
                    '#pragma once',
                    '#pragma once\n\n#include "CoreMinimal.h"'
                )
                added.append("CoreMinimal.h")
        
        # NOTE: Net/UnrealNetwork.h should now be handled by EngineKnowledge
        # If this gets triggered, it means EngineKnowledge needs updating
        # Check for Net/UnrealNetwork.h if using replication
        if 'DOREPLIFETIME' in content and '#include "Net/UnrealNetwork.h"' not in content:
            lines = content.split('\n')
            last_include_idx = -1
            for i, line in enumerate(lines):
                if line.strip().startswith('#include'):
                    last_include_idx = i
            
            if last_include_idx >= 0:
                lines.insert(last_include_idx + 1, '#include "Net/UnrealNetwork.h"')
                content = '\n'.join(lines)
                added.append("Net/UnrealNetwork.h (FALLBACK - EngineKnowledge should handle this)")
        
        if added:
            changes.append(f"{file_path.name}: Added includes: {', '.join(added)}")
        
        return content, changes


class DelegateGeneratedHPlugin(PostProcessPlugin):
    """DISABLED - Delegates actually NEED .generated.h for _DelegateWrapper types"""
    
    def __init__(self):
        super().__init__("DelegateGeneratedH", priority=40)
    
    def process_header(self, content: str, file_path: Path, context: Dict) -> Tuple[str, List[str]]:
        # DISABLED - This was removing .generated.h from delegate files, but delegates
        # declared with DECLARE_DYNAMIC_MULTICAST_DELEGATE need the .generated.h file
        # because UHT generates _DelegateWrapper types there
        return content, []


class EmptyLinesPlugin(PostProcessPlugin):
    """Clean up excessive empty lines"""
    
    def __init__(self):
        super().__init__("EmptyLines", priority=90)  # Run last
    
    def process_header(self, content: str, file_path: Path, context: Dict) -> Tuple[str, List[str]]:
        return self._clean(content, file_path)
    
    def process_source(self, content: str, file_path: Path, context: Dict) -> Tuple[str, List[str]]:
        return self._clean(content, file_path)
    
    def _clean(self, content: str, file_path: Path) -> Tuple[str, List[str]]:
        changes = []
        original_len = len(content)
        content = re.sub(r'\n{3,}', '\n\n', content)
        
        if len(content) != original_len:
            changes.append(f"{file_path.name}: Cleaned up empty lines")
        
        return content, changes


class HeaderFixerPlugin(PostProcessPlugin):
    """Use header_fixer.py for advanced fixes"""
    
    def __init__(self):
        super().__init__("HeaderFixer", priority=50)
        self.fixer = HeaderFixer()
    
    def process_header(self, content: str, file_path: Path, context: Dict) -> Tuple[str, List[str]]:
        changes = []
        
        # Apply header fixer fixes
        content, fixer_changes = self.fixer.fix_module_api(content, context["plugin_name"])
        changes.extend([f"{file_path.name}: {c}" for c in fixer_changes])
        
        content, fixer_changes = self.fixer.fix_forward_declarations(content)
        changes.extend([f"{file_path.name}: {c}" for c in fixer_changes])
        
        content, fixer_changes = self.fixer.fix_replication_setup(content)
        changes.extend([f"{file_path.name}: {c}" for c in fixer_changes])
        
        return content, changes


class UE5ValidatorPlugin(PostProcessPlugin):
    """Use ue5_validator.py for validation"""
    
    def __init__(self):
        super().__init__("UE5Validator", priority=5)  # Run early for validation
        self.validator = UE5Validator()
    
    def validate(self, content: str, file_path: Path, context: Dict) -> List[Dict]:
        if file_path.suffix != '.h':
            return []
        
        result = self.validator.validate_header_syntax(content, context["plugin_name"])
        
        issues = []
        for error in result.get('errors', []):
            issues.append({
                'severity': 'error',
                'message': error.get('message', 'Unknown error'),
                'fix': error.get('fix')
            })
        
        for warning in result.get('warnings', []):
            issues.append({
                'severity': 'warning',
                'message': warning.get('message', 'Unknown warning')
            })
        
        return issues


class ValidationRulesPlugin(PostProcessPlugin):
    """Use validation_rules.py for advanced validation"""
    
    def __init__(self):
        super().__init__("ValidationRules", priority=6)
        self.registry = RuleRegistry()
    
    def validate(self, content: str, file_path: Path, context: Dict) -> List[Dict]:
        # Determine item type from file
        item_type = "unknown"
        if 'UCLASS' in content:
            item_type = "actor"
        elif 'USTRUCT' in content:
            item_type = "struct"
        elif 'UCOMPONENT' in content:
            item_type = "component"
        
        # Build data dict (simplified)
        data = {
            "name": file_path.stem,
            "content": content
        }
        
        # Run validation
        issues = self.registry.validate(item_type, data)
        
        return [
            {
                'severity': issue.severity.value,
                'message': issue.message,
                'fix': issue.fix_suggestion
            }
            for issue in issues
        ]


# ============================================================================
# LEGACY METHODS (kept for backwards compat, but use plugins now)
# ============================================================================

class UE5PostProcessor_Legacy:
    """Legacy methods - kept for reference but plugins are used now"""


def main():
    """CLI entry point"""
    if len(sys.argv) < 3:
        print("Usage: python post_process.py <plugin_path> <plugin_name> [--verbose]")
        sys.exit(1)
    
    plugin_path = sys.argv[1]
    plugin_name = sys.argv[2]
    verbose = "--verbose" in sys.argv or "-v" in sys.argv
    
    processor = UE5PostProcessor(plugin_path, plugin_name, verbose)
    result = processor.process_all()
    
    # Print results
    if result["success"]:
        print(f"[OK] Post-processing complete: {result['fixes_applied']} fixes applied")
        if result.get('warnings'):
            print(f"[WARN] {len(result['warnings'])} warnings")
        if verbose:
            if result['fixes']:
                for fix in result['fixes']:
                    print(f"   - {fix}")
            if result.get('warnings'):
                for warning in result['warnings']:
                    print(f"   [WARN] {warning}")
    else:
        print(f"[ERROR] Post-processing failed with {len(result['errors'])} errors")
        for error in result['errors']:
            print(f"   - {error}")
        sys.exit(1)
    
    # Output JSON for Rust to parse
    print(json.dumps(result))


if __name__ == "__main__":
    main()
