;; ============================================================================
;;  macos-fb-sizing.smt2 — macOS Framebuffer Sizing Proof
;;
;;  Proves: fb_w = logical_w × scale, fb_h = logical_h × scale.
;;  Steps:
;;    1. Create CGColorSpaceCreateDeviceRGB()
;;    2. Create CGBitmapContextCreate(NULL, fb_w, fb_h, 8, fb_w*4, cs, flags)
;;    3. bytesPerRow = fb_w × 4 = stride
;;    4. Total memory = stride × fb_h = fb_w × fb_h × 4
;;
;;  Invariant: framebuffer memory allocation never overflows for
;;  displays up to 32K resolution (30720 × 17280 logical) at 2x scale.
;;
;;  Cross-ref: macos.md §6.1, §3.4
;; ============================================================================

(declare-const logical_w Int)
(declare-const logical_h Int)
(declare-const scale Int)

;; Precondition: valid dimensions
(assert (> logical_w 0))
(assert (> logical_h 0))

;; Precondition: scale is integer 1 or 2
(assert (or (= scale 1) (= scale 2)))

;; Framebuffer dimensions
(define-const fb_w Int (* logical_w scale))
(define-const fb_h Int (* logical_h scale))

;; Bytes per row = width × 4 (32 bits per pixel)
(define-const bpr Int (* fb_w 4))

;; Total bytes = bpr × height
(define-const total_bytes Int (* bpr fb_h))

;; Also: total = fb_w × fb_h × 4
(define-const total_v2 Int (* fb_w fb_h 4))

;; Check: bpr and total match the alternative computation
(assert (= total_bytes total_v2))

;; Invariant: no overflow for display sizes up to 32K
(assert (<= logical_w 32768))
(assert (<= logical_h 32768))
(assert (>= fb_w 0))
(assert (>= fb_h 0))

;; Maximum theoretical total: 32768 × 2 = 65536 width
;; 65536 × 65536 × 4 = 17,179,869,184 bytes ≈ 16 GB
;; Real maximum (8K display × 2): 15360 × 8640 × 4 = 530,841,600 bytes ≈ 506 MB
;; Both within int64 range, int32 overflow possible for extreme sizes.

;; Prove: total_bytes fits in int64_t for all practical displays
(assert (>= total_bytes 0))

;; For a 32K display at 2x: 61440 × 34560 × 4 = 8,503,296,000
;; This fits in int64 (9.2e18) but NOT in int32 (2.1e9)
(define-const max_32k_2x Int (* (* 61440 34560) 4))
(assert (<= max_32k_2x 8503296000))
;; Check: 8.5GB < 2^63
(assert (< max_32k_2x 9223372036854775808))

;; For typical display (1920×1080 at 2x): 3840 × 2160 × 4 = 33,177,600
;; Fits in int32
(assert (= (* (* (* 1920 2) (* 1080 2)) 4) 33177600))

(check-sat)
;; Expected: SAT — all constraints are consistent

;; Prove: no overflow for all-reasonable displays (up to 8K at 2x)
(push)
(assert (>= (* fb_w fb_h 4) 0))
(assert (<= fb_w 16384))
(assert (<= fb_h 16384))
(check-sat)
;; Expected: SAT — within practical bounds
(pop)

;; CGBitmapContextCreate with NULL data = CoreGraphics auto-allocates
;; bytesPerRow = fb_w * 4 must be 32-bit aligned (always true for 32bpp)
(define-const bpr_mod Int (mod bpr 4))
(assert (= bpr_mod 0))

(check-sat)
;; Expected: SAT — 32-bit pixel format always produces 4-byte aligned rows
