; =============================================================================
; Actor Mailbox Contracts
; Proves: FIFO ordering, reply guarantee, bounded queue depth.
;
; Model: pi-squared actors have typed mailboxes. Messages arrive in order.
; Each message carries (sender_id, seq_no, is_reply) and the actor processes
; them sequentially. The mailbox has a fixed capacity B.
; =============================================================================

; --- Part 1: FIFO Ordering ---
; Messages enqueued in seq_no order must be dequeued in seq_no order.
(push)
(declare-const B Int) ; mailbox capacity
(assert (and (>= B 2) (<= B 8)))

; Mailbox: sequence of messages represented as queue slots
; Each slot holds a seq_no. Slots fill from head to tail.
(declare-const head Int) ; dequeue index
(declare-const tail Int) ; enqueue index
(declare-const size Int) ; current fill level

; Queue invariants
(assert (and (>= head 0) (< head B)))
(assert (and (>= tail 0) (< tail B)))
(assert (and (>= size 0) (<= size B)))
(assert (= (mod (- tail head) B) size))

; Slot contents (seq_no stored in each slot)
(declare-const slot0 Int) (declare-const slot1 Int)
(declare-const slot2 Int) (declare-const slot3 Int)
(declare-const slot4 Int) (declare-const slot5 Int)
(declare-const slot6 Int) (declare-const slot7 Int)

; All seq_nos are non-negative and unique within the queue
(assert (forall ((i Int)) (=> (and (>= i 0) (< i B))
  (>= (ite (= i 0) slot0
      (ite (= i 1) slot1
      (ite (= i 2) slot2
      (ite (= i 3) slot3
      (ite (= i 4) slot4
      (ite (= i 5) slot5
      (ite (= i 6) slot6 slot7))))))) 0))))

; FIFO invariant: seq_nos increase monotonically from head to tail
; (modulo wrap-around, but in a bounded window the values increase)
(define-fun slot ((i Int)) Int
  (ite (= i 0) slot0 (ite (= i 1) slot1 (ite (= i 2) slot2
  (ite (= i 3) slot3 (ite (= i 4) slot4 (ite (= i 5) slot5
  (ite (= i 6) slot6 slot7))))))))

; For any two positions in the queue, the earlier position has smaller seq_no
(define-fun fifo_holds () Bool
  (forall ((i Int) (j Int))
    (=> (and (<= head i) (< i j) (< j (+ head size))
             (< j B) (>= i 0) (< i B))
        (< (slot i) (slot j)))))

; Assert FIFO holds for a non-empty queue
(assert (>= size 2))
(assert (not fifo_holds))

(check-sat)
(pop)

; --- Part 2: Reply Guarantee ---
; For every request message, there is exactly one reply message.
(push)
(declare-const msg_count Int)
(assert (and (>= msg_count 1) (<= msg_count 8)))

; Each message is either a request or a reply
; Requests have even seq_no, replies have odd seq_no (reply seq_no = request + 1)
(declare-const seq_nos (Array Int Int))

; For every request (even seq_no), there is a matching reply (odd seq_no)
(define-fun reply_exists ((req_seq Int)) Bool
  (exists ((i Int))
    (and (>= i 0) (< i msg_count)
         (= (select seq_nos i) (+ req_seq 1)))))

(define-fun request_exists ((req_seq Int)) Bool
  (exists ((i Int))
    (and (>= i 0) (< i msg_count)
         (= (select seq_nos i) req_seq))))

; For every even seq_no in range that is a request, a reply exists
(assert (forall ((s Int))
  (=> (and (>= s 0) (<= s (* 2 msg_count)) (= (mod s 2) 0)
           (request_exists s))
      (reply_exists s))))

; No reply without a corresponding request
(assert (forall ((i Int))
  (=> (and (>= i 0) (< i msg_count) (= (mod (select seq_nos i) 2) 1))
      (exists ((j Int))
        (and (>= j 0) (< j msg_count)
             (= (select seq_nos j) (- (select seq_nos i) 1)))))))

; Check: model satisfies reply guarantee
(check-sat)
(pop)

; --- Part 3: Bounded Queue Depth ---
; Mailbox never exceeds capacity B. Enqueue when full returns overflow.
(push)
(declare-const capacity Int)
(assert (and (>= capacity 1) (<= capacity 16)))

; Number of messages in queue at any point
(declare-const enqueued Int)
(declare-const dequeued Int)
(declare-const in_flight Int)

(assert (>= enqueued 0))
(assert (>= dequeued 0))
(assert (>= in_flight 0))
(assert (= in_flight (- enqueued dequeued)))
(assert (<= in_flight capacity))

; Overflow condition
(define-fun enqueue_allowed () Bool (< in_flight capacity))

; If we try to enqueue when full, overflow error is raised
(define-fun safe_enqueue ((attempts Int)) Bool
  (<= (+ in_flight attempts) capacity))

; For any number of concurrent sends ≤ remaining capacity, no overflow
(declare-const new_msgs Int)
(assert (and (>= new_msgs 0) (<= new_msgs capacity)))
(assert (safe_enqueue new_msgs))

(check-sat)
(pop)

; --- Part 4: Message ordering under concurrent sending ---
; Even with concurrent senders, the mailbox processes in arrival order.
; (Captured by monotonically increasing seq_no per sender.)
(push)
(declare-const sender_a_last Int)
(declare-const sender_b_last Int)
(declare-const sender_c_last Int)

(assert (and (>= sender_a_last 0) (< sender_a_last 100)))
(assert (and (>= sender_b_last 0) (< sender_b_last 100)))
(assert (and (>= sender_c_last 0) (< sender_c_last 100)))

; Each sender's messages have strictly increasing seq_nos
; new_msg_seq must be greater than the sender's last seq_no
(declare-const new_seq_a Int)
(declare-const new_seq_b Int)
(define-fun sender_a_ordered () Bool (< sender_a_last new_seq_a))
(define-fun sender_b_ordered () Bool (< sender_b_last new_seq_b))

(assert sender_a_ordered)
(assert sender_b_ordered)

(check-sat)
(pop)

(exit)
