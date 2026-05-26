(set-logic QF_BV)

(declare-fun stale_value () (_ BitVec 64))
(declare-fun written_value () (_ BitVec 64))
(declare-fun no_read_before_store () Bool)
(declare-fun full_width_store () Bool)

(assert no_read_before_store)
(assert full_width_store)

; Eager path: the ephemeral cell is zeroed first, then completely overwritten.
(define-fun eager_value_after_zero () (_ BitVec 64) #x0000000000000000)
(define-fun eager_value_after_store () (_ BitVec 64) written_value)
(define-fun eager_loaded_value () (_ BitVec 64) eager_value_after_store)

; Elided path: the pre-store bytes are arbitrary, but the full-width write still
; dominates the first read, so the stale contents cannot leak.
(define-fun elided_value_before_store () (_ BitVec 64) stale_value)
(define-fun elided_value_after_store () (_ BitVec 64) written_value)
(define-fun elided_loaded_value () (_ BitVec 64) elided_value_after_store)

(assert
  (or
    (not (= eager_value_after_store elided_value_after_store))
    (not (= eager_loaded_value elided_loaded_value))))

(check-sat)
