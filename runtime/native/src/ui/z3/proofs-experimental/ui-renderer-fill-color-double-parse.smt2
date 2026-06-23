; ============================================================================
; Proof: ui_parse_color(fill_str) called twice → redundant second call
; ============================================================================
;
; Target: ui_renderer.c:185-189
;   if (fill_str) {
;       uint32_t fill_color = ui_parse_color(fill_str);         // PARSE 1
;       if (fill_color != 0 || ui_color_a(ui_parse_color(fill_str)) == 0) // PARSE 2
;
; Claim: The second call to ui_parse_color is redundant. Since ui_parse_color
; is a pure function, parse_color(x) always equals parse_color(x). The result
; was already stored in fill_color on line 186.

(set-logic QF_UF)

; ── Abstract model ─────────────────────────────────────────────────────
; We model the color string as an uninterpreted sort and ui_parse_color
; as an uninterpreted function. The purity of parse_color means:
;   For any input s: parse_color(s) always returns the same uint32_t.
; This is inherent in the semantics of uninterpreted functions in SMT.

(declare-sort ColorString)               ; abstract color string type
(declare-sort Color32)                    ; uint32_t color value

; The pure function ui_parse_color: same input → same output
(declare-fun parse_color (ColorString) Color32)

; Extract alpha byte from a parsed color (0xAARRGGBB format)
; Alpha is in the high byte. 0 = transparent, 255 = opaque.
(declare-fun color_is_opaque (Color32) Bool)
(declare-fun color_is_transparent (Color32) Bool)
(declare-fun color_is_zero (Color32) Bool)

; Axioms: color properties
(assert (forall ((c Color32))
  (= (color_is_transparent c) (not (color_is_opaque c)))))
; Zero (0x00000000) is both transparent and zero
(assert (forall ((c Color32))
  (=> (color_is_zero c) (color_is_transparent c))))

; ── Concrete example ───────────────────────────────────────────────────
; A specific color string and its parsed value
(declare-const fill_str ColorString)      ; the input "fill_color" string value
(declare-const fill_val Color32)          ; the parsed color uint32_t

; fill_val is the result of parsing fill_str (this is established on line 186)
(assert (= fill_val (parse_color fill_str)))

; ── Claim 1: The TWO expressions are identical ─────────────────────────
; The condition on line 189 is:
;   fill_color != 0 || ui_color_a(ui_parse_color(fill_str)) == 0
;
; Where fill_color == parse_color(fill_str).
;
; The second call ui_parse_color(fill_str) returns the same as fill_color
; because parse_color is a pure function.
;
; Therefore the two conditions are equivalent:
;   C1: fill_color != 0 || ui_color_a(parse_color(fill_str)) == 0
;   C2: fill_color != 0 || ui_color_a(fill_color) == 0

; We prove this by asserting they differ and checking for sat:
(assert (not
  (= (or (not (color_is_zero fill_val))
         (color_is_transparent (parse_color fill_str)))
     (or (not (color_is_zero fill_val))
         (color_is_transparent fill_val)))))

(check-sat)
; Expected: unsat — both conditions are logically identical because
; parse_color(fill_str) == fill_val (by the assertion above)

; ── Claim 2: The entire inner condition is dead code ───────────────────
; The comment says "let it draw if the color parsed — even transparent is
; a choice." This means ALL valid color draws should proceed. The inner
; if-clause is vestigial.
;
; If we simplify to:
;   if (fill_str) {
;       uint32_t fill_color = parse_color(fill_str);
;       // Draw unconditionally — all color draws are intentional
;       ui_draw_XXX(..., fill_color);
;   }
;
; This eliminates one full color parse (cost: ~50-200 cycles) plus one
; branch per node per frame.

; ── Claim 3: The redundant parse is harmful ────────────────────────────
; The double parse calls the color parser twice:
;   Parser flow: parse first hex digit → find end → produce uint32_t
;   For "#RRGGBB": strchr, hex decode, bit shifts — ~30-50 operations
;   For "rgb(r,g,b)": strtok-like parsing — ~50-100 operations
;   For named colors: linear scan of ~30 entries + strcmp — ~100-200 ops
;
; Doing this TWICE for the same input string is pure waste. The second
; parse produces no new information.

; ── Conclusion ─────────────────────────────────────────────────────────
; The fix: replace the inner condition with the already-parsed fill_color.
; Both conditions provably select the same color values.
;
; Proof status: unsat (both expressions equivalent)
; ============================================================================
