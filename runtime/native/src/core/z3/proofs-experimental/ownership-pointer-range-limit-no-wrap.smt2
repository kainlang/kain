(set-logic QF_BV)

(declare-const base (_ BitVec 64))
(declare-const size (_ BitVec 64))

; Model the guard used by kain_ownership_try_range_limit():
;   size <= UINTPTR_MAX - base
(assert (bvule size (bvsub #xffffffffffffffff base)))

; Counterexample query: can base + size wrap below base after the guard?
(assert (bvult (bvadd base size) base))

(check-sat)
