#!/usr/bin/env python3
"""
Module Graph Validation Script

Validates module_graph.json for:
1. Schema compliance
2. Consistency (all referenced modules exist)
3. Circular dependencies
4. Orphaned modules
5. Data quality issues

Usage:
  python validate_module_graph.py <module_graph.json>
  python validate_module_graph.py ../metadata/module_graph.json
"""

import json
import sys
import os
from collections import defaultdict, deque
from pathlib import Path


class ModuleGraphValidator:
    def __init__(self, graph_path):
        self.graph_path = graph_path
        self.graph = None
        self.errors = []
        self.warnings = []
        self.info = []
        
    def load_graph(self):
        """Load and parse the module graph JSON."""
        try:
            with open(self.graph_path, 'r', encoding='utf-8') as f:
                self.graph = json.load(f)
            self.info.append(f"✓ Loaded {self.graph_path}")
            return True
        except FileNotFoundError:
            self.errors.append(f"✗ File not found: {self.graph_path}")
            return False
        except json.JSONDecodeError as e:
            self.errors.append(f"✗ JSON parse error: {e}")
            return False
    
    def validate_schema(self):
        """Validate required top-level fields."""
        required_fields = ["_meta", "modules", "transitive_public_deps", 
                          "type_to_module", "header_to_module", "api_to_module"]
        
        for field in required_fields:
            if field not in self.graph:
                self.errors.append(f"✗ Missing required field: {field}")
            else:
                self.info.append(f"✓ Found field: {field}")
        
        # Validate _meta
        if "_meta" in self.graph:
            meta = self.graph["_meta"]
            required_meta = ["generator", "source", "total_modules", 
                           "total_types_mapped", "total_headers_mapped", 
                           "total_api_symbols", "description"]
            for field in required_meta:
                if field not in meta:
                    self.errors.append(f"✗ Missing _meta field: {field}")
    
    def validate_module_consistency(self):
        """Validate that all referenced modules exist."""
        modules = self.graph.get("modules", {})
        module_names = set(modules.keys())
        
        self.info.append(f"✓ Found {len(module_names)} modules")
        
        # Check each module's dependencies
        missing_deps = defaultdict(set)
        
        for mod_name, mod_info in modules.items():
            # Validate module name matches key
            if mod_info.get("name") != mod_name:
                self.errors.append(f"✗ Module name mismatch: key='{mod_name}' vs name='{mod_info.get('name')}'")
            
            # Check all dependency types
            dep_types = ["public_deps", "private_deps", "dynamic_deps", 
                        "private_include_path_modules", "public_include_path_modules"]
            
            for dep_type in dep_types:
                deps = mod_info.get(dep_type, [])
                for dep in deps:
                    if dep not in module_names:
                        missing_deps[dep].add((mod_name, dep_type))
        
        if missing_deps:
            self.warnings.append(f"⚠ Found {len(missing_deps)} referenced modules that don't exist:")
            for dep, refs in sorted(missing_deps.items()):
                ref_list = ", ".join(f"{mod}({dtype})" for mod, dtype in sorted(refs))
                self.warnings.append(f"  - {dep}: referenced by {ref_list}")
        else:
            self.info.append("✓ All referenced modules exist")
    
    def detect_circular_dependencies(self):
        """Detect circular dependencies in the module graph."""
        modules = self.graph.get("modules", {})
        
        def find_cycle(start_module, visited, rec_stack, path):
            """DFS to find cycles."""
            visited.add(start_module)
            rec_stack.add(start_module)
            path.append(start_module)
            
            # Only follow public deps (they propagate)
            deps = modules.get(start_module, {}).get("public_deps", [])
            
            for dep in deps:
                if dep not in modules:
                    continue
                    
                if dep not in visited:
                    cycle = find_cycle(dep, visited, rec_stack, path)
                    if cycle:
                        return cycle
                elif dep in rec_stack:
                    # Found a cycle
                    cycle_start = path.index(dep)
                    return path[cycle_start:] + [dep]
            
            path.pop()
            rec_stack.remove(start_module)
            return None
        
        visited = set()
        cycles = []
        
        for module in modules:
            if module not in visited:
                cycle = find_cycle(module, visited, set(), [])
                if cycle:
                    cycles.append(cycle)
        
        if cycles:
            self.warnings.append(f"⚠ Found {len(cycles)} circular dependency chains:")
            for i, cycle in enumerate(cycles, 1):
                cycle_str = " → ".join(cycle)
                self.warnings.append(f"  {i}. {cycle_str}")
        else:
            self.info.append("✓ No circular dependencies detected")
    
    def validate_transitive_deps(self):
        """Validate transitive dependency closure."""
        modules = self.graph.get("modules", {})
        transitive = self.graph.get("transitive_public_deps", {})
        
        # Check that all modules have transitive deps computed
        for mod_name in modules:
            if mod_name not in transitive:
                self.warnings.append(f"⚠ Module '{mod_name}' missing transitive deps")
        
        # Check that transitive deps are actually reachable
        for mod_name, trans_deps in transitive.items():
            if mod_name not in modules:
                self.warnings.append(f"⚠ Transitive deps for unknown module: {mod_name}")
                continue
            
            # Compute actual transitive closure
            actual_trans = self._compute_transitive(mod_name, modules)
            
            # Compare
            trans_set = set(trans_deps)
            actual_set = set(actual_trans)
            
            if trans_set != actual_set:
                missing = actual_set - trans_set
                extra = trans_set - actual_set
                if missing:
                    self.warnings.append(f"⚠ Module '{mod_name}' transitive deps missing: {missing}")
                if extra:
                    self.warnings.append(f"⚠ Module '{mod_name}' transitive deps has extra: {extra}")
        
        if not self.warnings:
            self.info.append("✓ Transitive dependencies are correct")
    
    def _compute_transitive(self, module, modules, max_depth=10):
        """Compute transitive public dependencies for a module."""
        visited = set()
        queue = list(modules.get(module, {}).get("public_deps", []))
        depth = 0
        
        while queue and depth < max_depth:
            next_queue = []
            for dep in queue:
                if dep not in visited and dep in modules:
                    visited.add(dep)
                    next_queue.extend(modules[dep].get("public_deps", []))
            queue = next_queue
            depth += 1
        
        return sorted(visited)
    
    def validate_type_mappings(self):
        """Validate type-to-module mappings."""
        modules = self.graph.get("modules", {})
        type_to_module = self.graph.get("type_to_module", {})
        
        # Check that all mapped modules exist
        unknown_modules = set()
        for type_name, module_name in type_to_module.items():
            if module_name not in modules:
                unknown_modules.add(module_name)
        
        if unknown_modules:
            self.warnings.append(f"⚠ {len(unknown_modules)} types map to unknown modules:")
            for mod in sorted(unknown_modules):
                count = sum(1 for m in type_to_module.values() if m == mod)
                self.warnings.append(f"  - {mod}: {count} types")
        else:
            self.info.append(f"✓ All {len(type_to_module)} type mappings reference valid modules")
    
    def validate_header_mappings(self):
        """Validate header-to-module mappings."""
        modules = self.graph.get("modules", {})
        header_to_module = self.graph.get("header_to_module", {})
        
        # Check that all mapped modules exist
        unknown_modules = set()
        for header, module_name in header_to_module.items():
            if module_name not in modules:
                unknown_modules.add(module_name)
        
        if unknown_modules:
            self.warnings.append(f"⚠ {len(unknown_modules)} headers map to unknown modules:")
            for mod in sorted(unknown_modules):
                count = sum(1 for m in header_to_module.values() if m == mod)
                self.warnings.append(f"  - {mod}: {count} headers")
        else:
            self.info.append(f"✓ All {len(header_to_module)} header mappings reference valid modules")
    
    def validate_api_mappings(self):
        """Validate API-to-module mappings."""
        modules = self.graph.get("modules", {})
        api_to_module = self.graph.get("api_to_module", {})
        
        # Check that all mapped modules exist
        unknown_modules = set()
        for api, module_name in api_to_module.items():
            if module_name not in modules:
                unknown_modules.add(module_name)
        
        if unknown_modules:
            self.warnings.append(f"⚠ {len(unknown_modules)} API symbols map to unknown modules:")
            for mod in sorted(unknown_modules):
                apis = [a for a, m in api_to_module.items() if m == mod]
                self.warnings.append(f"  - {mod}: {', '.join(apis)}")
        else:
            self.info.append(f"✓ All {len(api_to_module)} API mappings reference valid modules")
    
    def find_orphaned_modules(self):
        """Find modules that no other module depends on."""
        modules = self.graph.get("modules", {})
        
        # Build reverse dependency map
        dependents = defaultdict(set)
        for mod_name, mod_info in modules.items():
            for dep_type in ["public_deps", "private_deps"]:
                for dep in mod_info.get(dep_type, []):
                    if dep in modules:
                        dependents[dep].add(mod_name)
        
        # Find modules with no dependents
        orphaned = []
        for mod_name in modules:
            if mod_name not in dependents:
                category = modules[mod_name].get("category", "Unknown")
                orphaned.append((mod_name, category))
        
        if orphaned:
            self.info.append(f"ℹ Found {len(orphaned)} orphaned modules (no dependents):")
            by_category = defaultdict(list)
            for mod, cat in orphaned:
                by_category[cat].append(mod)
            for cat, mods in sorted(by_category.items()):
                self.info.append(f"  {cat}: {len(mods)} modules")
                if len(mods) <= 10:
                    for mod in sorted(mods):
                        self.info.append(f"    - {mod}")
        else:
            self.info.append("✓ No orphaned modules (all have dependents)")
    
    def analyze_critical_modules(self):
        """Find the most depended-upon modules."""
        modules = self.graph.get("modules", {})
        
        # Count dependents for each module
        dependent_count = defaultdict(int)
        for mod_info in modules.values():
            for dep_type in ["public_deps", "private_deps"]:
                for dep in mod_info.get(dep_type, []):
                    if dep in modules:
                        dependent_count[dep] += 1
        
        # Sort by count
        critical = sorted(dependent_count.items(), key=lambda x: x[1], reverse=True)[:20]
        
        if critical:
            self.info.append(f"ℹ Top 20 most depended-upon modules:")
            for mod, count in critical:
                category = modules[mod].get("category", "Unknown")
                self.info.append(f"  {mod} ({category}): {count} dependents")
    
    def validate(self):
        """Run all validation checks."""
        print(f"Validating {self.graph_path}...")
        print()
        
        if not self.load_graph():
            return False
        
        print("Running validation checks...")
        print()
        
        self.validate_schema()
        self.validate_module_consistency()
        self.detect_circular_dependencies()
        self.validate_transitive_deps()
        self.validate_type_mappings()
        self.validate_header_mappings()
        self.validate_api_mappings()
        self.find_orphaned_modules()
        self.analyze_critical_modules()
        
        return True
    
    def print_report(self):
        """Print validation report."""
        print()
        print("=" * 60)
        print("VALIDATION REPORT")
        print("=" * 60)
        print()
        
        if self.errors:
            print(f"ERRORS ({len(self.errors)}):")
            for error in self.errors:
                print(f"  {error}")
            print()
        
        if self.warnings:
            print(f"WARNINGS ({len(self.warnings)}):")
            for warning in self.warnings:
                print(f"  {warning}")
            print()
        
        if self.info:
            print(f"INFO ({len(self.info)}):")
            for info in self.info:
                print(f"  {info}")
            print()
        
        print("=" * 60)
        if self.errors:
            print("RESULT: FAILED ✗")
            return False
        elif self.warnings:
            print("RESULT: PASSED WITH WARNINGS ⚠")
            return True
        else:
            print("RESULT: PASSED ✓")
            return True


def main():
    if len(sys.argv) < 2:
        print("Usage: python validate_module_graph.py <module_graph.json>")
        sys.exit(1)
    
    graph_path = sys.argv[1]
    
    validator = ModuleGraphValidator(graph_path)
    if validator.validate():
        success = validator.print_report()
        sys.exit(0 if success else 1)
    else:
        validator.print_report()
        sys.exit(1)


if __name__ == "__main__":
    main()
