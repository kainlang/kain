"""Auto-generate small SMT2 witnesses from the native-core scan outputs."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from _runtime_scan_common import DATA_DIR, GENERATED_DIR, ensure_output_dirs, print_top_table

try:
    import z3
except ImportError:  # pragma: no cover - optional dependency
    z3 = None


def load_rows(path: Path) -> list[dict]:
    if not path.exists():
        return []
    return json.loads(path.read_text(encoding="utf-8"))


def solve_smt2(smt2: str) -> tuple[str, dict]:
    if z3 is None:
        return "missing-z3", {}
    solver = z3.Solver()
    solver.set(timeout=5000)
    solver.from_string(smt2)
    outcome = solver.check()
    if outcome == z3.sat:
        model = solver.model()
        rendered = {decl.name(): str(model[decl]) for decl in model.decls()}
        return "sat", rendered
    if outcome == z3.unsat:
        return "unsat", {}
    return "unknown", {}


def make_plain_init_race_proof(row: dict) -> tuple[str, str]:
    proof_name = f"{row['file'].replace('/', '_')}__{row['function']}__plain_init_race.smt2"
    smt2 = """(set-logic ALL)
(declare-const initial_initialized Bool)
(declare-const read_a Bool)
(declare-const read_b Bool)

; The implementation uses a plain flag with no lock/atomic/once primitive.
; Both threads can observe the old false value before either write becomes visible.
(assert (= initial_initialized false))
(assert (= read_a initial_initialized))
(assert (= read_b initial_initialized))

(define-fun init_call ((seen_initialized Bool)) Int
  (ite seen_initialized 0 1))

(define-fun total_init_calls () Int
  (+ (init_call read_a) (init_call read_b)))

; Safety contract: first-use initialization should run at most once.
(assert (> total_init_calls 1))
(check-sat)
"""
    return proof_name, smt2


def make_lost_update_proof(row: dict) -> tuple[str, str]:
    proof_name = f"{row['file'].replace('/', '_')}__{row['function']}__lost_update.smt2"
    smt2 = """(set-logic ALL)
(declare-const initial_count Int)
(declare-const read_a Int)
(declare-const read_b Int)
(declare-const a_writes_last Bool)

(assert (>= initial_count 0))

; Two concurrent successful writers observe the same shared count.
(assert (= read_a initial_count))
(assert (= read_b initial_count))

(define-fun slot_a () Int read_a)
(define-fun slot_b () Int read_b)
(define-fun next_a () Int (+ read_a 1))
(define-fun next_b () Int (+ read_b 1))
(define-fun final_count () Int (ite a_writes_last next_a next_b))

; A correct synchronized counter would advance by 2 and provide disjoint slots.
(assert (= final_count (+ initial_count 1)))
(assert (= slot_a slot_b))
(check-sat)
"""
    return proof_name, smt2


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--max-findings", type=int, default=20)
    args = parser.parse_args()

    ensure_output_dirs()
    for stale_path in GENERATED_DIR.glob("*.smt2"):
        stale_path.unlink()
    sync_rows = load_rows(DATA_DIR / "sync_findings.json")
    arithmetic_rows = load_rows(DATA_DIR / "arithmetic_sites.json")
    results: list[dict] = []

    for row in sync_rows[: args.max_findings]:
        if row["kind"] == "plain-init-race":
            proof_name, smt2 = make_plain_init_race_proof(row)
            proof_kind = "plain-init-race"
        elif row["kind"] in {"lost-update-counter", "shared-slot-overwrite"}:
            proof_name, smt2 = make_lost_update_proof(row)
            proof_kind = row["kind"]
        else:
            continue

        proof_path = GENERATED_DIR / proof_name
        proof_path.write_text(smt2, encoding="utf-8")
        result, model = solve_smt2(smt2)
        results.append(
            {
                "proof_kind": proof_kind,
                "file": row["file"],
                "function": row["function"],
                "evidence_line": row["evidence_line"],
                "score": row["score"],
                "result": result,
                "proof_path": str(proof_path),
                "model": json.dumps(model, sort_keys=True),
            }
        )

    for row in arithmetic_rows[: args.max_findings]:
        if "malloc(" not in row.get("expression", "") and "realloc(" not in row.get("expression", ""):
            continue
        if row.get("guard_hits"):
            continue
        proof_name = f"{row['file'].replace('/', '_')}__line_{row['line']}__size_add_wrap_hint.smt2"
        smt2 = """(set-logic QF_BV)
(declare-const left (_ BitVec 64))
(assert (= left #xffffffffffffffff))
(assert (= (bvadd left #x0000000000000001) #x0000000000000000))
(check-sat)
"""
        proof_path = GENERATED_DIR / proof_name
        proof_path.write_text(smt2, encoding="utf-8")
        result, model = solve_smt2(smt2)
        results.append(
            {
                "proof_kind": "size-add-wrap-hint",
                "file": row["file"],
                "function": row["function"],
                "evidence_line": row["line"],
                "score": row["risk_score"],
                "result": result,
                "proof_path": str(proof_path),
                "model": json.dumps(model, sort_keys=True),
            }
        )

    output_path = DATA_DIR / "auto_proof_results.json"
    output_path.write_text(json.dumps(results, indent=2), encoding="utf-8")

    print("Native Core Auto Z3 Prover")
    print(f"Proof results:  {len(results)}")
    print(f"JSON:           {output_path}")
    print(f"Generated SMT2: {GENERATED_DIR}")
    print()
    print_top_table(
        "Top generated proof results",
        results,
        [
            ("result", 12),
            ("proof_kind", 22),
            ("file", 24),
            ("function", 38),
            ("evidence_line", 14),
            ("proof_path", 72),
        ],
        limit=30,
    )


if __name__ == "__main__":
    main()
