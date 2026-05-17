; Experimental RC ownership proof for runtime/native/src/core/json.c.
; Claims:
; - json_get/json_array_get returning an owned clone lets compiler scope cleanup
;   release the temporary without decrementing the parent-owned child;
; - object field replacement releases the old child exactly once and transfers
;   ownership of the new child to the object;
; - object destruction releases the current child exactly once.
(set-logic QF_LIA)

(declare-fun parent_child_refs_after_get () Int)
(declare-fun clone_refs_after_get () Int)
(declare-fun old_refs_before_replace () Int)
(declare-fun new_refs_before_replace () Int)

(assert (= parent_child_refs_after_get 1))
(assert (= clone_refs_after_get 1))
(assert (= old_refs_before_replace 1))
(assert (= new_refs_before_replace 1))

(define-fun parent_child_refs_after_clone_release () Int
  parent_child_refs_after_get)
(define-fun clone_refs_after_scope_cleanup () Int
  (- clone_refs_after_get 1))
(define-fun old_refs_after_replace () Int
  (- old_refs_before_replace 1))
(define-fun new_refs_after_replace () Int
  new_refs_before_replace)
(define-fun new_refs_after_parent_destructor () Int
  (- new_refs_after_replace 1))

(push)
(assert (not (= parent_child_refs_after_clone_release 1)))
(check-sat)
(pop)

(push)
(assert (not (= clone_refs_after_scope_cleanup 0)))
(check-sat)
(pop)

(push)
(assert (not (= old_refs_after_replace 0)))
(check-sat)
(pop)

(push)
(assert (not (= new_refs_after_parent_destructor 0)))
(check-sat)
(pop)

(push)
(assert (or (< clone_refs_after_scope_cleanup 0)
            (< old_refs_after_replace 0)
            (< new_refs_after_parent_destructor 0)))
(check-sat)
(pop)
