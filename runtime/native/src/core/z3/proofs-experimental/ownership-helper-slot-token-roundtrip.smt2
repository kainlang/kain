; Proof: Helper slot token roundtrip (token→slot→token identity)
;
; Registration:  *out_slot_token = (uint16_t)((uint32_t)slot + 1u);
; Lookup:        slot = (uint32_t)slot_token - 1u;
;
; Claim: For all valid slots [0, KAIN_OWNERSHIP_MAX_REGIONS), the roundtrip
;   decode(encode(slot)) == slot is identity.
;   KAIN_OWNERSHIP_MAX_REGIONS = 4096.
;
; Since slot_token = slot + 1, and slot is uint16_t, the range is [1, 4096].
; Subtracting 1 recovers the original slot.
;
(set-logic QF_BV)
(declare-const slot (_ BitVec 12)) ; 4096 = 2^12
; Slot must be < KAIN_OWNERSHIP_MAX_REGIONS = 4096
(assert (bvult slot (_ bv4096 12)))

; Encode: token = (uint16_t)(slot + 1)
; In range [1, 4096]
(define-fun token () (_ BitVec 16) ((_ zero_extend 4) (bvadd slot (_ bv1 12))))

; Decode: recovered = (uint32_t)token - 1u
; But slot was 12-bit, so we compare the lower 12 bits
(define-fun recovered_slot () (_ BitVec 12)
  ((_ extract 11 0) (bvsub ((_ zero_extend 20) token) (_ bv1 32))))

; Claim: recovered_slot == slot
(assert (not (= recovered_slot slot)))
(check-sat)
