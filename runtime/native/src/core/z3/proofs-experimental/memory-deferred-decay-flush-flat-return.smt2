; Proof: flat return for kain_alloc_should_flush_deferred_decay
;
; Current code:
;   static int kain_alloc_should_flush_deferred_decay(size_t payload_size) {
;       if (payload_size >= KAIN_ALLOC_DEFERRED_DECAY_FLUSH_SIZE_THRESHOLD) {
;           return 1;
;       }
;       return __kain_ownership_deferred_decay_count() >= KAIN_DEFERRED_DECAY_FLUSH_WATERMARK;
;   }
;
; Flat form:
;   static int kain_alloc_should_flush_deferred_decay(size_t payload_size) {
;       return payload_size >= KAIN_ALLOC_DEFERRED_DECAY_FLUSH_SIZE_THRESHOLD
;           || __kain_ownership_deferred_decay_count() >= KAIN_DEFERRED_DECAY_FLUSH_WATERMARK;
;   }
;
; Domain: both conditions produce bool (0 or 1).
; Proving: if (a) return 1; return b;  ===  return a || b;

(set-logic QF_BV)

(declare-const a (_ BitVec 64))  ; payload_size
(declare-const b (_ BitVec 64))  ; decay_count

(define-fun THRESHOLD () (_ BitVec 64) (_ bv262145 64))  ; KAIN_ALLOC_DEFERRED_DECAY_FLUSH_SIZE_THRESHOLD
(define-fun WATERMARK () (_ BitVec 64) (_ bv1024 64))    ; KAIN_DEFERRED_DECAY_FLUSH_WATERMARK

; Condition 1: payload_size >= THRESHOLD
(define-fun cond1 () (_ BitVec 64)
  (ite (bvuge a THRESHOLD) (_ bv1 64) (_ bv0 64)))

; Condition 2: decay_count >= WATERMARK
(define-fun cond2 () (_ BitVec 64)
  (ite (bvuge b WATERMARK) (_ bv1 64) (_ bv0 64)))

; If/else form (original)
(define-fun if_form () (_ BitVec 64)
  (ite (= cond1 (_ bv1 64)) (_ bv1 64) cond2))

; Flat logical OR form
(define-fun flat_form () (_ BitVec 64)
  (ite (or (= cond1 (_ bv1 64)) (= cond2 (_ bv1 64))) (_ bv1 64) (_ bv0 64)))

; Prove equivalence for all possible a, b
(assert (not (= if_form flat_form)))
(check-sat)
; Expected: unsat (equivalent for all inputs)
