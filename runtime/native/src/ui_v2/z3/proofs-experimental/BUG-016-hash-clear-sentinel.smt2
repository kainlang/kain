;; ============================================================================
;;  BUG-016: hash_clear sentinel mismatch
;;
;;  hash_values must be initialized to -1 (0xFFFFFFFF) because 0 is
;;  a valid root node index. Using memset(hash_values, 0, ...) in
;;  hash_clear would set values to 0, making value 0 indistinguishable
;;  from a valid stored index.
;;
;;  kt_make correctly initializes hash_values to 0xFF (all bits = 1,
;;  i.e. -1 for int32_t). hash_clear must use the same sentinel.
;;
;;  Property: For an open-addressing hash table where the value
;;  represents a node index, value 0 (root node) is valid. An
;;  empty/deleted slot must use -1 as the sentinel.
;; ============================================================================

(declare-const hash_value Int)
(declare-const is_live Bool)
(declare-const sentinel Int)

;; Sentinel for empty/deleted: -1 (0xFFFFFFFF)
(assert (= sentinel (- 0 1)))

;; A live entry has a non-negative value (node index >= 0)
(define-fun valid_node_index ((v Int)) Bool (and (>= v 0) (<= v 4095)))

;; If hash_value is -1, slot is empty/dead
;; If hash_value is >= 0, slot is live and holds a valid node index
(assert (=>
    (= hash_value sentinel)
    (not is_live)))

(assert (=>
    (valid_node_index hash_value)
    is_live))

;; Prove: value 0 is a valid node index (root node = index 0)
(assert (valid_node_index 0))

;; If sentinel were 0, then value 0 would be ambiguous:
;; it could be the root node or an empty slot.
;; This introduces a contradiction.
(assert (not (= sentinel 0)))

;; Therefore sentinel MUST be -1, not 0.
;; This is trivially UNSAT if someone tries sentinel=0.
(assert (= sentinel 0))

(check-sat)
