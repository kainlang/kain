(set-logic QF_LIA)

; Semantic Singularity benchmark proof sketch:
; every hot-loop memory/shatter index remains inside its authored lane bounds,
; and every checksum arithmetic expression stays far below signed i64 overflow.

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
(define-fun patched_sum () Int (+ checksum local_score i))
(define-fun normalize_sum () Int (+ (* (+ (mod patched_sum modulus) local_score) 31) 7))
(define-fun request_sum () Int (+ staged old_cell 7))
(define-fun reply_sum () Int (+ (* (mod request_sum modulus) 17) 34))
(define-fun next_cell_sum () Int (+ (mod reply_sum modulus) local_score))
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
      (< patched_sum int64_max)
      (< normalize_sum int64_max)
      (< request_sum int64_max)
      (< reply_sum int64_max)
      (< next_cell_sum int64_max)
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
