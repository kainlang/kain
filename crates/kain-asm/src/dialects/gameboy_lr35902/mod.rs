use crate::dialects::furby_6502::{
    ImportAsmOutput, RecoveryIssue, RecoveryReport, RecoverySectionScore,
};
use crate::error::{AsmError, AsmResult};
use indexmap::IndexMap;
use kain_core::{
    diagnostics::SpanMapper,
    span::Span,
    AsmBlock, AsmDataTable, AsmDirective, AsmInstr, AsmProgram, ParityTraceFrame, TranslitUnit,
};
use petgraph::graphmap::DiGraphMap;
use rayon::prelude::*;
use schemars::JsonSchema;
use serde::Serialize;
use smallvec::SmallVec;
use std::collections::{BTreeMap, HashSet};
use std::fs;
use std::path::{Path, PathBuf};
use tracing::{debug, info};

const SUPPORTED_FORMATS: &[&str] = &["lr35902-gameboy", "gameboy-lr35902", "gb-lr35902", "lr35902", "gameboy"];
const MAX_EXPAND_DEPTH: usize = 16;

const FLAG_Z: u8 = 0x80;
const FLAG_N: u8 = 0x40;
const FLAG_H: u8 = 0x20;
const FLAG_C: u8 = 0x10;

#[derive(Clone, Debug, PartialEq, Eq)]
struct Lr35902State {
    a: u8,
    b: u8,
    c: u8,
    d: u8,
    e: u8,
    h: u8,
    l: u8,
    f: u8,
    sp: u16,
    pc: u16,
    ime: bool,
    ime_enable_delay: u8,
    halted: bool,
    cycles: u64,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum MbcKind {
    None,
    Mbc1,
    Mbc5,
}

#[derive(Clone, Debug, PartialEq, Eq)]
struct Lr35902Memory {
    rom0: Vec<u8>,
    romx: Vec<Vec<u8>>,
    current_rom_bank: usize,
    rom_bank_low5: u8,
    rom_bank_high2: u8,
    rom_bank_low8: u8,
    rom_bank_high1: u8,
    banking_mode: u8,
    ram_enabled: bool,
    current_ram_bank: usize,
    mbc_kind: MbcKind,
    vram: [u8; 0x2000],
    eram: [[u8; 0x2000]; 4],
    wram0: [u8; 0x1000],
    wramx: [u8; 0x1000],
    oam: [u8; 0xa0],
    hram: [u8; 0x7f],
    io: [u8; 0x80],
    ie: u8,
    div_cycle_accum: u16,
    timer_cycle_accum: u16,
    ppu_cycle_accum: u16,
    dma_cycles_remaining: u16,
    dma_cycle_accum: u8,
    dma_source_page: u8,
    dma_next_index: u8,
    serial_cycles_remaining: u16,
    joypad_last_low: u8,
    joypad_buttons: u8,
    joypad_dpad: u8,
    framebuffer: Vec<u8>,
    last_ppu_mode: u8,
    stat_irq_latch: bool,
    window_line_counter: u8,
    apu_cycle_accum: u16,
    apu_frame_step: u8,
}

#[derive(Clone, Debug, PartialEq, Eq)]
enum StepEffect {
    None,
    Halt,
    PortWrite { port: u8, value: u8 },
    Interrupt { vector: u16 },
}

#[derive(Clone, Debug, PartialEq, Eq)]
struct StepResult {
    state: Lr35902State,
    memory: Lr35902Memory,
    effect: StepEffect,
    opcode: u8,
    cycles: u8,
}

impl Default for Lr35902State {
    fn default() -> Self {
        Self {
            a: 0,
            b: 0,
            c: 0,
            d: 0,
            e: 0,
            h: 0,
            l: 0,
            f: 0,
            sp: 0xfffe,
            pc: 0x0100,
            ime: false,
            ime_enable_delay: 0,
            halted: false,
            cycles: 0,
        }
    }
}

impl Default for Lr35902Memory {
    fn default() -> Self {
        Self {
            rom0: vec![0; 0x4000],
            romx: vec![vec![0; 0x4000]],
            current_rom_bank: 1,
            rom_bank_low5: 1,
            rom_bank_high2: 0,
            rom_bank_low8: 1,
            rom_bank_high1: 0,
            banking_mode: 0,
            ram_enabled: false,
            current_ram_bank: 0,
            mbc_kind: MbcKind::Mbc1,
            vram: [0; 0x2000],
            eram: [[0; 0x2000]; 4],
            wram0: [0; 0x1000],
            wramx: [0; 0x1000],
            oam: [0; 0xa0],
            hram: [0; 0x7f],
            io: [0; 0x80],
            ie: 0,
            div_cycle_accum: 0,
            timer_cycle_accum: 0,
            ppu_cycle_accum: 0,
            dma_cycles_remaining: 0,
            dma_cycle_accum: 0,
            dma_source_page: 0,
            dma_next_index: 0,
            serial_cycles_remaining: 0,
            joypad_last_low: 0x0f,
            joypad_buttons: 0x0f,
            joypad_dpad: 0x0f,
            framebuffer: vec![0; 160 * 144],
            last_ppu_mode: 0,
            stat_irq_latch: false,
            window_line_counter: 0,
            apu_cycle_accum: 0,
            apu_frame_step: 0,
        }
    }
}

const IF_BIT_VBLANK: u8 = 0x01;
const IF_BIT_LCDSTAT: u8 = 0x02;
const IF_BIT_TIMER: u8 = 0x04;
const IF_BIT_SERIAL: u8 = 0x08;
const IF_BIT_JOYPAD: u8 = 0x10;

const IO_IF: usize = 0x0f;
const IO_DIV: usize = 0x04;
const IO_TIMA: usize = 0x05;
const IO_TMA: usize = 0x06;
const IO_TAC: usize = 0x07;
const IO_JOYP: usize = 0x00;
const IO_SC: usize = 0x02;
const IO_LCDC: usize = 0x40;
const IO_STAT: usize = 0x41;
const IO_SCY: usize = 0x42;
const IO_SCX: usize = 0x43;
const IO_LY: usize = 0x44;
const IO_LYC: usize = 0x45;
const IO_DMA: usize = 0x46;
const IO_BGP: usize = 0x47;
const IO_OBP0: usize = 0x48;
const IO_OBP1: usize = 0x49;
const IO_WY: usize = 0x4a;
const IO_WX: usize = 0x4b;

fn detect_mbc_kind(rom0: &[u8]) -> MbcKind {
    let cart_type = rom0.get(0x147).copied().unwrap_or(0x00);
    match cart_type {
        0x00 | 0x08 | 0x09 => MbcKind::None,
        0x01..=0x03 => MbcKind::Mbc1,
        0x19..=0x1e => MbcKind::Mbc5,
        _ => MbcKind::Mbc1,
    }
}

fn effective_rom_bank0(mem: &Lr35902Memory) -> usize {
    match mem.mbc_kind {
        MbcKind::None | MbcKind::Mbc5 => 0,
        MbcKind::Mbc1 => {
            if mem.banking_mode == 0 {
                0
            } else {
                (mem.rom_bank_high2 as usize) << 5
            }
        }
    }
}

fn effective_rom_bankx(mem: &Lr35902Memory) -> usize {
    let mut bank = match mem.mbc_kind {
        MbcKind::None => 1,
        MbcKind::Mbc1 => {
            let high = if mem.banking_mode == 0 {
                (mem.rom_bank_high2 as usize) << 5
            } else {
                0
            };
            (mem.rom_bank_low5 as usize) | high
        }
        MbcKind::Mbc5 => ((mem.rom_bank_high1 as usize) << 8) | mem.rom_bank_low8 as usize,
    };
    if matches!(mem.mbc_kind, MbcKind::Mbc1 | MbcKind::Mbc5) && bank == 0 {
        bank = 1;
    }
    bank
}

fn map_rom_bank(mem: &Lr35902Memory, bank: usize, off: usize) -> u8 {
    if bank == 0 {
        return *mem.rom0.get(off).unwrap_or(&0xff);
    }
    if mem.romx.is_empty() {
        return 0xff;
    }
    let idx = (bank - 1) % mem.romx.len();
    mem.romx.get(idx).and_then(|b| b.get(off)).copied().unwrap_or(0xff)
}

fn refresh_mbc_state(mem: &mut Lr35902Memory) {
    mem.mbc_kind = detect_mbc_kind(&mem.rom0);
    mem.current_rom_bank = effective_rom_bankx(mem);
    mem.current_ram_bank = match mem.mbc_kind {
        MbcKind::None => 0,
        MbcKind::Mbc1 => {
            if mem.banking_mode == 0 {
                0
            } else {
                (mem.rom_bank_high2 & 0x03) as usize
            }
        }
        MbcKind::Mbc5 => (mem.rom_bank_high2 & 0x0f) as usize,
    };
    if !mem.eram.is_empty() {
        mem.current_ram_bank %= mem.eram.len();
    }
}

fn joypad_input_low_nibble(mem: &Lr35902Memory) -> u8 {
    let select = mem.io[IO_JOYP] & 0x30;
    let mut low = 0x0f;
    if (select & 0x10) == 0 {
        low &= mem.joypad_dpad;
    }
    if (select & 0x20) == 0 {
        low &= mem.joypad_buttons;
    }
    low
}

fn update_joypad_register(mem: &mut Lr35902Memory) {
    let select = mem.io[IO_JOYP] & 0x30;
    let low = joypad_input_low_nibble(mem);
    mem.io[IO_JOYP] = select | low;
    if (mem.joypad_last_low & !low) != 0 {
        request_interrupt(mem, IF_BIT_JOYPAD);
    }
    mem.joypad_last_low = low;
}

fn dma_oam_transfer(mem: &mut Lr35902Memory, page: u8) {
    mem.dma_source_page = page;
    mem.dma_next_index = 0;
    mem.dma_cycle_accum = 0;
    mem.dma_cycles_remaining = 160 * 4;
}

fn read8_raw(mem: &Lr35902Memory, addr: u16) -> u8 {
    match addr {
        0x0000..=0x3fff => {
            let off = addr as usize;
            let bank0 = effective_rom_bank0(mem);
            map_rom_bank(mem, bank0, off)
        }
        0x4000..=0x7fff => {
            let off = (addr as usize).saturating_sub(0x4000);
            let bankx = effective_rom_bankx(mem);
            map_rom_bank(mem, bankx, off)
        }
        0x8000..=0x9fff => mem.vram[(addr - 0x8000) as usize],
        0xa000..=0xbfff => {
            if !mem.ram_enabled {
                0xff
            } else {
                mem.eram[mem.current_ram_bank.min(mem.eram.len() - 1)][(addr - 0xa000) as usize]
            }
        }
        0xc000..=0xcfff => mem.wram0[(addr - 0xc000) as usize],
        0xd000..=0xdfff => mem.wramx[(addr - 0xd000) as usize],
        0xe000..=0xefff => mem.wram0[(addr - 0xe000) as usize],
        0xf000..=0xfdff => mem.wramx[(addr - 0xf000) as usize],
        0xfe00..=0xfe9f => mem.oam[(addr - 0xfe00) as usize],
        0xfea0..=0xfeff => 0xff,
        0xff00..=0xff7f => mem.io[(addr - 0xff00) as usize],
        0xff80..=0xfffe => mem.hram[(addr - 0xff80) as usize],
        0xffff => mem.ie,
    }
}

fn read8(state: &Lr35902State, mem: &Lr35902Memory, addr: u16) -> u8 {
    let _ = state;
    if mem.dma_cycles_remaining > 0 && addr < 0xff80 {
        return 0xff;
    }
    read8_raw(mem, addr)
}

fn write8(mem: &mut Lr35902Memory, addr: u16, value: u8) {
    let detected = detect_mbc_kind(&mem.rom0);
    if mem.mbc_kind != detected {
        mem.mbc_kind = detected;
    }
    if mem.dma_cycles_remaining > 0 && addr < 0xff80 && addr != 0xff46 {
        return;
    }
    match addr {
        0x0000..=0x1fff => {
            if !matches!(mem.mbc_kind, MbcKind::None) {
                mem.ram_enabled = (value & 0x0f) == 0x0a;
            }
        }
        0x2000..=0x3fff => {
            match mem.mbc_kind {
                MbcKind::None => {}
                MbcKind::Mbc1 => {
                    mem.rom_bank_low5 = value & 0x1f;
                    if mem.rom_bank_low5 == 0 {
                        mem.rom_bank_low5 = 1;
                    }
                }
                MbcKind::Mbc5 => {
                    if addr <= 0x2fff {
                        mem.rom_bank_low8 = value;
                    } else {
                        mem.rom_bank_high1 = value & 0x01;
                    }
                }
            }
            refresh_mbc_state(mem);
        }
        0x4000..=0x5fff => {
            match mem.mbc_kind {
                MbcKind::None => {}
                MbcKind::Mbc1 => {
                    mem.rom_bank_high2 = value & 0x03;
                }
                MbcKind::Mbc5 => {
                    mem.rom_bank_high2 = value & 0x0f;
                }
            }
            refresh_mbc_state(mem);
        }
        0x6000..=0x7fff => {
            if matches!(mem.mbc_kind, MbcKind::Mbc1) {
                mem.banking_mode = value & 0x01;
            }
            refresh_mbc_state(mem);
        }
        0x8000..=0x9fff => mem.vram[(addr - 0x8000) as usize] = value,
        0xa000..=0xbfff => {
            if mem.ram_enabled {
                let bank = mem.current_ram_bank.min(mem.eram.len() - 1);
                mem.eram[bank][(addr - 0xa000) as usize] = value;
            }
        }
        0xc000..=0xcfff => mem.wram0[(addr - 0xc000) as usize] = value,
        0xd000..=0xdfff => mem.wramx[(addr - 0xd000) as usize] = value,
        0xe000..=0xefff => mem.wram0[(addr - 0xe000) as usize] = value,
        0xf000..=0xfdff => mem.wramx[(addr - 0xf000) as usize] = value,
        0xfe00..=0xfe9f => mem.oam[(addr - 0xfe00) as usize] = value,
        0xfea0..=0xfeff => {}
        0xff00..=0xff7f => {
            let idx = (addr - 0xff00) as usize;
            if idx == IO_DIV {
                mem.io[IO_DIV] = 0;
                mem.div_cycle_accum = 0;
            } else if idx == IO_STAT {
                // STAT lower bits are read-only status; only control bits are writable.
                mem.io[IO_STAT] = (value & 0x78) | (mem.io[IO_STAT] & 0x07);
            } else if idx == IO_JOYP {
                mem.io[IO_JOYP] = (value & 0x30) | (mem.io[IO_JOYP] & 0x0f);
                update_joypad_register(mem);
            } else if idx == IO_DMA {
                mem.io[IO_DMA] = value;
                dma_oam_transfer(mem, value);
            } else if idx == IO_LCDC {
                let was_enabled = (mem.io[IO_LCDC] & 0x80) != 0;
                mem.io[IO_LCDC] = value;
                let now_enabled = (value & 0x80) != 0;
                if was_enabled && !now_enabled {
                    mem.ppu_cycle_accum = 0;
                    mem.io[IO_LY] = 0;
                    mem.last_ppu_mode = 0;
                    mem.stat_irq_latch = false;
                    mem.window_line_counter = 0;
                } else if !was_enabled && now_enabled {
                    mem.ppu_cycle_accum = 0;
                    mem.last_ppu_mode = 2;
                }
            } else if idx == IO_SC {
                mem.io[IO_SC] = value;
                if (value & 0x80) != 0 {
                    // Internal clock serial transfer completion (coarse): 8 bits * 512 cycles/bit.
                    mem.serial_cycles_remaining = 4096;
                }
            } else {
                mem.io[idx] = value;
            }
            if idx == IO_STAT || idx == IO_LYC || idx == IO_LCDC {
                update_stat_and_irq(mem);
            }
        }
        0xff80..=0xfffe => mem.hram[(addr - 0xff80) as usize] = value,
        0xffff => mem.ie = value,
    }
}

fn request_interrupt(mem: &mut Lr35902Memory, mask: u8) {
    mem.io[IO_IF] |= mask;
}

fn timer_period_cycles(tac: u8) -> u16 {
    match tac & 0x03 {
        0 => 1024, // 4096 Hz
        1 => 16,   // 262144 Hz
        2 => 64,   // 65536 Hz
        _ => 256,  // 16384 Hz
    }
}

fn update_stat_and_irq(mem: &mut Lr35902Memory) {
    let lcd_enabled = (mem.io[IO_LCDC] & 0x80) != 0;
    let ly = mem.io[IO_LY];
    let lyc = mem.io[IO_LYC];

    let mode = if !lcd_enabled {
        0u8
    } else if ly >= 144 {
        1u8
    } else if mem.ppu_cycle_accum < 80 {
        2u8
    } else if mem.ppu_cycle_accum < 252 {
        3u8
    } else {
        0u8
    };

    if mem.last_ppu_mode == 3 && mode == 0 && ly < 144 {
        render_scanline(mem, ly as usize);
    }
    mem.last_ppu_mode = mode;

    let mut stat = mem.io[IO_STAT];
    stat = (stat & !0x03) | mode;
    if ly == lyc {
        stat |= 0x04;
    } else {
        stat &= !0x04;
    }
    mem.io[IO_STAT] = stat;

    let lyc_irq = (stat & 0x40) != 0 && (stat & 0x04) != 0;
    let mode0_irq = (stat & 0x08) != 0 && mode == 0;
    let mode1_irq = (stat & 0x10) != 0 && mode == 1;
    let mode2_irq = (stat & 0x20) != 0 && mode == 2;
    let irq_line = lyc_irq || mode0_irq || mode1_irq || mode2_irq;

    // LCDSTAT requests on low->high transition of the internal STAT interrupt line.
    if irq_line && !mem.stat_irq_latch {
        request_interrupt(mem, IF_BIT_LCDSTAT);
    }
    mem.stat_irq_latch = irq_line;
}

fn bg_palette_shade(bgp: u8, color_id: u8) -> u8 {
    let shift = (color_id as u16 * 2) as u8;
    (bgp >> shift) & 0x03
}

fn read_tile_color_id(mem: &Lr35902Memory, lcdc: u8, tile_id: u8, fine_y: usize, fine_x: usize) -> u8 {
    let signed_tiles = (lcdc & 0x10) == 0;
    let tile_base = if signed_tiles {
        let signed = tile_id as i8 as i16;
        (0x1000i16 + signed * 16) as usize
    } else {
        tile_id as usize * 16
    };
    let row_addr = tile_base + fine_y * 2;
    let low = mem.vram.get(row_addr).copied().unwrap_or(0);
    let high = mem.vram.get(row_addr + 1).copied().unwrap_or(0);
    let bit = 7usize.saturating_sub(fine_x & 7);
    (((high >> bit) & 1) << 1) | ((low >> bit) & 1)
}

fn bg_map_tile(mem: &Lr35902Memory, map_base: usize, y: usize, x: usize) -> u8 {
    let tile_row = (y / 8) & 31;
    let tile_col = (x / 8) & 31;
    let map_idx = map_base + tile_row * 32 + tile_col;
    mem.vram.get(map_idx).copied().unwrap_or(0)
}

fn window_start_x(mem: &Lr35902Memory) -> i16 {
    mem.io[IO_WX] as i16 - 7
}

fn window_visible_on_line(mem: &Lr35902Memory, lcdc: u8, ly: usize) -> bool {
    if (lcdc & 0x20) == 0 || (lcdc & 0x01) == 0 {
        return false;
    }
    let wy = mem.io[IO_WY] as usize;
    if ly < wy {
        return false;
    }
    window_start_x(mem) <= 159
}

fn bg_window_color_id(
    mem: &Lr35902Memory,
    lcdc: u8,
    ly: usize,
    x: usize,
    window_visible: bool,
    window_line: usize,
    wx_start: i16,
) -> u8 {
    let bg_enabled = (lcdc & 0x01) != 0;
    if !bg_enabled {
        return 0;
    }

    if window_visible && (x as i16) >= wx_start {
        let win_x = (x as i16 - wx_start) as usize;
        let win_y = window_line;
        let map_base = if (lcdc & 0x40) != 0 { 0x1c00usize } else { 0x1800usize };
        let tile_id = bg_map_tile(mem, map_base, win_y, win_x);
        return read_tile_color_id(mem, lcdc, tile_id, win_y % 8, win_x % 8);
    }

    let scx = mem.io[IO_SCX] as usize;
    let scy = mem.io[IO_SCY] as usize;
    let bg_x = (scx + x) & 0xff;
    let bg_y = (scy + ly) & 0xff;
    let map_base = if (lcdc & 0x08) != 0 { 0x1c00usize } else { 0x1800usize };
    let tile_id = bg_map_tile(mem, map_base, bg_y, bg_x);
    read_tile_color_id(mem, lcdc, tile_id, bg_y % 8, bg_x % 8)
}

#[derive(Clone, Copy)]
struct LineSprite {
    index: usize,
    x: i16,
    y: i16,
    tile: u8,
    flags: u8,
}

fn select_line_sprites(mem: &Lr35902Memory, ly: usize, mode2_dots: u16) -> Vec<LineSprite> {
    let lcdc = mem.io[IO_LCDC];
    if (lcdc & 0x02) == 0 {
        return Vec::new();
    }
    let sprite_height: i16 = if (lcdc & 0x04) != 0 { 16 } else { 8 };
    let entries_scanned = usize::from((mode2_dots / 2).min(40));
    let mut selected = Vec::<LineSprite>::new();
    for i in 0..entries_scanned {
        let base = i * 4;
        let y = mem.oam[base] as i16 - 16;
        let x = mem.oam[base + 1] as i16 - 8;
        let tile = mem.oam[base + 2];
        let flags = mem.oam[base + 3];
        if (ly as i16) >= y && (ly as i16) < y + sprite_height && selected.len() < 10 {
            selected.push(LineSprite { index: i, x, y, tile, flags });
        }
    }
    selected
}

fn render_scanline(mem: &mut Lr35902Memory, ly: usize) {
    if ly >= 144 {
        return;
    }
    let lcdc = mem.io[IO_LCDC];
    let bgp = mem.io[IO_BGP];
    let window_visible = window_visible_on_line(mem, lcdc, ly);
    let window_line = mem.window_line_counter as usize;
    let wx_start = window_start_x(mem);
    let mut bg_color_ids = [0u8; 160];

    for x in 0..160usize {
        let color_id = bg_window_color_id(mem, lcdc, ly, x, window_visible, window_line, wx_start);
        bg_color_ids[x] = color_id;
        let shade = bg_palette_shade(bgp, color_id);
        let fb_idx = ly * 160 + x;
        if fb_idx < mem.framebuffer.len() {
            mem.framebuffer[fb_idx] = shade;
        }
    }
    if window_visible {
        mem.window_line_counter = mem.window_line_counter.wrapping_add(1);
    }

    let mut line_sprites = select_line_sprites(mem, ly, 80);
    if line_sprites.is_empty() {
        return;
    }
    let sprite_height: i16 = if (lcdc & 0x04) != 0 { 16 } else { 8 };
    line_sprites.sort_by(|a, b| a.x.cmp(&b.x).then(a.index.cmp(&b.index)));

    for x in 0..160usize {
        for sprite in &line_sprites {
            let sx = x as i16 - sprite.x;
            if !(0..8).contains(&sx) {
                continue;
            }

            let mut sprite_row = (ly as i16 - sprite.y) as usize;
            if (sprite.flags & 0x40) != 0 {
                sprite_row = (sprite_height as usize - 1).saturating_sub(sprite_row);
            }

            let mut tile_id = sprite.tile;
            if sprite_height == 16 {
                tile_id &= 0xfe;
                if sprite_row >= 8 {
                    tile_id = tile_id.wrapping_add(1);
                    sprite_row -= 8;
                }
            }

            let fine_x = if (sprite.flags & 0x20) != 0 {
                sx as usize
            } else {
                7usize.saturating_sub(sx as usize)
            };
            let color_id = read_tile_color_id(mem, 0x10, tile_id, sprite_row, fine_x);
            if color_id == 0 {
                continue;
            }

            if (sprite.flags & 0x80) != 0 && bg_color_ids[x] != 0 {
                continue;
            }

            let palette = if (sprite.flags & 0x10) != 0 {
                mem.io[IO_OBP1]
            } else {
                mem.io[IO_OBP0]
            };
            let shade = bg_palette_shade(palette, color_id);
            let fb_idx = ly * 160 + x;
            if fb_idx < mem.framebuffer.len() {
                mem.framebuffer[fb_idx] = shade;
            }
            break;
        }
    }
}

fn advance_ppu_cycles(mem: &mut Lr35902Memory, cycles: u16) {
    if (mem.io[IO_LCDC] & 0x80) == 0 {
        mem.ppu_cycle_accum = 0;
        mem.io[IO_LY] = 0;
        mem.window_line_counter = 0;
        update_stat_and_irq(mem);
        return;
    }

    for _ in 0..cycles {
        mem.ppu_cycle_accum = mem.ppu_cycle_accum.wrapping_add(1);
        if mem.ppu_cycle_accum >= 456 {
            mem.ppu_cycle_accum = 0;
            let mut ly = mem.io[IO_LY].wrapping_add(1);
            if ly == 144 {
                request_interrupt(mem, IF_BIT_VBLANK);
            }
            if ly > 153 {
                ly = 0;
                mem.window_line_counter = 0;
            }
            mem.io[IO_LY] = ly;
        }
        update_stat_and_irq(mem);
    }
}

fn advance_clock(mem: &mut Lr35902Memory, cycles: u8) {
    let c = cycles as u16;

    if mem.dma_cycles_remaining > 0 {
        let consumed = c.min(mem.dma_cycles_remaining);
        mem.dma_cycles_remaining -= consumed;
        mem.dma_cycle_accum = mem.dma_cycle_accum.saturating_add(consumed as u8);
        while mem.dma_cycle_accum >= 4 && mem.dma_next_index < 160 {
            mem.dma_cycle_accum -= 4;
            let src = ((mem.dma_source_page as u16) << 8) | mem.dma_next_index as u16;
            mem.oam[mem.dma_next_index as usize] = read8_raw(mem, src);
            mem.dma_next_index = mem.dma_next_index.wrapping_add(1);
        }
        if mem.dma_cycles_remaining == 0 {
            mem.dma_next_index = 0;
            mem.dma_cycle_accum = 0;
        }
    }

    // DIV increments every 256 CPU cycles.
    mem.div_cycle_accum = mem.div_cycle_accum.saturating_add(c);
    while mem.div_cycle_accum >= 256 {
        mem.div_cycle_accum -= 256;
        mem.io[IO_DIV] = mem.io[IO_DIV].wrapping_add(1);
    }

    // TIMA uses TAC-selected period; on overflow reload TMA and request timer interrupt.
    let tac = mem.io[IO_TAC];
    if (tac & 0x04) != 0 {
        let period = timer_period_cycles(tac);
        mem.timer_cycle_accum = mem.timer_cycle_accum.saturating_add(c);
        while mem.timer_cycle_accum >= period {
            mem.timer_cycle_accum -= period;
            let (next, overflow) = mem.io[IO_TIMA].overflowing_add(1);
            if overflow {
                mem.io[IO_TIMA] = mem.io[IO_TMA];
                request_interrupt(mem, IF_BIT_TIMER);
            } else {
                mem.io[IO_TIMA] = next;
            }
        }
    }

    advance_ppu_cycles(mem, c);

    if mem.serial_cycles_remaining > 0 {
        if mem.serial_cycles_remaining > c {
            mem.serial_cycles_remaining -= c;
        } else {
            mem.serial_cycles_remaining = 0;
            mem.io[IO_SC] &= !0x80;
            request_interrupt(mem, IF_BIT_SERIAL);
        }
    }

    // APU frame sequencer cadence (512 Hz) for length/sweep/envelope stepping.
    mem.apu_cycle_accum = mem.apu_cycle_accum.saturating_add(c);
    while mem.apu_cycle_accum >= 8192 {
        mem.apu_cycle_accum -= 8192;
        mem.apu_frame_step = (mem.apu_frame_step + 1) & 0x07;
    }

    if (mem.io[IO_LCDC] & 0x80) == 0 {
        update_stat_and_irq(mem);
    }
}

fn get_hl(state: &Lr35902State) -> u16 {
    ((state.h as u16) << 8) | state.l as u16
}

fn set_hl(state: &mut Lr35902State, value: u16) {
    state.h = (value >> 8) as u8;
    state.l = (value & 0xff) as u8;
}

fn get_bc(state: &Lr35902State) -> u16 {
    ((state.b as u16) << 8) | state.c as u16
}

fn set_bc(state: &mut Lr35902State, value: u16) {
    state.b = (value >> 8) as u8;
    state.c = (value & 0xff) as u8;
}

fn get_de(state: &Lr35902State) -> u16 {
    ((state.d as u16) << 8) | state.e as u16
}

fn set_de(state: &mut Lr35902State, value: u16) {
    state.d = (value >> 8) as u8;
    state.e = (value & 0xff) as u8;
}

fn read_r8(state: &Lr35902State, mem: &Lr35902Memory, idx: u8) -> u8 {
    match idx {
        0 => state.b,
        1 => state.c,
        2 => state.d,
        3 => state.e,
        4 => state.h,
        5 => state.l,
        6 => read8(state, mem, get_hl(state)),
        _ => state.a,
    }
}

fn write_r8(state: &mut Lr35902State, mem: &mut Lr35902Memory, idx: u8, value: u8) {
    match idx {
        0 => state.b = value,
        1 => state.c = value,
        2 => state.d = value,
        3 => state.e = value,
        4 => state.h = value,
        5 => state.l = value,
        6 => write8(mem, get_hl(state), value),
        _ => state.a = value,
    }
}

fn set_flag(state: &mut Lr35902State, flag: u8, enabled: bool) {
    if enabled {
        state.f |= flag;
    } else {
        state.f &= !flag;
    }
}

fn get_flag(state: &Lr35902State, flag: u8) -> bool {
    (state.f & flag) != 0
}

fn fetch8(state: &mut Lr35902State, mem: &Lr35902Memory) -> u8 {
    let v = read8(state, mem, state.pc);
    state.pc = state.pc.wrapping_add(1);
    v
}

fn fetch16(state: &mut Lr35902State, mem: &Lr35902Memory) -> u16 {
    let lo = fetch8(state, mem) as u16;
    let hi = fetch8(state, mem) as u16;
    (hi << 8) | lo
}

fn xor_into_a(state: &mut Lr35902State, rhs: u8) {
    state.a ^= rhs;
    set_flag(state, FLAG_Z, state.a == 0);
    set_flag(state, FLAG_N, false);
    set_flag(state, FLAG_H, false);
    set_flag(state, FLAG_C, false);
}

fn cp_a(state: &mut Lr35902State, rhs: u8) {
    let a = state.a;
    let res = a.wrapping_sub(rhs);
    set_flag(state, FLAG_Z, res == 0);
    set_flag(state, FLAG_N, true);
    set_flag(state, FLAG_H, (a & 0x0f) < (rhs & 0x0f));
    set_flag(state, FLAG_C, a < rhs);
}

fn inc8(state: &mut Lr35902State, v: u8) -> u8 {
    let out = v.wrapping_add(1);
    let carry = get_flag(state, FLAG_C);
    set_flag(state, FLAG_Z, out == 0);
    set_flag(state, FLAG_N, false);
    set_flag(state, FLAG_H, (v & 0x0f) == 0x0f);
    set_flag(state, FLAG_C, carry);
    out
}

fn dec8(state: &mut Lr35902State, v: u8) -> u8 {
    let out = v.wrapping_sub(1);
    let carry = get_flag(state, FLAG_C);
    set_flag(state, FLAG_Z, out == 0);
    set_flag(state, FLAG_N, true);
    set_flag(state, FLAG_H, (v & 0x0f) == 0);
    set_flag(state, FLAG_C, carry);
    out
}

fn add_into_a(state: &mut Lr35902State, rhs: u8) {
    let a = state.a;
    let (out, carry) = a.overflowing_add(rhs);
    state.a = out;
    set_flag(state, FLAG_Z, out == 0);
    set_flag(state, FLAG_N, false);
    set_flag(state, FLAG_H, ((a & 0x0f) + (rhs & 0x0f)) > 0x0f);
    set_flag(state, FLAG_C, carry);
}

fn adc_into_a(state: &mut Lr35902State, rhs: u8) {
    let c = if get_flag(state, FLAG_C) { 1 } else { 0 };
    let a = state.a;
    let (tmp, carry1) = a.overflowing_add(rhs);
    let (out, carry2) = tmp.overflowing_add(c);
    state.a = out;
    set_flag(state, FLAG_Z, out == 0);
    set_flag(state, FLAG_N, false);
    set_flag(state, FLAG_H, ((a & 0x0f) + (rhs & 0x0f) + c) > 0x0f);
    set_flag(state, FLAG_C, carry1 || carry2);
}

fn sub_from_a(state: &mut Lr35902State, rhs: u8) {
    let a = state.a;
    let (out, borrow) = a.overflowing_sub(rhs);
    state.a = out;
    set_flag(state, FLAG_Z, out == 0);
    set_flag(state, FLAG_N, true);
    set_flag(state, FLAG_H, (a & 0x0f) < (rhs & 0x0f));
    set_flag(state, FLAG_C, borrow);
}

fn sbc_from_a(state: &mut Lr35902State, rhs: u8) {
    let c = if get_flag(state, FLAG_C) { 1 } else { 0 };
    let a = state.a;
    let (tmp, borrow1) = a.overflowing_sub(rhs);
    let (out, borrow2) = tmp.overflowing_sub(c);
    state.a = out;
    set_flag(state, FLAG_Z, out == 0);
    set_flag(state, FLAG_N, true);
    set_flag(state, FLAG_H, (a & 0x0f) < ((rhs & 0x0f) + c));
    set_flag(state, FLAG_C, borrow1 || borrow2);
}

fn and_into_a(state: &mut Lr35902State, rhs: u8) {
    state.a &= rhs;
    set_flag(state, FLAG_Z, state.a == 0);
    set_flag(state, FLAG_N, false);
    set_flag(state, FLAG_H, true);
    set_flag(state, FLAG_C, false);
}

fn or_into_a(state: &mut Lr35902State, rhs: u8) {
    state.a |= rhs;
    set_flag(state, FLAG_Z, state.a == 0);
    set_flag(state, FLAG_N, false);
    set_flag(state, FLAG_H, false);
    set_flag(state, FLAG_C, false);
}

fn daa(state: &mut Lr35902State) {
    let mut a = state.a;
    let mut adjust = 0u8;
    let mut carry_out = get_flag(state, FLAG_C);
    let n = get_flag(state, FLAG_N);
    let h = get_flag(state, FLAG_H);
    let c = get_flag(state, FLAG_C);

    if !n {
        if h || (a & 0x0f) > 0x09 {
            adjust |= 0x06;
        }
        if c || a > 0x99 {
            adjust |= 0x60;
            carry_out = true;
        }
        a = a.wrapping_add(adjust);
    } else {
        if h {
            adjust |= 0x06;
        }
        if c {
            adjust |= 0x60;
        }
        a = a.wrapping_sub(adjust);
    }

    state.a = a;
    set_flag(state, FLAG_Z, state.a == 0);
    set_flag(state, FLAG_H, false);
    set_flag(state, FLAG_C, carry_out);
}

fn condition_holds(state: &Lr35902State, cc: u8) -> bool {
    match cc & 0x03 {
        0 => !get_flag(state, FLAG_Z), // NZ
        1 => get_flag(state, FLAG_Z),  // Z
        2 => !get_flag(state, FLAG_C), // NC
        _ => get_flag(state, FLAG_C),  // C
    }
}

fn add_hl(state: &mut Lr35902State, rhs: u16) {
    let hl = get_hl(state);
    let out = hl.wrapping_add(rhs);
    set_flag(state, FLAG_N, false);
    set_flag(state, FLAG_H, ((hl & 0x0fff) + (rhs & 0x0fff)) > 0x0fff);
    set_flag(state, FLAG_C, (hl as u32 + rhs as u32) > 0xffff);
    set_hl(state, out);
}

fn add_sp_signed(state: &mut Lr35902State, delta: i8) -> u16 {
    let sp = state.sp;
    let d = delta as i16 as u16;
    let out = sp.wrapping_add(d);
    set_flag(state, FLAG_Z, false);
    set_flag(state, FLAG_N, false);
    set_flag(state, FLAG_H, ((sp & 0x0f) + (d & 0x0f)) > 0x0f);
    set_flag(state, FLAG_C, ((sp & 0xff) + (d & 0xff)) > 0xff);
    out
}

fn push16(state: &mut Lr35902State, mem: &mut Lr35902Memory, value: u16) {
    state.sp = state.sp.wrapping_sub(1);
    write8(mem, state.sp, (value >> 8) as u8);
    state.sp = state.sp.wrapping_sub(1);
    write8(mem, state.sp, (value & 0xff) as u8);
}

fn pop16(state: &mut Lr35902State, mem: &Lr35902Memory) -> u16 {
    let lo = read8(state, mem, state.sp);
    state.sp = state.sp.wrapping_add(1);
    let hi = read8(state, mem, state.sp);
    state.sp = state.sp.wrapping_add(1);
    ((hi as u16) << 8) | lo as u16
}

fn interrupt_vector(pending_mask: u8) -> u16 {
    if (pending_mask & 0x01) != 0 { 0x40 }
    else if (pending_mask & 0x02) != 0 { 0x48 }
    else if (pending_mask & 0x04) != 0 { 0x50 }
    else if (pending_mask & 0x08) != 0 { 0x58 }
    else if (pending_mask & 0x10) != 0 { 0x60 }
    else { 0x40 }
}

fn service_interrupt(state: &mut Lr35902State, mem: &mut Lr35902Memory, pending_mask: u8) -> StepResult {
    let vector = interrupt_vector(pending_mask);
    let bit = match vector {
        0x40 => 0x01,
        0x48 => 0x02,
        0x50 => 0x04,
        0x58 => 0x08,
        0x60 => 0x10,
        _ => 0x01,
    };
    let if_idx = IO_IF;
    mem.io[if_idx] &= !bit;
    state.ime = false;
    state.halted = false;
    let ret = state.pc;
    push16(state, mem, ret);
    state.pc = vector;
    advance_clock(mem, 20);
    state.cycles = state.cycles.saturating_add(20);
    StepResult {
        state: state.clone(),
        memory: mem.clone(),
        effect: StepEffect::Interrupt { vector },
        opcode: 0xff,
        cycles: 20,
    }
}

fn cb_read_operand(state: &Lr35902State, mem: &Lr35902Memory, idx: u8) -> u8 {
    match idx {
        0 => state.b,
        1 => state.c,
        2 => state.d,
        3 => state.e,
        4 => state.h,
        5 => state.l,
        6 => read8(state, mem, get_hl(state)),
        _ => state.a,
    }
}

fn cb_write_operand(state: &mut Lr35902State, mem: &mut Lr35902Memory, idx: u8, value: u8) {
    match idx {
        0 => state.b = value,
        1 => state.c = value,
        2 => state.d = value,
        3 => state.e = value,
        4 => state.h = value,
        5 => state.l = value,
        6 => write8(mem, get_hl(state), value),
        _ => state.a = value,
    }
}

fn exec_cb_opcode(state: &mut Lr35902State, mem: &mut Lr35902Memory, cb: u8) -> u8 {
    let x = cb >> 6;
    let y = (cb >> 3) & 0x07;
    let z = cb & 0x07;
    let mut v = cb_read_operand(state, mem, z);
    let is_mem = z == 6;
    match x {
        0 => {
            match y {
                0 => { // RLC
                    let c = (v & 0x80) != 0;
                    v = (v << 1) | if c { 1 } else { 0 };
                    set_flag(state, FLAG_Z, v == 0);
                    set_flag(state, FLAG_N, false);
                    set_flag(state, FLAG_H, false);
                    set_flag(state, FLAG_C, c);
                }
                1 => { // RRC
                    let c = (v & 0x01) != 0;
                    v = (v >> 1) | if c { 0x80 } else { 0 };
                    set_flag(state, FLAG_Z, v == 0);
                    set_flag(state, FLAG_N, false);
                    set_flag(state, FLAG_H, false);
                    set_flag(state, FLAG_C, c);
                }
                2 => { // RL
                    let carry_in = if get_flag(state, FLAG_C) { 1 } else { 0 };
                    let c = (v & 0x80) != 0;
                    v = (v << 1) | carry_in;
                    set_flag(state, FLAG_Z, v == 0);
                    set_flag(state, FLAG_N, false);
                    set_flag(state, FLAG_H, false);
                    set_flag(state, FLAG_C, c);
                }
                3 => { // RR
                    let carry_in = if get_flag(state, FLAG_C) { 0x80 } else { 0 };
                    let c = (v & 0x01) != 0;
                    v = (v >> 1) | carry_in;
                    set_flag(state, FLAG_Z, v == 0);
                    set_flag(state, FLAG_N, false);
                    set_flag(state, FLAG_H, false);
                    set_flag(state, FLAG_C, c);
                }
                4 => { // SLA
                    let c = (v & 0x80) != 0;
                    v <<= 1;
                    set_flag(state, FLAG_Z, v == 0);
                    set_flag(state, FLAG_N, false);
                    set_flag(state, FLAG_H, false);
                    set_flag(state, FLAG_C, c);
                }
                5 => { // SRA
                    let c = (v & 0x01) != 0;
                    let msb = v & 0x80;
                    v = (v >> 1) | msb;
                    set_flag(state, FLAG_Z, v == 0);
                    set_flag(state, FLAG_N, false);
                    set_flag(state, FLAG_H, false);
                    set_flag(state, FLAG_C, c);
                }
                6 => { // SWAP
                    v = v.rotate_left(4);
                    set_flag(state, FLAG_Z, v == 0);
                    set_flag(state, FLAG_N, false);
                    set_flag(state, FLAG_H, false);
                    set_flag(state, FLAG_C, false);
                }
                _ => { // SRL
                    let c = (v & 0x01) != 0;
                    v >>= 1;
                    set_flag(state, FLAG_Z, v == 0);
                    set_flag(state, FLAG_N, false);
                    set_flag(state, FLAG_H, false);
                    set_flag(state, FLAG_C, c);
                }
            }
            cb_write_operand(state, mem, z, v);
            if is_mem { 16 } else { 8 }
        }
        1 => { // BIT
            let bit_set = (v & (1u8 << y)) != 0;
            set_flag(state, FLAG_Z, !bit_set);
            set_flag(state, FLAG_N, false);
            set_flag(state, FLAG_H, true);
            if is_mem { 12 } else { 8 }
        }
        2 => { // RES
            v &= !(1u8 << y);
            cb_write_operand(state, mem, z, v);
            if is_mem { 16 } else { 8 }
        }
        _ => { // SET
            v |= 1u8 << y;
            cb_write_operand(state, mem, z, v);
            if is_mem { 16 } else { 8 }
        }
    }
}

fn exec_grouped_opcode(
    opcode: u8,
    state: &mut Lr35902State,
    memory: &mut Lr35902Memory,
    _effect: &mut StepEffect,
) -> Option<u8> {
    if (0x40..=0x7f).contains(&opcode) && opcode != 0x76 {
        let dst = (opcode >> 3) & 0x07;
        let src = opcode & 0x07;
        let v = read_r8(state, memory, src);
        write_r8(state, memory, dst, v);
        return Some(if src == 6 || dst == 6 { 8 } else { 4 });
    }

    if (0xb8..=0xbf).contains(&opcode) {
        let v = read_r8(state, memory, opcode & 0x07);
        cp_a(state, v);
        return Some(if (opcode & 0x07) == 6 { 8 } else { 4 });
    }

    if (opcode & 0xc7) == 0x04 {
        let r = (opcode >> 3) & 0x07;
        let v = read_r8(state, memory, r);
        let out = inc8(state, v);
        write_r8(state, memory, r, out);
        return Some(if r == 6 { 12 } else { 4 });
    }

    if (opcode & 0xc7) == 0x05 {
        let r = (opcode >> 3) & 0x07;
        let v = read_r8(state, memory, r);
        let out = dec8(state, v);
        write_r8(state, memory, r, out);
        return Some(if r == 6 { 12 } else { 4 });
    }

    if (opcode & 0xc7) == 0x06 {
        let r = (opcode >> 3) & 0x07;
        let imm = fetch8(state, memory);
        write_r8(state, memory, r, imm);
        return Some(if r == 6 { 12 } else { 8 });
    }

    match opcode {
        0x01 => { let v = fetch16(state, memory); set_bc(state, v); Some(12) } // LD BC,d16
        0x11 => { let v = fetch16(state, memory); set_de(state, v); Some(12) } // LD DE,d16
        0x21 => { let v = fetch16(state, memory); set_hl(state, v); Some(12) } // LD HL,d16
        0x31 => { state.sp = fetch16(state, memory); Some(12) } // LD SP,d16
        0x02 => { write8(memory, get_bc(state), state.a); Some(8) } // LD (BC),A
        0x12 => { write8(memory, get_de(state), state.a); Some(8) } // LD (DE),A
        0x22 => { // LDI (HL),A
            let hl = get_hl(state);
            write8(memory, hl, state.a);
            set_hl(state, hl.wrapping_add(1));
            Some(8)
        }
        0x2a => { // LDI A,(HL)
            let hl = get_hl(state);
            state.a = read8(state, memory, hl);
            set_hl(state, hl.wrapping_add(1));
            Some(8)
        }
        0x0a => { state.a = read8(state, memory, get_bc(state)); Some(8) } // LD A,(BC)
        0x1a => { state.a = read8(state, memory, get_de(state)); Some(8) } // LD A,(DE)
        0x3a => { // LDD A,(HL)
            let hl = get_hl(state);
            state.a = read8(state, memory, hl);
            set_hl(state, hl.wrapping_sub(1));
            Some(8)
        }
        0x08 => { // LD (a16),SP
            let addr = fetch16(state, memory);
            write8(memory, addr, (state.sp & 0xff) as u8);
            write8(memory, addr.wrapping_add(1), (state.sp >> 8) as u8);
            Some(20)
        }
        0x09 => { add_hl(state, get_bc(state)); Some(8) } // ADD HL,BC
        0x19 => { add_hl(state, get_de(state)); Some(8) } // ADD HL,DE
        0x29 => { add_hl(state, get_hl(state)); Some(8) } // ADD HL,HL
        0x39 => { add_hl(state, state.sp); Some(8) } // ADD HL,SP
        0x03 => { set_bc(state, get_bc(state).wrapping_add(1)); Some(8) } // INC BC
        0x13 => { set_de(state, get_de(state).wrapping_add(1)); Some(8) } // INC DE
        0x23 => { set_hl(state, get_hl(state).wrapping_add(1)); Some(8) } // INC HL
        0x33 => { state.sp = state.sp.wrapping_add(1); Some(8) } // INC SP
        0x0b => { set_bc(state, get_bc(state).wrapping_sub(1)); Some(8) } // DEC BC
        0x1b => { set_de(state, get_de(state).wrapping_sub(1)); Some(8) } // DEC DE
        0x2b => { set_hl(state, get_hl(state).wrapping_sub(1)); Some(8) } // DEC HL
        0x3b => { state.sp = state.sp.wrapping_sub(1); Some(8) } // DEC SP
        0xd1 => { // POP DE
            let v = pop16(state, memory);
            set_de(state, v);
            Some(12)
        }
        0xe1 => { // POP HL
            let v = pop16(state, memory);
            set_hl(state, v);
            Some(12)
        }
        0xf1 => { // POP AF
            let v = pop16(state, memory);
            state.a = (v >> 8) as u8;
            state.f = (v as u8) & 0xf0;
            Some(12)
        }
        0xd5 => { push16(state, memory, get_de(state)); Some(16) } // PUSH DE
        0xe5 => { push16(state, memory, get_hl(state)); Some(16) } // PUSH HL
        0xf5 => { push16(state, memory, ((state.a as u16) << 8) | (state.f as u16)); Some(16) } // PUSH AF
        0xe2 => { // LD (C),A
            write8(memory, 0xff00u16 + state.c as u16, state.a);
            Some(8)
        }
        0xf2 => { // LD A,(C)
            state.a = read8(state, memory, 0xff00u16 + state.c as u16);
            Some(8)
        }
        0xea => { // LD (a16),A
            let addr = fetch16(state, memory);
            write8(memory, addr, state.a);
            Some(16)
        }
        0xfa => { // LD A,(a16)
            let addr = fetch16(state, memory);
            state.a = read8(state, memory, addr);
            Some(16)
        }
        0xf8 => { // LD HL,SP+r8
            let d = fetch8(state, memory) as i8;
            let out = add_sp_signed(state, d);
            set_hl(state, out);
            Some(12)
        }
        0xe8 => { // ADD SP,r8
            let d = fetch8(state, memory) as i8;
            state.sp = add_sp_signed(state, d);
            Some(16)
        }
        0xf9 => { state.sp = get_hl(state); Some(8) } // LD SP,HL
        0x07 => { // RLCA
            let c = (state.a & 0x80) != 0;
            state.a = (state.a << 1) | if c { 1 } else { 0 };
            set_flag(state, FLAG_Z, false);
            set_flag(state, FLAG_N, false);
            set_flag(state, FLAG_H, false);
            set_flag(state, FLAG_C, c);
            Some(4)
        }
        0x0f => { // RRCA
            let c = (state.a & 0x01) != 0;
            state.a = (state.a >> 1) | if c { 0x80 } else { 0 };
            set_flag(state, FLAG_Z, false);
            set_flag(state, FLAG_N, false);
            set_flag(state, FLAG_H, false);
            set_flag(state, FLAG_C, c);
            Some(4)
        }
        0x17 => { // RLA
            let carry = if get_flag(state, FLAG_C) { 1 } else { 0 };
            let c = (state.a & 0x80) != 0;
            state.a = (state.a << 1) | carry;
            set_flag(state, FLAG_Z, false);
            set_flag(state, FLAG_N, false);
            set_flag(state, FLAG_H, false);
            set_flag(state, FLAG_C, c);
            Some(4)
        }
        0x1f => { // RRA
            let carry = if get_flag(state, FLAG_C) { 0x80 } else { 0 };
            let c = (state.a & 0x01) != 0;
            state.a = (state.a >> 1) | carry;
            set_flag(state, FLAG_Z, false);
            set_flag(state, FLAG_N, false);
            set_flag(state, FLAG_H, false);
            set_flag(state, FLAG_C, c);
            Some(4)
        }
        0x30 => { // JR NC,r8
            let d = fetch8(state, memory) as i8;
            if condition_holds(state, 2) {
                state.pc = ((state.pc as i32) + (d as i32)) as u16;
                Some(12)
            } else {
                Some(8)
            }
        }
        0x38 => { // JR C,r8
            let d = fetch8(state, memory) as i8;
            if condition_holds(state, 3) {
                state.pc = ((state.pc as i32) + (d as i32)) as u16;
                Some(12)
            } else {
                Some(8)
            }
        }
        0x10 => { // STOP
            let _stop_pad = fetch8(state, memory);
            state.halted = true;
            Some(4)
        }
        _ => None,
    }
}

fn step_lr35902(mut state: Lr35902State, mut memory: Lr35902Memory) -> StepResult {
    let pending = memory.ie & memory.io[IO_IF] & 0x1f;
    if pending != 0 {
        state.halted = false;
        if state.ime {
            return service_interrupt(&mut state, &mut memory, pending);
        }
    }
    if state.halted {
        advance_clock(&mut memory, 4);
        state.cycles = state.cycles.saturating_add(4);
        return StepResult { state, memory, effect: StepEffect::Halt, opcode: 0x76, cycles: 4 };
    }

    let opcode = fetch8(&mut state, &memory);
    let mut effect = StepEffect::None;
    let cycles = match opcode {
        0x00 => 4, // NOP
        0x3e => { state.a = fetch8(&mut state, &memory); 8 } // LD A,d8
        0x06 => { state.b = fetch8(&mut state, &memory); 8 } // LD B,d8
        0x0e => { state.c = fetch8(&mut state, &memory); 8 } // LD C,d8
        0x16 => { state.d = fetch8(&mut state, &memory); 8 } // LD D,d8
        0x1e => { state.e = fetch8(&mut state, &memory); 8 } // LD E,d8
        0x26 => { state.h = fetch8(&mut state, &memory); 8 } // LD H,d8
        0x2e => { state.l = fetch8(&mut state, &memory); 8 } // LD L,d8
        0x7f => 4, // LD A,A
        0x78 => { state.a = state.b; 4 } // LD A,B
        0x79 => { state.a = state.c; 4 } // LD A,C
        0x7a => { state.a = state.d; 4 } // LD A,D
        0x7b => { state.a = state.e; 4 } // LD A,E
        0x7c => { state.a = state.h; 4 } // LD A,H
        0x7d => { state.a = state.l; 4 } // LD A,L
        0x7e => { state.a = read8(&state, &memory, get_hl(&state)); 8 } // LD A,(HL)
        0x77 => { write8(&mut memory, get_hl(&state), state.a); 8 } // LD (HL),A
        0x80 => { let v = state.b; add_into_a(&mut state, v); 4 } // ADD A,B
        0x81 => { let v = state.c; add_into_a(&mut state, v); 4 } // ADD A,C
        0x82 => { let v = state.d; add_into_a(&mut state, v); 4 } // ADD A,D
        0x83 => { let v = state.e; add_into_a(&mut state, v); 4 } // ADD A,E
        0x84 => { let v = state.h; add_into_a(&mut state, v); 4 } // ADD A,H
        0x85 => { let v = state.l; add_into_a(&mut state, v); 4 } // ADD A,L
        0x86 => { let v = read8(&state, &memory, get_hl(&state)); add_into_a(&mut state, v); 8 } // ADD A,(HL)
        0x87 => { let v = state.a; add_into_a(&mut state, v); 4 } // ADD A,A
        0xc6 => { let v = fetch8(&mut state, &memory); add_into_a(&mut state, v); 8 } // ADD A,d8
        0x88 => { let v = state.b; adc_into_a(&mut state, v); 4 } // ADC A,B
        0x89 => { let v = state.c; adc_into_a(&mut state, v); 4 } // ADC A,C
        0x8a => { let v = state.d; adc_into_a(&mut state, v); 4 } // ADC A,D
        0x8b => { let v = state.e; adc_into_a(&mut state, v); 4 } // ADC A,E
        0x8c => { let v = state.h; adc_into_a(&mut state, v); 4 } // ADC A,H
        0x8d => { let v = state.l; adc_into_a(&mut state, v); 4 } // ADC A,L
        0x8e => { let v = read8(&state, &memory, get_hl(&state)); adc_into_a(&mut state, v); 8 } // ADC A,(HL)
        0x8f => { let v = state.a; adc_into_a(&mut state, v); 4 } // ADC A,A
        0xce => { let v = fetch8(&mut state, &memory); adc_into_a(&mut state, v); 8 } // ADC A,d8
        0x90 => { let v = state.b; sub_from_a(&mut state, v); 4 } // SUB B
        0x91 => { let v = state.c; sub_from_a(&mut state, v); 4 } // SUB C
        0x92 => { let v = state.d; sub_from_a(&mut state, v); 4 } // SUB D
        0x93 => { let v = state.e; sub_from_a(&mut state, v); 4 } // SUB E
        0x94 => { let v = state.h; sub_from_a(&mut state, v); 4 } // SUB H
        0x95 => { let v = state.l; sub_from_a(&mut state, v); 4 } // SUB L
        0x96 => { let v = read8(&state, &memory, get_hl(&state)); sub_from_a(&mut state, v); 8 } // SUB (HL)
        0x97 => { let v = state.a; sub_from_a(&mut state, v); 4 } // SUB A
        0xd6 => { let v = fetch8(&mut state, &memory); sub_from_a(&mut state, v); 8 } // SUB d8
        0x98 => { let v = state.b; sbc_from_a(&mut state, v); 4 } // SBC A,B
        0x99 => { let v = state.c; sbc_from_a(&mut state, v); 4 } // SBC A,C
        0x9a => { let v = state.d; sbc_from_a(&mut state, v); 4 } // SBC A,D
        0x9b => { let v = state.e; sbc_from_a(&mut state, v); 4 } // SBC A,E
        0x9c => { let v = state.h; sbc_from_a(&mut state, v); 4 } // SBC A,H
        0x9d => { let v = state.l; sbc_from_a(&mut state, v); 4 } // SBC A,L
        0x9e => { let v = read8(&state, &memory, get_hl(&state)); sbc_from_a(&mut state, v); 8 } // SBC A,(HL)
        0x9f => { let v = state.a; sbc_from_a(&mut state, v); 4 } // SBC A,A
        0xde => { let v = fetch8(&mut state, &memory); sbc_from_a(&mut state, v); 8 } // SBC A,d8
        0xa0 => { let v = state.b; and_into_a(&mut state, v); 4 } // AND B
        0xa1 => { let v = state.c; and_into_a(&mut state, v); 4 } // AND C
        0xa2 => { let v = state.d; and_into_a(&mut state, v); 4 } // AND D
        0xa3 => { let v = state.e; and_into_a(&mut state, v); 4 } // AND E
        0xa4 => { let v = state.h; and_into_a(&mut state, v); 4 } // AND H
        0xa5 => { let v = state.l; and_into_a(&mut state, v); 4 } // AND L
        0xa6 => { let v = read8(&state, &memory, get_hl(&state)); and_into_a(&mut state, v); 8 } // AND (HL)
        0xa7 => { let v = state.a; and_into_a(&mut state, v); 4 } // AND A
        0xe6 => { let v = fetch8(&mut state, &memory); and_into_a(&mut state, v); 8 } // AND d8
        0xaf => { let v = state.a; xor_into_a(&mut state, v); 4 } // XOR A
        0xa8 => { let v = state.b; xor_into_a(&mut state, v); 4 } // XOR B
        0xa9 => { let v = state.c; xor_into_a(&mut state, v); 4 } // XOR C
        0xaa => { let v = state.d; xor_into_a(&mut state, v); 4 } // XOR D
        0xab => { let v = state.e; xor_into_a(&mut state, v); 4 } // XOR E
        0xac => { let v = state.h; xor_into_a(&mut state, v); 4 } // XOR H
        0xad => { let v = state.l; xor_into_a(&mut state, v); 4 } // XOR L
        0xae => { let v = read8(&state, &memory, get_hl(&state)); xor_into_a(&mut state, v); 8 } // XOR (HL)
        0xee => { let v = fetch8(&mut state, &memory); xor_into_a(&mut state, v); 8 } // XOR d8
        0xb0 => { let v = state.b; or_into_a(&mut state, v); 4 } // OR B
        0xb1 => { let v = state.c; or_into_a(&mut state, v); 4 } // OR C
        0xb2 => { let v = state.d; or_into_a(&mut state, v); 4 } // OR D
        0xb3 => { let v = state.e; or_into_a(&mut state, v); 4 } // OR E
        0xb4 => { let v = state.h; or_into_a(&mut state, v); 4 } // OR H
        0xb5 => { let v = state.l; or_into_a(&mut state, v); 4 } // OR L
        0xb6 => { let v = read8(&state, &memory, get_hl(&state)); or_into_a(&mut state, v); 8 } // OR (HL)
        0xb7 => { let v = state.a; or_into_a(&mut state, v); 4 } // OR A
        0xf6 => { let v = fetch8(&mut state, &memory); or_into_a(&mut state, v); 8 } // OR d8
        0xfe => { let v = fetch8(&mut state, &memory); cp_a(&mut state, v); 8 } // CP d8
        0xc3 => { state.pc = fetch16(&mut state, &memory); 16 } // JP a16
        0xc2 => { // JP NZ,a16
            let addr = fetch16(&mut state, &memory);
            if condition_holds(&state, 0) { state.pc = addr; 16 } else { 12 }
        }
        0xca => { // JP Z,a16
            let addr = fetch16(&mut state, &memory);
            if condition_holds(&state, 1) { state.pc = addr; 16 } else { 12 }
        }
        0xd2 => { // JP NC,a16
            let addr = fetch16(&mut state, &memory);
            if condition_holds(&state, 2) { state.pc = addr; 16 } else { 12 }
        }
        0xda => { // JP C,a16
            let addr = fetch16(&mut state, &memory);
            if condition_holds(&state, 3) { state.pc = addr; 16 } else { 12 }
        }
        0xe9 => { // JP (HL)
            state.pc = get_hl(&state);
            4
        }
        0xcd => { // CALL a16
            let addr = fetch16(&mut state, &memory);
            let ret = state.pc;
            push16(&mut state, &mut memory, ret);
            state.pc = addr;
            24
        }
        0xc4 => { // CALL NZ,a16
            let addr = fetch16(&mut state, &memory);
            if condition_holds(&state, 0) {
                let ret = state.pc;
                push16(&mut state, &mut memory, ret);
                state.pc = addr;
                24
            } else {
                12
            }
        }
        0xcc => { // CALL Z,a16
            let addr = fetch16(&mut state, &memory);
            if condition_holds(&state, 1) {
                let ret = state.pc;
                push16(&mut state, &mut memory, ret);
                state.pc = addr;
                24
            } else {
                12
            }
        }
        0xd4 => { // CALL NC,a16
            let addr = fetch16(&mut state, &memory);
            if condition_holds(&state, 2) {
                let ret = state.pc;
                push16(&mut state, &mut memory, ret);
                state.pc = addr;
                24
            } else {
                12
            }
        }
        0xdc => { // CALL C,a16
            let addr = fetch16(&mut state, &memory);
            if condition_holds(&state, 3) {
                let ret = state.pc;
                push16(&mut state, &mut memory, ret);
                state.pc = addr;
                24
            } else {
                12
            }
        }
        0xc9 => { // RET
            state.pc = pop16(&mut state, &memory);
            16
        }
        0xc0 => { // RET NZ
            if condition_holds(&state, 0) {
                state.pc = pop16(&mut state, &memory);
                20
            } else {
                8
            }
        }
        0xc8 => { // RET Z
            if condition_holds(&state, 1) {
                state.pc = pop16(&mut state, &memory);
                20
            } else {
                8
            }
        }
        0xd0 => { // RET NC
            if condition_holds(&state, 2) {
                state.pc = pop16(&mut state, &memory);
                20
            } else {
                8
            }
        }
        0xd8 => { // RET C
            if condition_holds(&state, 3) {
                state.pc = pop16(&mut state, &memory);
                20
            } else {
                8
            }
        }
        0xd9 => { // RETI
            state.pc = pop16(&mut state, &memory);
            state.ime = true;
            16
        }
        0xc7 => { let ret = state.pc; push16(&mut state, &mut memory, ret); state.pc = 0x00; 16 } // RST 00H
        0xcf => { let ret = state.pc; push16(&mut state, &mut memory, ret); state.pc = 0x08; 16 } // RST 08H
        0xd7 => { let ret = state.pc; push16(&mut state, &mut memory, ret); state.pc = 0x10; 16 } // RST 10H
        0xdf => { let ret = state.pc; push16(&mut state, &mut memory, ret); state.pc = 0x18; 16 } // RST 18H
        0xe7 => { let ret = state.pc; push16(&mut state, &mut memory, ret); state.pc = 0x20; 16 } // RST 20H
        0xef => { let ret = state.pc; push16(&mut state, &mut memory, ret); state.pc = 0x28; 16 } // RST 28H
        0xf7 => { let ret = state.pc; push16(&mut state, &mut memory, ret); state.pc = 0x30; 16 } // RST 30H
        0xff => { let ret = state.pc; push16(&mut state, &mut memory, ret); state.pc = 0x38; 16 } // RST 38H
        0xc5 => { // PUSH BC
            let v = ((state.b as u16) << 8) | state.c as u16;
            push16(&mut state, &mut memory, v);
            16
        }
        0xc1 => { // POP BC
            let v = pop16(&mut state, &memory);
            state.b = (v >> 8) as u8;
            state.c = (v & 0xff) as u8;
            12
        }
        0x18 => { let d = fetch8(&mut state, &memory) as i8; state.pc = ((state.pc as i32) + (d as i32)) as u16; 12 } // JR r8
        0x20 => { // JR NZ,r8
            let d = fetch8(&mut state, &memory) as i8;
            if !get_flag(&state, FLAG_Z) {
                state.pc = ((state.pc as i32) + (d as i32)) as u16;
                12
            } else {
                8
            }
        }
        0x28 => { // JR Z,r8
            let d = fetch8(&mut state, &memory) as i8;
            if get_flag(&state, FLAG_Z) {
                state.pc = ((state.pc as i32) + (d as i32)) as u16;
                12
            } else {
                8
            }
        }
        0x32 => { // LDD (HL),A
            let hl = get_hl(&state);
            write8(&mut memory, hl, state.a);
            set_hl(&mut state, hl.wrapping_sub(1));
            8
        }
        0x2a => { // LDI A,(HL)
            let hl = get_hl(&state);
            state.a = read8(&state, &memory, hl);
            set_hl(&mut state, hl.wrapping_add(1));
            8
        }
        0x3c => { let a = state.a; state.a = inc8(&mut state, a); 4 } // INC A
        0x3d => { let a = state.a; state.a = dec8(&mut state, a); 4 } // DEC A
        0x27 => { daa(&mut state); 4 } // DAA
        0x2f => { // CPL
            state.a ^= 0xff;
            set_flag(&mut state, FLAG_N, true);
            set_flag(&mut state, FLAG_H, true);
            4
        }
        0x37 => { // SCF
            set_flag(&mut state, FLAG_N, false);
            set_flag(&mut state, FLAG_H, false);
            set_flag(&mut state, FLAG_C, true);
            4
        }
        0x3f => { // CCF
            let c = get_flag(&state, FLAG_C);
            set_flag(&mut state, FLAG_N, false);
            set_flag(&mut state, FLAG_H, false);
            set_flag(&mut state, FLAG_C, !c);
            4
        }
        0xf3 => { state.ime = false; state.ime_enable_delay = 0; 4 } // DI
        0xfb => { state.ime_enable_delay = 2; 4 } // EI (IME enable delayed by one instruction)
        0x76 => { state.halted = true; effect = StepEffect::Halt; 4 } // HALT
        0xe0 => { // LDH [a8],A
            let port = fetch8(&mut state, &memory);
            let addr = 0xff00u16 + port as u16;
            write8(&mut memory, addr, state.a);
            effect = StepEffect::PortWrite { port, value: state.a };
            12
        }
        0xf0 => { // LDH A,[a8]
            let port = fetch8(&mut state, &memory);
            state.a = read8(&state, &memory, 0xff00u16 + port as u16);
            12
        }
        0xcb => { // CB-prefixed bit/rotate group
            let cb = fetch8(&mut state, &memory);
            exec_cb_opcode(&mut state, &mut memory, cb)
        }
        _ => {
            exec_grouped_opcode(opcode, &mut state, &mut memory, &mut effect).unwrap_or(4)
        }
    };

    advance_clock(&mut memory, cycles);
    state.f &= 0xf0;
    if state.ime_enable_delay > 0 {
        state.ime_enable_delay -= 1;
        if state.ime_enable_delay == 0 {
            state.ime = true;
        }
    }
    state.cycles = state.cycles.saturating_add(cycles as u64);
    StepResult { state, memory, effect, opcode, cycles }
}

#[derive(Clone)]
struct SourceLine {
    text: String,
    file: String,
    line: usize,
}

#[derive(Clone)]
struct CanonLine {
    text: String,
    file: String,
    line: usize,
    canon: usize,
}

#[derive(Clone)]
struct MacroDef {
    body: Vec<CanonLine>,
}

#[derive(Clone, Serialize, JsonSchema)]
struct SourceProvenance {
    kind: String,
    symbol: String,
    source_file: String,
    source_line_start: usize,
    source_line_end: usize,
    canonical_line_start: usize,
    canonical_line_end: usize,
}

#[derive(Serialize)]
struct GameboyMap {
    translit_units: Vec<TranslitUnit>,
    parity_trace_schema: ParityTraceFrame,
    source_provenance: Vec<SourceProvenance>,
    rom_model_path: String,
    parity_harness_path: String,
    trace_path: String,
    diagnostics_path: String,
}

#[derive(Clone, Serialize, JsonSchema)]
struct RomSection {
    name: String,
    bank: u16,
    start: u16,
    end: u16,
    bytes: usize,
}

#[derive(Clone, Serialize, JsonSchema)]
struct RomBank {
    bank: u16,
    used_bytes: usize,
}

#[derive(Clone, Serialize, JsonSchema)]
struct GameboyRomModel {
    sections: Vec<RomSection>,
    banks: Vec<RomBank>,
}

#[derive(Clone, Serialize, JsonSchema)]
struct ParityCheckpoint {
    tick: u64,
    label: String,
    source_line: usize,
}

#[derive(Clone, Serialize, JsonSchema)]
struct SemanticTraceSample {
    step: u64,
    pc: u16,
    opcode: u8,
    cycles: u8,
}

#[derive(Clone, Serialize, JsonSchema)]
struct TraceFrame {
    step: u64,
    pc: u16,
    opcode: u8,
    cycles: u8,
    a: u8,
    b: u8,
    c: u8,
    d: u8,
    e: u8,
    h: u8,
    l: u8,
    f: u8,
    sp: u16,
    ime: bool,
    halted: bool,
    effect: String,
}

#[derive(Clone, Serialize, JsonSchema)]
struct GameboyTraceArtifact {
    frames: Vec<TraceFrame>,
}

#[derive(Clone, Serialize, JsonSchema)]
struct GameboyParityHarness {
    checkpoints: Vec<ParityCheckpoint>,
    graph_nodes: usize,
    graph_edges: usize,
    semantic_probe_opcode: u8,
    semantic_probe_cycles: u8,
    semantic_trace: Vec<SemanticTraceSample>,
}

#[derive(Clone, Serialize, JsonSchema)]
struct AsmDiagnostic {
    severity: String,
    code: String,
    message: String,
    canonical_line: usize,
    canonical_col: usize,
    canonical_snippet: String,
    source_file: String,
    source_line: usize,
}

#[derive(Clone, Serialize, JsonSchema)]
struct GameboyDiagnostics {
    diagnostics: Vec<AsmDiagnostic>,
}

pub fn import_asm(input: &Path, format: &str, out_kn: Option<&Path>, validate_only: bool) -> AsmResult<ImportAsmOutput> {
    let _ = tracing_subscriber::fmt::try_init();
    let normalized = format.trim().to_ascii_lowercase();
    if !SUPPORTED_FORMATS.iter().any(|v| *v == normalized) {
        return Err(AsmError::runtime(format!("Unsupported asm format '{}'. Supported: {}", format, SUPPORTED_FORMATS.join(", "))));
    }
    info!("kain-asm import start: format={}, input={}", normalized, input.display());
    let raw = load_asm_with_includes(input)?;
    let canonical = canonicalize_asm(&raw);
    let canonical_text = canonical.iter().map(|l| l.text.as_str()).collect::<Vec<_>>().join("\n");
    let expanded = expand_rgbds_semantics(&canonical);
    let expanded_text = expanded.iter().map(|l| l.text.as_str()).collect::<Vec<_>>().join("\n");
    let (parsed, provenance) = parse_asm_program(&expanded);
    let translit_units = build_translit_units(&parsed);
    let report = build_recovery_report(input, &expanded, &parsed);

    let cwd = std::env::current_dir().unwrap_or_else(|_| PathBuf::from("."));
    let root = if cwd.join("crates").is_dir() { cwd.clone() } else if cwd.join("Kain").join("crates").is_dir() { cwd.join("Kain") } else { cwd };
    let research_dir = root.join("Research").join("gameboy");
    let generated_dir = root.join("generated");
    fs::create_dir_all(&research_dir).map_err(AsmError::Io)?;
    fs::create_dir_all(&generated_dir).map_err(AsmError::Io)?;

    let canonical_asm_path = research_dir.join("gameboy_canonical.asm");
    let generated_kn_path = out_kn.map(Path::to_path_buf).unwrap_or_else(|| generated_dir.join("gameboy_firmware.kn"));
    let map_json_path = generated_kn_path.parent().unwrap_or(&generated_dir).join("gameboy_map.json");
    let report_json_path = research_dir.join("gameboy_recovery_report.json");
    let rom_model_path = map_json_path.with_file_name("gameboy_rom_model.json");
    let parity_harness_path = map_json_path.with_file_name("gameboy_parity_harness.json");
    let trace_path = map_json_path.with_file_name("gameboy_trace.json");
    let diagnostics_path = map_json_path.with_file_name("gameboy_diagnostics.json");
    let rom_model = build_rom_model(&expanded, &parsed);
    let parity_harness = build_parity_harness(&parsed);
    let trace_artifact = build_trace_artifact();
    let diagnostics = build_diagnostics(&expanded, &expanded_text, &report);
    debug!(
        "import stages done: canonical_lines={}, expanded_lines={}, blocks={}",
        canonical.len(),
        expanded.len(),
        parsed.blocks.len()
    );

    if !validate_only {
        fs::write(&canonical_asm_path, canonical_text).map_err(AsmError::Io)?;
        fs::write(&generated_kn_path, render_kain_firmware(&parsed, &translit_units)).map_err(AsmError::Io)?;
        let map = GameboyMap {
            translit_units: translit_units.clone(),
            parity_trace_schema: default_parity_trace_schema(),
            source_provenance: provenance,
            rom_model_path: rom_model_path.display().to_string(),
            parity_harness_path: parity_harness_path.display().to_string(),
            trace_path: trace_path.display().to_string(),
            diagnostics_path: diagnostics_path.display().to_string(),
        };
        let map_json = serde_json::to_string_pretty(&map).map_err(|e| AsmError::runtime(format!("Failed to serialize gameboy map: {}", e)))?;
        fs::write(&map_json_path, map_json).map_err(AsmError::Io)?;
        let rom_json = serde_json::to_string_pretty(&rom_model).map_err(|e| AsmError::runtime(format!("Failed to serialize rom model: {}", e)))?;
        fs::write(&rom_model_path, rom_json).map_err(AsmError::Io)?;
        let parity_json = serde_json::to_string_pretty(&parity_harness).map_err(|e| AsmError::runtime(format!("Failed to serialize parity harness: {}", e)))?;
        fs::write(&parity_harness_path, parity_json).map_err(AsmError::Io)?;
        let trace_json = serde_json::to_string_pretty(&trace_artifact).map_err(|e| AsmError::runtime(format!("Failed to serialize parity trace: {}", e)))?;
        fs::write(&trace_path, trace_json).map_err(AsmError::Io)?;
        let diagnostics_json = serde_json::to_string_pretty(&diagnostics).map_err(|e| AsmError::runtime(format!("Failed to serialize diagnostics: {}", e)))?;
        fs::write(&diagnostics_path, diagnostics_json).map_err(AsmError::Io)?;
    }
    let report_json = serde_json::to_string_pretty(&report).map_err(|e| AsmError::runtime(format!("Failed to serialize gameboy recovery report: {}", e)))?;
    fs::write(&report_json_path, report_json).map_err(AsmError::Io)?;

    Ok(ImportAsmOutput { canonical_asm_path, generated_kn_path, map_json_path, report_json_path, parsed, translit_units })
}

fn load_asm_with_includes(entry: &Path) -> AsmResult<Vec<SourceLine>> {
    fn walk(path: &Path, stack: &mut HashSet<PathBuf>, out: &mut Vec<SourceLine>) -> AsmResult<()> {
        let canonical = fs::canonicalize(path).or_else(|_| Ok::<PathBuf, std::io::Error>(path.to_path_buf())).map_err(AsmError::Io)?;
        if !stack.insert(canonical.clone()) {
            return Err(AsmError::runtime(format!("Detected recursive include loop at {}", canonical.display())));
        }
        let content = fs::read_to_string(&canonical).map_err(AsmError::Io)?;
        let dir = canonical.parent().map(Path::to_path_buf).unwrap_or_else(|| PathBuf::from("."));
        let file = canonical.display().to_string();
        for (idx, line) in content.lines().enumerate() {
            let ln = idx + 1;
            if let Some(inc) = parse_include_path(line) {
                let inc_path = dir.join(inc);
                if inc_path.exists() {
                    walk(&inc_path, stack, out)?;
                    continue;
                }
            }
            out.push(SourceLine { text: line.to_string(), file: file.clone(), line: ln });
        }
        stack.remove(&canonical);
        Ok(())
    }
    let mut out = Vec::<SourceLine>::new();
    let mut stack = HashSet::<PathBuf>::new();
    walk(entry, &mut stack, &mut out)?;
    Ok(out)
}

fn parse_include_path(line: &str) -> Option<String> {
    let code = strip_comment(line).trim();
    if !code.to_ascii_uppercase().starts_with("INCLUDE ") { return None; }
    let i0 = code.find('"')?;
    let rest = &code[i0 + 1..];
    let i1 = rest.find('"')?;
    Some(rest[..i1].to_string())
}

fn canonicalize_asm(raw: &[SourceLine]) -> Vec<CanonLine> {
    let mut out = Vec::<CanonLine>::new();
    for line in raw {
        let trimmed = line.text.replace('\u{feff}', "").trim().to_string();
        if trimmed.is_empty() { continue; }
        let squashed = trimmed.chars().map(|c| if c.is_ascii() && !c.is_control() { c } else { ' ' }).collect::<String>().split_whitespace().collect::<Vec<_>>().join(" ");
        if squashed.is_empty() { continue; }
        out.push(CanonLine { text: squashed, file: line.file.clone(), line: line.line, canon: out.len() + 1 });
    }
    out
}

fn expand_rgbds_semantics(lines: &[CanonLine]) -> Vec<CanonLine> {
    let mut out = Vec::<CanonLine>::new();
    let mut macros = IndexMap::<String, MacroDef>::new();
    let mut symbols = IndexMap::<String, i64>::new();
    let mut stack = Vec::<(bool, bool)>::new();
    let mut macro_invoke_counter = 0u64;
    let mut active = true;
    let mut i = 0usize;
    while i < lines.len() {
        let line = lines[i].clone();
        let text = strip_comment(&line.text).trim().to_string();
        if text.is_empty() { i += 1; continue; }
        if let Some((name, consumed)) = parse_macro_definition(lines, i) {
            if active { macros.insert(name.to_ascii_uppercase(), MacroDef { body: lines[(i + 1)..(i + consumed - 1)].to_vec() }); }
            i += consumed;
            continue;
        }
        let token = text.split_whitespace().next().unwrap_or("").to_ascii_uppercase();
        if token == "IF" {
            let cond = if active { eval_cond(text[2..].trim(), &symbols) } else { false };
            stack.push((active, cond));
            active = active && cond;
            i += 1;
            continue;
        }
        if token == "ELSE" {
            if let Some((parent, cond)) = stack.last().copied() { active = parent && !cond; }
            i += 1;
            continue;
        }
        if token == "ENDC" {
            if let Some((parent, _)) = stack.pop() { active = parent; }
            i += 1;
            continue;
        }
        if token == "REPT" {
            if let Some((repeat_count, consumed)) = parse_rept_block(lines, i, &symbols) {
                if active {
                    let body = &lines[(i + 1)..(i + consumed - 1)];
                    for _ in 0..repeat_count {
                        for entry in body {
                            out.extend(expand_macro_call(entry, &macros, 0, &mut macro_invoke_counter));
                        }
                    }
                }
                i += consumed;
                continue;
            }
        }
        if !active { i += 1; continue; }
        if let Some((name, value)) = parse_symbol_assignment(&text, &symbols) {
            symbols.insert(name.to_ascii_uppercase(), value);
            out.push(line);
            i += 1;
            continue;
        }
        out.extend(expand_macro_call(&line, &macros, 0, &mut macro_invoke_counter));
        i += 1;
    }
    out
}

fn parse_rept_block(lines: &[CanonLine], start: usize, symbols: &IndexMap<String, i64>) -> Option<(usize, usize)> {
    let head = strip_comment(&lines[start].text).trim();
    let mut parts = head.split_whitespace();
    if !parts.next()?.eq_ignore_ascii_case("REPT") { return None; }
    let count_expr = parts.collect::<Vec<_>>().join(" ");
    let repeat_count = eval_expr_i64(count_expr.trim(), symbols)?.max(0) as usize;
    let mut depth = 0i32;
    let mut idx = start + 1;
    while idx < lines.len() {
        let token = strip_comment(&lines[idx].text).trim().split_whitespace().next().unwrap_or("").to_ascii_uppercase();
        if token == "REPT" {
            depth += 1;
        } else if token == "ENDR" {
            if depth == 0 {
                return Some((repeat_count, idx - start + 1));
            }
            depth -= 1;
        }
        idx += 1;
    }
    None
}

fn parse_macro_definition(lines: &[CanonLine], start: usize) -> Option<(String, usize)> {
    let head = strip_comment(&lines[start].text).trim();
    let name = if let Some((left, right)) = head.split_once(':') {
        if right.trim().eq_ignore_ascii_case("MACRO") { left.trim().to_string() } else { String::new() }
    } else {
        let mut p = head.split_whitespace();
        if p.next()?.eq_ignore_ascii_case("MACRO") { p.next().unwrap_or("").to_string() } else { String::new() }
    };
    if name.is_empty() { return None; }
    let mut idx = start + 1;
    while idx < lines.len() {
        if strip_comment(&lines[idx].text).trim().eq_ignore_ascii_case("ENDM") { return Some((name, idx - start + 1)); }
        idx += 1;
    }
    None
}

fn parse_symbol_assignment(text: &str, symbols: &IndexMap<String, i64>) -> Option<(String, i64)> {
    if let Some((left, right)) = text.split_once(" EQU ").or_else(|| text.split_once(" equ ")) {
        return Some((left.trim().to_string(), eval_expr_i64(right.trim(), symbols)?));
    }
    let mut p = text.split_whitespace();
    let k = p.next()?;
    let op = p.next()?.to_ascii_uppercase();
    if op == "EQU" || op == "DEF" {
        return Some((k.to_string(), eval_expr_i64(p.collect::<Vec<_>>().join(" ").trim(), symbols)?));
    }
    None
}
fn eval_cond(expr: &str, symbols: &IndexMap<String, i64>) -> bool { eval_expr_i64(expr, symbols).unwrap_or(0) != 0 }

fn parse_num(v: &str) -> Option<i64> {
    let t = v.trim();
    if let Some(h) = t.strip_prefix('$') { return i64::from_str_radix(h, 16).ok(); }
    if let Some(h) = t.strip_prefix("0x").or_else(|| t.strip_prefix("0X")) { return i64::from_str_radix(h, 16).ok(); }
    if let Some(h) = t.strip_prefix('%') { return i64::from_str_radix(h, 2).ok(); }
    t.parse::<i64>().ok()
}

#[derive(Clone, Debug, PartialEq, Eq)]
enum ExprTok {
    Num(i64),
    Ident(String),
    LParen,
    RParen,
    Comma,
    OrOr,
    AndAnd,
    EqEq,
    NotEq,
    Lt,
    Lte,
    Gt,
    Gte,
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    Bang,
    End,
}

fn eval_expr_i64(expr: &str, symbols: &IndexMap<String, i64>) -> Option<i64> {
    let toks = tokenize_expr(expr)?;
    let mut p = ExprParser { toks, idx: 0, symbols };
    let value = p.parse_or()?;
    if p.peek() != &ExprTok::End { return None; }
    Some(value)
}

fn tokenize_expr(expr: &str) -> Option<Vec<ExprTok>> {
    let mut out = SmallVec::<[ExprTok; 64]>::new();
    let bytes = expr.as_bytes();
    let mut i = 0usize;
    while i < bytes.len() {
        let c = bytes[i] as char;
        if c.is_ascii_whitespace() { i += 1; continue; }
        if c == '$' {
            let start = i + 1;
            let mut j = start;
            while j < bytes.len() && (bytes[j] as char).is_ascii_hexdigit() { j += 1; }
            if j == start { return None; }
            out.push(ExprTok::Num(i64::from_str_radix(&expr[start..j], 16).ok()?));
            i = j;
            continue;
        }
        if c == '%' {
            let start = i + 1;
            let mut j = start;
            while j < bytes.len() && matches!(bytes[j] as char, '0' | '1') { j += 1; }
            if j > start {
                out.push(ExprTok::Num(i64::from_str_radix(&expr[start..j], 2).ok()?));
                i = j;
            } else {
                out.push(ExprTok::Percent);
                i += 1;
            }
            continue;
        }
        if c.is_ascii_digit() {
            let start = i;
            let mut j = i;
            while j < bytes.len() && (bytes[j] as char).is_ascii_alphanumeric() { j += 1; }
            out.push(ExprTok::Num(parse_num(&expr[start..j])?));
            i = j;
            continue;
        }
        if c.is_ascii_alphabetic() || c == '_' || c == '.' {
            let start = i;
            let mut j = i;
            while j < bytes.len() {
                let ch = bytes[j] as char;
                if ch.is_ascii_alphanumeric() || ch == '_' || ch == '.' { j += 1; } else { break; }
            }
            out.push(ExprTok::Ident(expr[start..j].to_ascii_uppercase()));
            i = j;
            continue;
        }

        let two = if i + 1 < bytes.len() { Some(&expr[i..i + 2]) } else { None };
        if let Some(op) = two {
            match op {
                "||" => { out.push(ExprTok::OrOr); i += 2; continue; }
                "&&" => { out.push(ExprTok::AndAnd); i += 2; continue; }
                "==" => { out.push(ExprTok::EqEq); i += 2; continue; }
                "!=" => { out.push(ExprTok::NotEq); i += 2; continue; }
                "<=" => { out.push(ExprTok::Lte); i += 2; continue; }
                ">=" => { out.push(ExprTok::Gte); i += 2; continue; }
                _ => {}
            }
        }
        match c {
            '(' => out.push(ExprTok::LParen),
            ')' => out.push(ExprTok::RParen),
            ',' => out.push(ExprTok::Comma),
            '<' => out.push(ExprTok::Lt),
            '>' => out.push(ExprTok::Gt),
            '+' => out.push(ExprTok::Plus),
            '-' => out.push(ExprTok::Minus),
            '*' => out.push(ExprTok::Star),
            '/' => out.push(ExprTok::Slash),
            '!' => out.push(ExprTok::Bang),
            _ => return None,
        }
        i += 1;
    }
    out.push(ExprTok::End);
    Some(out.into_vec())
}

struct ExprParser<'a> {
    toks: Vec<ExprTok>,
    idx: usize,
    symbols: &'a IndexMap<String, i64>,
}

impl<'a> ExprParser<'a> {
    fn peek(&self) -> &ExprTok { &self.toks[self.idx] }
    fn eat(&mut self) -> ExprTok { let t = self.toks[self.idx].clone(); self.idx += 1; t }
    fn expect(&mut self, tok: ExprTok) -> Option<()> { if self.peek() == &tok { self.eat(); Some(()) } else { None } }

    fn parse_or(&mut self) -> Option<i64> {
        let mut lhs = self.parse_and()?;
        while self.peek() == &ExprTok::OrOr {
            self.eat();
            let rhs = self.parse_and()?;
            lhs = if lhs != 0 || rhs != 0 { 1 } else { 0 };
        }
        Some(lhs)
    }
    fn parse_and(&mut self) -> Option<i64> {
        let mut lhs = self.parse_eq()?;
        while self.peek() == &ExprTok::AndAnd {
            self.eat();
            let rhs = self.parse_eq()?;
            lhs = if lhs != 0 && rhs != 0 { 1 } else { 0 };
        }
        Some(lhs)
    }
    fn parse_eq(&mut self) -> Option<i64> {
        let mut lhs = self.parse_rel()?;
        loop {
            match self.peek() {
                ExprTok::EqEq => { self.eat(); let rhs = self.parse_rel()?; lhs = if lhs == rhs { 1 } else { 0 }; }
                ExprTok::NotEq => { self.eat(); let rhs = self.parse_rel()?; lhs = if lhs != rhs { 1 } else { 0 }; }
                _ => break,
            }
        }
        Some(lhs)
    }
    fn parse_rel(&mut self) -> Option<i64> {
        let mut lhs = self.parse_add()?;
        loop {
            match self.peek() {
                ExprTok::Lt => { self.eat(); let rhs = self.parse_add()?; lhs = if lhs < rhs { 1 } else { 0 }; }
                ExprTok::Lte => { self.eat(); let rhs = self.parse_add()?; lhs = if lhs <= rhs { 1 } else { 0 }; }
                ExprTok::Gt => { self.eat(); let rhs = self.parse_add()?; lhs = if lhs > rhs { 1 } else { 0 }; }
                ExprTok::Gte => { self.eat(); let rhs = self.parse_add()?; lhs = if lhs >= rhs { 1 } else { 0 }; }
                _ => break,
            }
        }
        Some(lhs)
    }
    fn parse_add(&mut self) -> Option<i64> {
        let mut lhs = self.parse_mul()?;
        loop {
            match self.peek() {
                ExprTok::Plus => { self.eat(); lhs += self.parse_mul()?; }
                ExprTok::Minus => { self.eat(); lhs -= self.parse_mul()?; }
                _ => break,
            }
        }
        Some(lhs)
    }
    fn parse_mul(&mut self) -> Option<i64> {
        let mut lhs = self.parse_unary()?;
        loop {
            match self.peek() {
                ExprTok::Star => { self.eat(); lhs *= self.parse_unary()?; }
                ExprTok::Slash => {
                    self.eat();
                    let rhs = self.parse_unary()?;
                    if rhs == 0 { return None; }
                    lhs /= rhs;
                }
                ExprTok::Percent => {
                    self.eat();
                    let rhs = self.parse_unary()?;
                    if rhs == 0 { return None; }
                    lhs %= rhs;
                }
                _ => break,
            }
        }
        Some(lhs)
    }
    fn parse_unary(&mut self) -> Option<i64> {
        match self.peek() {
            ExprTok::Bang => { self.eat(); Some(if self.parse_unary()? == 0 { 1 } else { 0 }) }
            ExprTok::Minus => { self.eat(); Some(-self.parse_unary()?) }
            ExprTok::Plus => { self.eat(); self.parse_unary() }
            _ => self.parse_primary(),
        }
    }
    fn parse_primary(&mut self) -> Option<i64> {
        match self.eat() {
            ExprTok::Num(n) => Some(n),
            ExprTok::Ident(name) => {
                if name == "TRUE" { return Some(1); }
                if name == "FALSE" { return Some(0); }
                if name == "DEF" {
                    self.expect(ExprTok::LParen)?;
                    let symbol = match self.eat() { ExprTok::Ident(s) => s, _ => return None };
                    self.expect(ExprTok::RParen)?;
                    return Some(if self.symbols.contains_key(&symbol) { 1 } else { 0 });
                }
                Some(self.symbols.get(&name).copied().unwrap_or(0))
            }
            ExprTok::LParen => {
                let v = self.parse_or()?;
                self.expect(ExprTok::RParen)?;
                Some(v)
            }
            _ => None,
        }
    }
}

fn expand_macro_call(line: &CanonLine, macros: &IndexMap<String, MacroDef>, depth: usize, macro_invoke_counter: &mut u64) -> Vec<CanonLine> {
    if depth >= MAX_EXPAND_DEPTH { return vec![line.clone()]; }
    let text = strip_comment(&line.text).trim().to_string();
    let mut p = text.split_whitespace();
    let name = p.next().unwrap_or("");
    if name.ends_with(':') { return vec![line.clone()]; }
    let Some(def) = macros.get(&name.to_ascii_uppercase()) else { return vec![line.clone()]; };
    *macro_invoke_counter += 1;
    let macro_id = *macro_invoke_counter;
    let local_prefix = format!("__m{}_{}", macro_id, name.to_ascii_lowercase());
    let args = text[name.len()..].trim().split(',').map(str::trim).filter(|v| !v.is_empty()).map(str::to_string).collect::<Vec<_>>();
    let mut out = Vec::<CanonLine>::new();
    for l in &def.body {
        let mut t = l.text.clone();
        for i in 0..args.len() { t = t.replace(&format!("\\{}", i + 1), &args[i]); }
        t = t.replace("\\@", &macro_id.to_string());
        t = rewrite_local_macro_labels(&t, &local_prefix);
        let nested = expand_macro_call(&CanonLine { text: t, file: l.file.clone(), line: l.line, canon: l.canon }, macros, depth + 1, macro_invoke_counter);
        out.extend(nested);
    }
    out
}

fn rewrite_local_macro_labels(text: &str, prefix: &str) -> String {
    let mut out = String::new();
    let chars = text.chars().collect::<Vec<_>>();
    let mut i = 0usize;
    while i < chars.len() {
        let c = chars[i];
        if c == '.' {
            let prev = if i == 0 { ' ' } else { chars[i - 1] };
            if !(prev.is_ascii_alphanumeric() || prev == '_' || prev == '.') {
                let mut j = i + 1;
                while j < chars.len() && (chars[j].is_ascii_alphanumeric() || chars[j] == '_') { j += 1; }
                if j > i + 1 {
                    out.push_str(prefix);
                    out.push('_');
                    out.extend(chars[(i + 1)..j].iter());
                    i = j;
                    continue;
                }
            }
        }
        out.push(c);
        i += 1;
    }
    out
}

fn parse_asm_program(lines: &[CanonLine]) -> (AsmProgram, Vec<SourceProvenance>) {
    let mut blocks = Vec::<AsmBlock>::new();
    let mut directives = Vec::<AsmDirective>::new();
    let mut data_tables = Vec::<AsmDataTable>::new();
    let mut provenance = Vec::<SourceProvenance>::new();
    let mut cur_label = String::new();
    let mut cur_instrs = Vec::<AsmInstr>::new();
    let mut cur_start = 1usize;
    let mut cur_file = String::new();
    let mut cur_src_start = 1usize;
    let mut cur_src_end = 1usize;

    let flush = |end_line: usize, blocks: &mut Vec<AsmBlock>, provenance: &mut Vec<SourceProvenance>, cur_label: &mut String, cur_instrs: &mut Vec<AsmInstr>, cur_start: usize, cur_file: &str, cur_src_start: usize, cur_src_end: usize| {
        if !cur_label.is_empty() && !cur_instrs.is_empty() {
            let label = cur_label.clone();
            blocks.push(AsmBlock { label: label.clone(), instructions: std::mem::take(cur_instrs), source_line_start: cur_start, source_line_end: end_line });
            provenance.push(SourceProvenance { kind: "block".to_string(), symbol: label, source_file: cur_file.to_string(), source_line_start: cur_src_start, source_line_end: cur_src_end, canonical_line_start: cur_start, canonical_line_end: end_line });
        }
        cur_label.clear();
    };

    for l in lines {
        let text = strip_comment(&l.text).trim();
        if text.is_empty() { continue; }
        if is_label_line(text) {
            flush(l.canon.saturating_sub(1), &mut blocks, &mut provenance, &mut cur_label, &mut cur_instrs, cur_start, &cur_file, cur_src_start, cur_src_end);
            cur_label = normalize_label(text);
            cur_start = l.canon;
            cur_file = l.file.clone();
            cur_src_start = l.line;
            cur_src_end = l.line;
            continue;
        }
        if is_directive_line(text) {
            let sym = text.split_whitespace().next().unwrap_or(text).to_string();
            directives.push(AsmDirective { name: text.to_string(), args: Vec::new(), source_line: l.canon });
            provenance.push(SourceProvenance { kind: "directive".to_string(), symbol: sym, source_file: l.file.clone(), source_line_start: l.line, source_line_end: l.line, canonical_line_start: l.canon, canonical_line_end: l.canon });
            continue;
        }
        if let Some((label, bytes)) = parse_data_line(text) {
            data_tables.push(AsmDataTable { label: label.clone(), bytes, source_line_start: l.canon, source_line_end: l.canon });
            provenance.push(SourceProvenance { kind: "data_table".to_string(), symbol: label, source_file: l.file.clone(), source_line_start: l.line, source_line_end: l.line, canonical_line_start: l.canon, canonical_line_end: l.canon });
            continue;
        }
        if cur_label.is_empty() {
            cur_label = format!("bank_entry_{}", l.canon);
            cur_start = l.canon;
            cur_file = l.file.clone();
            cur_src_start = l.line;
            cur_src_end = l.line;
        }
        if let Some(instr) = parse_instruction(text, l.canon) {
            cur_src_end = l.line;
            cur_instrs.push(instr);
        }
    }
    flush(lines.len(), &mut blocks, &mut provenance, &mut cur_label, &mut cur_instrs, cur_start, &cur_file, cur_src_start, cur_src_end);
    (AsmProgram { blocks, directives, data_tables }, provenance)
}

fn strip_comment(line: &str) -> &str { line.split_once(';').map(|(l, _)| l).unwrap_or(line) }
fn normalize_label(label: &str) -> String { label.trim().trim_end_matches("::").trim_end_matches(':').to_string() }
fn is_label_line(line: &str) -> bool {
    let t = line.trim();
    if t.ends_with("::") {
        let n = t.trim_end_matches("::");
        return !n.is_empty() && n.chars().all(|c| c.is_ascii_alphanumeric() || c == '_' || c == '.');
    }
    if t.ends_with(':') {
        let n = t.trim_end_matches(':');
        return !n.is_empty() && n.chars().all(|c| c.is_ascii_alphanumeric() || c == '_' || c == '.');
    }
    false
}

fn is_directive_line(line: &str) -> bool {
    let code = strip_comment(line).trim();
    if code.is_empty() { return false; }
    let upper = code.to_ascii_uppercase();
    if upper.contains(" EQU ") { return true; }
    let t = upper.split_whitespace().next().unwrap_or("");
    matches!(t, "SECTION" | "INCBIN" | "INCLUDE" | "ORG" | "MACRO" | "ENDM" | "REPT" | "ENDR" | "DEF" | "PURGE" | "UNION" | "NEXTU" | "ENDU" | "RSRESET" | "RSSET" | "FAIL" | "WARN" | "PRINTT" | "PRINTV" | "ASSERT")
}

fn parse_data_line(line: &str) -> Option<(String, Vec<String>)> {
    let upper = line.to_ascii_uppercase();
    let marker = if upper.contains(" DB ") || upper.starts_with("DB ") { "DB" } else if upper.contains(" DW ") || upper.starts_with("DW ") { "DW" } else { return None; };
    let pos = upper.find(&format!(" {} ", marker)).map(|p| p + 1).unwrap_or(0);
    let left = line[..pos].trim();
    let right = line[pos + marker.len()..].trim();
    let label = if left.is_empty() { "__anonymous_table".to_string() } else { normalize_label(left) };
    let values = right.split(|c: char| c == ',' || c.is_ascii_whitespace()).map(str::trim).filter(|v| !v.is_empty()).map(str::to_string).collect::<Vec<_>>();
    if values.is_empty() { None } else { Some((label, values)) }
}

fn parse_instruction(line: &str, source_line: usize) -> Option<AsmInstr> {
    let mut parts = line.split_whitespace();
    let opcode = parts.next()?.to_ascii_uppercase();
    if !is_opcode_keyword(&opcode) { return None; }
    let operand = parts.collect::<Vec<_>>().join(" ");
    Some(AsmInstr { opcode, operand: if operand.is_empty() { None } else { Some(operand) }, source_line })
}

fn build_translit_units(program: &AsmProgram) -> Vec<TranslitUnit> {
    program
        .blocks
        .par_iter()
        .map(|b| TranslitUnit {
            source_label: b.label.clone(),
            target_item: format!("gb_{}", normalize_identifier(&b.label)),
            source_line_start: b.source_line_start,
            source_line_end: b.source_line_end,
        })
        .collect()
}

fn normalize_identifier(label: &str) -> String {
    let mut out = String::new();
    for c in label.chars() {
        if c.is_ascii_alphanumeric() { out.push(c.to_ascii_lowercase()); } else if c == '_' || c == '.' { out.push('_'); }
    }
    if out.is_empty() { "bank_label".to_string() } else { out }
}

fn render_kain_firmware(program: &AsmProgram, units: &[TranslitUnit]) -> String {
    let mut out = String::new();
    out.push_str("# Generated by kain import-asm --format lr35902-gameboy\n");
    out.push_str("# Game Boy LR35902 transliteration seed\n");
    out.push_str("# Includes UE5-facing runtime shim entrypoints for fixed-step simulation\n\n");
    out.push_str("struct CpuState:\n    a: Int\n    b: Int\n    c: Int\n    d: Int\n    e: Int\n    h: Int\n    l: Int\n    f: Int\n    sp: Int\n    pc: Int\n    ime: Int\n    halted: Int\n    cycles: Int\n\n");
    out.push_str("struct Memory:\n    wram: Array<Int>\n    hram: Array<Int>\n    vram: Array<Int>\n    io_ports: Array<Int>\n    rom_banks: Array<Array<Int>>\n\n");
    out.push_str("struct Ue5ShimState:\n    cpu: CpuState\n    mem: Memory\n    tick: Int\n    last_effect: Int\n\n");
    out.push_str("fn read_port(port_id: Int) -> Int:\n");
    out.push_str("    let _port = port_id\n");
    out.push_str("    return 0\n\n");
    out.push_str("fn write_port(port_id: Int, value: Int):\n");
    out.push_str("    let _port = port_id\n");
    out.push_str("    let _value = value\n\n");
    out.push_str("fn read_rom0(mem: Memory, addr: Int) -> Int:\n");
    out.push_str("    if addr < 0:\n");
    out.push_str("        return 0\n");
    out.push_str("    if addr >= 16384:\n");
    out.push_str("        return 0\n");
    out.push_str("    return mem.rom_banks[0][addr]\n\n");
    out.push_str("fn u8(v: Int) -> Int:\n");
    out.push_str("    return v & 255\n\n");
    out.push_str("fn u16(v: Int) -> Int:\n");
    out.push_str("    return v & 65535\n\n");
    out.push_str("fn with_cycles(cpu: CpuState, cycles: Int) -> CpuState:\n");
    out.push_str("    return CpuState { a: cpu.a, b: cpu.b, c: cpu.c, d: cpu.d, e: cpu.e, h: cpu.h, l: cpu.l, f: cpu.f, sp: cpu.sp, pc: cpu.pc, ime: cpu.ime, halted: cpu.halted, cycles: cpu.cycles + cycles }\n\n");
    out.push_str("fn step(cpu: CpuState, mem: Memory) -> (CpuState, Memory, Int):\n");
    out.push_str("    if cpu.halted != 0:\n");
    out.push_str("        let hcpu = with_cycles(cpu, 4)\n");
    out.push_str("        return (hcpu, mem, 4)\n");
    out.push_str("    let opcode = read_rom0(mem, u16(cpu.pc))\n");
    out.push_str("    if opcode == 0:\n");
    out.push_str("        # NOP\n");
    out.push_str("        let ncpu = with_cycles(CpuState { a: cpu.a, b: cpu.b, c: cpu.c, d: cpu.d, e: cpu.e, h: cpu.h, l: cpu.l, f: cpu.f, sp: cpu.sp, pc: u16(cpu.pc + 1), ime: cpu.ime, halted: cpu.halted, cycles: cpu.cycles }, 4)\n");
    out.push_str("        return (ncpu, mem, 4)\n");
    out.push_str("    if opcode == 62:\n");
    out.push_str("        # LD A,d8\n");
    out.push_str("        let imm = read_rom0(mem, u16(cpu.pc + 1))\n");
    out.push_str("        let lcpu = with_cycles(CpuState { a: u8(imm), b: cpu.b, c: cpu.c, d: cpu.d, e: cpu.e, h: cpu.h, l: cpu.l, f: cpu.f, sp: cpu.sp, pc: u16(cpu.pc + 2), ime: cpu.ime, halted: cpu.halted, cycles: cpu.cycles }, 8)\n");
    out.push_str("        return (lcpu, mem, 8)\n");
    out.push_str("    if opcode == 175:\n");
    out.push_str("        # XOR A\n");
    out.push_str("        let xcpu = with_cycles(CpuState { a: 0, b: cpu.b, c: cpu.c, d: cpu.d, e: cpu.e, h: cpu.h, l: cpu.l, f: 128, sp: cpu.sp, pc: u16(cpu.pc + 1), ime: cpu.ime, halted: cpu.halted, cycles: cpu.cycles }, 4)\n");
    out.push_str("        return (xcpu, mem, 4)\n");
    out.push_str("    if opcode == 195:\n");
    out.push_str("        # JP a16\n");
    out.push_str("        let lo = read_rom0(mem, u16(cpu.pc + 1))\n");
    out.push_str("        let hi = read_rom0(mem, u16(cpu.pc + 2))\n");
    out.push_str("        let addr = u16((hi << 8) | lo)\n");
    out.push_str("        let jcpu = with_cycles(CpuState { a: cpu.a, b: cpu.b, c: cpu.c, d: cpu.d, e: cpu.e, h: cpu.h, l: cpu.l, f: cpu.f, sp: cpu.sp, pc: addr, ime: cpu.ime, halted: cpu.halted, cycles: cpu.cycles }, 16)\n");
    out.push_str("        return (jcpu, mem, 16)\n");
    out.push_str("    if opcode == 24:\n");
    out.push_str("        # JR r8 (signed)\n");
    out.push_str("        let raw_delta = read_rom0(mem, u16(cpu.pc + 1))\n");
    out.push_str("        let delta = if raw_delta >= 128:\n");
    out.push_str("            raw_delta - 256\n");
    out.push_str("        else:\n");
    out.push_str("            raw_delta\n");
    out.push_str("        let pc2 = u16(cpu.pc + 2 + delta)\n");
    out.push_str("        let jrcpu = with_cycles(CpuState { a: cpu.a, b: cpu.b, c: cpu.c, d: cpu.d, e: cpu.e, h: cpu.h, l: cpu.l, f: cpu.f, sp: cpu.sp, pc: pc2, ime: cpu.ime, halted: cpu.halted, cycles: cpu.cycles }, 12)\n");
    out.push_str("        return (jrcpu, mem, 12)\n");
    out.push_str("    if opcode == 118:\n");
    out.push_str("        # HALT\n");
    out.push_str("        let hlt = with_cycles(CpuState { a: cpu.a, b: cpu.b, c: cpu.c, d: cpu.d, e: cpu.e, h: cpu.h, l: cpu.l, f: cpu.f, sp: cpu.sp, pc: u16(cpu.pc + 1), ime: cpu.ime, halted: 1, cycles: cpu.cycles }, 4)\n");
    out.push_str("        return (hlt, mem, 4)\n");
    out.push_str("    # Fallback unknown opcode: timing-safe NOP\n");
    out.push_str("    let fcpu = with_cycles(CpuState { a: cpu.a, b: cpu.b, c: cpu.c, d: cpu.d, e: cpu.e, h: cpu.h, l: cpu.l, f: cpu.f, sp: cpu.sp, pc: u16(cpu.pc + 1), ime: cpu.ime, halted: cpu.halted, cycles: cpu.cycles }, 4)\n");
    out.push_str("    return (fcpu, mem, 4)\n\n");
    out.push_str("fn parity_frame(state: Ue5ShimState) -> Int:\n");
    out.push_str("    let _state = state\n");
    out.push_str("    return 0\n\n");
    out.push_str("fn ue5_init(cpu: CpuState, mem: Memory) -> Ue5ShimState:\n    return Ue5ShimState { cpu: cpu, mem: mem, tick: 0, last_effect: 0 }\n\n");
    out.push_str("fn ue5_reset(state: Ue5ShimState, cpu: CpuState, mem: Memory) -> Ue5ShimState:\n    let _old = state\n    return Ue5ShimState { cpu: cpu, mem: mem, tick: 0, last_effect: 0 }\n\n");
    out.push_str("fn ue5_tick_step(state: Ue5ShimState, step_count: Int) -> Ue5ShimState:\n");
    out.push_str("    if step_count <= 0:\n");
    out.push_str("        return state\n");
    out.push_str("    let (cpu1, mem1, eff1) = step(state.cpu, state.mem)\n");
    out.push_str("    return Ue5ShimState { cpu: cpu1, mem: mem1, tick: state.tick + 1, last_effect: eff1 }\n\n");
    out.push_str("fn ue5_apply_sensor_input(state: Ue5ShimState, port_id: Int, value: Int) -> Ue5ShimState:\n    write_port(port_id, value)\n    return state\n\n");
    out.push_str("fn ue5_read_actuator_output(state: Ue5ShimState, port_id: Int) -> Int:\n    let _state = state\n    return read_port(port_id)\n\n");
    out.push_str("const GAMEBOY_TABLES: Array<Array<Int>> = [\n");
    for table in &program.data_tables { out.push_str(&format!("    [{}],\n", table.bytes.join(", "))); }
    out.push_str("]\n\n");
    for unit in units {
        out.push_str(&format!("fn {}(cpu: CpuState, mem: Memory) -> (CpuState, Memory):\n    let next_cpu = cpu\n    let next_mem = mem\n", unit.target_item));
        if let Some(block) = program.blocks.iter().find(|b| b.label == unit.source_label) {
            for instr in &block.instructions {
                let op = instr.operand.as_deref().unwrap_or("");
                out.push_str(&format!("    # [{}:{}] {} {}\n", unit.source_label, instr.source_line, instr.opcode, op));
            }
        }
        out.push_str("    return (next_cpu, next_mem)\n\n");
    }
    out
}

fn build_rom_model(lines: &[CanonLine], program: &AsmProgram) -> GameboyRomModel {
    let mut current_section = "ROM0".to_string();
    let mut current_bank = 0u16;
    let mut section_start = 0u16;
    let mut pc = 0u16;
    let mut section_bytes = 0usize;
    let mut sections = Vec::<RomSection>::new();
    let mut bank_usage = BTreeMap::<u16, usize>::new();

    let flush_section = |name: &str, bank: u16, start: u16, pc_now: u16, bytes: usize, sections: &mut Vec<RomSection>| {
        if bytes == 0 { return; }
        sections.push(RomSection {
            name: name.to_string(),
            bank,
            start,
            end: pc_now.saturating_sub(1),
            bytes,
        });
    };

    for line in lines {
        let text = strip_comment(&line.text).trim();
        if text.is_empty() { continue; }

        if text.to_ascii_uppercase().starts_with("SECTION ") {
            flush_section(&current_section, current_bank, section_start, pc, section_bytes, &mut sections);
            let (name, bank, base) = parse_section_header(text).unwrap_or(("ROM0".to_string(), 0, pc));
            current_section = name;
            current_bank = bank;
            section_start = base;
            pc = base;
            section_bytes = 0;
            continue;
        }

        if parse_data_line(text).is_some() {
            if let Some((_, vals)) = parse_data_line(text) {
                section_bytes += vals.len();
                pc = pc.saturating_add(vals.len() as u16);
            }
            continue;
        }
        if let Some(instr) = parse_instruction(text, line.canon) {
            let size = estimate_instruction_size(&instr);
            section_bytes += size as usize;
            pc = pc.saturating_add(size);
        }
    }
    flush_section(&current_section, current_bank, section_start, pc, section_bytes, &mut sections);
    for sec in &sections {
        *bank_usage.entry(sec.bank).or_insert(0usize) += sec.bytes;
    }
    for table in &program.data_tables {
        let _ = table;
    }
    let banks = bank_usage.into_iter().map(|(bank, used_bytes)| RomBank { bank, used_bytes }).collect::<Vec<_>>();
    GameboyRomModel { sections, banks }
}

fn build_parity_harness(program: &AsmProgram) -> GameboyParityHarness {
    let checkpoints = program
        .blocks
        .iter()
        .take(512)
        .enumerate()
        .map(|(idx, block)| ParityCheckpoint {
            tick: idx as u64,
            label: block.label.clone(),
            source_line: block.source_line_start,
        })
        .collect::<Vec<_>>();
    let mut graph = DiGraphMap::<usize, ()>::new();
    for idx in 0..program.blocks.len() {
        graph.add_node(idx);
    }
    for (idx, block) in program.blocks.iter().enumerate() {
        for instr in &block.instructions {
            let op = instr.opcode.as_str();
            if op != "JP" && op != "JR" && op != "CALL" {
                continue;
            }
            let Some(operand) = instr.operand.as_deref() else {
                continue;
            };
            let target = operand
                .split(',')
                .next_back()
                .unwrap_or("")
                .trim()
                .trim_start_matches('.');
            if target.is_empty() {
                continue;
            }
            if let Some(to_idx) = program
                .blocks
                .iter()
                .position(|b| b.label.eq_ignore_ascii_case(target) || b.label.ends_with(&format!(".{}", target)))
            {
                graph.add_edge(idx, to_idx, ());
            }
        }
    }
    let probe = step_lr35902(Lr35902State::default(), Lr35902Memory::default());
    let semantic_trace = build_semantic_trace_sample();

    GameboyParityHarness {
        checkpoints,
        graph_nodes: graph.node_count(),
        graph_edges: graph.edge_count(),
        semantic_probe_opcode: probe.opcode,
        semantic_probe_cycles: probe.cycles,
        semantic_trace,
    }
}

fn build_semantic_trace_sample() -> Vec<SemanticTraceSample> {
    let mut mem = Lr35902Memory::default();
    // Probe program: LD A,$12 ; ADD A,$34 ; PUSH BC ; POP BC ; JR +2 ; NOP ; HALT
    mem.rom0[0x0100] = 0x3e;
    mem.rom0[0x0101] = 0x12;
    mem.rom0[0x0102] = 0xc6;
    mem.rom0[0x0103] = 0x34;
    mem.rom0[0x0104] = 0xc5;
    mem.rom0[0x0105] = 0xc1;
    mem.rom0[0x0106] = 0x18;
    mem.rom0[0x0107] = 0x02;
    mem.rom0[0x0108] = 0x00;
    mem.rom0[0x0109] = 0x76;

    let mut out = Vec::<SemanticTraceSample>::new();
    let mut state = Lr35902State::default();
    let mut memory = mem;
    for step in 0..8u64 {
        let before_pc = state.pc;
        let result = step_lr35902(state, memory);
        out.push(SemanticTraceSample {
            step,
            pc: before_pc,
            opcode: result.opcode,
            cycles: result.cycles,
        });
        state = result.state;
        memory = result.memory;
        if state.halted {
            break;
        }
    }
    out
}

fn build_trace_artifact() -> GameboyTraceArtifact {
    let mut mem = Lr35902Memory::default();
    // Probe program: LD A,$12 ; ADD A,$34 ; CB 7C ; PUSH BC ; POP BC ; JR +2 ; NOP ; HALT
    mem.rom0[0x0100] = 0x3e;
    mem.rom0[0x0101] = 0x12;
    mem.rom0[0x0102] = 0xc6;
    mem.rom0[0x0103] = 0x34;
    mem.rom0[0x0104] = 0xcb;
    mem.rom0[0x0105] = 0x7c; // BIT 7,H
    mem.rom0[0x0106] = 0xc5;
    mem.rom0[0x0107] = 0xc1;
    mem.rom0[0x0108] = 0x18;
    mem.rom0[0x0109] = 0x02;
    mem.rom0[0x010a] = 0x00;
    mem.rom0[0x010b] = 0x76;

    let mut state = Lr35902State::default();
    let mut memory = mem;
    let mut frames = Vec::<TraceFrame>::new();

    for step in 0..16u64 {
        let result = step_lr35902(state, memory);
        let effect = match &result.effect {
            StepEffect::None => "none".to_string(),
            StepEffect::Halt => "halt".to_string(),
            StepEffect::PortWrite { port, value } => format!("port_write:{:#04x}={:#04x}", port, value),
            StepEffect::Interrupt { vector } => format!("interrupt:{:#06x}", vector),
        };
        frames.push(TraceFrame {
            step,
            pc: result.state.pc,
            opcode: result.opcode,
            cycles: result.cycles,
            a: result.state.a,
            b: result.state.b,
            c: result.state.c,
            d: result.state.d,
            e: result.state.e,
            h: result.state.h,
            l: result.state.l,
            f: result.state.f,
            sp: result.state.sp,
            ime: result.state.ime,
            halted: result.state.halted,
            effect,
        });
        state = result.state;
        memory = result.memory;
        if state.halted {
            break;
        }
    }
    GameboyTraceArtifact { frames }
}

fn parse_section_header(text: &str) -> Option<(String, u16, u16)> {
    let rest = text.trim().strip_prefix("SECTION ")?.trim();
    let mut name = "ROM0".to_string();
    if let Some(i0) = rest.find('"') {
        let rest2 = &rest[i0 + 1..];
        if let Some(i1) = rest2.find('"') { name = rest2[..i1].to_string(); }
    }
    let up = rest.to_ascii_uppercase();
    let bank = if let Some(idx) = up.find("BANK[") {
        let chunk = &rest[(idx + 5)..];
        if let Some(end) = chunk.find(']') { parse_num(chunk[..end].trim()).unwrap_or(0).max(0) as u16 } else { 0 }
    } else { 0 };
    let base = if let Some(idx) = up.find("ROM0[$") {
        let chunk = &rest[(idx + 6)..];
        if let Some(end) = chunk.find(']') { parse_num(&format!("${}", &chunk[..end])).unwrap_or(0).max(0) as u16 } else { 0 }
    } else if let Some(idx) = up.find("ROMX[$") {
        let chunk = &rest[(idx + 6)..];
        if let Some(end) = chunk.find(']') { parse_num(&format!("${}", &chunk[..end])).unwrap_or(0).max(0) as u16 } else { 0x4000 }
    } else { 0 };
    Some((name, bank, base))
}

fn estimate_instruction_size(instr: &AsmInstr) -> u16 {
    let op = instr.opcode.as_str();
    let operand = instr.operand.as_deref().unwrap_or("");
    if op == "RST" || op == "RET" || op == "RETI" || op == "NOP" || op == "HALT" || op == "DAA" || op == "SCF" || op == "CCF" || op == "CPL" || op == "DI" || op == "EI" { return 1; }
    if op == "JP" || op == "CALL" { return 3; }
    if op == "JR" { return 2; }
    if op == "LD" {
        if operand.contains("[$") || operand.contains("($") { return 3; }
        if operand.contains('$') || operand.contains('%') || operand.chars().any(|c| c.is_ascii_digit()) { return 2; }
        return 1;
    }
    if operand.contains('$') || operand.contains('%') { 2 } else { 1 }
}

fn build_recovery_report(input: &Path, canonical: &[CanonLine], parsed: &AsmProgram) -> RecoveryReport {
    let mut unresolved_tokens = Vec::<RecoveryIssue>::new();
    let mut ambiguous_labels = Vec::<RecoveryIssue>::new();
    let mut seen = HashSet::<String>::new();
    for line in canonical {
        let t = line.text.trim();
        if t.is_empty() { continue; }
        let ok = is_label_line(t) || is_directive_line(t) || parse_data_line(t).is_some() || parse_instruction(t, line.canon).is_some();
        if !ok { unresolved_tokens.push(RecoveryIssue { line: line.canon, message: format!("Unrecognized canonical line: {}", t) }); }
        if is_label_line(t) {
            let label = normalize_label(t);
            if !seen.insert(label.clone()) { ambiguous_labels.push(RecoveryIssue { line: line.canon, message: format!("Duplicate label '{}'", label) }); }
        }
    }
    let total = canonical.len().max(1);
    let rec = canonical.len().saturating_sub(unresolved_tokens.len());
    let _ = parsed;
    RecoveryReport {
        input: input.display().to_string(),
        canonical_output: "Research/gameboy/gameboy_canonical.asm".to_string(),
        unresolved_tokens,
        ambiguous_labels,
        section_scores: vec![RecoverySectionScore { section: "global".to_string(), recognized: rec, total, confidence: (rec as f64) / (total as f64) }],
    }
}

fn build_diagnostics(lines: &[CanonLine], expanded_text: &str, report: &RecoveryReport) -> GameboyDiagnostics {
    let mapper = SpanMapper::new(expanded_text);
    let line_starts = compute_line_starts(expanded_text);
    let mut diagnostics = Vec::<AsmDiagnostic>::new();

    for issue in &report.unresolved_tokens {
        if let Some(diag) = issue_to_diag("error", "ASM_UNRESOLVED", issue, lines, &mapper, &line_starts) {
            diagnostics.push(diag);
        }
    }
    for issue in &report.ambiguous_labels {
        if let Some(diag) = issue_to_diag("warning", "ASM_AMBIGUOUS", issue, lines, &mapper, &line_starts) {
            diagnostics.push(diag);
        }
    }
    GameboyDiagnostics { diagnostics }
}

fn issue_to_diag(
    severity: &str,
    code: &str,
    issue: &RecoveryIssue,
    lines: &[CanonLine],
    mapper: &SpanMapper,
    line_starts: &[usize],
) -> Option<AsmDiagnostic> {
    if issue.line == 0 {
        return None;
    }
    let line_idx = lines
        .iter()
        .position(|line| line.canon == issue.line)
        .or_else(|| issue.line.checked_sub(1).filter(|idx| *idx < lines.len()))?;
    let canon = &lines[line_idx];
    let col0 = canon
        .text
        .char_indices()
        .find_map(|(idx, ch)| if ch.is_ascii_whitespace() { None } else { Some(idx) })
        .unwrap_or(0);
    let span_start = line_starts.get(line_idx).copied().unwrap_or(0).saturating_add(col0);
    let span = Span::new(span_start, span_start.saturating_add(1));
    let loc = mapper.span_to_location(span, "<expanded>");

    Some(AsmDiagnostic {
        severity: severity.to_string(),
        code: code.to_string(),
        message: issue.message.clone(),
        canonical_line: canon.canon,
        canonical_col: loc.col,
        canonical_snippet: canon.text.clone(),
        source_file: canon.file.clone(),
        source_line: canon.line,
    })
}

fn compute_line_starts(text: &str) -> Vec<usize> {
    let mut starts = vec![0usize];
    for (idx, ch) in text.char_indices() {
        if ch == '\n' {
            starts.push(idx + 1);
        }
    }
    starts
}

fn default_parity_trace_schema() -> ParityTraceFrame {
    let mut registers = BTreeMap::new();
    for reg in ["a", "b", "c", "d", "e", "h", "l", "f", "sp", "pc"] { registers.insert(reg.to_string(), 0); }
    let mut flags = BTreeMap::new();
    for fl in ["z", "n", "h", "c"] { flags.insert(fl.to_string(), false); }
    ParityTraceFrame { tick: 0, pc: 0, opcode: "NOP".to_string(), registers, flags, notes: vec!["lr35902-schema".to_string()] }
}

fn is_opcode_keyword(kw: &str) -> bool {
    matches!(kw, "ADC" | "ADD" | "AND" | "BIT" | "CALL" | "CCF" | "CP" | "CPL" | "DAA" | "DEC" | "DI" | "EI" | "HALT" | "INC" | "JP" | "JR" | "LD" | "LDD" | "LDH" | "LDI" | "NOP" | "OR" | "POP" | "PUSH" | "RES" | "RET" | "RETI" | "RL" | "RLA" | "RLC" | "RLCA" | "RR" | "RRA" | "RRC" | "RRCA" | "RST" | "SBC" | "SCF" | "SET" | "SLA" | "SRA" | "SRL" | "STOP" | "SUB" | "SWAP" | "XOR")
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::time::{SystemTime, UNIX_EPOCH};

    fn set_tile_row(mem: &mut Lr35902Memory, tile: usize, row: usize, low: u8, high: u8) {
        let base = tile * 16 + row * 2;
        mem.vram[base] = low;
        mem.vram[base + 1] = high;
    }

    #[test]
    fn macro_if_expansion() {
        let src = vec![
            CanonLine { text: "FLAG EQU 1".to_string(), file: "a.asm".to_string(), line: 1, canon: 1 },
            CanonLine { text: "LoadA: MACRO".to_string(), file: "a.asm".to_string(), line: 2, canon: 2 },
            CanonLine { text: "LD A, \\1".to_string(), file: "a.asm".to_string(), line: 3, canon: 3 },
            CanonLine { text: "ENDM".to_string(), file: "a.asm".to_string(), line: 4, canon: 4 },
            CanonLine { text: "IF FLAG".to_string(), file: "a.asm".to_string(), line: 5, canon: 5 },
            CanonLine { text: "LoadA $42".to_string(), file: "a.asm".to_string(), line: 6, canon: 6 },
            CanonLine { text: "ELSE".to_string(), file: "a.asm".to_string(), line: 7, canon: 7 },
            CanonLine { text: "NOP".to_string(), file: "a.asm".to_string(), line: 8, canon: 8 },
            CanonLine { text: "ENDC".to_string(), file: "a.asm".to_string(), line: 9, canon: 9 },
        ];
        let out = expand_rgbds_semantics(&src);
        assert!(out.iter().any(|l| l.text == "LD A, $42"));
        assert!(!out.iter().any(|l| l.text == "NOP"));
    }

    #[test]
    fn expression_engine_supports_logic_comparison_and_def() {
        let src = vec![
            CanonLine { text: "A EQU 2".to_string(), file: "a.asm".to_string(), line: 1, canon: 1 },
            CanonLine { text: "B EQU 3".to_string(), file: "a.asm".to_string(), line: 2, canon: 2 },
            CanonLine { text: "IF (A + B == 5) && DEF(A) || DEF(MISSING)".to_string(), file: "a.asm".to_string(), line: 3, canon: 3 },
            CanonLine { text: "LD A, $11".to_string(), file: "a.asm".to_string(), line: 4, canon: 4 },
            CanonLine { text: "ELSE".to_string(), file: "a.asm".to_string(), line: 5, canon: 5 },
            CanonLine { text: "LD A, $22".to_string(), file: "a.asm".to_string(), line: 6, canon: 6 },
            CanonLine { text: "ENDC".to_string(), file: "a.asm".to_string(), line: 7, canon: 7 },
            CanonLine { text: "IF DEF(MISSING) || (A * B != 6)".to_string(), file: "a.asm".to_string(), line: 8, canon: 8 },
            CanonLine { text: "LD B, $33".to_string(), file: "a.asm".to_string(), line: 9, canon: 9 },
            CanonLine { text: "ELSE".to_string(), file: "a.asm".to_string(), line: 10, canon: 10 },
            CanonLine { text: "LD B, $44".to_string(), file: "a.asm".to_string(), line: 11, canon: 11 },
            CanonLine { text: "ENDC".to_string(), file: "a.asm".to_string(), line: 12, canon: 12 },
        ];
        let out = expand_rgbds_semantics(&src);
        assert!(out.iter().any(|l| l.text == "LD A, $11"));
        assert!(!out.iter().any(|l| l.text == "LD A, $22"));
        assert!(!out.iter().any(|l| l.text == "LD B, $33"));
        assert!(out.iter().any(|l| l.text == "LD B, $44"));
    }

    #[test]
    fn rept_expands_body_count() {
        let src = vec![
            CanonLine { text: "REPT 3".to_string(), file: "a.asm".to_string(), line: 1, canon: 1 },
            CanonLine { text: "LD A, $01".to_string(), file: "a.asm".to_string(), line: 2, canon: 2 },
            CanonLine { text: "ENDR".to_string(), file: "a.asm".to_string(), line: 3, canon: 3 },
        ];
        let out = expand_rgbds_semantics(&src);
        assert_eq!(out.iter().filter(|l| l.text == "LD A, $01").count(), 3);
    }

    #[test]
    fn macro_local_labels_are_rewritten() {
        let src = vec![
            CanonLine { text: "LoopMacro: MACRO".to_string(), file: "a.asm".to_string(), line: 1, canon: 1 },
            CanonLine { text: ".loop:".to_string(), file: "a.asm".to_string(), line: 2, canon: 2 },
            CanonLine { text: "JR .loop".to_string(), file: "a.asm".to_string(), line: 3, canon: 3 },
            CanonLine { text: "ENDM".to_string(), file: "a.asm".to_string(), line: 4, canon: 4 },
            CanonLine { text: "LoopMacro".to_string(), file: "a.asm".to_string(), line: 5, canon: 5 },
        ];
        let out = expand_rgbds_semantics(&src);
        assert!(out.iter().any(|l| l.text.contains("__m")));
        assert!(!out.iter().any(|l| l.text == ".loop:"));
        assert!(!out.iter().any(|l| l.text == "JR .loop"));
    }

    #[test]
    fn import_writes_outputs() {
        let base = std::env::temp_dir().join(format!("kain_import_gb_test_{}", SystemTime::now().duration_since(UNIX_EPOCH).expect("clock").as_nanos()));
        fs::create_dir_all(&base).expect("mkdir");
        let input = base.join("gb_source.asm");
        fs::write(&input, "SECTION \"ROM0\", ROM0[$100]\nStart::\nLD A, $01\ndb $10, $20\n").expect("write input");
        let prev = std::env::current_dir().expect("cwd");
        std::env::set_current_dir(&base).expect("set cwd");
        let result = import_asm(&input, "lr35902-gameboy", None, false).expect("import");
        std::env::set_current_dir(prev).expect("restore");
        assert!(result.canonical_asm_path.exists());
        assert!(result.generated_kn_path.exists());
        assert!(result.map_json_path.exists());
        assert!(result.report_json_path.exists());
        let diag_path = result.map_json_path.parent().expect("map parent").join("gameboy_diagnostics.json");
        assert!(diag_path.exists());
        let trace_path = result.map_json_path.parent().expect("map parent").join("gameboy_trace.json");
        assert!(trace_path.exists());
    }

    #[test]
    fn diagnostics_resolve_sparse_canonical_line_numbers() {
        let lines = vec![
            CanonLine { text: "LD A, $01".to_string(), file: "a.asm".to_string(), line: 10, canon: 100 },
            CanonLine { text: "BAD TOKEN".to_string(), file: "a.asm".to_string(), line: 11, canon: 120 },
        ];
        let expanded = "LD A, $01\nBAD TOKEN";
        let mapper = SpanMapper::new(expanded);
        let starts = compute_line_starts(expanded);
        let issue = RecoveryIssue { line: 120, message: "Unrecognized canonical line: BAD TOKEN".to_string() };
        let diag = issue_to_diag("error", "ASM_UNRESOLVED", &issue, &lines, &mapper, &starts).expect("diag");
        assert_eq!(diag.canonical_line, 120);
        assert_eq!(diag.canonical_snippet, "BAD TOKEN");
        assert_eq!(diag.source_line, 11);
    }

    #[test]
    fn lr35902_step_ld_a_imm8_and_xor_flags() {
        let mut mem = Lr35902Memory::default();
        mem.rom0[0x100] = 0x3e; // LD A,d8
        mem.rom0[0x101] = 0x42;
        mem.rom0[0x102] = 0xaf; // XOR A

        let s0 = Lr35902State::default();
        let s1 = step_lr35902(s0, mem);
        assert_eq!(s1.opcode, 0x3e);
        assert_eq!(s1.cycles, 8);
        assert_eq!(s1.state.a, 0x42);
        assert_eq!(s1.state.pc, 0x0102);

        let s2 = step_lr35902(s1.state, s1.memory);
        assert_eq!(s2.opcode, 0xaf);
        assert_eq!(s2.state.a, 0x00);
        assert_eq!(s2.state.f & FLAG_Z, FLAG_Z);
        assert_eq!(s2.cycles, 4);
    }

    #[test]
    fn lr35902_step_jr_nz_respects_zero_flag() {
        let mut mem = Lr35902Memory::default();
        mem.rom0[0x100] = 0x20; // JR NZ, r8
        mem.rom0[0x101] = 0x02;

        let mut s0 = Lr35902State::default();
        s0.f = 0;
        let taken = step_lr35902(s0.clone(), mem.clone());
        assert_eq!(taken.state.pc, 0x0104);
        assert_eq!(taken.cycles, 12);

        s0.f = FLAG_Z;
        let not_taken = step_lr35902(s0, mem);
        assert_eq!(not_taken.state.pc, 0x0102);
        assert_eq!(not_taken.cycles, 8);
    }

    #[test]
    fn lr35902_step_ldh_emits_port_write_effect() {
        let mut mem = Lr35902Memory::default();
        mem.rom0[0x100] = 0x3e; // LD A,d8
        mem.rom0[0x101] = 0x7f;
        mem.rom0[0x102] = 0xe0; // LDH [a8],A
        mem.rom0[0x103] = 0x10;

        let s0 = Lr35902State::default();
        let s1 = step_lr35902(s0, mem);
        let s2 = step_lr35902(s1.state, s1.memory);
        assert_eq!(s2.opcode, 0xe0);
        assert_eq!(s2.memory.io[0x10], 0x7f);
        assert_eq!(s2.effect, StepEffect::PortWrite { port: 0x10, value: 0x7f });
    }

    #[test]
    fn lr35902_step_add_and_sub_update_a_and_flags() {
        let mut mem = Lr35902Memory::default();
        mem.rom0[0x100] = 0x3e; // LD A,$10
        mem.rom0[0x101] = 0x10;
        mem.rom0[0x102] = 0xc6; // ADD A,$22
        mem.rom0[0x103] = 0x22;
        mem.rom0[0x104] = 0xd6; // SUB $05
        mem.rom0[0x105] = 0x05;

        let s0 = Lr35902State::default();
        let s1 = step_lr35902(s0, mem);
        let s2 = step_lr35902(s1.state, s1.memory);
        assert_eq!(s2.state.a, 0x32);
        assert_eq!(s2.state.f & FLAG_N, 0);

        let s3 = step_lr35902(s2.state, s2.memory);
        assert_eq!(s3.state.a, 0x2d);
        assert_eq!(s3.state.f & FLAG_N, FLAG_N);
    }

    #[test]
    fn lr35902_step_call_and_ret_roundtrip_pc() {
        let mut mem = Lr35902Memory::default();
        mem.rom0[0x100] = 0xcd; // CALL $0200
        mem.rom0[0x101] = 0x00;
        mem.rom0[0x102] = 0x02;
        mem.rom0[0x200] = 0xc9; // RET

        let s0 = Lr35902State::default();
        let call = step_lr35902(s0, mem);
        assert_eq!(call.state.pc, 0x0200);
        assert_eq!(call.state.sp, 0xfffc);
        let ret = step_lr35902(call.state, call.memory);
        assert_eq!(ret.state.pc, 0x0103);
        assert_eq!(ret.state.sp, 0xfffe);
    }

    #[test]
    fn lr35902_step_cb_bit_sets_zero_when_bit_clear() {
        let mut mem = Lr35902Memory::default();
        mem.rom0[0x100] = 0xcb;
        mem.rom0[0x101] = 0x7c; // BIT 7,H
        let mut s0 = Lr35902State::default();
        s0.h = 0x00;
        let s1 = step_lr35902(s0, mem);
        assert_eq!(s1.cycles, 8);
        assert_eq!(s1.state.f & FLAG_Z, FLAG_Z);
        assert_eq!(s1.state.f & FLAG_H, FLAG_H);
    }

    #[test]
    fn lr35902_step_services_interrupt_when_ime_enabled() {
        let mut mem = Lr35902Memory::default();
        mem.ie = 0x01;
        mem.io[0x0f] = 0x01;
        let mut s0 = Lr35902State::default();
        s0.ime = true;
        let s1 = step_lr35902(s0, mem);
        assert_eq!(s1.state.pc, 0x0040);
        assert_eq!(s1.state.ime, false);
        assert_eq!(s1.cycles, 20);
        assert_eq!(s1.effect, StepEffect::Interrupt { vector: 0x0040 });
    }

    #[test]
    fn lr35902_div_increments_every_256_cycles() {
        let mut mem = Lr35902Memory::default();
        mem.rom0[0x100] = 0x00; // NOP
        let mut state = Lr35902State::default();
        for _ in 0..64 {
            let step = step_lr35902(state, mem);
            state = step.state;
            mem = step.memory;
        }
        assert_eq!(mem.io[IO_DIV], 1);
    }

    #[test]
    fn lr35902_tima_overflow_reloads_tma_and_requests_interrupt() {
        let mut mem = Lr35902Memory::default();
        mem.rom0[0x100] = 0xc3; // JP $0100 (16 cycles)
        mem.rom0[0x101] = 0x00;
        mem.rom0[0x102] = 0x01;
        mem.io[IO_TAC] = 0x05; // enable + 16-cycle timer period
        mem.io[IO_TIMA] = 0xff;
        mem.io[IO_TMA] = 0x42;
        let s0 = Lr35902State::default();
        let s1 = step_lr35902(s0, mem);
        assert_eq!(s1.state.pc, 0x0100);
        assert_eq!(s1.memory.io[IO_TIMA], 0x42);
        assert_eq!(s1.memory.io[IO_IF] & IF_BIT_TIMER, IF_BIT_TIMER);
    }

    #[test]
    fn lr35902_ppu_ly_and_vblank_interrupt_progress_with_cycles() {
        let mut mem = Lr35902Memory::default();
        mem.io[IO_LCDC] = 0x80;
        mem.rom0[0x100] = 0xc3; // JP $0100 (16 cycles steady cadence)
        mem.rom0[0x101] = 0x00;
        mem.rom0[0x102] = 0x01;
        let mut state = Lr35902State::default();
        for _ in 0..4104 {
            let step = step_lr35902(state, mem);
            state = step.state;
            mem = step.memory;
        }
        assert_eq!(mem.io[IO_LY], 144);
        assert_eq!(mem.io[IO_IF] & IF_BIT_VBLANK, IF_BIT_VBLANK);
    }

    #[test]
    fn lr35902_stat_mode_tracks_ppu_phase() {
        let mut mem = Lr35902Memory::default();
        mem.io[IO_LCDC] = 0x80;

        advance_clock(&mut mem, 1);
        assert_eq!(mem.io[IO_STAT] & 0x03, 2); // OAM scan

        advance_clock(&mut mem, 79);
        assert_eq!(mem.io[IO_STAT] & 0x03, 3); // pixel transfer

        advance_clock(&mut mem, 172);
        assert_eq!(mem.io[IO_STAT] & 0x03, 0); // HBlank
    }

    #[test]
    fn lr35902_stat_lyc_interrupt_requests_lcdstat() {
        let mut mem = Lr35902Memory::default();
        mem.io[IO_LCDC] = 0x80;
        mem.io[IO_STAT] = 0x40; // enable LYC==LY STAT interrupt
        mem.io[IO_LYC] = 1;

        // Advance one full line so LY becomes 1 and matches LYC.
        let mut state = Lr35902State::default();
        mem.rom0[0x100] = 0xc3;
        mem.rom0[0x101] = 0x00;
        mem.rom0[0x102] = 0x01;
        for _ in 0..29 {
            let step = step_lr35902(state, mem);
            state = step.state;
            mem = step.memory;
        }

        assert_eq!(mem.io[IO_LY], 1);
        assert_eq!(mem.io[IO_STAT] & 0x04, 0x04); // coincidence flag set
        assert_eq!(mem.io[IO_IF] & IF_BIT_LCDSTAT, IF_BIT_LCDSTAT);
    }

    #[test]
    fn lr35902_scanline_window_overrides_background() {
        let mut mem = Lr35902Memory::default();
        mem.io[IO_LCDC] = 0x91 | 0x20 | 0x40; // LCD on + BG + unsigned tile data + window on + window map 9c00
        mem.io[IO_BGP] = 0b1110_0100; // identity mapping for 2-bit shades
        mem.io[IO_WY] = 0;
        mem.io[IO_WX] = 7; // window starts at x=0

        // BG tile 0 -> color id 1 at leftmost pixel.
        mem.vram[0x1800] = 0;
        set_tile_row(&mut mem, 0, 0, 0x80, 0x00);
        // Window tile 1 -> color id 2 at leftmost pixel.
        mem.vram[0x1c00] = 1;
        set_tile_row(&mut mem, 1, 0, 0x00, 0x80);

        render_scanline(&mut mem, 0);
        assert_eq!(mem.framebuffer[0], 2);
    }

    #[test]
    fn lr35902_scanline_sprite_respects_bg_priority_flag() {
        let mut mem = Lr35902Memory::default();
        mem.io[IO_LCDC] = 0x93; // LCD on + BG on + OBJ on + unsigned tile data
        mem.io[IO_BGP] = 0b1110_0100;
        mem.io[IO_OBP0] = 0b1110_0100;

        // BG tile 0 -> color id 1 at x=0.
        mem.vram[0x1800] = 0;
        set_tile_row(&mut mem, 0, 0, 0x80, 0x00);

        // Sprite at (0,0), tile 2, priority behind BG.
        mem.oam[0] = 16; // y + 16
        mem.oam[1] = 8;  // x + 8
        mem.oam[2] = 2;  // tile
        mem.oam[3] = 0x80; // OBJ behind BG
        set_tile_row(&mut mem, 2, 0, 0x00, 0x80); // sprite color id 2

        render_scanline(&mut mem, 0);
        assert_eq!(mem.framebuffer[0], 1);
    }

    #[test]
    fn lr35902_oam_scan_overflow_prefers_first_ten_in_oam_order() {
        let mut mem = Lr35902Memory::default();
        mem.io[IO_LCDC] = 0x93;
        // First ten sprites are off to the right and all on line 0.
        for i in 0..10usize {
            let base = i * 4;
            mem.oam[base] = 16;
            mem.oam[base + 1] = 100;
            mem.oam[base + 2] = 0;
            mem.oam[base + 3] = 0;
        }
        // 11th sprite would affect x=0 if selected.
        mem.oam[40] = 16;
        mem.oam[41] = 8;
        mem.oam[42] = 1;
        mem.oam[43] = 0;
        set_tile_row(&mut mem, 1, 0, 0x80, 0x00);

        render_scanline(&mut mem, 0);
        assert_eq!(mem.framebuffer[0], 0);
    }

    #[test]
    fn lr35902_oam_scan_progresses_one_entry_per_two_dots() {
        let mut mem = Lr35902Memory::default();
        mem.io[IO_LCDC] = 0x93;
        for i in 0..3usize {
            let base = i * 4;
            mem.oam[base] = 16;
            mem.oam[base + 1] = 8 + i as u8;
            mem.oam[base + 2] = i as u8;
            mem.oam[base + 3] = 0;
        }

        let scan2 = select_line_sprites(&mem, 0, 4);
        assert_eq!(scan2.len(), 2);
        assert_eq!(scan2[0].index, 0);
        assert_eq!(scan2[1].index, 1);

        let scan3 = select_line_sprites(&mem, 0, 6);
        assert_eq!(scan3.len(), 3);
        assert_eq!(scan3[2].index, 2);
    }

    #[test]
    fn lr35902_window_line_counter_skips_hidden_lines() {
        let mut mem = Lr35902Memory::default();
        mem.io[IO_LCDC] = 0x91 | 0x20 | 0x40;
        mem.io[IO_BGP] = 0b1110_0100;
        mem.io[IO_WY] = 0;
        mem.io[IO_WX] = 7;
        // Window row uses tile 1; rows within the tile encode colors 1/2/3.
        mem.vram[0x1c00] = 1;
        set_tile_row(&mut mem, 1, 0, 0x80, 0x00);
        set_tile_row(&mut mem, 1, 1, 0x00, 0x80);
        set_tile_row(&mut mem, 1, 2, 0x80, 0x80);

        render_scanline(&mut mem, 0);
        assert_eq!(mem.framebuffer[0], 1);
        assert_eq!(mem.window_line_counter, 1);

        mem.io[IO_WX] = 167; // hidden this line
        render_scanline(&mut mem, 1);
        assert_eq!(mem.window_line_counter, 1);

        mem.io[IO_WX] = 7;
        render_scanline(&mut mem, 2);
        assert_eq!(mem.framebuffer[2 * 160], 2);
        assert_eq!(mem.window_line_counter, 2);
    }

    #[test]
    fn lr35902_window_counter_resets_on_new_frame() {
        let mut mem = Lr35902Memory::default();
        mem.io[IO_LCDC] = 0x80;
        mem.io[IO_LY] = 153;
        mem.ppu_cycle_accum = 455;
        mem.window_line_counter = 9;
        advance_ppu_cycles(&mut mem, 1);
        assert_eq!(mem.io[IO_LY], 0);
        assert_eq!(mem.window_line_counter, 0);
    }

    #[test]
    fn lr35902_stat_mode0_interrupt_on_mode_edge() {
        let mut mem = Lr35902Memory::default();
        mem.io[IO_LCDC] = 0x80;
        mem.io[IO_STAT] = 0x08; // mode 0 interrupt enable
        mem.io[IO_LY] = 0;
        mem.ppu_cycle_accum = 251; // currently mode 3
        mem.last_ppu_mode = 3;
        mem.io[IO_IF] = 0;
        mem.stat_irq_latch = false;

        advance_clock(&mut mem, 1); // enter HBlank mode 0
        assert_eq!(mem.io[IO_STAT] & 0x03, 0);
        assert_eq!(mem.io[IO_IF] & IF_BIT_LCDSTAT, IF_BIT_LCDSTAT);
    }

    #[test]
    fn lr35902_stat_lyc_interrupt_requires_rising_edge() {
        let mut mem = Lr35902Memory::default();
        mem.io[IO_LCDC] = 0x80;
        mem.io[IO_STAT] = 0x40; // LYC interrupt enable
        mem.io[IO_LY] = 5;
        mem.io[IO_LYC] = 5;
        mem.stat_irq_latch = false;
        mem.io[IO_IF] = 0;

        update_stat_and_irq(&mut mem);
        assert_eq!(mem.io[IO_IF] & IF_BIT_LCDSTAT, IF_BIT_LCDSTAT);

        mem.io[IO_IF] &= !IF_BIT_LCDSTAT;
        update_stat_and_irq(&mut mem);
        assert_eq!(mem.io[IO_IF] & IF_BIT_LCDSTAT, 0);
    }

    #[test]
    fn lr35902_stat_mode_trace_matches_known_boundaries() {
        let mut mem = Lr35902Memory::default();
        mem.io[IO_LCDC] = 0x80;
        mem.io[IO_LY] = 0;
        mem.ppu_cycle_accum = 0;
        update_stat_and_irq(&mut mem);
        assert_eq!(mem.io[IO_STAT] & 0x03, 2);

        advance_ppu_cycles(&mut mem, 79);
        assert_eq!(mem.io[IO_STAT] & 0x03, 2);

        advance_ppu_cycles(&mut mem, 1);
        assert_eq!(mem.io[IO_STAT] & 0x03, 3);

        advance_ppu_cycles(&mut mem, 171);
        assert_eq!(mem.io[IO_STAT] & 0x03, 3);

        advance_ppu_cycles(&mut mem, 1);
        assert_eq!(mem.io[IO_STAT] & 0x03, 0);

        advance_ppu_cycles(&mut mem, 203);
        assert_eq!(mem.io[IO_LY], 0);
        assert_eq!(mem.io[IO_STAT] & 0x03, 0);

        advance_ppu_cycles(&mut mem, 1);
        assert_eq!(mem.io[IO_LY], 1);
        assert_eq!(mem.io[IO_STAT] & 0x03, 2);
    }

    #[test]
    fn lr35902_stat_enters_vblank_mode_at_ly144_boundary() {
        let mut mem = Lr35902Memory::default();
        mem.io[IO_LCDC] = 0x80;
        mem.io[IO_LY] = 143;
        mem.ppu_cycle_accum = 455;
        mem.io[IO_IF] = 0;
        update_stat_and_irq(&mut mem);

        advance_ppu_cycles(&mut mem, 1);
        assert_eq!(mem.io[IO_LY], 144);
        assert_eq!(mem.io[IO_STAT] & 0x03, 1);
        assert_eq!(mem.io[IO_IF] & IF_BIT_VBLANK, IF_BIT_VBLANK);
    }

    #[test]
    fn lr35902_mbc_switches_romx_bank_for_4000_window() {
        let mut mem = Lr35902Memory::default();
        mem.rom0[0x147] = 0x01; // MBC1
        mem.romx = vec![vec![0; 0x4000], vec![0; 0x4000]];
        mem.romx[0][0] = 0x11; // bank 1 first byte
        mem.romx[1][0] = 0x22; // bank 2 first byte
        write8(&mut mem, 0x2000, 0x01);
        let s = Lr35902State::default();
        assert_eq!(read8(&s, &mem, 0x4000), 0x11);
        write8(&mut mem, 0x2000, 0x02);
        assert_eq!(read8(&s, &mem, 0x4000), 0x22);
    }

    #[test]
    fn lr35902_dma_ff46_copies_160_bytes_into_oam() {
        let mut mem = Lr35902Memory::default();
        for i in 0..0xa0u16 {
            write8(&mut mem, 0xc000u16 + i, (i & 0xff) as u8);
        }
        write8(&mut mem, 0xff46, 0xc0);
        for _ in 0..160 {
            advance_clock(&mut mem, 4);
        }
        assert_eq!(mem.oam[0], 0x00);
        assert_eq!(mem.oam[0x5f], 0x5f);
        assert_eq!(mem.oam[0x9f], 0x9f);
        assert_eq!(mem.dma_cycles_remaining, 0);
    }

    #[test]
    fn lr35902_grouped_ld_r_r_opcode_executes() {
        let mut mem = Lr35902Memory::default();
        mem.rom0[0x100] = 0x53; // LD D,E
        let mut s0 = Lr35902State::default();
        s0.e = 0x9a;
        let s1 = step_lr35902(s0, mem);
        assert_eq!(s1.state.d, 0x9a);
        assert_eq!(s1.cycles, 4);
    }

    #[test]
    fn lr35902_serial_start_completes_and_requests_interrupt() {
        let mut mem = Lr35902Memory::default();
        mem.rom0[0x100] = 0xc3; // JP $0100, 16 cycles
        mem.rom0[0x101] = 0x00;
        mem.rom0[0x102] = 0x01;
        write8(&mut mem, 0xff02, 0x81); // start transfer
        let mut state = Lr35902State::default();
        for _ in 0..256 {
            let step = step_lr35902(state, mem);
            state = step.state;
            mem = step.memory;
        }
        assert_eq!(mem.io[IO_SC] & 0x80, 0);
        assert_eq!(mem.io[IO_IF] & IF_BIT_SERIAL, IF_BIT_SERIAL);
    }

    #[test]
    fn lr35902_mbc5_selects_9bit_rom_bank() {
        let mut mem = Lr35902Memory::default();
        mem.rom0[0x147] = 0x19; // MBC5
        mem.romx = (0..300).map(|_| vec![0; 0x4000]).collect();
        mem.romx[0][0] = 0x11;   // bank 1
        mem.romx[255][0] = 0x22; // bank 256
        write8(&mut mem, 0x2000, 0x00); // low byte 0
        write8(&mut mem, 0x3000, 0x01); // high bit 1 => bank 256
        let s = Lr35902State::default();
        assert_eq!(read8(&s, &mem, 0x4000), 0x22);
    }

    #[test]
    fn lr35902_dma_blocks_non_hram_cpu_bus_access() {
        let mut mem = Lr35902Memory::default();
        write8(&mut mem, 0xc000, 0x5a);
        write8(&mut mem, 0xff46, 0xc0);
        let s = Lr35902State::default();
        assert_eq!(read8(&s, &mem, 0xc000), 0xff);
        assert_eq!(read8(&s, &mem, 0xff80), 0x00);
    }

    #[test]
    fn lr35902_ei_uses_delayed_ime_enable() {
        let mut mem = Lr35902Memory::default();
        mem.ie = IF_BIT_VBLANK;
        mem.io[IO_IF] = IF_BIT_VBLANK;
        mem.rom0[0x100] = 0xfb; // EI
        mem.rom0[0x101] = 0x00; // NOP

        let s0 = Lr35902State::default();
        let s1 = step_lr35902(s0, mem);
        assert!(!s1.state.ime);
        assert_eq!(s1.state.pc, 0x0101);

        let s2 = step_lr35902(s1.state, s1.memory);
        assert!(s2.state.ime);
        assert_eq!(s2.state.pc, 0x0102);

        let s3 = step_lr35902(s2.state, s2.memory);
        assert_eq!(s3.effect, StepEffect::Interrupt { vector: 0x0040 });
    }

    #[test]
    fn lr35902_joypad_interrupt_on_falling_edge() {
        let mut mem = Lr35902Memory::default();
        mem.joypad_dpad = 0x0f;
        write8(&mut mem, 0xff00, 0x20); // select dpad
        assert_eq!(mem.io[IO_IF] & IF_BIT_JOYPAD, 0);

        mem.joypad_dpad = 0x0e; // press Right
        write8(&mut mem, 0xff00, 0x20);
        assert_eq!(mem.io[IO_IF] & IF_BIT_JOYPAD, IF_BIT_JOYPAD);
    }

    #[test]
    fn lr35902_step_conditional_jp_and_call_respect_flags() {
        let mut mem = Lr35902Memory::default();
        mem.rom0[0x100] = 0xc2; // JP NZ,$0200
        mem.rom0[0x101] = 0x00;
        mem.rom0[0x102] = 0x02;
        mem.rom0[0x200] = 0xc4; // CALL NZ,$0300
        mem.rom0[0x201] = 0x00;
        mem.rom0[0x202] = 0x03;

        let mut s0 = Lr35902State::default();
        s0.f = 0;
        let jp_taken = step_lr35902(s0, mem.clone());
        assert_eq!(jp_taken.state.pc, 0x0200);
        assert_eq!(jp_taken.cycles, 16);

        let call_taken = step_lr35902(jp_taken.state, jp_taken.memory);
        assert_eq!(call_taken.state.pc, 0x0300);
        assert_eq!(call_taken.state.sp, 0xfffc);
        assert_eq!(call_taken.cycles, 24);

        let mut s1 = Lr35902State::default();
        s1.f = FLAG_Z;
        let jp_not_taken = step_lr35902(s1, mem.clone());
        assert_eq!(jp_not_taken.state.pc, 0x0103);
        assert_eq!(jp_not_taken.cycles, 12);

        let mut s_call_skip = Lr35902State::default();
        s_call_skip.pc = 0x0200;
        s_call_skip.f = FLAG_Z;
        let call_not_taken = step_lr35902(s_call_skip, mem);
        assert_eq!(call_not_taken.state.pc, 0x0203);
        assert_eq!(call_not_taken.state.sp, 0xfffe);
        assert_eq!(call_not_taken.cycles, 12);
    }

    #[test]
    fn lr35902_step_reti_and_rst_update_pc_sp_and_ime() {
        let mut mem = Lr35902Memory::default();
        mem.rom0[0x100] = 0xff; // RST 38H
        mem.rom0[0x038] = 0xd9; // RETI

        let s0 = Lr35902State::default();
        let rst = step_lr35902(s0, mem);
        assert_eq!(rst.state.pc, 0x0038);
        assert_eq!(rst.state.sp, 0xfffc);

        let reti = step_lr35902(rst.state, rst.memory);
        assert_eq!(reti.state.pc, 0x0101);
        assert_eq!(reti.state.sp, 0xfffe);
        assert!(reti.state.ime);
    }

    #[test]
    fn lr35902_step_daa_cpl_scf_ccf_flags() {
        let mut mem = Lr35902Memory::default();
        mem.rom0[0x100] = 0x3e; // LD A,$15
        mem.rom0[0x101] = 0x15;
        mem.rom0[0x102] = 0xc6; // ADD A,$27
        mem.rom0[0x103] = 0x27;
        mem.rom0[0x104] = 0x27; // DAA
        mem.rom0[0x105] = 0x2f; // CPL
        mem.rom0[0x106] = 0x37; // SCF
        mem.rom0[0x107] = 0x3f; // CCF

        let s0 = Lr35902State::default();
        let s1 = step_lr35902(s0, mem);
        let s2 = step_lr35902(s1.state, s1.memory);
        let daa_res = step_lr35902(s2.state, s2.memory);
        assert_eq!(daa_res.state.a, 0x42);
        assert_eq!(daa_res.state.f & FLAG_C, 0);

        let cpl_res = step_lr35902(daa_res.state, daa_res.memory);
        assert_eq!(cpl_res.state.a, 0xbd);
        assert_eq!(cpl_res.state.f & FLAG_N, FLAG_N);
        assert_eq!(cpl_res.state.f & FLAG_H, FLAG_H);

        let scf_res = step_lr35902(cpl_res.state, cpl_res.memory);
        assert_eq!(scf_res.state.f & FLAG_C, FLAG_C);
        assert_eq!(scf_res.state.f & FLAG_N, 0);
        assert_eq!(scf_res.state.f & FLAG_H, 0);

        let ccf_res = step_lr35902(scf_res.state, scf_res.memory);
        assert_eq!(ccf_res.state.f & FLAG_C, 0);
        assert_eq!(ccf_res.state.f & FLAG_N, 0);
        assert_eq!(ccf_res.state.f & FLAG_H, 0);
    }
}
