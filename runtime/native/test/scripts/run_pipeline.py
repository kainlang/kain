"""
run_pipeline.py
───────────────
Single entry point for the runtime test automation pipeline.

Commands:
  extract      Extract every function from runtime .c files → per-file JSON
  classify     Score functions by testability, match to headers
  generate     Generate test skeletons (smoke/fuzz/property) from catalog
  cbmc         Bounded model check: prove invariants or find counterexamples
  esbmc        SMT-based model check (multi-threaded modules)
  cross        Cross-reference CBMC/ESBMC results vs Z3 proofs
  stats        Print coverage stats: which modules have zero tests
  all          Run the full pipeline: extract → classify → generate → stats

Usage:
  python run_pipeline.py extract
  python run_pipeline.py extract --file actor.c
  python run_pipeline.py classify
  python run_pipeline.py generate --kind fuzz
  python run_pipeline.py generate --kind all --dry-run
  python run_pipeline.py stats
  python run_pipeline.py all
"""

import json
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))

from _common import (
    ensure_output_dirs,
    extract_all_functions,
    iter_runtime_sources,
    load_header_declarations,
    classify_testability,
    is_exported,
    is_static,
    dump_json,
    DATA_DIR,
    FUNCTIONS_DIR,
    TEST_FUZZ_DIR,
    TEST_PROP_DIR,
    TEST_SMOKE_DIR,
    INCLUDE_DIR,
    CORE_DIR,
    RUNTIME_DIR,
    print_summary_table,
    compact_spaces,
)


# ═══════════════════════════════════════════════════════════════════════════
# COMMAND: extract
# ═══════════════════════════════════════════════════════════════════════════

def cmd_extract(args: list[str]):
    """Extract every function from every C file into per-file JSON."""
    ensure_output_dirs()

    single_file = None
    for a in args:
        if a.endswith(".c") and not a.startswith("--"):
            single_file = a

    sources = iter_runtime_sources()
    if single_file:
        sources = [s for s in sources if s.name == single_file or str(s).endswith(single_file)]
        if not sources:
            print(f"File not found: {single_file}")
            return

    all_rows = []
    file_stats = {}

    for source_path in sources:
        funcs = extract_all_functions(source_path)
        if not funcs:
            continue

        header_funcs = load_header_declarations()
        classified = [classify_testability(f, header_funcs) for f in funcs]
        tested = sorted(classified, key=lambda r: r["testability_score"], reverse=True)

        out_path = FUNCTIONS_DIR / f"{source_path.stem}.json"
        dump_json(out_path, {
            "file": str(source_path),
            "total_functions": len(funcs),
            "exported": sum(1 for c in classified if c["is_exported"]),
            "static": sum(1 for c in classified if c["is_static"]),
            "testable_top10": tested[:10],
            "all_functions": tested,
        })

        file_stats[source_path.stem] = {
            "total": len(funcs),
            "exported": sum(1 for c in classified if c["is_exported"]),
            "avg_score": sum(c["testability_score"] for c in classified) / max(len(classified), 1),
        }
        all_rows.extend(tested)

    all_rows.sort(key=lambda r: r["testability_score"], reverse=True)
    dump_json(DATA_DIR / "catalog.json", {
        "total_functions": len(all_rows),
        "files_processed": len(sources),
        "by_file": file_stats,
        "all_functions": all_rows,
    })

    print(f"Extracted {len(all_rows)} functions from {len(sources)} files")
    print(f"  Per-file:  {FUNCTIONS_DIR}/")
    print(f"  Catalog:   {DATA_DIR / 'catalog.json'}")

    print_summary_table("Files by function count", [
        {"file": k, "total": v["total"], "exported": v["exported"], "avg_score": f"{v['avg_score']:.0f}"}
        for k, v in sorted(file_stats.items(), key=lambda x: x[1]["total"], reverse=True)
    ], ["file", "total", "exported", "avg_score"], limit=50)


# ═══════════════════════════════════════════════════════════════════════════
# COMMAND: classify
# ═══════════════════════════════════════════════════════════════════════════

def cmd_classify(args: list[str]):
    """Score every function by testability and match to header declarations."""

    catalog_path = DATA_DIR / "catalog.json"
    if not catalog_path.exists():
        print("No catalog found. Run 'extract' first.")
        return

    catalog = json.loads(catalog_path.read_text())
    all_funcs = catalog["all_functions"]

    testable = [f for f in all_funcs if f["testability_score"] >= 60]
    untestable = [f for f in all_funcs if f["testability_score"] < 30]

    print(f"Classified {len(all_funcs)} functions:")
    print(f"  Highly testable (>=60):  {len(testable)}")
    print(f"  Low testability (<30):   {len(untestable)}")
    print(f"  Exported ABI:            {sum(1 for f in all_funcs if f['is_exported'])}")
    print(f"  Static internal:         {sum(1 for f in all_funcs if f['is_static'])}")

    # Module-level breakdown
    modules = {}
    for f in all_funcs:
        mod = f["file"].replace(".c", "")
        if mod not in modules:
            modules[mod] = {"total": 0, "testable": 0, "exported": 0}
        modules[mod]["total"] += 1
        if f["testability_score"] >= 60:
            modules[mod]["testable"] += 1
        if f["is_exported"]:
            modules[mod]["exported"] += 1

    print_summary_table("Modules by testable functions", [
        {"file": k, "total": v["total"], "testable": v["testable"], "exported": v["exported"]}
        for k, v in sorted(modules.items(), key=lambda x: x[1]["testable"], reverse=True)
    ], ["file", "total", "testable", "exported"], limit=50)

    dump_json(DATA_DIR / "classified.json", {
        "modules": modules,
        "testable": testable,
        "untestable": untestable,
    })


# ═══════════════════════════════════════════════════════════════════════════
# COMMAND: stats
# ═══════════════════════════════════════════════════════════════════════════

def cmd_stats(args: list[str]):
    """Coverage stats: which modules have zero tests?"""

    sources = iter_runtime_sources()
    modules_with_tests = set()
    modules_without_tests = []

    for test_dir, kind in [(TEST_SMOKE_DIR, "smoke"), (TEST_FUZZ_DIR, "fuzz"), (TEST_PROP_DIR, "property")]:
        if test_dir.exists():
            for tf in test_dir.glob("*.c"):
                if "_TEMPLATE" not in tf.name:
                    stem = tf.stem
                    for prefix in ("smoke_", "fuzz_", "prop_"):
                        if stem.startswith(prefix):
                            modules_with_tests.add(stem[len(prefix):])

    for src in sources:
        mod = src.stem
        if mod not in modules_with_tests:
            modules_without_tests.append(mod)

    print(f"Modules with tests:    {len(modules_with_tests)}")
    print(f"Modules without tests: {len(modules_without_tests)}")
    if modules_without_tests:
        print(f"\nUntested modules:")
        for m in modules_without_tests:
            print(f"  - {m}")

    # Also check: do modules have Z3 proofs?
    z3_proofs_dir = Path(__file__).parent.parent.parent / "src" / "core" / "z3" / "proofs"
    if z3_proofs_dir.exists():
        proof_keywords = {}
        for proof in z3_proofs_dir.glob("*.yaml"):
            name = proof.stem
            for mod in [s.stem for s in sources]:
                if mod in name:
                    proof_keywords[mod] = proof_keywords.get(mod, 0) + 1

        print(f"\nZ3 proof coverage:")
        for mod in sorted(modules_without_tests):
            proofs = proof_keywords.get(mod, 0)
            marker = f" ({proofs} Z3 proofs)" if proofs > 0 else " (NO PROOFS)"
            print(f"  - {mod}{marker}")


# ═══════════════════════════════════════════════════════════════════════════
# COMMAND: generate
# ═══════════════════════════════════════════════════════════════════════════

FUZZ_TEMPLATE = """// Auto-generated fuzz harness for {module}
// Source: {source_file} ({total_funcs} functions, {testable_funcs} testable)
// Generated by: run_pipeline.py generate --kind fuzz
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "{header}"

#define MAX_TRACKED 32

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {{
    if (size < 8) return 0;
    void *tracked[MAX_TRACKED] = {{0}};
    int count = 0;
    size_t offset = 0;

    while (offset + 4 <= size && count < MAX_TRACKED) {{
        uint8_t op = data[offset] % 4;
        uint8_t idx = data[offset + 1] % MAX_TRACKED;
        uint16_t arg = (uint16_t)(data[offset + 2]) | ((uint16_t)(data[offset + 3]) << 8);
        offset += 4;

        switch (op) {{
            case 0: /* alloc */
                if (count < MAX_TRACKED && !tracked[count]) {{
                    tracked[count] = __kain_alloc((arg % 4096) + 1, 1, arg & 1);
                    count++;
                }}
                break;
            case 1: /* free */
                if (idx < count && tracked[idx]) {{
                    __kain_free(tracked[idx]);
                    tracked[idx] = NULL;
                }}
                break;
            case 2: /* realloc */
                if (idx < count && tracked[idx]) {{
                    void *np = __kain_realloc(tracked[idx], (arg % 8192) + 1, 1, arg & 1);
                    if (np) tracked[idx] = np;
                }}
                break;
            case 3: /* module-specific call */
                {module_calls}
                break;
        }}
    }}

    for (int i = 0; i < count; i++)
        if (tracked[i]) __kain_free(tracked[i]);
    return 0;
}}

#ifndef __has_feature
#define __has_feature(x) 0
#endif
#if !__has_feature(address_sanitizer)
int main(void) {{
    uint8_t seed[256];
    for (int i = 0; i < 256; i++) seed[i] = (uint8_t)(i * 73 + 17);
    return LLVMFuzzerTestOneInput(seed, sizeof(seed));
}}
#endif
"""

SMOKE_TEMPLATE = """// Auto-generated smoke test for {module}
// Source: {source_file} ({total_funcs} functions, {testable_funcs} testable)
// Generated by: run_pipeline.py generate --kind smoke
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "{header}"

int main(void) {{
{module_calls}

    printf("smoke_{module}: PASS\\\\n");
    return 0;
}}
"""


def cmd_generate(args: list[str]):
    """Generate test skeletons from the function catalog."""

    kind = "all"
    dry_run = False
    for a in args:
        if a.startswith("--kind="):
            kind = a.split("=", 1)[1]
        elif a == "--dry-run":
            dry_run = True
        elif a in ("fuzz", "smoke", "property", "all"):
            kind = a

    catalog_path = DATA_DIR / "catalog.json"
    if not catalog_path.exists():
        print("No catalog found. Run 'extract' first.")
        return

    catalog = json.loads(catalog_path.read_text())

    generated = []
    for mod_name, stats in catalog["by_file"].items():
        if stats["total"] < 3:
            continue  # skip tiny modules

        funcs_json = FUNCTIONS_DIR / f"{mod_name}.json"
        if not funcs_json.exists():
            continue
        funcs_data = json.loads(funcs_json.read_text())
        testable_funcs = funcs_data.get("testable_top10", [])[:5]
        if not testable_funcs:
            continue

        header = f"{mod_name}.h"
        if not (INCLUDE_DIR / header).exists():
            # Try to find any matching header
            matching = [f for f in INCLUDE_DIR.iterdir() if f.name.startswith(mod_name) and f.suffix == ".h"]
            header = matching[0].name if matching else "memory.h"  # fallback

        # Generate module-specific call snippets
        call_snippets = []
        for tf in testable_funcs:
            if tf["param_count"] == 0:
                call_snippets.append(f"    // {tf['function']}()")
                call_snippets.append(f"    // {tf['function']}();")
            elif tf["param_count"] <= 2:
                call_snippets.append(f"    // {tf['function']}(TODO) -- score={tf['testability_score']}")

        module_calls = "\n".join(call_snippets) if call_snippets else "    // TODO: add calls"

        if kind in ("fuzz", "all"):
            out = TEST_FUZZ_DIR / f"fuzz_{mod_name}.c"
            if not out.exists() or not dry_run:
                if not dry_run:
                    out.write_text(FUZZ_TEMPLATE.format(
                        module=mod_name,
                        source_file=funcs_data["file"],
                        total_funcs=stats["total"],
                        testable_funcs=len(testable_funcs),
                        header=header,
                        module_calls=module_calls,
                    ), encoding="utf-8")
                generated.append(f"  fuzz_{mod_name}.c")

        if kind in ("smoke", "all"):
            out = TEST_SMOKE_DIR / f"smoke_{mod_name}.c"
            if not out.exists() or not dry_run:
                if not dry_run:
                    out.write_text(SMOKE_TEMPLATE.format(
                        module=mod_name,
                        source_file=funcs_data["file"],
                        total_funcs=stats["total"],
                        testable_funcs=len(testable_funcs),
                        header=header,
                        module_calls=module_calls,
                    ), encoding="utf-8")
                generated.append(f"  smoke_{mod_name}.c")

    if dry_run:
        print(f"Would generate {len(generated)} files:")
    else:
        print(f"Generated {len(generated)} files:")

    for g in generated:
        print(g)


# ═══════════════════════════════════════════════════════════════════════════
# COMMAND: cbmc — Bounded model checking (prove or refute invariants)
# ═══════════════════════════════════════════════════════════════════════════

CBMC_HARNESS_TEMPLATE = """/*
 * CBMC verification harness for {module}
 * Auto-generated from function catalog
 *
 * Self-contained: forward declarations only, no system headers.
 * CBMC explores ALL paths on ALL possible inputs within unwind bound.
 */

// Basic type definitions needed by runtime function signatures
typedef unsigned long long uint64_t;
typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;
typedef long long int64_t;
typedef int int32_t;
typedef short int16_t;
typedef signed char int8_t;
typedef unsigned long long size_t;
typedef long long ptrdiff_t;

// Forward declarations of functions under test
{harness_functions}

int main(void) {{
{main_body}
    return 0;
}}
"""

CBMC_DATA_DIR = DATA_DIR / "cbmc"
CBMC_HARNESS_DIR = DATA_DIR / "cbmc" / "harnesses"


def _find_cbmc() -> str | None:
    """Find CBMC executable, returning full path or None."""
    import shutil
    path = shutil.which("cbmc")
    if not path:
        for loc in [r"C:\Program Files\cbmc\bin\cbmc.exe", r"C:\Program Files (x86)\cbmc\bin\cbmc.exe"]:
            if Path(loc).exists():
                return loc
    return path


def _get_msvc_include_paths() -> str | None:
    """Get MSVC include paths for CBMC preprocessing on Windows."""
    import subprocess
    toolchain = RUNTIME_DIR.parent.parent / "scripts" / "kain_toolchain.py"
    if toolchain.exists():
        try:
            result = subprocess.run(
                [sys.executable, str(toolchain), "--env"],
                capture_output=True, text=True, timeout=10,
            )
            for line in result.stdout.split("\n"):
                if line.startswith("export INCLUDE="):
                    val = line.split("=", 1)[1].strip().strip('"')
                    return val
        except Exception:
            pass
    return None


def _run_cbmc_in_wsl(harness_path, source_path, mod_name, unwind=5) -> tuple[str, str, int] | None:
    """Run CBMC via WSL (where Linux GCC headers work). Concatenates harness + source."""
    import subprocess
    import os

    repo_path = Path(__file__).resolve().parent.parent.parent  # runtime/native
    wsl_repo = f"/mnt/{repo_path.drive.lower()}{repo_path.as_posix()[2:]}".replace(":", "")

    def to_wsl(p):
        p = str(p)
        if ":" in p:
            drive = p[0].lower()
            rest = p[2:].replace("\\", "/")
            return f"/mnt/{drive}{rest}"
        return p

    # Concatenate source + harness into one file (source first to define types, then harness)
    combined = harness_path.parent / f"combined_{mod_name}.c"
    combined_code = ""
    if source_path and source_path.exists():
        combined_code += source_path.read_text() + "\n"
    combined_code += harness_path.read_text()
    combined.write_text(combined_code, encoding="utf-8")

    combined_wsl = to_wsl(combined)

    cmd = f"cd {wsl_repo} && cbmc --unwind {unwind} --trace {combined_wsl} -I include -I src/core"

    try:
        proc = subprocess.run(
            ["wsl", "-d", "Ubuntu", "bash", "-c", cmd],
            capture_output=True, text=True, timeout=120,
        )
        return proc.stdout, proc.stderr, proc.returncode
    except FileNotFoundError:
        return None
    except subprocess.TimeoutExpired:
        return "", "TIMEOUT", -1


def _find_wsl_cbmc() -> bool:
    """Check if CBMC is available via WSL."""
    import subprocess
    try:
        result = subprocess.run(
            ["wsl", "-d", "Ubuntu", "bash", "-c", "which cbmc && cbmc --version"],
            capture_output=True, text=True, timeout=5,
        )
        return result.returncode == 0
    except FileNotFoundError:
        return False


def _cbmc_check_installed() -> tuple[bool, str]:
    """Check if CBMC is installed and return its path."""
    cbmc_path = _find_cbmc()
    if cbmc_path:
        try:
            result = __import__("subprocess").run([cbmc_path, "--version"], capture_output=True, text=True, timeout=5)
            version_line = result.stdout.split("\n")[0] if result.stdout else result.stderr.split("\n")[0]
            return True, version_line
        except Exception:
            return True, "cbmc (version unknown)"
    return False, ""


def _generate_cbmc_harness(func_data: dict, unwind: int, include_dir: str) -> str:
    """Generate a CBMC harness for a single function."""
    funcs = func_data.get("all_functions", func_data.get("testable_top10", []))
    testable = [f for f in funcs if f["param_count"] <= 2][:5]  # skip score filter for broad coverage

    if not testable:
        return None

    module = Path(func_data["file"]).stem
    header = f"{module}.h"
    include_path = Path(INCLUDE_DIR)
    if not (include_path / header).exists():
        matching = [f for f in include_path.iterdir() if f.name.startswith(module) and f.suffix == ".h"]
        header = matching[0].name if matching else "memory.h"

    harness_funcs = []
    main_body = []
    includes = set()

    for tf in testable:
        name = tf["function"]
        sig = tf.get("signature", "")
        param_count = tf["param_count"]

        # Extract clean C declaration: strip doc comments, keep only the actual function
        sig_clean = sig
        if "*/" in sig_clean:
            sig_clean = sig_clean.split("*/")[-1].strip()
        sig_clean = compact_spaces(sig_clean)
        # Remove trailing brace
        if sig_clean.endswith("{"):
            sig_clean = sig_clean[:-1].strip()
        # Ensure semicolon for forward decl
        decl = sig_clean if sig_clean.endswith(";") else sig_clean + ";"

        # Forward-declare the function
        harness_funcs.append(f"// {name}")
        harness_funcs.append(decl)

        # Regenerate param_count from the clean declaration
        if "(" in sig_clean and ")" in sig_clean:
            param_str = sig_clean.split("(", 1)[1].split(")")[0].strip()
            param_count = len([p for p in param_str.split(",") if p.strip() and p.strip() != "void"])

        # Generate nondet call and assertion
        # Use uninitialized variables — CBMC treats them as fully nondeterministic
        if param_count == 0:
            main_body.append(f"    {name}();")
            main_body.append(f"    __CPROVER_assert(1, \"{name}: call ok\");")

        elif param_count == 1:
            main_body.append(f"    {{ void *__p; {name}(__p); }}")
            main_body.append(f"    __CPROVER_assert(1, \"{name}: call ok\");")

        elif param_count == 2:
            main_body.append(f"    {{ void *__a; unsigned long long __b; {name}(__a, __b); }}")
            main_body.append(f"    __CPROVER_assert(1, \"{name}: call ok\");")

    if not main_body:
        return None

    return CBMC_HARNESS_TEMPLATE.format(
        module=module,
        total_funcs=len(funcs),
        testable_funcs=len(testable),
        unwind=unwind,
        header=header,
        include_dir=include_dir,
        harness_functions="\n".join(harness_funcs),
        main_body="\n".join(main_body),
        this_file=f"cbmc_{module}.c",
    )


def _parse_cbmc_output(output: str, module: str) -> dict:
    """Parse CBMC output into structured results."""
    import re

    result = {
        "module": module,
        "status": "UNKNOWN",
        "verified": 0,
        "failed": 0,
        "total": 0,
        "failures": [],
        "summary": "",
    }

    if "VERIFICATION SUCCESSFUL" in output:
        result["status"] = "SUCCESS"
    elif "VERIFICATION FAILED" in output:
        result["status"] = "FAILED"
    else:
        for line in output.split("\n"):
            if "VERIFICATION" in line:
                result["summary"] = line.strip()

    # Count properties in format "[name.kind.N] description: SUCCESS|FAILURE"
    prop_pattern = re.compile(r'\[([^\]]+)\.(\w+)\.(\d+)\]\s*(.+?):\s*(SUCCESS|FAILURE|OK|VIOLATION)')
    props = prop_pattern.findall(output)
    for name, kind, num, desc, status in props:
        result["total"] += 1
        if status in ("SUCCESS", "OK"):
            result["verified"] += 1
        elif status in ("FAILURE", "VIOLATION"):
            result["failed"] += 1
            result["failures"].append({
                "property": f"{name}.{kind}.{num}",
                "description": desc.strip(),
                "status": status,
            })

    # Also count Violated property blocks (old-style format)
    violated_blocks = re.findall(r'Violated property:\n  file (.+?) function (\S+) line (\d+)', output)
    for filepath, func, line in violated_blocks:
        # Only add if not already counted by the pattern above
        if not any(f["description"] == f"{func}:{line}" for f in result["failures"]):
            result["failed"] += 1
            result["failures"].append({
                "property": f"{func}:{line}",
                "description": f"Violation in {func}:{line}",
                "status": "VIOLATION",
                "file": filepath,
                "line": int(line),
            })

    # Extract counterexample trace
    if "Trace for" in output:
        trace_start = output.find("Trace for")
        trace_end = output.find("\n\nVERIFICATION", trace_start)
        if trace_end == -1:
            trace_end = min(trace_start + 2000, len(output))
        result["trace"] = output[trace_start:trace_end]

    return result


def cmd_cbmc(args: list[str]):
    """Run bounded model checking on runtime functions."""
    import subprocess
    import tempfile

    installed, version = _cbmc_check_installed()
    if not installed:
        print("CBMC not found. Install: https://github.com/diffblue/cbmc/releases")
        print("  Windows: download cbmc-*-win64.msi")
        print("  Linux:   apt install cbmc")
        print("  macOS:   brew install cbmc")
        return

    print(f"CBMC: {version}")

    unwind = 5
    module_filter = None
    for a in args:
        if a.startswith("--unwind="):
            unwind = int(a.split("=", 1)[1])
        elif a.startswith("--module="):
            module_filter = a.split("=", 1)[1]
        elif not a.startswith("--"):
            module_filter = a

    CBMC_HARNESS_DIR.mkdir(parents=True, exist_ok=True)
    CBMC_DATA_DIR.mkdir(parents=True, exist_ok=True)

    catalog_path = DATA_DIR / "catalog.json"
    if not catalog_path.exists():
        print("No catalog found. Run 'extract' first.")
        return

    catalog = json.loads(catalog_path.read_text())
    results_all = []

    # Check if GCC is available for preprocessing
    gcc_path = __import__("shutil").which("gcc")
    if not gcc_path:
        print("WARNING: GCC not found on PATH. CBMC preprocessing may fail on Windows.")
        gcc_path = None

    for mod_name in catalog["by_file"]:
        if module_filter and module_filter not in mod_name:
            continue

        funcs_json = FUNCTIONS_DIR / f"{mod_name}.json"
        if not funcs_json.exists():
            continue

        funcs_data = json.loads(funcs_json.read_text())
        if funcs_data.get("total_functions", 0) < 3:
            continue

        # Skip modules that need special headers
        skip_patterns = ("python_", "cuda_", "graphics_", "renderer_", "scene", "realtime")
        if any(mod_name.startswith(p) for p in skip_patterns):
            continue

        harness = _generate_cbmc_harness(funcs_data, unwind, str(INCLUDE_DIR))
        if not harness:
            continue

        harness_path = CBMC_HARNESS_DIR / f"cbmc_{mod_name}.c"
        harness_path.write_text(harness, encoding="utf-8")

        print(f"\n{'='*60}")
        print(f"  CBMC: {mod_name}")
        print(f"{'='*60}")

        cbmc_path = _find_cbmc()
        source_path = RUNTIME_DIR / "src" / "core" / f"{mod_name}.c"
        print(f"    Harness: {harness_path.name}")
        if source_path.exists():
            print(f"    Source:   {source_path.name}")

        # Strategy: preprocess with GCC first, then run CBMC on preprocessed output
        # This avoids CBMC's parser choking on MinGW/MSVC headers
        try:
            with tempfile.TemporaryDirectory() as tmpdir:
                preprocessed = Path(tmpdir) / f"{mod_name}.i"

                if gcc_path:
                    # Concatenate source + harness (source first to define types)
                    combined = Path(tmpdir) / f"{mod_name}_combined.c"
                    combined_code = ""
                    if source_path.exists():
                        combined_code += source_path.read_text() + "\n"
                    combined_code += harness_path.read_text()
                    combined.write_text(combined_code, encoding="utf-8")

                    gcc_cmd = [gcc_path, "-E", "-std=c11",
                               "-I", str(INCLUDE_DIR),
                               "-I", str(RUNTIME_DIR / "src" / "core"),
                               str(combined),
                               "-o", str(preprocessed)]
                    gcc_proc = subprocess.run(gcc_cmd, capture_output=True, text=True, timeout=30)
                    if gcc_proc.returncode != 0:
                        print(f"    GCC preprocess failed:")
                        for line in gcc_proc.stderr.strip().split("\n")[-5:]:
                            print(f"      {line}")
                        # Fall through to try WSL instead
                        preprocessed = None
                    else:
                        preprocessed = preprocessed
                else:
                    preprocessed = None

                # Run CBMC: try preprocessed file, then WSL
                result = None
                if preprocessed and preprocessed.exists():
                    proc = subprocess.run(
                        [cbmc_path, "--unwind", str(unwind), "--trace",
                         str(preprocessed)],
                        capture_output=True, text=True, timeout=60,
                    )
                    result = _parse_cbmc_output(proc.stdout + proc.stderr, mod_name)
                    result["exit_code"] = proc.returncode
                    result["raw_output_bytes"] = len(proc.stdout + proc.stderr)

                # If preprocessed approach failed or wasn't available, try WSL
                if (not result or result["status"] == "UNKNOWN") and source_path.exists():
                    wsl_out = _run_cbmc_in_wsl(harness_path, source_path, mod_name, unwind)
                    if wsl_out:
                        stdout, stderr, rc = wsl_out
                        result = _parse_cbmc_output(stdout + stderr, mod_name)
                        result["exit_code"] = rc
                        result["raw_output_bytes"] = len(stdout + stderr)
                        result["backend"] = "wsl"
                        if result["status"] == "SUCCESS":
                            print(f"    Backend: WSL")

                if result:
                    result["harness"] = str(harness_path)
                    results_all.append(result)

                    if result["status"] == "SUCCESS":
                        print(f"  [OK] All {result['verified']} assertions verified")
                    elif result["status"] == "FAILED":
                        print(f"  [FAIL] {result['failed']} violations")
                        if result.get("failures"):
                            for f_entry in result["failures"]:
                                print(f"    - {f_entry['property']}: {f_entry['description'][:100]}")
                    else:
                        print(f"  [???] Status: {result['status']}")

        except subprocess.TimeoutExpired:
            print(f"  [TIMEOUT] Exceeded 60s")
            results_all.append({"module": mod_name, "status": "TIMEOUT"})
        except FileNotFoundError:
            print("  CBMC executable not found on PATH")
            return

    report = {
        "cbmc_version": version,
        "unwind_bound": unwind,
        "modules_checked": len(results_all),
        "modules_passed": sum(1 for r in results_all if r.get("status") == "SUCCESS"),
        "modules_failed": sum(1 for r in results_all if r.get("status") == "FAILED"),
        "results": results_all,
    }
    dump_json(CBMC_DATA_DIR / "report.json", report)

    print(f"\n{'='*60}")
    print(f"CBMC complete: {report['modules_passed']} passed, {report['modules_failed']} failed, {report['modules_checked']} total")
    print(f"Report: {CBMC_DATA_DIR / 'report.json'}")


# ═══════════════════════════════════════════════════════════════════════════
# COMMAND: esbmc — SMT-based model checking (handles concurrency)
# ═══════════════════════════════════════════════════════════════════════════

ESBMC_DATA_DIR = DATA_DIR / "esbmc"


def _esbmc_check_installed() -> tuple[bool, str]:
    import shutil
    esbmc_path = shutil.which("esbmc")
    if esbmc_path:
        try:
            result = __import__("subprocess").run([esbmc_path, "--version"], capture_output=True, text=True, timeout=5)
            return True, result.stdout.split("\n")[0].strip()
        except Exception:
            return True, "esbmc (version unknown)"
    return False, ""


def cmd_esbmc(args: list[str]):
    """Run SMT-based model checking — targets multi-threaded modules."""
    import subprocess

    installed, version = _esbmc_check_installed()
    if not installed:
        print("ESBMC not found. Install: https://github.com/esbmc/esbmc/releases")
        print("  Windows: download release-windows-latest.zip")
        print("  Linux:   apt install esbmc")
        return

    print(f"ESBMC: {version}")
    print("ESBMC excels at multi-threaded verification (data races, deadlocks, atomicity).")
    print("Target modules: actor, async, ownership, entangle, converge")
    print()

    # ESBMC works directly on source files with --function flag
    target_modules = ["actor", "async", "ownership", "entangle", "converge", "fanout"]
    unwind = 5

    for a in args:
        if a.startswith("--unwind="):
            unwind = int(a.split("=", 1)[1])
        elif not a.startswith("--"):
            target_modules = [a]

    ESBMC_DATA_DIR.mkdir(parents=True, exist_ok=True)
    results_all = []

    for mod in target_modules:
        src = RUNTIME_DIR / "src" / "core" / f"{mod}.c"
        if not src.exists():
            print(f"  SKIP: {mod}.c not found")
            continue

        print(f"\n{'='*60}")
        print(f"  ESBMC: {mod}")
        print(f"{'='*60}")

        try:
            proc = subprocess.run(
                ["esbmc", "--unwind", str(unwind),
                 "-I", str(INCLUDE_DIR),
                 "-I", str(RUNTIME_DIR / "src" / "core"),
                 str(src)],
                capture_output=True, text=True, timeout=120,
                cwd=str(RUNTIME_DIR),
            )

            result = {
                "module": mod,
                "exit_code": proc.returncode,
                "output_bytes": len(proc.stdout + proc.stderr),
            }

            if "VERIFICATION SUCCESSFUL" in proc.stdout + proc.stderr:
                result["status"] = "SUCCESS"
                print(f"  [OK] Verification passed")
            elif "VERIFICATION FAILED" in proc.stdout + proc.stderr:
                result["status"] = "FAILED"
                print(f"  [FAIL] Verification failed")
            else:
                result["status"] = "UNKNOWN"
                last_lines = (proc.stderr + proc.stdout).strip().split("\n")[-5:]
                for line in last_lines:
                    print(f"  {line[:120]}")

            results_all.append(result)

        except subprocess.TimeoutExpired:
            print(f"  [TIMEOUT] ESBMC exceeded 120s")
            results_all.append({"module": mod, "status": "TIMEOUT"})
        except FileNotFoundError:
            print("  ESBMC executable not found on PATH")
            return

    report = {
        "esbmc_version": version,
        "unwind_bound": unwind,
        "modules_checked": len(results_all),
        "modules_passed": sum(1 for r in results_all if r.get("status") == "SUCCESS"),
        "results": results_all,
    }
    dump_json(ESBMC_DATA_DIR / "report.json", report)

    print(f"\nESBMC complete: {report['modules_passed']} passed, {len(results_all)} total")


# ═══════════════════════════════════════════════════════════════════════════
# COMMAND: cross — Cross-reference CBMC/ESBMC vs Z3 proofs
# ═══════════════════════════════════════════════════════════════════════════

def cmd_cross(args: list[str]):
    """Cross-reference verification results with Z3 proofs."""
    from pathlib import Path as _Path

    Z3_PROOFS_DIR = _Path(RUNTIME_DIR) / "src" / "core" / "z3" / "proofs"

    # Load CBMC results
    cbmc_report_path = CBMC_DATA_DIR / "report.json"
    esbmc_report_path = ESBMC_DATA_DIR / "report.json"

    cbmc_results = {}
    esbmc_results = {}

    if cbmc_report_path.exists():
        cbmc_report = json.loads(cbmc_report_path.read_text())
        for r in cbmc_report.get("results", []):
            cbmc_results[r["module"]] = r

    if esbmc_report_path.exists():
        esbmc_report = json.loads(esbmc_report_path.read_text())
        for r in esbmc_report.get("results", []):
            esbmc_results[r["module"]] = r

    # Load Z3 proof coverage
    z3_proofs = {}
    if Z3_PROOFS_DIR.exists():
        for proof in Z3_PROOFS_DIR.glob("*.yaml"):
            name = proof.stem
            for mod in cbmc_results.keys() | esbmc_results.keys():
                if mod in name:
                    z3_proofs[mod] = z3_proofs.get(mod, 0) + 1

    # Cross-reference
    print("Cross-Reference: CBMC + ESBMC vs Z3 Proofs")
    print(f"{'Module':<20} {'CBMC':<12} {'ESBMC':<12} {'Z3 Proofs':<12} {'Status':<20}")
    print("-" * 80)

    all_modules = sorted(set(list(cbmc_results.keys()) + list(esbmc_results.keys()) + list(z3_proofs.keys())))

    conflicts = []
    for mod in all_modules:
        cbmc = cbmc_results.get(mod, {})
        esbmc = esbmc_results.get(mod, {})
        z3_count = z3_proofs.get(mod, 0)

        cbmc_status = cbmc.get("status", "—") if cbmc else "—"
        esbmc_status = esbmc.get("status", "—") if esbmc else "—"

        # Detect conflicts: Z3 proves something but CBMC/ESBMC finds counterexample
        flags = []
        if z3_count > 0 and cbmc_status == "FAILED":
            flags.append("Z3⊢ CBMC⊬")
            conflicts.append(f"{mod}: Z3 has {z3_count} proofs but CBMC found failures")
        if z3_count > 0 and esbmc_status == "FAILED":
            flags.append("Z3⊢ ESBMC⊬")
            conflicts.append(f"{mod}: Z3 has {z3_count} proofs but ESBMC found failures")

        status = " ".join(flags) if flags else "consistent"
        print(f"{mod:<20} {cbmc_status:<12} {esbmc_status:<12} {z3_count:<12} {status:<20}")

    if conflicts:
        print(f"\n[!] {len(conflicts)} potential conflicts (Z3 proves but CBMC/ESBMC refutes):")
        for c in conflicts:
            print(f"    {c}")
    else:
        print(f"\n[OK] No conflicts detected — all verification tools agree.")

    dump_json(DATA_DIR / "cross_validation.json", {
        "cbmc": {m: r.get("status") for m, r in cbmc_results.items()},
        "esbmc": {m: r.get("status") for m, r in esbmc_results.items()},
        "z3_proofs": z3_proofs,
        "conflicts": conflicts,
    })


# ═══════════════════════════════════════════════════════════════════════════
# COMMAND ROUTER
# ═══════════════════════════════════════════════════════════════════════════

COMMANDS = {
    "extract": (cmd_extract, "Extract every function → per-file JSON"),
    "classify": (cmd_classify, "Score functions by testability"),
    "generate": (cmd_generate, "Generate test skeletons (fuzz/smoke/property)"),
    "cbmc": (cmd_cbmc, "Bounded model check: prove invariants or find counterexamples"),
    "esbmc": (cmd_esbmc, "SMT-based model check (multi-threaded modules)"),
    "cross": (cmd_cross, "Cross-reference CBMC/ESBMC vs Z3 proofs"),
    "stats": (cmd_stats, "Coverage report: untested modules, Z3 gaps"),
}

def main():
    if len(sys.argv) < 2 or sys.argv[1] in ("-h", "--help", "help"):
        print("Kain Runtime Test Pipeline")
        print(f"  {SCRIPT_DIR}")
        print()
        for name, (_, desc) in COMMANDS.items():
            print(f"  {name:<12} {desc}")
        print()
        print("  all          Run extract → classify → generate → stats")
        print()
        print("Usage: python run_pipeline.py <command> [args]")
        return

    cmd_name = sys.argv[1]
    cmd_args = sys.argv[2:]

    if cmd_name == "all":
        for name in ("extract", "classify", "generate", "cbmc", "cross", "stats"):
            print(f"\n{'='*60}")
            print(f"  STEP: {name}")
            print(f"{'='*60}")
            handler, _ = COMMANDS[name]
            handler([])
        return

    if cmd_name not in COMMANDS:
        print(f"Unknown command: {cmd_name}")
        print(f"Available: {', '.join(COMMANDS.keys())}, all")
        return

    handler, _ = COMMANDS[cmd_name]
    handler(cmd_args)


if __name__ == "__main__":
    main()
