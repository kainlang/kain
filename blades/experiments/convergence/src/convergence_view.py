import pygame

PALETTE = {
    "bg": (8, 11, 18),
    "maze_open": (14, 19, 29),
    "maze_wall": (24, 31, 42),
    "pure": (70, 150, 255),
    "greedy": (255, 103, 92),
    "chaos": (92, 230, 152),
    "start": (255, 228, 105),
    "target": (255, 169, 48),
    "rat": (255, 255, 255),
    "hud": (234, 238, 246),
}


def _xy(index, width):
    return index % width, index // width


class ConvergenceWindow:
    def __init__(self, width, height, cell_size, title):
        pygame.init()
        pygame.font.init()
        self.grid_width = int(width)
        self.grid_height = int(height)
        self.cell_size = int(cell_size)
        self.width = self.grid_width * self.cell_size
        self.height = (self.grid_height * self.cell_size) + 72
        self.surface = pygame.display.set_mode((self.width, self.height))
        pygame.display.set_caption(str(title))
        self.clock = pygame.time.Clock()
        self.font = pygame.font.SysFont("Consolas", 18) or pygame.font.Font(None, 18)
        self.closed = False
        self.frames = 0

    def pump(self):
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                self.closed = True
        return 0 if self.closed else 1

    def _draw_trail(self, trail, color):
        for index in trail:
            x, y = _xy(int(index), self.grid_width)
            rect = pygame.Rect(
                x * self.cell_size + 4,
                y * self.cell_size + 4,
                self.cell_size - 8,
                self.cell_size - 8,
            )
            pygame.draw.rect(self.surface, color, rect, border_radius=4)

    def _draw_marker(self, index, color):
        x, y = _xy(int(index), self.grid_width)
        rect = pygame.Rect(
            x * self.cell_size + 1,
            y * self.cell_size + 1,
            self.cell_size - 2,
            self.cell_size - 2,
        )
        pygame.draw.rect(self.surface, color, rect, border_radius=6)

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
        self.surface.fill(PALETTE["bg"])

        for index, value in enumerate(maze_cells):
            x, y = _xy(index, self.grid_width)
            rect = pygame.Rect(
                x * self.cell_size,
                y * self.cell_size,
                self.cell_size,
                self.cell_size,
            )
            color = PALETTE["maze_wall"] if int(value) else PALETTE["maze_open"]
            pygame.draw.rect(self.surface, color, rect)

        self._draw_trail(pure_trail, PALETTE["pure"])
        self._draw_trail(greedy_trail, PALETTE["greedy"])
        self._draw_trail(chaos_trail, PALETTE["chaos"])
        self._draw_marker(start_index, PALETTE["start"])
        self._draw_marker(target_index, PALETTE["target"])
        self._draw_marker(rat_pos, PALETTE["rat"])

        hud = f"frame={frame} lane={lane} dist={distance} bias={oracle_bias} rat={rat_pos}"
        label = self.font.render(hud, True, PALETTE["hud"])
        self.surface.blit(label, (12, self.grid_height * self.cell_size + 16))

        lane_name = "pure" if lane == 0 else ("greedy" if lane == 1 else "chaos")
        label2 = self.font.render(f"winner={lane_name}", True, PALETTE["hud"])
        self.surface.blit(label2, (12, self.grid_height * self.cell_size + 38))

        pygame.display.flip()
        signature = (
            (frame * 31)
            + (distance * 17)
            + (len(pure_trail) * 13)
            + (len(greedy_trail) * 11)
            + (len(chaos_trail) * 7)
            + rat_pos
            + oracle_bias
        ) % 1000000007
        return signature

    def close(self):
        self.closed = True
        pygame.quit()
        return 0


def launch(width, height, cell_size, title):
    return ConvergenceWindow(width, height, cell_size, title)


def ping():
    return 1
