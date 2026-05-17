# Kain Benchmark Snapshot

- status: `PASS`
- generated_at: `2026-05-17T03:20:50.344006+00:00`
- warmups: `2`
- timed_runs: `5`
- languages: `kain, rust, cpp`
- root_snapshot: `benchmark/latest_json_fs_literal_pool.md`
- full_report: `benchmark/out/reports/latest.llm.md`
- json_report: `benchmark/out/reports/latest.json`

## Summary

| case | maturity | winner | kain median ms | rust median ms | cpp median ms |
| --- | --- | --- | --- | --- | --- |
| json_manual_roundtrip | implemented | cpp | 108.326 | 110.410 | 97.638 |
| filesystem_stream | implemented | cpp | 136.184 | 101.231 | 81.106 |
