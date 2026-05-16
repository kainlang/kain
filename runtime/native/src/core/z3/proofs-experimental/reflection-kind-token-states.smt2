; Experimental Z3 reference proof for the reflection kind classifiers in
; reflection.c.
; Claim: the 64-bit first-32-byte token state used by the switch classifiers is
; collision-free across the current reflection type/item kind universe.
(set-logic QF_BV)

; primitive
(define-fun kind_token_00 () (_ BitVec 64) (_ bv13382399331382586066 64))
; struct
(define-fun kind_token_01 () (_ BitVec 64) (_ bv9760905267964471132 64))
; enum
(define-fun kind_token_02 () (_ BitVec 64) (_ bv6788488601190999599 64))
; array
(define-fun kind_token_03 () (_ BitVec 64) (_ bv557078717461502598 64))
; pointer
(define-fun kind_token_04 () (_ BitVec 64) (_ bv418330185651559097 64))
; function
(define-fun kind_token_05 () (_ BitVec 64) (_ bv13019726129627826729 64))
; actor
(define-fun kind_token_06 () (_ BitVec 64) (_ bv13860379889069134867 64))
; message
(define-fun kind_token_07 () (_ BitVec 64) (_ bv13088764986731127309 64))
; component
(define-fun kind_token_08 () (_ BitVec 64) (_ bv16472132711788117557 64))
; service
(define-fun kind_token_09 () (_ BitVec 64) (_ bv17732766607181989341 64))
; module
(define-fun kind_token_10 () (_ BitVec 64) (_ bv5126111608608244006 64))

(assert
  (not
    (distinct
      kind_token_00 kind_token_01 kind_token_02 kind_token_03 kind_token_04
      kind_token_05 kind_token_06 kind_token_07 kind_token_08 kind_token_09
      kind_token_10)))
(check-sat)
