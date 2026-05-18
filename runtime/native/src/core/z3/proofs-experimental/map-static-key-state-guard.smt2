(set-logic QF_LIA)

(declare-fun existing_state () Int)
(declare-fun incoming_state () Int)
(declare-fun matched_existing () Bool)

(assert (or (= existing_state 0) (= existing_state 1) (= existing_state 2)))
(assert (or (= incoming_state 1) (= incoming_state 2)))

; kain_map_insert_prehashed only mutates an existing entry's key state when an
; owned key is replaced by a borrowed-static literal.
(define-fun result_state () Int
  (ite matched_existing
    (ite (and (= existing_state 1) (= incoming_state 2)) 2 existing_state)
    incoming_state))

; map_free_elems only releases keys whose final state is owned.
(define-fun free_releases_key () Bool
  (= result_state 1))

(assert
  (or
    ; A static insertion or update must not leave the entry in the owned state.
    (and (= incoming_state 2) (= result_state 1))
    ; If an entry already carried a static literal, freeing the map must not
    ; call rc_release on that literal.
    (and (= existing_state 2) matched_existing free_releases_key)))

(check-sat)
