; If the first open succeeds, then the eager parent-dir path would also succeed
; and the eventual post-parent-creation open would succeed too. Under those
; assumptions, "open first, then create parent dirs and retry once" is
; equivalent to the older eager create-parent-dirs path.

(set-logic QF_UF)

(declare-const open_now Bool)
(declare-const create_parent_dirs_success Bool)
(declare-const open_after_parent_dirs Bool)

(define-fun eager_success () Bool
  (and create_parent_dirs_success open_after_parent_dirs))

(define-fun retry_success () Bool
  (or open_now
      (and (not open_now) create_parent_dirs_success open_after_parent_dirs)))

(assert (=> open_now create_parent_dirs_success))
(assert (=> open_now open_after_parent_dirs))
(assert (not (= eager_success retry_success)))

(check-sat)
