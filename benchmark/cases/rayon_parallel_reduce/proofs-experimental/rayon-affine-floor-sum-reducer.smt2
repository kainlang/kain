; Exploratory proof for the rayon_parallel_reduce affine floor-sum reducer.
; Closed domain: ITERATIONS = 4000000, CHUNK = 8, lane(i) =
; ((i * 31) + (i / 8)) mod 1000003, MODULUS = 1000000007.
; The Kain LLVM lane preserves the scalar converge spec and folds i = 8*q + r:
; lane = (249*q + 31*r) mod 1000003.
(set-logic QF_NIA)

(define-fun iterations () Int 4000000)
(define-fun chunk () Int 8)
(define-fun q_count () Int 500000)
(define-fun lane_modulus () Int 1000003)
(define-fun modulus () Int 1000000007)
(define-fun affine_step () Int 249)
(define-fun residue_step () Int 31)
(define-fun residue_sum () Int 28)
(define-fun q_sum () Int 124999750000)
(define-fun floor_sum_r0 () Int 30875345)
(define-fun floor_sum_r1 () Int 30875361)
(define-fun floor_sum_r2 () Int 30875378)
(define-fun floor_sum_r3 () Int 30875394)
(define-fun floor_sum_r4 () Int 30875410)
(define-fun floor_sum_r5 () Int 30875425)
(define-fun floor_sum_r6 () Int 30875440)
(define-fun floor_sum_r7 () Int 30875454)
(define-fun floor_sum_total () Int
  (+ floor_sum_r0 floor_sum_r1 floor_sum_r2 floor_sum_r3
     floor_sum_r4 floor_sum_r5 floor_sum_r6 floor_sum_r7))
(define-fun affine_sum () Int
  (+ (* chunk affine_step q_sum) (* residue_step residue_sum q_count)))
(define-fun wrapped_sum () Int (- affine_sum (* lane_modulus floor_sum_total)))
(define-fun folded_acc () Int (mod wrapped_sum modulus))

(push)
; Inverted claim: 4000000 does not split into 500000 complete eight-lane chunks.
(assert (not (= (div iterations chunk) q_count)))
(check-sat)
(pop)

(push)
; Inverted claim: q_count * (q_count - 1) / 2 is not the closed q sum.
(assert (not (= (div (* q_count (- q_count 1)) 2) q_sum)))
(check-sat)
(pop)

(push)
; Inverted derivation: i = 8*q + r does not reduce the lane numerator to
; 249*q + 31*r for some benchmark-domain q/r.
(declare-const q Int)
(declare-const r Int)
(assert (and (>= q 0) (< q q_count) (>= r 0) (< r chunk)))
(define-fun i () Int (+ (* chunk q) r))
(assert
  (not
    (= (mod (+ (* i residue_step) (div i chunk)) lane_modulus)
       (mod (+ (* affine_step q) (* residue_step r)) lane_modulus))))
(check-sat)
(pop)

(push)
; Inverted segment invariant: if next is the first q whose floor level can
; increase, no q in [start, next) may have a different floor level.
(declare-const start Int)
(declare-const residue Int)
(declare-const witness_q Int)
(assert (and (>= start 0) (< start q_count) (>= residue 0) (< residue chunk)))
(define-fun bias () Int (* residue_step residue))
(define-fun level () Int (div (+ (* affine_step start) bias) lane_modulus))
(define-fun next_q () Int
  (div (+ (- (* (+ level 1) lane_modulus) bias) (- affine_step 1)) affine_step))
(assert (and (>= witness_q start) (< witness_q next_q) (< witness_q q_count)))
(assert (not (= (div (+ (* affine_step witness_q) bias) lane_modulus) level)))
(check-sat)
(pop)

(push)
; Inverted arithmetic claim: the eight residue floor sums do not total the
; benchmark wrap count discovered by the segment reducer.
(assert (not (= floor_sum_total 247003207)))
(check-sat)
(pop)

(push)
; Inverted final checksum claim for the folded reduction.
(assert (not (= folded_acc 987976414)))
(check-sat)
(pop)
