; C FFI tier classifier contract.
; Closed tier ids:
;   0 dynamic, 1 static, 2 bitcode, 3 inline, 4 fused
; The Rust-side contract is:
;   dynamic      <=> tier == 0
;   native_link  <=> tier in {1,2,3,4}
;   bitcodeish   <=> tier in {2,3}
;   fused        <=> tier == 4
; This proof asks Z3 for any closed-domain tier that violates disjointness or
; native-link exhaustiveness. `unsat` means no such tier exists.

(set-logic QF_BV)

(declare-fun tier () (_ BitVec 3))

(define-fun in_domain () Bool
  (or (= tier #b000)
      (= tier #b001)
      (= tier #b010)
      (= tier #b011)
      (= tier #b100)))

(define-fun dynamic () Bool
  (= tier #b000))

(define-fun native_link () Bool
  (and in_domain (not dynamic)))

(define-fun bitcodeish () Bool
  (or (= tier #b010) (= tier #b011)))

(define-fun fused () Bool
  (= tier #b100))

(define-fun expected_native_link () Bool
  (or (= tier #b001)
      (= tier #b010)
      (= tier #b011)
      (= tier #b100)))

(assert in_domain)
(assert
  (or
    ; Dynamic must never also be native-link.
    (and dynamic native_link)
    ; Bitcode/inline must never collapse back to dynamic.
    (and bitcodeish dynamic)
    ; Fused must never collapse back to dynamic or bitcode.
    (and fused dynamic)
    (and fused bitcodeish)
    ; Native-link must be exactly static|bitcode|inline|fused.
    (xor native_link expected_native_link)))

(check-sat)
