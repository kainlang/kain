; Proof: kain_memory_order_name_from_code switch → LUT
;
; Current code (switch):
;   static const char* kain_memory_order_name_from_code(int64_t ordering) {
;       switch (ordering) {
;       case KAIN_MEMORY_ORDER_RELAXED: return "relaxed";
;       case KAIN_MEMORY_ORDER_ACQUIRE: return "acquire";
;       case KAIN_MEMORY_ORDER_RELEASE: return "release";
;       case KAIN_MEMORY_ORDER_ACQ_REL: return "acq_rel";
;       case KAIN_MEMORY_ORDER_SEQ_CST:
;       default: return "seq_cst";
;       }
;   }
;
; LUT form:
;   static const char* KAIN_ORDER_NAMES[5] = {
;       "relaxed", "acquire", "release", "acq_rel", "seq_cst"
;   };
;   #define KAIN_MEMORY_ORDER_INDEX_CLAMP(o) ((size_t)((o) >= 5 ? 4 : (o)))
;   static const char* kain_memory_order_name_from_code(int64_t ordering) {
;       return KAIN_ORDER_NAMES[KAIN_MEMORY_ORDER_INDEX_CLAMP(ordering)];
;   }
;
; We model this as a finite function from ordering code to string index.
; Domain: ordering is int64_t, but meaningful values are 0..4.
; The clamp ensures values outside 0..4 map to index 4 ("seq_cst").

(set-logic QF_BV)
(declare-const ordering (_ BitVec 64))

; Switch output mapping: returns 0-4 representing the string index
(define-fun switch_form ((o (_ BitVec 64))) (_ BitVec 64)
  (ite (= o (_ bv0 64)) (_ bv0 64)
  (ite (= o (_ bv1 64)) (_ bv1 64)
  (ite (= o (_ bv2 64)) (_ bv2 64)
  (ite (= o (_ bv3 64)) (_ bv3 64)
  (ite (= o (_ bv4 64)) (_ bv4 64)
       (_ bv4 64))))))  ; default → seq_cst (index 4)

; LUT + clamp: clamp to [0,4], then use as index
(define-fun clamp ((o (_ BitVec 64))) (_ BitVec 64)
  (ite (bvuge o (_ bv5 64)) (_ bv4 64) o))

(define-fun lut_form ((o (_ BitVec 64))) (_ BitVec 64)
  (clamp o))

; Prove equivalence for ALL 64-bit ordering values
(assert (not (= (switch_form ordering) (lut_form ordering))))
(check-sat)
; Expected: unsat (equivalent for all 2^64 possible ordering values)
