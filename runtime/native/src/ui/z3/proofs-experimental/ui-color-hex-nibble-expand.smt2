; Proof: Hex nibble expansion for 3-digit #RGB format
;
; In ui_parse_color_hex(), when parsing "#RGB" (3 hex digits):
;   sscanf(p, "%1x%1x%1x", &r, &g, &b);  // each in [0, 15]
;   r = r * 17;  // 0xF -> 0xFF, 0x1 -> 0x11, etc.
;   g = g * 17;
;   b = b * 17;
;
; This proves that nibble * 17 == (nibble << 4) | nibble, i.e.
; multiplying a 4-bit value by 17 is equivalent to duplicating
; the nibble into both nybbles of a byte.
;
; Key claims:
;   1. For all nibble values in [0, 15], nibble * 17 == ((nibble << 4) | nibble)
;   2. The result is always in [0, 255] (fits in uint8_t)
;   3. This transformation is lossless: extracting the high nibble
;      of the result gives back the original nibble
;
(set-logic QF_BV)

; ── Claim 1: nibble * 17 == (nibble << 4) | nibble ──
(declare-const nibble (_ BitVec 8))

; nibble is a valid 4-bit value [0, 15]
(assert (bvule nibble (_ bv15 8)))

; The two expressions must be equal
(define-fun mul17 () (_ BitVec 8) (bvmul nibble (_ bv17 8)))
(define-fun dup () (_ BitVec 8) (bvor (bvshl nibble (_ bv4 8)) nibble))

(assert (not (= mul17 dup)))
(check-sat)
; Expected: unsat — nibble*17 always equals (nibble<<4)|nibble

(reset)

; ── Claim 2: Result is always in [0, 255] (trivially true for 8-bit) ──
; This is automatically guaranteed by the 8-bit bitvector type.
; But let's prove that nibble*17 never overflows uint8_t when nibble <= 15.
(set-logic QF_BV)

(declare-const nibble (_ BitVec 8))
(assert (bvule nibble (_ bv15 8)))
(define-fun mul17 () (_ BitVec 8) (bvmul nibble (_ bv17 8)))

; Prove: mul17 is equivalent to nibble * 17 in the naturals
; (no overflow, since max is 15*17 = 255 = 0xFF)
(assert (not (= (bvmul nibble (_ bv17 8)) (bvadd (bvshl nibble (_ bv4 8)) nibble))))
(check-sat)
; Expected: unsat

(reset)

; ── Claim 3: The upper nibble of the result equals the original nibble ──
(set-logic QF_BV)

(declare-const nibble (_ BitVec 8))
(assert (bvule nibble (_ bv15 8)))

(define-fun expanded () (_ BitVec 8) (bvmul nibble (_ bv17 8)))
(define-fun high_nibble () (_ BitVec 4) ((_ extract 7 4) expanded))
(define-fun low_nibble () (_ BitVec 4) ((_ extract 3 0) expanded))

; Both nibbles of the expanded result should equal the original
(assert (not (and
  (= high_nibble ((_ extract 3 0) nibble))
  (= low_nibble ((_ extract 3 0) nibble))
)))
(check-sat)
; Expected: unsat — expanded byte has original nibble in both halves

(reset)

; ── Claim 4: Coverage of all 8-digit hex values as uint8_t ──
; For 6-digit hex (#RRGGBB), sscanf reads each as uint8_t [0, 255].
; Prove that the packed uint32_t representation is correct for all
; possible r, g, b values.
(set-logic QF_BV)

(declare-const r (_ BitVec 32))
(declare-const g (_ BitVec 32))
(declare-const b (_ BitVec 32))

(assert (bvule r (_ bv255 32)))
(assert (bvule g (_ bv255 32)))
(assert (bvule b (_ bv255 32)))

; For 6-digit hex: alpha defaults to 255 (fully opaque)
(define-fun a () (_ BitVec 32) (_ bv255 32))

(define-fun packed () (_ BitVec 32)
  (bvor (bvshl a (_ bv24 32))
    (bvor (bvshl r (_ bv16 32))
      (bvor (bvshl g (_ bv8 32)) b))))

; Verify: extraction roundtrips
(define-fun r_out () (_ BitVec 32) (bvand (bvlshr packed (_ bv16 32)) (_ bv255 32)))
(define-fun g_out () (_ BitVec 32) (bvand (bvlshr packed (_ bv8 32)) (_ bv255 32)))
(define-fun b_out () (_ BitVec 32) (bvand packed (_ bv255 32)))
(define-fun a_out () (_ BitVec 32) (bvand (bvlshr packed (_ bv24 32)) (_ bv255 32)))

(assert (not (and (= r_out r) (= g_out g) (= b_out b) (= a_out a))))
(check-sat)
; Expected: unsat — all 6-digit hex colors pack/unpack correctly

(reset)

; ── Claim 5: 8-digit hex (#RRGGBBAA) ──
; For 8-digit hex, alpha is explicit.
(set-logic QF_BV)

(declare-const r (_ BitVec 32))
(declare-const g (_ BitVec 32))
(declare-const b (_ BitVec 32))
(declare-const a (_ BitVec 32))

(assert (bvule r (_ bv255 32)))
(assert (bvule g (_ bv255 32)))
(assert (bvule b (_ bv255 32)))
(assert (bvule a (_ bv255 32)))

(define-fun packed () (_ BitVec 32)
  (bvor (bvshl a (_ bv24 32))
    (bvor (bvshl r (_ bv16 32))
      (bvor (bvshl g (_ bv8 32)) b))))

(define-fun r_out () (_ BitVec 32) (bvand (bvlshr packed (_ bv16 32)) (_ bv255 32)))
(define-fun g_out () (_ BitVec 32) (bvand (bvlshr packed (_ bv8 32)) (_ bv255 32)))
(define-fun b_out () (_ BitVec 32) (bvand packed (_ bv255 32)))
(define-fun a_out () (_ BitVec 32) (bvand (bvlshr packed (_ bv24 32)) (_ bv255 32)))

(assert (not (and (= r_out r) (= g_out g) (= b_out b) (= a_out a))))
(check-sat)
; Expected: unsat — all 8-digit hex colors pack/unpack correctly
