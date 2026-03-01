# kain-asm — Assembly Importer Reference

> **Last Updated:** 2026-03-01
> **Status:** Production — 3 CPU dialects implemented. Data-driven dialect dispatch.

---

## Purpose

Imports assembly source files into KAIN IR. Currently targets retro/embedded processors. The importer produces `AsmProgram` / `TranslitUnit` structures (defined in `kain-core::asm_ir`) which can then be lowered to KAIN KAIN source or analyzed for parity tracing.

---

## Architecture

```
Assembly source (.asm / .s)
        ↓
  import_asm(path, format, out_kn, validate_only)
        ↓
  find_dialect(format)  ← data-driven DIALECTS table
        ↓
  dialect.importer(path, ...)
        ↓
   canonicalize_asm → parse_asm_program → build_translit_units
        ↓
  render_kain_firmware  (→ .kn source)
  build_recovery_report (→ RecoveryReport JSON)
        ↓
  ImportAsmOutput { kain_source, recovery_report, parity_schema }
```

---

## Supported Dialects

The dialect registry in `lib.rs` is a **data-driven static table** — adding a new dialect requires only a new `AsmDialect` entry, not code changes:

| Dialect ID | Aliases | Source file | Lines |
|---|---|---|---|
| `lr35902-gameboy` | `gameboy`, `gameboy-lr35902`, `gb-lr35902`, `lr35902` | `dialects/gameboy_lr35902/mod.rs` | 3,949 |
| `z80` | `z80-arcade`, `z80-spectrum`, `z80-msx` | `dialects/z80/mod.rs` | ~1,700 |
| `6502-furby` | `furby-6502`, `6502`, `furby` | `dialects/furby_6502/mod.rs` | 752 |

---

## Public API

```rust
// Discover what formats are registered
pub fn supported_formats() -> Vec<&'static str>
pub fn supported_format_aliases() -> Vec<&'static str>

// Import an assembly file
pub fn import_asm(
    input: &Path,
    format: &str,              // "gameboy", "6502", "z80", etc.
    out_kn: Option<&Path>,     // write .kn to this path if Some
    validate_only: bool,       // parse + validate without emitting
) -> AsmResult<ImportAsmOutput>
```

---

## Output Types

Defined in `kain-core::asm_ir` (re-exported from `kain-asm`):

```rust
pub struct ImportAsmOutput {
    pub kain_source: String,           // Generated .kn source
    pub recovery_report: RecoveryReport,
    pub parity_schema: ParityTraceFrame,
}

pub struct RecoveryReport {
    pub total_lines: usize,
    pub recovered_lines: usize,
    pub issues: Vec<RecoveryIssue>,
    pub section_scores: Vec<RecoverySectionScore>,
}

pub struct RecoveryIssue {
    pub line: usize,
    pub raw: String,
    pub reason: String,
}
```

---

## Game Boy LR35902 Dialect (`dialects/gameboy_lr35902/mod.rs` — 3,949 lines)

The most complete dialect. Contains a full LR35902 CPU + system emulator for parity tracing:

### CPU State (`Lr35902State`)
Registers: `A`, `B`, `C`, `D`, `E`, `H`, `L`, `F` (flags), `SP`, `PC`, `IME` (interrupt master enable), `halted`, `cycles`.

Flags: `Z` (zero, 0x80), `N` (subtract, 0x40), `H` (half-carry, 0x20), `C` (carry, 0x10).

### Memory Map (`Lr35902Memory`)
| Region | Size | Purpose |
|---|---|---|
| `rom0` | 16KB | Fixed ROM bank 0 |
| `romx` | 16KB × N | Switchable ROM banks |
| `vram` | 8KB | Video RAM |
| `eram` | 4 × 8KB | External (cartridge) RAM |
| `wram0` | 4KB | Work RAM bank 0 |
| `wramx` | 4KB | Work RAM bank 1 |
| `oam` | 160B | Object Attribute Memory (sprites) |
| `hram` | 127B | High RAM |
| `io` | 128B | I/O registers |

### MBC (Memory Bank Controller)
Supports: `None`, `MBC1`, `MBC5`. Implements ROM bank switching, RAM enable/disable, banking mode selection.

### Peripherals Emulated
- **PPU** (pixel processing unit) — framebuffer rendering, OAM, VRAM tiling
- **APU** (audio processing unit) — frame sequencer, channel timing
- **DMA** — OAM DMA with cycle-accurate timing
- **Timer** — DIV, TIMA, TMA, TAC registers with cycle accumulation
- **Joypad** — button and D-Pad input simulation
- **Serial** — serial transfer timing

### Import Pipeline
1. `canonicalize_asm` — strip BOM, page headers, normalize whitespace
2. `parse_asm_program` — extract `AsmBlock` (label + instructions) and `AsmDataTable` entries
3. `build_translit_units` — map blocks to `TranslitUnit` with normalized identifiers
4. `render_kain_firmware` — emit `.kn` source from translit units
5. `build_recovery_report` — score each section, report unrecoverable lines

Uses `rayon` for parallel section processing and `petgraph` for control flow graph analysis.

---

## 6502 / Furby Dialect (`dialects/furby_6502/mod.rs` — 752 lines)

Targets the **MOS 6502 processor** — the CPU in the Furby toy (among many others).

Pipeline functions:
- `canonicalize_asm` — normalize 6502 assembly syntax
- `parse_asm_program` — extract label blocks and data tables (`.byte`, `.word`, `.equ`)
- `is_opcode_keyword` — recognizes all 6502 opcode mnemonics (LDA, STA, JSR, BEQ, etc.)
- `is_directive_keyword` — `.org`, `.byte`, `.word`, `.equ`, `.include`
- `expand_compound_line` — splits compound inlined statements
- `render_kain_firmware` — emit KAIN functions from 6502 subroutines
- `build_recovery_report` — section-level recovery scoring
- `default_parity_trace_schema` — `ParityTraceFrame` schema for round-trip validation

`ImportAsmOutput`, `RecoveryReport`, `RecoveryIssue`, `RecoverySectionScore` — all defined here, re-exported as the crate's public output types.

---

## Z80 Dialect (`dialects/z80/mod.rs` — ~1,700 lines)

Targets the **Zilog Z80** — used in Game Boy (partial compatibility), ZX Spectrum, MSC, arcade boards.

Aliases: `z80-arcade`, `z80-spectrum`, `z80-msx`. All route to the same importer.

---

## Common KAIN AST Types Used

From `kain-core::asm_ir`:
- `AsmProgram` — top-level container
- `AsmBlock` — `{ label, instructions, start_line, end_line }`
- `AsmInstr` — `{ mnemonic, operands, source_line }`
- `AsmDataTable` — data section with `{ name, rows }`
- `AsmDirective` — `.org`, `.section`, `.byte`, etc.
- `TranslitUnit` — the KAIN-ready normalized representation of a block
- `ParityTraceFrame` — schema for round-trip parity validation

---

## Dependencies

| Crate | Role |
|---|---|
| `kain-core` | AST types, span, diagnostics |
| `indexmap` | Ordered maps for block/symbol tables |
| `petgraph` | CFG analysis (Game Boy dialect) |
| `rayon` | Parallel section processing (Game Boy dialect) |
| `smallvec` | Small inline vectors for instruction operands |
| `winnow` | Parser combinators (Z80 dialect) |
| `schemars` | `ParityTraceFrame` JSON schema generation |
| `serde` / `serde_json` | `RecoveryReport` serialization |
| `tracing` | Structured logging |

---

## Tests

- `lib.rs` inline: `alias_resolution_is_data_driven` — verifies dialect table lookup
- `furby_6502` inline: `canonicalize_removes_page_headers_and_bom`, `parser_extracts_blocks_and_tables`, `import_asm_generates_outputs`
