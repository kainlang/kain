(set-logic ALL)

(declare-const round Int)
(declare-const lane Int)
(declare-const position_x Int)
(declare-const position_y Int)
(declare-const velocity_x Int)
(declare-const velocity_y Int)
(declare-const health Int)
(declare-const team Int)
(declare-const active Bool)

(define-fun contribution ((r Int)) Int
  (let ((round_phase (mod r 5))
        (round_bias (mod r 7)))
    (ite (and active (> health (mod (+ r lane) 11)))
         (let ((motion (+ position_x (* velocity_x (+ round_phase 1))))
               (support (+ position_y (* velocity_y (+ (mod round_bias 3) 2)))))
           (ite (= (mod (+ team r lane) 3) 0)
                (+ motion support health lane)
                (+ motion (* support 2) team 17)))
         (+ team lane 23))))

(assert (>= lane 0))
(assert (<= lane 31))
(assert (not (= (contribution round) (contribution (+ round 1155)))))

(check-sat)
