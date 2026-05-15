; Experimental proof for the packed helper-allocation header token.
;
; The low 16 bits of the first header word carry (slot + 1), while the upper
; bits carry the fixed helper-allocation magic tag. This keeps the header at
; 16 bytes and lets the runtime recover the helper-owned registry slot without
; a hash lookup.
(set-logic QF_BV)

(define-fun MAGIC_TAG () (_ BitVec 64) #x4b41494e4d450000)
(define-fun TOKEN_MASK () (_ BitVec 64) #x000000000000ffff)

(declare-const slot_token (_ BitVec 16))
(assert (not (= slot_token #x0000)))

(define-fun tagged_word () (_ BitVec 64)
  (bvor MAGIC_TAG ((_ zero_extend 48) slot_token)))
(define-fun extracted_token () (_ BitVec 16)
  ((_ extract 15 0) tagged_word))
(define-fun extracted_tag () (_ BitVec 64)
  (bvand tagged_word (bvnot TOKEN_MASK)))

(assert
  (or
    (not (= extracted_token slot_token))
    (not (= extracted_tag MAGIC_TAG))))

(check-sat)
