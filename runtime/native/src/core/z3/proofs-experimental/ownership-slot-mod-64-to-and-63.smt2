; Proof: slot % 64 is equivalent to slot & 63 for occupancy bit index
;
; In ownership.c, the occupancy bit index is computed as:
;   ~(UINT64_C(1) << ((uint32_t)slot % KAIN_OWNERSHIP_WORD_BITS));
;   uint64_t bit = UINT64_C(1) << ((uint32_t)slot % KAIN_OWNERSHIP_WORD_BITS);
;
; KAIN_OWNERSHIP_WORD_BITS = 64 (a power of two)
;
; Since 64 is a power of two, slot % 64 can be replaced with slot & 63.
; This replaces a div instruction with a single AND.
;
; The proof: for all slot values in [0, 4095] (valid region range),
;   slot % 64 == slot & 63.
;
; Note: this holds for ALL 32-bit values, not just [0, 4095], because
; modulo a power of two is equivalent to bitwise AND with (divisor - 1)
; for all unsigned integers.

(set-logic QF_BV)

(declare-const slot (_ BitVec 32))

; Prove: slot % 64 == slot & 63 for all 32-bit values
(assert (not (= (bvurem slot (_ bv64 32)) (bvand slot (_ bv63 32)))))
(check-sat)
