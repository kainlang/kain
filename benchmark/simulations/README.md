# Simulation Suite

This folder is the suite-level data surface for simulation-oriented benchmark wrappers.

The rule is:

- kernel source stays in `benchmark/cases/`
- suite selection, default languages, wrapper-owned report names, and simulation telemetry live here
- `benchmark/run.py` stays generic
- `benchmark/run_wrapper.py sim` is the operator entrypoint

Current manifest:

- `simulations.json`: extracted k-os-sim simulation pack with throughput telemetry

Run it:

```powershell
python benchmark/run_sim.py
python benchmark/run_wrapper.py sim --runs 3 --warmups 1
```
