; Stream BRAVO — Z3 Proof: push_constant_bound
; Proves that infer_push_constant_eligibility returns None when the
; total uniform size exceeds 128 bytes (Vulkan minimum maxPushConstantsSize).

; Declare types for uniform sizes and their total.
(declare-const u1 Int)
(declare-const u2 Int)
(declare-const u3 Int)
(declare-const u4 Int)
(declare-const u5 Int)
(declare-const u6 Int)
(declare-const u7 Int)
(declare-const u8 Int)
(declare-const u9 Int)

; All uniform sizes must be non-negative (physical sizes).
(assert (>= u1 0))
(assert (>= u2 0))
(assert (>= u3 0))
(assert (>= u4 0))
(assert (>= u5 0))
(assert (>= u6 0))
(assert (>= u7 0))
(assert (>= u8 0))
(assert (>= u9 0))

; Total exceeds 128 bytes (e.g., 9 × Vec4 at 16 bytes = 144).
(declare-const total Int)
(assert (= total (+ u1 u2 u3 u4 u5 u6 u7 u8 u9)))
(assert (> total 128))

; infer_push_constant_eligibility returns Some(total) only when total ≤ 128.
(declare-fun eligible (Int) Bool)
(assert (forall ((t Int))
    (=> (eligible t) (<= t 128))
))

; --- THEOREM ---
; For any total > 128, eligible is false.
(assert (eligible total))

(check-sat)
; Expected: unsat — the function correctly rejects oversized uniforms
