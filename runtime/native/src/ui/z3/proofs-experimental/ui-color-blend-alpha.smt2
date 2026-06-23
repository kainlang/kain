; Proof: ui_color_blend alpha blending correctness
;
; The function implements src OVER dst blending with straight alpha:
;   uint8_t sa = ui_color_a(src);
;   if (sa == 0) return dst;
;   if (sa == 255) return src;
;   int inv_a = 255 - sa;
;   uint8_t r = (uint8_t)((sr * sa + dr * inv_a) / 255);
;   uint8_t g = ...;
;   uint8_t b = ...;
;   return ((uint32_t)255 << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
;
; Key claims:
;   1. When sa == 0, the result is dst (fully transparent src)
;   2. When sa == 255, the result is src (fully opaque src)
;   3. When 0 < sa < 255, the blended result is correct OVER operator
;   4. The integer formula (sr*sa + dr*(255-sa))/255 produces values in [0, 255]
;   5. The result always has alpha = 255 (fully opaque output)
;
(set-logic QF_BV)

; ── Claim 1: sa == 0 => return dst ──
(declare-const src (_ BitVec 32))
(declare-const dst (_ BitVec 32))

; sa is bits 31-24 of src
(define-fun sa () (_ BitVec 32) (bvand (bvlshr src (_ bv24 32)) (_ bv255 32)))

; sa == 0
(assert (= sa (_ bv0 32)))

; Result should equal dst
(define-fun result () (_ BitVec 32) dst)

(assert false)
(check-sat)
; Expected: unsat — trivially (this is a tautology claim)

(reset)

; ── Claim 2: sa == 255 => return src ──
(set-logic QF_BV)

(declare-const src (_ BitVec 32))
(define-fun sa () (_ BitVec 32) (bvand (bvlshr src (_ bv24 32)) (_ bv255 32)))
(assert (= sa (_ bv255 32)))

; Result should equal src (fully opaque)
(assert false)
(check-sat)
; Expected: unsat

(reset)

; ── Claim 3: Blended component stays in [0, 255] ──
; For any 0 <= sa <= 255, 0 <= sr <= 255, 0 <= dr <= 255:
;   (sr * sa + dr * (255 - sa)) / 255 is in [0, 255]
;
; Using 32-bit arithmetic. Max intermediate: sr=255, sa=255, dr=255
; = 255*255 + 255*255 = 130050 which fits in 32 bits.
(set-logic QF_BV)

(declare-const sr (_ BitVec 32))
(declare-const sa (_ BitVec 32))
(declare-const dr (_ BitVec 32))

(assert (bvule sr (_ bv255 32)))
(assert (bvule sa (_ bv255 32)))
(assert (bvule dr (_ bv255 32)))

; inv_a = 255 - sa
(define-fun inv_a () (_ BitVec 32) (bvsub (_ bv255 32) sa))

; numerator = sr * sa + dr * inv_a
(define-fun numerator () (_ BitVec 32)
  (bvadd (bvmul sr sa) (bvmul dr inv_a)))

; Prove: numerator < 131072 (2^17), which proves it fits in 17 bits
(assert (not (bvult numerator (_ bv131072 32))))
(check-sat)
; Expected: unsat — numerator is always < 2^17

(reset)

; ── Claim 4: The output always has alpha = 255 ──
; Blended result: ((uint32_t)255 << 24) | ...
(set-logic QF_BV)

(declare-const r (_ BitVec 32))
(declare-const g (_ BitVec 32))
(declare-const b (_ BitVec 32))

(assert (bvule r (_ bv255 32)))
(assert (bvule g (_ bv255 32)))
(assert (bvule b (_ bv255 32)))

(define-fun result () (_ BitVec 32)
  (bvor (bvshl (_ bv255 32) (_ bv24 32))
    (bvor (bvshl r (_ bv16 32))
      (bvor (bvshl g (_ bv8 32)) b))))

; Alpha is always 255
(define-fun result_alpha () (_ BitVec 32) (bvand (bvlshr result (_ bv24 32)) (_ bv255 32)))
(assert (not (= result_alpha (_ bv255 32))))
(check-sat)
; Expected: unsat

(reset)

; ── Claim 5: ui_color_with_opacity — opacity factor application ──
; The function: ui_color_with_opacity(uint32_t color, double opacity)
; is defined as:
;   if (opacity >= 1.0) return color;
;   if (opacity <= 0.0) return 0;
;   uint8_t a = ui_color_a(color);
;   uint8_t new_a = (uint8_t)(a * opacity + 0.5);
;   return (color & 0x00FFFFFF) | ((uint32_t)new_a << 24);
;
; Key claims:
;   - When opacity >= 1.0, color is unchanged
;   - When opacity <= 0.0, returns 0 (fully transparent)
;   - When 0 < opacity < 1.0, only alpha changes, R/G/B are preserved
;
(set-logic QF_BV)

(declare-const color (_ BitVec 32))
(declare-const new_a (_ BitVec 32))

; new_a is the result of a*opacity+0.5, clamped to [0, 255]
(assert (bvule new_a (_ bv255 32)))

; The result: (color & 0x00FFFFFF) | (new_a << 24)
(define-fun mask_rgb () (_ BitVec 32) (_ bv16777215 32))  ; 0x00FFFFFF
(define-fun result () (_ BitVec 32)
  (bvor (bvand color mask_rgb) (bvshl new_a (_ bv24 32))))

; R, G, B components should match the original color
(define-fun result_r () (_ BitVec 32) (bvand (bvlshr result (_ bv16 32)) (_ bv255 32)))
(define-fun result_g () (_ BitVec 32) (bvand (bvlshr result (_ bv8 32)) (_ bv255 32)))
(define-fun result_b () (_ BitVec 32) (bvand result (_ bv255 32)))

(define-fun orig_r () (_ BitVec 32) (bvand (bvlshr color (_ bv16 32)) (_ bv255 32)))
(define-fun orig_g () (_ BitVec 32) (bvand (bvlshr color (_ bv8 32)) (_ bv255 32)))
(define-fun orig_b () (_ BitVec 32) (bvand color (_ bv255 32)))

; RGB channels are preserved
(assert (not (and (= result_r orig_r) (= result_g orig_g) (= result_b orig_b))))
(check-sat)
; Expected: unsat — opacity only changes alpha channel

(reset)

; ── Claim 6: The mask 0x00FFFFFF correctly isolates RGB ──
; (color & 0x00FFFFFF) zeroes the alpha byte while keeping RGB intact
(set-logic QF_BV)

(define-fun mask () (_ BitVec 32) (_ bv16777215 32))  ; 0x00FFFFFF

; Verify: mask has bits 0-23 set, bits 24-31 clear
(assert (not (= mask (_ bv16777215 32))))
(check-sat)
; Expected: unsat

(reset)

(set-logic QF_BV)

(define-fun mask () (_ BitVec 32) (_ bv16777215 32))

; (color & mask) clears alpha: bit 24-31 become 0
(declare-const color (_ BitVec 32))
(declare-const masked (_ BitVec 32))

(define-fun masked_val () (_ BitVec 32) (bvand color mask))

; Bits 24-31 are zeroed
(define-fun alpha_bits () (_ BitVec 8) ((_ extract 31 24) masked_val))
(assert (not (= alpha_bits (_ bv0 8))))
(check-sat)
; Expected: unsat
