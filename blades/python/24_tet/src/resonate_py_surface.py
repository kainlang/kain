"""resonate_py_surface — thin pygame/modernGL surface for Kain-owned 24-TET piano.

Kain owns: event loop, key dedup, state mutation, musical logic.
Python owns: window init, rendering, audio synthesis, mouse hit-test, mgl staging.
"""

from __future__ import annotations

import math
import os
import warnings
from typing import Any

os.environ.setdefault("PYGAME_HIDE_SUPPORT_PROMPT", "1")
warnings.filterwarnings(
    "ignore",
    message=r"pkg_resources is deprecated as an API.*",
    category=UserWarning,
)

import moderngl
import numpy as np
import pygame

WINDOW_WIDTH = 1600
WINDOW_HEIGHT = 720
WINDOW_TITLE = "resonate-py 24 TET"
SAMPLE_RATE = 44_100
SYNTH_SECONDS = 1.45
WHITE_SLOTS = [0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17, 19, 21, 23]
BLACK_SLOTS = [slot for slot in range(24) if slot not in WHITE_SLOTS]
NOTE_NAMES = [
    "A", "A\u2191", "A#", "A#\u2191", "B", "B\u2191",
    "C", "C\u2191", "C#", "C#\u2191", "D", "D\u2191",
    "D#", "D#\u2191", "E", "E\u2191", "F", "F\u2191",
    "F#", "F#\u2191", "G", "G\u2191", "G#", "G#\u2191",
]

# Module-level state
_mgl_ctx: moderngl.Context | None = None
_mgl_buf: moderngl.Buffer | None = None
_pygame_init: bool = False
_mixer_ready: bool = False
_screen: pygame.Surface | None = None
_frame_surface: pygame.Surface | None = None
_clock: pygame.time.Clock | None = None
_font_title: pygame.font.Font | None = None
_font_body: pygame.font.Font | None = None
_font_small: pygame.font.Font | None = None
_font_tiny: pygame.font.Font | None = None
_backdrop: pygame.Surface | None = None
_key_geometry: dict[int, dict[str, Any]] | None = None
_sound_cache: dict[int, pygame.mixer.Sound] = {}
_frame_counter: int = 0

# Kain-owned effect parameters (set via config_effects from Kain event loop)
_effect_config: dict[str, int] = {
    "lfo1_val": 0,
    "lfo2_val": 0,
    "chorus_mix": 0,
    "delay_mix": 0,
    "reverb_mix": 0,
    "distortion_drive": 0,
    "filter_cutoff": 1000,
    "tremolo_depth": 0,
    "tremolo_rate": 20,
    "chorus_delay_ms": 18,
    "chorus_rate": 15,
    "delay_time_ms": 320,
    "delay_feedback": 280,
    "reverb_decay": 350,
    "fx_epoch": 0,
    "fx_frame": 0,
    "mod_filter": 1000,
    "mod_tremolo": 0,
    "mod_chorus": 0,
}


# ---------------------------------------------------------------------------
# Effect processing
# ---------------------------------------------------------------------------

def _apply_filter_tilt(samples: np.ndarray, cutoff: int, sample_rate: int = SAMPLE_RATE) -> np.ndarray:
    """Apply a gentle lowpass/lowshelf filter tilt based on cutoff parameter.
    cutoff: 0-1000 where 0=fully closed, 1000=fully open.
    This is a simplified one-pole filter applied in frequency domain
    via convolution with a short windowed-sinc kernel."""
    if cutoff >= 950:
        return samples
    blend = max(0.02, (cutoff + 50) / 1050.0)
    n = len(samples)
    filtered = np.zeros_like(samples)
    filtered[0] = samples[0]
    for i in range(1, n):
        filtered[i] = samples[i] * blend + filtered[i - 1] * (1.0 - blend)
    return filtered


def _apply_distortion(samples: np.ndarray, drive: int) -> np.ndarray:
    """Soft-clip waveshaping. drive: 0-1000."""
    if drive <= 5:
        return samples
    gain = 1.0 + (drive / 500.0)
    shaped = np.tanh(samples * gain * 1.5)
    norm = 1.0 / max(1e-6, float(np.max(np.abs(shaped))))
    return shaped * min(norm, 1.0) * 0.92


def _apply_chorus(samples: np.ndarray, mix: int, delay_ms: int, lfo: int) -> np.ndarray:
    """Simple modulated delay. mix: 0-1000, delay_ms: delay time, lfo: modulate offset."""
    if mix <= 5:
        return samples
    wet = mix / 1000.0
    delay_samples = int((delay_ms * SAMPLE_RATE) / 1000)
    if delay_samples < 2 or delay_samples >= len(samples):
        return samples
    # LFO modulates delay time by ±30%
    lfo_mod = (lfo / 500.0) * delay_samples * 0.3
    offset = max(1, delay_samples + int(lfo_mod))
    chorus = np.roll(samples, offset, axis=0)
    chorus[:offset] *= np.linspace(0.0, 1.0, offset, dtype=np.float32)[:, None]
    return samples * (1.0 - wet) + chorus * wet * 0.6


def _apply_delay_echo(samples: np.ndarray, mix: int, time_ms: int, feedback: int) -> np.ndarray:
    """Simple delay/echo. mix: 0-1000, time_ms: delay time, feedback: 0-1000."""
    if mix <= 5:
        return samples
    wet = mix / 1000.0
    fb = (feedback / 1000.0) * 0.7
    delay_samples = int((time_ms * SAMPLE_RATE) / 1000)
    if delay_samples < 2 or delay_samples >= len(samples):
        return samples
    echo = np.zeros_like(samples)
    for tap in range(1, 5):
        tap_samples = delay_samples * tap
        if tap_samples >= len(samples):
            break
        tap_gain = wet * (fb ** (tap - 1))
        echo[tap_samples:] += samples[:-tap_samples] * tap_gain
    return samples + echo


def _apply_tremolo(samples: np.ndarray, depth: int, rate: int, lfo_phase: int) -> np.ndarray:
    """Amplitude modulation. depth: 0-1000, rate: LFO rate, lfo_phase: 0-65535."""
    if depth <= 5:
        return samples
    mod_depth = depth / 1000.0
    n = len(samples)
    t = np.arange(n, dtype=np.float32) / SAMPLE_RATE
    # Convert phase to frequency: phase wraps at a rate determined by the tremolo_rate
    phase_rad = (lfo_phase / 65535.0) * 2.0 * math.pi
    lfo_freq = 0.5 + (rate / 15.0)  # ~0.5 to ~34 Hz
    lfo = np.sin(phase_rad + 2.0 * math.pi * lfo_freq * t)
    mod = 1.0 - mod_depth * 0.5 * (lfo * 0.5 + 0.5)
    return samples * mod[:, None]


# ---------------------------------------------------------------------------
# Audio helpers
# ---------------------------------------------------------------------------

def _freq(slot: int) -> float:
    """24-TET frequency in Hz for a given slot (0-23)."""
    return 220.0 * (2.0 ** ((float(slot) - 12.0) / 24.0))


def _make_sound(note_slot: int) -> pygame.mixer.Sound | None:
    if not _mixer_ready:
        return None
    cached = _sound_cache.get(note_slot)
    if cached is not None:
        return cached

    sample_count = int(SAMPLE_RATE * SYNTH_SECONDS)
    t = np.linspace(0.0, SYNTH_SECONDS, sample_count, endpoint=False, dtype=np.float32)
    freq = _freq(note_slot)
    fundamental = np.sin(2.0 * np.pi * freq * t)
    harmonic_2 = 0.34 * np.sin(2.0 * np.pi * freq * 2.0 * t + 0.08)
    harmonic_3 = 0.19 * np.sin(2.0 * np.pi * freq * 3.0 * t + 0.12)
    harmonic_5 = 0.08 * np.sin(2.0 * np.pi * freq * 5.0 * t + 0.2)
    body = fundamental + harmonic_2 + harmonic_3 + harmonic_5

    attack = max(1, int(SAMPLE_RATE * 0.012))
    decay = max(1, int(SAMPLE_RATE * 0.22))
    release = max(1, int(SAMPLE_RATE * 0.58))
    envelope = np.ones(sample_count, dtype=np.float32) * 0.66
    envelope[:attack] = np.linspace(0.0, 1.0, attack, dtype=np.float32)
    envelope[attack: attack + decay] = np.linspace(1.0, 0.7, decay, dtype=np.float32)
    envelope[-release:] *= np.linspace(1.0, 0.0, release, dtype=np.float32)

    rng = np.random.default_rng(note_slot + 4242)
    hammer = rng.normal(0.0, 0.12, sample_count).astype(np.float32)
    hammer *= np.exp(-42.0 * t, dtype=np.float32)

    wave = (body * envelope) + (hammer * 0.18)
    lowpass = np.exp(-3.5 * t, dtype=np.float32)
    wave = wave * (0.78 + (0.22 * lowpass))
    wave /= max(1.0, float(np.max(np.abs(wave))))

    stereo = np.stack((wave * 0.97, wave * 0.9), axis=1)
    sample = np.ascontiguousarray(np.int16(np.clip(stereo, -1.0, 1.0) * 32767))
    sound = pygame.sndarray.make_sound(sample)
    _sound_cache[note_slot] = sound
    return sound


def _active_voice_count() -> int:
    if not _mixer_ready:
        return 0
    return sum(1 for slot in range(24) if pygame.mixer.Channel(slot).get_busy())


# ---------------------------------------------------------------------------
# Pygame init / teardown
# ---------------------------------------------------------------------------

def _ensure_pygame() -> None:
    global _pygame_init, _mixer_ready, _clock, _font_title, _font_body, _font_small, _font_tiny
    if _pygame_init:
        return
    pygame.mixer.pre_init(SAMPLE_RATE, -16, 2, 512)
    pygame.init()
    pygame.display.init()
    pygame.font.init()
    try:
        pygame.mixer.init(SAMPLE_RATE, -16, 2, 512)
        pygame.mixer.set_num_channels(24)
        _mixer_ready = True
    except pygame.error:
        _mixer_ready = False
    _clock = pygame.time.Clock()
    _font_title = pygame.font.Font(None, 42)
    _font_body = pygame.font.Font(None, 28)
    _font_small = pygame.font.Font(None, 22)
    _font_tiny = pygame.font.Font(None, 18)
    _pygame_init = True


def _ensure_window() -> None:
    global _screen, _frame_surface
    _ensure_pygame()
    if _screen is None:
        try:
            _screen = pygame.display.set_mode(
                (WINDOW_WIDTH, WINDOW_HEIGHT),
                pygame.DOUBLEBUF | pygame.SCALED,
                vsync=1,
            )
        except Exception:
            _screen = pygame.display.set_mode(
                (WINDOW_WIDTH, WINDOW_HEIGHT),
                pygame.DOUBLEBUF,
            )
        pygame.display.set_caption(WINDOW_TITLE)
    if _frame_surface is None:
        _frame_surface = pygame.Surface((WINDOW_WIDTH, WINDOW_HEIGHT)).convert()
    if _backdrop is None or _key_geometry is None:
        _rebuild_scene_cache()


# ---------------------------------------------------------------------------
# Rendering helpers
# ---------------------------------------------------------------------------

def _draw_gradient(surface: pygame.Surface, rect: pygame.Rect, top: tuple[int, int, int], bottom: tuple[int, int, int]) -> None:
    height = max(1, rect.height)
    for row in range(height):
        blend = row / max(1, height - 1)
        color = (
            int(top[0] + (bottom[0] - top[0]) * blend),
            int(top[1] + (bottom[1] - top[1]) * blend),
            int(top[2] + (bottom[2] - top[2]) * blend),
        )
        pygame.draw.line(surface, color, (rect.left, rect.top + row), (rect.right, rect.top + row))


def _draw_text(surface: pygame.Surface, text: str, pos: tuple[int, int], color: tuple[int, int, int], *, font_key: str = "font_body") -> None:
    font = globals()["_" + font_key]
    glyph = font.render(text, True, color)
    surface.blit(glyph, pos)


def _white_rect(index: int, count: int, key_top: int, key_bottom: int) -> pygame.Rect:
    left_margin = 86
    right_margin = 86
    usable = WINDOW_WIDTH - left_margin - right_margin
    width = usable / count
    left = int(left_margin + index * width)
    return pygame.Rect(left, key_top, int(width) + 1, key_bottom - key_top)


def _rebuild_scene_cache() -> None:
    global _backdrop, _key_geometry
    backdrop = pygame.Surface((WINDOW_WIDTH, WINDOW_HEIGHT)).convert()
    _draw_gradient(backdrop, pygame.Rect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT), (10, 12, 19), (23, 18, 15))

    halo = pygame.Surface((WINDOW_WIDTH, WINDOW_HEIGHT), pygame.SRCALPHA)
    pygame.draw.ellipse(halo, (75, 116, 255, 26), pygame.Rect(220, 18, 1160, 228))
    pygame.draw.ellipse(halo, (255, 170, 80, 18), pygame.Rect(310, 44, 980, 182))
    backdrop.blit(halo, (0, 0))

    header_rect = pygame.Rect(54, 32, WINDOW_WIDTH - 108, 132)
    pygame.draw.rect(backdrop, (18, 22, 33), header_rect, border_radius=24)
    pygame.draw.rect(backdrop, (58, 68, 96), header_rect, 2, border_radius=24)

    wood_rect = pygame.Rect(40, 180, WINDOW_WIDTH - 80, WINDOW_HEIGHT - 228)
    _draw_gradient(backdrop, wood_rect, (72, 47, 28), (38, 23, 13))
    pygame.draw.rect(backdrop, (98, 66, 40), wood_rect, 2, border_radius=26)

    keyboard_shelf = pygame.Rect(60, 208, WINDOW_WIDTH - 120, WINDOW_HEIGHT - 258)
    pygame.draw.rect(backdrop, (42, 30, 20), keyboard_shelf, border_radius=20)

    key_top = keyboard_shelf.top + 28
    key_bottom = keyboard_shelf.bottom - 16
    white_keys: list[dict[str, Any]] = []
    white_index_by_slot: dict[int, int] = {}
    for index, slot in enumerate(WHITE_SLOTS):
        rect = _white_rect(index, len(WHITE_SLOTS), key_top, key_bottom)
        white_index_by_slot[slot] = index
        white_keys.append({"slot": slot, "rect": rect})

    black_keys: list[dict[str, Any]] = []
    for slot in BLACK_SLOTS:
        prev_slot = max(s for s in WHITE_SLOTS if s < slot)
        next_slot = min(s for s in WHITE_SLOTS if s > slot)
        prev_rect = white_keys[white_index_by_slot[prev_slot]]["rect"]
        next_rect = white_keys[white_index_by_slot[next_slot]]["rect"]
        width = int(prev_rect.width * 0.54)
        left = int((prev_rect.right + next_rect.left) / 2 - (width / 2))
        top = key_top
        height = int((key_bottom - key_top) * 0.61)
        black_keys.append({"slot": slot, "rect": pygame.Rect(left, top, width, height)})

    for key in white_keys:
        rect = key["rect"]
        _draw_gradient(backdrop, rect, (252, 248, 240), (220, 208, 188))
        pygame.draw.rect(backdrop, (103, 86, 71), rect, 2, border_radius=0)
        shadow = pygame.Surface((rect.width, rect.height), pygame.SRCALPHA)
        pygame.draw.rect(shadow, (0, 0, 0, 18), pygame.Rect(0, rect.height - 18, rect.width, 18))
        backdrop.blit(shadow, rect.topleft)

    for key in black_keys:
        rect = key["rect"]
        _draw_gradient(backdrop, rect, (66, 69, 78), (20, 22, 29))
        pygame.draw.rect(backdrop, (8, 10, 14), rect, 2, border_radius=8)
        inset = rect.inflate(-10, -18)
        pygame.draw.rect(backdrop, (98, 102, 116), inset, 1, border_radius=6)

    _draw_text(backdrop, "resonate-py // authored 24-TET instrument", (88, 54), (243, 245, 250), font_key="font_title")
    _draw_text(backdrop, "quarter-tone piano, pygame surface, LLVM path, and reactive Kain state mesh", (90, 98), (168, 183, 215), font_key="font_small")
    _draw_text(backdrop, "Z S X D C V G B H N J M + Q 2 W 3 E R 5 T 6 Y 7 U  |  Esc quits", (88, WINDOW_HEIGHT - 44), (203, 193, 176), font_key="font_small")

    key_geometry: dict[int, dict[str, Any]] = {}
    for index, key in enumerate(white_keys):
        key_geometry[key["slot"]] = {
            "slot": key["slot"],
            "rect": key["rect"],
            "label_pos": (key["rect"].left + 12, key["rect"].bottom - 64),
            "freq_pos": (key["rect"].left + 12, key["rect"].bottom - 38),
            "is_white": True,
            "draw_order": index,
        }
    for index, key in enumerate(black_keys):
        key_geometry[key["slot"]] = {
            "slot": key["slot"],
            "rect": key["rect"],
            "label_pos": (key["rect"].left + 10, key["rect"].top + 16),
            "freq_pos": (key["rect"].left + 8, key["rect"].top + 38),
            "is_white": False,
            "draw_order": 100 + index,
        }

    _backdrop = backdrop
    _key_geometry = key_geometry


# ---------------------------------------------------------------------------
# UI rendering
# ---------------------------------------------------------------------------

def _ambient_color(resonance_hash: int, ui_epoch: int) -> tuple[int, int, int]:
    hue = (resonance_hash // 137 + ui_epoch * 9) % 255
    return (70 + (hue // 4), 88 + ((hue * 2) % 92), 122 + ((hue * 3) % 104))


def _draw_header(surface: pygame.Surface, active: int, velocity: int, epoch: int, resonance_hash: int, pitch_value: int, ui_epoch: int, shader_epoch: int) -> None:
    caption = NOTE_NAMES[active] if 0 <= active < 24 else "idle"
    voices = _active_voice_count()
    ambient = _ambient_color(resonance_hash, ui_epoch)

    badge = pygame.Rect(1040, 54, 228, 70)
    pygame.draw.rect(surface, (27, 33, 48), badge, border_radius=18)
    pygame.draw.rect(surface, ambient, badge, 2, border_radius=18)
    _draw_text(surface, f"voices {voices:02d}", (1062, 66), (245, 248, 252))
    _draw_text(surface, f"resonate {resonance_hash % 100000:05d}", (1062, 92), (194, 210, 236), font_key="font_small")

    _draw_text(surface, f"note {caption}   velocity {velocity:03d}   pitch {pitch_value / 1000.0:.3f} Hz", (88, 112), (232, 216, 192), font_key="font_body")
    _draw_text(surface, f"epoch {epoch}   ui {ui_epoch}   shader {shader_epoch}   audio {'on' if _mixer_ready else 'off'}", (88, 138), (177, 188, 213), font_key="font_small")


def _draw_resonance_halo(surface: pygame.Surface, active: int, velocity: int, resonance_hash: int, ui_epoch: int, shader_epoch: int) -> None:
    if active < 0 or _key_geometry is None:
        return
    key = _key_geometry[active]
    rect = key["rect"]
    center = rect.centerx
    halo = pygame.Surface((WINDOW_WIDTH, WINDOW_HEIGHT), pygame.SRCALPHA)
    hue = _ambient_color(resonance_hash, ui_epoch)
    for radius, alpha in ((210, 24), (170, 40), (126, 56)):
        color = (min(255, hue[0] + 20), min(255, hue[1] + 22), min(255, hue[2] + 36), alpha)
        pygame.draw.ellipse(halo, color, pygame.Rect(center - radius, 188, radius * 2, 320))
    if shader_epoch > 0:
        pulse_y = 188 + ((shader_epoch * 9) % 332)
        pygame.draw.line(halo, (255, 198, 112, 72), (84, pulse_y), (WINDOW_WIDTH - 84, pulse_y), 2)
    surface.blit(halo, (0, 0))


def _draw_key(surface: pygame.Surface, slot: int, active: bool, velocity: int, resonance_hash: int, ui_epoch: int, shader_epoch: int) -> None:
    if _key_geometry is None:
        return
    key = _key_geometry[slot]
    rect = key["rect"]
    note_name = NOTE_NAMES[slot]
    pitch_text = f"{_freq(slot):.2f}"

    if key["is_white"]:
        if active:
            overlay = pygame.Surface((rect.width, rect.height), pygame.SRCALPHA)
            _draw_gradient(overlay, overlay.get_rect(), (255, 241, 210), (246, 191, 120))
            surface.blit(overlay, rect.topleft)
            glow = pygame.Surface((rect.width, 40), pygame.SRCALPHA)
            pygame.draw.rect(glow, (255, 176, 72, 120), glow.get_rect(), border_radius=8)
            surface.blit(glow, (rect.left, rect.bottom - 60))
        pygame.draw.rect(surface, (112, 90, 72), rect, 2)
        accent = _ambient_color(resonance_hash, ui_epoch)
        label_color = (57, 46, 38) if not active else (49, 32, 16)
        freq_color = (92, 82, 72) if not active else (120, 64, 18)
        if active:
            pygame.draw.rect(surface, accent, pygame.Rect(rect.left + 8, rect.top + 8, rect.width - 16, 12), border_radius=6)
        _draw_text(surface, note_name, key["label_pos"], label_color)
        _draw_text(surface, pitch_text, key["freq_pos"], freq_color, font_key="font_small")
    else:
        if active:
            overlay = pygame.Surface((rect.width, rect.height), pygame.SRCALPHA)
            _draw_gradient(overlay, overlay.get_rect(), (144, 116, 255), (40, 28, 78))
            surface.blit(overlay, rect.topleft)
            pygame.draw.rect(surface, (255, 212, 140), rect, 2, border_radius=8)
        accent = (227, 220, 214) if not active else (255, 237, 208)
        freq_color = (152, 157, 173) if not active else (250, 198, 136)
        _draw_text(surface, note_name, key["label_pos"], accent, font_key="font_small")
        _draw_text(surface, pitch_text, key["freq_pos"], freq_color, font_key="font_tiny")
        if active:
            pulse = pygame.Rect(rect.left + 10, rect.bottom - 18, rect.width - 20, 8)
            pygame.draw.rect(surface, (255, 191, 103), pulse, border_radius=4)

    if active:
        meter_h = max(16, int((velocity / 127.0) * 96))
        meter = pygame.Rect(rect.centerx - 6, rect.bottom - meter_h - 22, 12, meter_h)
        pygame.draw.rect(surface, (255, 208, 120), meter, border_radius=6)
        pygame.draw.rect(surface, (255, 246, 221), meter, 1, border_radius=6)


def _draw_diagnostic_overlay(surface: pygame.Surface, epoch: int, frame: int, velocity: int, resonance_hash: int) -> None:
    info = (
        f"frame {frame:05d}  ep {epoch:05d}  vel {velocity:03d}  "
        f"hash {resonance_hash % 100000:05d}"
    )
    bg = pygame.Rect(WINDOW_WIDTH - 620, 10, 606, 22)
    pygame.draw.rect(surface, (10, 12, 19, 180), bg, border_radius=6)
    _draw_text(surface, info, (WINDOW_WIDTH - 612, 12), (120, 200, 120), font_key="font_tiny")


def _draw_keyboard(surface: pygame.Surface, note_slot: int, velocity: int, epoch: int, resonance_hash: int, pitch_value: int, ui_epoch: int, shader_epoch: int, active_note: int, active_velocity: int) -> None:
    active = note_slot if 0 <= note_slot < 24 else active_note
    current_pitch = pitch_value if pitch_value > 0 else int(_freq(active if active >= 0 else 0) * 1000)
    current_velocity = velocity if velocity > 0 else active_velocity
    _draw_header(surface, active, current_velocity, epoch, resonance_hash, current_pitch, ui_epoch, shader_epoch)
    _draw_resonance_halo(surface, active, current_velocity, resonance_hash, ui_epoch, shader_epoch)

    if _key_geometry is not None:
        ordered = sorted(_key_geometry.values(), key=lambda item: item["draw_order"])
        for item in ordered:
            _draw_key(surface, item["slot"], item["slot"] == active, current_velocity, resonance_hash, ui_epoch, shader_epoch)

    footer_rect = pygame.Rect(980, WINDOW_HEIGHT - 70, 514, 34)
    pygame.draw.rect(surface, (24, 27, 37), footer_rect, border_radius=12)
    pygame.draw.rect(surface, (66, 78, 110), footer_rect, 1, border_radius=12)
    _draw_text(surface, "click ivory or ebony keys, or use the mapped keyboard row", (998, WINDOW_HEIGHT - 62), (183, 193, 217), font_key="font_small")
    pygame.display.set_caption(f"{WINDOW_TITLE} :: {NOTE_NAMES[active] if active >= 0 else 'idle'} :: epoch {epoch} :: voices {_active_voice_count()}")


# ---------------------------------------------------------------------------
# Hit test
# ---------------------------------------------------------------------------

def hit_test(x_pos: int, y_pos: int) -> int:
    if _key_geometry is None:
        return -2
    black_first = sorted((_key_geometry[slot] for slot in BLACK_SLOTS), key=lambda item: item["draw_order"])
    for item in black_first:
        if item["rect"].collidepoint(x_pos, y_pos):
            return item["slot"]
    for slot in WHITE_SLOTS:
        if _key_geometry[slot]["rect"].collidepoint(x_pos, y_pos):
            return slot
    return -2


# ---------------------------------------------------------------------------
# Effect configuration (called from Kain event loop)
# ---------------------------------------------------------------------------

def config_effects(
    lfo1_val: int,
    lfo2_val: int,
    chorus_mix: int,
    delay_mix: int,
    reverb_mix: int,
    distortion_drive: int,
    filter_cutoff: int,
    tremolo_depth: int,
    tremolo_rate: int,
    chorus_delay_ms: int,
    delay_time_ms: int,
    delay_feedback: int,
    reverb_decay: int,
    fx_epoch: int,
    fx_frame: int,
    mod_filter: int,
    mod_tremolo: int,
    mod_chorus: int,
) -> int:
    """Receive frame-level effect parameters from the Kain effects world."""
    _effect_config.update({
        "lfo1_val": lfo1_val,
        "lfo2_val": lfo2_val,
        "chorus_mix": chorus_mix,
        "delay_mix": delay_mix,
        "reverb_mix": reverb_mix,
        "distortion_drive": distortion_drive,
        "filter_cutoff": filter_cutoff,
        "tremolo_depth": tremolo_depth,
        "tremolo_rate": tremolo_rate,
        "chorus_delay_ms": chorus_delay_ms,
        "delay_time_ms": delay_time_ms,
        "delay_feedback": delay_feedback,
        "reverb_decay": reverb_decay,
        "fx_epoch": fx_epoch,
        "fx_frame": fx_frame,
        "mod_filter": mod_filter,
        "mod_tremolo": mod_tremolo,
        "mod_chorus": mod_chorus,
    })
    return fx_epoch


# ---------------------------------------------------------------------------
# Public surface API
# ---------------------------------------------------------------------------

def init() -> int:
    """Initialize pygame, create window, build scene cache. Returns 1 on success."""
    global _frame_counter
    _ensure_window()
    _frame_counter = 0
    version = pygame.get_sdl_version()
    return version[0] * 10000 + version[1] * 100 + version[2]


def render_frame(
    note_slot: int,
    velocity: int,
    epoch: int,
    resonance_hash: int,
    pitch_milli: int,
    ui_epoch: int,
    shader_epoch: int,
    active_note: int,
    active_velocity: int,
) -> int:
    """Render one frame. Returns 0."""
    global _frame_counter
    _ensure_window()

    frame_surface = _frame_surface
    frame_surface.blit(_backdrop, (0, 0))
    _draw_keyboard(frame_surface, note_slot, velocity, epoch, resonance_hash, pitch_milli, ui_epoch, shader_epoch, active_note, active_velocity)
    _draw_diagnostic_overlay(frame_surface, epoch, _frame_counter, velocity, resonance_hash)
    _screen.blit(frame_surface, (0, 0))
    pygame.display.flip()
    _clock.tick(60)
    _frame_counter += 1
    return 0


def play_note(slot: int, velocity: int) -> int:
    """Synthesize and play audio for the given note. Returns 0."""
    slot_i = int(slot)
    sound = _make_sound(slot_i)
    if sound is None:
        return 0
    channel = pygame.mixer.Channel(slot_i % 24)
    gain = max(0.18, min(1.0, velocity / 127.0))

    # Apply frame-level effect modulation from Kain-owned state
    ec = _effect_config
    lfo1 = ec["lfo1_val"]
    lfo2 = ec["lfo2_val"]
    mod_chorus_val = ec["mod_chorus"]
    mod_filter_val = ec["mod_filter"]
    mod_tremolo_val = ec["mod_tremolo"]

    # Tremolo: modulate gain
    trem_depth = ec["tremolo_depth"]
    trem_rate = ec["tremolo_rate"]
    if trem_depth > 5:
        # Apply tremolo by creating a modulated copy of the sound
        sound_array = pygame.sndarray.samples(sound).copy().astype(np.float32)
        # Simple LFO phase from mod_tremolo value (0-1000 scale)
        lfo_phase = (mod_tremolo_val * 65535) // 1000 if mod_tremolo_val > 0 else 0
        t = np.arange(len(sound_array), dtype=np.float32) / SAMPLE_RATE
        phase_rad = (lfo_phase / 65535.0) * 2.0 * math.pi
        lfo_freq = 0.5 + (trem_rate / 15.0)
        lfo = np.sin(phase_rad + 2.0 * math.pi * lfo_freq * t)
        mod = 1.0 - (trem_depth / 1000.0) * 0.5 * (lfo * 0.5 + 0.5)
        if sound_array.ndim == 1:
            sound_array *= mod
        else:
            sound_array *= mod[:, None]
        sound_array = np.clip(sound_array, -32768, 32767).astype(np.int16)
        sound = pygame.sndarray.make_sound(sound_array)

    # Distortion: waveshape the sound buffer
    dist_drive = ec["distortion_drive"]
    if dist_drive > 5:
        sound_array = pygame.sndarray.samples(sound).copy().astype(np.float32)
        sound_array = _apply_distortion(sound_array, dist_drive)
        sound_array = np.clip(sound_array, -32768, 32767).astype(np.int16)
        sound = pygame.sndarray.make_sound(sound_array)

    # Filter tilt from Kain-modulated filter_cutoff
    filter_cut = mod_filter_val
    if 0 < filter_cut < 950:
        sound_array = pygame.sndarray.samples(sound).copy().astype(np.float32)
        sound_array = _apply_filter_tilt(sound_array, filter_cut)
        sound_array = np.clip(sound_array, -32768, 32767).astype(np.int16)
        sound = pygame.sndarray.make_sound(sound_array)

    # Chorus: modulated short delay
    ch_mix = ec["chorus_mix"]
    ch_delay = ec["chorus_delay_ms"]
    if ch_mix > 5 and mod_chorus_val > 0:
        sound_array = pygame.sndarray.samples(sound).copy().astype(np.float32)
        sound_array = _apply_chorus(sound_array, ch_mix, ch_delay, lfo1)
        sound_array = np.clip(sound_array, -32768, 32767).astype(np.int16)
        sound = pygame.sndarray.make_sound(sound_array)

    # Delay/echo
    dl_mix = ec["delay_mix"]
    dl_time = ec["delay_time_ms"]
    dl_fb = ec["delay_feedback"]
    if dl_mix > 5 and dl_time > 10:
        sound_array = pygame.sndarray.samples(sound).copy().astype(np.float32)
        sound_array = _apply_delay_echo(sound_array, dl_mix, dl_time, dl_fb)
        sound_array = np.clip(sound_array, -32768, 32767).astype(np.int16)
        sound = pygame.sndarray.make_sound(sound_array)

    channel.set_volume(gain * 0.92, gain * 0.86)
    channel.play(sound)
    return 0


def shutdown() -> int:
    """Clean up pygame and moderngl. Returns 1."""
    global _mgl_ctx, _mgl_buf, _pygame_init, _mixer_ready, _screen, _frame_surface
    global _clock, _font_title, _font_body, _font_small, _font_tiny, _backdrop, _key_geometry
    global _sound_cache, _frame_counter
    if _mgl_buf is not None:
        try:
            _mgl_buf.release()
        except Exception:
            pass
        _mgl_buf = None
    if _mgl_ctx is not None:
        try:
            _mgl_ctx.release()
        except Exception:
            pass
        _mgl_ctx = None
    if _mixer_ready:
        try:
            pygame.mixer.stop()
        except Exception:
            pass
        _sound_cache = {}
    if _pygame_init:
        try:
            pygame.quit()
        except Exception:
            pass
    _pygame_init = False
    _mixer_ready = False
    _screen = None
    _frame_surface = None
    _clock = None
    _font_title = None
    _font_body = None
    _font_small = None
    _font_tiny = None
    _backdrop = None
    _key_geometry = None
    _sound_cache = {}
    _frame_counter = 0
    return 1


# ---------------------------------------------------------------------------
# ModernGL helpers (used by benchmark cases)
# ---------------------------------------------------------------------------

def mgl_prepare() -> int:
    global _mgl_ctx, _mgl_buf
    if _mgl_ctx is None:
        _mgl_ctx = moderngl.create_standalone_context()
    if _mgl_buf is None:
        seed = np.zeros(24, dtype="f4").tobytes()
        _mgl_buf = _mgl_ctx.buffer(seed)
    return _mgl_buf.size


def mgl_push(note_slot: int, velocity: int, epoch: int) -> int:
    mgl_prepare()
    arr = np.zeros(24, dtype="f4")
    arr[int(note_slot) % 24] = float(velocity) + (float(epoch) * 0.125)
    _mgl_buf.write(arr.tobytes())
    return int(_mgl_buf.size + int(arr.sum() * 100.0))
