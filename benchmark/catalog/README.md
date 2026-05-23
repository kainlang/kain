# Benchmark Catalog

This folder is the benchmark control metadata authority.

- `benchmarks.main.json`: full case manifest with `tags` and `suites` annotations.
- `suites.json`: named run profiles (runner, selectors, defaults).
- `retention.json`: artifact cleanup profiles used by `bench.py clean`.
- `simulations/`: simulation suite selection metadata.

Compatibility note:

- `benchmark/benchmarks.json` is now a thin include wrapper pointing at `catalog/benchmarks.main.json`.
