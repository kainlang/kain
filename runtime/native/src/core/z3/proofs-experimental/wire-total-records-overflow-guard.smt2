; wire-total-records-overflow-guard.smt2
;
; Prove: The overflow guard in abi_wire_zero_copy_binary_checksum correctly
; prevents int64_t overflow in the calculation iterations * 64.
;
; Source: X:/runtime/native/src/core/wire.c
; Function: abi_wire_zero_copy_binary_checksum, line 50
;
; Guard code:
;   if (iterations > (INT64_MAX / KAIN_WIRE_PACKET_COUNT)) { return -1; }
;   total_records = iterations * KAIN_WIRE_PACKET_COUNT;
;
; Constants: KAIN_WIRE_PACKET_COUNT = 64, INT64_MAX = 9223372036854775807
;
; We prove: For any iterations >= 0,
;   iterations <= INT64_MAX / 64  implies  iterations * 64 <= INT64_MAX
;
; Since iterations is int64_t (signed 64-bit), we model it as a signed
; 64-bit integer (QF_BV or QF_LIA).

(set-logic QF_LIA)

(declare-const iterations Int)

(define-const PACKET_COUNT Int 64)
(define-const INT64_MAX Int 9223372036854775807)

; Precondition: iterations is a valid int64_t (0 <= iterations <= INT64_MAX)
; (the function also checks iterations >= 0 before this point)
(assert (>= iterations 0))
(assert (<= iterations INT64_MAX))

; The guard check succeeds: iterations <= INT64_MAX / 64
(assert (<= iterations (div INT64_MAX PACKET_COUNT)))

; Negate the claim: iterations * 64 > INT64_MAX (overflow)
(assert (> (* iterations PACKET_COUNT) INT64_MAX))

(check-sat)
; Expected: unsat — the overflow cannot happen when the guard passes

; ── Also test the converse: the guard catches all overflow cases ──
(reset)

(declare-const iterations2 Int)
(assert (>= iterations2 0))
(assert (<= iterations2 INT64_MAX))

; The guard check FAILS: iterations > INT64_MAX / 64
(assert (> iterations2 (div INT64_MAX PACKET_COUNT)))

; Claim: iterations * 64 necessarily overflows (> INT64_MAX)
(assert (not (> (* iterations2 PACKET_COUNT) INT64_MAX)))

(check-sat)
; Expected: unsat — all values that fail the guard do overflow
