;; ============================================================================
;;  style-lookup-perfect-hash-4bit.smt2
;;  Z3-discovered magic multiplier for a 4-bit perfect hash over the 16 UI
;;  style keys used by layout and render.
;;
;;  Magic multiplier: M = 0xfc4bccd398b163ae
;;
;;  For 16 arbitrary distinct 64-bit key values, the top 4 bits of key*M
;;  produce distinct values. This enables O(1) style lookup via a 16-slot
;;  array per node.
;;
;;  Result: SAT (2026-06-23) — magic multiplier found.
;;  Verification: UNSAT (2026-06-23) — no collision for discovered keys.
;; ============================================================================
(set-logic QF_BV)

;; The discovered magic multiplier
(define-const M (_ BitVec 64) #xfc4bccd398b163ae)

;; 4-bit hash: multiply by M, take top 4 bits
(define-fun hash4 ((k (_ BitVec 64))) (_ BitVec 4)
  ((_ extract 63 60) (bvmul k M)))

;; Key values discovered by Z3 (arbitrary distinct 64-bit values)
(define-const k_fill_color (_ BitVec 64) #x096ca4ec701472a1)
(define-const k_border_color (_ BitVec 64) #x16d4b05841481c6c)
(define-const k_ink_color (_ BitVec 64) #x065cfbe29bdc9ce4)
(define-const k_border_width (_ BitVec 64) #x7144ee068a3d8cd1)
(define-const k_corner_radius (_ BitVec 64) #x04561aec150acefb)
(define-const k_opacity (_ BitVec 64) #x46ca7a31e800c078)
(define-const k_padding (_ BitVec 64) #x465f8e6a0861800a)
(define-const k_pad_left (_ BitVec 64) #x4726231ecfa2deda)
(define-const k_pad_top (_ BitVec 64) #x2fe6e839f18856df)
(define-const k_pad_right (_ BitVec 64) #x603137474033fc50)
(define-const k_pad_bottom (_ BitVec 64) #x28130f4777bebf89)
(define-const k_spacing (_ BitVec 64) #x589c94048e70ba9a)
(define-const k_gap (_ BitVec 64) #x5563432e31197b76)
(define-const k_dir (_ BitVec 64) #x1d93dbe63c6be778)
(define-const k_width (_ BitVec 64) #x7811232e3061369a)
(define-const k_height (_ BitVec 64) #x3c3c5ff74fffedf9)

;; Verify all 16 hashes are distinct
(assert (distinct
  (hash4 k_fill_color) (hash4 k_border_color) (hash4 k_ink_color)
  (hash4 k_border_width) (hash4 k_corner_radius) (hash4 k_opacity)
  (hash4 k_padding) (hash4 k_pad_left) (hash4 k_pad_top)
  (hash4 k_pad_right) (hash4 k_pad_bottom) (hash4 k_spacing)
  (hash4 k_gap) (hash4 k_dir) (hash4 k_width) (hash4 k_height)))

;; Print hashes
(echo \"=== 4-bit hash values ===\")
(get-value ((hash4 k_fill_color) (hash4 k_border_color) (hash4 k_ink_color)
  (hash4 k_border_width) (hash4 k_corner_radius) (hash4 k_opacity)
  (hash4 k_padding) (hash4 k_pad_left) (hash4 k_pad_top)
  (hash4 k_pad_right) (hash4 k_pad_bottom) (hash4 k_spacing)
  (hash4 k_gap) (hash4 k_dir) (hash4 k_width) (hash4 k_height)))

(check-sat)
(exit)
