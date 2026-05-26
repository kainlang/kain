; KQuantum Vulkan bridge safety bounds.
; Each query proves the negated bad state is unreachable and should print unsat.

(set-logic ALL)

; kqv_particle_budget keeps every requested particle count inside [1, 1048576].
(push)
(declare-const requested_particles Int)
(define-fun particle_cap () Int 1048576)
(define-fun particle_budget () Int
  (ite (<= requested_particles 0)
       1
       (ite (> requested_particles particle_cap) particle_cap requested_particles)))
(assert
  (or (< particle_budget 1)
      (> particle_budget particle_cap)
      (and (<= requested_particles 0) (not (= particle_budget 1)))
      (and (> requested_particles particle_cap) (not (= particle_budget particle_cap)))
      (and (>= requested_particles 1)
           (<= requested_particles particle_cap)
           (not (= particle_budget requested_particles)))))
(check-sat)
(pop)

; g_particles_drawn cannot overflow i64 for any int32 frame_budget and capped particles.
(push)
(declare-const frame_budget Int)
(declare-const particle_count Int)
(define-fun i32_min () Int (- 2147483648))
(define-fun i32_max () Int 2147483647)
(define-fun i64_max () Int 9223372036854775807)
(define-fun effective_frames () Int (ite (<= frame_budget 0) 3600 frame_budget))
(define-fun particles_drawn () Int (* effective_frames particle_count))
(assert (and (<= i32_min frame_budget) (<= frame_budget i32_max)))
(assert (and (<= 1 particle_count) (<= particle_count 1048576)))
(assert
  (or (< effective_frames 1)
      (> effective_frames i32_max)
      (< particles_drawn 0)
      (> particles_drawn i64_max)
      (and (= frame_budget 96)
           (= particle_count 262144)
           (not (= particles_drawn 25165824)))))
(check-sat)
(pop)

; Cleanup loops stay inside fixed KQV_MAX_SWAPCHAIN_IMAGES arrays after hostile image counts.
(push)
(declare-const raw_image_count Int)
(declare-const cleanup_index Int)
(define-fun max_swapchain_images () Int 8)
(define-fun u32_max () Int 4294967295)
(define-fun safe_image_count () Int
  (ite (> raw_image_count max_swapchain_images) max_swapchain_images raw_image_count))
(assert (and (<= 0 raw_image_count) (<= raw_image_count u32_max)))
(assert (and (<= 0 cleanup_index) (< cleanup_index safe_image_count)))
(assert
  (or (< safe_image_count 0)
      (> safe_image_count max_swapchain_images)
      (>= cleanup_index max_swapchain_images)))
(check-sat)
(pop)

; Accepted SPIR-V byte payloads are non-empty, <= 16 MiB, and divisible into u32 words.
(push)
(declare-const shader_byte_count Int)
(declare-const shader_word_count Int)
(define-fun max_shader_bytes () Int 16777216)
(assert (> shader_byte_count 0))
(assert (<= shader_byte_count max_shader_bytes))
(assert (= (mod shader_byte_count 4) 0))
(assert (= shader_word_count (div shader_byte_count 4)))
(assert
  (or (< shader_word_count 1)
      (> shader_word_count 4194304)
      (not (= shader_byte_count (* shader_word_count 4)))))
(check-sat)
(pop)
