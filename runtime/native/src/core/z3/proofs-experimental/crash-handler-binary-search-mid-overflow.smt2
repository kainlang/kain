; crash-handler-binary-search-mid-overflow.smt2
;
; Prove: The midpoint calculation `lo + (hi - lo) / 2` in lookup_crash_entry
; never overflows uint64_t (size_t on Win64) and always satisfies
; lo <= mid < hi for the valid search range.
;
; Source: X:/runtime/native/src/core/crash_handler.c
; Function: lookup_crash_entry (static)
;
; The binary search invariant:
;   size_t lo = 0;
;   size_t hi = crash_table_count;   // max 4096 (KAIN_CRASH_TABLE_MAX)
;   while (lo < hi) {
;       size_t mid = lo + (hi - lo) / 2;
;       ...
;   }
;
; Since hi <= 4096 and lo < hi, the values are tiny relative to 64-bit range.
; But we prove the stronger invariant: lo <= mid < hi always holds,
; which guarantees loop progress (hi-lo strictly decreases each iteration).

(set-logic QF_BV)

; ── 64-bit bitvector model of size_t ──
(declare-const lo (_ BitVec 64))
(declare-const hi (_ BitVec 64))

; Precondition: 0 <= lo < hi <= 4096 (crash_table_count max)
(assert (bvult lo hi))
(assert (bvule hi (_ bv4096 64)))

; Midpoint calculation
(define-fun diff () (_ BitVec 64) (bvsub hi lo))
; (hi - lo) / 2  using logical shift right (unsigned division by 2)
(define-fun half_diff () (_ BitVec 64) (bvlshr diff (_ bv1 64)))
(define-fun mid () (_ BitVec 64) (bvadd lo half_diff))

; ── Claim 1: mid does not overflow (mid < 2^64 is automatic for BV, but
;   we verify mid never wraps: lo + half_diff >= lo)
(push)
(assert (not (bvuge mid lo)))
(check-sat)
(pop)
; Expected: unsat — mid never wraps below lo

; ── Claim 2: mid < hi (mid is strictly below hi, ensuring loop progress)
(push)
(assert (not (bvult mid hi)))
(check-sat)
(pop)
; Expected: unsat — mid < hi always

; ── Claim 3: lo <= mid (mid is at least lo, preserving lower bound)
(push)
(assert (not (bvuge mid lo)))
(check-sat)
(pop)
; Expected: unsat — mid >= lo always

; ── Claim 4: mid <= hi - 1 (worst case: mid == hi-1 when lo = hi-1)
(push)
(assert (not (bvule mid (bvsub hi (_ bv1 64)))))
(check-sat)
(pop)
; Expected: unsat — mid <= hi-1 always

; ── Claim 5: The interval shrinks: (hi' - lo') < (hi - lo) for the
;   two possible branch outcomes (upper and lower half). This proves loop
;   termination.
;
; Branch outcome A: target <= ip_val → lo' = mid + 1
(define-fun lo_prime_A () (_ BitVec 64) (bvadd mid (_ bv1 64)))
(define-fun diff_prime_A () (_ BitVec 64) (bvsub hi lo_prime_A))
(push)
; If diff > 1, then diff_prime_A < diff (guaranteed progress)
(assert (bvugt diff (_ bv1 64)))
(assert (not (bvult diff_prime_A diff)))
(check-sat)
(pop)
; Expected: unsat — the upper-half branch always shrinks the interval

; Branch outcome B: target > ip_val → hi' = mid
(define-fun hi_prime_B () (_ BitVec 64) mid)
(define-fun diff_prime_B () (_ BitVec 64) (bvsub hi_prime_B lo))
(push)
; If diff > 1, then diff_prime_B < diff
(assert (bvugt diff (_ bv1 64)))
(assert (not (bvult diff_prime_B diff)))
(check-sat)
(pop)
; Expected: unsat — the lower-half branch always shrinks the interval

; ── Claim 6: Even in the worst-case scenario where lo=0 and hi=2^64-1,
;   the mid formula still works (no overflow) — this proves the formula is
;   universally safe for any unsigned range.
(declare-const lo_any (_ BitVec 64))
(declare-const hi_any (_ BitVec 64))
(assert (bvult lo_any hi_any))  ; any lo < hi

(define-fun diff_any () (_ BitVec 64) (bvsub hi_any lo_any))
(define-fun half_any () (_ BitVec 64) (bvlshr diff_any (_ bv1 64)))
(define-fun mid_any () (_ BitVec 64) (bvadd lo_any half_any))

; mid_any >= lo_any (no wrap below)
(push)
(assert (not (bvuge mid_any lo_any)))
(check-sat)
(pop)
; Expected: unsat

; mid_any < hi_any (strictly below hi)
(push)
(assert (not (bvult mid_any hi_any)))
(check-sat)
(pop)
; Expected: unsat

; Summary:
; All unsat → the midpoint formula lo + (hi-lo)/2 is universally safe for
; unsigned binary search. It never overflows, always stays in [lo, hi-1],
; and the interval strictly shrinks each iteration.
