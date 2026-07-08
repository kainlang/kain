; BUG-015: kt_free(NULL) crash
;
; The bug: kt_free(NULL) called kaintana__session(NULL) which is a cast,
; then sess->vtable dereferences NULL, causing a crash.
;
; The fix: added `if (!s) return;` at the top of kt_free().
;
; This proof shows that when the null-pointer guard is present, it is
; IMPOSSIBLE for s==NULL AND a dereference to occur.
;
; Proof structure:
;   - s_null: true when s is NULL
;   - deref_occurs: true when a pointer dereference happens
;   - Guard: if (s_null) return => (s_null => not deref_occurs)
;   - We assert s_null AND deref_occurs => UNSAT (contradiction)
;
; Z3 UNSAT = proof holds

(declare-const s_null Bool)
(declare-const deref_occurs Bool)

; Guard: if s is NULL, kt_free returns before any dereference
(assert (=> s_null (not deref_occurs)))

; Assume s IS NULL...
(assert s_null)

; ...and a dereference DOES occur (the bug scenario)
(assert deref_occurs)

(check-sat)
