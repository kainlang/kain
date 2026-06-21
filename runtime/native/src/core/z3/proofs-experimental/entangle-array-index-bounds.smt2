; entangle-array-index-bounds.smt2
;
; Claim: All array accesses to g_kain_entangle_bindings[index] are always
; within bounds [0, 127].
;
; entangle_registry_register: uses g_kain_entangle_binding_count as index
; after guard ensures count < 128. The binding is stored at slot count.
;
; entangle_registry_get: uses index parameter after guard ensures
; index < g_kain_entangle_binding_count (which is always <= 128).
;
; Key invariant: binding_count in [0, ENTANGLE_MAX_BINDINGS] and all array
; accesses use indices < binding_count.

(set-logic QF_BV)

(define-const MAX_BINDINGS (_ BitVec 64) #x0000000000000080) ; 128

; Invariant 1: After register guard, count < 128
(declare-const count (_ BitVec 64))

; The guard condition in entangle_registry_register is:
; if (g_kain_entangle_binding_count >= ENTANGLE_MAX_BINDINGS) return -3;
; So after the guard passes: count < 128
(assert (bvult count MAX_BINDINGS))

; The access: g_kain_entangle_bindings[count] = binding;
; This is always valid since count in [0, 127]
(push)
(assert (not (bvult count MAX_BINDINGS)))
(check-sat)
(pop)
; unsat = count < 128 guaranteed

; Invariant 2: count+1 <= 128 after increment
; The assignment g_kain_entangle_binding_count += 1 happens after the store.
; Next call will see count' = count + 1.
; If count was 127 before increment, then count' = 128, and the NEXT
; call will fail the guard. This is correct behavior.
(define-fun next_count () (_ BitVec 64) (bvadd count #x0000000000000001))
(push)
(assert (not (bvule next_count MAX_BINDINGS)))
(check-sat)
(pop)
; unsat = count+1 <= 128

; Invariant 3: entangle_registry_get bounds check is correct
; if (index >= g_kain_entangle_binding_count) return -1;
; When index < count, the access g_kain_entangle_bindings[index] is valid.
(declare-const index (_ BitVec 64))
(assert (bvult index count))
(assert (bvult count MAX_BINDINGS))

; index < count < 128, so index in [0, 126], always valid array index
(push)
(assert (not (bvult index MAX_BINDINGS)))
(check-sat)
(pop)
; unsat = index < 128 guaranteed

; Invariant 4: Post guard, pre_count in [0,127] -> post_count in [1,128]
; The guard is: count >= 128 -> fail
; So stored index = pre_count where pre_count in [0, 127]
(declare-const pre_count (_ BitVec 64))
(assert (bvult pre_count MAX_BINDINGS))    ; pre_count in [0, 127]
(define-fun post_count () (_ BitVec 64) (bvadd pre_count #x0000000000000001))
(push)
(assert (not (bvule post_count MAX_BINDINGS)))
(check-sat)
(pop)
; unsat = post_count <= 128
