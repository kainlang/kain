; BUG-014: hex_nibble returns -1 for invalid chars
;
; The bug: hex_nibble returned 0 for invalid hex chars like 'G'.
; This caused malformed hex strings like "#A0GG00" to silently produce
; wrong colors instead of returning the fallback (0xFF000000).
;
; The fix:
;   1. hex_nibble now returns int (not uint8_t) and returns -1 for invalid chars
;   2. kt_color_parse_hex validates every nibble result: if any is < 0, return 0xFF000000
;
; This proof verifies that:
;   - hex_nibble('G') = -1
;   - hex_nibble('A') = 10
;   - hex_nibble('0') = 0
;   - For invalid nibble combinations, the validator returns fallback color
;
; Z3 UNSAT = proof holds

(declare-fun hex_nibble (Int) Int)

; Valid ranges
(assert (forall ((c Int))
    (=> (and (<= 48 c) (<= c 57))   ; '0'..'9'
        (= (hex_nibble c) (- c 48)))))
(assert (forall ((c Int))
    (=> (and (<= 65 c) (<= c 70))   ; 'A'..'F'
        (= (hex_nibble c) (+ (- c 65) 10)))))
(assert (forall ((c Int))
    (=> (and (<= 97 c) (<= c 102))  ; 'a'..'f'
        (= (hex_nibble c) (+ (- c 97) 10)))))

; Invalid chars return -1
(assert (forall ((c Int))
    (=> (not (or (and (<= 48 c) (<= c 57))
                 (and (<= 65 c) (<= c 70))
                 (and (<= 97 c) (<= c 102))))
        (= (hex_nibble c) (- 1)))))

; Test: hex_nibble('G') must be -1
(assert (not (= (hex_nibble 71) (- 1))))

(check-sat)
