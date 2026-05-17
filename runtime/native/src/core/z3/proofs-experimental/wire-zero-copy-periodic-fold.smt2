; Proof backbone for runtime/native/src/core/wire.c.
;
; The native zero_copy_binary_wire lane folds a scalar packed-record loop into:
;   full_period_sum(block) + scalar_tail(remainder)
;
; This proof checks the arithmetic obligations that make the fold legal:
; - packed fields fit in their lanes and decode exactly;
; - word2's high lane decodes seq % 97 because the low payload/flag lane is
;   strictly below 2048;
; - the chosen period is divisible by the packet, payload, and seqmod periods;
; - the word3 linear shift constant is exactly (4096 * period) mod 1000003;
; - the baked wrap-count table rule is an exact expansion of modular addition.
(set-logic QF_LIA)

(define-fun PERIOD () Int 397312)
(define-fun WORD3_MOD () Int 1000003)
(define-fun WORD3_SHIFT () Int 385071)

(declare-const seq Int)
(declare-const version Int)
(declare-const kind Int)
(declare-const flags Int)
(declare-const route Int)
(declare-const payload Int)
(declare-const seq_mod Int)
(declare-const payload_mod Int)
(declare-const base Int)
(declare-const shift Int)

(define-fun word0 () Int
  (+ (* seq 4096) (* kind 256) (* flags 16) version))
(define-fun word1 () Int
  (+ (* payload 128) route))
(define-fun word2 () Int
  (+ (* seq_mod 2048) (* payload_mod 16) flags))
(define-fun wrap_fold () Int
  (- (+ base shift)
     (* WORD3_MOD (ite (and (> shift 0) (>= base (- WORD3_MOD shift))) 1 0))))

(assert (and
  (>= seq 0)
  (>= version 1) (<= version 4)
  (>= kind 0) (<= kind 7)
  (>= flags 0) (<= flags 15)
  (>= route 0) (<= route 63)
  (>= payload 0) (<= payload 4095)
  (>= seq_mod 0) (<= seq_mod 96)
  (>= payload_mod 0) (<= payload_mod 126)
  (>= base 0) (< base WORD3_MOD)
  (>= shift 0) (< shift WORD3_MOD)))

(assert
  (or
    ; Period constants used by the C fold.
    (not (= PERIOD (* 4096 97)))
    (not (= (mod PERIOD 64) 0))
    (not (= (mod (div PERIOD 64) 16) 0))
    (not (= (mod PERIOD 4096) 0))
    (not (= (mod PERIOD 97) 0))
    (not (= WORD3_SHIFT (mod (* 4096 PERIOD) WORD3_MOD)))

    ; Header/body lane roundtrip.
    (not (= (mod word0 16) version))
    (not (= (mod (div word0 16) 16) flags))
    (not (= (mod (div word0 256) 16) kind))
    (not (= (div word0 4096) seq))
    (not (= (mod word1 128) route))
    (not (= (div word1 128) payload))
    (not (= (div word2 2048) seq_mod))

    ; Modular wrap expansion used by the table counts.
    (not (= wrap_fold (mod (+ base shift) WORD3_MOD)))))

(check-sat)
