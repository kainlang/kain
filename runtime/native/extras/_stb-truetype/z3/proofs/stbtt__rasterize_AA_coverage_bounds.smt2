;; stbtt__rasterize_AA_coverage_bounds.smt2
;; Final pixel coverage is always clamped to [0, 255]
;;
;; In the v2 rasterizer, float coverage is converted to uint8:
;;   k = |coverage| * 255 + 0.5;
;;   m = (int)k;
;;   if (m > 255) m = 255;
;;   result->pixels[...] = (unsigned char) m;
;;
(set-logic QF_BV)
(set-info :status unsat)

;; ────────────────────────────────────────────────────────────────────────────
;; Claim 1: After clamp, m is always in [0, 255]
;;
;; m starts non-negative (|coverage| ≥ 0 ⇒ k ≥ 0.5 ⇒ m ≥ 0).
;; Clamp caps at 255. So m_final ∈ [0, 255].
;;─────────────────────────────────────────────────────────────────────────────
(push)
(declare-fun m () (_ BitVec 32))

;; m = (int)(|coverage|*255 + 0.5) ≥ 0
(assert (bvsge m #x00000000))

;; After clamp
(define-const m_final (_ BitVec 32)
  (ite (bvsgt m #x000000ff) #x000000ff m))

(assert (or (bvslt m_final #x00000000)
            (bvsgt m_final #x000000ff)))
(check-sat)
;; Expected: unsat — m_final is always in [0, 255]
(pop)

;; ────────────────────────────────────────────────────────────────────────────
;; Claim 2: uint8 cast is value-preserving for values in [0, 255]
;;
;; Any 32-bit value in [0, 255] has its 8-bit truncation equal to itself.
;;─────────────────────────────────────────────────────────────────────────────
(push)
(declare-fun v () (_ BitVec 32))

(assert (bvsge v #x00000000))
(assert (bvsle v #x000000ff))

(define-const v8 (_ BitVec 8) ((_ extract 7 0) v))
(define-const v_restored (_ BitVec 32) ((_ zero_extend 24) v8))

(assert (not (= v v_restored)))
(check-sat)
;; Expected: unsat — values in [0, 255] are uint8-preserved
(pop)

(exit)
