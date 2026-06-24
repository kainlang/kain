;; ownership-state-transition-table.smt2
;;
;; PROOF: State transition check table == branch ladders for all 4 operations
;;         (begin_observe, begin_collapse, begin_share, decay) and 5 states
;;         (IDLE, OBSERVED, COLLAPSED, SHARED, DECAYED).
;;
;; Result: UNSAT -- no counterexample exists.
;; The 2D lookup table and the original if-ladder always agree.
;;
;; This enables replacing 12+ if-statements across 4 functions with one
;; 4x5 lookup table, saving ~20 lines per call site.
;;
;; Domain: state in {0..4}, op in {0..3}

(set-logic QF_BV)

;; States: IDLE=0, OBSERVED=1, COLLAPSED=2, SHARED=3, DECAYED=4
;; Error codes (8-bit 2's complement): OK=0, ERR_OBSERVED=-4=0xFC,
;;   ERR_COLLAPSED=-5=0xFB, ERR_DECAYED=-6=0xFA

;; ---- PROPOSED: 4x5 lookup table ----
(define-fun table_transition ((op (_ BitVec 3)) (state (_ BitVec 3))) (_ BitVec 8)
  (let ((r_observe (ite (= state #b000) #x00     ;; IDLE -> OK
                  (ite (= state #b001) #x00       ;; OBSERVED -> OK
                  (ite (= state #b010) #xfb       ;; COLLAPSED -> ERR_COLLAPSED
                  (ite (= state #b011) #xfb       ;; SHARED -> ERR_COLLAPSED
                       #xfa))))))                 ;; DECAYED -> ERR_DECAYED
    (let ((r_collapse (ite (= state #b000) #x00   ;; IDLE -> OK
                     (ite (= state #b001) #xfc    ;; OBSERVED -> ERR_OBSERVED
                     (ite (= state #b010) #xfb    ;; COLLAPSED -> ERR_COLLAPSED
                     (ite (= state #b011) #xfb    ;; SHARED -> ERR_COLLAPSED
                          #xfa))))))              ;; DECAYED -> ERR_DECAYED
      (ite (= op #b000) r_observe        ;; begin_observe
      (ite (= op #b001) r_collapse       ;; begin_collapse
      (ite (= op #b010) r_collapse       ;; begin_share
                          r_collapse))))))   ;; decay


;; ---- REFERENCE: original branch ladders ----

;; begin_observe: DECAYED -> ERR_DECAYED, SHARED -> ERR_COLLAPSED,
;;                COLLAPSED -> ERR_COLLAPSED, else OK
(define-fun ref_observe ((state (_ BitVec 3))) (_ BitVec 8)
  (ite (= state #b100) #xfa
  (ite (= state #b011) #xfb
  (ite (= state #b010) #xfb
       #x00))))

;; begin_collapse/begin_share/decay: same as observe but also
;; rejects OBSERVED with ERR_OBSERVED
(define-fun ref_collapse ((state (_ BitVec 3))) (_ BitVec 8)
  (ite (= state #b100) #xfa
  (ite (= state #b011) #xfb
  (ite (= state #b010) #xfb
  (ite (= state #b001) #xfc
       #x00)))))


(declare-const state (_ BitVec 3))
(declare-const op (_ BitVec 3))

;; Reference dispatch matching the original code
(define-fun ref ((op (_ BitVec 3)) (state (_ BitVec 3))) (_ BitVec 8)
  (ite (= op #b000) (ref_observe state)
  (ite (= op #b001) (ref_collapse state)
  (ite (= op #b010) (ref_collapse state)
                    (ref_collapse state)))))

;; Constrain to valid ranges: op in [0,3], state in [0,4]
(assert (and (bvult op #b100) (bvult state #b101)))

;; Claim: table and reference produce the same result.
;; If unsat, the claim holds.
(assert (not (= (table_transition op state) (ref op state))))

(check-sat)
