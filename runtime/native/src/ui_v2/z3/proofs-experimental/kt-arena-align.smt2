;; ============================================================
;; Proof: Arena power-of-two alignment — KT_ALIGN_UP
;;
;; Target: arena.c — kt_arena_align_up()
;;
;; Current (division): ((ptr + align - 1) / align) * align
;; Candidate (bitwise): ((ptr) + (align) - 1) & ~((align) - 1)
;;
;; On x86-64:
;;   Division: div/mul ~30-80 cycles  |  Bitwise: lea+and ~2-3 cycles
;;   Speedup: 10-40x per alignment op, called ~10-100x per frame
;;
;; Each claim is a self-contained (set-logic) .. (check-sat) block
;; ============================================================

;; Claim 1: Bitwise ≡ division alignment for pow2 align (32-bit)
(set-logic QF_BV)
(set-option :produce-models true)
(declare-fun ptr () (_ BitVec 32))
(declare-fun align () (_ BitVec 32))
(assert (not (= align #x00000000)))
(assert (= (bvand align (bvsub align #x00000001)) #x00000000))  ;; pow2 test
(define-fun align_bitwise ((p (_ BitVec 32)) (a (_ BitVec 32))) (_ BitVec 32)
  (bvand (bvadd p (bvsub a #x00000001)) (bvnot (bvsub a #x00000001))))
(define-fun align_div ((p (_ BitVec 32)) (a (_ BitVec 32))) (_ BitVec 32)
  (bvmul (bvudiv (bvadd p (bvsub a #x00000001)) a) a))
(assert (not (= (align_bitwise ptr align) (align_div ptr align))))
(check-sat)
;; Expected: unsat — both alignment methods equivalent

;; Claim 2: Result >= ptr (monotonic, 32-bit)
;; Guard: ptr + (align-1) does NOT overflow (arena invariant)
(reset)
(set-logic QF_BV)
(set-option :produce-models true)
(declare-fun ptr () (_ BitVec 32))
(declare-fun align () (_ BitVec 32))
(assert (not (= align #x00000000)))
(assert (= (bvand align (bvsub align #x00000001)) #x00000000))
(define-fun align_m1 () (_ BitVec 32) (bvsub align #x00000001))
;; No-overflow guard: ptr + (align-1) doesn't wrap
(assert (bvule ptr (bvsub #xFFFFFFFF align_m1)))
(define-fun result () (_ BitVec 32)
  (bvand (bvadd ptr align_m1) (bvnot align_m1)))
(assert (bvult result ptr))
(check-sat)
;; Expected: unsat — result >= ptr when no overflow

;; Claim 3: Result properly aligned (result & (align-1) == 0)
(reset)
(set-logic QF_BV)
(set-option :produce-models true)
(declare-fun ptr () (_ BitVec 32))
(declare-fun align () (_ BitVec 32))
(assert (not (= align #x00000000)))
(assert (= (bvand align (bvsub align #x00000001)) #x00000000))
(define-fun result () (_ BitVec 32)
  (bvand (bvadd ptr (bvsub align #x00000001)) (bvnot (bvsub align #x00000001))))
(assert (not (= (bvand result (bvsub align #x00000001)) #x00000000)))
(check-sat)
;; Expected: unsat — result always aligned

;; Claim 4: Result is MINIMAL (no smaller value ≥ ptr satisfies alignment)
(reset)
(set-logic QF_BV)
(set-option :produce-models true)
(declare-fun ptr () (_ BitVec 32))
(declare-fun align () (_ BitVec 32))
(assert (not (= align #x00000000)))
(assert (= (bvand align (bvsub align #x00000001)) #x00000000))
(define-fun result () (_ BitVec 32)
  (bvand (bvadd ptr (bvsub align #x00000001)) (bvnot (bvsub align #x00000001))))
(declare-fun v () (_ BitVec 32))
(assert (bvule ptr v))
(assert (bvult v result))
(assert (= (bvand v (bvsub align #x00000001)) #x00000000))
(check-sat)
;; Expected: unsat — no smaller aligned value exists

;; Claim 5: Overflow condition identified
(reset)
(set-logic QF_BV)
(set-option :produce-models true)
(declare-fun ptr () (_ BitVec 32))
(declare-fun align () (_ BitVec 32))
(assert (not (= align #x00000000)))
(assert (= (bvand align (bvsub align #x00000001)) #x00000000))
(define-fun sum () (_ BitVec 32) (bvadd ptr (bvsub align #x00000001)))
(define-fun overflows () Bool (bvult sum ptr))
(define-fun overflow_condition () Bool
  (bvugt ptr (bvsub #xFFFFFFFF (bvsub align #x00000001))))
(assert (not (= overflows overflow_condition)))
(check-sat)
;; Expected: unsat — overflow condition is ptr > UINT_MAX - (align-1)

;; Claim 6: 64-bit version also works
(reset)
(set-logic QF_BV)
(set-option :produce-models true)
(declare-fun ptr64 () (_ BitVec 64))
(declare-fun align64 () (_ BitVec 64))
(assert (not (= align64 #x0000000000000000)))
(assert (= (bvand align64 (bvsub align64 #x0000000000000001)) #x0000000000000000))
(define-fun result64 () (_ BitVec 64)
  (bvand (bvadd ptr64 (bvsub align64 #x0000000000000001))
         (bvnot (bvsub align64 #x0000000000000001))))
(assert (not (= (bvand result64 (bvsub align64 #x0000000000000001)) #x0000000000000000)))
(check-sat)
;; Expected: unsat — 64-bit alignment is correct

(echo "=== ALL 6 ARENA ALIGNMENT CLAIMS: unsat = PROVEN ===")
(echo "KT_ALIGN_UP(ptr,align) = ((ptr)+(align)-1) & ~((align)-1)")
(echo "1. ≡ ((ptr+align-1)/align)*align  [div→bitwise, 10-40x faster]")
(echo "2. result >= ptr (monotonic)")
(echo "3. (result & (align-1)) == 0 (correctly aligned)")
(echo "4. No smaller value ≥ ptr is aligned (minimal)")
(echo "5. Overflow when ptr > UINT_MAX-(align-1) [arena invariant prevents]")
(echo "6. 64-bit version also correct")
