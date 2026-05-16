; Bit-vector proof for the dirty `&`, `^`, and `|` packet mix.
; Counterexample query: if the expression differs from the intended 130,
; Z3 returns sat. The expected result is unsat.

(set-logic QF_BV)

(define-fun packet_mix () (_ BitVec 64)
  (bvadd
    (bvand (_ bv123 64) (_ bv63 64))
    (bvxor (_ bv45 64) (_ bv5 64))
    (bvor (_ bv29 64) (_ bv7 64))))

(assert (not (= packet_mix (_ bv130 64))))

(check-sat)
