# KAIN BY EXAMPLE — Part 2: Dispatch → Machine Stones (L3–L6)

> **One compilable snippet per feature. No prose, no theory — just proof that it compiles.**
> Part 2 covers LAYER 3 (converge), LAYER 4 (orchestrate), LAYER 5 (pulse, resonate), LAYER 6 (axiom, shatter, teleport).

______________________________________________________________________

## LAYER 3 — DISPATCH

### converge — Hardware Capability Dispatch (Real SIMD)

```kn
fn scalar_dot(left: ptr<Int>, right: ptr<Int>, cells: Int, bias: Int, mod: Int) -> Int with Unsafe:
    var i: Int = 0
    var total: Int = 0
    while i < cells:
        let l = mem_load(ptr_offset(left, i, "Int"), "Int") + bias
        let r = mem_load(ptr_offset(right, i, "Int"), "Int")
        total = (total + (l * r)) % mod
        i = i + 1
    return total

converge fast_dot(left: ptr<Int>, right: ptr<Int>, cells: Int, bias: Int, mod: Int) -> Int:
    spec reference:
        return scalar_dot(left, right, cells, bias, mod)
    fast avx512_lane when capability("cpu.x86.avx512f"):
        return simd_dot_avx512(left, right, cells, bias, mod)
    fast avx2_lane when capability("cpu.x86.avx2"):
        return simd_dot_avx2(left, right, cells, bias, mod)
    verify random(8)
```

______________________________________________________________________

## LAYER 4 — STAGE GRAPH

### orchestrate — 7-Stage Full Pipeline (All Stage Kinds)

```kn
orchestrate signal_pipeline(value: Int, epoch: Int) -> Int:
    stage host_base: cpu mix(value + epoch)
        when capability("cpu.scalar") residency host transfer none policy telemetry_prefer_cpu
    stage fast_lane: converge fast_mix(host_base, epoch)
        deps [host_base] residency host policy static
    stage law_check: law signal_valid(host_base)
        after fast_lane residency host policy static
    stage world_score: world world_score(host_base, epoch, fast_lane)
        after fast_lane requires law_check residency shared transfer shared_view policy telemetry_balance_latency
    stage gpu_tune: gpu fast_mix(world_score + epoch, 7)
        after world_score residency device transfer host_to_device
        guarded by silicon_truth fallback degrade degrade_fn policy telemetry_prefer_gpu
    stage patch_step: patch commit_ack(Authority)
        deps [world_score, gpu_tune] requires law_check residency host policy telemetry_prefer_cpu fallback degrade degrade_fn
    stage final_out: dispatch dispatch_style(patch_step + gpu_tune, epoch)
        deps [patch_step, gpu_tune] residency shared transfer shared_view policy telemetry_balance_latency
    return world_score + patch_step + final_out
```

### orchestrate — Multi-Language (C + Python + GPU)

```kn
orchestrate preflight_pipeline(seed: Int, authority: Authority) -> Int:
    stage cpu_seed: cpu mix(seed + authority.signal)
        when capability("cpu.scalar") residency host transfer none policy static
    stage c_shadow: c host_shadow(cpu_seed + authority.epoch)
        after cpu_seed residency host fallback cpu_seed policy telemetry_prefer_cpu
    stage py_shadow: python py_shadow(c_shadow + authority.drift)
        after c_shadow residency host fallback degrade c_shadow policy telemetry_prefer_cpu
    stage converge_lane: converge fast_mix(py_shadow + cpu_seed)
        deps [cpu_seed, py_shadow] residency shared transfer shared_view policy telemetry_balance_latency
    stage gpu_lane: gpu fast_mix(converge_lane + authority.gpu_epoch)
        after converge_lane residency device transfer host_to_device
        guarded by silicon_truth fallback degrade c_shadow policy telemetry_prefer_gpu
    stage legal: law signal_valid(gpu_lane)
        after gpu_lane residency host transfer device_to_host policy static
    stage committed: patch commit(authority, mod(gpu_lane + c_shadow), converge_lane, gpu_lane)
        after legal requires legal residency host policy telemetry_balance_latency
    stage final_lane: dispatch dispatch_style(committed + py_shadow, authority.epoch)
        deps [cpu_seed, c_shadow, py_shadow, committed] residency shared transfer shared_view policy telemetry_balance_latency
    if legal == false:
        return c_shadow
    return final_lane
```

______________________________________________________________________

## LAYER 5 — TEMPORAL

### pulse — Teleport Inside a Timed Beat

```kn
pulse clock_driver every 8ms jitter 1ms:
    let shard = Shard { bias: 1, phase: 2, token: 3, gpu_hint: 4, alive: true }
    let moved = teleport shard from Authority to Mirror via pulse_bus
    let _debug = pulse_tick + pulse_dt_ms + pulse_missed + moved.bias + moved.gpu_hint
```

> **Duration units:** `ns`, `us`, `ms`, `s`, `tick`, `ticks`
> **Body locals:** `pulse_tick`, `pulse_dt_ms`, `pulse_missed`

### resonate — Calls Orchestrate Pipeline on State Change

```kn
resonate Authority.signal dampen 0 ms:
    Authority.last_old = resonate_old_i64
    Authority.last_new = resonate_new_i64
    Authority.shadow = signal_pipeline(
        resonate_new_i64 + Authority.tick,
        Authority.tick
    )
```

> **Handler locals:** `resonate_new_i64`, `resonate_old_i64`, `resonate_fired`
> **Key pattern:** Resonate calls an orchestrate pipeline — the compiler-owned causal chain: patch → resonate → orchestrate → world update.
> **Anti-self-feedback:** cannot write to own trigger field.

______________________________________________________________________

## LAYER 6 — MACHINE STONES

### axiom — Capability Assumption with Fallback

```kn
axiom machine_truth:
    when target("llvm")
    when arch("x86_64")
    when capability("memory.shatter")
    when capability("world.teleport")
    guarantee "machine supports shatter + teleport + inline asm"
    fallback scalar_fallback
```

### shatter struct — Structure-of-Arrays Layout

```kn
shatter struct Particle:
    position_x: Float
    position_y: Float
    velocity_x: Float
    velocity_y: Float
    alive: Bool

fn use_shattered(particles: [Particle]) -> Int:
    let p = particles[0]
    return (p.position_x + p.velocity_x) as Int
```

### teleport — Zero-Copy with Integrity Proof

```kn
shatter struct Shard:
    bias: Int
    phase: Int
    checksum: Int
    alive: Bool

fn shard_score(s: Shard) -> Int:
    return ((s.bias * 31) + (s.phase * 17) + s.checksum) % 1000000007

let shard = Shard { bias: 42, phase: 13, checksum: 7, alive: true }
let score_before = shard_score(shard)
let moved = teleport shard from Authority to Mirror via shard_bus
let score_after = shard_score(moved)
// score_before == score_after: zero-copy integrity preserved
```
