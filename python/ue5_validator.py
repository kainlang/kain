"""
UE5 Code Validator
Integrates with UnrealBuildTool and UnrealHeaderTool to validate generated C++ code
"""

import subprocess
import json
import re
from pathlib import Path
from typing import Dict, List, Optional, Tuple
import config as cfg

config = cfg.config


class UE5Validator:
    """Validates UE5 C++ code using UBT and UHT"""
    
    def __init__(self, ue5_source_path: Optional[str] = None, 
                 ue5_engine_path: Optional[str] = None):
        """
        Initialize validator
        
        Args:
            ue5_source_path: Path to UE5 source code (for UHT)
            ue5_engine_path: Path to UE5 Engine installation
        """
        # Try config first
        if not ue5_engine_path and config.ue5_engine_path:
            ue5_engine_path = config.ue5_engine_path
        
        if not ue5_source_path and config.ue5_source_path:
            ue5_source_path = config.ue5_source_path
        
        self.ue5_source_path = Path(ue5_source_path) if ue5_source_path else None
        self.ue5_engine_path = Path(ue5_engine_path) if ue5_engine_path else None
        
        # Try to auto-detect UE5 paths
        if not self.ue5_engine_path:
            self.ue5_engine_path = self._detect_ue5_engine()
        
        if self.ue5_engine_path:
            self.ubt_path = self.ue5_engine_path / "Engine" / "Binaries" / "DotNET" / "UnrealBuildTool" / "UnrealBuildTool.exe"
            self.uht_path = self.ue5_engine_path / "Engine" / "Binaries" / "Win64" / "UnrealHeaderTool.exe"
        else:
            self.ubt_path = None
            self.uht_path = None
    
    def _detect_ue5_engine(self) -> Optional[Path]:
        """Try to detect UE5 installation"""
        common_paths = [
            # User's custom paths
            Path(r"F:\3D and Creative work\Apps\UE_5.7"),
            Path(r"F:\3D and Creative work\Apps\UE_5.6"),
            Path(r"F:\3D and Creative work\Apps\UE_5.5"),
            Path(r"F:\3D and Creative work\Apps\UE_5.4"),
            # Standard Epic Games paths
            Path("C:/Program Files/Epic Games/UE_5.7"),
            Path("C:/Program Files/Epic Games/UE_5.6"),
            Path("C:/Program Files/Epic Games/UE_5.5"),
            Path("C:/Program Files/Epic Games/UE_5.4"),
            Path("C:/Program Files/Epic Games/UE_5.3"),
            # Custom install paths
            Path("D:/UnrealEngine"),
            Path("C:/UnrealEngine"),
        ]
        
        for path in common_paths:
            if path.exists() and (path / "Engine").exists():
                return path
        
        return None
    
    def validate_header_syntax(self, header_content: str, plugin_name: str) -> Dict:
        """
        Validate UE5 header syntax without full compilation
        
        Args:
            header_content: C++ header content
            plugin_name: Plugin name
            
        Returns:
            Dict with validation results
        """
        errors = []
        warnings = []
        
        # Check for common UE5 header issues
        
        # 1. Check for GENERATED_BODY or GENERATED_UCLASS_BODY
        if "UCLASS" in header_content and "GENERATED_" not in header_content:
            errors.append({
                "type": "missing_macro",
                "message": "UCLASS without GENERATED_BODY() or GENERATED_UCLASS_BODY()",
                "severity": "error"
            })
        
        # 2. Check for .generated.h include
        if "UCLASS" in header_content or "USTRUCT" in header_content or "UENUM" in header_content:
            if ".generated.h" not in header_content:
                errors.append({
                    "type": "missing_include",
                    "message": "Missing .generated.h include (required for UCLASS/USTRUCT/UENUM)",
                    "severity": "error"
                })
        
        # 3. Check module API macro
        module_api = f"{plugin_name.upper()}_API"
        if "GAME_API" in header_content:
            errors.append({
                "type": "wrong_api_macro",
                "message": f"Using GAME_API instead of {module_api}",
                "severity": "error",
                "fix": f"Replace GAME_API with {module_api}"
            })
        
        # 4. Check for proper UPROPERTY usage
        uproperty_pattern = r'UPROPERTY\([^)]*\)\s*\n\s*(\w+)\s+(\w+);'
        for match in re.finditer(uproperty_pattern, header_content):
            prop_type = match.group(1)
            prop_name = match.group(2)
            
            # Check for pointer types without UPROPERTY protection
            if '*' in prop_type and 'UPROPERTY' not in match.group(0):
                warnings.append({
                    "type": "unprotected_pointer",
                    "message": f"Pointer property '{prop_name}' should use UPROPERTY for GC protection",
                    "severity": "warning"
                })
        
        # 5. Check for replicated properties without GetLifetimeReplicatedProps
        if "Replicated" in header_content:
            if "GetLifetimeReplicatedProps" not in header_content:
                errors.append({
                    "type": "missing_replication_function",
                    "message": "Replicated properties require GetLifetimeReplicatedProps() declaration",
                    "severity": "error",
                    "fix": "Add: virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;"
                })
        
        # 6. Check for proper include guards
        if "#pragma once" not in header_content:
            errors.append({
                "type": "missing_include_guard",
                "message": "Missing #pragma once",
                "severity": "error"
            })
        
        # 7. Check for CoreMinimal.h
        if "CoreMinimal.h" not in header_content:
            warnings.append({
                "type": "missing_core_include",
                "message": "Missing #include \"CoreMinimal.h\"",
                "severity": "warning"
            })
        
        return {
            "valid": len(errors) == 0,
            "errors": errors,
            "warnings": warnings,
            "error_count": len(errors),
            "warning_count": len(warnings)
        }
    
    def run_uht_check(self, plugin_path: str) -> Dict:
        """
        Run UnrealHeaderTool to validate headers
        
        Args:
            plugin_path: Path to plugin directory
            
        Returns:
            Dict with UHT results
        """
        if not self.uht_path or not self.uht_path.exists():
            return {
                "success": False,
                "error": "UnrealHeaderTool not found",
                "uht_path": str(self.uht_path) if self.uht_path else None
            }
        
        plugin_path = config.resolve_path(plugin_path)
        
        if not plugin_path.exists():
            return {
                "success": False,
                "error": f"Plugin path not found: {plugin_path}"
            }
        
        # Find .uplugin file
        uplugin_files = list(plugin_path.glob("*.uplugin"))
        if not uplugin_files:
            return {
                "success": False,
                "error": "No .uplugin file found"
            }
        
        uplugin_file = uplugin_files[0]
        
        try:
            # Run UHT
            cmd = [
                str(self.uht_path),
                str(uplugin_file),
                "-NoEnginePlugins",
                "-WarningsAsErrors"
            ]
            
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=60
            )
            
            return {
                "success": result.returncode == 0,
                "returncode": result.returncode,
                "stdout": result.stdout,
                "stderr": result.stderr,
                "errors": self._parse_uht_errors(result.stderr)
            }
        
        except subprocess.TimeoutExpired:
            return {
                "success": False,
                "error": "UHT execution timed out"
            }
        except Exception as e:
            return {
                "success": False,
                "error": str(e)
            }
    
    def run_ubt_check(self, plugin_path: str, target_name: str = "Development") -> Dict:
        """
        Run UnrealBuildTool to check if plugin compiles
        
        Args:
            plugin_path: Path to plugin directory
            target_name: Build target (Development, Shipping, etc.)
            
        Returns:
            Dict with UBT results
        """
        if not self.ubt_path or not self.ubt_path.exists():
            return {
                "success": False,
                "error": "UnrealBuildTool not found",
                "ubt_path": str(self.ubt_path) if self.ubt_path else None
            }
        
        plugin_path = config.resolve_path(plugin_path)
        
        try:
            # Run UBT in check mode (doesn't actually compile, just validates)
            cmd = [
                str(self.ubt_path),
                "-Mode=QueryTargets",
                f"-Project={plugin_path}"
            ]
            
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=120
            )
            
            return {
                "success": result.returncode == 0,
                "returncode": result.returncode,
                "stdout": result.stdout,
                "stderr": result.stderr,
                "errors": self._parse_ubt_errors(result.stderr)
            }
        
        except subprocess.TimeoutExpired:
            return {
                "success": False,
                "error": "UBT execution timed out"
            }
        except Exception as e:
            return {
                "success": False,
                "error": str(e)
            }
    
    def _parse_uht_errors(self, stderr: str) -> List[Dict]:
        """Parse UHT error messages"""
        errors = []
        
        # UHT error pattern: filename(line): error: message
        error_pattern = r'(.+?)\((\d+)\):\s*(error|warning):\s*(.+)'
        
        for match in re.finditer(error_pattern, stderr):
            errors.append({
                "file": match.group(1),
                "line": int(match.group(2)),
                "severity": match.group(3),
                "message": match.group(4)
            })
        
        return errors
    
    def _parse_ubt_errors(self, stderr: str) -> List[Dict]:
        """Parse UBT error messages"""
        errors = []
        
        # UBT error pattern
        error_pattern = r'(.+?)\((\d+),(\d+)\):\s*(error|warning)\s*(\w+):\s*(.+)'
        
        for match in re.finditer(error_pattern, stderr):
            errors.append({
                "file": match.group(1),
                "line": int(match.group(2)),
                "column": int(match.group(3)),
                "severity": match.group(4),
                "code": match.group(5),
                "message": match.group(6)
            })
        
        return errors
    
    def validate_plugin(self, plugin_path: str, quick_check: bool = True) -> Dict:
        """
        Comprehensive plugin validation
        
        Args:
            plugin_path: Path to plugin
            quick_check: If True, only run syntax checks. If False, run full UHT/UBT
            
        Returns:
            Dict with validation results
        """
        plugin_path = config.resolve_path(plugin_path)
        
        results = {
            "plugin_path": str(plugin_path),
            "syntax_check": None,
            "uht_check": None,
            "ubt_check": None,
            "overall_valid": False
        }
        
        # Find all headers
        header_files = list(plugin_path.rglob("*.h"))
        
        # Run syntax checks on all headers
        syntax_results = []
        for header_file in header_files:
            content = header_file.read_text(encoding='utf-8', errors='ignore')
            plugin_name = plugin_path.name
            
            validation = self.validate_header_syntax(content, plugin_name)
            validation["file"] = str(header_file.relative_to(plugin_path))
            syntax_results.append(validation)
        
        results["syntax_check"] = {
            "files_checked": len(syntax_results),
            "results": syntax_results,
            "total_errors": sum(r["error_count"] for r in syntax_results),
            "total_warnings": sum(r["warning_count"] for r in syntax_results)
        }
        
        # If quick check, stop here
        if quick_check:
            results["overall_valid"] = results["syntax_check"]["total_errors"] == 0
            return results
        
        # Run UHT check
        if self.uht_path and self.uht_path.exists():
            results["uht_check"] = self.run_uht_check(str(plugin_path))
        
        # Run UBT check
        if self.ubt_path and self.ubt_path.exists():
            results["ubt_check"] = self.run_ubt_check(str(plugin_path))
        
        # Determine overall validity
        syntax_valid = results["syntax_check"]["total_errors"] == 0
        uht_valid = results["uht_check"]["success"] if results["uht_check"] else True
        ubt_valid = results["ubt_check"]["success"] if results["ubt_check"] else True
        
        results["overall_valid"] = syntax_valid and uht_valid and ubt_valid
        
        return results
    
    def get_compiler_info(self) -> Dict:
        """Get information about available UE5 tools"""
        return {
            "ue5_engine_path": str(self.ue5_engine_path) if self.ue5_engine_path else None,
            "ue5_source_path": str(self.ue5_source_path) if self.ue5_source_path else None,
            "ubt_available": self.ubt_path.exists() if self.ubt_path else False,
            "uht_available": self.uht_path.exists() if self.uht_path else False,
            "ubt_path": str(self.ubt_path) if self.ubt_path else None,
            "uht_path": str(self.uht_path) if self.uht_path else None
        }
