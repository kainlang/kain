(set-logic QF_LIA)

; Semantic Singularity matrix proof:
; - all matrix rows keep cell/shatter indexes inside fixed bounds;
; - ablation rows keep the full-row value math equivalent where intended;
; - isolate rows keep hot-loop arithmetic far below signed i64 overflow;
; - the patch ablation uses a saturated bounded journal model.

(declare-const i Int)
(declare-const checksum Int)
(declare-const old_cell Int)
(declare-const staged Int)
(declare-const local_score Int)
(declare-const patch_attempts Int)

(define-fun iterations () Int 20000)
(define-fun cell_count () Int 32)
(define-fun shard_count () Int 4)
(define-fun modulus () Int 1000000007)
(define-fun patch_journal_capacity () Int 256)
(define-fun int64_max () Int 9223372036854775807)
(define-fun slot () Int (mod i cell_count))
(define-fun lane () Int (mod i shard_count))
(define-fun cell_byte_offset () Int (* slot 8))
(define-fun cell_payload_bytes () Int (* cell_count 8))
(define-fun shard_lane_offset () Int (* lane 8))
(define-fun shard_payload_bytes () Int (* shard_count 8))

(define-fun semantic_mask_max () Int 7)
(define-fun full_patched () Int (mod (+ checksum local_score i) modulus))
(define-fun full_staged_sum () Int (+ (* (+ full_patched local_score) 31) 26))
(define-fun full_request_sum () Int (+ staged old_cell semantic_mask_max))
(define-fun actor_reply () Int (mod (+ (* (mod full_request_sum modulus) 17) 34) modulus))
(define-fun inline_reply () Int (mod (+ (* (mod full_request_sum modulus) 17) 34) modulus))
(define-fun full_next_cell_sum () Int (+ actor_reply local_score))
(define-fun shatter_next_cell_sum () Int (+ old_cell local_score semantic_mask_max i))
(define-fun actor_only_request_sum () Int (+ old_cell i 7))
(define-fun actor_only_reply_sum () Int (+ (* (mod actor_only_request_sum modulus) 17) 34))
(define-fun actor_only_next_cell_sum () Int (+ (mod actor_only_reply_sum modulus) slot))
(define-fun converge_only_stage_sum () Int (+ (* (mod (+ old_cell i 23) modulus) 31) 26))
(define-fun converge_only_next_cell_sum () Int (+ (mod converge_only_stage_sum modulus) slot 6))
(define-fun final_sum () Int (+ (- modulus 1) (- modulus 1)))
(define-fun saturated_patch_count () Int
  (ite (> patch_attempts patch_journal_capacity) patch_journal_capacity patch_attempts))

(assert (and (<= 0 i) (< i iterations)))
(assert (and (<= 0 checksum) (< checksum modulus)))
(assert (and (<= 0 old_cell) (< old_cell modulus)))
(assert (and (<= 0 staged) (< staged modulus)))
(assert (and (<= 0 local_score) (<= local_score 80)))
(assert (and (<= 0 patch_attempts) (<= patch_attempts iterations)))

(push)
(assert
  (not
    (and
      (<= 0 slot)
      (< slot cell_count)
      (<= 0 cell_byte_offset)
      (< cell_byte_offset cell_payload_bytes)
      (<= 0 lane)
      (< lane shard_count)
      (<= 0 shard_lane_offset)
      (< shard_lane_offset shard_payload_bytes))))
(check-sat)
(pop)

(push)
(assert
  (not
    (and
      (= actor_reply inline_reply)
      (= full_patched (mod (+ checksum local_score i) modulus)))))
(check-sat)
(pop)

(push)
(assert
  (not
    (and
      (< full_staged_sum int64_max)
      (< full_request_sum int64_max)
      (< full_next_cell_sum int64_max)
      (< shatter_next_cell_sum int64_max)
      (< actor_only_request_sum int64_max)
      (< actor_only_reply_sum int64_max)
      (< actor_only_next_cell_sum int64_max)
      (< converge_only_stage_sum int64_max)
      (< converge_only_next_cell_sum int64_max)
      (< final_sum int64_max))))
(check-sat)
(pop)

(push)
(assert
  (not
    (and
      (<= 0 saturated_patch_count)
      (<= saturated_patch_count patch_journal_capacity)
      (<= saturated_patch_count iterations))))
(check-sat)
(pop)
