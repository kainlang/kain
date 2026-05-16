; actor-mailbox-tail-node-single-allocation-bounds.smt2
; Experimental proof for the candidate mailbox layout where a single heap block
; stores payload bytes first and the MessageNode metadata at the aligned tail.
; Claim: when the guarded allocation size is align_up(payload_size, node_align)
; plus node_size, the node offset never overlaps the payload and the total size
; stays large enough to cover the node tail.

(set-logic QF_BV)

(define-fun node_align () (_ BitVec 64) #x0000000000000008)
(define-fun node_align_mask () (_ BitVec 64) #x0000000000000007)
(define-fun node_size () (_ BitVec 64) #x0000000000000028)

(declare-fun payload_size () (_ BitVec 64))

(define-fun padded_payload () (_ BitVec 64)
  (bvadd payload_size node_align_mask))

(define-fun node_offset () (_ BitVec 64)
  (bvand padded_payload (bvnot node_align_mask)))

(define-fun allocation_size () (_ BitVec 64)
  (bvadd node_offset node_size))

; Guard shape the runtime candidate would use before malloc:
; no wrap on payload_size + node_align_mask
; no wrap on node_offset + node_size
(assert (bvule payload_size (bvsub #xffffffffffffffff node_align_mask)))
(assert (bvule node_offset (bvsub #xffffffffffffffff node_size)))

; Try to find a payload that violates the intended layout contract.
(assert
  (or
    (bvult node_offset payload_size)
    (bvult allocation_size node_offset)
    (bvult allocation_size payload_size)
    (not (= (bvand node_offset node_align_mask) #x0000000000000000))
    (bvuge (bvsub node_offset payload_size) node_align)))

(check-sat)
