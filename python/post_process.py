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
        self.plugins.append(IncludeOrderFixPlugin())  # NEW: Reorder includes and add guards
        self.plugins.append(ForwardDeclFixPlugin())  # NEW: Add missing forward declarations
        self.plugins.append(DuplicateForwardDeclPlugin())
        self.plugins.append(ReplicationFixPlugin())  # NEW: Add GetLifetimeReplicatedProps
        self.plugins.append(ShaderInitFixPlugin())  # NEW: Add shader initialization
        self.plugins.append(MissingIncludesPlugin())
        self.plugins.append(DelegateGeneratedHPlugin())
        self.plugins.append(FormattingFixPlugin())  # NEW: Normalize formatting
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


class FormattingFixPlugin(PostProcessPlugin):
    """Normalize formatting: blank lines, indentation, line endings, trailing whitespace"""
    
    def __init__(self):
        super().__init__("FormattingFix", priority=85)  # Run near the end
    
    def process_header(self, content: str, file_path: Path, context: Dict) -> Tuple[str, List[str]]:
        return self._format(content, file_path)
    
    def process_source(self, content: str, file_path: Path, context: Dict) -> Tuple[str, List[str]]:
        return self._format(content, file_path)
    
    def _format(self, content: str, file_path: Path) -> Tuple[str, List[str]]:
        changes = []
        original = content
        
        # 1. Normalize line endings to LF
        if '\r\n' in content:
            content = content.replace('\r\n', '\n')
            changes.append(f"{file_path.name}: Normalized line endings to LF")
        
        # 2. Remove trailing whitespace from each line
        lines = content.split('\n')
        stripped_lines = [line.rstrip() for line in lines]
        if lines != stripped_lines:
            content = '\n'.join(stripped_lines)
            changes.append(f"{file_path.name}: Removed trailing whitespace")
        
        # 3. Normalize indentation to tabs
        # Convert 4 spaces to tabs
        lines = content.split('\n')
        normalized_lines = []
        for line in lines:
            # Count leading spaces
            leading_spaces = len(line) - len(line.lstrip(' '))
            if leading_spaces > 0 and leading_spaces % 4 == 0:
                # Convert groups of 4 spaces to tabs
                tabs = '\t' * (leading_spaces // 4)
                normalized_lines.append(tabs + line.lstrip(' '))
            else:
                normalized_lines.append(line)
        
        new_content = '\n'.join(normalized_lines)
        if new_content != content:
            content = new_content
            changes.append(f"{file_path.name}: Normalized indentation to tabs")
        
        # 4. Normalize blank lines to single (already handled by EmptyLinesPlugin, but ensure)
        content = re.sub(r'\n{3,}', '\n\n', content)
        
        return content, changes


class IncludeOrderFixPlugin(PostProcessPlugin):
    """Reorder includes to UE5 conventions and add include guards"""
    
    def __init__(self):
        super().__init__("IncludeOrderFix", priority=12)  # Run early
    
    def process_header(self, content: str, file_path: Path, context: Dict) -> Tuple[str, List[str]]:
        changes = []
        
        # 1. Check for include guard
        if not self._has_include_guard(content):
            content = self._add_include_guard(content, file_path)
            changes.append(f"{file_path.name}: Added include guard")
        
        # 2. Reorder includes
        content, reordered = self._reorder_includes(content, file_path)
        if reordered:
            changes.append(f"{file_path.name}: Reordered includes to UE5 conventions")
        
        return content, changes
    
    def _has_include_guard(self, content: str) -> bool:
        """Check if header has include guard"""
        return '#pragma once' in content or '#ifndef' in content
    
    def _add_include_guard(self, content: str, file_path: Path) -> str:
        """Add #pragma once include guard"""
        return '#pragma once\n\n' + content
    
    def _reorder_includes(self, content: str, file_path: Path) -> Tuple[str, bool]:
        """Reorder includes to UE5 conventions: CoreMinimal first, then engine, then project"""
        lines = content.split('\n')
        
        # Extract includes
        includes = []
        non_include_lines = []
        pragma_once_idx = -1
        
        for i, line in enumerate(lines):
            stripped = line.strip()
            if stripped.startswith('#include'):
                includes.append(line)
            elif stripped == '#pragma once':
                pragma_once_idx = i
                non_include_lines.append(line)
            else:
                non_include_lines.append(line)
        
        if not includes:
            return content, False
        
        # Categorize includes
        core_minimal = []
        engine_includes = []
        project_includes = []
        generated_includes = []
        
        for inc in includes:
            if 'CoreMinimal.h' in inc:
                core_minimal.append(inc)
            elif '.generated.h' in inc:
                generated_includes.append(inc)
            elif inc.strip().startswith('#include <') or 'Engine/' in inc or 'Runtime/' in inc:
                engine_includes.append(inc)
            else:
                project_includes.append(inc)
        
        # Reorder: CoreMinimal, engine, project, generated
        ordered_includes = []
        if core_minimal:
            ordered_includes.extend(core_minimal)
        if engine_includes:
            if ordered_includes:
                ordered_includes.append('')  # Blank line separator
            ordered_includes.extend(sorted(engine_includes))
        if project_includes:
            if ordered_includes:
                ordered_includes.append('')  # Blank line separator
            ordered_includes.extend(sorted(project_includes))
        if generated_includes:
            if ordered_includes:
                ordered_includes.append('')  # Blank line separator
            ordered_includes.extend(generated_includes)
        
        # Reconstruct content
        new_lines = []
        
        # Add pragma once
        if pragma_once_idx >= 0:
            new_lines.append('#pragma once')
            new_lines.append('')
        
        # Add ordered includes
        new_lines.extend(ordered_includes)
        new_lines.append('')
        
        # Add rest of content (skip old includes and pragma once)
        in_include_section = True
        for line in non_include_lines:
            stripped = line.strip()
            if stripped == '#pragma once':
                continue  # Already added
            if stripped.startswith('#include'):
                continue  # Already added
            if in_include_section and not stripped:
                continue  # Skip blank lines in include section
            if stripped:
                in_include_section = False
            new_lines.append(line)
        
        new_content = '\n'.join(new_lines)
        
        # Check if order changed
        changed = new_content != content
        
        return new_content, changed


class ReplicationFixPlugin(PostProcessPlugin):
    """Add GetLifetimeReplicatedProps implementation for replicated properties"""
    
    def __init__(self):
        super().__init__("ReplicationFix", priority=25)
    
    def process_source(self, content: str, file_path: Path, context: Dict) -> Tuple[str, List[str]]:
        changes = []
        
        # Check if this is an actor/component source file with replication
        if not self._has_replicated_properties(content):
            return content, changes
        
        # Check if GetLifetimeReplicatedProps is already implemented
        if 'GetLifetimeReplicatedProps' in content:
            return content, changes
        
        # Extract class name from file
        class_name = self._extract_class_name(content)
        if not class_name:
            return content, changes
        
        # Extract replicated property names
        replicated_props = self._extract_replicated_properties(content)
        if not replicated_props:
            return content, changes
        
        # Generate GetLifetimeReplicatedProps implementation
        impl = self._generate_replication_impl(class_name, replicated_props)
        
        # Insert before the last closing brace (end of file)
        lines = content.split('\n')
        
        # Find the last non-empty line
        insert_idx = len(lines)
        for i in range(len(lines) - 1, -1, -1):
            if lines[i].strip():
                insert_idx = i
                break
        
        # Insert the implementation
        lines.insert(insert_idx, impl)
        content = '\n'.join(lines)
        
        changes.append(f"{file_path.name}: Added GetLifetimeReplicatedProps for {len(replicated_props)} replicated properties")
        
        return content, changes
    
    def _has_replicated_properties(self, content: str) -> bool:
        """Check if content has replicated properties"""
        return 'Replicated' in content or 'DOREPLIFETIME' in content
    
    def _extract_class_name(self, content: str) -> str:
        """Extract class name from source file"""
        # Look for class definition pattern: void AClassName::
        match = re.search(r'void\s+([AU][A-Z]\w+)::', content)
        if match:
            return match.group(1)
        return ""
    
    def _extract_replicated_properties(self, content: str) -> List[str]:
        """Extract replicated property names from source"""
        props = []
        
        # Look for DOREPLIFETIME macros
        for match in re.finditer(r'DOREPLIFETIME\([^,]+,\s*(\w+)\)', content):
            props.append(match.group(1))
        
        return props
    
    def _generate_replication_impl(self, class_name: str, props: List[str]) -> str:
        """Generate GetLifetimeReplicatedProps implementation"""
        impl = f"\nvoid {class_name}::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const\n"
        impl += "{\n"
        impl += "\tSuper::GetLifetimeReplicatedProps(OutLifetimeProps);\n"
        impl += "\t\n"
        
        for prop in props:
            impl += f"\tDOREPLIFETIME({class_name}, {prop});\n"
        
        impl += "}\n"
        
        return impl


class ShaderInitFixPlugin(PostProcessPlugin):
    """Add shader initialization in BeginPlay for actors using shaders"""
    
    def __init__(self):
        super().__init__("ShaderInitFix", priority=26)
    
    def process_source(self, content: str, file_path: Path, context: Dict) -> Tuple[str, List[str]]:
        changes = []
        
        # Check if this is an actor source file with shaders
        if not self._has_shader_usage(content):
            return content, changes
        
        # Check if shader initialization is already present
        if 'LoadObject<UMaterialInterface>' in content or 'SetMaterial' in content:
            return content, changes
        
        # Extract class name
        class_name = self._extract_class_name(content)
        if not class_name:
            return content, changes
        
        # Extract shader/material references
        shader_refs = self._extract_shader_references(content)
        if not shader_refs:
            return content, changes
        
        # Check if BeginPlay exists
        if f'{class_name}::BeginPlay()' in content:
            # Insert shader init into existing BeginPlay
            content = self._insert_into_begin_play(content, class_name, shader_refs)
            changes.append(f"{file_path.name}: Added shader initialization to BeginPlay")
        else:
            # Create BeginPlay with shader init
            impl = self._generate_begin_play_with_shader_init(class_name, shader_refs)
            
            # Insert before the last closing brace
            lines = content.split('\n')
            insert_idx = len(lines)
            for i in range(len(lines) - 1, -1, -1):
                if lines[i].strip():
                    insert_idx = i
                    break
            
            lines.insert(insert_idx, impl)
            content = '\n'.join(lines)
            changes.append(f"{file_path.name}: Added BeginPlay with shader initialization")
        
        return content, changes
    
    def _has_shader_usage(self, content: str) -> bool:
        """Check if content uses shaders"""
        return ('Shader' in content or 'Material' in content) and 'AActor' in content
    
    def _extract_class_name(self, content: str) -> str:
        """Extract class name from source file"""
        match = re.search(r'void\s+(A[A-Z]\w+)::', content)
        if match:
            return match.group(1)
        return ""
    
    def _extract_shader_references(self, content: str) -> List[str]:
        """Extract shader/material member variable names"""
        refs = []
        
        # Look for material/shader member variables
        for match in re.finditer(r'UMaterialInterface\*\s+(\w+)', content):
            refs.append(match.group(1))
        
        for match in re.finditer(r'UMaterial\*\s+(\w+)', content):
            refs.append(match.group(1))
        
        return refs
    
    def _insert_into_begin_play(self, content: str, class_name: str, shader_refs: List[str]) -> str:
        """Insert shader initialization into existing BeginPlay"""
        # Find BeginPlay implementation
        begin_play_pattern = rf'void\s+{class_name}::BeginPlay\(\)\s*\{{'
        match = re.search(begin_play_pattern, content)
        
        if not match:
            return content
        
        # Find the position after Super::BeginPlay()
        super_call_pattern = r'Super::BeginPlay\(\);'
        super_match = re.search(super_call_pattern, content[match.end():])
        
        if not super_match:
            return content
        
        insert_pos = match.end() + super_match.end()
        
        # Generate shader init code
        init_code = "\n\t\n\t// Initialize shaders\n"
        for ref in shader_refs:
            init_code += f"\tif ({ref})\n"
            init_code += "\t{\n"
            init_code += f"\t\t// Shader {ref} is ready\n"
            init_code += "\t}\n"
        
        # Insert the code
        content = content[:insert_pos] + init_code + content[insert_pos:]
        
        return content
    
    def _generate_begin_play_with_shader_init(self, class_name: str, shader_refs: List[str]) -> str:
        """Generate BeginPlay implementation with shader initialization"""
        impl = f"\nvoid {class_name}::BeginPlay()\n"
        impl += "{\n"
        impl += "\tSuper::BeginPlay();\n"
        impl += "\t\n"
        impl += "\t// Initialize shaders\n"
        
        for ref in shader_refs:
            impl += f"\tif ({ref})\n"
            impl += "\t{\n"
            impl += f"\t\t// Shader {ref} is ready\n"
            impl += "\t}\n"
        
        impl += "}\n"
        
        return impl


class ForwardDeclFixPlugin(PostProcessPlugin):
    """Add missing forward declarations and handle circular dependencies"""
    
    def __init__(self):
        super().__init__("ForwardDeclFix", priority=15)  # Run early
    
    def process_header(self, content: str, file_path: Path, context: Dict) -> Tuple[str, List[str]]:
        changes = []
        
        # Extract used types from the header
        used_types = self._extract_used_types(content)
        
        # Extract already forward-declared types
        existing_forward_decls = self._extract_forward_decls(content)
        
        # Extract included types
        included_types = self._extract_included_types(content)
        
        # Determine which types need forward declarations
        needed_forward_decls = []
        for type_name in used_types:
            # Skip if already forward declared or included
            if type_name in existing_forward_decls or type_name in included_types:
                continue
            
            # Skip primitive types and UE5 core types
            if self._is_primitive_or_core_type(type_name):
                continue
            
            needed_forward_decls.append(type_name)
        
        if not needed_forward_decls:
            return content, changes
        
        # Sort forward declarations: classes before structs
        classes = [t for t in needed_forward_decls if t.startswith('A') or t.startswith('U')]
        structs = [t for t in needed_forward_decls if t.startswith('F')]
        enums = [t for t in needed_forward_decls if t.startswith('E')]
        
        # Generate forward declarations
        forward_decls = []
        for cls in sorted(classes):
            forward_decls.append(f"class {cls};")
        for struct in sorted(structs):
            forward_decls.append(f"struct {struct};")
        for enum in sorted(enums):
            forward_decls.append(f"enum class {enum} : uint8;")
        
        if not forward_decls:
            return content, changes
        
        # Insert forward declarations after #pragma once and includes
        lines = content.split('\n')
        insert_idx = 0
        
        # Find the position after includes
        for i, line in enumerate(lines):
            if line.strip().startswith('#include'):
                insert_idx = i + 1
            elif line.strip().startswith('#pragma once'):
                insert_idx = i + 1
        
        # Insert forward declarations
        forward_decl_block = '\n// Forward declarations\n' + '\n'.join(forward_decls) + '\n'
        lines.insert(insert_idx, forward_decl_block)
        
        content = '\n'.join(lines)
        changes.append(f"{file_path.name}: Added {len(forward_decls)} forward declarations")
        
        return content, changes
    
    def _extract_used_types(self, content: str) -> set:
        """Extract all type names used in the header"""
        types = set()
        
        # Look for pointer types: Type*
        for match in re.finditer(r'\b([AUFE][A-Z]\w+)\*', content):
            types.add(match.group(1))
        
        # Look for reference types: Type&
        for match in re.finditer(r'\b([AUFE][A-Z]\w+)&', content):
            types.add(match.group(1))
        
        # Look for template parameters: TArray<Type>
        for match in re.finditer(r'TArray<([AUFE][A-Z]\w+)\*?>', content):
            types.add(match.group(1))
        
        for match in re.finditer(r'TMap<\w+,\s*([AUFE][A-Z]\w+)\*?>', content):
            types.add(match.group(1))
        
        return types
    
    def _extract_forward_decls(self, content: str) -> set:
        """Extract already forward-declared types"""
        decls = set()
        
        for match in re.finditer(r'^\s*(?:class|struct|enum class)\s+([AUFE][A-Z]\w+)\s*;', content, re.MULTILINE):
            decls.add(match.group(1))
        
        return decls
    
    def _extract_included_types(self, content: str) -> set:
        """Extract types from #include statements"""
        types = set()
        
        for match in re.finditer(r'#include\s+"([^"]+)\.h"', content):
            # Extract type name from file name
            file_name = match.group(1)
            # Remove path components
            type_name = file_name.split('/')[-1]
            types.add(type_name)
        
        return types
    
    def _is_primitive_or_core_type(self, type_name: str) -> bool:
        """Check if type is a primitive or UE5 core type that doesn't need forward declaration"""
        core_types = {
            'FString', 'FName', 'FText', 'FVector', 'FRotator', 'FTransform',
            'FLinearColor', 'FColor', 'FVector2D', 'FVector4', 'FQuat',
            'TArray', 'TMap', 'TSet', 'TSubclassOf', 'TWeakObjectPtr',
            'UObject', 'AActor', 'UActorComponent', 'USceneComponent',
        }
        return type_name in core_types


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
