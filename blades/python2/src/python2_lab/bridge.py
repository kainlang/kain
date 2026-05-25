from __future__ import annotations

import importlib.util
import json
import os
from typing import Any

os.environ.setdefault("PYGAME_HIDE_SUPPORT_PROMPT", "1")

import numpy as np
import pygame

_PYGAME_READY = False
_PYGAME_SURFACE: pygame.Surface | None = None


def _plan(plan_text: str) -> dict[str, Any]:
    return json.loads(plan_text)


def _state(state_text: str) -> dict[str, Any]:
    return json.loads(state_text)


def _int(payload: dict[str, Any], key: str, default: int) -> int:
    return int(payload.get(key, default))


def _bool(payload: dict[str, Any], key: str, default: bool) -> bool:
    return bool(payload.get(key, default))


def _color(payload: dict[str, Any], key: str, default: list[int]) -> tuple[int, int, int]:
    raw = payload.get(key, default)
    return (int(raw[0]) & 255, int(raw[1]) & 255, int(raw[2]) & 255)


def _ensure_pygame(plan: dict[str, Any]) -> None:
    global _PYGAME_READY
    if _PYGAME_READY:
        return
    os.environ.setdefault("SDL_VIDEODRIVER", str(plan.get("pygame_driver", "dummy")))
    pygame.display.init()
    pygame.font.init()
    _PYGAME_READY = True


def _surface(plan: dict[str, Any]) -> pygame.Surface:
    global _PYGAME_SURFACE
    _ensure_pygame(plan)
    width = _int(plan, "arena_width", 320)
    height = _int(plan, "arena_height", 200)
    use_display = _bool(plan, "pygame_use_display", True)
    hidden = _bool(plan, "pygame_hidden", True)

    if use_display:
        flags = getattr(pygame, "HIDDEN", 0) if hidden else 0
        surface = pygame.display.get_surface()
        if surface is None or surface.get_size() != (width, height):
            surface = pygame.display.set_mode((width, height), flags)
        _PYGAME_SURFACE = surface
    elif _PYGAME_SURFACE is None or _PYGAME_SURFACE.get_size() != (width, height):
        _PYGAME_SURFACE = pygame.Surface((width, height), depth=32)

    assert _PYGAME_SURFACE is not None
    return _PYGAME_SURFACE


def module_digest(plan_text: str) -> int:
    plan = _plan(plan_text)
    score = 0
    if importlib.util.find_spec("pygame") is not None:
        score += _int(plan, "pygame_bonus", 211)
    if importlib.util.find_spec("numpy") is not None:
        score += _int(plan, "numpy_bonus", 223)
    return score


def driver_name(plan_text: str) -> str:
    plan = _plan(plan_text)
    _ensure_pygame(plan)
    return str(pygame.display.get_driver())


def render_frame(plan_text: str, state_text: str) -> np.ndarray:
    plan = _plan(plan_text)
    state = _state(state_text)
    surface = _surface(plan)
    width, height = surface.get_size()

    background = _color(plan, "background", [10, 14, 24])
    lane = _color(plan, "lane", [20, 28, 42])
    player = _color(plan, "player", [255, 176, 64])
    ball = _color(plan, "ball", [242, 246, 255])
    ghost = _color(plan, "ghost", [72, 230, 188])
    accent = int(state.get("accent", 90)) & 255

    surface.fill(background)

    frame = int(state.get("frame", 0))
    score = int(state.get("score", 0))
    lives = int(state.get("lives", 0))
    paddle_x = int(state.get("paddle_x", 0))
    paddle_y = int(state.get("paddle_y", height - 16))
    paddle_w = int(state.get("paddle_w", 56))
    paddle_h = int(state.get("paddle_h", 10))
    ball_x = int(state.get("ball_x", width // 2))
    ball_y = int(state.get("ball_y", height // 2))
    ghost_x = int(state.get("ghost_x", 32))
    ghost_y = int(state.get("ghost_y", 48))

    lane_glow = ((accent + 40) & 255, (64 + accent // 3) & 255, (200 - accent // 4) & 255)
    scanline = pygame.Color((accent + frame * 3) & 255, 36, 58, 255)

    pygame.draw.rect(surface, lane, pygame.Rect(0, height - 40, width, 40))
    pygame.draw.line(surface, lane_glow, (0, height - 41), (width, height - 41), 2)
    pygame.draw.line(surface, scanline, (0, max(0, (frame * 7) % height)), (width, max(0, (frame * 7) % height)), 1)

    for index in range(6):
        star_x = (frame * 11 + index * 47 + score * 3) % max(1, width)
        star_y = (frame * 5 + index * 29 + lives * 17) % max(1, height - 48)
        surface.set_at((star_x, star_y), pygame.Color(255, 255, 255, 255))

    paddle = pygame.Rect(paddle_x, paddle_y, paddle_w, paddle_h)
    ghost_rect = pygame.Rect(ghost_x, ghost_y, 34, 18)

    pygame.draw.rect(surface, player, paddle, border_radius=4)
    pygame.draw.rect(surface, ghost, ghost_rect, border_radius=6)
    pygame.draw.rect(surface, (255, 255, 255), ghost_rect.inflate(-16, -8), border_radius=4)
    pygame.draw.circle(surface, ball, (ball_x + 6, ball_y + 6), 6)

    for life in range(max(0, lives)):
        pygame.draw.rect(surface, player, pygame.Rect(10 + life * 14, 10, 10, 6), border_radius=2)

    score_bar_width = min(width - 20, 10 + (score % max(1, width - 20)))
    pygame.draw.rect(surface, lane_glow, pygame.Rect(10, 22, score_bar_width, 5), border_radius=2)

    pygame.event.pump()
    if _bool(plan, "pygame_use_display", True):
        pygame.display.flip()

    width_major = pygame.surfarray.array3d(surface)
    return np.ascontiguousarray(np.transpose(width_major, (1, 0, 2)), dtype=np.uint8)


def frame_signature(image: Any) -> int:
    return int(np.asarray(image, dtype=np.int64).sum())
