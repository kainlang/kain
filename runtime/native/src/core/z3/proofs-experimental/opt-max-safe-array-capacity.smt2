; =============================================================================
; Optimization: Maximum Safe Capacity for array_new / array_push
;
; Model: arr->data = malloc((size_t)arr->cap * sizeof(long long))
;   sizeof(long long) = 8
;   arr->cap is signed long long, clamped >= 4
;
; Constraint: (size_t)cap * 8 must NOT overflow uint64_t (SIZE_MAX)
;   i.e., cap * 8 < 2^64  →  cap < 2^61
;
; Objective: MAXIMIZE cap such that the multiplication does NOT wrap.
;
; This tells us the absolute maximum capacity before array_new's malloc
; receives a wrapped size and returns an undersized buffer.
;
; Use: Z3's optimization engine finds the largest safe cap automatically.
; =============================================================================
(set-option :opt.priority lex)
(set-logic QF_LIA)

(declare-const cap Int)

; arr->cap is clamped >= 4
(assert (>= cap 4))

; (size_t)cap * 8 must not overflow uint64_t
; We represent this as: cap * 8 < 2^64
(assert (< (* cap 8) 18446744073709551616))

; MAXIMIZE the capacity
(maximize cap)

(check-sat)
(get-model)
(get-objectives)

; Expected: cap = 2305843009213693951 (= 0x1FFFFFFFFFFFFFFF)
; This is the largest signed long long >= 4 that does NOT overflow
; when multiplied by sizeof(long long) = 8.
;
; cap_max = floor((2^64 - 1) / 8) = 2305843009213693951
; In hex: 0x1FFFFFFFFFFFFFFF
;
; Any cap > this value causes (size_t)cap * 8 to wrap modulo 2^64.
; The runtime needs to guard array_new against cap > 0x1FFFFFFFFFFFFFFF
; before calling malloc, or switch to a growth policy that never reaches
; this threshold.
