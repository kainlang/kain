#!/usr/bin/env python3
"""
mine_compiler_errors.py — Extract EVERY error emission point from the Kain compiler source.

Scans all Rust source files across core, error-semantic, error, and sys-codegen crates.
Outputs TSV files cataloging every parse_error(), DiagnosticReport, bail!, anyhow!,
DiagnosticCode reference, Err() return with message, and KainError construction.

Output: X:/docs-tsv/errors_mined_compiler.tsv (master)
        X:/docs-tsv/errors_mined_by_file.tsv (per-file breakdown)
"""

import os, re, sys
from pathlib import Path
from collections import defaultdict

CRATES = [
    "X:/crates/core",
    "X:/crates/sys-codegen",
]

# Also scan error corpus fixtures for error code references
ERROR_CORPUS = "X:/crates/semantic/error_corpus"

OUTPUT_TSV = Path("X:/docs-tsv/errors_mined_compiler.tsv")
OUTPUT_BY_FILE = Path("X:/docs-tsv/errors_mined_by_file.tsv")
OUTPUT_SUMMARY = Path("X:/docs-tsv/errors_mined_summary.tsv")

# Patterns to match — each is (name, regex, group_for_message)
PATTERNS = [
    ("parser_error",       r'parser_error\s*\(\s*"([^"]+)"',         1),
    ("parser_error_var",   r'parser_error\s*\(\s*(\w+)',              1),
    ("rich_parser_error",  r'rich_parser_error\s*\(\s*"([^"]+)"',    1),
    ("DiagnosticReport",   r'DiagnosticReport::new\s*\([^)]*\)',     0),
    ("DiagnosticCode::",   r'DiagnosticCode::(\w+)',                  1),
    ("bail!",              r'\bbail!\s*\(\s*"([^"]+)"',              1),
    ("anyhow!",            r'\banyhow!\s*\(\s*"([^"]+)"',            1),
    ("Err(",               r'Err\s*\(\s*(KainError|format_err|anyhow)', 1),
    ("KainError::",        r'KainError::(\w+)',                       1),
    ("add_diag",           r'add_diag\s*\(\s*[^)]*',                 0),
    ("emit_err",           r'emit_err\s*\(\s*"([^"]+)"',             1),
    ("error! (log)",       r'error!\s*\(\s*"([^"]+)"',               1),
]


def scan_file(filepath: Path) -> list[dict]:
    """Scan a single Rust file for all error emission points."""
    try:
        with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
            content = f.read()
    except Exception as e:
        return [{'file': str(filepath), 'line': 0, 'pattern': 'read_error', 'message': str(e), 'code': ''}]

    lines = content.split('\n')
    results = []

    for pattern_name, regex, msg_group in PATTERNS:
        for match in re.finditer(regex, content, re.MULTILINE):
            # Find line number
            pos = match.start()
            line_num = content[:pos].count('\n') + 1

            message = ""
            if msg_group > 0 and match.lastindex >= msg_group:
                message = match.group(msg_group)
            elif msg_group == 0:
                # Extract first string literal from the match
                str_match = re.search(r'"([^"]+)"', match.group(0))
                if str_match:
                    message = str_match.group(1)
                else:
                    message = match.group(0)[:120]

            code = ""
            if pattern_name == "DiagnosticCode::":
                code = f"KAIN-{match.group(1).upper()}"
            elif pattern_name == "KainError::":
                code = match.group(1)

            # Get 1 line of context
            context_line = lines[line_num - 1].strip() if line_num <= len(lines) else ""

            results.append({
                'file': str(filepath),
                'line': line_num,
                'pattern': pattern_name,
                'message': message[:200],
                'code': code,
                'context': context_line[:150],
            })

    return results


def is_relevant_file(path: Path) -> bool:
    """Check if a file is a Rust source file (not generated, not in target/)."""
    return path.suffix == '.rs' and 'target/' not in str(path)


def scan_error_corpus() -> list[dict]:
    """Scan error corpus .kn fixtures for KAIN- error code references."""
    results = []
    corpus = Path(ERROR_CORPUS)
    if not corpus.exists():
        return results

    for filepath in corpus.rglob('*.kn'):
        try:
            with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
                content = f.read()
        except:
            continue

        # Look for KAIN-XXXX-XXXX error code patterns in comments or expected error annotations
        for match in re.finditer(r'KAIN-\w+-\d+', content):
            code = match.group(0)
            # Find what line it's on
            pos = match.start()
            line_num = content[:pos].count('\n') + 1
            context_line = content.split('\n')[line_num - 1].strip() if line_num > 0 else ""

            results.append({
                'file': str(filepath),
                'line': line_num,
                'pattern': 'error_corpus_fixture',
                'message': f"Fixture expects error: {code}",
                'code': code,
                'context': context_line[:150],
            })

    return results


def categorize(path: str) -> str:
    """Categorize a file by its crate."""
    if 'core/src' in path:
        if 'parser.rs' in path: return 'core/parser'
        if 'types.rs' in path: return 'core/types'
        if 'ast.rs' in path: return 'core/ast'
        return 'core/other'
    if 'error-semantic' in path: return 'error-semantic'
    if 'error/src' in path: return 'error'
    if 'sys-codegen' in path:
        if 'component.rs' in path: return 'codegen/component'
        if 'codegen_llvm' in path: return 'codegen/llvm'
        return 'codegen/other'
    return 'other'


def main():
    print("=" * 60)
    print("  mine_compiler_errors.py — Mining error surface")
    print("=" * 60)
    print()

    all_results = []
    files_scanned = 0
    files_with_errors = 0

    # Scan error corpus for KAIN- code references in .kn fixtures
    corpus_results = scan_error_corpus()
    if corpus_results:
        print(f"  Error corpus fixtures: {len(corpus_results)} error code references")
        all_results.extend(corpus_results)

    for crate_root in CRATES:
        root = Path(crate_root)
        if not root.exists():
            print(f"  [SKIP] {root} not found")
            continue

        for filepath in root.rglob('*.rs'):
            if not is_relevant_file(filepath):
                continue
            files_scanned += 1
            results = scan_file(filepath)
            if results:
                files_with_errors += 1
                all_results.extend(results)

    # Deduplicate near-identical entries (same file, same pattern, same line)
    seen = set()
    unique = []
    for r in all_results:
        key = (r['file'], r['line'], r['pattern'])
        if key not in seen:
            seen.add(key)
            unique.append(r)

    # Write master TSV
    with open(OUTPUT_TSV, 'w', encoding='utf-8') as f:
        f.write("category\tfile\tline\tpattern\tmessage\tcode\tcontext\n")
        for r in sorted(unique, key=lambda x: (x['file'], x['line'])):
            cat = categorize(r['file'])
            f.write(f"{cat}\t{r['file']}\t{r['line']}\t{r['pattern']}\t{r['message']}\t{r['code']}\t{r['context']}\n")

    # Write per-file breakdown
    by_file = defaultdict(list)
    for r in unique:
        by_file[r['file']].append(r)

    with open(OUTPUT_BY_FILE, 'w', encoding='utf-8') as f:
        f.write("file\ttotal_errors\tparser_error\tDiagnosticReport\tDiagnosticCode\tbail!\tKainError\tother\n")
        for filepath in sorted(by_file.keys()):
            entries = by_file[filepath]
            counts = defaultdict(int)
            for e in entries:
                counts[e['pattern']] += 1
            total = len(entries)
            pe = counts.get('parser_error', 0) + counts.get('parser_error_var', 0)
            dr = counts.get('DiagnosticReport', 0)
            dc = counts.get('DiagnosticCode::', 0)
            bl = counts.get('bail!', 0)
            ke = counts.get('KainError::', 0)
            ot = total - pe - dr - dc - bl - ke
            rel = filepath.replace('X:\\', '').replace('X:/', '')
            f.write(f"{rel}\t{total}\t{pe}\t{dr}\t{dc}\t{bl}\t{ke}\t{ot}\n")

    # Write summary
    with open(OUTPUT_SUMMARY, 'w', encoding='utf-8') as f:
        f.write("category\tfiles\terror_points\tpct_of_total\n")
        by_cat = defaultdict(int)
        for r in unique:
            by_cat[categorize(r['file'])] += 1
        total = len(unique)
        for cat in sorted(by_cat.keys()):
            count = by_cat[cat]
            pct = count / total * 100
            f.write(f"{cat}\t{count}\t{count}\t{pct:.1f}%\n")

    # Stats
    print(f"  Files scanned:    {files_scanned}")
    print(f"  Files with errors: {files_with_errors}")
    print(f"  Raw hits:         {len(all_results)}")
    print(f"  Unique hits:      {len(unique)}")
    print()

    # Distribution by pattern
    print("  By pattern:")
    pattern_counts = defaultdict(int)
    for r in unique:
        pattern_counts[r['pattern']] += 1
    for p, c in sorted(pattern_counts.items(), key=lambda x: -x[1]):
        print(f"    {p:25s} {c:4d}")

    print()
    print(f"  Outputs:")
    print(f"    {OUTPUT_TSV}")
    print(f"    {OUTPUT_BY_FILE}")
    print(f"    {OUTPUT_SUMMARY}")
    print(f"\n  Vs documented: error_codes.tsv ({sum(1 for _ in open('X:/docs-tsv/error_codes.tsv'))-1})")
    print(f"  Vs documented: error_codes_parser.tsv ({sum(1 for _ in open('X:/docs-tsv/error_codes_parser.tsv'))-1})")
    print(f"  Vs documented: error_codes_typechecker.tsv ({sum(1 for _ in open('X:/docs-tsv/error_codes_typechecker.tsv'))-1})")
    print()

    known = sum(1 for _ in open('X:/docs-tsv/error_codes.tsv')) - 1 \
          + sum(1 for _ in open('X:/docs-tsv/error_codes_parser.tsv')) - 1 \
          + sum(1 for _ in open('X:/docs-tsv/error_codes_typechecker.tsv')) - 1
    mined = len(unique)
    print(f"  Total documented: {known}")
    print(f"  Total mined:      {mined}")
    if mined > known:
        print(f"  ⚠️  {mined - known} UNDOCUMENTED error points found in compiler source!")
    print()
    print("=" * 60)


if __name__ == '__main__':
    main()
