# 🏆 CBMC Trophy Case

*auto-generated · last updated: 2026-06-08 19:41*

| file | asserts | status |
|------|--------|--------|
| arena.c | 833 | ✅ |
| bitfield.c | 88 | ✅ |
| buddy.c | 610 | ✅ |
| cpu.c | 99 | ✅ |
| crash_handler.c | 86 | ✅ |
| deferred_free.c | 676 | ✅ |
| entangle.c | 124 | ✅ |
| fanout.c | 302 | ✅ |
| fixup.c | 562 | ✅ |
| handle.c | 420 | ✅ |
| services.c | 849 | ✅ |
| union.c | 240 | ✅ |
| version.c | 122 | ✅ |
| virtual_alloc.c | 78 | ✅ |
| actor.c | — | ⏳ rerun |
| batch_queue.c | — | ⏳ rerun |
| converge.c | — | ⏳ rerun |
| memory.c | — | ⏳ rerun |
| ownership.c | — | ⏳ rerun |
| async.c | — | ⏳ |
| attrition.c | — | ⏳ |
| compatibility.c | — | ⏳ |
| contract.c | — | ⏳ |
| core.c | — | ⏳ |
| cuda_runtime.c | — | ⏳ |
| diagnostics.c | — | ⏳ |
| graphics_system.c | — | ⏳ |
| host_bridge.c | — | ⏳ |
| input_system.c | — | ⏳ |
| interop_contracts.c | — | ⏳ |
| interop_zero_copy.c | — | ⏳ |
| json.c | — | ⏳ |
| json_benchmark.c | — | ⏳ |
| machine_stones.c | — | ⏳ |
| net_system.c | — | ⏳ |
| process_system.c | — | ⏳ |
| profile.c | — | ⏳ |
| python_runtime.c | — | ⏳ |
| ray_sphere_benchmark.c | — | ⏳ |
| realtime.c | — | ⏳ |
| reflection.c | — | ⏳ |
| renderer_backend.c | — | ⏳ |
| renderer_session.c | — | ⏳ |
| scene.c | — | ⏳ |
| simd.c | — | ⏳ |
| stdlib_abi.c | — | ⏳ |
| wire.c | — | ⏳ |

**14/47 files verified · 5089 assertions, 0 violations**

```
python test/scripts/run_pipeline.py cbmc --harness check_<module>
python test/scripts/run_pipeline.py trophy
```
