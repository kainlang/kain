; Proof: abi_ui_node_is_visible -- node with HIDDEN flag is never visible
;
; The function (line ~286):
;   static int abi_ui_node_is_visible(const KainNativeUiNode* node) {
;       return node && ((node->flags & ABI_UI_NODE_HIDDEN) == 0);
;   }
;
; ABI_UI_NODE_HIDDEN = 1 << 0 = 1
;
; Key claims:
;   1. If node->flags has bit 0 set, is_visible returns false (0)
;   2. If node->flags has bit 0 clear, is_visible returns true (1)
;   3. The HIDDEN check is a single-bit mask, no false positives/negatives

(set-logic QF_BV)

; ============================================================
; Claim 1: flags & HIDDEN != 0 => is_visible == 0
; ============================================================
(reset)
(set-logic QF_BV)

(define-const HIDDEN (_ BitVec 64) #x0000000000000001)

(declare-fun flags () (_ BitVec 64))

; Precondition: HIDDEN bit is set
(assert (= (bvand flags HIDDEN) HIDDEN))

; is_visible: ((flags & HIDDEN) == 0)
; In bitvector, we test whether (flags & HIDDEN) is zero
(define-const flags_and_hidden (_ BitVec 64) (bvand flags HIDDEN))

; Prove: is_visible == 0, i.e., (flags & HIDDEN) != 0
; We already asserted the HIDDEN bit is set, so prove that the check
; (flags & HIDDEN) == 0 would be FALSE
(assert (= flags_and_hidden #x0000000000000000))
(check-sat)
; Expected: unsat -- hidden node cannot have no hidden bit set

; ============================================================
; Claim 2: flags & HIDDEN == 0 => is_visible == 1
; ============================================================
(reset)
(set-logic QF_BV)

(define-const HIDDEN (_ BitVec 64) #x0000000000000001)

(declare-fun flags () (_ BitVec 64))

; Precondition: HIDDEN bit is NOT set
(assert (not (= (bvand flags HIDDEN) HIDDEN)))

; The actual check: ((flags & HIDDEN) == 0)
(define-const flags_and_hidden (_ BitVec 64) (bvand flags HIDDEN))

; Prove: (flags & HIDDEN) == 0 holds
(assert (not (= flags_and_hidden #x0000000000000000)))
(check-sat)
; Expected: unsat -- when HIDDEN bit is not set, the check passes

; ============================================================
; Claim 3: The HIDDEN check is precise -- it tests exactly bit 0
; and no other bits interfere.
; Prove: (flags & HIDDEN) is either 0 or HIDDEN (never other value)
; ============================================================
(reset)
(set-logic QF_BV)

(define-const HIDDEN (_ BitVec 64) #x0000000000000001)

(declare-fun flags () (_ BitVec 64))

(define-const flags_and_hidden (_ BitVec 64) (bvand flags HIDDEN))

; Prove: the result is either 0 or 1
(assert (not (or (= flags_and_hidden #x0000000000000000)
                  (= flags_and_hidden #x0000000000000001))))
(check-sat)
; Expected: unsat -- AND with single-bit mask only produces 0 or that bit
