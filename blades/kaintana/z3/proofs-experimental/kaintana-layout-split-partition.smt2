(set-logic QF_NRA)

(declare-const width Real)
(declare-const gap Real)
(declare-const fraction Real)
(declare-const left_width Real)
(declare-const right_width Real)

(assert (>= width 0.0))
(assert (>= gap 0.0))
(assert (>= width gap))
(assert (>= fraction 0.0))
(assert (<= fraction 1.0))

(assert (= left_width (* (- width gap) fraction)))
(assert (= right_width (- width left_width gap)))

; Bad partition: either side goes negative or the split stops conserving width.
(assert (or (< left_width 0.0)
            (< right_width 0.0)
            (not (= (+ left_width gap right_width) width))))

(check-sat)
