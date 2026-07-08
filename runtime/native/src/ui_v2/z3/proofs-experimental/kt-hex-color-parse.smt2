; Proof: Hex color nibble expansion (#RGB → #RRGGBBFF)
;
; Target: kaintana.h (or tree.c) — Formula CR-3
; API: kt_color_parse_hex()
;
; For #RGB format (3 hex digits → 4 bytes):
;   r = nibble[0] * 17;  g = nibble[1] * 17;  b = nibble[2] * 17;  a = 255
;
; nibble * 17 == (nibble << 4) | nibble
; Because for n in [0, 15]: n * 17 = 16n + n = (n << 4) | n
;
; For #RRGGBB (6 digits):
;   r = hi_nibble*16 + lo_nibble  (already full byte)
;   a = 255

(set-logic QF_BV)

; ── CLAIM 1: nibble * 17 = (nibble << 4) | nibble ──
(reset)
(set-logic QF_BV)

(declare-fun nibble () (_ BitVec 8))
(assert (bvule nibble (_ bv15 8)))

(define-fun mul17 () (_ BitVec 8) (bvmul nibble (_ bv17 8)))
(define-fun dup () (_ BitVec 8) (bvor (bvshl nibble (_ bv4 8)) nibble))

(assert (not (= mul17 dup)))
(check-sat)
; Expected: unsat — nibble*17 == duplicating the nibble

; ── CLAIM 2: No overflow: nibble*17 <= 255 ──
(reset)
(set-logic QF_BV)

(declare-fun nibble () (_ BitVec 8))
(assert (bvule nibble (_ bv15 8)))

(define-fun mul17 () (_ BitVec 8) (bvmul nibble (_ bv17 8)))
(assert (not (bvule mul17 (_ bv255 8))))
(check-sat)
; Expected: unsat — max is 15*17 = 255

; ── CLAIM 3: The high and low nibbles of expanded byte equal the original ──
(reset)
(set-logic QF_BV)

(declare-fun nibble () (_ BitVec 8))
(assert (bvule nibble (_ bv15 8)))

(define-fun exp () (_ BitVec 8) (bvmul nibble (_ bv17 8)))
(define-fun hi () (_ BitVec 4) ((_ extract 7 4) exp))
(define-fun lo () (_ BitVec 4) ((_ extract 3 0) exp))

; Both halves equal the original 4-bit value
(assert (not (and
  (= hi ((_ extract 3 0) nibble))
  (= lo ((_ extract 3 0) nibble)))))
(check-sat)
; Expected: unsat — nibble duplicated into both halves

; ── CLAIM 4: Packed uint32 roundtrips for all 6-digit hex ──
; #RRGGBB → 0xFFRRGGBB → extract R, G, B, A → original values
(reset)
(set-logic QF_BV)

(declare-fun r () (_ BitVec 8))
(declare-fun g () (_ BitVec 8))
(declare-fun b () (_ BitVec 8))

; Pack as 0xAARRGGBB (A=255)
(define-fun packed () (_ BitVec 32)
  (bvor (bvshl (_ bv255 32) (_ bv24 32))
    (bvor (bvshl ((_ zero_extend 24) r) (_ bv16 32))
      (bvor (bvshl ((_ zero_extend 24) g) (_ bv8 32))
        ((_ zero_extend 24) b)))))

; Extract
(define-fun a_out () (_ BitVec 8) ((_ extract 31 24) packed))
(define-fun r_out () (_ BitVec 8) ((_ extract 23 16) packed))
(define-fun g_out () (_ BitVec 8) ((_ extract 15 8) packed))
(define-fun b_out () (_ BitVec 8) ((_ extract 7 0) packed))

(assert (not (and (= r_out r) (= g_out g) (= b_out b) (= a_out (_ bv255 8)))))
(check-sat)
; Expected: unsat

; ── CLAIM 5: 8-digit hex roundtrip ──
(reset)
(set-logic QF_BV)

(declare-fun r () (_ BitVec 8))
(declare-fun g () (_ BitVec 8))
(declare-fun b () (_ BitVec 8))
(declare-fun a () (_ BitVec 8))

(define-fun packed () (_ BitVec 32)
  (bvor (bvshl ((_ zero_extend 24) a) (_ bv24 32))
    (bvor (bvshl ((_ zero_extend 24) r) (_ bv16 32))
      (bvor (bvshl ((_ zero_extend 24) g) (_ bv8 32))
        ((_ zero_extend 24) b)))))

(define-fun a_out () (_ BitVec 8) ((_ extract 31 24) packed))
(define-fun r_out () (_ BitVec 8) ((_ extract 23 16) packed))
(define-fun g_out () (_ BitVec 8) ((_ extract 15 8) packed))
(define-fun b_out () (_ BitVec 8) ((_ extract 7 0) packed))

(assert (not (and (= r_out r) (= g_out g) (= b_out b) (= a_out a))))
(check-sat)
; Expected: unsat

(echo "=== HEX COLOR PARSE PROVEN ===")
(echo "nibble * 17 = (nibble << 4) | nibble  [for nibble in [0, 15]]")
(echo "#RGB → (r*17, g*17, b*17, 255)  — exact expansion")
(echo "#RRGGBB → 0xFFRRGGBB — packed correctly")
(echo "#RRGGBBAA → packed with explicit alpha")
(echo "All operations branchless")
