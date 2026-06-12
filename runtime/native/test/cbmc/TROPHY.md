# 🏆 CBMC Trophy Case

*auto-generated · last updated: 2026-06-11 23:38*

| file | asserts | status |
|------|--------|--------|
| arena.c | 833 | ✅ |
| attrition.c | 568 | ✅ |
| bitfield.c | 88 | ✅ |
| buddy.c | 610 | ✅ |
| contract.c | 1175 | ✅ |
| cpu.c | 99 | ✅ |
| crash_handler.c | 86 | ✅ |
| deferred_free.c | 676 | ✅ |
| diagnostics.c | 674 | ✅ |
| entangle.c | 124 | ✅ |
| fanout.c | 302 | ✅ |
| fixup.c | 562 | ✅ |
| graphics_system.c | 991 | ✅ |
| handle.c | 420 | ✅ |
| host_bridge.c | 619 | ✅ |
| machine_stones.c | 619 | ✅ |
| renderer_backend.c | 239 | ✅ |
| scene.c | 259 | ✅ |
| services.c | 849 | ✅ |
| union.c | 240 | ✅ |
| version.c | 122 | ✅ |
| virtual_alloc.c | 78 | ✅ |
| wire.c | 128 | ✅ |
| actor.c | — | ⏳ rerun |
| async.c | — | ⏳ rerun |
| batch_queue.c | — | ⏳ rerun |
| compatibility.c | — | ⏳ rerun |
| converge.c | — | ⏳ rerun |
| event.c | — | ⏳ rerun |
| input_system.c | — | ⏳ rerun |
| interop_contracts.c | — | ⏳ rerun |
| interop_zero_copy.c | — | ⏳ rerun |
| json.c | — | ⏳ rerun |
| memory.c | — | ⏳ rerun |
| ownership.c | — | ⏳ rerun |
| profile.c | — | ⏳ rerun |
| reflection.c | — | ⏳ rerun |
| renderer_session.c | — | ⏳ rerun |
| simd.c | — | ⏳ rerun |
| stdlib_abi.c | — | ⏳ rerun |
| core.c | — | ⏳ |
| cuda_runtime.c | — | ⏳ |
| json_benchmark.c | — | ⏳ |
| net_system.c | — | ⏳ |
| process_system.c | — | ⏳ |
| python_runtime.c | — | ⏳ |
| ray_sphere_benchmark.c | — | ⏳ |
| realtime.c | — | ⏳ |

**23/48 files verified · 10361 assertions, 0 violations**

```
python test/scripts/run_pipeline.py cbmc --harness check_<module>
python test/scripts/run_pipeline.py trophy
```
