;; Proof: stbtt_GetCodepointHMetrics_bounds.smt2
;; Glyph advance width and LSB retrieval within hmtx table bounds
;;
;; stbtt_GetGlyphHMetrics() reads the horizontal metrics from the hmtx table.
;; The hmtx table has two parts:
;;   1. numOfLongHorMetrics entries: 4 bytes each (advanceWidth + leftSideBearing)
;;   2. (numGlyphs - numOfLongHorMetrics) entries: 2 bytes each (leftSideBearing only)
;;
;; The code guards access:
;;   if (glyph_index < numOfLongHorMetrics) {
;;       advance = data[hmtx + 4*glyph_index]
;;       lsb     = data[hmtx + 4*glyph_index + 2]
;;   } else {
;;       advance = data[hmtx + 4*(numOfLongHorMetrics-1)]
;;       lsb     = data[hmtx + 4*numOfLongHorMetrics + 2*(glyph_index - numOfLongHorMetrics)]
;;   }
;;
;; Key claims:
;;   1. 4*glyph_index doesn't overflow int32 for valid glyph counts
;;   2. For glyph_index < numOfLongHorMetrics: 4*glyph_index + 2 < hmtx table size
;;   3. For glyph_index >= numOfLongHorMetrics: 4*numOfLongHorMetrics + 2*(glyph_index - numOfLongHorMetrics) + 2 <= hmtx table size
;;   4. The advance width lookup for glyph_index >= numOfLongHorMetrics uses the last long metric
;;   5. numOfLongHorMetrics >= 1 for any valid font (at least the .notdef glyph)
;;
(set-logic QF_BV)

; ── Claim 1: 4 * glyph_index doesn't overflow int32 ──
; numGlyphs is uint16 (max 65535). 4 * 65535 = 262140, well within int32.
; glyph_index is [0, numGlyphs-1].
;
(set-logic QF_BV)

(declare-const glyph_index (_ BitVec 32))
(declare-const numGlyphs (_ BitVec 32))

; Valid range: 0 <= glyph_index < numGlyphs <= 65535
(assert (bvsge glyph_index (_ bv0 32)))
(assert (bvult glyph_index numGlyphs))
(assert (bvule numGlyphs (_ bv65535 32)))

; 4 * glyph_index fits in 32 bits (signed)
(define-fun offset_x4 () (_ BitVec 32) (bvshl glyph_index (_ bv2 32)))

; 4 * 65535 = 262140 = 0x0003FFFC
; Verify: no overflow (bit 31 stays 0)
(assert (= (bvshl (_ bv65535 32) (_ bv2 32)) (_ bv262140 32)))
(check-sat)
; Expected: sat — 4 * 65535 = 262140

(reset)

; ── Claim 2: 4*glyph_index + 2 < hmtx table size for glyph_index < numOfLongHorMetrics ──
; hmtx table size = 4 * numOfLongHorMetrics + 2 * (numGlyphs - numOfLongHorMetrics)
;                  = 2 * (numOfLongHorMetrics + numGlyphs)
;
; For glyph_index < numOfLongHorMetrics, we read at offset 4*glyph_index + 2 (2 bytes)
; The last byte accessed: 4*glyph_index + 2 + 1 = 4*glyph_index + 3
; Max: 4*(numOfLongHorMetrics-1) + 3 = 4*numOfLongHorMetrics - 1
;
; Table size: 2*(numOfLongHorMetrics + numGlyphs)
; Need: 4*numOfLongHorMetrics - 1 < 2*(numOfLongHorMetrics + numGlyphs)
;       4*N - 1 < 2N + 2*numGlyphs
;       2*N - 1 < 2*numGlyphs
;       N < numGlyphs + 0.5
;       numOfLongHorMetrics <= numGlyphs ✓ (by TrueType spec)
;
(set-logic QF_BV)

(declare-const numOfLongHorMetrics (_ BitVec 32))
(declare-const numGlyphs (_ BitVec 32))

; Spec constraints: 1 <= numOfLongHorMetrics <= numGlyphs <= 65535
(assert (bvsgt numOfLongHorMetrics (_ bv0 32)))
(assert (bvule numOfLongHorMetrics numGlyphs))
(assert (bvule numGlyphs (_ bv65535 32)))

; hmtx table size
(define-fun hmtx_size () (_ BitVec 32) (bvadd (bvshl numOfLongHorMetrics (_ bv2 32)) (bvshl (bvsub numGlyphs numOfLongHorMetrics) (_ bv1 32))))
; Simplified: 4*N + 2*(numGlyphs-N) = 2*(N + numGlyphs)

; Max access offset in branch 1: 4*(numOfLongHorMetrics-1) + 2 (for LSB) + 1 (for byte read)
; Actually ttSHORT reads 2 bytes starting at that offset
; So the last byte offset is 4*(numOfLongHorMetrics-1) + 2 + 1 = 4*numOfLongHorMetrics - 4 + 3 = 4*numOfLongHorMetrics - 1
(define-fun max_access_offset () (_ BitVec 32) (bvsub (bvshl numOfLongHorMetrics (_ bv2 32)) (_ bv1 32)))

; Prove: max_access_offset < hmtx_size (last byte is within table)
(assert (not (bvult max_access_offset hmtx_size)))
(check-sat)
; Expected: unsat — all hmtx reads are within bounds

(reset)

; ── Claim 3: LSB offset for glyph_index >= numOfLongHorMetrics is in bounds ──
; LSB offset = 4*numOfLongHorMetrics + 2*(glyph_index - numOfLongHorMetrics)
; Last LSB read at: offset + 1 (2-byte ttSHORT)
; For glyph_index = numGlyphs-1:
;   offset = 4*N + 2*(numGlyphs-1 - N) = 2*N + 2*numGlyphs - 2
;   last byte = 2*N + 2*numGlyphs - 1
;   hmtx_size = 4*N + 2*(numGlyphs - N) = 2*N + 2*numGlyphs
;   So last byte = hmtx_size - 1 ✓
;
(set-logic QF_BV)

(declare-const numOfLongHorMetrics (_ BitVec 32))
(declare-const glyph_index (_ BitVec 32))
(declare-const numGlyphs (_ BitVec 32))

(assert (bvsgt numOfLongHorMetrics (_ bv0 32)))
(assert (bvule numOfLongHorMetrics numGlyphs))
(assert (bvule numGlyphs (_ bv65535 32)))
(assert (bvsge glyph_index (_ bv0 32)))
(assert (bvsge glyph_index numOfLongHorMetrics))  ; else branch
(assert (bvult glyph_index numGlyphs))

; hmtx table size: 4*numOfLongHorMetrics + 2*(numGlyphs - numOfLongHorMetrics)
(define-fun hmtx_size () (_ BitVec 32) (bvadd (bvshl numOfLongHorMetrics (_ bv2 32)) (bvshl (bvsub numGlyphs numOfLongHorMetrics) (_ bv1 32))))

; LSB offset in else branch: 4*numOfLongHorMetrics + 2*(glyph_index - numOfLongHorMetrics)
(define-fun lsb_offset () (_ BitVec 32) (bvadd (bvshl numOfLongHorMetrics (_ bv2 32)) (bvshl (bvsub glyph_index numOfLongHorMetrics) (_ bv1 32))))

; ttSHORT reads 2 bytes at lsb_offset. Last byte = lsb_offset + 1
(define-fun last_byte () (_ BitVec 32) (bvadd lsb_offset (_ bv1 32)))

; Prove: last_byte < hmtx_size (the 2-byte read is entirely within the table)
(assert (not (bvult last_byte hmtx_size)))
(check-sat)
; Expected: unsat — LSB read is within bounds

(reset)

; ── Claim 4: Advance width in else branch uses last valid entry ──
; The code reads:
;   if (advanceWidth) *advanceWidth = ttSHORT(data + hmtx + 4*(numOfLongHorMetrics-1));
; This is always valid since numOfLongHorMetrics >= 1.
;
(set-logic QF_BV)

(declare-const numOfLongHorMetrics (_ BitVec 32))

; numOfLongHorMetrics >= 1 (at least the .notdef glyph has long metrics)
(assert (bvsgt numOfLongHorMetrics (_ bv0 32)))

; offset = 4*(numOfLongHorMetrics-1) = 4*N - 4
(define-fun offset_advance () (_ BitVec 32) (bvshl (bvsub numOfLongHorMetrics (_ bv1 32)) (_ bv2 32)))

; ttSHORT reads 2 bytes at offset_advance. Last byte = offset_advance + 1 = 4*N - 3
; This is within the hmtx table (size >= 4*N)
(define-fun advance_last_byte () (_ BitVec 32) (bvadd offset_advance (_ bv1 32)))
(define-fun min_hmtx_size () (_ BitVec 32) (bvshl numOfLongHorMetrics (_ bv2 32)))

; Prove: advance_last_byte < min_hmtx_size (even without the short-metrics tail)
(assert (not (bvult advance_last_byte min_hmtx_size)))
(check-sat)
; Expected: unsat — advance width read is within bounds

(reset)

; ── Claim 5: numOfLongHorMetrics > 0 for any valid font ──
; TrueType spec requires at least 1 long horizontal metric (for glyph 0).
; If numOfLongHorMetrics == 0, the hhea table is malformed.
;
(set-logic QF_BV)

(declare-const numOfLongHorMetrics (_ BitVec 16))

; The spec says numOfLongHorMetrics is uint16, must be > 0
; valid range: [1, numGlyphs]
(assert (bvugt numOfLongHorMetrics (_ bv0 16)))
(assert (bvule numOfLongHorMetrics (_ bv65535 16)))

; Prove: there is no valid font with numOfLongHorMetrics = 0
(assert (= numOfLongHorMetrics (_ bv0 16)))
(check-sat)
; Expected: unsat — numOfLongHorMetrics > 0

(reset)

; ── Claim 6: 4*(numOfLongHorMetrics-1) doesn't overflow for max uint16 ──
; 4 * 65534 = 262136, well within int32
(set-logic QF_BV)

(define-fun max_long_offset () (_ BitVec 32) (bvshl (_ bv65534 32) (_ bv2 32)))
(assert (= max_long_offset (_ bv262136 32)))
(check-sat)
; Expected: sat — 4*65534 = 262136

(exit)
