; Quantumerlang benchmark arithmetic proof.
; The hot loop uses 64 logical lanes and 300,000 rounds. This proves the
; Kain lane index stays inside the shattered lane/cell arrays and that the
; pre-modulo arithmetic used by both rows remains far inside signed i64.

(set-logic QF_LIA)

(declare-const index Int)
(declare-const lane Int)
(declare-const old_cell Int)
(declare-const bias Int)
(declare-const phase Int)
(declare-const salt Int)

(define-fun modulus () Int 1000000007)
(define-fun workers () Int 64)
(define-fun rounds () Int 300000)
(define-fun i64-max () Int 9223372036854775807)

(assert (>= index 0))
(assert (< index rounds))
(assert (= lane (mod index workers)))
(assert (>= old_cell 0))
(assert (< old_cell modulus))
(assert (>= bias 1))
(assert (<= bias 97))
(assert (>= phase 1))
(assert (<= phase 89))
(assert (>= salt 1))
(assert (<= salt 101))

(define-fun request_raw () Int (+ (* index 13) old_cell lane))
(define-fun alive_reply_raw () Int (+ (* (mod request_raw modulus) 17) bias phase salt lane))
(define-fun inactive_reply_raw () Int (+ (* (mod request_raw modulus) 17) bias salt lane modulus (- phase)))
(define-fun flux_alive_raw () Int (+ (* (mod alive_reply_raw modulus) 31) 7))
(define-fun flux_inactive_raw () Int (+ (* (mod inactive_reply_raw modulus) 31) 7))
(define-fun next_alive_raw () Int (+ (mod flux_alive_raw modulus) old_cell index lane))
(define-fun next_inactive_raw () Int (+ (mod flux_inactive_raw modulus) old_cell index lane))

(assert
  (not
    (and
      (>= lane 0)
      (< lane workers)
      (< (* lane 8) (* workers 8))
      (< request_raw i64-max)
      (< alive_reply_raw i64-max)
      (< inactive_reply_raw i64-max)
      (< flux_alive_raw i64-max)
      (< flux_inactive_raw i64-max)
      (< next_alive_raw i64-max)
      (< next_inactive_raw i64-max))))

(check-sat)
