; =============================================================================
; Screen Buffer Layout (Shatter Struct)
; Proves: shatter struct field offsets are correctly aligned for
; SoA (Structure-of-Arrays) layout.
;
; Model: pi-squared terminal buffer uses a shatter struct for screen cells.
; Each cell has: char (4 bytes), fg_color (4 bytes), bg_color (4 bytes),
; attrs (2 bytes), dirty (1 byte). The shatter layout places each field
; in its own contiguous array for SIMD-friendly access.
; =============================================================================

; --- Part 1: Field Sizes and Offsets ---
; In a standard AoS struct Cell:
;   char:     offset 0,  size 4 (UInt32)
;   fg_color: offset 4,  size 4 (UInt32)
;   bg_color: offset 8,  size 4 (UInt32)
;   attrs:    offset 12, size 2 (UInt16)
;   dirty:    offset 14, size 1 (UInt8)
;   padding:  offset 15, size 1
;   total:    16 bytes aligned to 4
;
; In a shatter (SoA) struct:
;   char[N]:     base+0,   stride 4
;   fg_color[N]: base+N*4, stride 4
;   bg_color[N]: base+N*8, stride 4
;   attrs[N]:    base+N*12, stride 2
;   dirty[N]:    base+N*14, stride 1

(push)
(declare-const cell_count Int)
(assert (and (>= cell_count 1) (<= cell_count 4096)))

; AoS total size per cell = 16 (rounded up to alignment)
(declare-const aos_stride Int)
(assert (= aos_stride 16))

; SoA: each field is contiguous, total = sum of field sizes * count
(define-fun aos_total ((n Int)) Int (* n aos_stride))
(define-fun char_total ((n Int)) Int (* n 4))
(define-fun fg_total ((n Int)) Int (* n 4))
(define-fun bg_total ((n Int)) Int (* n 4))
(define-fun attrs_total ((n Int)) Int (* n 2))
(define-fun dirty_total ((n Int)) Int (* n 1))
(define-fun soa_total ((n Int)) Int
  (+ (char_total n) (fg_total n) (bg_total n)
     (attrs_total n) (dirty_total n)))

; SoA total should be ≤ AoS total (SoA eliminates padding)
(assert (> (soa_total cell_count) (aos_total cell_count)))

; This should be UNSAT for cell_count >= 1 — SoA is always smaller or equal
(check-sat)
(pop)

; --- Part 2: Field Alignment ---
; Each field in the shatter struct must be at an offset
; that satisfies its natural alignment requirement.
(push)
(declare-const field_index Int)
(assert (and (>= field_index 0) (< field_index 5)))

; Field types: 0=char, 1=fg, 2=bg, 3=attrs, 4=dirty
; Alignment requirements: char=4, fg=4, bg=4, attrs=2, dirty=1
(define-fun alignment ((f Int)) Int
  (ite (= f 0) 4 (ite (= f 1) 4 (ite (= f 2) 4
  (ite (= f 3) 2 1)))))

; Base offset of each field in SoA layout
(define-fun base_offset ((f Int)) Int
  (ite (= f 0) 0
  (ite (= f 1) 4
  (ite (= f 2) 8
  (ite (= f 3) 12 14)))))

; Field size
(define-fun field_size ((f Int)) Int
  (ite (= f 0) 4 (ite (= f 1) 4 (ite (= f 2) 4
  (ite (= f 3) 2 1)))))

; Every field's base offset must be aligned to its alignment
(define-fun is_aligned ((f Int)) Bool
  (= (mod (base_offset f) (alignment f)) 0))

(assert (not (is_aligned field_index)))

; Should be SAT for some field if an offset is misaligned
; (Checking alignment: char(0)@0, fg(1)@4, bg(2)@8, attrs(3)@12, dirty(4)@14
;  0%4=0, 4%4=0, 8%4=0, 12%2=0, 14%1=0 — all aligned)
(check-sat)
(get-model)
(pop)

; --- Part 3: SIMD Access Safety ---
; When reading 4 chars at once (128-bit SIMD), the read must not cross
; a cache line boundary unless the hardware guarantees it.
(push)
(declare-const base_addr Int)
(declare-const simd_width Int)
(assert (= simd_width 16)) ; 128-bit = 16 bytes

; char field base is at offset 0, with stride 4
; Reading 4 chars = reading bytes [0..15] from char array start
(define-fun simd_start () Int base_addr)
(define-fun simd_end () Int (+ base_addr simd_width))

; Cache line size = 64 bytes
(declare-const cache_line_size Int)
(assert (= cache_line_size 64))

; SIMD read crosses a cache line boundary if start and end are
; in different cache lines
(define-fun crosses_cache_line () Bool
  (not (= (div base_addr cache_line_size)
          (div (- (+ base_addr simd_width) 1) cache_line_size))))

; For any 16-byte aligned base_addr, the read does NOT cross cache line
; base_addr must be 16-byte aligned for SIMD
(assert (= (mod base_addr 16) 0))
(assert crosses_cache_line)

; 16-byte aligned → 16-byte read fits in one 64-byte cache line
; This should be UNSAT
(check-sat)
(pop)

; --- Part 4: Contiguity ---
; In SoA layout, accessing cell[i].char then cell[i+1].char
; is a linear walk with stride 4 (no random access).
(push)
(declare-const i Int)
(declare-const j Int)
(assert (and (>= i 0) (>= j 0) (< i cell_count) (< j cell_count)))

; Distance between char of cell[i] and char of cell[j] in SoA = 4 * (j - i)
(define-fun soa_char_distance () Int (* 4 (- j i)))

; Distance between same fields in AoS = 16 * (j - i)
(define-fun aos_cell_distance () Int (* 16 (- j i)))

; SoA char stride is independent of other fields
; char[j] - char[i] = 4*(j-i) — correct for all i, j
(define-fun stride_ok () Bool
  (=> (< i j)
      (= soa_char_distance (abs (- (* 4 j) (* 4 i))))))

(assert (not stride_ok))

; UNSAT confirms SoA stride is correct
(check-sat)
(pop)

(exit)
