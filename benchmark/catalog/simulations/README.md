# Simulation Suite

This folder owns simulation suite selection metadata.

- Kernel sources remain in `benchmark/cases/`.
- Suite defaults and case selection are defined in `simulations.json`.
- Main runner stays generic; suite orchestration happens in `bench.py` / wrapper presets.

Run the simulation suite:

```powershell
python benchmark/bench.py suite sim
python benchmark/run_sim.py
python benchmark/run_wrapper.py sim --runs 3 --warmups 1
```
