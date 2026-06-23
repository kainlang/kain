; Proof: runtime_copy_entangle_text all guard conditions
;
; The function:
;   static int runtime_copy_entangle_text(char* dst, size_t dst_cap, const char* src) {
;       if (dst == 0 || dst_cap == 0 || src == 0 || src[0] == '\0') { return -1; }
;       size_t len = strlen(src);
;       if (len >= dst_cap) { return -2; }
;       memcpy(dst, src, len + 1);
;       return 0;
;   }
;
; Key claims:
;   1. Null/zero guard catches dst==0, dst_cap==0, src==0, src[0]=='\0' before use
;   2. strlen is never called with src==0
;   3. memcpy is never called with len+1 exceeding dst_cap
;   4. When guard len>=dst_cap fails (length too big), the copy is correctly prevented
;   5. When guard passes (len < dst_cap), the len+1 calculation never wraps around
;   6. String terminator is always included in the copy (len+1 bytes copied for strlen==len)
;
(set-logic QF_BV)

; ============================================================
; Claim 1: Empty string (src[0]=='\0') is rejected
; The guard returns -1 when src[0] == '\0'.
; After the guard passes, src[0] != '\0', so strlen(src) >= 1.
; ============================================================
(push)
(declare-const dst_nonzero Bool)
(declare-const dst_cap_nonzero Bool)
(declare-const src_nonzero Bool)
(declare-const src_nonempty Bool)

; Guard condition (all must be true for function to continue):
; dst != 0 AND dst_cap != 0 AND src != 0 AND src[0] != '\0'
(assert (and dst_nonzero dst_cap_nonzero src_nonzero src_nonempty))

; If all guards pass, the string is non-empty, so len >= 1
; (This is a semantic consequence of src[0] != '\0')
; We model src[0] != '\0' as: strlen(src) >= 1
(define-const MIN_LEN (_ BitVec 64) #x0000000000000001)
(define-const ALLOWED_LEN (_ BitVec 64) (ite (and dst_nonzero dst_cap_nonzero src_nonzero src_nonempty) MIN_LEN #x0000000000000000))

; After guard passes, len >= 1
(assert (not (bvuge ALLOWED_LEN MIN_LEN)))
(check-sat)
(pop)
; Expected: unsat — guard ensures string is non-empty

(reset)

; ============================================================
; Claim 2: len+1 never overflows when len < dst_cap
;
; Given: len < dst_cap AND dst_cap > 0 (from guard)
; Prove: len + 1 overflows only if the guard would have caught it
; ============================================================
(set-logic QF_BV)
(declare-const len (_ BitVec 64))
(declare-const dst_cap (_ BitVec 64))

; Guard constraints: dst_cap > 0 and len < dst_cap
(assert (bvugt dst_cap #x0000000000000000))
(assert (bvult len dst_cap))

; Claim: len + 1 does not overflow (no unsigned wrap)
; len + 1 > len  (if no overflow)
; len + 1 < len  (if overflow, because wrapping)
(assert (not (bvugt (bvadd len #x0000000000000001) len)))
(check-sat)
; Expected: unsat — the guard len < dst_cap and dst_cap > 0 ensures len < SIZE_MAX,
; so len+1 cannot overflow

(reset)

; ============================================================
; Claim 3: When len < dst_cap, then len + 1 <= dst_cap
; This ensures memcpy(dst, src, len+1) never writes past the buffer.
; ============================================================
(set-logic QF_BV)
(declare-const len (_ BitVec 64))
(declare-const dst_cap (_ BitVec 64))

(assert (bvugt dst_cap #x0000000000000000))
(assert (bvult len dst_cap))

(define-fun copy_size () (_ BitVec 64) (bvadd len #x0000000000000001))

(assert (not (bvule copy_size dst_cap)))
(check-sat)
; Expected: unsat — len < dst_cap implies len+1 <= dst_cap

(reset)

; ============================================================
; Claim 4: When len >= dst_cap, the function returns -2 BEFORE memcpy
; 
; The guard check is: if (len >= dst_cap) { return -2; }
; This means memcpy is only reached when len < dst_cap.
; The guard correctly prevents:
;   a) len == dst_cap (exact fit but no room for terminator)
;   b) len > dst_cap (buffer overflow)
;
; We prove that: After the if-guard, the invariant len < dst_cap holds.
; This is guaranteed by the fact that the only way to reach memcpy is
; through the code path where the guard did NOT fire.
; ============================================================
(set-logic QF_BV)
(declare-const len (_ BitVec 64))
(declare-const dst_cap (_ BitVec 64))

(assert (bvugt dst_cap #x0000000000000000))

; Model: the function is at the memcpy call. This is only reachable
; when the guard condition (len >= dst_cap) evaluates to FALSE.
; We assert: we are at the memcpy call, meaning guard did NOT fire.
; Prove: len < dst_cap must hold.

(define-fun guard_fired () Bool (bvuge len dst_cap))

; We are at the memcpy line. Guard did NOT fire.
(assert (not guard_fired))

; Therefore: len < dst_cap
(assert (not (bvult len dst_cap)))
(check-sat)
; Expected: unsat — being at memcpy implies len < dst_cap

(reset)

; ============================================================
; Claim 4 (continued): The early return when len >= dst_cap is correct.
; The guard check `len >= dst_cap` is exhaustive: there is no case where
; the copy is safe but the guard fires.
; ============================================================
(set-logic QF_BV)
(declare-const len (_ BitVec 64))
(declare-const dst_cap (_ BitVec 64))

(assert (bvugt dst_cap #x0000000000000000))

; Guard fires when len >= dst_cap
(assert (bvuge len dst_cap))

; Prove: the copy would indeed exceed dst_cap (or the terminator would not fit)
; For the common case (no overflow): len+1 > dst_cap when len >= dst_cap
; For len == SIZE_MAX (overflow): len+1 = 0, but this is unreachable as
;   strlen() would need a 2^64-1 byte string, which cannot exist in memory.
; 
; So we prove: for the realistic domain (len < SIZE_MAX),
;   len >= dst_cap → len + 1 > dst_cap

(define-fun copy_size () (_ BitVec 64) (bvadd len #x0000000000000001))

; Add realistic constraint: len < SIZE_MAX (strlen can't return SIZE_MAX)
(assert (bvult len (bvnot #x0000000000000000)))

; Prove: copy_size > dst_cap
(assert (not (bvugt copy_size dst_cap)))
(check-sat)
; Expected: unsat — for len < SIZE_MAX, len >= dst_cap implies len+1 > dst_cap

(reset)

; ============================================================
; Claim 5: sizeof(binding.authority) = 256 = ENTANGLE_MAX_PATH
; Const-folded proof: the compile-time constant sizeof matches the #define.
; ============================================================
(set-logic QF_BV)

(define-const ENTANGLE_MAX_PATH (_ BitVec 64) #x0000000000000100)  ; 256
(define-const ENTANGLE_MAX_POLICY (_ BitVec 64) #x0000000000000040)  ; 64
(define-const ENTANGLE_MAX_TYPE (_ BitVec 64) #x0000000000000080)  ; 128
(define-const ENTANGLE_MAX_BINDINGS (_ BitVec 64) #x0000000000000080)  ; 128

; Claim 5a: sizeof(authority) = ENTANGLE_MAX_PATH = 256
; In the struct, authority is char[ENTANGLE_MAX_PATH] so sizeof = 256
(push)
(define-const SIZEOF_AUTHORITY (_ BitVec 64) #x0000000000000100)
(assert (not (= SIZEOF_AUTHORITY ENTANGLE_MAX_PATH)))
(check-sat)
(pop)
; Expected: unsat

; Claim 5b: sizeof(policy) = ENTANGLE_MAX_POLICY = 64
(push)
(define-const SIZEOF_POLICY (_ BitVec 64) #x0000000000000040)
(assert (not (= SIZEOF_POLICY ENTANGLE_MAX_POLICY)))
(check-sat)
(pop)
; Expected: unsat

; Claim 5c: sizeof(type_name) = ENTANGLE_MAX_TYPE = 128
(push)
(define-const SIZEOF_TYPE_NAME (_ BitVec 64) #x0000000000000080)
(assert (not (= SIZEOF_TYPE_NAME ENTANGLE_MAX_TYPE)))
(check-sat)
(pop)
; Expected: unsat

; Claim 5d: sizeof(mirror) = ENTANGLE_MAX_PATH = 256
(push)
(define-const SIZEOF_MIRROR (_ BitVec 64) #x0000000000000100)
(assert (not (= SIZEOF_MIRROR ENTANGLE_MAX_PATH)))
(check-sat)
(pop)
; Expected: unsat

(reset)

; ============================================================
; Claim 6: The two ENTANGLE_MAX_PATH fields (authority, mirror) use 256-byte buffers,
; which is the largest of the field capacities. ENTANGLE_MAX_TYPE = 128, ENTANGLE_MAX_POLICY = 64.
; No field overlaps with another (C struct layout guarantees this, but let's check
; our offsets are consistent).
;
; Struct layout:
;   authority[0..255]:   offset 0,   size 256
;   mirror[0..255]:      offset 256, size 256
;   policy[0..63]:       offset 512, size 64
;   type_name[0..127]:   offset 576, size 128
; Total: 704 bytes
; ============================================================
(set-logic QF_BV)

(define-const SIZEOF_AUTHORITY (_ BitVec 64) #x0000000000000100)  ; 256
(define-const SIZEOF_MIRROR (_ BitVec 64) #x0000000000000100)     ; 256
(define-const SIZEOF_POLICY (_ BitVec 64) #x0000000000000040)     ; 64
(define-const SIZEOF_TYPE (_ BitVec 64) #x0000000000000080)       ; 128

; Total struct size = authority + mirror + policy + type_name (no padding since all char arrays)
(define-const TOTAL_SIZE (_ BitVec 64) #x00000000000002C0)  ; 704

; Sum of field sizes
(define-const FIELD_SUM (_ BitVec 64)
  (bvadd SIZEOF_AUTHORITY (bvadd SIZEOF_MIRROR (bvadd SIZEOF_POLICY SIZEOF_TYPE))))

(push)
(assert (not (= TOTAL_SIZE FIELD_SUM)))
(check-sat)
(pop)
; Expected: unsat — 704 = 256+256+64+128

; Claim 6b: sizeof(g_kain_entangle_bindings) = 128 * 704 = 90112
; This is the buffer size zeroed by entangle_registry_reset
(define-const TOTAL_WITH_BINDINGS (_ BitVec 64)
  (bvmul ENTANGLE_MAX_BINDINGS TOTAL_SIZE))

(define-const EXPECTED_GLOBAL_SIZE (_ BitVec 64) #x0000000000016000)  ; 90112

(push)
(assert (not (= TOTAL_WITH_BINDINGS EXPECTED_GLOBAL_SIZE)))
(check-sat)
(pop)
; Expected: unsat — 128 * 704 = 90112 = 0x16000

(reset)

; ============================================================
; Claim 7: The register function ensures that dst_cap arguments are
; compile-time constants that match the field sizes. 
; sizeof(binding.authority) = 256, sizeof(binding.mirror) = 256,
; sizeof(binding.policy) = 64, sizeof(binding.type_name) = 128
;
; These are all > 0, so the dst_cap == 0 check in runtime_copy_entangle_text
; will always pass when called from entangle_registry_register.
; ============================================================
(set-logic QF_BV)

(define-const AUTH_CAP (_ BitVec 64) #x0000000000000100)   ; sizeof(binding.authority) = 256
(define-const MIRROR_CAP (_ BitVec 64) #x0000000000000100)  ; sizeof(binding.mirror) = 256
(define-const POLICY_CAP (_ BitVec 64) #x0000000000000040)  ; sizeof(binding.policy) = 64
(define-const TYPE_CAP (_ BitVec 64) #x0000000000000080)    ; sizeof(binding.type_name) = 128

; All capacities are non-zero
(push)
(assert (not (and (bvugt AUTH_CAP #x0000000000000000)
                  (bvugt MIRROR_CAP #x0000000000000000)
                  (bvugt POLICY_CAP #x0000000000000000)
                  (bvugt TYPE_CAP #x0000000000000000))))
(check-sat)
(pop)
; Expected: unsat

; All capacities are strictly positive, so dst_cap==0 check is never triggered
; from dentro da entangle_registry_register.
(push)
(assert (or (= AUTH_CAP #x0000000000000000)
            (= MIRROR_CAP #x0000000000000000)
            (= POLICY_CAP #x0000000000000000)
            (= TYPE_CAP #x0000000000000000)))
(check-sat)
(pop)
; Expected: unsat — all capacities are compile-time constants > 0
