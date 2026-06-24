# GameOfLife

Conway's Game of Life cellular automaton running inside a markscript-powered TUI widget. The grid renders as ASCII art (filled and empty circles) and evolves at a configurable tick rate. Proves the plugin system can do more than text – it can simulate, animate, and render to the terminal.

## Metadata
| Property | Value |
|----------|-------|
| Name | game_of_life |
| Version | 2.0.0 |
| Description | Conway's Game of Life in the TUI sidebar |
| Tools | gol_tick, gol_toggle, gol_reset, gol_randomize |
| Widgets | 1 |
| GridWidth | 20 |
| GridHeight | 12 |

## Commands
| Command | Description | Usage |
|---------|-------------|-------|
| gol | Show Game of Life controls | /gol [tick|toggle|reset|random|help] |
| gol-tick | Advance the simulation one generation | /gol-tick |
| gol-toggle | Start/stop auto-advance | /gol-toggle |
| gol-reset | Reset to a clean grid | /gol-reset |
| gol-random | Randomize the grid | /gol-random |

## Tools
| Name | Description | Handler |
|------|-------------|---------|
| gol_tick | Advance one generation of the Game of Life | 201 |
| gol_toggle | Toggle cell at (x, y) | 202 |
| gol_reset | Clear the grid to all dead cells | 203 |
| gol_randomize | Fill the grid with random live cells | 204 |

## Widgets
| Widget | Type | Width | Update | Refresh Action |
|--------|------|-------|--------|----------------|
| game_of_life | custom | 30 | 500 | tick game of life |

## Grid
| Row | Cells | Live | Generation |
|-----|-------|------|------------|
| 0 | ░ █ ░ ░ █ ░ ░ ░ █ ░ ░ █ ░ ░ ░ █ ░ ░ █ ░ | 7 | 0 |
| 1 | ░ █ ░ ░ █ ░ ░ ░ █ ░ ░ █ ░ ░ ░ █ ░ ░ █ ░ | 7 | 0 |
| 2 | ░ ░ █ ░ ░ ░ █ ░ ░ ░ ░ ░ █ ░ ░ ░ █ ░ ░ ░ | 5 | 0 |
| 3 | █ ░ ░ ░ █ ░ ░ █ ░ ░ █ ░ ░ █ ░ ░ █ ░ ░ █ | 7 | 0 |
| 4 | ░ █ ░ █ ░ ░ ░ ░ █ ░ ░ ░ ░ █ ░ █ ░ ░ ░ ░ | 5 | 0 |
| 5 | ░ ░ ░ ░ ░ █ █ ░ ░ ░ ░ ░ █ █ ░ ░ ░ ░ ░ ░ | 4 | 0 |
| 6 | ░ ░ █ ░ ░ ░ ░ ░ ░ █ █ ░ ░ ░ ░ ░ █ ░ ░ ░ | 4 | 0 |
| 7 | █ ░ ░ ░ █ ░ ░ █ ░ ░ ░ ░ █ ░ ░ █ ░ ░ ░ █ | 6 | 0 |
| 8 | ░ ░ █ ░ ░ ░ █ ░ ░ ░ ░ █ ░ ░ ░ █ ░ ░ ░ ░ | 4 | 0 |
| 9 | ░ █ ░ ░ █ ░ ░ ░ █ ░ ░ █ ░ ░ ░ █ ░ ░ █ ░ | 7 | 0 |
| 10 | ░ ░ ░ █ ░ ░ █ ░ ░ ░ ░ ░ █ ░ ░ █ ░ ░ ░ ░ | 4 | 0 |
| 11 | █ ░ ░ ░ █ ░ ░ █ ░ ░ █ ░ ░ █ ░ ░ █ ░ ░ █ | 7 | 0 |

## Controls
| Key | Action | Description |
|-----|--------|-------------|
| Space | Toggle | Start/stop simulation |
| R | Reset | Clear grid |
| G | Randomize | Random fill (50%) |
| Arrow keys | Move cursor | Select cell to toggle |
| Enter | Toggle cell | Flip cell at cursor position |

## Patterns
| Name | Description | Period | Popularity |
|------|-------------|--------|------------|
| Block | 2x2 stable block | Static | Most common still life |
| Blinker | 3-cell oscillating line | 2 | Simplest oscillator |
| Glider | 5-cell moving pattern | 4 | Most famous spaceship |
| Pulsar | 48-cell period-3 oscillator | 3 | Classic large oscillator |
| Beacon | 8-cell period-2 oscillator | 2 | Common in random soups |

## Handler

> tick game of life

The Game of Life handler is triggered when the widget's refresh action dispatches. It runs a cellular automaton tick that applies Conway's rules to each cell:

```kain
// Conway's Game of Life ~ full simulation kernel
// Rules:
//   1. Any live cell with <2 live neighbors dies (underpopulation)
//   2. Any live cell with 2-3 live neighbors survives
//   3. Any live cell with >3 live neighbors dies (overpopulation)
//   4. Any dead cell with exactly 3 live neighbors becomes alive (reproduction)

const GOL_WIDTH:  Int = 20
const GOL_HEIGHT: Int = 12

struct GolGrid:
    cells:   [Int]      // flat array, row-major: cells[row * WIDTH + col]
    live:    Int         // current live cell count
    gen:     Int         // generation counter
    running: Bool        // auto-advance enabled

fn gol_create(w: Int, h: Int) -> GolGrid:
    var cells: [Int] = []
    var total = w * h
    var i: Int = 0
    while i < total:
        push(cells, 0)
        i = i + 1
    return GolGrid { cells: cells, live: 0, gen: 0, running: false }

fn gol_randomize(grid: GolGrid, w: Int, h: Int, seed: Int) -> GolGrid:
    var g = grid
    var total = w * h
    var i: Int = 0
    var live_count: Int = 0
    let salt = seed
    while i < total:
        // Simple pseudo-random: multiply and mod
        let r = (i * 1103515245 + 12345 + salt) & 32767
        let val = if r < 16384: 1 else: 0
        g.cells[i] = val
        live_count = live_count + val
        i = i + 1
    g.live = live_count
    g.gen = 0
    return g

fn gol_tick(grid: GolGrid, w: Int, h: Int) -> GolGrid:
    var g = grid
    var new_cells: [Int] = []
    var total = w * h
    var i: Int = 0
    var live_count: Int = 0
    while i < total:
        let row: Int = i / w
        let col: Int = i % w
        let neighbors = gol_count_neighbors(g.cells, row, col, w, h)
        let old = g.cells[i]
        var new_val: Int = 0
        if old == 1:
            if neighbors < 2: new_val = 0       // underpopulation
            elif neighbors <= 3: new_val = 1     // survival
            else: new_val = 0                    // overpopulation
        else:
            if neighbors == 3: new_val = 1       // reproduction
            else: new_val = 0
        push(new_cells, new_val)
        live_count = live_count + new_val
        i = i + 1
    g.cells = new_cells
    g.live = live_count
    g.gen = g.gen + 1
    return g

fn gol_count_neighbors(cells: [Int], row: Int, col: Int, w: Int, h: Int) -> Int:
    var count: Int = 0
    var dr: Int = -1
    while dr <= 1:
        var dc: Int = -1
        while dc <= 1:
            if dr == 0 and dc == 0:
                dc = dc + 1
                continue
            let nr = row + dr
            let nc = col + dc
            if nr >= 0 and nr < h and nc >= 0 and nc < w:
                let idx = nr * w + nc
                if idx >= 0 and idx < len(cells):
                    count = count + cells[idx]
            dc = dc + 1
        dr = dr + 1
    return count

fn gol_render(grid: GolGrid, w: Int, h: Int) -> [String]:
    var lines: [String] = []
    push(lines, "Conway's Game of Life")
    push(lines, "Gen: " + str(grid.gen) + "  Live: " + str(grid.live) + "  " + if grid.running: "RUN" else: "STOP")
    var r: Int = 0
    while r < h:
        var row_str: String = ""
        var c: Int = 0
        while c < w:
            let idx2 = r * w + c
            if idx2 >= 0 and idx2 < len(grid.cells):
                if grid.cells[idx2] == 1:
                    row_str = row_str + "█ "
                else:
                    row_str = row_str + "░ "
            c = c + 1
        push(lines, row_str)
        r = r + 1
    return lines

fn gol_toggle_cell(grid: GolGrid, x: Int, y: Int, w: Int, h: Int) -> GolGrid:
    var g = grid
    if x < 0 or x >= w or y < 0 or y >= h: return g
    let idx3 = y * w + x
    if idx3 >= 0 and idx3 < len(g.cells):
        g.cells[idx3] = if g.cells[idx3] == 1: 0 else: 1
    return g

fn gol_reset(grid: GolGrid, w: Int, h: Int) -> GolGrid:
    var g = grid
    var total2 = w * h
    var i2: Int = 0
    while i2 < total2:
        g.cells[i2] = 0
        i2 = i2 + 1
    g.live = 0
    g.gen = 0
    g.running = false
    return g

// Test patterns
fn gol_load_glider(grid: GolGrid, w: Int, h: Int) -> GolGrid:
    var g = gol_reset(grid, w, h)
    // Glider: classic 5-cell pattern that moves diagonally
    //   ░ █ ░
    //   ░ ░ █
    //   █ █ █
    let glider_positions = [
        (1, 0), (2, 1), (0, 2), (1, 2), (2, 2)
    ]
    var pi: Int = 0
    while pi < 5:
        let (gx, gy) = glider_positions[pi]
        g.cells[gy * w + gx] = 1
        pi = pi + 1
    g.live = 5
    g.gen = 0
    return g

fn gol_load_blinker(grid: GolGrid, w: Int, h: Int) -> GolGrid:
    var g = gol_reset(grid, w, h)
    // Blinker: 3-cell vertical line that becomes horizontal
    let cx = 10
    let cy = 6
    g.cells[(cy-1) * w + cx] = 1
    g.cells[cy * w + cx] = 1
    g.cells[(cy+1) * w + cx] = 1
    g.live = 3
    g.gen = 0
    return g

fn gol_load_block(grid: GolGrid, w: Int, h: Int) -> GolGrid:
    var g = gol_reset(grid, w, h)
    // Block: 2x2 stable block
    let bx = 9
    let by = 5
    g.cells[by * w + bx] = 1
    g.cells[by * w + bx + 1] = 1
    g.cells[(by+1) * w + bx] = 1
    g.cells[(by+1) * w + bx + 1] = 1
    g.live = 4
    g.gen = 0
    return g

// Main entry: run N ticks and return rendered output
fn gol_run(ticks: Int, w: Int, h: Int, seed: Int) -> [String]:
    var grid = gol_create(w, h)
    grid = gol_randomize(grid, w, h, seed)
    var t: Int = 0
    while t < ticks:
        grid = gol_tick(grid, w, h)
        t = t + 1
    return gol_render(grid, w, h)
```

## Presets
| Name | Cells | Period | Description |
|------|-------|--------|-------------|
| Random | 50% | varies | Random initial state |
| Glider | 5 | ∞ (moving) | Diagonal-moving spaceship |
| Blinker | 3 | 2 | Vertical↔Horizontal oscillation |
| Block | 4 | Static | 2x2 stable block |
| Pulsar | 48 | 3 | Classic period-3 oscillator |
| Gosper | 36 | ∞ (moving) | Glider gun >> infinite stream |

## Glider Sequence
Shows a glider moving diagonally across the grid over 4 generations.

| Gen | Row 0 | Row 1 | Row 2 | Row 3 | Row 4 |
|-----|-------|-------|-------|-------|-------|
| 0 | ░██░ | ░░██ | ███░ | ░░░░ | ░░░░ |
| 1 | ░██░ | █░██ | ░██░ | ░██░ | ░░░░ |
| 2 | ░░██ | ░███ | ███░ | ░██░ | ░░░░ |
| 3 | ██░░ | ░███ | ░██░ | ░██░ | ░█░░ |
| 4 | ██░░ | ░██░ | ░██░ | ░██░ | ░██░ |

## Test Cases
| Test | Input | Expected | Description |
|------|-------|----------|-------------|
| Underpopulation | Block with 1 neighbor | Cell dies | Live cell with 0-1 neighbors dies |
| Survival | Block 2x2 | All 4 survive | Live cell with 2-3 neighbors survives |
| Overpopulation | 4-cell cluster | Corner dies | Live cell with 4+ neighbors dies |
| Reproduction | 3 dead neighbors | Cell becomes alive | Dead cell with exactly 3 neighbors becomes alive |
| Blinker stability | Vertical line | Horizontal line | Period-2 oscillation confirmed |

> tick game of life
