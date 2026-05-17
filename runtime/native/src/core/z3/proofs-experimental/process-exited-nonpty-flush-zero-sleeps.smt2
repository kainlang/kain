(set-logic QF_LIA)

; The post-exit flush policy keeps the sleepy multi-attempt path only for PTY
; handles. Normal anonymous-pipe process capture performs no Sleep calls in
; abi_process_flush_exited_output; abi_process_wait immediately performs the
; deterministic pump after this helper returns.

(declare-const is_pty Int)
(declare-const old_attempt0_had_data Int)
(declare-const new_flush_sleep_ms Int)

(assert (or (= is_pty 0) (= is_pty 1)))
(assert (or (= old_attempt0_had_data 0) (= old_attempt0_had_data 1)))

; In the benchmark-shaped old path, each exited child emits one stdout chunk.
; The first flush attempt has data, then the policy sleeps once, observes quiet
; attempts 1 and 2, and sleeps again before breaking: two sleeps.
(assert (= old_attempt0_had_data 1))

; New policy: if the handle is not a PTY, the flush helper returns before any
; Sleep. PTY remains on the old compatibility path.
(assert (= new_flush_sleep_ms (ite (= is_pty 0) 0 30)))

; Prove the non-PTY path cannot retain a positive post-exit flush sleep.
(assert (= is_pty 0))
(assert (> new_flush_sleep_ms 0))

(check-sat)
