# 🏆 CBMC Trophy Case

*auto-generated · last updated: 2026-06-08 03:58*

| file | asserts | status |
|------|--------|--------|
| arena.c | 833 | ✅ |
| bitfield.c | 88 | ✅ |
| buddy.c | 610 | ✅ |
| cpu.c | 99 | ✅ |
| crash_handler.c | 86 | ✅ |
| deferred_free.c | 676 | ✅ |
| entangle.c | 124 | ✅ |
| fixup.c | 562 | ✅ |
| handle.c | 420 | ✅ |
| union.c | 240 | ✅ |
| version.c | 122 | ✅ |
| actor.c | — | ⏳ rerun |
| batch_queue.c | — | ⏳ rerun |
| async.c | — | ⏳ |
| attrition.c | — | ⏳ |
| compatibility.c | — | ⏳ |
| contract.c | — | ⏳ |
| converge.c | — | ⏳ |
| core.c | — | ⏳ |
| cuda_runtime.c | — | ⏳ |
| diagnostics.c | — | ⏳ |
| fanout.c | — | ⏳ |
| graphics_system.c | — | ⏳ |
| host_bridge.c | — | ⏳ |
| input_system.c | — | ⏳ |
| interop_contracts.c | — | ⏳ |
| interop_zero_copy.c | — | ⏳ |
| json.c | — | ⏳ |
| json_benchmark.c | — | ⏳ |
| machine_stones.c | — | ⏳ |
| memory.c | — | ⏳ |
| net_system.c | — | ⏳ |
| ownership.c | — | ⏳ |
| process_system.c | — | ⏳ |
| profile.c | — | ⏳ |
| python_runtime.c | — | ⏳ |
| ray_sphere_benchmark.c | — | ⏳ |
| realtime.c | — | ⏳ |
| reflection.c | — | ⏳ |
| renderer_backend.c | — | ⏳ |
| renderer_session.c | — | ⏳ |
| scene.c | — | ⏳ |
| services.c | — | ⏳ |
| simd.c | — | ⏳ |
| stdlib_abi.c | — | ⏳ |
| virtual_alloc.c | — | ⏳ |
| wire.c | — | ⏳ |

**11/47 files verified · 3860 assertions, 0 violations**

```
python test/scripts/run_pipeline.py cbmc --harness check_<module>
python test/scripts/run_pipeline.py trophy
```
