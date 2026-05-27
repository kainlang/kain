(set-logic QF_BV)

(declare-const count (_ BitVec 64))

; Model the guarded dequeue in __kain_ownership_flush_deferred_decay():
;   if count == 0 break; else count = count - 1
(assert (bvugt count #x0000000000000000))

; Counterexample query: can count - 1 wrap upward past the prior count?
(assert (bvugt (bvsub count #x0000000000000001) count))

(check-sat)
