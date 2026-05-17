(set-logic QF_NIA)

(define-fun modm ((x Int)) Int (mod x 1000000007))
(define-fun payload_base ((i Int)) Int
  (ite (= (mod i 2) 0) 135 145))
(define-fun score ((i Int)) Int
  (+ (payload_base i) (mod i 7)))

; The Kain manual JSON row has two literal payloads:
; A: id 17 + count 42 + len("orbital") 7 + enabled score 17 + payload len 52 = 135
; B: id 23 + count 57 + len("lattice") 7 + enabled score 5 + payload len 53 = 145
; The payload selector repeats every 2 and round_mod repeats every 7, so the
; combined contribution repeats every 14 documents.
(define-fun period_sum () Int
  (+ (score 0) (score 1) (score 2) (score 3) (score 4) (score 5) (score 6)
     (score 7) (score 8) (score 9) (score 10) (score 11) (score 12) (score 13)))

; 250000 = 14 * 17857 + 2. The two-item remainder is score(0)+score(1) = 281.
(define-fun collapsed_checksum () Int
  (modm (+ (* 17857 period_sum) (score 0) (score 1))))

(assert (not (and
  (= period_sum 2002)
  (= collapsed_checksum 35749995))))
(check-sat)
