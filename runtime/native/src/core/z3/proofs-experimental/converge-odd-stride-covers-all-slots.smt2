; converge-odd-stride-covers-all-slots.smt2
;
; Claim: The hash probe stride in the converge tune cache always visits all
; 64 slots before repeating, because stride is always odd.
;
; In `abi_converge_select_lane_for_key`:
;   stride = ((key >> 6) | 1ull) & (KAIN_CONVERGE_TUNE_CACHE_CAP - 1u);
;
; KAIN_CONVERGE_TUNE_CACHE_CAP = 64 = 2^6. The stride computation:
; 1. key >> 6 shifts right by 6
; 2. | 1ull guarantees the LSB is set (odd)
; 3. & 63 masks to [0, 63]
;
; An odd stride modulo 64 is always coprime with 64 (since 64=2^6 and odd
; numbers share no factor 2). Therefore the sequence:
;   slot_i = (base + i * stride) mod 64
; visits all 64 slots before repeating (i = 0..63 produce distinct values).
;
; Proof: For any odd stride s (0 < s < 64, s & 1 == 1), the map
;   f(i) = (i * s) mod 64  for i in [0, 63]
; is a bijection. Equivalently, if a != b then (a * s) mod 64 != (b * s) mod 64.

(set-logic QF_BV)

; 6-bit values (0..63) to model modulo 64 arithmetic
(declare-const a (_ BitVec 6))
(declare-const b (_ BitVec 6))
(declare-const stride (_ BitVec 6))

; a and b are distinct
(assert (not (= a b)))

; stride is odd (LSB = 1) and non-zero
(assert (= ((_ extract 0 0) stride) #b1))
(assert (not (= stride #b000000)))

; Negate injectivity: a*stride mod 64 == b*stride mod 64
; In 6-bit BV multiplication, overflow wraps modulo 64 naturally.
(assert (= (bvmul a stride) (bvmul b stride)))

(check-sat)
; unsat = stride is invertible modulo 64 → all 64 slots are visited
; sat = counterexample: stride is not coprime with 64 (shouldn't happen for odd stride)
