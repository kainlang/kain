; Vulkain bridge clamp and fixed-budget invariants.
(set-logic QF_BV)

(declare-fun requested_width () (_ BitVec 32))
(declare-fun requested_height () (_ BitVec 32))
(declare-fun requested_budget () (_ BitVec 32))
(declare-fun requested_draw_vertices () (_ BitVec 32))
(declare-fun requested_instance_count () (_ BitVec 32))
(declare-fun shader_bytes () (_ BitVec 32))
(declare-fun requested_image_count () (_ BitVec 32))

(define-fun width_clamped () (_ BitVec 32)
  (ite (bvult requested_width #x00000001)
       #x00000001
       (ite (bvugt requested_width #x00004000) #x00004000 requested_width)))

(define-fun height_clamped () (_ BitVec 32)
  (ite (bvult requested_height #x00000001)
       #x00000001
       (ite (bvugt requested_height #x00004000) #x00004000 requested_height)))

(define-fun budget_clamped32 () (_ BitVec 32)
  (ite (bvult requested_budget #x00000001)
       #x00000001
       (ite (bvugt requested_budget #x00001000) #x00001000 requested_budget)))

(define-fun budget_clamped64 () (_ BitVec 64)
  ((_ zero_extend 32) budget_clamped32))

(define-fun draw_vertices_clamped32 () (_ BitVec 32)
  (ite (bvult requested_draw_vertices #x00000003)
       #x00000003
       (ite (bvugt requested_draw_vertices #x00001000) #x00001000 requested_draw_vertices)))

(define-fun draw_vertices_clamped64 () (_ BitVec 64)
  ((_ zero_extend 32) draw_vertices_clamped32))

(define-fun vertices_drawn () (_ BitVec 64)
  (bvmul budget_clamped64 draw_vertices_clamped64))

(define-fun instance_count_clamped32 () (_ BitVec 32)
  (ite (bvult requested_instance_count #x00000001)
       #x00000001
       (ite (bvugt requested_instance_count #x000f4240) #x000f4240 requested_instance_count)))

(define-fun instance_count_clamped64 () (_ BitVec 64)
  ((_ zero_extend 32) instance_count_clamped32))

(define-fun kloner_frame_vertices () (_ BitVec 64)
  (bvadd #x0000000000000006
         (bvmul #x0000000000000006 instance_count_clamped64)))

(define-fun kloner_total_vertices () (_ BitVec 64)
  (bvmul budget_clamped64 kloner_frame_vertices))

(define-fun safe_image_count () (_ BitVec 32)
  (ite (bvult requested_image_count #x00000001)
       #x00000001
       (ite (bvugt requested_image_count #x00000008) #x00000008 requested_image_count)))

(define-fun shader_word_count () (_ BitVec 32)
  (bvlshr shader_bytes #x00000002))

(push)
(assert (not (and (bvuge width_clamped #x00000001) (bvule width_clamped #x00004000))))
(check-sat)
(pop)

(push)
(assert (not (and (bvuge height_clamped #x00000001) (bvule height_clamped #x00004000))))
(check-sat)
(pop)

(push)
(assert (and (= (bvand shader_bytes #x00000003) #x00000000)
             (bvuge shader_bytes #x00000004)
             (bvule shader_bytes #x01000000)))
(assert (not (bvule shader_word_count #x00400000)))
(check-sat)
(pop)

(push)
(assert (not (and (bvuge draw_vertices_clamped32 #x00000003) (bvule draw_vertices_clamped32 #x00001000))))
(check-sat)
(pop)

(push)
(assert (not (bvule vertices_drawn #x0000000001000000)))
(check-sat)
(pop)

(push)
(assert (not (and (bvuge instance_count_clamped32 #x00000001) (bvule instance_count_clamped32 #x000f4240))))
(check-sat)
(pop)

(push)
(assert (not (bvule kloner_frame_vertices #x00000000005b8d86)))
(check-sat)
(pop)

(push)
(assert (not (bvule kloner_total_vertices #x00000005b8d86000)))
(check-sat)
(pop)

(push)
(assert (not (bvult kloner_total_vertices #x8000000000000000)))
(check-sat)
(pop)

(push)
(assert (not (and (bvuge safe_image_count #x00000001) (bvule safe_image_count #x00000008))))
(check-sat)
(pop)
