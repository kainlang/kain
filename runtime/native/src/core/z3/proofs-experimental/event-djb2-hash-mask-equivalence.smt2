; Proof: DJB2 hash modulo 256 is equivalent to bitwise AND with 255
;
; The event bus uses DJB2 hashing with KAIN_EVENT_BUS_BUCKETS = 256:
;   return (unsigned int)(hash % KAIN_EVENT_BUS_BUCKETS);
;
; Since 256 is a power of two (2^8), the modulo operation is equivalent to:
;   return (unsigned int)(hash & (KAIN_EVENT_BUS_BUCKETS - 1));
;   = return (unsigned int)(hash & 255);
;
; This replaces a div instruction with a single AND — ~20-80 cycle savings
; per hash call on x86-64.
;
; The proof: for all 32-bit unsigned values, hash % 256 = hash & 255.
; We prove this by showing that no counterexample exists.

(set-logic QF_BV)

(declare-const hash (_ BitVec 32))

; Prove: hash % 256 == hash & 255 for all 32-bit values
; If we can't find a counterexample, the proof holds
(assert (not (= (bvurem hash (_ bv256 32)) (bvand hash (_ bv255 32)))))
(check-sat)
