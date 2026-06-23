; Proof: RGBA color packing into uint32_t produces non-overlapping bit positions
;
; The functions:
;   uint32_t ui_parse_color_hex() and others produce colors as:
;     return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
;
; Component accessors:
;   uint8_t ui_color_r(uint32_t color) { return (uint8_t)((color >> 16) & 0xFF); }
;   uint8_t ui_color_g(uint32_t color) { return (uint8_t)((color >> 8)  & 0xFF); }
;   uint8_t ui_color_b(uint32_t color) { return (uint8_t)((color)       & 0xFF); }
;   uint8_t ui_color_a(uint32_t color) { return (uint8_t)((color >> 24) & 0xFF); }
;
; Key claims:
;   1. Each component occupies unique, non-overlapping 8-bit lanes:
;      A = bits 24-31, R = bits 16-23, G = bits 8-15, B = bits 0-7
;   2. Packing is invertible: packing then extracting gives back the original component values
;   3. Components from different fields cannot alias each other due to distinct bit ranges
;
(set-logic QF_BV)

; ── Claim 1: Non-overlapping bit positions ──
; For any r, g, b, a in [0, 255], the packed value's bits come from
; exactly one source.

(declare-const a (_ BitVec 32))
(declare-const r (_ BitVec 32))
(declare-const g (_ BitVec 32))
(declare-const b (_ BitVec 32))

(assert (bvule a (_ bv255 32)))
(assert (bvule r (_ bv255 32)))
(assert (bvule g (_ bv255 32)))
(assert (bvule b (_ bv255 32)))

(define-fun packed () (_ BitVec 32)
  (bvor (bvshl a (_ bv24 32))
    (bvor (bvshl r (_ bv16 32))
      (bvor (bvshl g (_ bv8 32)) b))))

; Extract each component from packed value and verify it matches original
(define-fun a_out () (_ BitVec 8) ((_ extract 31 24) packed))
(define-fun r_out () (_ BitVec 8) ((_ extract 23 16) packed))
(define-fun g_out () (_ BitVec 8) ((_ extract 15 8) packed))
(define-fun b_out () (_ BitVec 8) ((_ extract 7 0) packed))

; Assert that extraction recovers the original values
(assert (not (and
  (= a_out ((_ extract 7 0) a))
  (= r_out ((_ extract 7 0) r))
  (= g_out ((_ extract 7 0) g))
  (= b_out ((_ extract 7 0) b))
)))

(check-sat)
; Expected: unsat — all component values are recoverable from packed form

(reset)

; ── Claim 2: No cross-contamination between component lanes ──
; Changing one component does not affect the extracted value of another component.
(set-logic QF_BV)

(declare-const a1 (_ BitVec 32))
(declare-const a2 (_ BitVec 32))
(declare-const r (_ BitVec 32))
(declare-const g (_ BitVec 32))
(declare-const b (_ BitVec 32))

(assert (bvule a1 (_ bv255 32)))
(assert (bvule a2 (_ bv255 32)))
(assert (bvule r (_ bv255 32)))
(assert (bvule g (_ bv255 32)))
(assert (bvule b (_ bv255 32)))

; Different alpha values
(assert (not (= a1 a2)))

(define-fun pack1 () (_ BitVec 32)
  (bvor (bvshl a1 (_ bv24 32))
    (bvor (bvshl r (_ bv16 32))
      (bvor (bvshl g (_ bv8 32)) b))))
(define-fun pack2 () (_ BitVec 32)
  (bvor (bvshl a2 (_ bv24 32))
    (bvor (bvshl r (_ bv16 32))
      (bvor (bvshl g (_ bv8 32)) b))))

; Extract non-alpha components — they should be IDENTICAL even though a changed
(define-fun r1 () (_ BitVec 8) ((_ extract 23 16) pack1))
(define-fun r2 () (_ BitVec 8) ((_ extract 23 16) pack2))
(define-fun g1 () (_ BitVec 8) ((_ extract 15 8) pack1))
(define-fun g2 () (_ BitVec 8) ((_ extract 15 8) pack2))
(define-fun b1 () (_ BitVec 8) ((_ extract 7 0) pack1))
(define-fun b2 () (_ BitVec 8) ((_ extract 7 0) pack2))

; They must all be equal (changing alpha does not affect other channels)
(assert (not (and (= r1 r2) (= g1 g2) (= b1 b2))))

(check-sat)
; Expected: unsat — modifying alpha does not affect R, G, or B extraction

(reset)

; ── Claim 3: Each lane is exactly 8 bits wide and non-overlapping ──
; A mask test: verify that each field's mask only captures its own bits.
(set-logic QF_BV)

; Alpha mask: 0xFF000000
; Red mask:   0x00FF0000
; Green mask: 0x0000FF00
; Blue mask:  0x000000FF

; Verify masks are disjoint
(define-fun a_mask () (_ BitVec 32) (_ bv4278190080 32))  ; 0xFF000000
(define-fun r_mask () (_ BitVec 32) (_ bv16711680 32))    ; 0x00FF0000
(define-fun g_mask () (_ BitVec 32) (_ bv65280 32))       ; 0x0000FF00
(define-fun b_mask () (_ BitVec 32) (_ bv255 32))         ; 0x000000FF

; No two masks share any bits
(assert (not (= (bvand a_mask r_mask) (_ bv0 32))))
(check-sat)
; Expected: unsat — alpha mask and red mask are disjoint

(reset)

(set-logic QF_BV)
(define-fun a_mask () (_ BitVec 32) (_ bv4278190080 32))
(define-fun r_mask () (_ BitVec 32) (_ bv16711680 32))
(define-fun g_mask () (_ BitVec 32) (_ bv65280 32))
(define-fun b_mask () (_ BitVec 32) (_ bv255 32))

(assert (not (= (bvand r_mask g_mask) (_ bv0 32))))
(check-sat)
; Expected: unsat — red mask and green mask are disjoint

(reset)

(set-logic QF_BV)
(define-fun a_mask () (_ BitVec 32) (_ bv4278190080 32))
(define-fun r_mask () (_ BitVec 32) (_ bv16711680 32))
(define-fun g_mask () (_ BitVec 32) (_ bv65280 32))
(define-fun b_mask () (_ BitVec 32) (_ bv255 32))

(assert (not (= (bvand g_mask b_mask) (_ bv0 32))))
(check-sat)
; Expected: unsat — green mask and blue mask are disjoint

(reset)

(set-logic QF_BV)
(define-fun a_mask () (_ BitVec 32) (_ bv4278190080 32))
(define-fun r_mask () (_ BitVec 32) (_ bv16711680 32))
(define-fun g_mask () (_ BitVec 32) (_ bv65280 32))
(define-fun b_mask () (_ BitVec 32) (_ bv255 32))

(assert (not (= (bvand a_mask b_mask) (_ bv0 32))))
(check-sat)
; Expected: unsat — alpha mask and blue mask are disjoint

(reset)

; ── Claim 4: OR-ing all masks covers all 32 bits exactly ──
(set-logic QF_BV)

(define-fun a_mask () (_ BitVec 32) (_ bv4278190080 32))
(define-fun r_mask () (_ BitVec 32) (_ bv16711680 32))
(define-fun g_mask () (_ BitVec 32) (_ bv65280 32))
(define-fun b_mask () (_ BitVec 32) (_ bv255 32))

(define-fun all_masks () (_ BitVec 32) (bvor (bvor a_mask r_mask) (bvor g_mask b_mask)))

; all_masks should equal 0xFFFFFFFF (all 32 bits set)
(assert (not (= all_masks (_ bv4294967295 32))))
(check-sat)
; Expected: unsat — combined masks equal 0xFFFFFFFF (cover all 32 bits)
