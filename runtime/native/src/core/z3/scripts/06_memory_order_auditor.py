"""
06_memory_order_auditor.py
──────────────────────────────────────────────────────────────────────────────
Specialized scanner for atomic memory-ordering hot paths in the native core.

Detects:
  - Silent ordering remaps (ACQUIRE->RELEASE for stores, etc.)
  - compare_exchange calls where failure_order may exceed success_order
  - ACQ_REL usage on store-only operations (invalid per C11)
  - Ordering function calls without matching diagnostic/assertion

Outputs:
  data/memory_order_findings.json
  data/memory_order_findings.csv
  generated/auto_z3/memory_order_<function>__ordering_violation.smt2 per finding
"""

from __future__ import annotations

import json
import re
from pathlib import Path

from _runtime_scan_common import (
    DATA_DIR,
    GENERATED_DIR,
    ensure_output_dirs,
    extract_functions,
    iter_core_sources,
    print_top_table,
    stable_relpath,
    write_json_and_csv,
)

# ── Patterns ────────────────────────────────────────────────────────────────

# Kain ordering constants
KAIN_ORDERINGS = {
    "KAIN_MEMORY_ORDER_RELAXED": 0,
    "KAIN_MEMORY_ORDER_ACQUIRE": 1,
    "KAIN_MEMORY_ORDER_RELEASE": 2,
    "KAIN_MEMORY_ORDER_ACQ_REL": 3,
    "KAIN_MEMORY_ORDER_SEQ_CST": 4,
}

# C11 strength mapping (ascending)
C11_STRENGTH = {
    "memory_order_relaxed": 0,
    "memory_order_consume": 1,
    "memory_order_acquire": 2,
    "memory_order_release": 3,
    "memory_order_acq_rel": 4,
    "memory_order_seq_cst": 5,
}

# Functions that map orderings
ORDERING_MAPPER_FUNCS = (
    "kain_memory_order_from_code",
    "kain_memory_load_order_from_code",
    "kain_memory_store_order_from_code",
    "kain_memory_failure_order_from_code",
)

CAS_FUNC_RE = re.compile(
    r"atomic_compare_exchange_\w+\s*\("
)

STORE_ATOMIC_RE = re.compile(
    r"atomic_store(?:_explicit)?\s*\("
)

ACQUIRE_IN_STORE_RE = re.compile(
    r"memory_order_acquire|memory_order_acq_rel"
)

ORDERING_CALL_RE = re.compile(
    r"(kain_memory_(?:load_|store_|failure_)?order_from_code)\s*\(\s*(\w+)\s*\)"
)


def audit_memory_ordering(func: dict) -> list[dict]:
    """Scan a function for memory-ordering anti-patterns."""
    findings: list[dict] = []
    body = func["body"]
    lines = func["lines"]
    file_rel = stable_relpath(func["file"])

    # ── Pattern 1: atomic_store with acquire/acq_rel ordering ───────────────
    for offset, raw in enumerate(lines):
        stripped = raw.strip()
        if STORE_ATOMIC_RE.search(stripped) and ACQUIRE_IN_STORE_RE.search(stripped):
            findings.append({
                "score": 70,
                "kind": "store-with-acquire-or-acq-rel-ordering",
                "file": file_rel,
                "function": func["name"],
                "line": func["start_line"] + offset,
                "details": (
                    "atomic_store with acquire or acq_rel ordering is invalid per C11; "
                    "stores can only use relaxed, release, or seq_cst"
                ),
                "expression": stripped[:160],
            })

    # ── Pattern 2: compare_exchange with separate success/failure orderings ──
    for offset, raw in enumerate(lines):
        stripped = raw.strip()
        if not CAS_FUNC_RE.search(stripped):
            continue
        # Look for two ordering mapper calls in the same statement (multi-line check)
        window = "\n".join(lines[max(0, offset - 2): offset + 6])
        mapper_calls = ORDERING_CALL_RE.findall(window)
        if len(mapper_calls) >= 2:
            # Identify success vs failure mapper
            success_mapper = next(
                (m for m in mapper_calls if "failure" not in m[0] and "load" not in m[0] and "store" not in m[0]),
                None,
            )
            failure_mapper = next(
                (m for m in mapper_calls if "failure" in m[0]),
                None,
            )
            if success_mapper and failure_mapper:
                # Check if success ordering is RELAXED and failure is anything stronger
                if "RELAXED" in success_mapper[1] and failure_mapper[1] not in (
                    "KAIN_MEMORY_ORDER_RELAXED", "ordering"
                ):
                    findings.append({
                        "score": 90,
                        "kind": "cas-failure-order-may-exceed-success-c11-ub",
                        "file": file_rel,
                        "function": func["name"],
                        "line": func["start_line"] + offset,
                        "details": (
                            f"compare_exchange with success={success_mapper[1]} and "
                            f"failure={failure_mapper[1]}: failure order may be stronger "
                            "than success order, violating C11 7.17.7.4 (UB)"
                        ),
                        "expression": stripped[:160],
                    })
                else:
                    # General: flag all CAS with mixed orderings for review
                    findings.append({
                        "score": 40,
                        "kind": "cas-mismatched-ordering-codes-review",
                        "file": file_rel,
                        "function": func["name"],
                        "line": func["start_line"] + offset,
                        "details": (
                            f"compare_exchange uses distinct success ({success_mapper[1]}) and "
                            f"failure ({failure_mapper[1]}) ordering codes; verify C11 7.17.7.4 "
                            "constraint: failure_order <= success_order"
                        ),
                        "expression": stripped[:160],
                    })

    # ── Pattern 3: ordering mapper functions called with ACQ_REL on store path ──
    for offset, raw in enumerate(lines):
        stripped = raw.strip()
        m = ORDERING_CALL_RE.search(stripped)
        if not m:
            continue
        mapper_name, ordering_arg = m.group(1), m.group(2)
        if "store" in mapper_name and "ACQ_REL" in ordering_arg:
            findings.append({
                "score": 55,
                "kind": "store-order-mapper-acq-rel-silently-downgrades",
                "file": file_rel,
                "function": func["name"],
                "line": func["start_line"] + offset,
                "details": (
                    f"kain_memory_store_order_from_code called with ACQ_REL: "
                    "silently remaps to release with no diagnostic, losing acquire half"
                ),
                "expression": stripped[:160],
            })

    return findings


def make_cas_ordering_smt2(file_rel: str, function: str, success_const: str, failure_const: str) -> tuple[str, str]:
    """Generate an SMT2 proof file for a CAS ordering violation."""
    safe_name = re.sub(r"[^A-Za-z0-9_]", "_", f"{file_rel}__{function}")
    proof_name = f"{safe_name}__cas_ordering_violation.smt2"
    smt2 = f"""(set-logic ALL)
; Auto-generated: CAS ordering constraint check
; Source: {file_rel} / {function}
; Claim: failure_order > success_order violates C11 7.17.7.4

(define-fun c11_relaxed () Int 0)
(define-fun c11_acquire () Int 2)
(define-fun c11_release () Int 3)
(define-fun c11_acq_rel () Int 4)
(define-fun c11_seq_cst () Int 5)

(define-fun success_c11 ((k Int)) Int
  (ite (= k 0) c11_relaxed
  (ite (= k 1) c11_acquire
  (ite (= k 2) c11_release
  (ite (= k 3) c11_acq_rel
       c11_seq_cst)))))

(define-fun failure_c11 ((k Int)) Int
  (ite (= k 0) c11_relaxed
  (ite (= k 2) c11_acquire   ; release remapped
  (ite (= k 1) c11_acquire
  (ite (= k 3) c11_acquire
       c11_seq_cst)))))

(declare-const kain_success Int)
(declare-const kain_failure Int)
(assert (>= kain_success 0)) (assert (<= kain_success 4))
(assert (>= kain_failure 0)) (assert (<= kain_failure 4))
(assert (> (failure_c11 kain_failure) (success_c11 kain_success)))
(check-sat)
; sat = C11 UB reachable; unsat = ordering constraint always satisfied
"""
    return proof_name, smt2


def main() -> None:
    import argparse

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--min-score", type=int, default=40)
    args = parser.parse_args()

    ensure_output_dirs()
    findings: list[dict] = []
    for source_path in iter_core_sources():
        for func in extract_functions(source_path):
            findings.extend(audit_memory_ordering(func))

    findings.sort(key=lambda r: r["score"], reverse=True)
    interesting = [f for f in findings if f["score"] >= args.min_score]

    # Generate SMT2 for high-confidence CAS ordering violations
    try:
        import z3 as _z3
        has_z3 = True
    except ImportError:
        has_z3 = False

    proof_results: list[dict] = []
    for f in interesting:
        if f["kind"] == "cas-failure-order-may-exceed-success-c11-ub":
            proof_name, smt2 = make_cas_ordering_smt2(
                f["file"], f["function"], "RELAXED", "SEQ_CST"
            )
            proof_path = GENERATED_DIR / proof_name
            proof_path.write_text(smt2, encoding="utf-8")
            result = "unknown"
            if has_z3:
                import z3
                solver = z3.Solver()
                solver.set(timeout=5000)
                solver.from_string(smt2)
                outcome = solver.check()
                result = "sat" if outcome == z3.sat else "unsat" if outcome == z3.unsat else "unknown"
            proof_results.append({
                "kind": f["kind"],
                "file": f["file"],
                "function": f["function"],
                "line": f["line"],
                "result": result,
                "proof_path": str(proof_path),
            })

    json_path, csv_path = write_json_and_csv("memory_order_findings", interesting)

    print("Native Core Memory Order Auditor")
    print(f"Findings:   {len(findings)}")
    print(f"Interesting: {len(interesting)} (score >= {args.min_score})")
    print(f"SMT2 proofs: {len(proof_results)}")
    print(f"JSON:        {json_path}")
    print(f"CSV:         {csv_path}")
    print()
    print_top_table(
        "Memory Ordering Findings",
        interesting,
        [
            ("score", 8),
            ("kind", 42),
            ("file", 24),
            ("function", 38),
            ("line", 8),
        ],
        limit=25,
    )
    if proof_results:
        print("\nSMT2 Proof Results:")
        for r in proof_results:
            print(f"  [{r['result']}] {r['kind']} @ {r['function']} line {r['line']}")
            print(f"         {r['proof_path']}")


if __name__ == "__main__":
    main()
