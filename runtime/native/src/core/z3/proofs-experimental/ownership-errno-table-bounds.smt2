; Proof: Errno table lookup is always in bounds
;
; The function kain_ownership_errno_from_status computes:
;   int idx = -status;
;   if ((unsigned int)idx <= 9u) return KAIN_OWNERSHIP_ERRNO_TABLE[idx];
;   return EINVAL;
;
; Status values:
;   0  (OK)              -> idx = 0
;   -1 (ERR_INVALID)     -> idx = 1
;   -2 (ERR_NOT_FOUND)   -> idx = 2
;   -3 (ERR_CAPACITY)    -> idx = 3
;   -4 (ERR_OBSERVED)    -> idx = 4
;   -5 (ERR_COLLAPSED)   -> idx = 5
;   -6 (ERR_DECAYED)     -> idx = 6
;   -7 (ERR_OVERFLOW)    -> idx = 7
;   -8 (ERR_NOT_OBSERVED)-> idx = 8
;   -9 (ERR_NOT_COLLAPSED)-> idx = 9
;
; All status values are in [-9, 0], so idx = -status is in [0, 9].
; The table has 10 entries (indices 0-9).
;
; This proves the guard (unsigned int)idx <= 9u always passes for
; valid status values, so no out-of-bounds access occurs.

(set-logic QF_BV)

(declare-const status (_ BitVec 32))
(declare-const max_neg_status (_ BitVec 32))
(declare-const table_upper_bound (_ BitVec 32))

; status range: [ -9, 0 ] — encoded as 32-bit signed bitvectors
; Actually, status is a regular int (C). In QF_BV, we model it as 32-bit signed.
; The known values: status ∈ {-9, -8, ..., -1, 0}
; We use the unsigned interpretation: status as bv32 ranges from 0xFFFFFFF7 to 0.

; Model status as any value in [-9, 0] using signed comparison
(assert (= max_neg_status (_ bv9 32)))
(assert (= table_upper_bound (_ bv9 32)))

; status in [-9, 0] as signed integer
(assert (bvsge status (bvneg (_ bv9 32))))  ; status >= -9
(assert (bvsle status (_ bv0 32)))           ; status <= 0

(define-fun idx () (_ BitVec 32)
  (bvneg status))  ; -status (as unsigned, since status <= 0, -status >= 0)

; The guard: (unsigned int)idx <= 9u
; In BV32: idx <= 9
(define-fun guard_passes () Bool
  (bvule idx table_upper_bound))

; Claim: For all valid status values in [-9, 0], the guard always passes
; (idx <= 9). If this is unsat, the guard never fails.
(assert (not guard_passes))
(check-sat)

(reset)

; ============================================================
; Claim 2: The table has exactly 10 entries covering all 10 cases
; ============================================================
(set-logic QF_BV)

; Enumerate all valid status values and verify each maps to unique idx
(declare-const s0 (_ BitVec 32))
(declare-const s1 (_ BitVec 32))
(declare-const s2 (_ BitVec 32))
(declare-const s3 (_ BitVec 32))
(declare-const s4 (_ BitVec 32))
(declare-const s5 (_ BitVec 32))
(declare-const s6 (_ BitVec 32))
(declare-const s7 (_ BitVec 32))
(declare-const s8 (_ BitVec 32))
(declare-const s9 (_ BitVec 32))

; All status values are in [-9, 0] and distinct
(assert (= s0 (_ bv0 32)))     ; 0
(assert (= s1 (bvneg (_ bv1 32))))   ; -1
(assert (= s2 (bvneg (_ bv2 32))))   ; -2
(assert (= s3 (bvneg (_ bv3 32))))   ; -3
(assert (= s4 (bvneg (_ bv4 32))))   ; -4
(assert (= s5 (bvneg (_ bv5 32))))   ; -5
(assert (= s6 (bvneg (_ bv6 32))))   ; -6
(assert (= s7 (bvneg (_ bv7 32))))   ; -7
(assert (= s8 (bvneg (_ bv8 32))))   ; -8
(assert (= s9 (bvneg (_ bv9 32))))   ; -9

; Verify all distinct
(assert (not (distinct s0 s1 s2 s3 s4 s5 s6 s7 s8 s9)))
(check-sat)

(reset)

; ============================================================
; Claim 3: idx = -status maps each valid status to a unique index [0,9]
; ============================================================
(set-logic QF_BV)

(declare-const status (_ BitVec 32))
(assert (bvsge status (bvneg (_ bv9 32))))
(assert (bvsle status (_ bv0 32)))

(define-fun idx () (_ BitVec 32) (bvneg status))

; The maximum idx value is when status is most negative
; For status = -9: idx = 9
; For status = -1: idx = 1
; All idx values are within [0, 9]
; Prove idx never exceeds 9
(assert (bvugt idx (_ bv9 32)))
(check-sat)
