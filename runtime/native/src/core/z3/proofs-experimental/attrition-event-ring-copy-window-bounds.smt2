; Experimental proof for the attrition flight-recorder ring extraction in
; attrition.c.
; Claim: once event_next_index is a valid ring cursor, the copy window derived
; by available/count/start_index and every copied candidate slot stay inside the
; 1024-entry event ring.
(set-logic QF_LIA)

(declare-const event_write_count Int)
(declare-const max_events Int)
(declare-const event_next_index Int)
(declare-const i Int)

(define-fun capacity () Int 1024)
(define-fun available () Int
  (ite (< event_write_count capacity) event_write_count capacity))
(define-fun count () Int
  (ite (< available max_events) available max_events))
(define-fun start_index () Int
  (mod (+ event_next_index (- capacity count)) capacity))
(define-fun candidate_index () Int
  (mod (+ start_index i) capacity))

(assert (>= event_write_count 0))
(assert (>= max_events 0))
(assert (>= event_next_index 0))
(assert (< event_next_index capacity))
(assert (>= i 0))
(assert (< i count))

(assert
  (or
    (< count 0)
    (> count capacity)
    (< start_index 0)
    (>= start_index capacity)
    (< candidate_index 0)
    (>= candidate_index capacity)))

(check-sat)
