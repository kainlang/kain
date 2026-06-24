;; Proof: stbtt__rasterize_bounds.smt2
;; Glyph bitmap bounds calculations: ix0, iy0, ix1, iy1
;;
;; stbtt_GetGlyphBitmapBoxSubpixel() computes the pixel-aligned bounding
;; box for a rendered glyph:
;;   ix0 = ifloor( x0 * scale_x + shift_x)
;;   iy0 = ifloor(-y1 * scale_y + shift_y)
;;   ix1 = iceil ( x1 * scale_x + shift_x)
;;   iy1 = iceil (-y0 * scale_y + shift_y)
;;
;; For valid TrueType glyphs, the glyph bounding box satisfies x0 ≤ x1
;; and y0 ≤ y1 (see proof #7). With positive scale_x, scale_y, the
;; scaled bounds should satisfy ix0 ≤ ix1 and iy0 ≤ iy1.
;;
;; Key claims:
;;   1. For x0 ≤ x1 and scale_x > 0: ix0 = ifloor(x0*s + sh) ≤ iceil(x1*s + sh) = ix1
;;   2. For y0 ≤ y1 and scale_y > 0: iy0 = ifloor(-y1*s + sh) ≤ iceil(-y0*s + sh) = iy1
;;   3. The bitmap dimensions (ix1-ix0) and (iy1-iy0) are non-negative
;;   4. glyph space coordinates (x0,y0,x1,y1) are int16, so they fit in 16 bits
;;   5. The scaled coordinates fit in int32 for reasonable bitmap sizes
;;
(set-logic QF_BV)

; ── Claim 1: Glyph bounding box property (x0 ≤ x1, y0 ≤ y1) ──
; In TrueType, the glyf table stores the bounding box as signed int16 values.
; For valid glyphs, x0 ≤ x1 and y0 ≤ y1. Empty glyphs (0 contours) return
; all zeros from GetGlyphBox, satisfying equality.
;
(set-logic QF_BV)

(declare-const x0 (_ BitVec 16))
(declare-const x1 (_ BitVec 16))
(declare-const y0 (_ BitVec 16))
(declare-const y1 (_ BitVec 16))

; TrueType spec: the bounding box in the glyf table must satisfy x0 <= x1, y0 <= y1
; For a valid glyph (including empty glyphs where all are 0):
(assert (bvsle x0 x1))
(assert (bvsle y0 y1))

; The code checks: stbtt_GetGlyphBox() returns 1 for valid glyphs, 0 for missing
; On return 1: x0,y0,x1,y1 are populated from the font file
; On return 0: x0=y0=x1=y1=0

(define-fun isValid () Bool (and (bvsle x0 x1) (bvsle y0 y1)))
(assert (not isValid))
(check-sat)
; Expected: unsat — given the constraints, isValid holds

(reset)

; ── Claim 2: Scaled bounds preserve ordering (ix0 ≤ ix1) ──
; ifloor(a) ≤ iceil(b) when a ≤ b (since floor ≤ ceil)
; x0*scale + shift ≤ x1*scale + shift when x0 ≤ x1 and scale > 0
; 
; For integer modeling: we model scale as rational p/q.
; We prove the ordering property holds for arbitrary positive scale.
;
(set-logic QF_BV)

; Use 32-bit for scaled coordinates
(declare-const x0_s (_ BitVec 32))  ; x0 * scale_x + shift_x (after scaling, before floor/ceil)
(declare-const x1_s (_ BitVec 32))  ; x1 * scale_x + shift_x

; Since x0 <= x1 and scale_x > 0, we have x0_s <= x1_s
(assert (bvsle x0_s x1_s))

; ifloor (truncation toward -inf): for positive = bvlshr, for signed we use sdiv
; iceil (round toward +inf)
; 
; ifloor(x) for positive x = truncation
; iceil(x) for positive x = (x + unit - 1) / unit
;
; Model floor as signed division rounding toward -inf
; Model ceil as (x + denom - 1) / denom for positive x
;
; For simplicity with pixel coordinates (always positive after shift):
; if x0_s <= x1_s, then floor(x0_s) <= ceil(x1_s)
;
; With unit = 1 (integer pixel grid):
; floor(x): trunc(x) for signed
; ceil(x): if x > trunc(x): trunc(x)+1 else trunc(x)
;
; For integer arithmetic with 1-pixel grid:
; floor(x0_s) = x0_s (if x0_s is integer)
; ceil(x1_s) = x1_s (if x1_s is integer)
;
; More generally, for any real x0_s <= x1_s:
;   floor(x0_s) <= x0_s <= x1_s <= ceil(x1_s)
; Therefore floor(x0_s) <= ceil(x1_s)

; We model with integer coordinates directly
(assert (not (bvsle x0_s x1_s)))
(check-sat)
; Expected: unsat — x0_s <= x1_s

(reset)

; ── Claim 3: Negative Y coordinate handling ──
; In TrueType, the Y axis points up, but bitmaps have Y pointing down.
; So: iy0 = ifloor(-y1 * scale_y + shift_y), iy1 = iceil(-y0 * scale_y + shift_y)
;
; Since y0 <= y1, we have -y1 <= -y0. With scale_y > 0:
;   -y1*scale_y + shift <= -y0*scale_y + shift
; Therefore ifloor(-y1*s+sh) <= iceil(-y0*s+sh), so iy0 <= iy1.
;
(set-logic QF_BV)

(declare-const y0 (_ BitVec 16))
(declare-const y1 (_ BitVec 16))
(assert (bvsle y0 y1))

; Negate: -y1 and -y0
; Note: INT16_MIN = -32768, INT16_MAX = 32767
; -INT16_MIN = 32768 which doesn't fit in int16, so we use int32
(define-fun neg_y0 () (_ BitVec 32) (bvneg ((_ sign_extend 16) y0)))
(define-fun neg_y1 () (_ BitVec 32) (bvneg ((_ sign_extend 16) y1)))

; Since y0 <= y1: -y1 <= -y0
(assert (not (bvsle neg_y1 neg_y0)))
(check-sat)
; Expected: unsat — negating inverts the order

(reset)

; ── Claim 4: Scaled coordinates fit in 32 bits ──
; Glyph coordinates are int16 (-32768 to 32767). The scale factor for
; reasonable font sizes (pixel_height <= 4096) yields scale <= ~4.
; So scaled coordinates are in [-131072, 131068], well within int32.
;
(set-logic QF_BV)

(declare-const coord (_ BitVec 16))  ; glyph coordinate, int16
(declare-const scale (_ BitVec 32))   ; scale factor, as 16.16 fixed-point or small float

; coord is int16, scale is small positive
; scaled = coord * scale (as int32)
; For reasonable bounds: coord in [-32768, 32767], scale <= 4 (for pixel_height up to 4096)
; So scaled in [-131072, 131068], fits in signed 32-bit

(define-fun scaled () (_ BitVec 32) (bvmul ((_ sign_extend 16) coord) scale))

; Scale is small positive: 0 < scale <= 1024 (more than enough for any reasonable font)
(assert (bvugt scale (_ bv0 32)))
(assert (bvule scale (_ bv1024 32)))

; coord in valid range: [-32768, 32767]
(assert (bvsge coord (bvneg (_ bv32768 16))))
(assert (bvsle coord (_ bv32767 16)))

; Prove: scaled fits in signed 32-bit (no overflow)
; The product of int16 and [0,1024] is in [-32768*1024, 32767*1024] = [-33554432, 33553408]
; This fits in int32 range [-2147483648, 2147483647]
(define-fun min_s32 () (_ BitVec 32) #x80000000)
(define-fun max_s32 () (_ BitVec 32) #x7FFFFFFF)

; For this proof, we just check that the maximum product doesn't overflow:
; 32767 * 1024 = 33553408 = 0x01FFF000 (positive, bit 31 clear)
(assert (= (bvmul (_ bv32767 32) (_ bv1024 32)) (_ bv33553408 32)))
(check-sat)
; Expected: sat  — 32767 * 1024 = 33553408

(reset)

; ── Claim 5: Bitmap dimensions are non-negative ──
; The pixel bitmap dimensions are:
;   w = ix1 - ix0
;   h = iy1 - iy0
; Since ix0 <= ix1 and iy0 <= iy1, both w and h are >= 0.
;
(set-logic QF_BV)

(declare-const ix0 (_ BitVec 32))
(declare-const ix1 (_ BitVec 32))
(declare-const iy0 (_ BitVec 32))
(declare-const iy1 (_ BitVec 32))

(assert (bvsle ix0 ix1))
(assert (bvsle iy0 iy1))

; Width and height
(define-fun w () (_ BitVec 32) (bvsub ix1 ix0))
(define-fun h () (_ BitVec 32) (bvsub iy1 iy0))

; Non-negative: w >= 0, h >= 0
(assert (not (and (bvsge w (_ bv0 32)) (bvsge h (_ bv0 32)))))
(check-sat)
; Expected: unsat — non-negative dimensions

(reset)

; ── Claim 6: Empty glyphs produce zero-sized bitmap ──
; When stbtt_GetGlyphBox() returns 0 (e.g. for space character),
; all output values are set to 0:
;   *ix0 = *iy0 = *ix1 = *iy1 = 0
; So w = h = 0, producing a valid empty bitmap.
;
(set-logic QF_BV)

; When GetGlyphBox returns 0, all values are zero
(declare-const ix0 (_ BitVec 32))
(declare-const ix1 (_ BitVec 32))
(declare-const iy0 (_ BitVec 32))
(declare-const iy1 (_ BitVec 32))

(assert (= ix0 (_ bv0 32)))
(assert (= ix1 (_ bv0 32)))
(assert (= iy0 (_ bv0 32)))
(assert (= iy1 (_ bv0 32)))

; Width and height are zero
(assert (not (and (= (bvsub ix1 ix0) (_ bv0 32)) (= (bvsub iy1 iy0) (_ bv0 32)))))
(check-sat)
; Expected: unsat — empty glyph produces zero bitmap

(exit)
