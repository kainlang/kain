(set-logic QF_LIA)

(declare-const current Int)
(declare-const target Int)
(declare-const speed Int)
(declare-const board_height Int)
(declare-const paddle_height Int)

(define-fun paddle_limit () Int (- board_height paddle_height))
(define-fun clamp_int ((value Int)) Int
  (ite (< value 0)
       0
       (ite (> value paddle_limit)
            paddle_limit
            value)))
(define-fun driven_paddle () Int
  (ite (< current target)
       (clamp_int (+ current speed))
       (ite (> current target)
            (clamp_int (- current speed))
            (clamp_int current))))

(assert (> board_height 0))
(assert (>= paddle_height 0))
(assert (>= board_height paddle_height))
(assert (>= speed 0))
(assert (>= current 0))
(assert (<= current paddle_limit))
(assert (or (< driven_paddle 0) (> driven_paddle paddle_limit)))

(check-sat)
