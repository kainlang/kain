; Z3 Proof: Hash table linear probe produces distinct indices
; Target: X:/runtime/native/src/ui/ui_system.c lines 42-56 (abi_ui_index_insert)
;
; The probe computes: (start_index + probe) & index_mask
; where index_mask = index_capacity - 1, and capacity is a power of two.
;
; Claim: For any start_index, and any distinct probe values p1, p2
; in [0, capacity-1], the resulting indices are distinct.
;
; Result: unsat (always distinct — no phantom hash collisions)

(set-logic QF_BV)

(declare-const start (_ BitVec 32))
(declare-const p1 (_ BitVec 32))
(declare-const p2 (_ BitVec 32))
(declare-const cap (_ BitVec 32))
(declare-const mask (_ BitVec 32))

; cap is a power of two, mask = cap - 1
(assert (= cap (bvadd mask #x00000001)))
(assert (not (= mask #x00000000)))
; Verify mask is all-ones for some lower bits
(assert (= (bvand mask (bvadd mask #x00000001)) #x00000000))

; p1 and p2 are distinct probes within capacity
(assert (bvult p1 cap))
(assert (bvult p2 cap))
(assert (not (= p1 p2)))

; Probe indices collide
(assert (= (bvand (bvadd start p1) mask)
           (bvand (bvadd start p2) mask)))

(check-sat)
; Expected: unsat
