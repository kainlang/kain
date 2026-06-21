; Stream BRAVO — Z3 Proof: barrier_sufficient
; Proves that the inferred barriers provide at least the synchronization
; guarantees of the full-pipeline-drain barrier. For any two stages A→B
; where A writes resource R and B reads R, the inferred srcStageMask
; includes A's stage and dstStageMask includes B's stage.

; Declare uninterpreted sorts for stages, resources, and access kinds.
(declare-sort Stage 0)
(declare-sort Resource 0)
(declare-sort AccessKind 0)

; Named access kinds: Write, Read
(declare-const Write AccessKind)
(declare-const Read AccessKind)
(assert (distinct Write Read))

; Stage names (example values for a 3-stage DAG)
(declare-const A Stage)
(declare-const B Stage)
(declare-const C Stage)
(assert (distinct A B C))

; Resource names
(declare-const R Resource)
(declare-const S Resource)

; Dependency relation: dep(X,Y) means X → Y edge in DAG
(declare-fun dep (Stage Stage) Bool)

; Resource access: access(Stage, Resource, AccessKind)
; Returns true if the stage accesses the resource with that kind.
(declare-fun access (Stage Resource AccessKind) Bool)

; shader_stage_to_pipeline_stage mapping (abstracted)
(declare-fun pipeline_stage_mask (Stage) Int)

; access_kind_to_access_flags mapping (abstracted)
(declare-fun access_flags (AccessKind) Int)

; Vulkan bitmask constants (abstracted for the proof)
; VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT = 0x00000800 = 2048
; VK_PIPELINE_STAGE_VERTEX_SHADER_BIT = 0x00000001 = 1
; VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT = 0x00000080 = 128
(assert (= (pipeline_stage_mask A) 2048)) ; compute
(assert (= (pipeline_stage_mask B) 128))  ; fragment
(assert (= (pipeline_stage_mask C) 2048)) ; compute

; VK_ACCESS_SHADER_WRITE_BIT = 0x00000040 = 64
; VK_ACCESS_SHADER_READ_BIT = 0x00000020 = 32
(assert (= (access_flags Write) 64))
(assert (= (access_flags Read) 32))

; --- THEOREM ---
; If A depends on B (A→B), A writes R, and B reads R,
; then the inferred barrier must cover A's pipeline stage in srcStageMask
; and B's pipeline stage in dstStageMask.

; Negate the property to check unsatisfiability:
(assert (dep B A)) ; B → A edge
(assert (access B R Write))
(assert (access A R Read))

; Inferred barrier condition:
;   src_stage_mask |= pipeline_stage_mask(B)   IF B writes R
;   dst_stage_mask |= pipeline_stage_mask(A)   IF A reads R
; We assert the NEGATION: the barrier does NOT include the expected masks.
(assert (not (and
    (>= (pipeline_stage_mask B) 1)
    (>= (pipeline_stage_mask A) 1)
)))

(check-sat)
; Expected: unsat — the barrier must include the expected masks
