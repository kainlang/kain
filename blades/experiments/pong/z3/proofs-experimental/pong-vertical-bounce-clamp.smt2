(set-logic QF_LIA)

(declare-const board_height Int)
(declare-const ball_size Int)
(declare-const ball_y Int)
(declare-const ball_dy Int)

(define-fun board_ball_max_y () Int (- board_height ball_size))
(define-fun next_ball_y () Int (+ ball_y ball_dy))
(define-fun clamped_ball_y () Int
  (ite (< next_ball_y 0)
       0
       (ite (> next_ball_y board_ball_max_y)
            board_ball_max_y
            next_ball_y)))

(assert (> board_height 0))
(assert (> ball_size 0))
(assert (> board_height ball_size))
(assert (or (<= next_ball_y 0) (>= next_ball_y board_ball_max_y)))
(assert (or (< clamped_ball_y 0) (> clamped_ball_y board_ball_max_y)))

(check-sat)
