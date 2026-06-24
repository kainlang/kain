; Proof: Packed 64-bit+64-bit+32-bit comparison == 4 separate field comparisons
;
; KainActorRef layout (24 bytes on x86_64):
;   offset 0:  actor_id        (uint64_t, 8 bytes)
;   offset 8:  generation      (uint32_t, 4 bytes)
;   offset 12: execution_class (uint32_t, 4 bytes)
;   offset 16: locality_class  (uint32_t, 4 bytes)
;
; Since generation and execution_class are adjacent at offsets 8 and 12,
; they can be loaded as a single 64-bit word and compared directly,
; reducing 4 field comparisons to 3 wider comparisons:
;   1. actor_id (uint64_t)
;   2. packed generation<<32 | execution_class (uint64_t)
;   3. locality_class (uint32_t)

(set-logic QF_BV)

(declare-const a_actor_id (_ BitVec 64))
(declare-const a_generation (_ BitVec 32))
(declare-const a_execution_class (_ BitVec 32))
(declare-const a_locality_class (_ BitVec 32))

(declare-const b_actor_id (_ BitVec 64))
(declare-const b_generation (_ BitVec 32))
(declare-const b_execution_class (_ BitVec 32))
(declare-const b_locality_class (_ BitVec 32))

; Reference: 4 separate field comparisons
(define-fun reference () (_ BitVec 1)
  (ite (and (= a_actor_id b_actor_id)
            (= a_generation b_generation)
            (= a_execution_class b_execution_class)
            (= a_locality_class b_locality_class))
    (_ bv1 1) (_ bv0 1)))

; Packed: pack generation and execution_class into one uint64_t
; generation is at lower address (offset 8), execution_class at offset 12
; On little-endian x86_64, a 64-bit load at offset 8 gives:
;   result[63:32] = execution_class (originally at [12])
;   result[31:0]  = generation (originally at [8])
;
; But for equality comparison, the relative packing doesn't matter.
; It only matters that both A and B are packed identically.
(define-fun a_packed_ge () (_ BitVec 64)
  (bvor (bvshl ((_ zero_extend 32) a_generation) (_ bv32 64))
        ((_ zero_extend 32) a_execution_class)))

(define-fun b_packed_ge () (_ BitVec 64)
  (bvor (bvshl ((_ zero_extend 32) b_generation) (_ bv32 64))
        ((_ zero_extend 32) b_execution_class)))

(define-fun candidate () (_ BitVec 1)
  (ite (and (= a_actor_id b_actor_id)
            (= a_packed_ge b_packed_ge)
            (= a_locality_class b_locality_class))
    (_ bv1 1) (_ bv0 1)))

(assert (not (= reference candidate)))
(check-sat)
; unsat = packed comparison is equivalent to field-by-field comparison
