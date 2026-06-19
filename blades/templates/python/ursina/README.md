# Kain + Ursina Template

**A modular Kain project template for building Ursina 3D applications with full semantic stack integration.**

## Quick Start

```bash
# Build (typecheck + LLVM compile + native link attempt)
kain build --target llvm

# The Python interop path only works through native LLVM (not the interpreter).
# Run the compiled executable directly when native linking succeeds.
```

## File Structure

| File | Layer | What It Does |
|------|-------|-------------|
| `src/main.kn` | **Entry** | 5-phase lifecycle: Probe → Pre-Sim → Bridge → Launch → Validate |
| `src/world.kn` | **State** | `GameWorld` + `GameMirror` + 4 entangles + 4 laws + 7 patches + `GameHUD` component |
| `src/ui.kn` | **UI** | Reusable prop-based components: `StatusBadge`, `FPSDisplay`, `EntityInspector`, `DebugHUD` |
| `src/actors.kn` | **Concurrency** | `UrsinaBridge` actor <--> fire-and-forget handlers for frame ticks and keyboard events |
| `src/bridge.kn` | **Python** | Complete Ursina bootstrap (inline Python, no .py files) ->> 3D scene, camera orbit, callback wiring |
| `src/scene.kn` | **Data** | `shatter struct EntityTemplate` + scene constants + 7 preset entities |
| `src/pipeline.kn` | **Dispatch** | `converge entity_update` + `orchestrate ursina_game_loop` |
| `src/helpers.kn` | **Utils** | Pure math functions, actor message packing, constants |
| `build.kn` | **Build** | Project configuration |

## Semantic Stack (12 constructs)

`world` • `entangle` • `law` • `patch` • `shatter struct` • `actor` • `component` • `converge` • `orchestrate` • `import` • `python_exec` • `python_actor_callback`

## Adding Stuff

- **New actors?** → Drop in `actors.kn`, spawn in `main.kn`, wire callbacks in `bridge.kn`
- **New scene objects?** → Add to `entity_data` in `bridge.kn`'s inline Python
- **New state?** → Add to `world.kn`: fields, entangles, laws, patches
- **New UI?** → Drop a component in `ui.kn`, compose in `GameHUD`
- **New pipeline?** → Add converge/orchestrate to `pipeline.kn`

## Known Constraints

- Orchestrate stage clauses must be on ONE line
- Actor fire-and-forget handlers only (no `reply_to` --> avoids LLVM codegen conflict)
- Build DSL (`build.kn`) uses a special evaluation path that `kain check` can't parse === this is expected
