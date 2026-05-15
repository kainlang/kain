#!/usr/bin/env python3
"""
Plugin Quality Gate Validator

Validates completed plugins against quality standards:
- Zero TODOs
- Zero placeholders
- Zero simplifications
- Minimum LOC count
- Compression ratio
- KAIN compilation
- UE5 plugin loading
"""

import os
import sys
import re
import subprocess
from pathlib import Path
from typing import Dict, List, Tuple
from dataclasses import dataclass


@dataclass
class ValidationResult:
    """Result of a validation check."""
    check_name: str
    passed: bool
    message: str
    details: List[str] = None


class PluginValidator:
    """Validates plugin quality."""
    
    def __init__(self, plugin_dir: str, min_loc: int = 5000, min_compression: float = 15.0):
        self.plugin_dir = Path(plugin_dir)
        self.min_loc = min_loc
        self.min_compression = min_compression
        self.results: List[ValidationResult] = []
        
    def find_kain_files(self) -> List[Path]:
        """Find all .kn files in plugin directory."""
        kain_files = []
        for root, dirs, files in os.walk(self.plugin_dir):
            # Skip hidden directories and build directories
            dirs[:] = [d for d in dirs if not d.startswith('.') and d not in ['_Builds', 'Intermediate', 'Binaries']]
            
            for file in files:
                if file.endswith('.kn'):
                    kain_files.append(Path(root) / file)
        
        return kain_files
    
    def find_generated_files(self) -> List[Path]:
        """Find all generated C++ files."""
        generated_files = []
        source_dir = self.plugin_dir / 'Source'
        
        if source_dir.exists():
            for root, dirs, files in os.walk(source_dir):
                for file in files:
                    if file.endswith(('.h', '.cpp')):
                        generated_files.append(Path(root) / file)
        
        return generated_files
    
    def count_lines(self, files: List[Path]) -> int:
        """Count total lines of code in files."""
        total_lines = 0
        
        for filepath in files:
            try:
                with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
                    lines = f.readlines()
                    # Count non-empty, non-comment lines
                    code_lines = [l for l in lines if l.strip() and not l.strip().startswith('//')]
                    total_lines += len(code_lines)
            except Exception as e:
                print(f"Warning: Could not read {filepath}: {e}")
        
        return total_lines
    
    def check_forbidden_patterns(self, files: List[Path], patterns: List[str], check_name: str) -> ValidationResult:
        """Check for forbidden patterns in files."""
        violations = []
        
        for filepath in files:
            try:
                with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read()
                    
                    for pattern in patterns:
                        matches = re.finditer(pattern, content, re.IGNORECASE)
                        for match in matches:
                            # Get line number
                            line_num = content[:match.start()].count('\n') + 1
                            line_content = content.split('\n')[line_num - 1].strip()
                            violations.append(f"{filepath.name}:{line_num}: {line_content}")
            except Exception as e:
                print(f"Warning: Could not read {filepath}: {e}")
        
        if violations:
            return ValidationResult(
                check_name=check_name,
                passed=False,
                message=f"Found {len(violations)} violations",
                details=violations[:10]  # Limit to first 10
            )
        else:
            return ValidationResult(
                check_name=check_name,
                passed=True,
                message="No violations found"
            )
    
    def validate_no_todos(self) -> ValidationResult:
        """Validate zero TODOs in code."""
        kain_files = self.find_kain_files()
        patterns = [r'\bTODO\b', r'\bFIXME\b', r'\bXXX\b']
        return self.check_forbidden_patterns(kain_files, patterns, "No TODOs")
    
    def validate_no_placeholders(self) -> ValidationResult:
        """Validate zero placeholders in code."""
        kain_files = self.find_kain_files()
        patterns = [r'\{\{.*?\}\}', r'<PLACEHOLDER>', r'PLACEHOLDER']
        return self.check_forbidden_patterns(kain_files, patterns, "No Placeholders")
    
    def validate_no_simplifications(self) -> ValidationResult:
        """Validate zero simplifications in code."""
        kain_files = self.find_kain_files()
        patterns = [r'\bsimplif(y|ied|ication)\b', r'\bstub\b', r'\bmock\b']
        return self.check_forbidden_patterns(kain_files, patterns, "No Simplifications")
    
    def validate_loc_count(self) -> ValidationResult:
        """Validate minimum LOC count."""
        kain_files = self.find_kain_files()
        total_loc = self.count_lines(kain_files)
        
        if total_loc >= self.min_loc:
            return ValidationResult(
                check_name="LOC Count",
                passed=True,
                message=f"Plugin has {total_loc} lines (minimum: {self.min_loc})"
            )
        else:
            return ValidationResult(
                check_name="LOC Count",
                passed=False,
                message=f"Plugin has only {total_loc} lines (minimum: {self.min_loc})"
            )
    
    def validate_compression_ratio(self) -> ValidationResult:
        """Validate compression ratio."""
        kain_files = self.find_kain_files()
        generated_files = self.find_generated_files()
        
        kain_loc = self.count_lines(kain_files)
        generated_loc = self.count_lines(generated_files)
        
        if kain_loc == 0:
            return ValidationResult(
                check_name="Compression Ratio",
                passed=False,
                message="No KAIN source files found"
            )
        
        ratio = generated_loc / kain_loc if kain_loc > 0 else 0
        
        if ratio >= self.min_compression:
            return ValidationResult(
                check_name="Compression Ratio",
                passed=True,
                message=f"Compression ratio: 1:{ratio:.1f} (minimum: 1:{self.min_compression})"
            )
        else:
            return ValidationResult(
                check_name="Compression Ratio",
                passed=False,
                message=f"Compression ratio: 1:{ratio:.1f} (minimum: 1:{self.min_compression})"
            )
    
    def validate_kain_compilation(self) -> ValidationResult:
        """Validate KAIN compilation."""
        # Find KAIN.toml
        kain_toml = self.plugin_dir / 'KAIN.toml'
        
        if not kain_toml.exists():
            return ValidationResult(
                check_name="KAIN Compilation",
                passed=False,
                message="KAIN.toml not found"
            )
        
        # Try to compile
        try:
            result = subprocess.run(
                ['kain', 'build', '--ue5', '--dry-run'],
                cwd=str(self.plugin_dir),
                capture_output=True,
                text=True,
                timeout=60
            )
            
            if result.returncode == 0:
                return ValidationResult(
                    check_name="KAIN Compilation",
                    passed=True,
                    message="Plugin compiles successfully"
                )
            else:
                return ValidationResult(
                    check_name="KAIN Compilation",
                    passed=False,
                    message="Compilation failed",
                    details=[result.stderr[:500]]
                )
        except subprocess.TimeoutExpired:
            return ValidationResult(
                check_name="KAIN Compilation",
                passed=False,
                message="Compilation timed out"
            )
        except FileNotFoundError:
            return ValidationResult(
                check_name="KAIN Compilation",
                passed=False,
                message="kain command not found in PATH"
            )
        except Exception as e:
            return ValidationResult(
                check_name="KAIN Compilation",
                passed=False,
                message=f"Compilation error: {e}"
            )
    
    def validate_ue5_plugin(self) -> ValidationResult:
        """Validate UE5 plugin structure."""
        # Check for required files
        required_files = [
            self.plugin_dir / f'{self.plugin_dir.name}.uplugin',
            self.plugin_dir / 'Source',
        ]
        
        missing_files = [f for f in required_files if not f.exists()]
        
        if missing_files:
            return ValidationResult(
                check_name="UE5 Plugin Structure",
                passed=False,
                message="Missing required files",
                details=[str(f) for f in missing_files]
            )
        
        # Check .uplugin format
        uplugin_path = self.plugin_dir / f'{self.plugin_dir.name}.uplugin'
        try:
            with open(uplugin_path, 'r', encoding='utf-8') as f:
                import json
                uplugin_data = json.load(f)
                
                required_keys = ['FileVersion', 'Version', 'VersionName', 'FriendlyName', 'Description', 'Category', 'Modules']
                missing_keys = [k for k in required_keys if k not in uplugin_data]
                
                if missing_keys:
                    return ValidationResult(
                        check_name="UE5 Plugin Structure",
                        passed=False,
                        message="Invalid .uplugin format",
                        details=[f"Missing key: {k}" for k in missing_keys]
                    )
        except Exception as e:
            return ValidationResult(
                check_name="UE5 Plugin Structure",
                passed=False,
                message=f"Could not parse .uplugin: {e}"
            )
        
        return ValidationResult(
            check_name="UE5 Plugin Structure",
            passed=True,
            message="Plugin structure is valid"
        )
    
    def run_all_validations(self) -> bool:
        """Run all validation checks."""
        print("=" * 80)
        print("PLUGIN QUALITY GATE VALIDATOR")
        print(f"Plugin: {self.plugin_dir.name}")
        print(f"Directory: {self.plugin_dir}")
        print("=" * 80)
        print()
        
        # Run all checks
        checks = [
            self.validate_no_todos,
            self.validate_no_placeholders,
            self.validate_no_simplifications,
            self.validate_loc_count,
            self.validate_compression_ratio,
            self.validate_kain_compilation,
            self.validate_ue5_plugin,
        ]
        
        for check in checks:
            print(f"Running check: {check.__name__.replace('validate_', '').replace('_', ' ').title()}...")
            result = check()
            self.results.append(result)
            
            status_icon = "✓" if result.passed else "✗"
            print(f"  {status_icon} {result.message}")
            
            if result.details:
                print(f"    Details:")
                for detail in result.details:
                    print(f"      - {detail}")
            print()
        
        # Summary
        passed = sum(1 for r in self.results if r.passed)
        total = len(self.results)
        
        print("=" * 80)
        print("VALIDATION SUMMARY")
        print("=" * 80)
        print(f"Passed: {passed}/{total}")
        print(f"Failed: {total - passed}/{total}")
        print()
        
        if passed == total:
            print("✓ All quality gates passed!")
            return True
        else:
            print("✗ Quality gate validation failed")
            print("\nFailed checks:")
            for result in self.results:
                if not result.passed:
                    print(f"  - {result.check_name}: {result.message}")
            return False


def main():
    """Main entry point."""
    if len(sys.argv) < 2:
        print("Usage: python validate_plugin.py <plugin_dir> [min_loc] [min_compression]")
        print("Example: python validate_plugin.py /path/to/plugin 5000 15.0")
        sys.exit(1)
    
    plugin_dir = sys.argv[1]
    min_loc = int(sys.argv[2]) if len(sys.argv) > 2 else 5000
    min_compression = float(sys.argv[3]) if len(sys.argv) > 3 else 15.0
    
    validator = PluginValidator(plugin_dir, min_loc, min_compression)
    success = validator.run_all_validations()
    
    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()
