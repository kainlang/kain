; Helper-owned decay slot reclaim and tombstone probe proof.
;
; Target:
;   runtime/native/src/core/ownership.c
;
; Claims:
;   - UINT32_MAX tombstone cannot collide with empty 0 or encoded slots 1..4096.
;   - A linear probe lookup skips tombstones and still finds a later live slot.
;   - Helper decay that frees an idle heap allocation clears occupancy, making the
;     old helper token fail instead of leaving a stale occupied region.
(set-logic QF_BV)

(define-fun MAX_REGIONS () (_ BitVec 32) (_ bv4096 32))
(define-fun TOMBSTONE () (_ BitVec 32) #xffffffff)
(define-fun EMPTY () (_ BitVec 32) (_ bv0 32))
(define-fun OK () (_ BitVec 4) #x0)
(define-fun NOT_FOUND () (_ BitVec 4) #xe)
(define-fun STATE_IDLE () (_ BitVec 2) #b00)
(define-fun STATE_DECAYED () (_ BitVec 2) #b11)
(define-fun KIND_HEAP () (_ BitVec 3) #b001)

(declare-const slot (_ BitVec 32))
(declare-const encoded_slot (_ BitVec 32))
(declare-const ptr_matches Bool)
(declare-const occupied_before Bool)
(declare-const free_status_ok Bool)
(declare-const state_before (_ BitVec 2))
(declare-const kind_before (_ BitVec 3))
(declare-const observers_before (_ BitVec 32))

(assert (bvult slot MAX_REGIONS))
(assert (= encoded_slot (bvadd slot (_ bv1 32))))

; Encoded slots are exactly 1..4096.
(assert (or (= encoded_slot EMPTY) (= encoded_slot TOMBSTONE)))
(check-sat)
(reset-assertions)

(assert (bvult slot MAX_REGIONS))
(assert (= encoded_slot (bvadd slot (_ bv1 32))))
(assert (= encoded_slot TOMBSTONE))
(check-sat)
(reset-assertions)

; Tombstones do not terminate probing. In a two-probe slice, a tombstone at the
; first slot and a live encoded slot at the second must be found.
(declare-const target_slot (_ BitVec 32))
(declare-const target_encoded (_ BitVec 32))
(assert (bvult target_slot MAX_REGIONS))
(assert (= target_encoded (bvadd target_slot (_ bv1 32))))
(define-fun probe0 () (_ BitVec 32) TOMBSTONE)
(define-fun probe1 () (_ BitVec 32) target_encoded)
(define-fun found_after_two_probes () Bool
  (ite (= probe0 EMPTY) false
  (ite (= probe0 TOMBSTONE)
       (and (not (= probe1 EMPTY)) (not (= probe1 TOMBSTONE)) (= probe1 target_encoded))
       (= probe0 target_encoded))))
(assert (not found_after_two_probes))
(check-sat)
(reset-assertions)

; Helper decay reclaim contract.
(assert occupied_before)
(assert ptr_matches)
(assert free_status_ok)
(assert (= state_before STATE_IDLE))
(assert (= kind_before KIND_HEAP))
(assert (= observers_before (_ bv0 32)))

(define-fun can_reclaim () Bool
  (and occupied_before
       ptr_matches
       free_status_ok
       (= state_before STATE_IDLE)
       (= kind_before KIND_HEAP)
       (= observers_before (_ bv0 32))))
(define-fun occupied_after () Bool
  (ite can_reclaim false occupied_before))
(define-fun state_after () (_ BitVec 2)
  (ite can_reclaim STATE_DECAYED state_before))
(define-fun helper_token_status_after () (_ BitVec 4)
  (ite (and occupied_after ptr_matches (= state_after STATE_IDLE)) OK NOT_FOUND))

(assert (or occupied_after (not (= helper_token_status_after NOT_FOUND))))
(check-sat)
