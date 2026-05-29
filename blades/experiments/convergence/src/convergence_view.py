import math
import os
import sys
from collections import deque

import pygame


PALETTE = {
    "bg_top": (8, 12, 20),
    "bg_bottom": (4, 6, 10),
    "panel": (12, 17, 26),
    "panel_soft": (16, 22, 32),
    "panel_edge": (58, 74, 96),
    "panel_edge_soft": (30, 40, 54),
    "text": (232, 238, 246),
    "text_soft": (162, 174, 190),
    "text_dim": (107, 118, 136),
    "maze_floor": (10, 14, 20),
    "maze_grid": (24, 31, 42),
    "maze_wall": (39, 49, 66),
    "maze_wall_edge": (76, 92, 118),
    "pure": (77, 158, 255),
    "greedy": (255, 118, 95),
    "chaos": (99, 224, 160),
    "start": (255, 220, 110),
    "target": (255, 165, 64),
    "rat": (245, 248, 255),
    "accent": (140, 180, 255),
    "spark": (204, 214, 228),
}


def _xy(index, width):
    return index % width, index // width


def _clamp(value, low, high):
    return max(low, min(high, value))


def _lerp(a, b, t):
    return int(a + ((b - a) * t))


def _blend(c0, c1, t):
    return (
        _lerp(c0[0], c1[0], t),
        _lerp(c0[1], c1[1], t),
        _lerp(c0[2], c1[2], t),
    )


def _alpha(color, value):
    return (color[0], color[1], color[2], value)


def _choose_font(candidates, size, bold=False):
    for name in candidates:
        path = pygame.font.match_font(name, bold=bold)
        if path:
            return pygame.font.Font(path, size)
    return pygame.font.Font(None, size)


class ConvergenceWindow:
    def __init__(self, width, height, cell_size, title):
        pygame.init()
        pygame.font.init()

        self.grid_width = int(width)
        self.grid_height = int(height)
        self.cell_size = int(cell_size)

        self.margin = 20
        self.header_h = 72
        self.footer_h = 104
        self.sidebar_w = 304
        self.maze_px_w = self.grid_width * self.cell_size
        self.maze_px_h = self.grid_height * self.cell_size
        self.maze_frame_pad = 12

        self.left_panel_w = self.maze_px_w + (self.maze_frame_pad * 2)
        self.left_panel_h = self.maze_px_h + (self.maze_frame_pad * 2)
        self.content_h = max(self.left_panel_h, 420)
        self.width = self.margin + self.left_panel_w + 16 + self.sidebar_w + self.margin
        self.height = self.margin + self.header_h + self.content_h + self.footer_h + self.margin

        self.surface = pygame.display.set_mode((self.width, self.height))
        pygame.display.set_caption(str(title))
        self.clock = pygame.time.Clock()
        self.closed = False
        self.frames = 0
        self.history = deque(maxlen=180)
        self.capture_path = os.environ.get("CONVERGENCE_CAPTURE_PATH")
        self.captured = False
        self.route_cache_key = None
        self.route_cache = []
        self.lab_maze = self._generate_lab_maze(self.grid_width, self.grid_height)
        self.lab_spine_x = self.grid_width - 4
        self.lab_spine_y = self.grid_height // 2
        self.lab_spine_hub_x = self.grid_width // 2
        self.lab_spine_end_y = self.grid_height - 2

        self.title_font = _choose_font(["Cascadia Mono", "Consolas", "Courier New", "DejaVu Sans Mono"], 28, bold=True)
        self.body_font = _choose_font(["Cascadia Mono", "Consolas", "Courier New", "DejaVu Sans Mono"], 18, bold=False)
        self.small_font = _choose_font(["Cascadia Mono", "Consolas", "Courier New", "DejaVu Sans Mono"], 14, bold=False)
        self.mono_font = _choose_font(["Cascadia Mono", "Consolas", "Courier New", "DejaVu Sans Mono"], 16, bold=False)

        self.background = self._build_background()
        self.layout = self._build_layout()
        self.grid_points = [self._cell_center(index) for index in range(self.grid_width * self.grid_height)]
        self.cell_rects = [self._cell_rect(index) for index in range(self.grid_width * self.grid_height)]

    def _build_layout(self):
        left_x = self.margin
        top_y = self.margin + self.header_h
        maze_x = left_x + self.maze_frame_pad
        maze_y = top_y + self.maze_frame_pad
        sidebar_x = left_x + self.left_panel_w + 16

        return {
            "header": pygame.Rect(left_x, self.margin, self.width - (self.margin * 2), self.header_h),
            "maze_panel": pygame.Rect(left_x, top_y, self.left_panel_w, self.left_panel_h),
            "maze": pygame.Rect(maze_x, maze_y, self.maze_px_w, self.maze_px_h),
            "sidebar": pygame.Rect(sidebar_x, top_y, self.sidebar_w, self.content_h),
            "footer": pygame.Rect(left_x, self.margin + self.header_h + self.content_h, self.width - (self.margin * 2), self.footer_h),
        }

    def _build_background(self):
        surface = pygame.Surface((self.width, self.height))
        for y in range(self.height):
            t = y / max(1, self.height - 1)
            color = _blend(PALETTE["bg_top"], PALETTE["bg_bottom"], t)
            pygame.draw.line(surface, color, (0, y), (self.width, y))

        overlay = pygame.Surface((self.width, self.height), pygame.SRCALPHA)
        for x in range(0, self.width, 64):
            pygame.draw.line(overlay, _alpha((32, 44, 60), 28), (x, 0), (x, self.height), 1)
        for y in range(0, self.height, 64):
            pygame.draw.line(overlay, _alpha((32, 44, 60), 20), (0, y), (self.width, y), 1)
        pygame.draw.circle(overlay, _alpha(PALETTE["accent"], 20), (self.width - 120, 52), 132)
        pygame.draw.circle(overlay, _alpha((72, 96, 138), 12), (self.width - 160, self.height - 80), 176)
        surface.blit(overlay, (0, 0))
        return surface

    def _cell_center(self, index):
        x, y = _xy(index, self.grid_width)
        return (
            self.layout["maze"].x + (x * self.cell_size) + (self.cell_size // 2),
            self.layout["maze"].y + (y * self.cell_size) + (self.cell_size // 2),
        )

    def _cell_rect(self, index):
        x, y = _xy(index, self.grid_width)
        return pygame.Rect(
            self.layout["maze"].x + (x * self.cell_size),
            self.layout["maze"].y + (y * self.cell_size),
            self.cell_size,
            self.cell_size,
        )

    def _maze_metrics(self, maze_cells):
        total = len(maze_cells)
        open_cells = 0
        signature = 0
        branch_cells = 0
        dead_ends = 0
        for idx, value in enumerate(maze_cells):
            wall = 1 if int(value) else 0
            if wall == 0:
                open_cells += 1
            signature = ((signature * 131) + (idx * 17) + wall) % 1000000007

        for idx, value in enumerate(maze_cells):
            if int(value) != 0:
                continue
            x, y = _xy(idx, self.grid_width)
            degree = 0
            if y > 0 and int(maze_cells[idx - self.grid_width]) == 0:
                degree += 1
            if x + 1 < self.grid_width and int(maze_cells[idx + 1]) == 0:
                degree += 1
            if y + 1 < self.grid_height and int(maze_cells[idx + self.grid_width]) == 0:
                degree += 1
            if x > 0 and int(maze_cells[idx - 1]) == 0:
                degree += 1
            if degree <= 1:
                dead_ends += 1
            if degree >= 3:
                branch_cells += 1

        wall_cells = total - open_cells
        return {
            "signature": signature,
            "open_cells": open_cells,
            "wall_cells": wall_cells,
            "branch_cells": branch_cells,
            "dead_ends": dead_ends,
            "open_ratio": open_cells / max(1, total),
        }

    def _maze_shortest_path(self, maze_cells, start_index, target_index):
        if start_index < 0 or start_index >= len(maze_cells):
            return []
        if target_index < 0 or target_index >= len(maze_cells):
            return []
        if int(maze_cells[start_index]) != 0 or int(maze_cells[target_index]) != 0:
            return []

        came_from = [-1] * len(maze_cells)
        queue = deque([start_index])
        came_from[start_index] = start_index

        while queue:
            node = queue.popleft()
            if node == target_index:
                break

            x, y = _xy(node, self.grid_width)
            neighbors = []
            if y > 0:
                neighbors.append(node - self.grid_width)
            if x + 1 < self.grid_width:
                neighbors.append(node + 1)
            if y + 1 < self.grid_height:
                neighbors.append(node + self.grid_width)
            if x > 0:
                neighbors.append(node - 1)

            for next_index in neighbors:
                if int(maze_cells[next_index]) != 0:
                    continue
                if came_from[next_index] != -1:
                    continue
                came_from[next_index] = node
                queue.append(next_index)

        if came_from[target_index] == -1:
            return []

        path = [target_index]
        while path[-1] != start_index:
            path.append(came_from[path[-1]])
        path.reverse()
        return path

    def _maze_route(self, maze_cells, start_index, target_index):
        metrics = self._maze_metrics(maze_cells)
        key = (metrics["signature"], int(start_index), int(target_index))
        if key != self.route_cache_key:
            self.route_cache_key = key
            self.route_cache = self._maze_shortest_path(maze_cells, start_index, target_index)
        metrics["path_length"] = len(self.route_cache)
        return self.route_cache, metrics

    def _maze_seed(self, width, height):
        return ((width * 733) + (height * 977) + ((width * height) * 31) + 19) % 1000000007

    def _maze_step(self, seed):
        return ((seed * 1664525) + 1013904223) % 1000000007

    def _carve_lab_room(self, grid, width, height, origin_x, origin_y, room_w, room_h):
        for y in range(room_h):
            for x in range(room_w):
                px = _clamp(origin_x + x, 0, width - 1)
                py = _clamp(origin_y + y, 0, height - 1)
                grid[(py * width) + px] = 0

    def _carve_lab_spine(self, grid, width, height):
        hub_x = width // 2
        hub_y = height // 2
        spine_x = width - 4
        spine_end_y = height - 2
        for x in range(hub_x, spine_x + 1):
            grid[(hub_y * width) + x] = 0
        for y in range(hub_y, spine_end_y + 1):
            grid[(y * width) + spine_x] = 0

    def _generate_lab_maze(self, width, height):
        total = width * height
        grid = [1] * total
        stack = []
        seed = self._maze_seed(width, height)

        start_index = (1 * width) + 1
        grid[start_index] = 0
        stack.append(start_index)

        while stack:
            current = stack[-1]
            carved = False
            tries = 0
            start_dir = seed % 4
            current_x, current_y = _xy(current, width)

            while tries < 4 and carved is False:
                chosen = (start_dir + tries) % 4
                next_x = current_x
                next_y = current_y
                wall_x = current_x
                wall_y = current_y

                if chosen == 0:
                    next_y = current_y - 2
                    wall_y = current_y - 1
                if chosen == 1:
                    next_x = current_x + 2
                    wall_x = current_x + 1
                if chosen == 2:
                    next_y = current_y + 2
                    wall_y = current_y + 1
                if chosen == 3:
                    next_x = current_x - 2
                    wall_x = current_x - 1

                if next_x > 0 and next_x < width - 1 and next_y > 0 and next_y < height - 1:
                    next_index = (next_y * width) + next_x
                    if grid[next_index] != 0:
                        wall_index = (wall_y * width) + wall_x
                        grid[wall_index] = 0
                        grid[next_index] = 0
                        stack.append(next_index)
                        carved = True
                tries += 1

            if carved is False:
                stack.pop()
            seed = self._maze_step(seed + current + len(stack))

        self._carve_lab_room(grid, width, height, 1, 1, 2, 2)
        self._carve_lab_room(grid, width, height, (width // 2) - 1, (height // 2) - 1, 2, 2)
        self._carve_lab_room(grid, width, height, width - 4, height - 3, 4, 2)
        self._carve_lab_spine(grid, width, height)
        return grid

    def _rounded_panel(self, rect, fill, edge, radius=18):
        shadow = pygame.Surface((rect.width + 12, rect.height + 12), pygame.SRCALPHA)
        pygame.draw.rect(shadow, (0, 0, 0, 85), shadow.get_rect().move(5, 7), border_radius=radius)
        self.surface.blit(shadow, (rect.x - 6, rect.y - 6))
        pygame.draw.rect(self.surface, fill, rect, border_radius=radius)
        pygame.draw.rect(self.surface, edge, rect, width=1, border_radius=radius)

    def _draw_text(self, text, font, color, pos):
        label = font.render(str(text), True, color)
        self.surface.blit(label, pos)
        return label.get_size()

    def _draw_header(self, frame, distance, lane, bias, rat_pos, target_index):
        rect = self.layout["header"]
        self._rounded_panel(rect, PALETTE["panel"], PALETTE["panel_edge"], radius=20)
        distance_label = "pending" if distance < 0 else f"{distance:03d}"

        self._draw_text("CONVERGENCE / PROCEDURAL MAZE LAB", self.title_font, PALETTE["text"], (rect.x + 18, rect.y + 10))
        self._draw_text("instrumented search chamber / lane telemetry / route analysis", self.small_font, PALETTE["text_soft"], (rect.x + 20, rect.y + 44))

        metric_x = rect.right - 360
        metric_y = rect.y + 16
        self._draw_text(f"frame {frame:04d}", self.body_font, PALETTE["text"], (metric_x, metric_y))
        self._draw_text(f"distance {distance_label}", self.body_font, PALETTE["text"], (metric_x + 120, metric_y))
        self._draw_text(f"bias {bias:02d}", self.body_font, PALETTE["text"], (metric_x, metric_y + 28))
        lane_name = "reference" if lane == 0 else ("heuristic" if lane == 1 else "wander")
        self._draw_text(f"winner {lane_name}", self.body_font, PALETTE["text"], (metric_x + 120, metric_y + 28))
        self._draw_text(f"rat {rat_pos}", self.small_font, PALETTE["text_soft"], (rect.right - 110, rect.y + 48))
        self._draw_text(f"target {target_index}", self.small_font, PALETTE["text_soft"], (rect.right - 110, rect.y + 28))

    def _draw_maze_frame(self):
        panel = self.layout["maze_panel"]
        self._rounded_panel(panel, PALETTE["panel_soft"], PALETTE["panel_edge_soft"], radius=22)
        self._draw_text("FIELD ARRAY", self.small_font, PALETTE["text_soft"], (panel.x + 16, panel.y + 10))
        self._draw_text("carved topology / live trace", self.small_font, PALETTE["text_dim"], (panel.x + 16, panel.y + 30))

        inner = self.layout["maze"]
        inset = pygame.Rect(inner.x - 6, inner.y - 6, inner.width + 12, inner.height + 12)
        pygame.draw.rect(self.surface, (6, 9, 14), inset, border_radius=14)
        pygame.draw.rect(self.surface, PALETTE["panel_edge_soft"], inset, width=1, border_radius=14)

    def _draw_maze(self, maze_cells, frame):
        maze_rect = self.layout["maze"]
        self.surface.fill(PALETTE["maze_floor"], maze_rect)

        floor_overlay = pygame.Surface((maze_rect.width, maze_rect.height), pygame.SRCALPHA)
        for y in range(maze_rect.height):
            shade = 5 + ((y // max(1, self.cell_size // 3)) % 2)
            pygame.draw.line(floor_overlay, _alpha((12 + shade, 16 + shade, 22 + shade), 28), (0, y), (maze_rect.width, y), 1)
        for x in range(0, maze_rect.width, self.cell_size):
            pygame.draw.line(floor_overlay, _alpha(PALETTE["maze_grid"], 26), (x, 0), (x, maze_rect.height), 1)
        for y in range(0, maze_rect.height, self.cell_size):
            pygame.draw.line(floor_overlay, _alpha(PALETTE["maze_grid"], 18), (0, y), (maze_rect.width, y), 1)
        self.surface.blit(floor_overlay, maze_rect.topleft)

        for index, value in enumerate(maze_cells):
            rect = self.cell_rects[index]
            x, y = _xy(index, self.grid_width)
            if int(value):
                pygame.draw.rect(self.surface, PALETTE["maze_wall"], rect)
                pygame.draw.line(self.surface, PALETTE["maze_wall_edge"], rect.topleft, rect.topright, 1)
                pygame.draw.line(self.surface, PALETTE["maze_wall_edge"], rect.topleft, rect.bottomleft, 1)
                pygame.draw.line(self.surface, (24, 28, 36), rect.bottomleft, rect.bottomright, 1)
                pygame.draw.line(self.surface, (24, 28, 36), rect.topright, rect.bottomright, 1)
            else:
                is_spine = (
                    (x == self.lab_spine_x and self.lab_spine_y <= y <= self.lab_spine_end_y)
                    or (y == self.lab_spine_y and self.lab_spine_hub_x <= x <= self.lab_spine_x)
                )
                degree = 0
                if y > 0 and int(maze_cells[index - self.grid_width]) == 0:
                    degree += 1
                if x + 1 < self.grid_width and int(maze_cells[index + 1]) == 0:
                    degree += 1
                if y + 1 < self.grid_height and int(maze_cells[index + self.grid_width]) == 0:
                    degree += 1
                if x > 0 and int(maze_cells[index - 1]) == 0:
                    degree += 1

                if is_spine:
                    fill = (16, 38, 54) if y == self.lab_spine_y else (18, 44, 64)
                    pygame.draw.rect(self.surface, fill, rect)
                    pygame.draw.rect(self.surface, (118, 208, 255), rect, width=1)
                    pygame.draw.line(self.surface, (198, 242, 255), rect.midleft, rect.midright, 1)
                    pygame.draw.line(self.surface, (42, 92, 120), rect.topright, rect.bottomright, 1)
                else:
                    if degree >= 3:
                        fill = (18, 24, 34)
                    elif degree == 2:
                        fill = (14, 19, 26)
                    elif degree == 1:
                        fill = (16, 22, 30)
                    else:
                        fill = (12, 16, 22)
                    if ((x + y + frame) % 2) == 0:
                        fill = _blend(fill, (22, 28, 39), 0.18)
                    pygame.draw.rect(self.surface, fill, rect)
                    if degree >= 3:
                        pygame.draw.rect(self.surface, (42, 56, 75), rect, width=1)
                    elif degree == 1:
                        pygame.draw.rect(self.surface, (28, 36, 48), rect, width=1)
                    if (x % self.grid_width) in (0, self.grid_width - 1):
                        pygame.draw.rect(self.surface, (8, 10, 14), rect, width=1)

        sweep = pygame.Surface((maze_rect.width, maze_rect.height), pygame.SRCALPHA)
        sweep_span = max(48, self.cell_size * 3)
        sweep_phase = ((frame * 9) + (maze_rect.width // 3)) % (maze_rect.width + sweep_span + 80)
        sweep_x = sweep_phase - (sweep_span // 2) - 40
        pygame.draw.rect(sweep, _alpha((88, 174, 255), 18), (sweep_x, 0, sweep_span, maze_rect.height))
        pygame.draw.rect(sweep, _alpha((245, 248, 255), 8), (sweep_x + (sweep_span // 3), 0, sweep_span // 3, maze_rect.height))
        self.surface.blit(sweep, maze_rect.topleft)

    def _draw_maze_rulers(self):
        maze = self.layout["maze"]
        ruler_color = _alpha(PALETTE["panel_edge_soft"], 190)
        label_color = PALETTE["text_dim"]

        for x in range(0, self.grid_width, 4):
            px = maze.x + (x * self.cell_size) + (self.cell_size // 2)
            pygame.draw.line(self.surface, ruler_color, (px, maze.y - 8), (px, maze.y - 1), 1)
            self._draw_text(f"{x:02d}", self.small_font, label_color, (px - 9, maze.y - 26))

        for y in range(0, self.grid_height, 3):
            py = maze.y + (y * self.cell_size) + (self.cell_size // 2)
            pygame.draw.line(self.surface, ruler_color, (maze.x - 8, py), (maze.x - 1, py), 1)
            self._draw_text(f"{y:02d}", self.small_font, label_color, (maze.x - 32, py - 8))

    def _draw_route_trace(self, route, frame):
        if len(route) < 2:
            return

        points = [self.grid_points[int(index)] for index in route if 0 <= int(index) < len(self.grid_points)]
        if len(points) < 2:
            return

        overlay = pygame.Surface((self.width, self.height), pygame.SRCALPHA)
        pygame.draw.lines(overlay, _alpha((208, 220, 234), 62), False, points, 11)
        pygame.draw.lines(overlay, _alpha(PALETTE["accent"], 170), False, points, 4)
        pygame.draw.aalines(overlay, _alpha((255, 255, 255), 90), False, points)
        pulse_point = points[int(frame) % len(points)]
        pygame.draw.circle(overlay, _alpha((255, 255, 255), 180), pulse_point, 5)
        pygame.draw.circle(overlay, _alpha(PALETTE["target"], 170), pulse_point, 12, 2)
        self.surface.blit(overlay, (0, 0))

    def _trail_points(self, trail):
        points = []
        for entry in trail:
            index = int(entry)
            if index < 0:
                continue
            if index >= len(self.grid_points):
                continue
            points.append(self.grid_points[index])
        return points

    def _route_points(self, start_index, target_index):
        start_x, start_y = _xy(int(start_index), self.grid_width)
        target_x, target_y = _xy(int(target_index), self.grid_width)
        points = [self.grid_points[int(start_index)]]

        step_x = 1 if target_x >= start_x else -1
        x = start_x
        while x != target_x:
            x += step_x
            index = (start_y * self.grid_width) + x
            points.append(self.grid_points[index])

        step_y = 1 if target_y >= start_y else -1
        y = start_y
        while y != target_y:
            y += step_y
            index = (y * self.grid_width) + target_x
            points.append(self.grid_points[index])

        return points

    def _draw_route_skeleton(self, start_index, target_index):
        points = self._route_points(start_index, target_index)
        if len(points) < 2:
            return
        overlay = pygame.Surface((self.width, self.height), pygame.SRCALPHA)
        for idx in range(0, len(points) - 1, 2):
            pygame.draw.line(overlay, _alpha((132, 142, 158), 70), points[idx], points[idx + 1], 3)
        pygame.draw.aalines(overlay, _alpha((190, 202, 218), 120), False, points)
        for idx, point in enumerate(points[::max(1, len(points) // 10)]):
            pygame.draw.circle(overlay, _alpha((255, 232, 160), 140), point, 2 + (idx % 2))
        self.surface.blit(overlay, (0, 0))

    def _draw_trail(self, trail, color, glow_alpha=68, body_alpha=205):
        points = self._trail_points(trail)
        if len(points) == 1:
            point = points[0]
            halo = pygame.Surface((self.width, self.height), pygame.SRCALPHA)
            pygame.draw.circle(halo, _alpha(color, 90), point, 10)
            pygame.draw.circle(halo, _alpha((255, 255, 255), 180), point, 4)
            pygame.draw.circle(self.surface, color, point, 3)
            self.surface.blit(halo, (0, 0))
            return
        if len(points) < 2:
            return
        glow = pygame.Surface((self.width, self.height), pygame.SRCALPHA)
        pygame.draw.lines(glow, _alpha(color, glow_alpha), False, points, 11)
        pygame.draw.lines(glow, _alpha(color, body_alpha), False, points, 4)
        pygame.draw.aalines(glow, _alpha((245, 248, 255), 64), False, points)
        self.surface.blit(glow, (0, 0))

    def _draw_marker(self, index, color, kind, frame):
        if index < 0 or index >= len(self.grid_points):
            return
        x, y = self.grid_points[index]
        pulse = 2 + int((math.sin(frame / 8.0) + 1.0) * 1.25)

        halo = pygame.Surface((self.width, self.height), pygame.SRCALPHA)
        if kind == "rat":
            pygame.draw.circle(halo, _alpha(color, 40), (x, y), pulse + 8)
            pygame.draw.circle(halo, _alpha(color, 130), (x, y), pulse + 2)
            pygame.draw.circle(self.surface, color, (x, y), 4)
            pygame.draw.circle(self.surface, PALETTE["accent"], (x, y), pulse + 5, 1)
            pygame.draw.line(self.surface, PALETTE["text"], (x - 7, y), (x - 3, y), 1)
            pygame.draw.line(self.surface, PALETTE["text"], (x + 3, y), (x + 7, y), 1)
            pygame.draw.line(self.surface, PALETTE["text"], (x, y - 7), (x, y - 3), 1)
            pygame.draw.line(self.surface, PALETTE["text"], (x, y + 3), (x, y + 7), 1)
        elif kind == "start":
            pygame.draw.circle(halo, _alpha(color, 48), (x, y), pulse + 7)
            pygame.draw.rect(self.surface, color, (x - 6, y - 6, 12, 12), width=2, border_radius=2)
            pygame.draw.rect(self.surface, (255, 255, 255), (x - 8, y - 8, 16, 16), width=1, border_radius=3)
        elif kind == "target":
            pygame.draw.circle(halo, _alpha(color, 62), (x, y), pulse + 10)
            pygame.draw.circle(self.surface, color, (x, y), 11, 2)
            pygame.draw.circle(self.surface, color, (x, y), 3)
            offset = 15
            pygame.draw.line(self.surface, color, (x - offset, y), (x - 7, y), 2)
            pygame.draw.line(self.surface, color, (x + 7, y), (x + offset, y), 2)
            pygame.draw.line(self.surface, color, (x, y - offset), (x, y - 7), 2)
            pygame.draw.line(self.surface, color, (x, y + 7), (x, y + offset), 2)
        self.surface.blit(halo, (0, 0))

    def _draw_sidebar_card(self, rect, title, subtitle=None):
        self._rounded_panel(rect, PALETTE["panel"], PALETTE["panel_edge_soft"], radius=18)
        self._draw_text(title, self.body_font, PALETTE["text"], (rect.x + 16, rect.y + 12))
        if subtitle is not None:
            self._draw_text(subtitle, self.small_font, PALETTE["text_soft"], (rect.x + 16, rect.y + 40))

    def _draw_lane_row(self, rect, label, color, count, active=False):
        track = pygame.Rect(rect.x + 16, rect.y + 42, rect.width - 32, 16)
        pygame.draw.rect(self.surface, (20, 26, 36), track, border_radius=8)
        max_width = track.width
        filled = _clamp(int((count / 32.0) * max_width), 12, max_width)
        pygame.draw.rect(self.surface, color, (track.x, track.y, filled, track.height), border_radius=8)
        if active:
            pygame.draw.rect(self.surface, (245, 248, 255), track, width=1, border_radius=8)
        self._draw_text(f"{label}  {count:03d}", self.small_font, PALETTE["text"], (rect.x + 16, rect.y + 16))

    def _draw_sidebar(self, telemetry, target_index, rat_pos, metrics):
        sidebar = self.layout["sidebar"]
        self._rounded_panel(sidebar, PALETTE["panel_soft"], PALETTE["panel_edge_soft"], radius=22)

        self._draw_text("INSTRUMENT BAY", self.body_font, PALETTE["text"], (sidebar.x + 16, sidebar.y + 14))
        self._draw_text("topology / lane pressure / scanline", self.small_font, PALETTE["text_soft"], (sidebar.x + 16, sidebar.y + 42))

        summary = pygame.Rect(sidebar.x + 14, sidebar.y + 72, sidebar.width - 28, 60)
        self._draw_sidebar_card(summary, "TOPOLOGY")
        self._draw_text(f"sig {metrics['signature']}  |  route {metrics['path_length']}", self.small_font, PALETTE["text_soft"], (summary.x + 16, summary.y + 28))
        self._draw_text(f"open {metrics['open_cells']:03d} / {metrics['wall_cells']:03d}", self.small_font, PALETTE["text"], (summary.x + 16, summary.y + 46))
        self._draw_text(f"branches {metrics['branch_cells']:03d}  dead {metrics['dead_ends']:03d}", self.small_font, PALETTE["text_soft"], (summary.x + 176, summary.y + 46))

        lane_cards = [
            (pygame.Rect(sidebar.x + 14, sidebar.y + 132, sidebar.width - 28, 64), "REFERENCE", PALETTE["pure"], telemetry.pure_count, telemetry.best_lane == 0),
            (pygame.Rect(sidebar.x + 14, sidebar.y + 204, sidebar.width - 28, 64), "GREEDY", PALETTE["greedy"], telemetry.greedy_count, telemetry.best_lane == 1),
            (pygame.Rect(sidebar.x + 14, sidebar.y + 276, sidebar.width - 28, 64), "CHAOS", PALETTE["chaos"], telemetry.chaos_count, telemetry.best_lane == 2),
        ]
        for rect, label, color, count, active in lane_cards:
            self._draw_lane_row(rect, label, color, count, active)

        stats = pygame.Rect(sidebar.x + 14, sidebar.y + 348, sidebar.width - 28, 88)
        self._draw_sidebar_card(stats, "RUN METRICS", "deterministic enough to audit")
        self._draw_text(f"target {target_index}", self.small_font, PALETTE["text"], (stats.x + 16, stats.y + 28))
        self._draw_text(f"rat {rat_pos}", self.small_font, PALETTE["text"], (stats.x + 16, stats.y + 48))
        self._draw_text(f"best distance {telemetry.best_distance}", self.small_font, PALETTE["text"], (stats.x + 16, stats.y + 68))

        trace = pygame.Rect(sidebar.x + 14, sidebar.bottom - 138, sidebar.width - 28, 124)
        self._draw_sidebar_card(trace, "SIGNAL TRACE", "recent frames and lane bias")
        self._draw_history_graph(trace.inflate(-18, -46))

    def _draw_history_graph(self, rect):
        if len(self.history) < 2:
            pygame.draw.rect(self.surface, (12, 16, 22), rect, border_radius=12)
            pygame.draw.rect(self.surface, (42, 56, 75), rect, width=1, border_radius=12)
            for tick in range(0, 4):
                ty = rect.bottom - 10 - (tick * ((rect.height - 18) // 3))
                pygame.draw.line(self.surface, (33, 43, 57), (rect.x + 6, ty), (rect.right - 6, ty), 1)
            for tick in range(0, 5):
                tx = rect.x + 8 + int(tick * ((rect.width - 16) / 4))
                pygame.draw.line(self.surface, (33, 43, 57), (tx, rect.y + 6), (tx, rect.bottom - 8), 1)
            self._draw_text("arming / awaiting samples", self.small_font, PALETTE["text_dim"], (rect.x + 12, rect.centery - 8))
            return

        pygame.draw.rect(self.surface, (12, 16, 22), rect, border_radius=12)
        pygame.draw.rect(self.surface, (42, 56, 75), rect, width=1, border_radius=12)

        frames = list(self.history)
        max_distance = max(1, max(item["distance"] for item in frames))
        points = []
        lane_points = []
        for idx, item in enumerate(frames):
            t = idx / max(1, len(frames) - 1)
            x = rect.x + 8 + int(t * (rect.width - 16))
            y = rect.bottom - 8 - int((item["distance"] / max_distance) * (rect.height - 16))
            points.append((x, y))
            lane_y = rect.bottom - 10
            lane_bar_h = 8 + (item["lane"] * 3)
            lane_points.append((x, lane_y - lane_bar_h, item["lane"]))

        pygame.draw.lines(self.surface, _alpha(PALETTE["spark"], 180), False, points, 2)
        pygame.draw.aalines(self.surface, _alpha((255, 255, 255), 110), False, points)
        for tick in range(0, 4):
            ty = rect.bottom - 10 - (tick * ((rect.height - 18) // 3))
            pygame.draw.line(self.surface, (33, 43, 57), (rect.x + 6, ty), (rect.right - 6, ty), 1)

        for x, lane_y, lane in lane_points[::2]:
            color = PALETTE["pure"] if lane == 0 else (PALETTE["greedy"] if lane == 1 else PALETTE["chaos"])
            pygame.draw.line(self.surface, color, (x, rect.bottom - 8), (x, lane_y), 2)

    def _draw_footer(self, telemetry, frame_signature):
        rect = self.layout["footer"]
        self._rounded_panel(rect, PALETTE["panel"], PALETTE["panel_edge"], radius=20)
        audit_label = "pending" if frame_signature < 0 else str(frame_signature)
        self._draw_text("TELEMETRY", self.body_font, PALETTE["text"], (rect.x + 18, rect.y + 12))
        self._draw_text(f"frame signature {telemetry.frame_signature}  |  audit {audit_label}", self.small_font, PALETTE["text_soft"], (rect.x + 18, rect.y + 40))
        self._draw_text(f"maze {telemetry.width}x{telemetry.height}  |  trails {telemetry.trail_capacity}", self.small_font, PALETTE["text_dim"], (rect.x + 18, rect.y + 64))
        self._draw_text("escape: close window or press ESC", self.small_font, PALETTE["text_dim"], (rect.right - 240, rect.y + 12))

        strip = pygame.Rect(rect.right - 240, rect.y + 16, 220, 64)
        pygame.draw.rect(self.surface, (14, 19, 27), strip, border_radius=14)
        pygame.draw.rect(self.surface, (42, 56, 75), strip, width=1, border_radius=14)
        self._draw_text("lane mix", self.small_font, PALETTE["text_soft"], (strip.x + 12, strip.y + 10))
        self._draw_text(
            f"{telemetry.pure_count:02d} / {telemetry.greedy_count:02d} / {telemetry.chaos_count:02d}",
            self.body_font,
            PALETTE["text"],
            (strip.x + 12, strip.y + 30),
        )

    def pump(self):
        events = pygame.event.get()
        for event in events:
            if event.type == pygame.QUIT:
                self.closed = True
            if event.type == pygame.KEYDOWN and event.key == pygame.K_ESCAPE:
                self.closed = True
        return 0 if self.closed else 1

    def draw_frame(
        self,
        maze_cells,
        pure_trail,
        greedy_trail,
        chaos_trail,
        start_index,
        target_index,
        distance,
        lane,
        frame,
        rat_pos,
        oracle_bias,
    ):
        self.clock.tick(60)
        self.frames += 1
        maze_cells = self.lab_maze

        self.surface.blit(self.background, (0, 0))
        self._draw_header(frame, distance, lane, oracle_bias, rat_pos, target_index)
        self._draw_maze_frame()
        self._draw_maze(maze_cells, frame)
        self._draw_maze_rulers()

        route, metrics = self._maze_route(maze_cells, start_index, target_index)
        self._draw_route_trace(route, frame)

        self._draw_trail(pure_trail, PALETTE["pure"], glow_alpha=40, body_alpha=170)
        self._draw_trail(greedy_trail, PALETTE["greedy"], glow_alpha=36, body_alpha=160)
        self._draw_trail(chaos_trail, PALETTE["chaos"], glow_alpha=36, body_alpha=160)

        self._draw_marker(start_index, PALETTE["start"], "start", frame)
        self._draw_marker(target_index, PALETTE["target"], "target", frame)
        self._draw_marker(rat_pos, PALETTE["rat"], "rat", frame)

        self.history.append(
            {
                "frame": int(frame),
                "distance": int(distance),
                "lane": int(lane),
                "bias": int(oracle_bias),
                "rat": int(rat_pos),
            }
        )

        signature = (
            (frame * 31)
            + (distance * 17)
            + (len(pure_trail) * 13)
            + (len(greedy_trail) * 11)
            + (len(chaos_trail) * 7)
            + rat_pos
            + oracle_bias
        ) % 1000000007

        self._draw_sidebar(
            type("TelemetryMirror", (), {
                "pure_count": len(pure_trail),
                "greedy_count": len(greedy_trail),
                "chaos_count": len(chaos_trail),
                "best_lane": int(lane),
                "best_distance": int(distance),
                "width": self.grid_width,
                "height": self.grid_height,
                "trail_capacity": len(pure_trail),
                "frame_signature": int(signature),
                "route_length": len(route),
            })(),
            target_index,
            rat_pos,
            metrics,
        )
        self._draw_footer(
            type("TelemetryMirror", (), {
                "frame_signature": int(signature),
                "width": self.grid_width,
                "height": self.grid_height,
                "trail_capacity": len(pure_trail),
                "pure_count": len(pure_trail),
                "greedy_count": len(greedy_trail),
                "chaos_count": len(chaos_trail),
            })(),
            int(distance),
        )

        pygame.display.flip()

        if self.capture_path and not self.captured:
            pygame.image.save(self.surface, self.capture_path)
            self.captured = True

        return signature

    def close(self):
        self.closed = True
        pygame.quit()
        return 0


def launch(width, height, cell_size, title):
    return ConvergenceWindow(width, height, cell_size, title)


def ping():
    return 1
