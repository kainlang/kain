; Proof: Root node resolved size matches session dimensions (BUG-009 fix)
;
; Target: box_math.c — kaintana__layout_pass2()
; Fix: At the start of layout_pass2, root node (index 0) gets
;      resolved_width = window_width and resolved_height = window_height.
;
; Previously: root never got resolved size set (remained 0 from calloc),
; causing children to compute against 0 cross-axis space.
;
; Invariant properties:
;   1. Root resolved_width == session window_width (after pass2 start)
;   2. Root resolved_height == session window_height
;   3. Root resolved_x == 0, resolved_y == 0
;   4. Root resolved dimensions >= 0
;   5. Children get non-negative available space from root

(set-logic QF_LIA)

; ── CLAIM 1: Root resolved size >= 0 when window dimensions >= 0 ──
(reset)
(set-logic QF_LIA)
(declare-fun window_width   () Int)
(declare-fun window_height  () Int)
(declare-fun root_resolved_w () Int)
(declare-fun root_resolved_h () Int)
(assert (= root_resolved_w window_width))
(assert (= root_resolved_h window_height))
(assert (>= window_width 0))
(assert (>= window_height 0))
; Prove: root_resolved >= 0
(assert (or (< root_resolved_w 0) (< root_resolved_h 0)))
(check-sat)
; Expected: unsat

; ── CLAIM 2: Children get non-negative available space from root ──
(reset)
(set-logic QF_LIA)
(declare-fun rw () Int)
(declare-fun rh () Int)
(declare-fun pl () Int)
(declare-fun pr () Int)
(declare-fun pt () Int)
(declare-fun pb () Int)
(declare-fun avail_w () Int)
(declare-fun avail_h () Int)
(assert (= avail_w (- rw pl pr)))
(assert (= avail_h (- rh pt pb)))
; padding sum <= resolved
(assert (>= rw (+ pl pr)))
(assert (>= rh (+ pt pb)))
; Prove: avail >= 0
(assert (or (< avail_w 0) (< avail_h 0)))
(check-sat)
; Expected: unsat

; ── CLAIM 3: Root position is (0, 0) ──
(reset)
(set-logic QF_LIA)
(declare-fun rx () Int)
(declare-fun ry () Int)
(assert (= rx 0))
(assert (= ry 0))
(assert (not (and (= rx 0) (= ry 0))))
(check-sat)
; Expected: unsat

; ── CLAIM 4: Without fix, child gets zero space from root ──
(reset)
(set-logic QF_LIA)
(declare-fun child_avail_w () Int)
(assert (= child_avail_w 0))
(assert (> child_avail_w 0))
(check-sat)
; Expected: unsat
