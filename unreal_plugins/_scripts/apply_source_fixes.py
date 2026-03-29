#!/usr/bin/env python3
"""
KAIN Source File Pattern Fixer
Applies systematic fixes to .kn source files to resolve parse errors.

Usage:
    python apply_source_fixes.py <plugin_directory> [--dry-run]
    
Example:
    python apply_source_fixes.py ../VoxelForgePro
    python apply_source_fixes.py ../VoxelForgePro --dry-run
"""

import os
import sys
import re
import shutil
from pathlib import Path
from typing import List, Tuple, Dict
import argparse


class SourceFixer:
    """Applies systematic pattern fixes to KAIN source files."""
    
    def __init__(self, dry_run: bool = False):
        self.dry_run = dry_run
        self.fixes_applied: Dict[str, int] = {}
        self.new_patterns_found: List[str] = []
        
    def log(self, message: str):
        """Print log message."""
        print(f"[SourceFixer] {message}")
        
    def backup_file(self, filepath: Path) -> Path:
        """Create .kn.bak backup of file."""
        backup_path = filepath.with_suffix('.kn.bak')
        if not self.dry_run:
            shutil.copy2(filepath, backup_path)
        self.log(f"Created backup: {backup_path}")
        return backup_path
        
    def verify_line_count(self, original: str, modified: str, filepath: Path) -> bool:
        """Verify line count hasn't changed drastically (indicates corruption)."""
        original_lines = len(original.splitlines())
        modified_lines = len(modified.splitlines())
        
        # Allow up to 20% change in line count (for for-loop conversions)
        threshold = original_lines * 0.2
        diff = abs(original_lines - modified_lines)
        
        if diff > threshold:
            self.log(f"WARNING: Line count changed drastically in {filepath}")
            self.log(f"  Original: {original_lines} lines, Modified: {modified_lines} lines")
            self.log(f"  Difference: {diff} lines (threshold: {threshold})")
            return False
        return True
        
    def pattern_1_var_to_let(self, content: str) -> Tuple[str, int]:
        """Pattern 1: Replace 'var ' → 'let ' (with space to avoid matching 'variable')."""
        count = content.count('var ')
        modified = content.replace('var ', 'let ')
        return modified, count
        
    def pattern_2_not_to_equals_false(self, content: str) -> Tuple[str, int]:
        """Pattern 2: Replace ' not ' → ' == false ' (with spaces to avoid matching 'notify')."""
        count = content.count(' not ')
        modified = content.replace(' not ', ' == false ')
        return modified, count
        
    def pattern_3_and_operator(self, content: str) -> Tuple[str, int]:
        """Pattern 3: Replace ' && ' → ' and '."""
        count = content.count(' && ')
        modified = content.replace(' && ', ' and ')
        return modified, count
        
    def pattern_4_or_operator(self, content: str) -> Tuple[str, int]:
        """Pattern 4: Replace ' || ' → ' or '."""
        count = content.count(' || ')
        modified = content.replace(' || ', ' or ')
        return modified, count
        
    def pattern_5_let_mut(self, content: str) -> Tuple[str, int]:
        """Pattern 5: Replace 'let mut ' → 'let '."""
        count = content.count('let mut ')
        modified = content.replace('let mut ', 'let ')
        return modified, count
        
    def pattern_6_for_loop_to_while(self, content: str) -> Tuple[str, int]:
        """Pattern 6: Convert 'for i in start..end:' to while loops with counter."""
        # Regex to match: for <var> in <start>..<end>:
        pattern = r'for\s+(\w+)\s+in\s+(.+?)\.\.(.+?):\s*\n'
        matches = list(re.finditer(pattern, content))
        count = len(matches)
        
        if count == 0:
            return content, 0
            
        # Process matches in reverse order to preserve indices
        for match in reversed(matches):
            var_name = match.group(1)
            start_expr = match.group(2).strip()
            end_expr = match.group(3).strip()
            
            # Get indentation of the for loop
            line_start = content.rfind('\n', 0, match.start()) + 1
            indent = content[line_start:match.start()]
            
            # Find the loop body (all indented lines after the for statement)
            body_start = match.end()
            body_lines = []
            lines = content[body_start:].split('\n')
            
            # Determine body indentation (should be more than for loop indent)
            if lines and lines[0].strip():
                body_indent = len(lines[0]) - len(lines[0].lstrip())
            else:
                body_indent = len(indent) + 4
                
            for line in lines:
                if line.strip() == '':
                    body_lines.append(line)
                    continue
                line_indent = len(line) - len(line.lstrip())
                if line_indent >= body_indent:
                    body_lines.append(line)
                else:
                    break
                    
            body = '\n'.join(body_lines)
            body_end = body_start + len(body)
            
            # Construct while loop replacement
            replacement = f"{indent}let {var_name} = {start_expr}\n"
            replacement += f"{indent}while {var_name} < {end_expr}:\n"
            replacement += body
            if body and not body.endswith('\n'):
                replacement += '\n'
            replacement += f"{indent}    {var_name} = {var_name} + 1\n"
            
            # Replace in content
            content = content[:match.start()] + replacement + content[body_end:]
            
        return content, count
        
    def pattern_7_struct_field_access(self, content: str) -> Tuple[str, int]:
        """Pattern 7: Replace 'struct_var::field' → 'struct_var.field' (only for struct types)."""
        # This is tricky - we need to avoid enum variants (which use ::)
        # Heuristic: if the identifier before :: starts with lowercase, it's likely a variable
        # If it starts with uppercase, it's likely an enum type
        
        pattern = r'(\b[a-z_][a-z0-9_]*)::([a-z_][a-z0-9_]*)'
        matches = list(re.finditer(pattern, content))
        count = len(matches)
        
        if count == 0:
            return content, 0
            
        # Replace :: with . for lowercase identifiers
        modified = re.sub(pattern, r'\1.\2', content)
        return modified, count
        
    def pattern_8_struct_literals(self, content: str) -> Tuple[str, int]:
        """Pattern 8: Replace struct literals 'TypeName { field: val }' with field-by-field assignment."""
        # Match: TypeName { field1: val1, field2: val2 }
        # This is complex - we'll handle simple cases first
        
        # Pattern for Vec3i { x, y, z } shorthand
        vec_pattern = r'(Vec\d+[if]?)\s*\{\s*([a-z_][a-z0-9_]*)\s*,\s*([a-z_][a-z0-9_]*)\s*,\s*([a-z_][a-z0-9_]*)\s*\}'
        vec_matches = list(re.finditer(vec_pattern, content, re.IGNORECASE))
        vec_count = len(vec_matches)
        
        # Replace Vec3i { x, y, z } with vec3i(x, y, z)
        for match in reversed(vec_matches):
            type_name = match.group(1).lower()  # vec3i
            x = match.group(2)
            y = match.group(3)
            z = match.group(4)
            replacement = f"{type_name}({x}, {y}, {z})"
            content = content[:match.start()] + replacement + content[match.end():]
            
        # Pattern for TypeName { field: val, field2: val2 }
        # This is harder - we'll log these for manual review
        struct_pattern = r'([A-Z][a-zA-Z0-9]*)\s*\{\s*([a-z_][a-z0-9_]*)\s*:\s*([^,}]+)'
        struct_matches = list(re.finditer(struct_pattern, content))
        
        if struct_matches:
            self.new_patterns_found.append(
                f"Found {len(struct_matches)} struct literal patterns that need manual conversion"
            )
            
        return content, vec_count
        
    def pattern_9_match_arm_braces(self, content: str) -> Tuple[str, int]:
        """Pattern 9: Replace match arm braces '=> { body }' → '=> \\n    body'."""
        # Match: => { single_statement }
        pattern = r'=>\s*\{\s*([^}]+?)\s*\}'
        matches = list(re.finditer(pattern, content))
        count = len(matches)
        
        if count == 0:
            return content, 0
            
        # Process matches in reverse order
        for match in reversed(matches):
            body = match.group(1).strip()
            
            # Get indentation of the match arm
            line_start = content.rfind('\n', 0, match.start()) + 1
            indent = content[line_start:match.start()]
            
            # Calculate body indentation (4 spaces more than match arm)
            body_indent = ' ' * (len(indent) + 4)
            
            # Replace with indented body
            replacement = f"=>\n{body_indent}{body}"
            content = content[:match.start()] + replacement + content[match.end():]
            
        return content, count
        
    def pattern_10_reserved_keywords(self, content: str) -> Tuple[str, int]:
        """Pattern 10: Rename reserved keyword parameters (e.g., 'state' → 'current_state')."""
        # Common reserved keywords that might be used as parameter names
        reserved = {
            'state': 'current_state',
            'type': 'value_type',
            'match': 'match_value',
            'if': 'if_value',
            'while': 'while_value',
            'for': 'for_value',
            'return': 'return_value',
            'let': 'let_value',
            'fn': 'fn_value',
            'struct': 'struct_value',
            'enum': 'enum_value',
            'actor': 'actor_value',
        }
        
        count = 0
        for keyword, replacement in reserved.items():
            # Match keyword as parameter name: fn name(state: Type)
            param_pattern = rf'\(([^)]*\b{keyword}\b[^)]*)\)'
            matches = list(re.finditer(param_pattern, content))
            
            if matches:
                # Replace keyword in parameter lists
                for match in reversed(matches):
                    param_list = match.group(1)
                    new_param_list = re.sub(rf'\b{keyword}\b', replacement, param_list)
                    content = content[:match.start(1)] + new_param_list + content[match.end(1):]
                    count += 1
                    
        return content, count
        
    def apply_all_patterns(self, content: str, filepath: Path) -> str:
        """Apply all pattern fixes to content."""
        original_content = content
        
        # Apply each pattern
        patterns = [
            ('var_to_let', self.pattern_1_var_to_let),
            ('not_to_equals_false', self.pattern_2_not_to_equals_false),
            ('and_operator', self.pattern_3_and_operator),
            ('or_operator', self.pattern_4_or_operator),
            ('let_mut', self.pattern_5_let_mut),
            ('for_loop_to_while', self.pattern_6_for_loop_to_while),
            ('struct_field_access', self.pattern_7_struct_field_access),
            ('struct_literals', self.pattern_8_struct_literals),
            ('match_arm_braces', self.pattern_9_match_arm_braces),
            ('reserved_keywords', self.pattern_10_reserved_keywords),
        ]
        
        for pattern_name, pattern_func in patterns:
            content, count = pattern_func(content)
            if count > 0:
                self.log(f"  {pattern_name}: {count} replacements")
                if pattern_name not in self.fixes_applied:
                    self.fixes_applied[pattern_name] = 0
                self.fixes_applied[pattern_name] += count
                
        # Verify line count
        if not self.verify_line_count(original_content, content, filepath):
            self.log(f"  WARNING: Skipping file due to line count change")
            return original_content
            
        return content
        
    def process_file(self, filepath: Path) -> bool:
        """Process a single .kn file."""
        self.log(f"Processing: {filepath}")
        
        try:
            # Read original content
            with open(filepath, 'r', encoding='utf-8') as f:
                original_content = f.read()
                
            # Apply fixes
            modified_content = self.apply_all_patterns(original_content, filepath)
            
            # Check if anything changed
            if original_content == modified_content:
                self.log(f"  No changes needed")
                return True
                
            # Create backup
            if not self.dry_run:
                self.backup_file(filepath)
                
            # Write modified content
            if not self.dry_run:
                with open(filepath, 'w', encoding='utf-8') as f:
                    f.write(modified_content)
                self.log(f"  ✓ File updated")
            else:
                self.log(f"  [DRY RUN] Would update file")
                
            return True
            
        except Exception as e:
            self.log(f"  ERROR: {e}")
            return False
            
    def process_directory(self, directory: Path) -> Tuple[int, int]:
        """Process all .kn files in directory."""
        kn_files = list(directory.rglob('*.kn'))
        
        if not kn_files:
            self.log(f"No .kn files found in {directory}")
            return 0, 0
            
        self.log(f"Found {len(kn_files)} .kn files")
        
        success_count = 0
        fail_count = 0
        
        for filepath in kn_files:
            if self.process_file(filepath):
                success_count += 1
            else:
                fail_count += 1
                
        return success_count, fail_count
        
    def print_summary(self):
        """Print summary of fixes applied."""
        print("\n" + "="*60)
        print("SUMMARY")
        print("="*60)
        
        if self.fixes_applied:
            print("\nFixes Applied:")
            for pattern_name, count in sorted(self.fixes_applied.items()):
                print(f"  {pattern_name}: {count} replacements")
        else:
            print("\nNo fixes applied")
            
        if self.new_patterns_found:
            print("\nNew Patterns Found (Manual Review Needed):")
            for pattern in self.new_patterns_found:
                print(f"  - {pattern}")
                
        print("="*60)


def main():
    parser = argparse.ArgumentParser(
        description='Apply systematic pattern fixes to KAIN source files'
    )
    parser.add_argument(
        'plugin_directory',
        type=str,
        help='Path to plugin directory containing .kn files'
    )
    parser.add_argument(
        '--dry-run',
        action='store_true',
        help='Preview changes without modifying files'
    )
    
    args = parser.parse_args()
    
    # Resolve plugin directory
    plugin_dir = Path(args.plugin_directory).resolve()
    
    if not plugin_dir.exists():
        print(f"ERROR: Directory not found: {plugin_dir}")
        sys.exit(1)
        
    if not plugin_dir.is_dir():
        print(f"ERROR: Not a directory: {plugin_dir}")
        sys.exit(1)
        
    # Create fixer and process directory
    fixer = SourceFixer(dry_run=args.dry_run)
    
    print(f"\nProcessing plugin: {plugin_dir.name}")
    print(f"Directory: {plugin_dir}")
    if args.dry_run:
        print("Mode: DRY RUN (no files will be modified)")
    print()
    
    success_count, fail_count = fixer.process_directory(plugin_dir)
    
    # Print summary
    fixer.print_summary()
    
    print(f"\nResults:")
    print(f"  Success: {success_count} files")
    print(f"  Failed: {fail_count} files")
    
    if fail_count > 0:
        sys.exit(1)
    else:
        sys.exit(0)


if __name__ == '__main__':
    main()
