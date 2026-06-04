"""resonate_py_bridge — thin pygame/modernGL surface for Kain-owned 24-TET piano.

Kain owns all musical logic (pitch, velocity, note names, scores, dynamics).
Python owns only: window management, event polling, audio synthesis, and display.
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
KEY_ORDER = [
    pygame.K_z, pygame.K_s, pygame.K_x, pygame.K_d,
    pygame.K_c, pygame.K_v, pygame.K_g, pygame.K_b,
    pygame.K_h, pygame.K_n, pygame.K_j, pygame.K_m,
    pygame.K_q, pygame.K_2, pygame.K_w, pygame.K_3,
    pygame.K_e, pygame.K_r, pygame.K_5, pygame.K_t,
    pygame.K_6, pygame.K_y, pygame.K_7, pygame.K_u,
]
KEY_CHAR_MAP = {
    "z": 0, "s": 1, "x": 2, "d": 3,
    "c": 4, "v": 5, "g": 6, "b": 7,
    "h": 8, "n": 9, "j": 10, "m": 11,
    "q": 12, "2": 13, "w": 14, "3": 15,
    "e": 16, "r": 17, "5": 18, "t": 19,
    "6": 20, "y": 21, "7": 22, "u": 23,
}
WHITE_SLOTS = [0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17, 19, 21, 23]
BLACK_SLOTS = [slot for slot in range(24) if slot not in WHITE_SLOTS]
NOTE_NAMES = [
    "A", "A\u2191", "A#", "A#\u2191", "B", "B\u2191",
    "C", "C\u2191", "C#", "C#\u2191", "D", "D\u2191",
    "D#", "D#\u2191", "E", "E\u2191", "F", "F\u2191",
    "F#", "F#\u2191", "G", "G\u2191", "G#", "G#\u2191",
]

STATE: dict[str, Any] = {
    "mgl_ctx": None,
    "mgl_buf": None,
    "pygame_init": False,
    "mixer_ready": False,
    "screen": None,
    "frame_surface": None,
    "clock": None,
    "font_title": None,
    "font_body": None,
    "font_small": None,
    "font_tiny": None,
    "backdrop": None,
    "key_geometry": None,
    "sound_cache": {},
    "active_note": -1,
    "active_velocity": 0,
    "held_keys": set(),
    "mouse_down": False,
    "latched_note": -1,
    "frame": 0,
    "last_epoch": -1,
    "audio_hits": 0,
}


# ---------------------------------------------------------------------------
# Audio helpers (pure computation, no Kain bridge needed)
# ---------------------------------------------------------------------------

def _freq(slot: int) -> float:
    """24-TET frequency in Hz for a given slot (0-23)."""
    return 220.0 * (2.0 ** ((float(slot) - 12.0) / 24.0))


# ---------------------------------------------------------------------------
# Pygame init / teardown
# ---------------------------------------------------------------------------

def _ensure_pygame() -> None:
    if STATE["pygame_init"]:
        return
    pygame.mixer.pre_init(SAMPLE_RATE, -16, 2, 512)
    pygame.init()
    pygame.display.init()
    pygame.font.init()
    try:
        pygame.mixer.init(SAMPLE_RATE, -16, 2, 512)
        pygame.mixer.set_num_channels(24)
        STATE["mixer_ready"] = True
    except pygame.error:
        STATE["mixer_ready"] = False
    STATE["clock"] = pygame.time.Clock()
    STATE["font_title"] = pygame.font.Font(None, 42)
    STATE["font_body"] = pygame.font.Font(None, 28)
    STATE["font_small"] = pygame.font.Font(None, 22)
    STATE["font_tiny"] = pygame.font.Font(None, 18)
    STATE["pygame_init"] = True


def _ensure_window() -> None:
    _ensure_pygame()
    if STATE["screen"] is None:
        flags = pygame.DOUBLEBUF
        STATE["screen"] = pygame.display.set_mode((WINDOW_WIDTH, WINDOW_HEIGHT), flags)
        pygame.display.set_caption(WINDOW_TITLE)
    if STATE["frame_surface"] is None:
        STATE["frame_surface"] = pygame.Surface((WINDOW_WIDTH, WINDOW_HEIGHT)).convert()
    if STATE["backdrop"] is None or STATE["key_geometry"] is None:
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
    font = STATE[font_key]
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

    STATE["backdrop"] = backdrop
    STATE["key_geometry"] = key_geometry


# ---------------------------------------------------------------------------
# Audio synthesis (kept in Python — needs pygame mixer)
# ---------------------------------------------------------------------------

def _make_sound(note_slot: int) -> pygame.mixer.Sound | None:
    if not STATE["mixer_ready"]:
        return None
    cached = STATE["sound_cache"].get(note_slot)
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
    STATE["sound_cache"][note_slot] = sound
    return sound


def _play_note(note_slot: int, velocity: int) -> None:
    sound = _make_sound(note_slot)
    if sound is None:
        return
    channel = pygame.mixer.Channel(int(note_slot) % 24)
    gain = max(0.18, min(1.0, velocity / 127.0))
    channel.set_volume(gain * 0.92, gain * 0.86)
    channel.play(sound)
    STATE["audio_hits"] += 1


def _active_voice_count() -> int:
    if not STATE["mixer_ready"]:
        return 0
    return sum(1 for slot in range(24) if pygame.mixer.Channel(slot).get_busy())


# ---------------------------------------------------------------------------
# UI rendering (driven by Kain-provided params)
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
    _draw_text(surface, f"epoch {epoch}   ui {ui_epoch}   shader {shader_epoch}   audio {'on' if STATE['mixer_ready'] else 'off'}", (88, 138), (177, 188, 213), font_key="font_small")


def _draw_resonance_halo(surface: pygame.Surface, active: int, velocity: int, resonance_hash: int, ui_epoch: int, shader_epoch: int) -> None:
    if active < 0:
        return
    key = STATE["key_geometry"][active]
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
    key = STATE["key_geometry"][slot]
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


def _draw_keyboard(surface: pygame.Surface, note_slot: int, velocity: int, epoch: int, resonance_hash: int, pitch_value: int, ui_epoch: int, shader_epoch: int) -> None:
    active = note_slot if 0 <= note_slot < 24 else STATE["active_note"]
    current_pitch = pitch_value if pitch_value > 0 else int(_freq(active if active >= 0 else 0) * 1000)
    _draw_header(surface, active, velocity, epoch, resonance_hash, current_pitch, ui_epoch, shader_epoch)
    _draw_resonance_halo(surface, active, velocity, resonance_hash, ui_epoch, shader_epoch)

    ordered = sorted(STATE["key_geometry"].values(), key=lambda item: item["draw_order"])
    for item in ordered:
        _draw_key(surface, item["slot"], item["slot"] == active, velocity, resonance_hash, ui_epoch, shader_epoch)

    footer_rect = pygame.Rect(980, WINDOW_HEIGHT - 70, 514, 34)
    pygame.draw.rect(surface, (24, 27, 37), footer_rect, border_radius=12)
    pygame.draw.rect(surface, (66, 78, 110), footer_rect, 1, border_radius=12)
    _draw_text(surface, "click ivory or ebony keys, or use the mapped keyboard row", (998, WINDOW_HEIGHT - 62), (183, 193, 217), font_key="font_small")
    pygame.display.set_caption(f"{WINDOW_TITLE} :: {NOTE_NAMES[active] if active >= 0 else 'idle'} :: epoch {epoch} :: voices {_active_voice_count()}")


# ---------------------------------------------------------------------------
# Event handling
# ---------------------------------------------------------------------------

def _hit_test(pos: tuple[int, int]) -> int:
    x_pos, y_pos = pos
    black_first = sorted((STATE["key_geometry"][slot] for slot in BLACK_SLOTS), key=lambda item: item["draw_order"])
    for item in black_first:
        if item["rect"].collidepoint(x_pos, y_pos):
            return item["slot"]
    for slot in WHITE_SLOTS:
        if STATE["key_geometry"][slot]["rect"].collidepoint(x_pos, y_pos):
            return slot
    return -2


def _event_note(event: pygame.event.Event) -> int:
    if event.type == pygame.KEYDOWN:
        if event.key == pygame.K_ESCAPE:
            return -1
        char = str(getattr(event, "unicode", "") or "").lower()
        if char in KEY_CHAR_MAP:
            held_keys = STATE["held_keys"]
            if char not in held_keys:
                held_keys.add(char)
                return KEY_CHAR_MAP[char]
            return -2
        if event.key in KEY_ORDER:
            held_keys = STATE["held_keys"]
            if event.key not in held_keys:
                held_keys.add(event.key)
                return KEY_ORDER.index(event.key)
            return -2
    if event.type == pygame.KEYUP:
        char = str(getattr(event, "unicode", "") or "").lower()
        if char:
            STATE["held_keys"].discard(char)
        if event.key in KEY_ORDER:
            STATE["held_keys"].discard(event.key)
        return -2
    if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
        if STATE["mouse_down"] == False:
            STATE["mouse_down"] = True
            return _hit_test(event.pos)
        return -2
    if event.type == pygame.MOUSEBUTTONUP and event.button == 1:
        STATE["mouse_down"] = False
        return -2
    if event.type == pygame.WINDOWLEAVE:
        STATE["mouse_down"] = False
        STATE["held_keys"].clear()
        return -2
    return -2


# ---------------------------------------------------------------------------
# Game loop (event poll + render — triggered by Kain each frame)
# ---------------------------------------------------------------------------

def window_frame(
    note_slot: int,
    velocity: int,
    epoch: int,
    resonance_hash: int,
    pitch_value: int,
    ui_epoch: int,
    shader_epoch: int,
) -> int:
    _ensure_window()

    selected_note = -2
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            return -1
        event_note = _event_note(event)
        if event_note == -1:
            return -1
        if event_note >= 0:
            selected_note = event_note

    if epoch != STATE["last_epoch"] and 0 <= note_slot < 24:
        _play_note(int(note_slot), int(velocity))
        STATE["last_epoch"] = int(epoch)
        STATE["active_note"] = int(note_slot)
        STATE["active_velocity"] = int(velocity)

    active_note = STATE["active_note"] if STATE["active_note"] >= 0 else note_slot
    active_velocity = STATE["active_velocity"] if STATE["active_velocity"] > 0 else velocity

    frame_surface = STATE["frame_surface"]
    frame_surface.blit(STATE["backdrop"], (0, 0))
    _draw_keyboard(frame_surface, active_note, active_velocity, epoch, resonance_hash, pitch_value, ui_epoch, shader_epoch)
    STATE["screen"].blit(frame_surface, (0, 0))
    pygame.display.flip()
    STATE["clock"].tick(60)
    STATE["frame"] += 1

    if selected_note >= 0:
        STATE["active_note"] = selected_note
        STATE["active_velocity"] = max(76, min(127, int(active_velocity) + 6))
        return selected_note + 1
    return 0


# ---------------------------------------------------------------------------
# Initialization / teardown
# ---------------------------------------------------------------------------

def reset() -> int:
    if STATE["mgl_buf"] is not None:
        try:
            STATE["mgl_buf"].release()
        except Exception:
            pass
        STATE["mgl_buf"] = None
    if STATE["mgl_ctx"] is not None:
        try:
            STATE["mgl_ctx"].release()
        except Exception:
            pass
        STATE["mgl_ctx"] = None
    if STATE["mixer_ready"]:
        pygame.mixer.stop()
        STATE["sound_cache"] = {}
    if STATE["pygame_init"]:
        try:
            pygame.quit()
        except Exception:
            pass
    STATE.update(
        {
            "pygame_init": False,
            "mixer_ready": False,
            "screen": None,
            "frame_surface": None,
            "clock": None,
            "font_title": None,
            "font_body": None,
            "font_small": None,
            "font_tiny": None,
            "backdrop": None,
            "key_geometry": None,
            "sound_cache": {},
            "active_note": -1,
            "active_velocity": 0,
            "held_keys": set(),
            "mouse_down": False,
            "latched_note": -1,
            "frame": 0,
            "last_epoch": -1,
            "audio_hits": 0,
        }
    )
    return 1


def pygame_init() -> int:
    _ensure_pygame()
    version = pygame.get_sdl_version()
    return version[0] * 10000 + version[1] * 100 + version[2]


def mgl_prepare() -> int:
    if STATE["mgl_ctx"] is None:
        STATE["mgl_ctx"] = moderngl.create_standalone_context()
    if STATE["mgl_buf"] is None:
        seed = np.zeros(24, dtype="f4").tobytes()
        STATE["mgl_buf"] = STATE["mgl_ctx"].buffer(seed)
    return STATE["mgl_buf"].size


def mgl_push(note_slot: int, velocity: int, epoch: int) -> int:
    mgl_prepare()
    arr = np.zeros(24, dtype="f4")
    arr[int(note_slot) % 24] = float(velocity) + (float(epoch) * 0.125)
    STATE["mgl_buf"].write(arr.tobytes())
    return int(STATE["mgl_buf"].size + int(arr.sum() * 100.0))
