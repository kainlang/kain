; entangle-max-bindings-power-of-two.smt2
;
; Claim: ENTANGLE_MAX_BINDINGS = 128 is a power of two (2^7).
; This property enables:
; 1. Index masking: index & 127 works as well as index % 128
; 2. 128-bit bitset: the full registry fits in a pair of uint64_t's
;    for O(1) occupancy check
; 3. Low-bit isolation: finding a free slot via CTZ on a 128-bit
;    bitset is always correct for a power-of-two-sized pool

(set-logic QF_BV)

; 128 = 0x80, 127 = 0x7F
(define-const MAX_BINDINGS (_ BitVec 64) #x0000000000000080)
(define-const MASK (_ BitVec 64) #x000000000000007F)

; Claim 1: 128 is a power of two
; A power of two has exactly one bit set, so (x & (x-1)) == 0
(push)
(assert (not (= (bvand MAX_BINDINGS (bvsub MAX_BINDINGS #x0000000000000001)) #x0000000000000000)))
(check-sat)
(pop)
; unsat = 128 is power of two

; Claim 2: index & 127 == index % 128 for all unsigned indices
; Since 128 is a power of two, AND with (128-1) is equivalent to modulo 128.
(declare-const index (_ BitVec 64))

(push)
(assert (not (= (bvand index MASK) (bvurem index MAX_BINDINGS))))
(check-sat)
(pop)
; unsat = AND-mask == modulo for all index values

; Claim 3: index & 127 is always < 128
(push)
(assert (not (bvult (bvand index MASK) MAX_BINDINGS)))
(check-sat)
(pop)
; unsat = masked index always < 128

; Claim 4: After guard, count < 128, so count is always a valid index
(declare-const count (_ BitVec 64))
(assert (bvult count MAX_BINDINGS))

; count is always in [0, 127]
(push)
(assert (bvuge count MAX_BINDINGS))
(check-sat)
(pop)
; unsat = count < 128 -> count cannot be >= 128

; Claim 5: count+1 <= 128 when count < 128
(define-fun next_count () (_ BitVec 64) (bvadd count #x0000000000000001))
(push)
(assert (not (bvule next_count MAX_BINDINGS)))
(check-sat)
(pop)
; unsat = count+1 <= 128 when count < 128
