; Experimental proof for the HTTP concurrency accepted-socket staging array.
;
; Claim:
; - if a worker claims socket index i
; - and 0 <= i < rounds
; - then the byte span [i * sizeof(SOCKET), i * sizeof(SOCKET) + sizeof(SOCKET))
;   stays inside malloc(rounds * sizeof(SOCKET)).
(set-logic QF_LIA)

(declare-const rounds Int)
(declare-const claim Int)

(define-fun socket_bytes () Int 8)
(define-fun total_bytes () Int (* rounds socket_bytes))
(define-fun offset () Int (* claim socket_bytes))

(assert (>= rounds 1))
(assert (>= claim 0))
(assert (< claim rounds))

(assert
  (not
    (and
      (>= offset 0)
      (<= (+ offset socket_bytes) total_bytes))))

(check-sat)
