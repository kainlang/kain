#!/usr/bin/env python3
"""
extract_all_errors.py — Mine EVERY error emission point from the Kain compiler.

Scans ALL .rs files under X:/crates/ (skipping UE5/unreal/target) for:
  * parser_error, rich_parser_error, rich_parser_error_with_code
  * type_error, type_error_with_code
  * KainError::runtime, KainError::type_error, KainError::Io, KainError::{other}
  * layout_overflow_error, duplicate_symbol_error, shadow_builtin_symbol_error
  * DiagnosticCode::*, ErrorKind::*
  * DiagnosticReport::new, DiagnosticBuilder::new

Outputs TSV files to X:/scripts/errors/ + cross-ref against spec.
"""

import os, re, sys
from pathlib import Path
from collections import defaultdict, Counter

# ── Configuration ───────────────────────────────────────────────────────────

CRATES_DIR = Path("X:/crates")
OUTPUT_DIR = Path("X:/scripts/errors")
SPEC_FILE = Path("X:/docs/KAIN_ERROR_SPECS.md")
TSV_SPEC_FILES = [
    Path("X:/docs-tsv/error_codes.tsv"),
    Path("X:/docs-tsv/error_codes_parser.tsv"),
    Path("X:/docs-tsv/error_codes_typechecker.tsv"),
]
EXCLUDE_PATHS = {"ue5", "unreal", "Unreal", "target", ".kain/cache"}

ALL_TSV = OUTPUT_DIR / "errors_extracted_all.tsv"
SUMMARY_TSV = OUTPUT_DIR / "errors_extracted_summary.tsv"
BY_CATEGORY_TSV = OUTPUT_DIR / "errors_extracted_by_category.tsv"
CROSSREF_TSV = OUTPUT_DIR / "errors_extracted_crossref.tsv"

# ── DiagnosticCode -> code_str mapping ──────────────────────────────────────

DIAGNOSTIC_CODE_MAP = {}  # variant_name -> "KAIN-XXXX-NNNN"

def build_diagnostic_code_map():
    code_rs = CRATES_DIR / "error" / "src" / "code.rs"
    if not code_rs.exists():
        print("  [WARN] error/src/code.rs not found")
        return
    with open(code_rs, encoding="utf-8", errors="replace") as f:
        content = f.read()
    for m in re.finditer(
        # Use [\w-]+ to handle codes like KAIN-PULSE-BUDGET-0001
        r'pub const (\w+): Self = Self::new\("(KAIN-[\w-]+-\d+)"\);',
        content,
    ):
        DIAGNOSTIC_CODE_MAP[m.group(1)] = m.group(2)
    print(f"  DiagnosticCode variants in code.rs: {len(DIAGNOSTIC_CODE_MAP)}")

build_diagnostic_code_map()

# ── Category prefix to category name ────────────────────────────────────────

CODE_CATEGORY_MAP = {
    "PARSE": "Parse", "TYPE": "Type", "VALIDATE": "Validation",
    "CODEGEN": "Codegen", "SHADER": "Shader", "EFFECT": "Effect",
    "BORROW": "Borrow", "MEM": "Memory", "WORLD": "World",
    "ACTOR": "Actor", "RUNTIME": "Runtime", "COMPTIME": "Comptime",
    "STATE": "State", "CONVERGE": "Converge", "ENTANGLE": "Entangle",
    "PATCH": "Patch", "IO": "Io", "CONFIG": "Config", "TEST": "Test",
    "INTERNAL": "Internal", "PULSE": "Pulse",
    "PULSE-BUDGET": "PulseBudget",
}

def code_to_category(code_str: str) -> str:
    """Map KAIN-XXXX-NNNN to category name."""
    if not code_str.startswith("KAIN-"):
        return "Other"
    rest = code_str[5:]
    # Find the last hyphen to extract category prefix
    if "-" in rest:
        cat = rest.rsplit("-", 1)[0]  # e.g. "PULSE-BUDGET" or "TYPE"
        return CODE_CATEGORY_MAP.get(cat, cat.capitalize())
    return "Other"


# ── Pattern definitions ─────────────────────────────────────────────────────
# Each pattern: (name, regex, default_category, code_family)
# Regex must capture message or code variant name.
# We use two capture approaches:
#   Group 1 or 2 = string literal or format!("template", ...) content
#   For DiagnosticCode/ErrorKind: group 1 = variant name

# Shared sub-pattern to match either a string literal or format!("...", ...):
#   "literal"  OR  format!("template", ...)  OR  format!("template")
ARG_STR = r'(?:"([^"]*)"|format!\s*\(\s*"([^"]*)"\s*[,)])'

PATTERNS = [
    # ── Parser error functions ──
    (
        "parser_error",
        # Negative lookbehind: NOT preceded by "rich_" to avoid overlap
        r'(?<!rich_)parser_error\s*\(\s*(?:"([^"]*)"|format!\s*\(\s*"([^"]*)"\s*[,)])',
        "Parse",
        "KAIN-PARSE-XXXX",
    ),
    (
        "rich_parser_error",
        r'rich_parser_error\s*\(\s*(?:"([^"]*)"|format!\s*\(\s*"([^"]*)"\s*[,)])',
        "Parse",
        "KAIN-PARSE-XXXX",
    ),
    (
        "rich_parser_error_with_code",
        r'rich_parser_error_with_code\s*\(\s*[^,]+,\s*(?:"([^"]*)"|format!\s*\(\s*"([^"]*)"\s*[,)])',
        "Parse",
        None,  # resolved from the DiagnosticCode arg
    ),
    # ── Type error functions (NOT type_error_with_code) ──
    (
        "type_error",
        r'(?:env\.)?type_error\s*\(\s*(?:"([^"]*)"|format!\s*\(\s*"([^"]*)"\s*[,)])',
        "Type",
        "KAIN-TYPE-XXXX",
    ),
    (
        "type_error_with_code",
        r'type_error_with_code\s*\(\s*[^,]+,\s*(?:"([^"]*)"|format!\s*\(\s*"([^"]*)"\s*[,)])',
        "Type",
        None,  # resolved from the DiagnosticCode arg
    ),
    # ── KainError constructors ──
    (
        "KainError::runtime",
        r'KainError::runtime\s*\(\s*(?:"([^"]*)"|format!\s*\(\s*"([^"]*)"\s*[,)])',
        "Runtime",
        "KAIN-RUNTIME-XXXX",
    ),
    (
        "KainError::type_error",
        r'KainError::type_error\s*\(\s*(?:"([^"]*)"|format!\s*\(\s*"([^"]*)"\s*[,)])',
        "Type",
        "KAIN-TYPE-XXXX",
    ),
    (
        "KainError::Io",
        r'KainError::Io\s*\(\s*(?:"([^"]*)"|format!\s*\(\s*"([^"]*)"\s*[,)])',
        "IO",
        "KAIN-IO-XXXX",
    ),
    # ── Helper error functions ──
    (
        "layout_overflow_error",
        r'layout_overflow_error\s*\(\s*(?:"([^"]*)"|format!\s*\(\s*"([^"]*)"\s*[,)])',
        "Memory",
        "KAIN-MEM-XXXX",
    ),
    (
        "duplicate_symbol_error",
        r'duplicate_symbol_error\s*\(',
        "Type",
        "KAIN-TYPE-0004",
    ),
    (
        "shadow_builtin_symbol_error",
        r'shadow_builtin_symbol_error\s*\(',
        "Type",
        "KAIN-TYPE-0005",
    ),
    # ── Typed code references ──
    (
        "DiagnosticCode",
        r'DiagnosticCode::(\w+)',
        None,  # resolved dynamically
        None,
    ),
    (
        "ErrorKind",
        r'ErrorKind::(\w+)',
        None,
        None,
    ),
    # ── Diagnostic report builders ──
    (
        "DiagnosticReport::new",
        r'DiagnosticReport::new\s*\(',
        "Codegen",
        "KAIN-CODEGEN-XXXX",
    ),
    (
        "DiagnosticBuilder::new",
        r'DiagnosticBuilder::new\s*\(',
        "Codegen",
        "KAIN-CODEGEN-XXXX",
    ),
]


def scan_file(filepath: Path) -> list[dict]:
    """Scan a single .rs file for error emission points."""
    try:
        with open(filepath, encoding="utf-8", errors="replace") as f:
            lines = f.readlines()
        content = "".join(lines)
    except Exception as e:
        return [{
            "file": str(filepath),
            "line": 0,
            "pattern_type": "read_error",
            "message": str(e)[:200],
            "code": "",
            "category": "Other",
            "source_crate": classify_crate(filepath),
        }]

    results = []
    relpath = str(filepath.relative_to(CRATES_DIR.parent).as_posix())

    for pattern_name, regex, default_category, default_code in PATTERNS:
        for match in re.finditer(regex, content):
            line_num = content[: match.start()].count("\n") + 1

            if pattern_name == "DiagnosticCode":
                # pattern_name, message=variant_name, code=KAIN-XXXX-NNNN
                variant = match.group(1)
                code = DIAGNOSTIC_CODE_MAP.get(variant, "")
                cat = code_to_category(code) if code else "Codegen"
                results.append({
                    "file": relpath, "line": line_num,
                    "pattern_type": pattern_name,
                    "message": variant,
                    "code": code,
                    "category": cat,
                    "source_crate": classify_crate(filepath),
                })

            elif pattern_name == "ErrorKind":
                variant = match.group(1)
                # Map ErrorKind variants to proper categories
                ek_category = {
                    "Parse": "Parse", "Type": "Type", "Validation": "Validation",
                    "Codegen": "Codegen", "Effect": "Effect", "Borrow": "Borrow",
                    "Runtime": "Runtime", "World": "World", "Shader": "Shader",
                    "Component": "Actor", "Comptime": "Comptime",
                    "State": "State", "Test": "Test", "Memory": "Memory",
                    "Internal": "Internal", "Converge": "Converge",
                    "Entangle": "Entangle", "Patch": "Patch",
                    "Config": "Config", "Io": "Io",
                }.get(variant, "Other")

                results.append({
                    "file": relpath, "line": line_num,
                    "pattern_type": pattern_name,
                    "message": variant,
                    "code": f"KAIN-{variant.upper()}-XXXX",
                    "category": ek_category,
                    "source_crate": classify_crate(filepath),
                })

            elif pattern_name == "rich_parser_error_with_code":
                # Extract message from group 1 or 2
                msg = match.group(1) or match.group(2) or ""
                # Try to find which DiagnosticCode was used as first arg
                # Match: rich_parser_error_with_code(DiagnosticCode::Foo, ...)
                full_match = match.group(0)
                dc_match = re.search(r'DiagnosticCode::(\w+)', full_match)
                code = ""
                if dc_match:
                    code = DIAGNOSTIC_CODE_MAP.get(dc_match.group(1), "")
                if not code:
                    code = "KAIN-PARSE-XXXX"
                cat = code_to_category(code) if code else "Parse"

                results.append({
                    "file": relpath, "line": line_num,
                    "pattern_type": pattern_name,
                    "message": msg[:300],
                    "code": code,
                    "category": cat,
                    "source_crate": classify_crate(filepath),
                })

            elif pattern_name == "type_error_with_code":
                msg = match.group(1) or match.group(2) or ""
                full_match = match.group(0)
                dc_match = re.search(r'DiagnosticCode::(\w+)', full_match)
                code = ""
                if dc_match:
                    code = DIAGNOSTIC_CODE_MAP.get(dc_match.group(1), "")
                if not code:
                    code = "KAIN-TYPE-XXXX"
                cat = code_to_category(code) if code else "Type"

                results.append({
                    "file": relpath, "line": line_num,
                    "pattern_type": pattern_name,
                    "message": msg[:300],
                    "code": code,
                    "category": cat,
                    "source_crate": classify_crate(filepath),
                })

            elif pattern_name in ("duplicate_symbol_error", "shadow_builtin_symbol_error",
                                  "DiagnosticReport::new", "DiagnosticBuilder::new"):
                inner = match.group(0)
                sm = re.search(r'"([^"]+)"', inner)
                msg = sm.group(1) if sm else inner[:120]
                results.append({
                    "file": relpath, "line": line_num,
                    "pattern_type": pattern_name,
                    "message": msg[:300],
                    "code": default_code or "",
                    "category": default_category or "Other",
                    "source_crate": classify_crate(filepath),
                })

            else:
                # All others: message from group 1 (direct) or 2 (format template)
                msg = match.group(1) or match.group(2) or ""
                results.append({
                    "file": relpath, "line": line_num,
                    "pattern_type": pattern_name,
                    "message": msg[:300],
                    "code": default_code or "",
                    "category": default_category or "Other",
                    "source_crate": classify_crate(filepath),
                })

    return results


def classify_crate(filepath: Path) -> str:
    """Determine which crate a file belongs to."""
    try:
        rel = filepath.relative_to(CRATES_DIR)
        return rel.parts[0]
    except (ValueError, IndexError):
        pass
    return "other"


def is_relevant_file(path: Path) -> bool:
    """Filter: only .rs files, exclude UE5/unreal/target."""
    if path.suffix != ".rs":
        return False
    posix = str(path).replace("\\", "/")
    for ex in EXCLUDE_PATHS:
        if f"/{ex}/" in posix or f"/{ex}" in posix:
            return False
    return True


# ── Scan ────────────────────────────────────────────────────────────────────

def scan_all_crates() -> list[dict]:
    all_results = []
    files_scanned = 0
    files_with_errors = 0

    for filepath in sorted(CRATES_DIR.rglob("*.rs")):
        if not is_relevant_file(filepath):
            continue
        files_scanned += 1
        results = scan_file(filepath)
        if results:
            files_with_errors += 1
            all_results.extend(results)

    return all_results, files_scanned, files_with_errors


def deduplicate(results: list[dict]) -> list[dict]:
    seen = set()
    unique = []
    for r in results:
        key = (r["file"], r["line"], r["pattern_type"], r["message"][:100])
        if key not in seen:
            seen.add(key)
            unique.append(r)
    return unique


# ── Parse error specs ───────────────────────────────────────────────────────

def parse_toml_spec() -> dict:
    """Parse the [[diagnostics]] blocks from the TOML spec file."""
    spec = {}
    if not SPEC_FILE.exists():
        return spec
    with open(SPEC_FILE, encoding="utf-8", errors="replace") as f:
        content = f.read()
    blocks = re.split(r'\[\[diagnostics\]\]', content)
    for block in blocks[1:]:
        code_m = re.search(r'code\s*=\s*"(KAIN-\w+-\d+)"', block)
        title_m = re.search(r'title\s*=\s*"([^"]*)"', block)
        sev_m = re.search(r'severity\s*=\s*"([^"]*)"', block)
        key_m = re.search(r'docs_key\s*=\s*"([^"]*)"', block)
        help_m = re.search(r'help\s*=\s*"""(.*?)"""', block, re.DOTALL)
        code = code_m.group(1) if code_m else "UNKNOWN"
        if code == "UNKNOWN":
            continue
        spec[code] = {
            "title": title_m.group(1) if title_m else "",
            "severity": sev_m.group(1) if sev_m else "error",
            "docs_key": key_m.group(1) if key_m else "",
            "help": help_m.group(1).strip()[:200] if help_m else "",
            "source": "spec_toml",
        }
    return spec


def parse_tsv_specs() -> dict:
    """Parse all three docs-tsv error code files for the full 439-code set."""
    spec = {}
    for tsv_file in TSV_SPEC_FILES:
        if not tsv_file.exists():
            continue
        with open(tsv_file, encoding="utf-8", errors="replace") as f:
            reader = csv.DictReader(f, delimiter="\t")
            for row in reader:
                code = row.get("code", "").strip()
                if code and code.startswith("KAIN-"):
                    spec[code] = {
                        "title": row.get("title", ""),
                        "severity": row.get("severity", "error"),
                        "docs_key": row.get("category", ""),
                        "help": row.get("summary", ""),
                        "source": str(tsv_file),
                    }
    return spec


# ── Output writers ──────────────────────────────────────────────────────────

def write_all_tsv(results: list[dict]):
    with open(ALL_TSV, "w", encoding="utf-8", newline="") as f:
        f.write("category\tfile\tline\tpattern_type\tmessage\tcode\tsource_crate\n")
        for r in sorted(results, key=lambda x: (x["category"], x["file"], x["line"])):
            msg = r["message"].replace("\t", " ").replace("\n", " ").replace("\r", "")
            f.write(f"{r['category']}\t{r['file']}\t{r['line']}\t"
                    f"{r['pattern_type']}\t{msg}\t{r['code']}\t{r['source_crate']}\n")


def write_summary_tsv(results: list[dict]):
    by_file = defaultdict(list)
    for r in results:
        by_file[r["file"]].append(r)

    with open(SUMMARY_TSV, "w", encoding="utf-8", newline="") as f:
        f.write("file\ttotal\tparser_error\ttype_error\tkain_error\tdiagnostic_code\tother\n")
        for filepath in sorted(by_file.keys()):
            entries = by_file[filepath]
            counts = Counter(e["pattern_type"] for e in entries)

            # Group parser categories
            pe = (counts.get("parser_error", 0)
                  + counts.get("rich_parser_error", 0)
                  + counts.get("rich_parser_error_with_code", 0))
            te = (counts.get("type_error", 0)
                  + counts.get("type_error_with_code", 0))
            ke = (counts.get("KainError::runtime", 0)
                  + counts.get("KainError::type_error", 0)
                  + counts.get("KainError::Io", 0))
            dc = (counts.get("DiagnosticCode", 0)
                  + counts.get("ErrorKind", 0))
            ot = (counts.get("layout_overflow_error", 0)
                  + counts.get("duplicate_symbol_error", 0)
                  + counts.get("shadow_builtin_symbol_error", 0)
                  + counts.get("DiagnosticReport::new", 0)
                  + counts.get("DiagnosticBuilder::new", 0))

            f.write(f"{filepath}\t{len(entries)}\t{pe}\t{te}\t{ke}\t{dc}\t{ot}\n")


def write_by_category_tsv(results: list[dict], spec: dict):
    by_cat = defaultdict(list)
    for r in results:
        by_cat[r["category"]].append(r)

    with open(BY_CATEGORY_TSV, "w", encoding="utf-8", newline="") as f:
        f.write("category\ttotal\tfiles\tunique_messages\thas_spec_code\n")
        for cat in sorted(by_cat.keys()):
            entries = by_cat[cat]
            files = set(e["file"] for e in entries)
            unique_msgs = set(e["message"] for e in entries if e["message"])
            has_spec = "yes" if any(e["code"] in spec for e in entries) else "no"
            f.write(f"{cat}\t{len(entries)}\t{len(files)}\t{len(unique_msgs)}\t{has_spec}\n")


def write_crossref_tsv(all_mined: list[dict], spec_toml: dict, spec_tsv: dict):
    """Cross-reference spec codes with mined codes."""
    mined_codes = set()
    for r in all_mined:
        if r["code"] and r["code"].startswith("KAIN-"):
            mined_codes.add(r["code"])

    # Merge both spec sources: TOML takes precedence
    full_spec = {}
    full_spec.update(spec_tsv)
    full_spec.update(spec_toml)  # TOML overrides TSV for matching keys

    with open(CROSSREF_TSV, "w", encoding="utf-8", newline="") as f:
        f.write("code\ttitle\tseverity\tin_spec\tin_compiler\tstatus\tspec_source\n")
        for code in sorted(full_spec.keys()):
            s = full_spec[code]
            in_compiler = "yes" if code in mined_codes else "no"
            status = "implemented" if in_compiler == "yes" else "spec_only"
            f.write(f"{code}\t{s['title']}\t{s['severity']}\tyes\t{in_compiler}\t{status}\t{s.get('source', '')}\n")

        # Compiler-only codes not in any spec
        for code in sorted(mined_codes):
            if code not in full_spec:
                f.write(f"{code}\t\t\tno\tyes\tcompiler_only\t\n")


# ── Main ────────────────────────────────────────────────────────────────────

def main():
    print("=" * 72)
    print("  Kain Compiler Error Extraction — extract_all_errors.py")
    print("=" * 72)
    print()

    # Step 1: Scan
    print("[1/5] Scanning crate source files for error patterns...")
    all_results, files_scanned, files_with_errors = scan_all_crates()
    print(f"  Files scanned:       {files_scanned}")
    print(f"  Files with errors:   {files_with_errors}")
    print(f"  Raw matches:         {len(all_results)}")

    # Step 2: Deduplicate
    print()
    print("[2/5] Deduplicating...")
    unique = deduplicate(all_results)
    print(f"  Unique hits:         {len(unique)}")

    # Step 3: Parse error specs
    print()
    print("[3/5] Parsing error specs...")
    spec_toml = parse_toml_spec()
    spec_tsv = parse_tsv_specs()
    print(f"  TOML spec codes:     {len(spec_toml)}")
    print(f"  TSV spec codes:      {len(spec_tsv)}")

    full_spec = {}
    full_spec.update(spec_tsv)
    full_spec.update(spec_toml)
    print(f"  Combined unique:     {len(full_spec)}")

    # Step 4: Write TSVs
    print()
    print("[4/5] Writing TSV outputs...")
    write_all_tsv(unique)
    write_summary_tsv(unique)
    write_by_category_tsv(unique, full_spec)
    write_crossref_tsv(unique, spec_toml, spec_tsv)
    print(f"  -> {ALL_TSV}")
    print(f"  -> {SUMMARY_TSV}")
    print(f"  -> {BY_CATEGORY_TSV}")
    print(f"  -> {CROSSREF_TSV}")

    # Step 5: Analysis
    print()
    print("[5/5] Analysis...")
    print()

    # Distribution by pattern type (show top patterns)
    pattern_counts = Counter(r["pattern_type"] for r in unique)
    print("  By pattern type:")
    for p, c in sorted(pattern_counts.items(), key=lambda x: -x[1]):
        print(f"    {p:35s} {c:5d}")

    print()
    print("  By pattern type (grouped):")
    emission_patterns = {
        "parser_error*":     sum(pattern_counts.get(n, 0) for n in ["parser_error", "rich_parser_error", "rich_parser_error_with_code"]),
        "type_error*":       sum(pattern_counts.get(n, 0) for n in ["type_error", "type_error_with_code"]),
        "KainError::*":      sum(pattern_counts.get(n, 0) for n in ["KainError::runtime", "KainError::type_error", "KainError::Io"]),
        "DiagnosticCode::*": pattern_counts.get("DiagnosticCode", 0),
        "ErrorKind::*":      pattern_counts.get("ErrorKind", 0),
        "helper_funcs":      sum(pattern_counts.get(n, 0) for n in ["layout_overflow_error", "duplicate_symbol_error", "shadow_builtin_symbol_error"]),
        "report_builders":   sum(pattern_counts.get(n, 0) for n in ["DiagnosticReport::new", "DiagnosticBuilder::new"]),
    }
    for grp, cnt in sorted(emission_patterns.items(), key=lambda x: -x[1]):
        print(f"    {grp:30s} {cnt:5d}")

    # Distribution by category (use proper categories only)
    print()
    cat_counts = Counter(r["category"] for r in unique)
    print("  By error category:")
    for c, n in sorted(cat_counts.items(), key=lambda x: -x[1]):
        if n > 5:
            print(f"    {c:20s} {n:5d}")

    # Spec coverage analysis
    mined_codes = set()
    for r in unique:
        if r["code"] and r["code"].startswith("KAIN-"):
            mined_codes.add(r["code"])

    implemented = mined_codes & set(full_spec.keys())
    spec_only = set(full_spec.keys()) - mined_codes
    compiler_only = mined_codes - set(full_spec.keys())

    # Count which DiagnosticCode variants are actually used (emitted by compiler)
    typed_used = set()
    for r in unique:
        if r["pattern_type"] == "DiagnosticCode":
            typed_used.add(r["message"])

    # Count real error emissions (not just references)
    emission_types = {"parser_error", "rich_parser_error", "rich_parser_error_with_code",
                      "type_error", "type_error_with_code",
                      "KainError::runtime", "KainError::type_error", "KainError::Io",
                      "layout_overflow_error", "duplicate_symbol_error",
                      "shadow_builtin_symbol_error",
                      "DiagnosticReport::new", "DiagnosticBuilder::new"}
    real_emissions = [r for r in unique if r["pattern_type"] in emission_types]

    # Classify emission types
    generic_emission_types = {"parser_error", "rich_parser_error",
                              "type_error",
                              "KainError::runtime", "KainError::type_error", "KainError::Io",
                              "layout_overflow_error",
                              "DiagnosticReport::new", "DiagnosticBuilder::new"}
    specific_emission_types = {"rich_parser_error_with_code", "type_error_with_code",
                               "duplicate_symbol_error", "shadow_builtin_symbol_error"}

    generic_count = sum(1 for r in real_emissions if r["pattern_type"] in generic_emission_types)
    specific_count = sum(1 for r in real_emissions if r["pattern_type"] in specific_emission_types)

    # Count unique format template strings (vs instantiated messages)
    format_template_count = sum(1 for r in unique
                                 if r["pattern_type"] in emission_types
                                 and "{}" in r["message"])

    # Count truly raw string literals (no format interpolation)
    literal_count = sum(1 for r in unique
                         if r["pattern_type"] in emission_types
                         and "{}" not in r["message"]
                         and r["message"])

    print()
    print("  " + "-" * 60)
    print("  SUMMARY")
    print("  " + "-" * 60)
    print(f"  Rust files scanned:                       {files_scanned}")
    print(f"  Files with error emissions:               {files_with_errors}")
    print(f"  Total error-related references:           {len(unique)}")
    print(f"  Real error emission points:               {len(real_emissions)}")
    print(f"    Specific typed codes (with_code):        {specific_count}")
    print(f"    Generic/fallback codes:                  {generic_count}")
    print(f"    Unique format-template messages:         {format_template_count}")
    print(f"    Unique string-literal messages:          {literal_count}")
    print()
    print(f"  Error spec codes (designed):              {len(full_spec)}")
    print(f"    TOML spec ([[diagnostics]] blocks):       {len(spec_toml)}")
    print(f"    TSV docs-tsv files:                      {len(spec_tsv)}")
    print()
    print(f"  DiagnosticCode variants in code.rs:       {len(DIAGNOSTIC_CODE_MAP)}")
    print(f"  DiagnosticCode variants used in source:    {len(typed_used)}")
    print(f"  Typed-code coverage of spec:               {len(typed_used)}/{len(full_spec)} ({len(typed_used)/max(len(full_spec),1)*100:.1f}%)")
    print()
    print(f"  Mined codes matching spec:                 {len(implemented)} ({len(implemented)/max(len(full_spec),1)*100:.1f}%)")
    print(f"  Spec codes NOT found in compiler:          {len(spec_only)}")
    print(f"  Compiler codes NOT found in spec:          {len(compiler_only)}")
    print()

    # Per-crate breakdown
    crate_counts = Counter(r["source_crate"] for r in unique)
    print("  Per-crate breakdown:")
    for crate, n in sorted(crate_counts.items(), key=lambda x: -x[1]):
        if n > 10:
            print(f"    {crate:25s} {n:4d}")

    print()
    # Top files
    file_counts = Counter(r["file"] for r in real_emissions)
    print("  Top files by real error emission count:")
    for fpath, n in file_counts.most_common(15):
        print(f"    {n:5d}  {fpath}")

    print()
    print(f"  Comparison with prior attempt: 208 unique hits")
    print(f"  This extraction (real emissions): {len(real_emissions)}")
    print(f"  This extraction (all references):  {len(unique)}")
    print(f"  Improvement:                       {len(unique)/max(208,1):.1f}x")
    print()
    print("=" * 72)


if __name__ == "__main__":
    # Need csv for reading TSV spec files
    import csv
    main()
