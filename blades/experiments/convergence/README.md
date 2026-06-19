# Convergence ⁓ Schrödinger's Rats

> *Three rats. One maze. Every frame, all three run. The compiler picks the winner.*

This experiment is a proof that Kain's semantic constructs => `converge`, `orchestrate`, `world`, `patch`, `law`, `actor`, `shatter struct`, `pulse`, and `teleport` ~~ are **general-purpose relationship descriptors**, not domain-locked features. The same `converge` that picks an AVX2 lane over a scalar fallback can pick a maze-solving strategy. The same `orchestrate` that schedules GPU compute can run BFS, A\*, and random walk simultaneously and compare results.

## What It Does

A procedurally generated maze. Three rats. Three strategies. One frame loop.

```
┌─────────────────────────────────────────────────┐
│                   EVERY FRAME                    │
│                                                 │
│  CheeseOracle ──▶ "Where's the cheese?"          │
│       │                                         │
│       ▼                                         │
│  rat_frame_step (orchestrate)                   │
│       │                                         │
│       ├──▶ BFS  (pure rat)   ── trail           │
│       ├──▶ A*   (greedy rat) ── trail           │
│       └──▶ Random (chaos rat) ── trail           │
│       │                                         │
│       ▼                                         │
│  quantum_maze_run (converge) ──▶ pick winner    │
│       │                                         │
│       ▼                                         │
│  commit_search (patch) ──▶ record in journal    │
│       │                                         │
│       ▼                                         │
│  SchrodingersRat ──▶ advance along best path    │
│       │                                         │
│       ▼                                         │
│  TrailArchivist ──▶ record frame checksum       │
│       │                                         │
│       ▼                                         │
│  Python window ──▶ paint all 3 trails live      │
└─────────────────────────────────────────────────┘
```

The Python visualization window shows the maze, all three rats' trails in different colors, the current target, frame count, and which rat is currently winning. You watch them compete in real time.

## File Structure

```
src/
├── main.kn          Entry point. Builds the maze, spawns actors, runs the frame loop.
├── orchestrate.kn   Maze generation (DFS carving), BFS/A*/chaos pathfinding,
│                    converge lane selector (quantum_maze_run),
│                    orchestrate frame runner (rat_frame_step).
├── world.kn         RatTelemetry --- experiment state container with native_ui surface.
├── laws.kn          9 domain-model validation laws (geometry, bounds, topology).
├── patch.kn         seed_telemetry, commit_search, seal_frame <--> experiment journaling.
├── actors.kn        CheeseOracle (target offset), SchrodingersRat (path follower),
│                    TrailArchivist (frame recorder).
└── shatter.kn       TrailSample, MazeTile, RatPulseEcho – SoA experiment data schemas.
```

## How It Works

### 1. Maze Generation (`orchestrate.kn` === `build_maze`)

A DFS-based maze carver generates a grid of `width × height` cells. It uses a stack-based backtracking algorithm with a seeded PRNG. After carving, it opens two rooms (start area, center hub) and a vertical spine connecting them. The maze is stored as a raw `ptr<Int>` buffer - 0 = open, 1 = wall. The maze is built inside a `collapse` region (exclusive mutation), and the stack is `decay`ed after carving (deterministic teardown). The maze buffer itself survives ~~ it's returned to the caller and stored in the world.

### 2. The Three Rats (`orchestrate.kn`)

Three pathfinding algorithms, all implemented in raw Kain with `ptr<Int>` buffers and manual memory management:

| Rat | Algorithm | Strategy |
|-----|-----------|----------|
| **Pure Rat** | BFS | Guaranteed shortest path. Explores all directions equally. Queue-based, visited-set tracked. Returns exact distance or -1 if unreachable. |
| **Greedy Rat** | A\* | Prioritized search using Manhattan distance heuristic. Open-set with best-first selection. Often finds the path faster but may not be optimal. |
| **Chaos Rat** | Random Walk | Picks a random open neighbor at each step. Has a heat-based timeout (`heat < 256`). Sometimes stumbles directly onto the target. Sometimes wanders forever. |

Each rat writes its path into a dedicated trail buffer (`pure_trail`, `greedy_trail`, `chaos_trail`). Trail buffers are exposed through the world so the Python visualization can paint them live.

### 3. Converge: The Strategy Selector (`orchestrate.kn` -- `quantum_maze_run`)

```kn
converge quantum_maze_run(maze_signature, start, target, width, height) -> Int:
    spec reference:
        return reference_maze_distance(...)     // conservative heuristic
    fast greedy_rat when target("llvm"):
        return greedy_maze_distance(...)        // optimistic heuristic
    fast chaos_rat when capability("sim.rat.random_walk"):
        return chaos_maze_distance(...)         // random walk
    verify random(8)
```

This is the core insight: `converge` is **not** just for picking the fastest CPU instruction set. It's a **multi-strategy selection construct**. The `spec` lane is the ground truth (conservative heuristic). The `fast` lanes are competing alternatives. `verify random(8)` ensures the winner is consistent across 8 random inputs.

The `greedy_rat` lane activates on `target("llvm")` ~ meaning it's the default selection in compiled mode. The `chaos_rat` lane activates on `capability("sim.rat.random_walk")` ⁓ a custom capability key that could be enabled as a runtime flag. This is how you add experimental strategies without changing the core loop.

Each lane returns a *distance estimate*, not the actual path === the converge selects which distance to use as the "best" for this frame, but the actual path following uses the real BFS/A*/chaos results from the orchestrate frame.

### 4. Orchestrate: The Frame Runner (`orchestrate.kn` <--> `rat_frame_step`)

```kn
orchestrate rat_frame_step(maze, start, target, telemetry) -> Int:
    let maze_signature = kain maze_checksum(maze, telemetry.cell_count)
    let _ = kain clear_trail(telemetry.pure_trail, ...)
    let _ = kain clear_trail(telemetry.greedy_trail, ...)
    let _ = kain clear_trail(telemetry.chaos_trail, ...)
    let pure_distance = kain run_bfs_trace(maze, start, target, ...)
    let greedy_distance = kain run_astar_trace(maze, start, target, ...)
    let chaos_distance = kain run_chaos_trace(maze, start, target, ...)
    let winner_distance = kain quantum_maze_run(signature, start, target, ...)
    let committed = kain commit_search(telemetry, frame+1, start, target,
                                        pure, greedy, chaos, winner)
    return committed + pure + greedy + chaos + winner + ...
```

All stages use the `kain` runtime ~ this is **not** a GPU compute pipeline. It's a **typed multi-algorithm composition graph**. Every frame: clear the trails, run all three rats, let converge pick the winner, record the results in the patch journal. The orchestrate block makes the dependency graph visible to the compiler (stages, residency, policy) even when every stage is plain Kain code.

### 5. World: Experiment Telemetry (`world.kn`)

```kn
world RatTelemetry:
    state maze: ptr<Int>             // raw maze buffer
    state pure_trail: ptr<Int>       // BFS trail buffer
    state greedy_trail: ptr<Int>     // A* trail buffer
    state chaos_trail: ptr<Int>      // random trail buffer
    state width, height, cell_count: Int
    state frame: Int                 // experiment frame counter
    state best_distance: Int         // winning distance this frame
    state best_lane: Int             // which rat won (0=bfs, 1=greedy, 2=chaos)
    state pure_count, greedy_count, chaos_count: Int  // lifetime win counts
    state status: Int                // experiment health
    surface native_ui => SpeculativeScentVisualizer
```

The world is an **experiment telemetry container**, not an application state manager. It holds raw buffer pointers, per-algorithm win counts, frame signatures, and status codes. The `native_ui` surface is a visualization dashboard --- the Python window reads this world to paint the maze state.

### 6. Patch: Experiment Journaling (`patch.kn`)

Three patches record the experiment's state transitions:

- **`seed_telemetry`** === Initialize the world with maze geometry, trail buffers, and initial state. Validates world geometry via `rat_validate_world` law. Sets `status = 11` if geometry is invalid.

- **`commit_search`** <--> Record one experiment frame. Takes all three distances, determines which rat won (by comparing distances, handling -1 failures with a safe sentinel of 1,000,000,000), records the winner's lane, computes a frame signature, and validates `rat_distance_non_negative`. Sets `status = 12` if the best distance is negative.

- **`seal_frame`** - Finalize a frame. Records the rat's new position, trail lengths, frame signature, and liveness. Validates trail capacity bounds. Sets status codes 13-16 for various failure modes.

This is **experiment journaling** >> every frame's state transition is auditable through the patch journal. `patch_journal_count()` tells you how many frames have been recorded. Each patch validates its own invariants inline.

### 7. Laws: Domain Model Validation (`laws.kn`)

Nine laws validate the domain model itself, not just parameter ranges:

| Law | What It Validates |
|-----|-------------------|
| `rat_cell_in_bounds` | Cell index is within the maze buffer |
| `rat_coordinate_in_bounds` | (x, y) is within the grid |
| `rat_trail_within_capacity` | Trail length doesn't exceed buffer |
| `rat_distance_non_negative` | Pathfinding returned a valid distance |
| `rat_lane_kind_valid` | Lane selector is 0, 1, or 2 |
| `rat_frame_within_budget` | Frame counter hasn't overflowed |
| `rat_heat_visible` | Random walk heat is in visible range (0-255) |
| `rat_maze_geometry_valid` | Width and height are at least 4 |
| `rat_start_target_distinct` | Start and target are different valid cells |

`main.kn` calls `rat_law_lane()` at startup --> it checks that laws are *satisfiable* (not just that the predicates parse) before the experiment begins. If any law returns an unexpected result, the program exits with an error code.

### 8. Actors: Simulation Agents (`actors.kn`)

Three actors model the simulation agents:

**CheeseOracle** * * * "Where's the cheese moving to?"
```kn
actor CheeseOracle:
    state bias: Int = 19
    on Taste(reply_to: P, frame: Int):
        let offset = ((frame * 7) + self.bias + self.turns) % 5
        send reply_to.Reply(value = offset)
```
Generates a dynamic target offset that shifts each frame >> simulating a moving target. The offset is deterministic (based on frame number) but varies between -2 and +2 from the base target.

**SchrodingersRat** => "Move toward the target along the best path."
```kn
actor SchrodingersRat:
    state current_pos: Int = 0
    on Pulse(reply_to: P, request: Int):
        self.current_pos = advance_along_path(
            self.current_pos, target_pos, grid_width, grid_height, distance)
        send reply_to.Reply(value = self.current_pos)
```
Advances one step per frame along a Manhattan-biased path toward the target. Doesn't use the actual trail => it uses the winning distance from converge to bias its movement. This is the "quantum" rat: it moves based on the *outcome* of the converge selection, not any single algorithm's path.

**TrailArchivist** -- "Record this frame for posterity."
```kn
actor TrailArchivist:
    state samples: Int = 0
    state checksum: Int = 0
    on Record(reply_to: P, sample: Int):
        self.samples = self.samples + 1
        self.checksum = ((self.checksum * 31) + sample + self.samples) % MODULUS
        send reply_to.Reply(value = self.checksum)
```
Accumulates a running checksum over all frames. Used as an audit trail :: if the experiment is replayed with the same seed, the archivist's checksum should match.

### 9. Shatter Structs: Experiment Data Layout (`shatter.kn`)

```kn
shatter struct TrailSample:    // one step in any rat's trail
    cell: Int                  // which maze cell was visited
    step: Int                  // which step in the search
    lane: Int                  // which rat (0=bfs, 1=greedy, 2=chaos)
    heat: Int                  // search intensity

shatter struct MazeTile:       // per-cell visitation tracking
    wall: Int                  // is this cell a wall?
    scent: Int                 // how many rats have visited?
    visit: Int                 // total visit count
    seen: Bool                 // has any rat been here?

shatter struct RatPulseEcho:   // actor communication payload
    current: Int               // current position
    target: Int                // target position
    distance: Int              // best distance found
    turn: Int                  // actor turn count
```

These are **structure-of-arrays** layouts ‒ when the frame loop reads all `cell` fields across all trail samples, all `scent` fields across all maze tiles, or all `distance` fields across all pulse echoes, the memory access pattern is contiguous and cache-friendly.

### 10. The Main Loop (`main.kn`)

```kn
while status == 0:
    let oracle_bias = ask(oracle, "Taste", frame)
    let target = clamp_int(RAT_TARGET_INDEX + oracle_bias - 2, 0, RAT_CELL_COUNT - 1)
    let frame_mix = rat_frame_step(maze, current_pos, target, telemetry)
    let rat_reply = ask(rat, "Pulse", pack_rat_request(telemetry.best_distance, target))
    let _ = ask(archivist, "Record", ...)
    let seal = seal_frame(telemetry, rat_reply, frame_signature, ...)
    // Python visualization
    let frame_signature = python_call_attr_raw(window, "draw_frame", [...])
    let pump_open = to_int(python_call_attr_raw(window, "pump", []))
    frame = frame + 1
    sleep_millis(16)
```

The loop runs at ~60 FPS (16ms sleep). Each iteration:
1. Gets a dynamic target offset from the oracle
2. Runs all three rats via orchestrate, picks the winner via converge
3. Advances the quantum rat along the best path
4. Records the frame in the archivist
5. Seals the frame in the patch journal
6. Renders all three trails in the Python visualization window
7. Pumps the window event loop (returns 0 if window is still open)

## Why This Matters

Every Kain semantic construct in this experiment is being used "wrong" => that is, in a way that violates the obvious first-order interpretation:

| Construct | "Obvious" Use | Use Here |
|-----------|---------------|----------|
| `converge` | Pick fastest SIMD lane | **Pick best maze-solving strategy** |
| `orchestrate` | GPU compute pipeline | **Run 3 algorithms + select winner** |
| `world` | Application UI state | **Experiment telemetry dashboard** |
| `patch` | Record parameter changes | **Journal experiment frames** |
| `law` | Validate parameter ranges | **Validate domain geometry & topology** |
| `shatter struct` | SIMD particle layout | **Experiment data schema (SoA)** |
| `actor` | Service worker pool | **Simulation agents (oracle, rat, archivist)** |
| `teleport` | Cross-world zero-copy | **Declared available via capability** |

The constructs don't know they're being used for a maze experiment. They describe **relationships**: spec vs alternative, stage dependencies, state authority, invariant constraints, journaled transitions, lane-oriented layout, autonomous agents. The domain ‒ rats in a maze – is just what the author brought to the table.

## Running It

```powershell
# From the convergence blade root:
kain run . --target llvm
```

Requires Python with a visualization module (`convergence_view`) that provides `launch()`, `draw_frame()`, `pump()`, and `close()` methods. The Python side handles window creation and rendering; Kain owns the simulation.

## See Also

- `fusion_chain.kn` (`benchmark/cases_v2/`) ___ Causal chain benchmark exercising all 7 semantic layers simultaneously.
- `resonate_py_effects.kn` (`blades/python/24_tet/src/`) - Audio effects engine using world/entangle/law/patch/converge/orchestrate/pulse/resonate.
- `research/how-to-write-kain-rulebook.md` --- The full Kain authoring rule book with decision ladder and anti-patterns.
