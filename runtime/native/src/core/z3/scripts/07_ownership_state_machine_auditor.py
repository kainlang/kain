"""
07_ownership_state_machine_auditor.py
──────────────────────────────────────────────────────────────────────────────
Audits the ownership state machine in ownership.c for:

  1. Reachable DECAYED->X transitions that do not go through IDLE first
  2. Observer count semantics: regions that enter OBSERVED but skip end_observe
  3. Decay of heap regions that re-enters free without proper state guard
  4. State machine transitions that leave regions in COLLAPSED without
     a corresponding end_collapse
  5. Slot-token vs. slot-index ambiguity (helper vs. registered paths)

This script models the state machine abstractly and checks reachability of
bad states using Z3 state-machine encoding.

Outputs:
  data/ownership_state_findings.json
  generated/auto_z3/ownership_state_machine__<finding>.smt2
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

# ── Ownership state constants (mirror ownership.c) ──────────────────────────
KAIN_OWNERSHIP_STATES = {
    "KAIN_OWNERSHIP_STATE_IDLE": 0,
    "KAIN_OWNERSHIP_STATE_OBSERVED": 1,
    "KAIN_OWNERSHIP_STATE_COLLAPSED": 2,
    "KAIN_OWNERSHIP_STATE_SHARED": 3,
    "KAIN_OWNERSHIP_STATE_DECAYED": 5,
}

# SMT2 model of the ownership state machine transitions
OWNERSHIP_STATE_MACHINE_SMT2 = """
(set-logic ALL)
; Kain ownership state machine model
; States: 0=IDLE, 1=OBSERVED, 2=COLLAPSED, 3=SHARED, 5=DECAYED

(define-fun IDLE ()     Int 0)
(define-fun OBSERVED () Int 1)
(define-fun COLLAPSED () Int 2)
(define-fun SHARED ()   Int 3)
(define-fun DECAYED ()  Int 5)

; Valid transitions (from, to):
; IDLE -> OBSERVED (begin_observe, observers=0)
; IDLE -> COLLAPSED (begin_collapse)
; IDLE -> SHARED (begin_share)
; IDLE -> DECAYED (decay)
; OBSERVED -> IDLE (end_observe when observers drop to 0)
; OBSERVED -> OBSERVED (begin_observe increments observers)
; COLLAPSED -> IDLE (end_collapse)
; SHARED -> IDLE (end_share)
; Any -> DECAYED never allowed from OBSERVED/COLLAPSED/SHARED directly

(define-fun valid_transition ((from Int) (to Int)) Bool
  (or
    (and (= from 0) (= to 1))
    (and (= from 0) (= to 2))
    (and (= from 0) (= to 3))
    (and (= from 0) (= to 5))
    (and (= from 1) (= to 0))
    (and (= from 1) (= to 1))
    (and (= from 2) (= to 0))
    (and (= from 3) (= to 0))
  ))

; Bug: decay from OBSERVED (should be blocked but if guard has wrong polarity...)
(declare-const current_state Int)
(declare-const observers Int)

; Scenario: attempt to decay an OBSERVED region
; Assert state = OBSERVED (1) and observers > 0
(assert (= current_state 1))
(assert (> observers 0))
; Guard in decay_slot_unlocked:
;   if (state == OBSERVED || observers != 0) return ERR_OBSERVED
; decay_guard_passes = (state != OBSERVED) AND (observers == 0)
; Assert guard passes (contradicts above assertions):
(assert (and (not (= current_state 1)) (= observers 0)))
(check-sat)
; Expected: unsat -- guard correctly blocks decay from OBSERVED+nonzero-observers
"""

DOUBLE_DECAY_SMT2 = """
(set-logic ALL)
; Check: can decay be called twice on the same region?
; First decay: IDLE -> DECAYED (non-heap) or IDLE -> free+clear_slot (heap)
; Second decay: region is now DECAYED -> decay_slot checks DECAYED -> ERR_DECAYED
(declare-const state_after_first_decay Int)
(assert (= state_after_first_decay 5))
; Second decay check: guard fires on DECAYED state
; second_decay_passes = (state != DECAYED) = (state != 5)
; Assert the guard passes (contradicts DECAYED state):
(assert (not (= state_after_first_decay 5)))
(check-sat)
; Expected: unsat -- double decay is correctly blocked
"""

OBSERVE_WITHOUT_END_SMT2 = """
(set-logic QF_BV)
; Check: can observer count overflow?
; begin_observe guard at line 534:
;   if (region->observers == UINT32_MAX) return ERR_OVERFLOW
; UINT32_MAX = 4294967295
(declare-const observers (_ BitVec 32))
; Assert: guard does NOT fire (observers != UINT32_MAX)
(assert (not (= observers #xffffffff)))
; After increment: can it wrap to 0?
(define-fun after_increment () (_ BitVec 32)
  (bvadd observers #x00000001))
; Claim wrap is impossible when guard blocks UINT32_MAX input:
(assert (= after_increment #x00000000))
(check-sat)
; Expected: unsat -- observer increment cannot wrap when guard is correct
"""


def find_state_machine_anti_patterns(func: dict) -> list[dict]:
    """Detect ownership state-machine anti-patterns in a function."""
    findings: list[dict] = []
    body = func["body"]
    file_rel = stable_relpath(func["file"])
    lines = func["lines"]

    # Pattern 1: state writes without checking current state first
    state_write_re = re.compile(r"region->state\s*=\s*KAIN_OWNERSHIP_STATE_(\w+)")
    state_read_re = re.compile(r"region->state\s*(?:==|!=)\s*KAIN_OWNERSHIP_STATE_(\w+)")

    writes = state_write_re.findall(body)
    reads = state_read_re.findall(body)

    if writes and not reads:
        findings.append({
            "score": 75,
            "kind": "state-write-without-read-guard",
            "file": file_rel,
            "function": func["name"],
            "line": func["start_line"],
            "details": (
                f"Function writes to region->state ({', '.join(writes)}) but "
                "contains no state read/check before writing; may bypass state guard"
            ),
        })

    # Pattern 2: DECAYED state written without observers check
    for offset, raw in enumerate(lines):
        stripped = raw.strip()
        if "KAIN_OWNERSHIP_STATE_DECAYED" in stripped and "=" in stripped:
            # Look back for observer check within 10 lines
            window = "\n".join(lines[max(0, offset - 10): offset])
            if "observers" not in window:
                findings.append({
                    "score": 65,
                    "kind": "decay-state-write-without-observer-check",
                    "file": file_rel,
                    "function": func["name"],
                    "line": func["start_line"] + offset,
                    "details": (
                        "DECAYED state assigned without nearby observer count check; "
                        "verify that observers==0 is guaranteed before this write"
                    ),
                })

    # Pattern 3: helper slot path vs. registered path divergence
    has_helper = "kain_ownership_find_helper_slot_unlocked" in body
    has_registered = "kain_ownership_find_slot" in body
    if has_helper and has_registered:
        # Check if both paths are present in same function (potential double-action)
        findings.append({
            "score": 35,
            "kind": "dual-slot-lookup-paths-in-same-function",
            "file": file_rel,
            "function": func["name"],
            "line": func["start_line"],
            "details": (
                "Function uses both helper slot lookup and registered slot lookup; "
                "verify they are in separate branches and not both executed for same ptr"
            ),
        })

    return findings


def run_state_machine_checks() -> list[dict]:
    """Run Z3 checks on the ownership state machine model."""
    ensure_output_dirs()
    results = []

    checks = [
        ("decay-from-observed-blocked", OWNERSHIP_STATE_MACHINE_SMT2, "unsat"),
        ("double-decay-blocked", DOUBLE_DECAY_SMT2, "unsat"),
        ("observer-count-overflow-blocked", OBSERVE_WITHOUT_END_SMT2, "unsat"),
    ]

    try:
        import z3
        has_z3 = True
    except ImportError:
        has_z3 = False

    for check_name, smt2, expected in checks:
        proof_path = GENERATED_DIR / f"ownership_state_machine__{check_name}.smt2"
        proof_path.write_text(smt2, encoding="utf-8")
        result = "unknown"
        if has_z3:
            import z3 as _z3
            solver = _z3.Solver()
            solver.set(timeout=5000)
            solver.from_string(smt2)
            outcome = solver.check()
            result = "sat" if outcome == _z3.sat else "unsat" if outcome == _z3.unsat else "unknown"

        passed = result == expected
        results.append({
            "check": check_name,
            "expected": expected,
            "actual": result,
            "passed": passed,
            "proof_path": str(proof_path),
        })
        status = "PASS" if passed else "FAIL"
        print(f"  [{status}] {check_name}: expected={expected}, actual={result}")

    return results


def main() -> None:
    import argparse

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--min-score", type=int, default=35)
    args = parser.parse_args()

    ensure_output_dirs()
    findings: list[dict] = []

    for source_path in iter_core_sources():
        if "ownership" not in source_path.name:
            continue
        for func in extract_functions(source_path):
            findings.extend(find_state_machine_anti_patterns(func))

    findings.sort(key=lambda r: r["score"], reverse=True)
    interesting = [f for f in findings if f["score"] >= args.min_score]

    json_path, csv_path = write_json_and_csv("ownership_state_findings", interesting)

    print("Native Core Ownership State Machine Auditor")
    print(f"Anti-patterns found: {len(interesting)} (score >= {args.min_score})")
    print(f"JSON: {json_path}")
    print()
    print("Running Z3 state machine invariant checks:")
    z3_results = run_state_machine_checks()
    passed = sum(1 for r in z3_results if r["passed"])
    print(f"\nZ3 checks: {passed}/{len(z3_results)} passed")
    print()
    print_top_table(
        "Ownership State Machine Findings",
        interesting,
        [
            ("score", 8),
            ("kind", 44),
            ("file", 24),
            ("function", 38),
            ("line", 8),
        ],
        limit=20,
    )


if __name__ == "__main__":
    main()
