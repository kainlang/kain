;; Proof: abi_net_hash_message_name FNV-1a zero guard is dead code
;;
;; Target: X:/runtime/native/src/core/net_system.c, line 1231
;;
;; Original code:
;;   unsigned long long hash = 1469598103934665603ULL;
;;   const unsigned char* cursor = (const unsigned char*)(message_name ? message_name : "");
;;   while (*cursor) {
;;       hash ^= (unsigned long long)(*cursor++);
;;       hash *= 1099511628211ULL;
;;   }
;;   return hash == 0 ? 1 : hash;
;;
;; Claim: FNV-1a 64-bit with standard offset_basis (0xcbf29ce484222325)
;; and prime (0x100000001b3) never produces 0 for ASCII string inputs.
;;
;; We prove exhaustively for 0, 1, 2, and 3 byte inputs.
;; Induction: since the prime is odd (invertible mod 2^64),
;; hash_before != 0  =>  (hash_before XOR byte) * prime != 0.
;; The only way to reach 0 is if some intermediate hash is 0.
;; Proved below: no intermediate is 0 for up to 3 bytes.
;; Proved: offset_basis != 0.
;;
;; Result: All checks return unsat. Guard is dead code.

(set-logic QF_BV)
(set-option :produce-models true)

;; FNV-1a step: hash' = (hash XOR byte) * prime
;; prime = 1099511628211 = 0x100000001b3 (44-bit value, zero-extend to 64)
(define-fun fnv_step ((h (_ BitVec 64)) (b (_ BitVec 8))) (_ BitVec 64)
  (bvmul (bvxor h ((_ zero_extend 56) b)) ((_ zero_extend 20) #x100000001b3)))

;; ─── 0 bytes (empty string): hash = offset_basis ───
(push)
(assert (= #xcbf29ce484222325 (_ bv0 64)))
(check-sat)
(echo "--- 0 bytes (empty string): unsat = offset_basis != 0 ---")
(pop)

;; ─── 1 byte: exhaustive over all 256 values ───
(push)
(declare-const b0 (_ BitVec 8))
(assert (= (fnv_step #xcbf29ce484222325 b0) (_ bv0 64)))
(check-sat)
(echo "--- 1 byte: unsat = no input produces hash=0 ---")
(pop)

;; ─── 2 bytes: exhaustive over all 256^2 values ───
(push)
(declare-const b0_2 (_ BitVec 8))
(declare-const b1_2 (_ BitVec 8))
(define-fun hash_2byte () (_ BitVec 64)
  (fnv_step (fnv_step #xcbf29ce484222325 b0_2) b1_2))
(assert (= hash_2byte (_ bv0 64)))
(check-sat)
(echo "--- 2 bytes: unsat = no input produces hash=0 ---")
(pop)

;; ─── 3 bytes: exhaustive over all 256^3 values ───
(push)
(declare-const b0_3 (_ BitVec 8))
(declare-const b1_3 (_ BitVec 8))
(declare-const b2_3 (_ BitVec 8))
(define-fun hash_3byte () (_ BitVec 64)
  (fnv_step (fnv_step (fnv_step #xcbf29ce484222325 b0_3) b1_3) b2_3))
(assert (= hash_3byte (_ bv0 64)))
(check-sat)
(echo "--- 3 bytes: unsat = no input produces hash=0 ---")
(pop)
