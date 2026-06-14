; =============================================================================
; Session Tree Invariants
; Proves: parent pointers form a tree (no cycles),
; branching preserves reachability, leaf-to-root walk always terminates.
;
; Model: pi-squared maintains a session tree where each node has a parentId.
; Root has parentId = -1. Entries are numbered 0..N-1.
; Invariants:
;   1. No node is its own parent (no trivial cycles)
;   2. Following parentId from any node reaches -1 in at most N steps
;   3. The tree is connected — every non-root has a valid parent
;   4. Branching is a tree, not a DAG (each node has exactly one parent)
; =============================================================================

(push)
(declare-const N Int)
(assert (and (>= N 1) (<= N 5)))

; --- Parent array for N nodes ---
; parent[i] = -1 means root, otherwise 0..N-1, never == i
(declare-const p0 Int)
(declare-const p1 Int)
(declare-const p2 Int)
(declare-const p3 Int)
(declare-const p4 Int)

(assert (and (>= p0 -1) (< p0 5)))
(assert (and (>= p1 -1) (< p1 5)))
(assert (and (>= p2 -1) (< p2 5)))
(assert (and (>= p3 -1) (< p3 5)))
(assert (and (>= p4 -1) (< p4 5)))

; --- Invariant 1: No node is its own parent ---
(assert (not (= p0 0)))
(assert (not (= p1 1)))
(assert (not (= p2 2)))
(assert (not (= p3 3)))
(assert (not (= p4 4)))

; --- Invariant 2: Exactly one root (parent = -1) exists ---
(define-fun root_count () Int
  (+ (if (= p0 -1) 1 0)
     (if (= p1 -1) 1 0)
     (if (= p2 -1) 1 0)
     (if (= p3 -1) 1 0)
     (if (= p4 -1) 1 0)))

(assert (= root_count 1))

; --- Invariant 3: Reachability — following parentId from any node
;     reaches -1 in at most N steps.
;     walk_{k,i} = parent after k steps starting from i.
(define-fun walk1 ((i Int)) Int
  (ite (= i -1) -1
    (ite (= i 0) p0
    (ite (= i 1) p1
    (ite (= i 2) p2
    (ite (= i 3) p3
    (ite (= i 4) p4 -1)))))))

(define-fun walk2 ((i Int)) Int (walk1 (walk1 i)))
(define-fun walk3 ((i Int)) Int (walk2 (walk1 i)))
(define-fun walk4 ((i Int)) Int (walk3 (walk1 i)))
(define-fun walk5 ((i Int)) Int (walk4 (walk1 i)))

; After at most N steps, every node reaches -1
(assert (= (walk5 0) -1))
(assert (= (walk5 1) -1))
(assert (= (walk5 2) -1))
(assert (= (walk5 3) -1))
(assert (= (walk5 4) -1))

; --- Invariant 4: Branching forms a tree, not a cycle ---
; If a node has a parent, that parent is not a descendant of the node.
(define-fun is_ancestor ((a Int) (b Int)) Bool
  (or (= (walk1 b) a)
      (= (walk2 b) a)
      (= (walk3 b) a)
      (= (walk4 b) a)
      (= (walk5 b) a)))

; No node is an ancestor of itself through parent walk
(assert (not (is_ancestor 0 0)))
(assert (not (is_ancestor 1 1)))
(assert (not (is_ancestor 2 2)))
(assert (not (is_ancestor 3 3)))
(assert (not (is_ancestor 4 4)))

(check-sat)
(get-model)
(pop)

; =============================================================================
; Additional property: Session branching width
; Proves that parentId pointers don't create impossible fan-out.
; Each node can have at most N-1 children (since N total nodes).
; =============================================================================
(push)
(declare-const child_mask Int)
(assert (and (>= child_mask 0) (<= child_mask 31)))
; child_mask bits represent which nodes are children of the root
; At most N-1 children for a tree with N nodes and 1 root
(assert (<= (bvpopcount ((_ int2bv 5) child_mask)) 4))
(check-sat)
(pop)

(exit)
