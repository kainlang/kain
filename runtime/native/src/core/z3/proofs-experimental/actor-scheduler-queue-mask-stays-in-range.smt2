; Claim: (cursor & 1023) produces a valid slot index for the scheduler queue
; KAIN_SCHEDULER_QUEUE_CAPACITY = 1024 (power of 2)
; KAIN_SCHEDULER_QUEUE_MASK = 1023 = KAIN_SCHEDULER_QUEUE_CAPACITY - 1
;
; Used at actor.c line 4023 (enqueue) and 4132 (dequeue):
;   slot_index = g_scheduler.enqueue_cursor & KAIN_SCHEDULER_QUEUE_MASK;
;
; For any size_t cursor value (0..2^64-1), the masked result is always
; in [0, 1023], which is a valid slot in queue[1024].
;
; Solver result: unsat — mask result is always < capacity
(set-logic QF_BV)
(declare-const cursor (_ BitVec 64))

(define-fun slot ((c (_ BitVec 64))) (_ BitVec 64)
  (bvand c (_ bv1023 64)))

; The mask result must always be less than 1024 (capacity)
(assert (bvuge (slot cursor) (_ bv1024 64)))
(check-sat)
; unsat = result always in valid range ✅
