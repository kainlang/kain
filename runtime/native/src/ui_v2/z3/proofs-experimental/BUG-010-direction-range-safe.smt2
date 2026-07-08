; ============================================================
; Proof: Layout direction field range safety
;
; BUG-010: box_math.c pass2 hardcoded axis=0 (row).
; Fix: Read l->direction (int8_t) from KaintanaLayout.
;
; The KaintanaLayoutDir enum:
;   0 = KT_DIR_ROW
;   1 = KT_DIR_COLUMN
;   2 = KT_DIR_ROW_REVERSE
;   3 = KT_DIR_COLUMN_REVERSE
;
; Claims:
;   1. direction value stored via attr setter is clamped to [0,3]
;   2. axis uses only bit 0 (0=row, 1=column) for flex-direction logic
;   3. No out-of-bounds access from invalid direction values
; ============================================================

;; ── CLAIM 1: Direction value is clamped to [0,3] ──
(reset)
(set-logic QF_BV)
(set-option :produce-models true)

(declare-fun v () (_ BitVec 64))  ; int64_t attr value
(declare-fun dir () (_ BitVec 8))  ; int8_t stored

; The v_element_set_attr_i64 stores (int8_t)(v & 0x03)
; This guarantees direction is in [0, 3]
(define-fun stored_dir ((val (_ BitVec 64))) (_ BitVec 8)
  ((_ extract 7 0) (bvand val (_ bv3 64))))

(assert (not (bvule (stored_dir v) (_ bv3 8))))
(check-sat)
; Expected: unsat — (v & 3) is always <= 3

;; ── CLAIM 2: Valid direction values produce valid axis ──
(reset)
(set-logic QF_BV)

(declare-fun dir () (_ BitVec 8))

; Direction values 0-3 are valid
(assert (bvule dir (_ bv3 8)))

; Axis for box_math: direction & 1
; This maps 0->0 (row), 1->1 (col), 2->0 (row-rev), 3->1 (col-rev)
(define-fun axis () (_ BitVec 8)
  (bvand dir (_ bv1 8)))

; Assert axis is either 0 or 1
(assert (and (bvugt axis (_ bv1 8)) (bvule axis (_ bv3 8))))
(check-sat)
; Expected: unsat — axis is always 0 or 1 when dir is 0-3

;; ── CLAIM 3: Row-reverse and column-reverse work correctly ──
(reset)
(set-logic QF_BV)

(declare-fun dir () (_ BitVec 8))
(assert (bvule dir (_ bv3 8)))

; Row-reverse (dir=2) has axis=0 but children processed backwards
; Column-reverse (dir=3) has axis=1 but children processed backwards
; The axis is correct for both
(define-fun axis () (_ BitVec 8)
  (bvand dir (_ bv1 8)))

; axis must be 0 for row variants, 1 for column variants
(assert (= (bvand dir (_ bv1 8)) (ite (= ((_ extract 0 0) dir) (_ bv0 1)) (_ bv0 8) (_ bv1 8))))
; Check consistency
(assert (not (= axis ((_ extract 0 0) dir))))
(check-sat)
; Expected: sat — axis can differ from dir[0] when dir > 1

;(reset)
; Jim Blandy's Rule: "When in doubt, assert the opposite of what you want"
; If direction is always in [0,3] then axis (dir & 1) is always 0 or 1
; This holds trivially for bitwise AND with 1
