; Proof: abi_ui_index_start_slot_u64 always produces index in [0, mask]
;
; The function (line ~90):
;   static uint32_t abi_ui_index_start_slot_u64(uint64_t hash, uint32_t mask) {
;       return (uint32_t)(hash & mask);
;   }
;
; Since mask is always a power-of-two-minus-one (e.g., ABI_UI_NODE_INDEX_MASK = 4095,
; ABI_UI_STYLE_INDEX_MASK = 8191, etc.), the AND operation guarantees the result
; fits within [0, mask].
;
; Key claims:
;   1. For any hash and any mask that is power-of-two-minus-one (i.e., mask = 2^k - 1),
;      output = hash & mask is always <= mask
;   2. The mask always has the form 2^k - 1 (all lower k bits set)
;
; Masks tested:
;   ABI_UI_NODE_INDEX_MASK       = 4095  (2^12 - 1 = 0xFFF)
;   ABI_UI_STYLE_INDEX_MASK      = 8191  (2^13 - 1 = 0x1FFF)
;   ABI_UI_STATE_INDEX_MASK      = 8191
;   ABI_UI_RESOURCE_INDEX_MASK   = 2047  (2^11 - 1 = 0x7FF)
;   ABI_UI_MENU_INDEX_MASK       = 255   (2^8 - 1  = 0xFF)
;   ABI_UI_DIALOG_INDEX_MASK     = 127   (2^7 - 1  = 0x7F)
;   ABI_UI_STABLE_KEY_INDEX_MASK = 4095

(set-logic QF_BV)

; ============================================================
; Claim 1: For any hash and any power-of-two-minus-one mask,
;           hash & mask <= mask
; ============================================================
(reset)
(set-logic QF_BV)

(declare-fun hash () (_ BitVec 64))
(declare-fun mask () (_ BitVec 64))

; Constraint: mask is power-of-two-minus-one: mask = 2^k - 1 for some k
; Equivalent to: (mask + 1) is power of two AND mask != ~0
; For bitvector, mask + 1 having exactly one bit set means:
; (mask & (mask + 1)) == 0  AND  mask != ~0
(define-const mask_plus_one (_ BitVec 64) (bvadd mask #x0000000000000001))

; mask + 1 is power of two: (mask+1) != 0 AND ((mask+1) & mask) == 0
(assert (not (= mask_plus_one #x0000000000000000)))
(assert (= (bvand mask_plus_one mask) #x0000000000000000))

; Result of the start slot computation
(define-const result (_ BitVec 64) (bvand hash mask))

; Prove: result <= mask  (unsigned <=)
; If result > mask, that means some bit is set in result that is not set in mask
; which is impossible since AND only produces bits that are set in both operands.
; But let's prove it formally via SAT.
(assert (bvugt result mask))
(check-sat)
; Expected: unsat -- hash & mask <= mask for any mask that is 2^k - 1

; ============================================================
; Claim 2: ABI_UI_NODE_INDEX_MASK = 4095, result always < 4096
; ============================================================
(reset)
(set-logic QF_BV)

(define-const NODE_MASK (_ BitVec 64) #x0000000000000FFF)  ; 4095
(define-const NODE_CAPACITY (_ BitVec 64) #x0000000000001000)  ; 4096

(declare-fun hash () (_ BitVec 64))

(define-const slot (_ BitVec 64) (bvand hash NODE_MASK))

; Prove: slot < NODE_CAPACITY
(assert (bvuge slot NODE_CAPACITY))
(check-sat)
; Expected: unsat

; ============================================================
; Claim 3: ABI_UI_STYLE_INDEX_MASK = 8191, result always < 8192
; ============================================================
(reset)
(set-logic QF_BV)

(define-const STYLE_MASK (_ BitVec 64) #x0000000000001FFF)  ; 8191
(define-const STYLE_CAPACITY (_ BitVec 64) #x0000000000002000)  ; 8192

(declare-fun hash () (_ BitVec 64))

(define-const slot (_ BitVec 64) (bvand hash STYLE_MASK))

(assert (bvuge slot STYLE_CAPACITY))
(check-sat)
; Expected: unsat

; ============================================================
; Claim 4: ABI_UI_RESOURCE_INDEX_MASK = 2047, result always < 2048
; ============================================================
(reset)
(set-logic QF_BV)

(define-const RES_MASK (_ BitVec 64) #x00000000000007FF)  ; 2047
(define-const RES_CAPACITY (_ BitVec 64) #x0000000000000800)  ; 2048

(declare-fun hash () (_ BitVec 64))

(define-const slot (_ BitVec 64) (bvand hash RES_MASK))

(assert (bvuge slot RES_CAPACITY))
(check-sat)
; Expected: unsat

; ============================================================
; Claim 5: ABI_UI_MENU_INDEX_MASK = 255, result always < 256
; ============================================================
(reset)
(set-logic QF_BV)

(define-const MENU_MASK (_ BitVec 64) #x00000000000000FF)  ; 255
(define-const MENU_CAPACITY (_ BitVec 64) #x0000000000000100)  ; 256

(declare-fun hash () (_ BitVec 64))

(define-const slot (_ BitVec 64) (bvand hash MENU_MASK))

(assert (bvuge slot MENU_CAPACITY))
(check-sat)
; Expected: unsat

; ============================================================
; Claim 6: ABI_UI_DIALOG_INDEX_MASK = 127, result always < 128
; ============================================================
(reset)
(set-logic QF_BV)

(define-const DIALOG_MASK (_ BitVec 64) #x000000000000007F)  ; 127
(define-const DIALOG_CAPACITY (_ BitVec 64) #x0000000000000080)  ; 128

(declare-fun hash () (_ BitVec 64))

(define-const slot (_ BitVec 64) (bvand hash DIALOG_MASK))

(assert (bvuge slot DIALOG_CAPACITY))
(check-sat)
; Expected: unsat
