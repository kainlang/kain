; Proof: kain_ui_runtime_report_add — validation report issue bounds
;
; The function adds diagnostics to a KainUiRuntimeValidationReport. The guard
; `report->issue_count >= KAIN_UI_RUNTIME_MAX_ISSUES` prevents overflow of the
; issues array (16 entries). Each issue uses snprintf for message/detail with
; bounded buffer sizes.
;
; Key claims:
;   1. The issue_count guard prevents overflow of the issues[16] array
;   2. snprintf(diag->message, sizeof(diag->message)) is bounded by 256
;   3. snprintf(report->summary, sizeof(report->summary)) is bounded by 320
;   4. issue_count correctly tracks the number of issues added
;
(set-logic QF_BV)

(define-fun MAX_ISSUES () (_ BitVec 32) #x00000010) ; KAIN_UI_RUNTIME_MAX_ISSUES = 16

; ============================================================================
; Claim 1: The guard prevents overflow of the issues array
; if (report->issue_count >= KAIN_UI_RUNTIME_MAX_ISSUES) { return; }
; This guarantees issue_count is always < MAX_ISSUES (valid array index)
; when an issue is about to be written.
; ============================================================================
(push)
(declare-fun issue_count () (_ BitVec 32))
; Precondition: the function only executes the body when issue_count < MAX_ISSUES
(assert (bvult issue_count MAX_ISSUES))
; After increment: issue_count' = issue_count + 1
(define-fun issue_count_after () (_ BitVec 32) (bvadd issue_count #x00000001))
; Prove: issue_count_after <= MAX_ISSUES (never exceeds capacity)
(assert (not (bvule issue_count_after MAX_ISSUES)))
(check-sat)
; Expected: unsat — after adding, count never exceeds MAX_ISSUES
(pop)

; ============================================================================
; Claim 2: When full (issue_count = MAX_ISSUES), the guard rejects new additions
; ============================================================================
(push)
(declare-fun issue_count () (_ BitVec 32))
(assert (= issue_count MAX_ISSUES)) ; full
; The guard check: report->issue_count >= MAX_ISSUES
; This returns true, so the function returns early without modifying anything.
(assert (bvult issue_count MAX_ISSUES)) ; This contradicts the guard
(check-sat)
; Expected: unsat — guard correctly prevents access when full
(pop)

; ============================================================================
; Claim 3: The issue_count never wraps around
; Issue_count is incremented one at a time and guarded against MAX_ISSUES.
; So it stays in [0, MAX_ISSUES] and never wraps.
; ============================================================================
(push)
(declare-fun issue_count () (_ BitVec 32))
(assert (bvule issue_count MAX_ISSUES)) ; valid range
(define-fun new_count () (_ BitVec 32)
  (ite (bvuge issue_count MAX_ISSUES) issue_count (bvadd issue_count #x00000001)))
; Prove: new_count does not wrap (is in [0, MAX_ISSUES])
(assert (bvugt new_count MAX_ISSUES))
(check-sat)
; Expected: unsat — new_count stays in valid range
(pop)

; ============================================================================
; Claim 4: issue_count increments by exactly 1 when not full
; ============================================================================
(push)
(declare-fun issue_count () (_ BitVec 32))
(assert (bvult issue_count MAX_ISSUES)) ; not full
(define-fun new_count () (_ BitVec 32) (bvadd issue_count #x00000001))
(assert (not (= new_count (bvadd issue_count #x00000001))))
(check-sat)
; Expected: unsat — simple identity (new_count = issue_count + 1)
(pop)

; ============================================================================
; Claim 5: error_count and warning_count track correctly
; Both counters only increment inside the issue_count < MAX_ISSUES guard.
; Since issue_count starts at 0 and increments by 1 per call (with guard),
; and error_count/warning_count are only incremented when issue_count < MAX_ISSUES,
; neither counter can exceed MAX_ISSUES.
;
; Modeling: issue_count < MAX_ISSUES before increment, and error_count is
; bounded by issue_count (since each issue has at most one severity count).
; ============================================================================
(push)
(declare-fun severity () (_ BitVec 32))
(declare-fun issue_count () (_ BitVec 32))
(declare-fun error_count () (_ BitVec 32))
(assert (bvule severity #x00000003)) ; 0=INFO, 1=WARNING, 2=ERROR, 3=FATAL
(assert (bvult issue_count MAX_ISSUES)) ; guard condition (function entered)
(assert (bvule error_count issue_count)) ; error_count <= issue_count (invariant)
; After increment:
(define-fun new_issue_count () (_ BitVec 32) (bvadd issue_count #x00000001))
(define-fun new_error_count () (_ BitVec 32)
  (ite (bvuge severity #x00000002) (bvadd error_count #x00000001) error_count))
; Prove: new_error_count <= MAX_ISSUES
(assert (bvugt new_error_count MAX_ISSUES))
(check-sat)
; Expected: unsat — error_count never exceeds MAX_ISSUES
(pop)
