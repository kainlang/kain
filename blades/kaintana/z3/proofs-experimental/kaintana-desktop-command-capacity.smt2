(set-logic QF_LIA)

(declare-const command_count Int)
(declare-const capacity Int)

(assert (= capacity 2048))
(assert (>= command_count 0))
(assert (< command_count capacity))

; If we only append under `command_count < capacity`,
; the next count is still within the fixed command arena.
(assert (not (<= (+ command_count 1) capacity)))

(check-sat)
