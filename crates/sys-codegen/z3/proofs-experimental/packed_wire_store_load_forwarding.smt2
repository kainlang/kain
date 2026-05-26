(set-logic QF_AUFBV)

; Negated correctness search for forwarding compiler-owned ephemeral stack-buffer
; stores into immediately dominated loads. If this is unsat, there is no packet
; in zero_copy_binary_wire where the four word addresses alias incorrectly or a
; load-after-store returns a value other than the last same-address store.

(define-fun word_addr ((packet (_ BitVec 64)) (lane (_ BitVec 64))) (_ BitVec 64)
  (bvadd (bvshl packet (_ bv2 64)) lane))

(define-fun byte_end_for_word ((word_index (_ BitVec 64))) (_ BitVec 64)
  (bvadd (bvshl word_index (_ bv3 64)) (_ bv7 64)))

(declare-const packet (_ BitVec 64))
(declare-const word0 (_ BitVec 64))
(declare-const word1 (_ BitVec 64))
(declare-const word2 (_ BitVec 64))
(declare-const word3 (_ BitVec 64))
(declare-const memory0 (Array (_ BitVec 64) (_ BitVec 64)))

(define-fun a0 () (_ BitVec 64) (word_addr packet (_ bv0 64)))
(define-fun a1 () (_ BitVec 64) (word_addr packet (_ bv1 64)))
(define-fun a2 () (_ BitVec 64) (word_addr packet (_ bv2 64)))
(define-fun a3 () (_ BitVec 64) (word_addr packet (_ bv3 64)))

(define-fun memory1 () (Array (_ BitVec 64) (_ BitVec 64)) (store memory0 a0 word0))
(define-fun memory2 () (Array (_ BitVec 64) (_ BitVec 64)) (store memory1 a1 word1))
(define-fun memory3 () (Array (_ BitVec 64) (_ BitVec 64)) (store memory2 a2 word2))
(define-fun memory4 () (Array (_ BitVec 64) (_ BitVec 64)) (store memory3 a3 word3))

(assert (bvult packet (_ bv64 64)))

(assert
  (or
    ; packet*4 + lanes 0..3 are distinct word addresses, so later stores do not
    ; clobber earlier lane loads.
    (= a0 a1) (= a0 a2) (= a0 a3) (= a1 a2) (= a1 a3) (= a2 a3)
    ; 8-byte words stay fully inside the 2048-byte stack buffer.
    (not (bvult (byte_end_for_word a0) (_ bv2048 64)))
    (not (bvult (byte_end_for_word a1) (_ bv2048 64)))
    (not (bvult (byte_end_for_word a2) (_ bv2048 64)))
    (not (bvult (byte_end_for_word a3) (_ bv2048 64)))
    ; The forwarded SSA values equal the memory values the real loads would see.
    (not (= (select memory4 a0) word0))
    (not (= (select memory4 a1) word1))
    (not (= (select memory4 a2) word2))
    (not (= (select memory4 a3) word3))))

(check-sat)
