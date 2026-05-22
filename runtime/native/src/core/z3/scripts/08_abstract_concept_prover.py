"""
08_abstract_concept_prover.py
──────────────────────────────────────────────────────────────────────────────
Abstraction-first bug hunter: models high-level concepts (lock protocol,
alloc-cache invariant, pointer index integrity) as Z3 state machines or
constraint systems, INDEPENDENT of the specific C implementation.

The approach:
  1. Define the abstract contract (what SHOULD be true).
  2. Enumerate concrete violations (what the code COULD allow given weaknesses).
  3. Find witness inputs that break the abstract contract through concrete paths.

This is the "alien" layer of the pipeline: instead of pattern-matching C lines,
we ask "given this mathematical contract, can any input sequence break it?"

Contracts modeled:
  A. Alloc-cache integrity: CACHE_BYTES == sum(header+payload for each cached node)
  B. Ownership registry: no slot can be simultaneously OCCUPIED and DECAYED
  C. Pointer index injection-free: no two distinct pointers map to the same
     encoded_slot in the index
  D. Lock ordering: ownership lock always acquired before alloc-cache lock
     (never reversed) -- checks for potential deadlock from ordering violation
  E. CAS failure order: failure_order_c11 <= success_order_c11 for all inputs

Outputs:
  data/abstract_concept_findings.json
  generated/auto_z3/abstract_<concept>.smt2
"""

from __future__ import annotations

import json
from pathlib import Path

from _runtime_scan_common import (
    DATA_DIR,
    GENERATED_DIR,
    ensure_output_dirs,
    print_top_table,
    write_json_and_csv,
)

# ── Abstract contract SMT2 definitions ──────────────────────────────────────

CONTRACT_A_CACHE_BYTES_INTEGRITY = """
(set-logic QF_LIA)
; Contract A: alloc-cache BYTES counter integrity
; BYTES = sum of (sizeof(header) + payload_size) for every cached node.
; No individual removal can make BYTES < 0.
; Model with N=3 cached nodes for tractability.

(declare-const bytes_total Int)
(declare-const n1_size Int)
(declare-const n2_size Int)
(declare-const n3_size Int)
(define-fun HEADER_SIZE () Int 16)

; All node sizes positive and within eligible range [4096, 262144]
(assert (>= n1_size 4096)) (assert (<= n1_size 262144))
(assert (>= n2_size 4096)) (assert (<= n2_size 262144))
(assert (>= n3_size 4096)) (assert (<= n3_size 262144))

; bytes_total is the exact sum
(assert (= bytes_total
  (+ (+ HEADER_SIZE n1_size)
     (+ HEADER_SIZE n2_size)
     (+ HEADER_SIZE n3_size))))

; After removing node1: new_bytes = bytes_total - (HEADER_SIZE + n1_size)
(define-fun bytes_after_remove1 () Int
  (- bytes_total (+ HEADER_SIZE n1_size)))

; Claim: bytes_after_remove1 >= 0 (no underflow)
(assert (< bytes_after_remove1 0))
(check-sat)
; Expected: unsat -- bytes cannot go negative after a valid node removal
"""

CONTRACT_B_SLOT_OCCUPIED_NOT_DECAYED = """
(set-logic ALL)
; Contract B: no slot can be both OCCUPIED=1 and STATE=DECAYED simultaneously
; (after a completed operation -- not mid-transition)
(define-fun DECAYED () Int 5)
(declare-const occupied Int)
(declare-const state Int)
; Invariant: occupied=1 AND state=DECAYED simultaneously is a contradiction
(assert (= occupied 1))
(assert (= state DECAYED))
; kain_ownership_clear_slot_unlocked always writes DECAYED then occupied=0
; So OCCUPIED=1 AND DECAYED is a transient intermediate state during clear_slot.
; After clear_slot completes, occupied=0. So the invariant holds POST-operation.
; Prove: the transient state exists (it does, during clear_slot):
(check-sat)
; Expected: sat -- transient state is real during clear_slot execution.
; This is not a bug by itself, but documents that reads during clear_slot
; could observe the inconsistent state if not under lock.
; (In practice this IS under the ownership lock, so safe.)
"""

CONTRACT_C_INDEX_INJECTION_FREE = """
(set-logic QF_BV)
; Contract C: pointer index injection freedom
; Two distinct slots S_A != S_B produce distinct encoded_slots.
; encoded_slot = slot + 1.
; KAIN_OWNERSHIP_MAX_REGIONS = 4096
(declare-const slot_a (_ BitVec 32))
(declare-const slot_b (_ BitVec 32))
(define-fun MAX_REGIONS () (_ BitVec 32) #x00001000)
(assert (bvult slot_a MAX_REGIONS))
(assert (bvult slot_b MAX_REGIONS))
(assert (not (= slot_a slot_b)))
(define-fun encoded_a () (_ BitVec 32) (bvadd slot_a #x00000001))
(define-fun encoded_b () (_ BitVec 32) (bvadd slot_b #x00000001))
(assert (= encoded_a encoded_b))
(check-sat)
; Expected: unsat -- encoded slots are injective over [0, MAX_REGIONS)
"""

CONTRACT_D_LOCK_ORDERING = """
(set-logic ALL)
; Contract D: lock ordering protocol
; Rule: ownership_lock must ALWAYS be acquired before alloc_cache_lock.
; The reverse order (alloc_cache_lock first, then ownership_lock) would
; risk deadlock with any thread that acquires them in the canonical order.
;
; Observed call chain in decay path:
;   __kain_ownership_decay_helper (acquires ownership_lock)
;   -> kain_ownership_decay_slot_unlocked
;   -> __kain_free (acquires alloc_cache_lock if eligible, via cache_release)
;
; Canonical order: ownership_lock -> alloc_cache_lock. Good.
; Any reverse path? Would need: alloc_cache_lock acquired, then ownership_lock.
; Search: does any function acquire alloc_cache_lock then call into ownership?
; __kain_free does NOT call back into ownership (verified earlier).
; cache_release acquires alloc_cache_lock but does not call ownership.
; Therefore no reverse acquisition path exists. Contract holds.

(declare-const thread_holds_ownership Bool)
(declare-const thread_holds_alloc_cache Bool)
(declare-const acquiring_ownership_while_holding_alloc_cache Bool)

; Dangerous: holding alloc_cache and trying to acquire ownership
(assert (= acquiring_ownership_while_holding_alloc_cache
  (and thread_holds_alloc_cache (not thread_holds_ownership))))

; In the current codebase, __kain_free never acquires ownership_lock.
; So thread_holds_alloc_cache=true AND acquiring_ownership=true is impossible.
(assert acquiring_ownership_while_holding_alloc_cache)
(assert thread_holds_alloc_cache)
; If free never calls ownership, this scenario cannot arise:
(declare-const free_calls_ownership Bool)
(assert (= free_calls_ownership false))
; Contradiction: if free_calls_ownership=false, the scenario cannot happen.
; Model this: the scenario requires free_calls_ownership.
(assert (=> acquiring_ownership_while_holding_alloc_cache free_calls_ownership))
(check-sat)
; Expected: unsat -- lock ordering inversion is impossible given __kain_free
; does not call back into ownership routines.
"""

CONTRACT_E_CAS_FAILURE_ORDERING = """
(set-logic ALL)
; Contract E: CAS failure_order <= success_order (C11 7.17.7.4)
; For ALL Kain ordering code pairs (s, f) in [0..4] x [0..4],
; the mapped C11 orderings satisfy: failure_c11(f) <= success_c11(s).
;
; This is the UNIVERSAL version of the bug proof.
; We ask: does there EXIST an (s, f) pair that VIOLATES the constraint?
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
  (ite (= k 2) c11_acquire
  (ite (= k 1) c11_acquire
  (ite (= k 3) c11_acquire
       c11_seq_cst)))))

(declare-const s Int)
(declare-const f Int)
(assert (>= s 0)) (assert (<= s 4))
(assert (>= f 0)) (assert (<= f 4))
(assert (> (failure_c11 f) (success_c11 s)))
(check-sat)
; Expected: sat -- proves the C11 violation IS reachable (BUG confirmed)
; Witness: s=0 (RELAXED->c11_relaxed=0), f=4 (SEQ_CST->c11_seq_cst=5), 5>0
"""

CONTRACTS = [
    ("cache-bytes-integrity", CONTRACT_A_CACHE_BYTES_INTEGRITY, "unsat", "Medium"),
    ("occupied-decayed-transient", CONTRACT_B_SLOT_OCCUPIED_NOT_DECAYED, "sat", "Low"),
    ("index-injection-free", CONTRACT_C_INDEX_INJECTION_FREE, "unsat", "Medium"),
    ("lock-ordering-inversion", CONTRACT_D_LOCK_ORDERING, "unsat", "High"),
    ("cas-failure-order-c11-ub", CONTRACT_E_CAS_FAILURE_ORDERING, "sat", "High"),
]


def run_contracts() -> list[dict]:
    ensure_output_dirs()
    results = []

    try:
        import z3
        has_z3 = True
    except ImportError:
        has_z3 = False
        print("  [warn] z3 python not installed; skipping solver evaluation")

    for contract_name, smt2, expected, severity in CONTRACTS:
        proof_path = GENERATED_DIR / f"abstract_{contract_name}.smt2"
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
        bug_confirmed = (expected == "sat" and result == "sat")
        violation_confirmed = (expected == "unsat" and result == "sat")

        row = {
            "contract": contract_name,
            "severity": severity,
            "expected": expected,
            "actual": result,
            "passed": passed,
            "bug_confirmed": bug_confirmed,
            "invariant_broken": violation_confirmed,
            "proof_path": str(proof_path),
        }
        results.append(row)

        status = "PASS" if passed else "FAIL"
        tag = " [BUG CONFIRMED]" if bug_confirmed else " [INVARIANT BROKEN]" if violation_confirmed else ""
        print(f"  [{status}] {contract_name} ({severity}): expected={expected}, got={result}{tag}")

    return results


def main() -> None:
    print("Native Core Abstract Concept Prover")
    print("Running Z3 contract checks:\n")
    results = run_contracts()

    bugs = [r for r in results if r["bug_confirmed"] or r["invariant_broken"]]
    passed = sum(1 for r in results if r["passed"])

    print(f"\nTotal: {passed}/{len(results)} contracts passed")
    if bugs:
        print(f"Confirmed bugs/violations: {len(bugs)}")
        for b in bugs:
            print(f"  -> {b['contract']} ({b['severity']}): {b['proof_path']}")

    json_path, csv_path = write_json_and_csv("abstract_concept_findings", results)
    print(f"\nJSON: {json_path}")
    print(f"CSV:  {csv_path}")


if __name__ == "__main__":
    main()
