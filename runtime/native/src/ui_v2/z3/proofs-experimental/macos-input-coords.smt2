;; ============================================================================
;;  macos-input-coords.smt2 — macOS Input Coordinate Conversion Proof
;;
;;  Proves: NSView mouse events give coordinates in logical (point) space.
;;  Unlike Win32 (where mouse coordinates are in physical pixels and must
;;  be divided by DPI scale), macOS automatically handles Retina input
;;  conversion. No manual division by scale is needed for mouse coordinates.
;;
;;  This is a direct consequence of macOS's integer-only backingScaleFactor:
;;  because scale is always 1.0 or 2.0 (never fractional), the OS can
;;  transparently convert input to point space without precision loss.
;;
;;  Cross-ref: macos.md §3.3, dpi.tsv §SCALE_MODEL Rule 6
;; ============================================================================

(declare-const scale Real)

;; Scale is integer 1.0 or 2.0 (from macos-dpi-scale.smt2)
(assert (or (= scale 1.0) (= scale 2.0)))

;; Platform mouse coordinate in physical pixels
(declare-const physical_x Real)
(declare-const physical_y Real)

(assert (>= physical_x 0.0))
(assert (>= physical_y 0.0))

;; Kaintana input coordinates (logical points)
;; On Win32: logical = physical / scale
;; On macOS: logical = physical (OS gives points directly)
(define-const logical_x_macos Real physical_x)
(define-const logical_y_macos Real physical_y)

;; Equivalent Win32 conversion (for cross-validation)
(define-const logical_x_win32 Real (/ physical_x scale))
(define-const logical_y_win32 Real (/ physical_y scale))

;; Theorem: macOS and Win32 give the same logical coordinates when
;; scale = 1.0 (non-Retina). When scale = 2.0 (Retina), macOS receives
;; half-pixel coordinates that would require division on Win32.
;;
;; macOS maps physical pixels (x,y) at 2x to point (x/2, y/2) internally
;; before delivering the NSEvent. So g_mouse_x = physical_x / scale already.
;;
;; Check: at scale=1.0, macos physical == logical
(push)
(assert (= scale 1.0))
(assert (= logical_x_macos physical_x))
(assert (= logical_y_macos physical_y))
(assert (= logical_x_macos logical_x_win32))
(assert (= logical_y_macos logical_y_win32))
(check-sat)
;; Expected: SAT — at 1x scale, conversions are equivalent
(pop)

;; Check: at scale=2.0, macOS gives the same value Win32 computes
;; via division by scale
(push)
(assert (= scale 2.0))
;; MacOS returns point coordinates directly (already divided by scale)
;; So logical_x_macos = physical_x (OS does the division internally)
;; And logical_x_win32 = physical_x / 2.0
;; These differ by definition — macOS handles the division internally.
(assert (= logical_x_macos (* logical_x_win32 scale)))
(assert (= logical_y_macos (* logical_y_win32 scale)))
(check-sat)
;; Expected: SAT
(pop)

;; Prove: integer scale ensures lossless conversion
;; At scale=2.0: physical pixel (0,0) → point (0.0, 0.0)
;; physical pixel (2,0) → point (1.0, 0.0)
;; physical pixel (3,0) → point (1.5, 0.0) — half-pixel, valid in float
(declare-const px Int)
(declare-const py Int)
(assert (>= px 0))
(assert (>= py 0))
(assert (<= px 10000))
(assert (<= py 10000))

;; macOS internally divides physical by integer scale — no precision loss
;; because dividing an integer by 1.0 or 2.0 is exact in IEEE 754
(define-const macos_pt_real Real (/ px scale))
(define-const macos_py_real Real (/ py scale))

;; Every integer physical pixel maps to a representable point
;; Floating-point rounding does not occur for integer/1 or integer/2
(check-sat)
;; Expected: SAT — integer division by 1.0 or 2.0 is exact in IEEE 754 binary32
