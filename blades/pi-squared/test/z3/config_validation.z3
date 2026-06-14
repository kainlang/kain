; =============================================================================
; Config Validation
; Proves: valid_thinking_level accepts known levels, rejects unknown;
; config merge is associative and idempotent.
;
; Model: pi-squared has thinking_level: "none" | "standard" | "deep" | "insight"
; Config merge combines two config dicts, with second value winning on conflict.
; =============================================================================

; --- Part 1: Thinking Level Domain ---
(push)
(declare-datatypes () ((ThinkingLevel
  NoneLevel StandardLevel DeepLevel InsightLevel InvalidLevel)))

; Valid thinking levels are exactly the four known levels
(define-fun is_valid ((l ThinkingLevel)) Bool
  (or (= l NoneLevel) (= l StandardLevel)
      (= l DeepLevel) (= l InsightLevel)))

; Check that the known levels are valid
(assert (is_valid NoneLevel))
(assert (is_valid StandardLevel))
(assert (is_valid DeepLevel))
(assert (is_valid InsightLevel))

; Check that InvalidLevel is NOT valid
(assert (not (is_valid InvalidLevel)))

(check-sat)
(pop)

; --- Part 2: Config Merge Associativity ---
; merge(a, merge(b, c)) == merge(merge(a, b), c)
(push)
(declare-datatypes () ((Field
  (mk-field (name Int) (value Int)))))

; Config is a tuple of 4 fields
(declare-const a0 Int) (declare-const a1 Int) (declare-const a2 Int) (declare-const a3 Int)
(declare-const b0 Int) (declare-const b1 Int) (declare-const b2 Int) (declare-const b3 Int)
(declare-const c0 Int) (declare-const c1 Int) (declare-const c2 Int) (declare-const c3 Int)

; Merge: for each field, if b's value is non-negative use it, else use a's
(define-fun merge_0 ((x Int) (y Int)) Int (ite (>= y 0) y x))
(define-fun merge_1 ((x Int) (y Int)) Int (ite (>= y 0) y x))
(define-fun merge_2 ((x Int) (y Int)) Int (ite (>= y 0) y x))
(define-fun merge_3 ((x Int) (y Int)) Int (ite (>= y 0) y x))

; Associativity: merge(a, merge(b, c)) == merge(merge(a, b), c)
; Field 0
(assert (not (=
  (merge_0 a0 (merge_0 b0 c0))
  (merge_0 (merge_0 a0 b0) c0))))

; Field 1
(assert (not (=
  (merge_1 a1 (merge_1 b1 c1))
  (merge_1 (merge_1 a1 b1) c1))))

; Field 2
(assert (not (=
  (merge_2 a2 (merge_2 b2 c2))
  (merge_2 (merge_2 a2 b2) c2))))

; Field 3
(assert (not (=
  (merge_3 a3 (merge_3 b3 c3))
  (merge_3 (merge_3 a3 b3) c3))))

; If SAT, associativity fails for this field definition.
; We know merge defined as (ite (>= y 0) y x) IS associative —
; this should be UNSAT, confirming associativity.
(check-sat)
(pop)

; --- Part 3: Idempotency ---
; merge(a, a) == a
(push)
(declare-const d0 Int) (declare-const d1 Int) (declare-const d2 Int) (declare-const d3 Int)

(assert (not (and
  (= (merge_0 d0 d0) d0)
  (= (merge_1 d1 d1) d1)
  (= (merge_2 d2 d2) d2)
  (= (merge_3 d3 d3) d3))))

; UNSAT confirms idempotency
(check-sat)
(pop)

; --- Part 4: Default preservation ---
; merge(a, empty) == a where empty = -1 for all fields (negative = unset)
(push)
(assert (not (and
  (= (merge_0 d0 (- 1)) d0)
  (= (merge_1 d1 (- 1)) d1)
  (= (merge_2 d2 (- 1)) d2)
  (= (merge_3 d3 (- 1)) d3))))

; UNSAT confirms empty merge is identity
(check-sat)
(pop)

(exit)
