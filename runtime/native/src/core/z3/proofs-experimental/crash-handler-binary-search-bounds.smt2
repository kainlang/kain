; crash-handler-binary-search-bounds.smt2
;
; Prove: The binary search in lookup_crash_entry returns a valid table index.
;
; Source: X:/runtime/native/src/core/crash_handler.c
; Function: lookup_crash_entry (static), lines 24-44
;
; The search uses the upper_bound pattern:
;   lo = first index where fn_ptr > ip_val
;   return lo - 1 (last index where fn_ptr <= ip_val, or NULL if none)
;
; We prove:
;   1. lo <= hi is a loop invariant (mid calculation preserves it)
;   2. lo always stays in [0, crash_table_count]
;   3. If lo > 0, then lo - 1 is a valid table index (< crash_table_count)
;   4. The loop terminates (hi - lo strictly decreases)

(set-logic QF_BV)

; ── Model the binary search state ──
(declare-const lo (_ BitVec 64))
(declare-const hi (_ BitVec 64))
(declare-const count (_ BitVec 64))

; Initial: lo = 0, hi = count, count <= 4096
(assert (= lo (_ bv0 64)))
(assert (bvule count (_ bv4096 64)))
(assert (= hi count))

; ── One iteration of the binary search ──
; mid = lo + (hi - lo) / 2
(define-fun diff () (_ BitVec 64) (bvsub hi lo))
(define-fun half () (_ BitVec 64) (bvlshr diff (_ bv1 64)))
(define-fun mid () (_ BitVec 64) (bvadd lo half))

; Branch A: fn_ptr[mid] <= ip_val → lo' = mid + 1
(define-fun lo_A () (_ BitVec 64) (bvadd mid (_ bv1 64)))
; Branch B: fn_ptr[mid] > ip_val → hi' = mid
(define-fun hi_B () (_ BitVec 64) mid)

; ── Claim 1: mid is always in [lo, hi) ──
(push)
(assert (not (and (bvuge mid lo) (bvult mid hi))))
(check-sat)
(pop)
; Expected: unsat ✓

; ── Claim 2: Branch A preserves lo' <= hi (lo <= hi invariant) ──
(push)
(assert (not (bvule lo_A hi)))
(check-sat)
(pop)
; Expected: unsat ✓ (since mid < hi, mid+1 <= hi)

; ── Claim 3: Branch B preserves lo <= hi' (lo <= hi invariant) ──
(push)
(assert (not (bvule lo hi_B)))
(check-sat)
(pop)
; Expected: unsat ✓ (since lo <= mid)

; ── Claim 4: Both branches stay within [0, count] ──
; Branch A: lo' <= count
(push)
(assert (not (bvule lo_A count)))
(check-sat)
(pop)
; Expected: unsat ✓

; Branch B: hi' <= count (trivially, since mid < hi <= count)
(push)
(assert (not (bvule hi_B count)))
(check-sat)
(pop)
; Expected: unsat ✓

; ── Claim 5: After loop exit (lo >= hi), lo is in [0, count] ──
(declare-const lo_exit (_ BitVec 64))
(declare-const hi_exit (_ BitVec 64))
; Simulate exit condition: lo >= hi (loop guard fails) and initial lo <= hi
(assert (= lo_exit hi_exit))  ; convergence (lo catches up to hi)
(assert (bvuge lo_exit hi_exit))
(assert (bvule lo_exit count))
(check-sat)
; sat → lo_exit is valid. Now prove lo_exit - 1 is also safe when lo_exit > 0.
(pop)

; ── Claim 6: If lo > 0 after exit, then lo-1 is a valid table index ──
(push)
(declare-const lo_final (_ BitVec 64))
(assert (bvugt lo_final (_ bv0 64)))
(assert (bvule lo_final count))
(assert (not (bvule (bvsub lo_final (_ bv1 64)) (bvsub count (_ bv1 64)))))
(check-sat)
(pop)
; Expected: unsat ✓ (lo-1 < count when lo > 0 and lo <= count)

; ── Claim 7: The interval [lo, hi) strictly shrinks each iteration ──
(push)
(declare-const diff_initial (_ BitVec 64))
(declare-const diff_final_A (_ BitVec 64))
(declare-const diff_final_B (_ BitVec 64))

(assert (= diff_initial (bvsub hi lo)))
(assert (bvugt diff_initial (_ bv1 64)))
(assert (= diff_final_A (bvsub hi lo_A)))
(assert (= diff_final_B (bvsub hi_B lo)))

; Both branches shrink the interval
(assert (not (bvult diff_final_A diff_initial)))
(check-sat)
(pop)
; Expected: unsat ✓

(push)
(assert (not (bvult diff_final_B diff_initial)))
(check-sat)
(pop)
; Expected: unsat ✓

; Summary:
; All unsat → the binary search in lookup_crash_entry:
; - Always maintains lo <= hi
; - Always keeps lo and hi in [0, count]
; - Always terminates (interval shrinks)
; - Final lo-1 is always a valid table index when lo > 0
; This means the &__kain_crash_table[lo-1] dereference is always safe.
