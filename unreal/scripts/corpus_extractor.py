"""
KAIN Corpus Extractor v1
Scans a massive corpus of UE5 plugins/projects and extracts:
  Pass 1: Type Registry   — UCLASS/USTRUCT/UENUM + includes + modules + hierarchy
  Pass 2: Widget Registry  — SWidget subclasses, properties, slots, delegate types
  Pass 3: Codegen Rules    — include co-occurrence, constructor patterns, frequency data

Designed for "trash can" input: recursively walks any folder structure,
ignores non-C++ files, deduplicates by type name, handles encoding errors.

Outputs:
  - engine_knowledge_expanded.json  (feeds into EngineKnowledge)
  - widget_registry.json            (new: Slate widget database)
  - codegen_rules.json              (new: structural patterns)

Usage:
    python corpus_extractor.py <corpus_dir> [corpus_dir2...] --output <output_dir>
    python corpus_extractor.py <corpus_dir> --output <output_dir> --stats-only
"""

import os
import re
import json
import sys
import time
from pathlib import Path
from collections import defaultdict


# ═══════════════════════════════════════════════════════════════════
# Pass 1: Type Registry Extractor
# ═══════════════════════════════════════════════════════════════════

class TypeRegistryExtractor:
    """Extracts UCLASS, USTRUCT, UENUM declarations from C++ headers."""

    def __init__(self):
        self.class_pattern = re.compile(
            r'UCLASS\((.*?)\)\s+class\s+(?:[\w_]*?API\s+)?(\w+)\s*(?::\s*public\s+([\w:,\s]+?))?(?:\s*\{)',
            re.DOTALL
        )
        self.struct_pattern = re.compile(
            r'USTRUCT\((.*?)\)\s+struct\s+(?:[\w_]*?API\s+)?(\w+)\s*(?::\s*public\s+([\w:,\s]+?))?(?:\s*\{)',
            re.DOTALL
        )
        self.enum_pattern = re.compile(
            r'UENUM\((.*?)\)\s+enum\s+(?:class\s+)?(\w+)(?:\s*:\s*\w+)?\s*\{([^}]*)\}',
            re.DOTALL
        )
        self.include_pattern = re.compile(r'#include\s+"([^"]+)"')

        # Results (deduped by name)
        self.classes = {}      # name -> class_info
        self.structs = {}      # name -> struct_info
        self.enums = {}        # name -> enum_info
        self.include_map = {}  # type_name -> header_path
        self.module_map = {}   # type_name -> module_name

        # Stats
        self.files_scanned = 0
        self.files_with_types = 0
        self.duplicates_skipped = 0

    def scan_directory(self, root_dir):
        """Recursively scan all .h files in a directory tree."""
        root_path = Path(root_dir)
        if not root_path.exists():
            print(f"  ⚠️  Path not found: {root_dir}")
            return

        h_files = list(root_path.rglob("*.h"))
        total = len(h_files)
        print(f"  📁 Found {total:,} header files")

        for i, header_path in enumerate(h_files):
            if i > 0 and i % 5000 == 0:
                print(f"  📊 Progress: {i:,}/{total:,} files ({i*100//total}%)")

            self.scan_file(str(header_path), str(root_dir))

    def scan_file(self, file_path, root_dir):
        """Scan a single header file."""
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
        except Exception:
            return

        self.files_scanned += 1
        found_any = False

        header_rel = self._compute_header_path(file_path, root_dir)
        module = self._guess_module(file_path, header_rel)

        # === CLASSES ===
        for match in self.class_pattern.finditer(content):
            meta = match.group(1).strip()
            name = match.group(2)
            parent_raw = match.group(3) or ""
            parent = parent_raw.split(',')[0].strip() if parent_raw else ""
            parent = re.sub(r'<.*>', '', parent).strip()

            if name in self.classes:
                self.duplicates_skipped += 1
                continue

            prefix = "A" if name.startswith("A") and len(name) > 1 and name[1].isupper() else "U"
            is_abstract = "Abstract" in meta or "ABSTRACT" in meta

            self.classes[name] = {
                "name": name,
                "parent": parent,
                "header": header_rel,
                "module": module,
                "prefix": prefix,
                "is_abstract": is_abstract,
            }
            self.include_map[name] = header_rel
            self.module_map[name] = module
            found_any = True

        # === STRUCTS ===
        for match in self.struct_pattern.finditer(content):
            name = match.group(2)
            parent_raw = match.group(3) or ""
            parent = parent_raw.split(',')[0].strip() if parent_raw else ""

            if name in self.structs:
                self.duplicates_skipped += 1
                continue

            is_table_row = "FTableRowBase" in parent

            self.structs[name] = {
                "name": name,
                "parent": parent,
                "header": header_rel,
                "module": module,
                "is_table_row": is_table_row,
            }
            self.include_map[name] = header_rel
            self.module_map[name] = module
            found_any = True

        # === ENUMS ===
        for match in self.enum_pattern.finditer(content):
            name = match.group(2)
            body = match.group(3)
            meta = match.group(1).strip()

            if name in self.enums:
                self.duplicates_skipped += 1
                continue

            is_flags = "Flags" in meta or "UMETA(Bitflags)" in body

            values = []
            for line in body.split('\n'):
                line = line.strip().rstrip(',')
                line = re.sub(r'//.*', '', line).strip()
                line = re.sub(r'UMETA\(.*?\)', '', line).strip()
                if not line or line.startswith('#'):
                    continue
                val_name = line.split('=')[0].split('UMETA')[0].strip().rstrip(',')
                if val_name and val_name not in ('{', '}', ''):
                    values.append(val_name)

            # Filter _MAX
            values = [v for v in values if not v.endswith('_MAX') and v]

            self.enums[name] = {
                "name": name,
                "header": header_rel,
                "module": module,
                "values": values,
                "is_flags": is_flags,
            }
            self.include_map[name] = header_rel
            self.module_map[name] = module
            found_any = True

        if found_any:
            self.files_with_types += 1

    def _compute_header_path(self, file_path, root_dir):
        """Compute UE5-style include path relative to Public/ or Classes/."""
        rel = os.path.relpath(file_path, root_dir).replace('\\', '/')
        for marker in ('Public/', 'Classes/'):
            idx = rel.find(marker)
            if idx != -1:
                return rel[idx + len(marker):]
        return os.path.basename(file_path)

    def _guess_module(self, file_path, header_rel):
        """Guess the UE5 module from the file path."""
        parts = Path(file_path).parts
        for i, part in enumerate(parts):
            if part in ('Public', 'Private', 'Classes'):
                if i > 0:
                    return parts[i - 1]
        # Heuristic fallbacks
        path_str = file_path.replace('\\', '/')
        for mod_name in ['Niagara', 'EnhancedInput', 'GameplayAbilities',
                         'AIModule', 'NavigationSystem', 'UMG', 'Slate',
                         'SlateCore', 'PropertyEditor', 'UnrealEd',
                         'AnimGraphRuntime', 'PhysicsCore', 'Chaos',
                         'PCG', 'MassEntity', 'StateTree', 'SmartObjects',
                         'GeometryCore', 'GeometryFramework',
                         'CommonUI', 'CommonInput', 'GameplayTags',
                         'GameplayTasks', 'MovieScene', 'LevelSequence',
                         'MediaAssets', 'AudioMixer', 'SignificanceManager']:
            if mod_name in path_str:
                return mod_name
        return 'Engine'

    def get_results(self):
        """Return the extracted data in EngineKnowledge-compatible format."""
        return {
            "engine_version": "5.4+corpus",
            "classes": list(self.classes.values()),
            "structs": list(self.structs.values()),
            "enums": list(self.enums.values()),
            "type_aliases": [],
            "include_map": self.include_map,
        }

    def print_stats(self):
        print(f"\n  📊 Pass 1 Results:")
        print(f"     Files scanned:     {self.files_scanned:,}")
        print(f"     Files with types:  {self.files_with_types:,}")
        print(f"     Classes:           {len(self.classes):,}")
        print(f"     Structs:           {len(self.structs):,}")
        print(f"     Enums:             {len(self.enums):,}")
        print(f"     Include mappings:  {len(self.include_map):,}")
        print(f"     Duplicates skipped:{self.duplicates_skipped:,}")
        total = len(self.classes) + len(self.structs) + len(self.enums)
        print(f"     Total unique types:{total:,}")


# ═══════════════════════════════════════════════════════════════════
# Pass 2: Widget Registry Extractor
# ═══════════════════════════════════════════════════════════════════

class WidgetRegistryExtractor:
    """Extracts Slate widget classes, their properties, slots, and delegate types."""

    def __init__(self):
        # Match: class SMyWidget : public SCompoundWidget
        self.widget_class_pattern = re.compile(
            r'class\s+(?:[\w_]*?API\s+)?(S\w+)\s*:\s*public\s+(S\w+)',
        )
        # Match: SLATE_ARGUMENT(FText, Label)
        self.slate_arg_pattern = re.compile(
            r'SLATE_ARGUMENT\s*\(\s*([^,]+?)\s*,\s*(\w+)\s*\)'
        )
        # Match: SLATE_ATTRIBUTE(float, Value)
        self.slate_attr_pattern = re.compile(
            r'SLATE_ATTRIBUTE\s*\(\s*([^,]+?)\s*,\s*(\w+)\s*\)'
        )
        # Match: SLATE_EVENT(FOnClicked, OnClicked)
        self.slate_event_pattern = re.compile(
            r'SLATE_EVENT\s*\(\s*([^,]+?)\s*,\s*(\w+)\s*\)'
        )
        # Match: SLATE_STYLE_ARGUMENT(FButtonStyle, ButtonStyle)
        self.slate_style_pattern = re.compile(
            r'SLATE_STYLE_ARGUMENT\s*\(\s*([^,]+?)\s*,\s*(\w+)\s*\)'
        )
        # Match: SLATE_NAMED_SLOT(FArguments, Content)
        self.slate_named_slot_pattern = re.compile(
            r'SLATE_NAMED_SLOT\s*\(\s*[^,]+?\s*,\s*(\w+)\s*\)'
        )
        # Match: SLATE_DEFAULT_SLOT(FArguments, Content)
        self.slate_default_slot_pattern = re.compile(
            r'SLATE_DEFAULT_SLOT\s*\(\s*[^,]+?\s*,\s*(\w+)\s*\)'
        )
        # Match: SLATE_SUPPORTS_SLOT(SMyWidget)  or  SLATE_SUPPORTS_SLOT_WITH_ARGS
        self.slate_supports_slot_pattern = re.compile(
            r'SLATE_SUPPORTS_SLOT\w*\s*\(\s*(\w+)'
        )

        # Delegate type patterns (for extracting delegate signatures)
        # DECLARE_DELEGATE_RetVal(FReply)
        self.delegate_retval_pattern = re.compile(
            r'DECLARE_DELEGATE_RetVal\s*\(\s*(\w+)\s*,\s*(\w+)\s*\)'
        )
        # DECLARE_DELEGATE_OneParam(FOnFloatValueChanged, float)
        self.delegate_oneparam_pattern = re.compile(
            r'DECLARE_DELEGATE_OneParam\s*\(\s*(\w+)\s*,\s*([^)]+?)\s*\)'
        )
        # DECLARE_DELEGATE_TwoParams(FOnTextCommitted, const FText&, ETextCommit::Type)
        self.delegate_twoparams_pattern = re.compile(
            r'DECLARE_DELEGATE_TwoParams\s*\(\s*(\w+)\s*,\s*([^,]+?)\s*,\s*([^)]+?)\s*\)'
        )

        self.widgets = {}       # widget_name -> widget_info
        self.delegates = {}     # delegate_name -> delegate_info
        self.files_scanned = 0

    def scan_directory(self, root_dir):
        """Recursively scan all .h files for Slate widgets."""
        root_path = Path(root_dir)
        if not root_path.exists():
            return

        h_files = list(root_path.rglob("*.h"))
        total = len(h_files)
        print(f"  📁 Scanning {total:,} headers for Slate widgets...")

        for i, header_path in enumerate(h_files):
            if i > 0 and i % 5000 == 0:
                print(f"  📊 Progress: {i:,}/{total:,} files ({i*100//total}%)")
            self.scan_file(str(header_path), str(root_dir))

    def scan_file(self, file_path, root_dir):
        """Scan a single header for Slate widget declarations."""
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
        except Exception:
            return

        self.files_scanned += 1

        # Skip files that clearly aren't Slate-related
        if 'SLATE_BEGIN_ARGS' not in content and 'SCompoundWidget' not in content and 'SLeafWidget' not in content:
            return

        header_rel = self._compute_header_path(file_path, root_dir)

        # Extract delegate types first (needed for event resolution)
        for match in self.delegate_retval_pattern.finditer(content):
            ret_type = match.group(1)
            delegate_name = match.group(2)
            self.delegates[delegate_name] = {
                "name": delegate_name,
                "return_type": ret_type,
                "params": [],
            }

        for match in self.delegate_oneparam_pattern.finditer(content):
            delegate_name = match.group(1)
            param_type = match.group(2).strip()
            self.delegates[delegate_name] = {
                "name": delegate_name,
                "return_type": "void",
                "params": [param_type],
            }

        for match in self.delegate_twoparams_pattern.finditer(content):
            delegate_name = match.group(1)
            param1 = match.group(2).strip()
            param2 = match.group(3).strip()
            self.delegates[delegate_name] = {
                "name": delegate_name,
                "return_type": "void",
                "params": [param1, param2],
            }

        # Extract widget classes
        for match in self.widget_class_pattern.finditer(content):
            widget_name = match.group(1)
            parent_name = match.group(2)

            if widget_name in self.widgets:
                continue

            # Find the SLATE_BEGIN_ARGS block for this widget
            args_block = self._extract_args_block(content, widget_name)

            properties = {}
            events = {}
            slots = []

            if args_block:
                # Extract SLATE_ARGUMENT entries
                for arg_match in self.slate_arg_pattern.finditer(args_block):
                    arg_type = arg_match.group(1).strip()
                    arg_name = arg_match.group(2).strip()
                    properties[arg_name] = {
                        "type": arg_type,
                        "kind": "argument",
                    }

                # Extract SLATE_ATTRIBUTE entries
                for attr_match in self.slate_attr_pattern.finditer(args_block):
                    attr_type = attr_match.group(1).strip()
                    attr_name = attr_match.group(2).strip()
                    properties[attr_name] = {
                        "type": attr_type,
                        "kind": "attribute",
                    }

                # Extract SLATE_EVENT entries
                for evt_match in self.slate_event_pattern.finditer(args_block):
                    evt_type = evt_match.group(1).strip()
                    evt_name = evt_match.group(2).strip()
                    events[evt_name] = {
                        "delegate_type": evt_type,
                    }

                # Extract SLATE_STYLE_ARGUMENT entries
                for style_match in self.slate_style_pattern.finditer(args_block):
                    style_type = style_match.group(1).strip()
                    style_name = style_match.group(2).strip()
                    properties[style_name] = {
                        "type": f"const {style_type}*",
                        "kind": "style",
                    }

                # Extract slots
                for slot_match in self.slate_named_slot_pattern.finditer(args_block):
                    slots.append({"name": slot_match.group(1), "kind": "named"})

                for slot_match in self.slate_default_slot_pattern.finditer(args_block):
                    slots.append({"name": slot_match.group(1), "kind": "default"})

                for slot_match in self.slate_supports_slot_pattern.finditer(args_block):
                    slots.append({"name": "_Children", "kind": "multi", "slot_class": slot_match.group(1)})

            self.widgets[widget_name] = {
                "name": widget_name,
                "parent": parent_name,
                "header": header_rel,
                "properties": properties,
                "events": events,
                "slots": slots,
            }

    def _extract_args_block(self, content, widget_name):
        """Extract the SLATE_BEGIN_ARGS ... SLATE_END_ARGS block for a widget."""
        # Look for SLATE_BEGIN_ARGS(SWidgetName) ... SLATE_END_ARGS()
        pattern = re.compile(
            r'SLATE_BEGIN_ARGS\s*\(\s*' + re.escape(widget_name) + r'\s*\)(.*?)SLATE_END_ARGS\s*\(\s*\)',
            re.DOTALL
        )
        match = pattern.search(content)
        if match:
            return match.group(1)
        return None

    def _compute_header_path(self, file_path, root_dir):
        rel = os.path.relpath(file_path, root_dir).replace('\\', '/')
        for marker in ('Public/', 'Classes/'):
            idx = rel.find(marker)
            if idx != -1:
                return rel[idx + len(marker):]
        return os.path.basename(file_path)

    def get_results(self):
        return {
            "widgets": self.widgets,
            "delegates": self.delegates,
        }

    def print_stats(self):
        print(f"\n  📊 Pass 2 Results:")
        print(f"     Files scanned:     {self.files_scanned:,}")
        print(f"     Widgets found:     {len(self.widgets):,}")
        print(f"     Delegates found:   {len(self.delegates):,}")
        total_props = sum(len(w['properties']) for w in self.widgets.values())
        total_events = sum(len(w['events']) for w in self.widgets.values())
        total_slots = sum(len(w['slots']) for w in self.widgets.values())
        print(f"     Total properties:  {total_props:,}")
        print(f"     Total events:      {total_events:,}")
        print(f"     Total slots:       {total_slots:,}")


# ═══════════════════════════════════════════════════════════════════
# Pass 3: Codegen Rules Extractor
# ═══════════════════════════════════════════════════════════════════

class CodegenRulesExtractor:
    """Extracts structural patterns and frequency data from C++ source."""

    def __init__(self):
        # Track include co-occurrence: when type X is used, what includes appear?
        self.include_cooccurrence = defaultdict(lambda: defaultdict(int))
        # Track constructor patterns: how are components initialized?
        self.constructor_patterns = defaultdict(int)
        # Track common parent classes
        self.parent_class_frequency = defaultdict(int)
        # Track replication patterns
        self.replication_count = 0
        self.replication_with_getlifetime = 0
        # Track module dependencies from Build.cs files
        self.module_deps = defaultdict(lambda: defaultdict(int))

        self.files_scanned = 0

    def scan_directory(self, root_dir):
        """Scan .h, .cpp, and .Build.cs files for patterns."""
        root_path = Path(root_dir)
        if not root_path.exists():
            return

        # Scan .cpp files for constructor patterns
        cpp_files = list(root_path.rglob("*.cpp"))
        print(f"  📁 Scanning {len(cpp_files):,} source files for patterns...")

        for i, cpp_path in enumerate(cpp_files):
            if i > 0 and i % 5000 == 0:
                print(f"  📊 Progress: {i:,}/{len(cpp_files):,} ({i*100//len(cpp_files)}%)")
            self._scan_cpp(str(cpp_path))

        # Scan Build.cs files for module dependencies
        build_files = list(root_path.rglob("*.Build.cs"))
        print(f"  📁 Scanning {len(build_files):,} Build.cs files...")
        for build_path in build_files:
            self._scan_build_cs(str(build_path))

    def _scan_cpp(self, file_path):
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
        except Exception:
            return

        self.files_scanned += 1

        # Detect CreateDefaultSubobject patterns
        for match in re.finditer(r'CreateDefaultSubobject<(\w+)>\s*\(\s*TEXT\(\s*"([^"]+)"\s*\)', content):
            comp_type = match.group(1)
            self.constructor_patterns[f"CreateDefaultSubobject<{comp_type}>"] += 1

        # Detect SetupAttachment patterns
        if 'SetupAttachment' in content:
            self.constructor_patterns["SetupAttachment(RootComponent)"] += 1

        # Detect replication patterns
        if 'GetLifetimeReplicatedProps' in content:
            self.replication_with_getlifetime += 1
        if 'DOREPLIFETIME' in content:
            self.replication_count += 1

        # Detect tick patterns
        if 'PrimaryActorTick.bCanEverTick = true' in content:
            self.constructor_patterns["PrimaryActorTick.bCanEverTick = true"] += 1
        if 'bReplicates = true' in content:
            self.constructor_patterns["bReplicates = true"] += 1

    def _scan_build_cs(self, file_path):
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
        except Exception:
            return

        # Extract module name from class declaration
        module_match = re.search(r'public\s+class\s+(\w+)\s*:\s*ModuleRules', content)
        if not module_match:
            return
        module_name = module_match.group(1)

        # Extract dependencies
        for match in re.finditer(r'"(\w+)"', content):
            dep = match.group(1)
            if dep != module_name:
                self.module_deps[module_name][dep] += 1

    def get_results(self):
        # Build codegen rules from frequency data
        rules = []

        # Rule: Component constructor pattern
        top_components = sorted(self.constructor_patterns.items(), key=lambda x: -x[1])[:50]
        for pattern, freq in top_components:
            if freq >= 5:
                rules.append({
                    "pattern": pattern,
                    "frequency": freq,
                    "category": "constructor",
                })

        # Rule: Replication
        if self.replication_count > 0:
            rules.append({
                "pattern": "DOREPLIFETIME requires GetLifetimeReplicatedProps",
                "frequency": self.replication_count,
                "with_getlifetime": self.replication_with_getlifetime,
                "category": "replication",
            })

        # Module dependency frequency
        module_dep_freq = defaultdict(int)
        for module, deps in self.module_deps.items():
            for dep, count in deps.items():
                module_dep_freq[dep] += 1

        top_deps = sorted(module_dep_freq.items(), key=lambda x: -x[1])[:30]

        return {
            "codegen_rules": rules,
            "constructor_patterns": dict(self.constructor_patterns),
            "module_dependency_frequency": dict(top_deps),
            "parent_class_frequency": dict(self.parent_class_frequency),
        }

    def print_stats(self):
        print(f"\n  📊 Pass 3 Results:")
        print(f"     Source files scanned: {self.files_scanned:,}")
        print(f"     Constructor patterns: {len(self.constructor_patterns):,}")
        print(f"     Replication uses:     {self.replication_count:,}")
        print(f"     Build.cs modules:     {len(self.module_deps):,}")


# ═══════════════════════════════════════════════════════════════════
# Main Orchestrator
# ═══════════════════════════════════════════════════════════════════

def main():
    import argparse
    parser = argparse.ArgumentParser(
        description='KAIN Corpus Extractor - Extract UE5 type intelligence from plugin corpus'
    )
    parser.add_argument('corpus_dirs', nargs='+', help='Directories to scan (recursive)')
    parser.add_argument('--output', '-o', default='.', help='Output directory for JSON files')
    parser.add_argument('--stats-only', action='store_true', help='Only print stats, no output files')
    parser.add_argument('--pass1-only', action='store_true', help='Only run Pass 1 (Type Registry)')
    parser.add_argument('--pass2-only', action='store_true', help='Only run Pass 2 (Widget Registry)')
    parser.add_argument('--pass3-only', action='store_true', help='Only run Pass 3 (Codegen Rules)')
    args = parser.parse_args()

    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)

    run_all = not (args.pass1_only or args.pass2_only or args.pass3_only)

    print("=" * 70)
    print("🔍 KAIN Corpus Extractor v1")
    print("=" * 70)
    print(f"📂 Corpus directories: {len(args.corpus_dirs)}")
    for d in args.corpus_dirs:
        print(f"   → {d}")
    print(f"📁 Output: {output_dir}")
    print()

    start_time = time.time()

    # ═══ PASS 1: Type Registry ═══
    if run_all or args.pass1_only:
        print("━" * 70)
        print("📋 PASS 1: Type Registry (UCLASS/USTRUCT/UENUM)")
        print("━" * 70)
        type_extractor = TypeRegistryExtractor()
        for corpus_dir in args.corpus_dirs:
            print(f"\n🔍 Scanning: {corpus_dir}")
            type_extractor.scan_directory(corpus_dir)
        type_extractor.print_stats()

        if not args.stats_only:
            out_path = output_dir / "engine_knowledge_expanded.json"
            results = type_extractor.get_results()
            with open(out_path, 'w', encoding='utf-8') as f:
                json.dump(results, f, indent=2, ensure_ascii=False)
            print(f"\n  💾 Saved: {out_path} ({os.path.getsize(out_path) / 1024:.0f} KB)")

    # ═══ PASS 2: Widget Registry ═══
    if run_all or args.pass2_only:
        print(f"\n{'━' * 70}")
        print("🎨 PASS 2: Widget Registry (Slate widgets)")
        print("━" * 70)
        widget_extractor = WidgetRegistryExtractor()
        for corpus_dir in args.corpus_dirs:
            print(f"\n🔍 Scanning: {corpus_dir}")
            widget_extractor.scan_directory(corpus_dir)
        widget_extractor.print_stats()

        if not args.stats_only:
            out_path = output_dir / "widget_registry.json"
            results = widget_extractor.get_results()
            with open(out_path, 'w', encoding='utf-8') as f:
                json.dump(results, f, indent=2, ensure_ascii=False)
            print(f"\n  💾 Saved: {out_path} ({os.path.getsize(out_path) / 1024:.0f} KB)")

    # ═══ PASS 3: Codegen Rules ═══
    if run_all or args.pass3_only:
        print(f"\n{'━' * 70}")
        print("⚙️  PASS 3: Codegen Rules (patterns & frequency)")
        print("━" * 70)
        rules_extractor = CodegenRulesExtractor()
        for corpus_dir in args.corpus_dirs:
            print(f"\n🔍 Scanning: {corpus_dir}")
            rules_extractor.scan_directory(corpus_dir)
        rules_extractor.print_stats()

        if not args.stats_only:
            out_path = output_dir / "codegen_rules.json"
            results = rules_extractor.get_results()
            with open(out_path, 'w', encoding='utf-8') as f:
                json.dump(results, f, indent=2, ensure_ascii=False)
            print(f"\n  💾 Saved: {out_path} ({os.path.getsize(out_path) / 1024:.0f} KB)")

    # ═══ Summary ═══
    elapsed = time.time() - start_time
    print(f"\n{'=' * 70}")
    print(f"✅ Extraction complete in {elapsed:.1f}s")
    print(f"{'=' * 70}")


if __name__ == '__main__':
    main()
