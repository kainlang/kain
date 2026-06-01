; Minimal solver witnesses for the PTX storage-buffer sidecar/runtime size mismatch.
;
; Codegen truth:
; - crates/gpu/src/codegen_ptx.rs lowers Bool storage lanes as 4-byte u32 accesses.
; - crates/gpu/src/codegen_ptx.rs lowers Vec3/IVec3/UVec3 storage lanes with 16-byte stride.
;
; Runtime/sidecar metadata drift:
; - crates/driver/src/compute_residency.rs uses bool = 1 byte and vec3 = 12 bytes.
; - crates/gpu-runtime/src/nvidia_ptx.rs uses the same bool = 1 and vec3 = 12 table.
; - crates/gpu-runtime/src/executor.rs duplicates the same table.
;
; Witnesses:
; - Bool count = 1 already under-allocates (1 byte vs 4 bytes accessed).
; - Vec3 count = 2 under-allocates (24 bytes vs 28-byte exclusive end).

(set-logic QF_NIA)

(define-fun bool_count () Int 1)
(define-fun runtime_bool_bytes () Int (* 1 bool_count))
(define-fun ptx_bool_required_exclusive_end () Int (+ (* 4 (- bool_count 1)) 4))

(define-fun vec3_count () Int 2)
(define-fun runtime_vec3_bytes () Int (* 12 vec3_count))
(define-fun ptx_vec3_required_exclusive_end () Int (+ (* 16 (- vec3_count 1)) 12))

(assert (< runtime_bool_bytes ptx_bool_required_exclusive_end))
(assert (< runtime_vec3_bytes ptx_vec3_required_exclusive_end))

(check-sat)
(get-model)
