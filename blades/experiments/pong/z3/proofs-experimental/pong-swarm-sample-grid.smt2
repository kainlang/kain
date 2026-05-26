(set-logic QF_LIA)

(declare-const sample_count Int)
(declare-const index Int)
(declare-const frame_clock Int)
(declare-const ball_x Int)
(declare-const swarm_energy Int)

(define-fun orbit () Int
  (mod (+ (* index 17) (* frame_clock 5) ball_x swarm_energy) 464))

(assert (>= sample_count 32))
(assert (<= sample_count 512))
(assert (>= index 0))
(assert (< index sample_count))
(assert (>= frame_clock 0))
(assert (>= ball_x 0))
(assert (<= ball_x 886))
(assert (>= swarm_energy 0))

(assert
  (or
    (and
      (>= sample_count 32)
      (< sample_count 96)
      (or
        (< (+ 48 (div (* 804 (mod index 8)) 8)) 48)
        (> (+ 48 (div (* 804 (mod index 8)) 8)) 851)
        (< (+ 48 (mod (+ (* 11 (div index 8)) orbit) 464)) 48)
        (> (+ 48 (mod (+ (* 11 (div index 8)) orbit) 464)) 511)))
    (and
      (>= sample_count 96)
      (< sample_count 160)
      (or
        (< (+ 48 (div (* 804 (mod index 12)) 12)) 48)
        (> (+ 48 (div (* 804 (mod index 12)) 12)) 851)
        (< (+ 48 (mod (+ (* 11 (div index 12)) orbit) 464)) 48)
        (> (+ 48 (mod (+ (* 11 (div index 12)) orbit) 464)) 511)))
    (and
      (>= sample_count 160)
      (< sample_count 256)
      (or
        (< (+ 48 (div (* 804 (mod index 14)) 14)) 48)
        (> (+ 48 (div (* 804 (mod index 14)) 14)) 851)
        (< (+ 48 (mod (+ (* 11 (div index 14)) orbit) 464)) 48)
        (> (+ 48 (mod (+ (* 11 (div index 14)) orbit) 464)) 511)))
    (and
      (>= sample_count 256)
      (<= sample_count 512)
      (or
        (< (+ 48 (div (* 804 (mod index 16)) 16)) 48)
        (> (+ 48 (div (* 804 (mod index 16)) 16)) 851)
        (< (+ 48 (mod (+ (* 11 (div index 16)) orbit) 464)) 48)
        (> (+ 48 (mod (+ (* 11 (div index 16)) orbit) 464)) 511)))))

(check-sat)
