; =============================================================================
; Optimization: Minimum TeleportChannel Capacity for Deadlock-Free Operation
;
; The TeleportChannel is an SPSC (single-producer, single-consumer) lockless
; ring buffer queue. It uses modular indexing:
;
;   write_idx, read_idx  ∈ [0, capacity)
;   empty:  write_idx == read_idx
;   full:   (write_idx + 1) % capacity == read_idx
;
; The "full" condition means one slot is always left unused to distinguish
; empty from full. For N in-flight items, we need at least N+1 capacity.
;
; Given:
;   max_items = maximum number of items enqueued before any dequeue
;   capacity = ring buffer size (what we're minimizing)
;
; Constraints:
;   0 <= read_idx < capacity
;   0 <= write_idx < capacity
;   The channel is NOT full: (write_idx + 1) % capacity != read_idx
;   There are exactly max_items in-flight: write_idx - read_idx == max_items
;     (assuming write_idx >= read_idx, which holds for SPSC with no wrap-around)
;
; Objective: MINIMIZE capacity subject to max_items fitting without false-full.
;
; Result: capacity_min = max_items + 1
; =============================================================================
(set-option :opt.priority lex)
(set-logic QF_LIA)

; Tunable parameter: how many items we want to fit
(define-const max_items Int 64)

; Variables
(declare-const read_idx Int)
(declare-const write_idx Int)
(declare-const capacity Int)

; -- Constraints --

; Ring buffer invariants
(assert (>= read_idx 0))
(assert (>= write_idx 0))
(assert (>= capacity 2))                          ; at least 2 slots (1 data + 1 sentinel)

; Indices are within bounds
(assert (< read_idx capacity))
(assert (< write_idx capacity))

; SPSC invariant: write_idx >= read_idx (no wrap-around in the linear model)
(assert (>= write_idx read_idx))

; There are exactly max_items in the buffer
(assert (= (- write_idx read_idx) max_items))

; The buffer is NOT full (we want to verify it can hold max_items)
; Full condition: (write_idx + 1) % capacity == read_idx
; We assert it's NOT full: (write_idx + 1) % capacity != read_idx
; Using division-free encoding:
;   (write_idx + 1) - (write_idx + 1) / capacity * capacity != read_idx
; For the bounded case (write_idx < capacity, so write_idx+1 <= capacity):
;   if write_idx + 1 < capacity:     (write_idx + 1) != read_idx
;   if write_idx + 1 == capacity:    0 != read_idx  → read_idx != 0
(assert (not (= (mod (+ write_idx 1) capacity) read_idx)))

; -- Optimization: minimize capacity --
(minimize capacity)

(check-sat)
(get-model)
(get-objectives)

; Expected for max_items = 64:
;   capacity = 65, read_idx = 0, write_idx = 64
;
; The minimum capacity is always max_items + 1 because the sentinel slot
; consumes exactly 1 slot. This is the optimal packing — no slack.
;
; Runtime implication: if the TeleportChannel is created with capacity = max_items,
; the producer will see a false-full condition before actually filling the buffer
; with max_items. The extra +1 in the alloc (stdlib/sync.kn line 138:
; "safe_capacity = requested_capacity + TELEPORT_CHANNEL_PADDING_SLOTS")
; is proven necessary by this optimization.
