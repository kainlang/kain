# Kain Lattice — IDE Syntax Highlighting Preview & Palette Redesign

**Current theme:** `lattice` (default) · **Background:** `#0d1218` · **Text:** `#e2e8ee`

---

## ⚠️ Current Palette — Legacy Grouping (Pre-Ladder)

> Lattice was built months before the 7-layer decision ladder was finalized. Keywords are grouped by **domain** (actor words, world words, shader words) — not by semantic layer. Result: 6 of 16 syntax roles are blue-tinted, temporal constructs (`pulse`, `resonate`) sit in the World family, and machine stones (`axiom`, `shatter`, `teleport`) are split across World and Proof.

### Active Syntax Roles — Lattice Theme

<table>
<tr><th>Role</th><th>Hex</th><th>Swatch</th><th>Keywords</th></tr>
<tr><td><code>keyword.core</code></td><td><code>#b3c5d8</code></td><td><span style="display:inline-block;width:72px;height:18px;background:#b3c5d8;border-radius:2px;"></span></td><td><code>fn let mut var const if else elif match for while loop break continue return await in with as pub mod use self Self true false none and or</code></td></tr>
<tr><td><code>keyword.type</code></td><td><code>#8fc2e8</code></td><td><span style="display:inline-block;width:72px;height:18px;background:#8fc2e8;border-radius:2px;"></span></td><td><code>type struct enum trait impl</code></td></tr>
<tr><td><code>keyword.effect</code></td><td><code>#d6b46f</code></td><td><span style="display:inline-block;width:72px;height:18px;background:#d6b46f;border-radius:2px;"></span></td><td><code>Pure IO async Async GPU Reactive Unsafe</code></td></tr>
<tr><td><code>family.actor</code></td><td><code>#e18a75</code></td><td><span style="display:inline-block;width:72px;height:18px;background:#e18a75;border-radius:2px;"></span></td><td><code>actor spawn send receive emit on</code></td></tr>
<tr><td><code>family.world</code></td><td><code>#58d0c3</code></td><td><span style="display:inline-block;width:72px;height:18px;background:#58d0c3;border-radius:2px;"></span></td><td><code>world entangle state surface patch law pulse shatter teleport single_writer dampen</code> ⬅ <b>too many layers lumped together</b></td></tr>
<tr><td><code>family.ownership</code></td><td><code>#d3ef73</code></td><td><span style="display:inline-block;width:72px;height:18px;background:#d3ef73;border-radius:2px;"></span></td><td><code>collapse observe decay share weak</code></td></tr>
<tr><td><code>family.proof</code></td><td><code>#e3cb70</code></td><td><span style="display:inline-block;width:72px;height:18px;background:#e3cb70;border-radius:2px;"></span></td><td><code>converge orchestrate axiom spec fast verify random when target capability guarantee fallback every jitter from to via stage</code> ⬅ <b>L3+L4+L6 keywords all in one bucket</b></td></tr>
<tr><td><code>family.shader</code></td><td><code>#7ea9ff</code></td><td><span style="display:inline-block;width:72px;height:18px;background:#7ea9ff;border-radius:2px;"></span></td><td><code>shader vertex fragment compute uniform workgroup render component comptime macro fanout test</code></td></tr>
<tr><td><code>type</code></td><td><code>#c3d2ea</code></td><td><span style="display:inline-block;width:72px;height:18px;background:#c3d2ea;border-radius:2px;"></span></td><td>Capitalized identifiers: <code>Int Bool String Float Vec3 OmniPacket</code> ...</td></tr>
<tr><td><code>identifier</code></td><td><code>#e2e8ee</code></td><td><span style="display:inline-block;width:72px;height:18px;background:#e2e8ee;border-radius:2px;"></span></td><td>Variables, function names, field names</td></tr>
<tr><td><code>string</code></td><td><code>#98c5b2</code></td><td><span style="display:inline-block;width:72px;height:18px;background:#98c5b2;border-radius:2px;"></span></td><td><code>"hello"</code> <code>'x'</code> raw strings</td></tr>
<tr><td><code>number</code></td><td><code>#cda678</code></td><td><span style="display:inline-block;width:72px;height:18px;background:#cda678;border-radius:2px;"></span></td><td><code>42</code> <code>3.14</code> <code>0xFF</code></td></tr>
<tr><td><code>comment</code></td><td><code>#738391</code></td><td><span style="display:inline-block;width:72px;height:18px;background:#738391;border-radius:2px;"></span></td><td><code>// line comment</code> <code># hash comment</code></td></tr>
<tr><td><code>operator</code></td><td><code>#acc0d1</code></td><td><span style="display:inline-block;width:72px;height:18px;background:#acc0d1;border-radius:2px;"></span></td><td><code>+ - * / % = == != < > <= >= -> => :: .. && || ( ) { } [ ] , . : ;</code></td></tr>
<tr><td><code>directive</code></td><td><code>#98a9cf</code></td><td><span style="display:inline-block;width:72px;height:18px;background:#98a9cf;border-radius:2px;"></span></td><td><code>@0</code> <code>@1</code> (GPU binding slots)</td></tr>
<tr><td><code>invalid</code></td><td><code>#db8b88</code></td><td><span style="display:inline-block;width:72px;height:18px;background:#db8b88;border-radius:2px;"></span></td><td>Unrecognized / parse-error tokens</td></tr>
</table>

### Hue Distribution — Current Palette

```
████████████████████████████████████████████████████  keyword.core     #b3c5d8  BLUE-GRAY
████████████████████████████████████████████████████  keyword.type     #8fc2e8  SKY BLUE
████████████████████████████████████████████████████  type             #c3d2ea  LIGHT BLUE
████████████████████████████████████████████████████  family.shader    #7ea9ff  SOFT BLUE
████████████████████████████████████████████████████  directive        #98a9cf  BLUE-LAVENDER
████████████████████████████████████████████████████  operator         #acc0d1  GRAY-BLUE
████████████████████████████████████████████████████  identifier       #e2e8ee  WHITE
████████████████████████████████████████████████████  comment          #738391  MUTED GRAY
████████████████████████████████████████████████████  family.actor     #e18a75  SALMON
████████████████████████████████████████████████████  family.world     #58d0c3  TEAL
████████████████████████████████████████████████████  family.ownership #d3ef73  LIME
████████████████████████████████████████████████████  family.proof     #e3cb70  GOLD
████████████████████████████████████████████████████  keyword.effect   #d6b46f  WARM GOLD
████████████████████████████████████████████████████  string           #98c5b2  TEAL-GREEN
████████████████████████████████████████████████████  number           #cda678  GOLDEN TAN
████████████████████████████████████████████████████  invalid          #db8b88  SOFT RED
```

**6 blue-tinted roles.** The non-blue colors (salmon, teal, lime, gold) are doing heavy lifting while half the palette sits in blue-gray territory. The visual result: a file looks blue-white-gray with occasional flashes of color. The layer structure is invisible.

---

## Live Preview — Current Lattice Rendering

<div style="background:#0d1218;color:#e2e8ee;font-family:'Cascadia Code','JetBrains Mono','Fira Code',Consolas,monospace;font-size:13px;line-height:1.55;padding:20px;border-radius:6px;white-space:pre;overflow-x:auto;">

<span style="color:#738391">// ============================================================================</span>
<span style="color:#738391">//  KAIN SYNTAX PREVIEW — Lattice Theme (current, pre-ladder)</span>
<span style="color:#738391">// ============================================================================</span>

<span style="font-weight:bold;color:#b3c5d8">use</span> <span style="color:#c3d2ea">std::runtime</span>
<span style="font-weight:bold;color:#b3c5d8">use</span> <span style="color:#c3d2ea">std::actor</span>
<span style="font-weight:bold;color:#b3c5d8">use</span> <span style="color:#c3d2ea">std::math</span>

<span style="color:#738391">// ── L0: PLAIN CODE ───────────────────────────────────────────────────────</span>
<span style="font-weight:bold;color:#b3c5d8">const</span> <span style="color:#c3d2ea">MAX</span>: <span style="color:#c3d2ea">Int</span> = <span style="color:#cda678">1000</span>
<span style="font-weight:bold;color:#8fc2e8">type</span> <span style="color:#c3d2ea">Score</span> = <span style="color:#c3d2ea">Int</span>

<span style="font-weight:bold;color:#8fc2e8">struct</span> <span style="color:#c3d2ea">Packet</span>:
    <span style="color:#e2e8ee">id</span>: <span style="color:#c3d2ea">Int</span>
    <span style="color:#e2e8ee">payload</span>: <span style="color:#c3d2ea">Int</span>

<span style="font-weight:bold;color:#8fc2e8">enum</span> <span style="color:#c3d2ea">Mode</span>:
    <span style="color:#c3d2ea">Scalar</span>
    <span style="color:#c3d2ea">Vectorized</span>

<span style="font-weight:bold;color:#b3c5d8">fn</span> <span style="color:#e2e8ee">process</span><span style="color:#acc0d1">(</span><span style="color:#e2e8ee">p</span>: <span style="color:#c3d2ea">Packet</span><span style="color:#acc0d1">)</span> <span style="color:#acc0d1">-></span> <span style="color:#c3d2ea">Int</span> <span style="font-weight:bold;color:#b3c5d8">with</span> <span style="font-weight:bold;color:#d6b46f">Pure</span>:
    <span style="font-weight:bold;color:#b3c5d8">if</span> <span style="color:#e2e8ee">p</span><span style="color:#acc0d1">.</span><span style="color:#e2e8ee">id</span> <span style="color:#acc0d1">></span> <span style="color:#cda678">0</span>:
        <span style="font-weight:bold;color:#b3c5d8">return</span> <span style="color:#e2e8ee">p</span><span style="color:#acc0d1">.</span><span style="color:#e2e8ee">payload</span> <span style="color:#acc0d1">*</span> <span style="color:#cda678">2</span>
    <span style="font-weight:bold;color:#b3c5d8">return</span> <span style="color:#cda678">0</span>

<span style="color:#738391">// ── L1: STATE AUTHORITY — world, entangle ────────────────────────────────</span>
<span style="font-weight:bold;color:#58d0c3">world</span> <span style="color:#c3d2ea">AppState</span>:
    <span style="font-weight:bold;color:#58d0c3">state</span> <span style="color:#e2e8ee">counter</span>: <span style="color:#c3d2ea">Int</span> = <span style="color:#cda678">0</span>
    <span style="font-weight:bold;color:#58d0c3">state</span> <span style="color:#e2e8ee">last_signal</span>: <span style="color:#c3d2ea">Int</span> = <span style="color:#cda678">0</span>

<span style="font-weight:bold;color:#58d0c3">world</span> <span style="color:#c3d2ea">Mirror</span>:
    <span style="font-weight:bold;color:#58d0c3">state</span> <span style="color:#e2e8ee">counter_copy</span>: <span style="color:#c3d2ea">Int</span> = <span style="color:#cda678">0</span>

<span style="font-weight:bold;color:#58d0c3">entangle</span> <span style="color:#c3d2ea">AppState</span><span style="color:#acc0d1">.</span><span style="color:#e2e8ee">counter</span> <span style="color:#acc0d1"><-></span> <span style="color:#c3d2ea">Mirror</span><span style="color:#acc0d1">.</span><span style="color:#e2e8ee">counter_copy</span> <span style="font-weight:bold;color:#58d0c3">with</span> <span style="font-weight:bold;color:#58d0c3">single_writer</span>

<span style="color:#738391">// ── L2: STATE INTEGRITY — patch, law ─────────────────────────────────────</span>
<span style="font-weight:bold;color:#58d0c3">law</span> <span style="color:#e2e8ee">counter_valid</span><span style="color:#acc0d1">(</span><span style="color:#e2e8ee">v</span>: <span style="color:#c3d2ea">Int</span><span style="color:#acc0d1">)</span> <span style="color:#acc0d1">-></span> <span style="color:#c3d2ea">Bool</span>:
    <span style="font-weight:bold;color:#b3c5d8">return</span> <span style="color:#e2e8ee">v</span> <span style="color:#acc0d1">>=</span> <span style="color:#cda678">0</span>

<span style="font-weight:bold;color:#58d0c3">patch</span> <span style="color:#e2e8ee">increment</span><span style="color:#acc0d1">(</span><span style="color:#e2e8ee">s</span>: <span style="color:#c3d2ea">AppState</span><span style="color:#acc0d1">)</span> <span style="color:#acc0d1">-></span> <span style="color:#c3d2ea">Int</span>:
    <span style="color:#e2e8ee">s</span><span style="color:#acc0d1">.</span><span style="color:#e2e8ee">counter</span> = <span style="color:#e2e8ee">s</span><span style="color:#acc0d1">.</span><span style="color:#e2e8ee">counter</span> <span style="color:#acc0d1">+</span> <span style="color:#cda678">1</span>
    <span style="font-weight:bold;color:#b3c5d8">return</span> <span style="color:#e2e8ee">s</span><span style="color:#acc0d1">.</span><span style="color:#e2e8ee">counter</span>

<span style="color:#738391">// ── L3: DISPATCH — converge ──────────────────────────────────────────────</span>
<span style="font-weight:bold;color:#e3cb70">converge</span> <span style="color:#e2e8ee">fast_hash</span><span style="color:#acc0d1">(</span><span style="color:#e2e8ee">v</span>: <span style="color:#c3d2ea">Int</span><span style="color:#acc0d1">)</span> <span style="color:#acc0d1">-></span> <span style="color:#c3d2ea">Int</span>:
    <span style="font-weight:bold;color:#e3cb70">spec</span> <span style="color:#e2e8ee">reference</span>:
        <span style="font-weight:bold;color:#b3c5d8">return</span> <span style="color:#e2e8ee">v</span> <span style="color:#acc0d1">*</span> <span style="color:#cda678">31</span> <span style="color:#acc0d1">+</span> <span style="color:#cda678">7</span>
    <span style="font-weight:bold;color:#e3cb70">fast</span> <span style="color:#e2e8ee">simd_lane</span> <span style="font-weight:bold;color:#e3cb70">when</span> <span style="font-weight:bold;color:#e3cb70">capability</span><span style="color:#acc0d1">(</span><span style="color:#98c5b2">"cpu.avx2"</span><span style="color:#acc0d1">)</span>:
        <span style="font-weight:bold;color:#b3c5d8">return</span> <span style="color:#e2e8ee">v</span> <span style="color:#acc0d1">*</span> <span style="color:#cda678">48</span> <span style="color:#acc0d1">+</span> <span style="color:#cda678">14</span>
    <span style="font-weight:bold;color:#e3cb70">verify</span> <span style="font-weight:bold;color:#e3cb70">random</span><span style="color:#acc0d1">(</span><span style="color:#cda678">8</span><span style="color:#acc0d1">)</span>

<span style="color:#738391">// ── L4: STAGE GRAPH — orchestrate ────────────────────────────────────────</span>
<span style="font-weight:bold;color:#e3cb70">orchestrate</span> <span style="color:#e2e8ee">pipeline</span><span style="color:#acc0d1">(</span><span style="color:#e2e8ee">v</span>: <span style="color:#c3d2ea">Int</span><span style="color:#acc0d1">)</span> <span style="color:#acc0d1">-></span> <span style="color:#c3d2ea">Int</span>:
    <span style="font-weight:bold;color:#e3cb70">stage</span> <span style="color:#e2e8ee">a</span>: <span style="color:#e2e8ee">cpu</span> <span style="color:#e2e8ee">process</span><span style="color:#acc0d1">(</span><span style="color:#e2e8ee">v</span><span style="color:#acc0d1">)</span>
    <span style="font-weight:bold;color:#e3cb70">stage</span> <span style="color:#e2e8ee">b</span>: <span style="font-weight:bold;color:#e3cb70">converge</span> <span style="color:#e2e8ee">fast_hash</span><span style="color:#acc0d1">(</span><span style="color:#e2e8ee">a</span><span style="color:#acc0d1">)</span>
    <span style="font-weight:bold;color:#b3c5d8">return</span> <span style="color:#e2e8ee">b</span>

<span style="color:#738391">// ── L5: TEMPORAL — pulse, resonate ───────────────────────────────────────</span>
<span style="font-weight:bold;color:#58d0c3">resonate</span> <span style="color:#c3d2ea">AppState</span><span style="color:#acc0d1">.</span><span style="color:#e2e8ee">counter</span> <span style="font-weight:bold;color:#58d0c3">dampen</span> <span style="color:#cda678">10</span> <span style="color:#e2e8ee">ms</span>:
    <span style="color:#c3d2ea">AppState</span><span style="color:#acc0d1">.</span><span style="color:#e2e8ee">last_signal</span> = <span style="color:#e2e8ee">resonate_new_i64</span>  <span style="color:#738391">// ⬅ resonate is teal? it belongs in L5 Temporal, not L1 World</span>

<span style="font-weight:bold;color:#58d0c3">pulse</span> <span style="color:#e2e8ee">heartbeat</span> <span style="font-weight:bold;color:#e3cb70">every</span> <span style="color:#cda678">16</span> <span style="color:#e2e8ee">ms</span> <span style="font-weight:bold;color:#e3cb70">jitter</span> <span style="color:#cda678">2</span> <span style="color:#e2e8ee">ms</span>:
    <span style="color:#c3d2ea">AppState</span><span style="color:#acc0d1">.</span><span style="color:#e2e8ee">counter</span> = <span style="color:#c3d2ea">AppState</span><span style="color:#acc0d1">.</span><span style="color:#e2e8ee">counter</span> <span style="color:#acc0d1">+</span> <span style="color:#cda678">1</span>  <span style="color:#738391">// ⬅ pulse is teal, every/jitter are gold — three L5 keywords, two different colors</span>

<span style="color:#738391">// ── L6: MACHINE STONES — axiom, shatter, teleport ────────────────────────</span>
<span style="font-weight:bold;color:#58d0c3">shatter</span> <span style="font-weight:bold;color:#8fc2e8">struct</span> <span style="color:#c3d2ea">Shard</span>:                   <span style="color:#738391">// ⬅ shatter = teal (World family)</span>
    <span style="color:#e2e8ee">bias</span>: <span style="color:#c3d2ea">Int</span>
    <span style="color:#e2e8ee">phase</span>: <span style="color:#c3d2ea">Int</span>

<span style="font-weight:bold;color:#e3cb70">axiom</span> <span style="color:#e2e8ee">machine_ok</span>:                    <span style="color:#738391">// ⬅ axiom = gold (Proof family) — same layer, different color!</span>
    <span style="font-weight:bold;color:#e3cb70">when</span> <span style="font-weight:bold;color:#e3cb70">target</span><span style="color:#acc0d1">(</span><span style="color:#98c5b2">"llvm"</span><span style="color:#acc0d1">)</span>
    <span style="font-weight:bold;color:#e3cb70">when</span> <span style="font-weight:bold;color:#e3cb70">capability</span><span style="color:#acc0d1">(</span><span style="color:#98c5b2">"memory.shatter"</span><span style="color:#acc0d1">)</span>
    <span style="font-weight:bold;color:#e3cb70">guarantee</span> <span style="color:#98c5b2">"machine supports shatter layout"</span>
    <span style="font-weight:bold;color:#e3cb70">fallback</span> <span style="color:#e2e8ee">fallback_fn</span>

<span style="font-weight:bold;color:#b3c5d8">let</span> <span style="color:#e2e8ee">moved</span> = <span style="font-weight:bold;color:#58d0c3">teleport</span> <span style="color:#e2e8ee">shard</span> <span style="font-weight:bold;color:#e3cb70">from</span> <span style="color:#c3d2ea">AppState</span> <span style="font-weight:bold;color:#e3cb70">to</span> <span style="color:#c3d2ea">Mirror</span> <span style="font-weight:bold;color:#e3cb70">via</span> <span style="color:#e2e8ee">bus</span>  <span style="color:#738391">// ⬅ teleport=teal, from/to/via=gold — three colors for one L6 expression</span>

<span style="color:#738391">// ── L7: SYSTEMS — actor, ownership ───────────────────────────────────────</span>
<span style="font-weight:bold;color:#e18a75">actor</span> <span style="color:#c3d2ea">Worker</span>:                          <span style="color:#738391">// ⬅ actor = salmon — this works</span>
    <span style="font-weight:bold;color:#58d0c3">state</span> <span style="color:#e2e8ee">ticks</span>: <span style="color:#c3d2ea">Int</span> = <span style="color:#cda678">0</span>              <span style="color:#738391">// ⬅ state = teal even inside an actor — should it be?</span>
    <span style="font-weight:bold;color:#e18a75">on</span> <span style="color:#c3d2ea">Tick</span><span style="color:#acc0d1">(</span><span style="color:#e2e8ee">n</span>: <span style="color:#c3d2ea">Int</span><span style="color:#acc0d1">)</span>:
        <span style="color:#e2e8ee">self</span><span style="color:#acc0d1">.</span><span style="color:#e2e8ee">ticks</span> = <span style="color:#e2e8ee">self</span><span style="color:#acc0d1">.</span><span style="color:#e2e8ee">ticks</span> <span style="color:#acc0d1">+</span> <span style="color:#e2e8ee">n</span>

<span style="font-weight:bold;color:#b3c5d8">fn</span> <span style="color:#e2e8ee">raw_ops</span><span style="color:#acc0d1">()</span> <span style="color:#acc0d1">-></span> <span style="color:#c3d2ea">Int</span> <span style="font-weight:bold;color:#b3c5d8">with</span> <span style="font-weight:bold;color:#d6b46f">Unsafe</span>:
    <span style="font-weight:bold;color:#b3c5d8">let</span> <span style="font-weight:bold;color:#b3c5d8">mut</span> <span style="color:#e2e8ee">p</span>: <span style="color:#e2e8ee">ptr</span><span style="color:#acc0d1"><</span><span style="color:#c3d2ea">Int</span><span style="color:#acc0d1">></span> = <span style="color:#e2e8ee">alloc_zeroed</span><span style="color:#acc0d1">(</span><span style="color:#cda678">1</span>, <span style="color:#98c5b2">"Int"</span><span style="color:#acc0d1">)</span>
    <span style="font-weight:bold;color:#d3ef73">collapse</span> <span style="color:#e2e8ee">p</span>:                      <span style="color:#738391">// ⬅ collapse = lime — this works</span>
        <span style="color:#e2e8ee">mem_store</span><span style="color:#acc0d1">(</span><span style="color:#e2e8ee">p</span>, <span style="color:#cda678">42</span>, <span style="color:#98c5b2">"Int"</span><span style="color:#acc0d1">)</span>
        <span style="color:#cda678">0</span>
    <span style="font-weight:bold;color:#b3c5d8">let</span> <span style="color:#e2e8ee">v</span> = <span style="font-weight:bold;color:#d3ef73">observe</span> <span style="color:#e2e8ee">p</span>: <span style="color:#e2e8ee">mem_load</span><span style="color:#acc0d1">(</span><span style="color:#e2e8ee">p</span>, <span style="color:#98c5b2">"Int"</span><span style="color:#acc0d1">)</span>
    <span style="font-weight:bold;color:#d3ef73">decay</span> <span style="color:#e2e8ee">p</span>
    <span style="font-weight:bold;color:#b3c5d8">return</span> <span style="color:#e2e8ee">v</span>

<span style="color:#738391">// ── GPU / SHADERS ────────────────────────────────────────────────────────</span>
<span style="font-weight:bold;color:#7ea9ff">shader</span> <span style="font-weight:bold;color:#7ea9ff">compute</span> <span style="color:#c3d2ea">Kernel</span><span style="color:#acc0d1">(</span><span style="color:#e2e8ee">id</span>: <span style="color:#c3d2ea">UVec3</span><span style="color:#acc0d1">)</span> <span style="color:#acc0d1">-></span> <span style="color:#c3d2ea">Void</span> <span style="font-weight:bold;color:#7ea9ff">workgroup</span><span style="color:#acc0d1">(</span><span style="color:#cda678">8</span>,<span style="color:#cda678">1</span>,<span style="color:#cda678">1</span><span style="color:#acc0d1">)</span>:
    <span style="font-weight:bold;color:#7ea9ff">uniform</span> <span style="color:#e2e8ee">buf</span>: <span style="color:#c3d2ea">StorageBuffer</span><span style="color:#acc0d1"><</span><span style="color:#c3d2ea">UInt</span><span style="color:#acc0d1">></span> <span style="font-weight:bold;color:#98a9cf">@0</span>
    <span style="color:#e2e8ee">buf</span><span style="color:#acc0d1">[</span><span style="color:#e2e8ee">id</span><span style="color:#acc0d1">.</span><span style="color:#e2e8ee">x</span><span style="color:#acc0d1">]</span> = <span style="color:#e2e8ee">buf</span><span style="color:#acc0d1">[</span><span style="color:#e2e8ee">id</span><span style="color:#acc0d1">.</span><span style="color:#e2e8ee">x</span><span style="color:#acc0d1">]</span> <span style="color:#acc0d1">+</span> <span style="color:#cda678">1</span>

<span style="color:#738391">// ── FFI ──────────────────────────────────────────────────────────────────</span>
<span style="color:#738391">// include <windows.h> as win</span>
</div>

---

## 🔴 The Problem — What's Broken

The current lattice families were organized around **keyword domain** — "these words are about actors, these are about worlds, these are about shaders." But the decision ladder is about **semantic layer** — how high up the compiler-owned stack the construct sits. These two axes don't align:

| Lattice Family | Contains Keywords From | Layers Covered |
|---------------|----------------------|----------------|
| `family.world` (#58d0c3 teal) | `world` `entangle` `state` `surface` `patch` `law` `pulse` `resonate` `shatter` `teleport` `single_writer` `dampen` | **L1, L2, L5, L6** — four different layers, one color |
| `family.proof` (#e3cb70 gold) | `converge` `orchestrate` `axiom` `spec` `fast` `verify` `random` `when` `target` `capability` `guarantee` `fallback` `every` `jitter` `from` `to` `via` `stage` | **L3, L4, L5, L6** — four layers, clauses from L5+L6 mixed in |
| `family.actor` (#e18a75 salmon) | `actor` `spawn` `send` `on` `receive` `emit` | **L7** — actually correct, just one layer |
| `family.ownership` (#d3ef73 lime) | `collapse` `observe` `decay` `share` `weak` | **L7** — correct |
| `keyword.core` (#b3c5d8 blue-gray) | `fn` `let` `if` `match` `return` ... | **L0** — correct |
| `keyword.type` (#8fc2e8 sky blue) | `type` `struct` `enum` `trait` `impl` | **L0** — correct |
| `keyword.effect` (#d6b46f gold) | `Pure` `IO` `Async` `GPU` `Reactive` `Unsafe` | **Effect system** — correct |
| `family.shader` (#7ea9ff blue) | `shader` `vertex` `fragment` `compute` `component` `render` `comptime` `macro` `test` `fanout` | **GPU/UI** — mostly correct, but `fanout` is L7 ownership and `test` is tooling |

**Root cause:** Pulse, resonate, shatter, teleport, axiom, and their clauses were classified into World/Proof because those were the only "weird keyword" buckets available. Nobody had defined L5 (Temporal) or L6 (Machine Stones) as distinct visual categories.

---

## 🟢 Proposal — Layer-Based Palette (L0→L7)

Each layer gets a **distinct hue region**. The visual hierarchy matches the semantic hierarchy. Reading a file, you can instantly see which layers are in play by the color density.

### Layer → Hue Mapping

<table>
<tr><th>Layer</th><th>Hue</th><th>Swatch</th><th>Rationale</th></tr>
<tr><td><b>L0 Plain Code</b></td><td>Cool gray</td><td><span style="display:inline-block;width:72px;height:18px;background:#a0adba;border-radius:2px;"></span> <code>#a0adba</code></td><td>Foundation. Not colorful — this is the "no semantic construct" fallback. Should recede visually.</td></tr>
<tr><td><b>L0 Types</b></td><td>Steel blue</td><td><span style="display:inline-block;width:72px;height:18px;background:#8bb4d0;border-radius:2px;"></span> <code>#8bb4d0</code></td><td>Type declarations are still L0 but deserve distinction from control flow.</td></tr>
<tr><td><b>L1 State Authority</b></td><td>Teal</td><td><span style="display:inline-block;width:72px;height:18px;background:#4ec9b0;border-radius:2px;"></span> <code>#4ec9b0</code></td><td>Keep the teal. It works. "Water table" — everything flows from here.</td></tr>
<tr><td><b>L2 State Integrity</b></td><td>Sea green</td><td><span style="display:inline-block;width:72px;height:18px;background:#5cb88d;border-radius:2px;"></span> <code>#5cb88d</code></td><td>Adjacent to L1 on the spectrum — "the quality gate on the water."</td></tr>
<tr><td><b>L3 Dispatch</b></td><td>Amber</td><td><span style="display:inline-block;width:72px;height:18px;background:#e0a850;border-radius:2px;"></span> <code>#e0a850</code></td><td>Selection, optimization, choice. Amber = "decision point."</td></tr>
<tr><td><b>L4 Stage Graph</b></td><td>Copper</td><td><span style="display:inline-block;width:72px;height:18px;background:#d4855e;border-radius:2px;"></span> <code>#d4855e</code></td><td>Related to L3 but warmer — "pipeline, flow between stages."</td></tr>
<tr><td><b>L5 Temporal</b></td><td>Violet</td><td><span style="display:inline-block;width:72px;height:18px;background:#b68cd4;border-radius:2px;"></span> <code>#b68cd4</code></td><td>Time, rhythm, heartbeat. Purple = the only color that isn't in nature — perfect for "compiler-owned timing."</td></tr>
<tr><td><b>L6 Machine Stones</b></td><td>Rust</td><td><span style="display:inline-block;width:72px;height:18px;background:#d47a5a;border-radius:2px;"></span> <code>#d47a5a</code></td><td>Hardware, capability, physical layout. Rust/copper = "machine truth."</td></tr>
<tr><td><b>L7a Actors</b></td><td>Salmon</td><td><span style="display:inline-block;width:72px;height:18px;background:#e87461;border-radius:2px;"></span> <code>#e87461</code></td><td>Keep the salmon. Living concurrent entities — warm, alive.</td></tr>
<tr><td><b>L7b Ownership</b></td><td>Lime</td><td><span style="display:inline-block;width:72px;height:18px;background:#c8e654;border-radius:2px;"></span> <code>#c8e654</code></td><td>Keep the lime. Raw memory lifecycle — sharp, electric.</td></tr>
<tr><td><b>Effects</b></td><td>Gold</td><td><span style="display:inline-block;width:72px;height:18px;background:#d4b050;border-radius:2px;"></span> <code>#d4b050</code></td><td>Capability gates. Gold = "permission granted."</td></tr>
<tr><td><b>GPU / Shaders</b></td><td>Blue</td><td><span style="display:inline-block;width:72px;height:18px;background:#6b9cf0;border-radius:2px;"></span> <code>#6b9cf0</code></td><td>Code that runs elsewhere. Blue = "off-CPU, on-device."</td></tr>
</table>

### Complete Keyword → Layer Assignment

```
L0 PLAIN CODE        gray      fn let mut var const if else elif match for while loop
  (core keywords)              break continue return await in with as pub mod use
                               self Self true false none and or defer

L0 TYPE SYSTEM       steel     type struct enum trait impl where

L1 STATE AUTHORITY   teal      world entangle surface single_writer

L2 STATE INTEGRITY   sea-green patch law

L3 DISPATCH          amber     converge spec fast verify random

L4 STAGE GRAPH       copper    orchestrate stage

L5 TEMPORAL          violet    pulse resonate every jitter dampen

L6 MACHINE STONES    rust      axiom shatter teleport
  (stone clauses)    rust      when target arch capability guarantee fallback from to via

L7a ACTORS           salmon    actor spawn send on receive emit

L7b OWNERSHIP        lime      collapse observe decay share weak fanout

EFFECTS              gold      Pure IO async Async GPU Reactive Unsafe

GPU / SHADER / UI    blue      shader vertex fragment compute uniform workgroup
                               component render comptime macro test
```

### Hue Palette — Visual Spectrum

```
L0 core     ████████████████████████████████████████  gray       #a0adba
L0 type     ████████████████████████████████████████  steel      #8bb4d0
L1 state    ████████████████████████████████████████  teal       #4ec9b0
L2 integrity████████████████████████████████████████  sea-green  #5cb88d
L3 dispatch ████████████████████████████████████████  amber      #e0a850
L4 stage    ████████████████████████████████████████  copper     #d4855e
L5 temporal ████████████████████████████████████████  violet     #b68cd4
L6 machine  ████████████████████████████████████████  rust       #d47a5a
L7a actors  ████████████████████████████████████████  salmon     #e87461
L7b owner   ████████████████████████████████████████  lime       #c8e654
effects     ████████████████████████████████████████  gold       #d4b050
gpu/shader  ████████████████████████████████████████  blue       #6b9cf0
```

**12 distinct hues.** Zero blue dominance. Every layer immediately identifiable.

---

## Live Preview — Proposed Layer-Based Palette

<div style="background:#0d1218;color:#e2e8ee;font-family:'Cascadia Code','JetBrains Mono','Fira Code',Consolas,monospace;font-size:13px;line-height:1.55;padding:20px;border-radius:6px;white-space:pre;overflow-x:auto;">

<span style="color:#738391">// ============================================================================</span>
<span style="color:#738391">//  KAIN SYNTAX PREVIEW — Proposed Layer-Based Palette</span>
<span style="color:#738391">//  Each layer has a distinct hue. The visual hierarchy IS the semantic hierarchy.</span>
<span style="color:#738391">// ============================================================================</span>

<span style="font-weight:bold;color:#a0adba">use</span> <span style="color:#c3d2ea">std::runtime</span>
<span style="font-weight:bold;color:#a0adba">use</span> <span style="color:#c3d2ea">std::actor</span>
<span style="font-weight:bold;color:#a0adba">use</span> <span style="color:#c3d2ea">std::math</span>

<span style="color:#738391">// ═══════════════════════════════════════════════════════════════════════════</span>
<span style="color:#738391">//  L0: PLAIN CODE — gray / steel blue</span>
<span style="color:#738391">//  The fallback. Use only when no L1-L7 construct fits.</span>
<span style="color:#738391">// ═══════════════════════════════════════════════════════════════════════════</span>

<span style="font-weight:bold;color:#a0adba">const</span> <span style="color:#c3d2ea">MAX</span>: <span style="color:#c3d2ea">Int</span> = <span style="color:#cda678">1000</span>
<span style="font-weight:bold;color:#8bb4d0">type</span> <span style="color:#c3d2ea">Score</span> = <span style="color:#c3d2ea">Int</span>

<span style="font-weight:bold;color:#8bb4d0">struct</span> <span style="color:#c3d2ea">Packet</span>:
    <span style="color:#e2e8ee">id</span>: <span style="color:#c3d2ea">Int</span>
    <span style="color:#e2e8ee">payload</span>: <span style="color:#c3d2ea">Int</span>

<span style="font-weight:bold;color:#8bb4d0">enum</span> <span style="color:#c3d2ea">Mode</span>:
    <span style="color:#c3d2ea">Scalar</span>
    <span style="color:#c3d2ea">Vectorized</span>

<span style="font-weight:bold;color:#a0adba">fn</span> <span style="color:#e2e8ee">process</span><span style="color:#acc0d1">(</span><span style="color:#e2e8ee">p</span>: <span style="color:#c3d2ea">Packet</span><span style="color:#acc0d1">)</span> <span style="color:#acc0d1">-></span> <span style="color:#c3d2ea">Int</span>:
    <span style="font-weight:bold;color:#a0adba">if</span> <span style="color:#e2e8ee">p</span><span style="color:#acc0d1">.</span><span style="color:#e2e8ee">id</span> <span style="color:#acc0d1">></span> <span style="color:#cda678">0</span>:
        <span style="font-weight:bold;color:#a0adba">return</span> <span style="color:#e2e8ee">p</span><span style="color:#acc0d1">.</span><span style="color:#e2e8ee">payload</span> <span style="color:#acc0d1">*</span> <span style="color:#cda678">2</span>
    <span style="font-weight:bold;color:#a0adba">return</span> <span style="color:#cda678">0</span>

<span style="color:#738391">// ═══════════════════════════════════════════════════════════════════════════</span>
<span style="color:#738391">//  L1+L2: STATE AUTHORITY + INTEGRITY — teal / sea-green</span>
<span style="color:#738391">//  world = compiler-owned state. patch = journaled mutation. law = invariant.</span>
<span style="color:#738391">// ═══════════════════════════════════════════════════════════════════════════</span>

<span style="font-weight:bold;color:#4ec9b0">world</span> <span style="color:#c3d2ea">AppState</span>:
    <span style="font-weight:bold;color:#4ec9b0">state</span> <span style="color:#e2e8ee">counter</span>: <span style="color:#c3d2ea">Int</span> = <span style="color:#cda678">0</span>
    <span style="font-weight:bold;color:#4ec9b0">state</span> <span style="color:#e2e8ee">last_signal</span>: <span style="color:#c3d2ea">Int</span> = <span style="color:#cda678">0</span>

<span style="font-weight:bold;color:#4ec9b0">world</span> <span style="color:#c3d2ea">Mirror</span>:
    <span style="font-weight:bold;color:#4ec9b0">state</span> <span style="color:#e2e8ee">counter_copy</span>: <span style="color:#c3d2ea">Int</span> = <span style="color:#cda678">0</span>

<span style="font-weight:bold;color:#4ec9b0">entangle</span> <span style="color:#c3d2ea">AppState</span><span style="color:#acc0d1">.</span><span style="color:#e2e8ee">counter</span> <span style="color:#acc0d1"><-></span> <span style="color:#c3d2ea">Mirror</span><span style="color:#acc0d1">.</span><span style="color:#e2e8ee">counter_copy</span> <span style="font-weight:bold;color:#4ec9b0">with</span> <span style="font-weight:bold;color:#4ec9b0">single_writer</span>

<span style="font-weight:bold;color:#5cb88d">law</span> <span style="color:#e2e8ee">counter_valid</span><span style="color:#acc0d1">(</span><span style="color:#e2e8ee">v</span>: <span style="color:#c3d2ea">Int</span><span style="color:#acc0d1">)</span> <span style="color:#acc0d1">-></span> <span style="color:#c3d2ea">Bool</span>:
    <span style="font-weight:bold;color:#a0adba">return</span> <span style="color:#e2e8ee">v</span> <span style="color:#acc0d1">>=</span> <span style="color:#cda678">0</span>

<span style="font-weight:bold;color:#5cb88d">patch</span> <span style="color:#e2e8ee">increment</span><span style="color:#acc0d1">(</span><span style="color:#e2e8ee">s</span>: <span style="color:#c3d2ea">AppState</span><span style="color:#acc0d1">)</span> <span style="color:#acc0d1">-></span> <span style="color:#c3d2ea">Int</span>:
    <span style="color:#e2e8ee">s</span><span style="color:#acc0d1">.</span><span style="color:#e2e8ee">counter</span> = <span style="color:#e2e8ee">s</span><span style="color:#acc0d1">.</span><span style="color:#e2e8ee">counter</span> <span style="color:#acc0d1">+</span> <span style="color:#cda678">1</span>
    <span style="font-weight:bold;color:#a0adba">return</span> <span style="color:#e2e8ee">s</span><span style="color:#acc0d1">.</span><span style="color:#e2e8ee">counter</span>

<span style="color:#738391">// ═══════════════════════════════════════════════════════════════════════════</span>
<span style="color:#738391">//  L3: DISPATCH — amber</span>
<span style="color:#738391">//  converge = spec + platform-gated fast lanes with fuzzing.</span>
<span style="color:#738391">// ═══════════════════════════════════════════════════════════════════════════</span>

<span style="font-weight:bold;color:#e0a850">converge</span> <span style="color:#e2e8ee">fast_hash</span><span style="color:#acc0d1">(</span><span style="color:#e2e8ee">v</span>: <span style="color:#c3d2ea">Int</span><span style="color:#acc0d1">)</span> <span style="color:#acc0d1">-></span> <span style="color:#c3d2ea">Int</span>:
    <span style="font-weight:bold;color:#e0a850">spec</span> <span style="color:#e2e8ee">reference</span>:
        <span style="font-weight:bold;color:#a0adba">return</span> <span style="color:#e2e8ee">v</span> <span style="color:#acc0d1">*</span> <span style="color:#cda678">31</span> <span style="color:#acc0d1">+</span> <span style="color:#cda678">7</span>
    <span style="font-weight:bold;color:#e0a850">fast</span> <span style="color:#e2e8ee">simd_lane</span> <span style="font-weight:bold;color:#d47a5a">when</span> <span style="font-weight:bold;color:#d47a5a">capability</span><span style="color:#acc0d1">(</span><span style="color:#98c5b2">"cpu.avx2"</span><span style="color:#acc0d1">)</span>:
        <span style="font-weight:bold;color:#a0adba">return</span> <span style="color:#e2e8ee">v</span> <span style="color:#acc0d1">*</span> <span style="color:#cda678">48</span> <span style="color:#acc0d1">+</span> <span style="color:#cda678">14</span>
    <span style="font-weight:bold;color:#e0a850">verify</span> <span style="font-weight:bold;color:#e0a850">random</span><span style="color:#acc0d1">(</span><span style="color:#cda678">8</span><span style="color:#acc0d1">)</span>

<span style="color:#738391">// ═══════════════════════════════════════════════════════════════════════════</span>
<span style="color:#738391">//  L4: STAGE GRAPH — copper</span>
<span style="color:#738391">//  orchestrate = typed multi-runtime pipeline.</span>
<span style="color:#738391">// ═══════════════════════════════════════════════════════════════════════════</span>

<span style="font-weight:bold;color:#d4855e">orchestrate</span> <span style="color:#e2e8ee">pipeline</span><span style="color:#acc0d1">(</span><span style="color:#e2e8ee">v</span>: <span style="color:#c3d2ea">Int</span><span style="color:#acc0d1">)</span> <span style="color:#acc0d1">-></span> <span style="color:#c3d2ea">Int</span>:
    <span style="font-weight:bold;color:#d4855e">stage</span> <span style="color:#e2e8ee">a</span>: <span style="color:#e2e8ee">cpu</span> <span style="color:#e2e8ee">process</span><span style="color:#acc0d1">(</span><span style="color:#e2e8ee">v</span><span style="color:#acc0d1">)</span>
    <span style="font-weight:bold;color:#d4855e">stage</span> <span style="color:#e2e8ee">b</span>: <span style="font-weight:bold;color:#e0a850">converge</span> <span style="color:#e2e8ee">fast_hash</span><span style="color:#acc0d1">(</span><span style="color:#e2e8ee">a</span><span style="color:#acc0d1">)</span>
    <span style="font-weight:bold;color:#a0adba">return</span> <span style="color:#e2e8ee">b</span>

<span style="color:#738391">// ═══════════════════════════════════════════════════════════════════════════</span>
<span style="color:#738391">//  L5: TEMPORAL — violet</span>
<span style="color:#738391">//  pulse = timed recurrence. resonate = reactive tripwire.</span>
<span style="color:#738391">//  Now unmistakably its own layer — no longer dumped into World teal.</span>
<span style="color:#738391">// ═══════════════════════════════════════════════════════════════════════════</span>

<span style="font-weight:bold;color:#b68cd4">resonate</span> <span style="color:#c3d2ea">AppState</span><span style="color:#acc0d1">.</span><span style="color:#e2e8ee">counter</span> <span style="font-weight:bold;color:#b68cd4">dampen</span> <span style="color:#cda678">10</span> <span style="color:#e2e8ee">ms</span>:
    <span style="color:#c3d2ea">AppState</span><span style="color:#acc0d1">.</span><span style="color:#e2e8ee">last_signal</span> = <span style="color:#e2e8ee">resonate_new_i64</span>

<span style="font-weight:bold;color:#b68cd4">pulse</span> <span style="color:#e2e8ee">heartbeat</span> <span style="font-weight:bold;color:#b68cd4">every</span> <span style="color:#cda678">16</span> <span style="color:#e2e8ee">ms</span> <span style="font-weight:bold;color:#b68cd4">jitter</span> <span style="color:#cda678">2</span> <span style="color:#e2e8ee">ms</span>:
    <span style="color:#c3d2ea">AppState</span><span style="color:#acc0d1">.</span><span style="color:#e2e8ee">counter</span> = <span style="color:#c3d2ea">AppState</span><span style="color:#acc0d1">.</span><span style="color:#e2e8ee">counter</span> <span style="color:#acc0d1">+</span> <span style="color:#cda678">1</span>

<span style="color:#738391">// ═══════════════════════════════════════════════════════════════════════════</span>
<span style="color:#738391">//  L6: MACHINE STONES — rust</span>
<span style="color:#738391">//  axiom = capability assumption. shatter = SoA layout. teleport = zero-copy.</span>
<span style="color:#738391">//  All stone keywords + clauses now share one color.</span>
<span style="color:#738391">// ═══════════════════════════════════════════════════════════════════════════</span>

<span style="font-weight:bold;color:#d47a5a">shatter</span> <span style="font-weight:bold;color:#8bb4d0">struct</span> <span style="color:#c3d2ea">Shard</span>:
    <span style="color:#e2e8ee">bias</span>: <span style="color:#c3d2ea">Int</span>
    <span style="color:#e2e8ee">phase</span>: <span style="color:#c3d2ea">Int</span>

<span style="font-weight:bold;color:#d47a5a">axiom</span> <span style="color:#e2e8ee">machine_ok</span>:
    <span style="font-weight:bold;color:#d47a5a">when</span> <span style="font-weight:bold;color:#d47a5a">target</span><span style="color:#acc0d1">(</span><span style="color:#98c5b2">"llvm"</span><span style="color:#acc0d1">)</span>
    <span style="font-weight:bold;color:#d47a5a">when</span> <span style="font-weight:bold;color:#d47a5a">capability</span><span style="color:#acc0d1">(</span><span style="color:#98c5b2">"memory.shatter"</span><span style="color:#acc0d1">)</span>
    <span style="font-weight:bold;color:#d47a5a">guarantee</span> <span style="color:#98c5b2">"machine supports shatter layout"</span>
    <span style="font-weight:bold;color:#d47a5a">fallback</span> <span style="color:#e2e8ee">fallback_fn</span>

<span style="font-weight:bold;color:#a0adba">let</span> <span style="color:#e2e8ee">moved</span> = <span style="font-weight:bold;color:#d47a5a">teleport</span> <span style="color:#e2e8ee">shard</span> <span style="font-weight:bold;color:#d47a5a">from</span> <span style="color:#c3d2ea">AppState</span> <span style="font-weight:bold;color:#d47a5a">to</span> <span style="color:#c3d2ea">Mirror</span> <span style="font-weight:bold;color:#d47a5a">via</span> <span style="color:#e2e8ee">bus</span>

<span style="color:#738391">// ═══════════════════════════════════════════════════════════════════════════</span>
<span style="color:#738391">//  L7a: ACTORS — salmon</span>
<span style="color:#738391">//  actor = message-oriented concurrent entity. spawn/send/on.</span>
<span style="color:#738391">// ═══════════════════════════════════════════════════════════════════════════</span>

<span style="font-weight:bold;color:#e87461">actor</span> <span style="color:#c3d2ea">Worker</span>:
    <span style="font-weight:bold;color:#4ec9b0">state</span> <span style="color:#e2e8ee">ticks</span>: <span style="color:#c3d2ea">Int</span> = <span style="color:#cda678">0</span>
    <span style="font-weight:bold;color:#e87461">on</span> <span style="color:#c3d2ea">Tick</span><span style="color:#acc0d1">(</span><span style="color:#e2e8ee">n</span>: <span style="color:#c3d2ea">Int</span><span style="color:#acc0d1">)</span>:
        <span style="color:#e2e8ee">self</span><span style="color:#acc0d1">.</span><span style="color:#e2e8ee">ticks</span> = <span style="color:#e2e8ee">self</span><span style="color:#acc0d1">.</span><span style="color:#e2e8ee">ticks</span> <span style="color:#acc0d1">+</span> <span style="color:#e2e8ee">n</span>

<span style="color:#738391">// ═══════════════════════════════════════════════════════════════════════════</span>
<span style="color:#738391">//  L7b: OWNERSHIP — lime</span>
<span style="color:#738391">//  collapse/observe/decay = raw pointer state machine.</span>
<span style="color:#738391">// ═══════════════════════════════════════════════════════════════════════════</span>

<span style="font-weight:bold;color:#a0adba">fn</span> <span style="color:#e2e8ee">raw_ops</span><span style="color:#acc0d1">()</span> <span style="color:#acc0d1">-></span> <span style="color:#c3d2ea">Int</span> <span style="font-weight:bold;color:#a0adba">with</span> <span style="font-weight:bold;color:#d4b050">Unsafe</span>:
    <span style="font-weight:bold;color:#a0adba">let</span> <span style="font-weight:bold;color:#a0adba">mut</span> <span style="color:#e2e8ee">p</span>: <span style="color:#e2e8ee">ptr</span><span style="color:#acc0d1"><</span><span style="color:#c3d2ea">Int</span><span style="color:#acc0d1">></span> = <span style="color:#e2e8ee">alloc_zeroed</span><span style="color:#acc0d1">(</span><span style="color:#cda678">1</span>, <span style="color:#98c5b2">"Int"</span><span style="color:#acc0d1">)</span>
    <span style="font-weight:bold;color:#c8e654">collapse</span> <span style="color:#e2e8ee">p</span>:
        <span style="color:#e2e8ee">mem_store</span><span style="color:#acc0d1">(</span><span style="color:#e2e8ee">p</span>, <span style="color:#cda678">42</span>, <span style="color:#98c5b2">"Int"</span><span style="color:#acc0d1">)</span>
        <span style="color:#cda678">0</span>
    <span style="font-weight:bold;color:#a0adba">let</span> <span style="color:#e2e8ee">v</span> = <span style="font-weight:bold;color:#c8e654">observe</span> <span style="color:#e2e8ee">p</span>: <span style="color:#e2e8ee">mem_load</span><span style="color:#acc0d1">(</span><span style="color:#e2e8ee">p</span>, <span style="color:#98c5b2">"Int"</span><span style="color:#acc0d1">)</span>
    <span style="font-weight:bold;color:#c8e654">decay</span> <span style="color:#e2e8ee">p</span>
    <span style="font-weight:bold;color:#a0adba">return</span> <span style="color:#e2e8ee">v</span>

<span style="color:#738391">// ═══════════════════════════════════════════════════════════════════════════</span>
<span style="color:#738391">//  GPU / SHADERS — blue</span>
<span style="color:#738391">//  shader vertex/fragment/compute, component, render, comptime, macro.</span>
<span style="color:#738391">// ═══════════════════════════════════════════════════════════════════════════</span>

<span style="font-weight:bold;color:#6b9cf0">shader</span> <span style="font-weight:bold;color:#6b9cf0">compute</span> <span style="color:#c3d2ea">Kernel</span><span style="color:#acc0d1">(</span><span style="color:#e2e8ee">id</span>: <span style="color:#c3d2ea">UVec3</span><span style="color:#acc0d1">)</span> <span style="color:#acc0d1">-></span> <span style="color:#c3d2ea">Void</span> <span style="font-weight:bold;color:#6b9cf0">workgroup</span><span style="color:#acc0d1">(</span><span style="color:#cda678">8</span>,<span style="color:#cda678">1</span>,<span style="color:#cda678">1</span><span style="color:#acc0d1">)</span>:
    <span style="font-weight:bold;color:#6b9cf0">uniform</span> <span style="color:#e2e8ee">buf</span>: <span style="color:#c3d2ea">StorageBuffer</span><span style="color:#acc0d1"><</span><span style="color:#c3d2ea">UInt</span><span style="color:#acc0d1">></span> <span style="font-weight:bold;color:#98a9cf">@0</span>
    <span style="color:#e2e8ee">buf</span><span style="color:#acc0d1">[</span><span style="color:#e2e8ee">id</span><span style="color:#acc0d1">.</span><span style="color:#e2e8ee">x</span><span style="color:#acc0d1">]</span> = <span style="color:#e2e8ee">buf</span><span style="color:#acc0d1">[</span><span style="color:#e2e8ee">id</span><span style="color:#acc0d1">.</span><span style="color:#e2e8ee">x</span><span style="color:#acc0d1">]</span> <span style="color:#acc0d1">+</span> <span style="color:#cda678">1</span>

<span style="color:#738391">// ═══════════════════════════════════════════════════════════════════════════</span>
<span style="color:#738391">//  PROGRAM ENTRY — the full spectrum in one file</span>
<span style="color:#738391">// ═══════════════════════════════════════════════════════════════════════════</span>

<span style="font-weight:bold;color:#a0adba">fn</span> <span style="color:#e2e8ee">main</span><span style="color:#acc0d1">()</span> <span style="color:#acc0d1">-></span> <span style="color:#c3d2ea">Int</span> <span style="font-weight:bold;color:#a0adba">with</span> <span style="font-weight:bold;color:#d4b050">Unsafe</span>:
    <span style="color:#738391">// L0: plain       → gray</span>
    <span style="font-weight:bold;color:#a0adba">let</span> <span style="color:#e2e8ee">x</span> = <span style="color:#cda678">42</span>

    <span style="color:#738391">// L1+L2: state    → teal + sea-green</span>
    <span style="font-weight:bold;color:#a0adba">let</span> <span style="color:#e2e8ee">_</span> = <span style="color:#e2e8ee">increment</span><span style="color:#acc0d1">(</span><span style="color:#c3d2ea">AppState</span><span style="color:#acc0d1">)</span>

    <span style="color:#738391">// L3: dispatch    → amber</span>
    <span style="font-weight:bold;color:#a0adba">let</span> <span style="color:#e2e8ee">h</span> = <span style="color:#e2e8ee">fast_hash</span><span style="color:#acc0d1">(</span><span style="color:#e2e8ee">x</span><span style="color:#acc0d1">)</span>

    <span style="color:#738391">// L4: stage graph → copper</span>
    <span style="font-weight:bold;color:#a0adba">let</span> <span style="color:#e2e8ee">r</span> = <span style="color:#e2e8ee">pipeline</span><span style="color:#acc0d1">(</span><span style="color:#e2e8ee">h</span><span style="color:#acc0d1">)</span>

    <span style="color:#738391">// L6: machine    → rust (teleport + from/to/via all same color)</span>
    <span style="font-weight:bold;color:#a0adba">let</span> <span style="color:#e2e8ee">s</span> = <span style="color:#c3d2ea">Shard</span> <span style="color:#acc0d1">{</span> <span style="color:#e2e8ee">bias</span>: <span style="color:#cda678">1</span>, <span style="color:#e2e8ee">phase</span>: <span style="color:#cda678">2</span> <span style="color:#acc0d1">}</span>
    <span style="font-weight:bold;color:#a0adba">let</span> <span style="color:#e2e8ee">moved</span> = <span style="font-weight:bold;color:#d47a5a">teleport</span> <span style="color:#e2e8ee">s</span> <span style="font-weight:bold;color:#d47a5a">from</span> <span style="color:#c3d2ea">AppState</span> <span style="font-weight:bold;color:#d47a5a">to</span> <span style="color:#c3d2ea">Mirror</span> <span style="font-weight:bold;color:#d47a5a">via</span> <span style="color:#e2e8ee">bus</span>

    <span style="color:#738391">// L7a: actors    → salmon</span>
    <span style="font-weight:bold;color:#a0adba">let</span> <span style="color:#e2e8ee">w</span> = <span style="font-weight:bold;color:#e87461">spawn</span> <span style="color:#c3d2ea">Worker</span><span style="color:#acc0d1">()</span>
    <span style="font-weight:bold;color:#e87461">send</span> <span style="color:#e2e8ee">w</span><span style="color:#acc0d1">.</span><span style="color:#c3d2ea">Tick</span><span style="color:#acc0d1">(</span><span style="color:#e2e8ee">n</span> = <span style="color:#cda678">1</span><span style="color:#acc0d1">)</span>

    <span style="font-weight:bold;color:#a0adba">return</span> <span style="color:#e2e8ee">r</span>
</div>

---

## What Changed — Side by Side

| Construct | Old (Lattice Family) | Old Color | New (Layer) | New Color |
|-----------|---------------------|-----------|-------------|-----------|
| `pulse` `resonate` `dampen` | World | teal `#58d0c3` | **L5 Temporal** | violet `#b68cd4` |
| `every` `jitter` | Proof | gold `#e3cb70` | **L5 Temporal** | violet `#b68cd4` |
| `shatter` `teleport` | World | teal `#58d0c3` | **L6 Machine** | rust `#d47a5a` |
| `axiom` `guarantee` `fallback` | Proof | gold `#e3cb70` | **L6 Machine** | rust `#d47a5a` |
| `when` `target` `capability` `arch` | Proof | gold `#e3cb70` | **L6 Machine** | rust `#d47a5a` |
| `from` `to` `via` | Proof | gold `#e3cb70` | **L6 Machine** | rust `#d47a5a` |
| `patch` `law` | World | teal `#58d0c3` | **L2 Integrity** | sea-green `#5cb88d` |
| `converge` `spec` `fast` `verify` `random` | Proof | gold `#e3cb70` | **L3 Dispatch** | amber `#e0a850` |
| `orchestrate` `stage` | Proof | gold `#e3cb70` | **L4 Stage Graph** | copper `#d4855e` |
| `world` `entangle` `state` `surface` `single_writer` | World | teal `#4ec9b0` | **L1 Authority** | teal `#4ec9b0` (kept) |
| `fn` `let` `if` `match` `return` ... | Core | blue-gray `#b3c5d8` | **L0 Core** | gray `#a0adba` (desaturated) |
| `type` `struct` `enum` `trait` `impl` | Type | sky blue `#8fc2e8` | **L0 Type** | steel `#8bb4d0` (desaturated) |
| `actor` `spawn` `send` `on` | Actor | salmon `#e18a75` | **L7a Actor** | salmon `#e87461` (kept) |
| `collapse` `observe` `decay` `share` `weak` | Ownership | lime `#d3ef73` | **L7b Ownership** | lime `#c8e654` (kept) |
| `Pure` `IO` `Async` `GPU` `Reactive` `Unsafe` | Effect | gold `#d6b46f` | **Effects** | gold `#d4b050` (kept) |
| `shader` `vertex` `fragment` `compute` `component` `render` ... | Shader | blue `#7ea9ff` | **GPU/Shader** | blue `#6b9cf0` (kept) |

---

## Implementation Note

The classifier in `crates/lattice/src/lib.rs` maps words → `KeywordFamily` → `SemanticRole` → color. To implement this proposal:

1. **Replace the 8 `KeywordFamily` variants** with layer-based variants: `Layer0Core`, `Layer0Type`, `Layer1Authority`, `Layer2Integrity`, `Layer3Dispatch`, `Layer4Stage`, `Layer5Temporal`, `Layer6Machine`, `Layer7Actor`, `Layer7Ownership`, `Effect`, `GpuShader`
2. **Add 4 new `SemanticRole` variants** (currently 16 syntax roles, need ~20): split `SyntaxFamilyWorld` into L1/L2/L5/L6, split `SyntaxFamilyProof` into L3/L4/L5 clauses/L6 clauses
3. **Update `lattice.toml`** with the new role keys and hex values
4. The `highlight_source_line()` engine doesn't change — it's purely driven by the classification table

The Rust side is a 30-minute change. The TOML side is color-picking.

---

## Open Questions

1. **`state` keyword** — currently World teal. Inside an `actor` block, should `state` remain teal (it's actor-local state, not world state) or shift to salmon? Leaning: keep teal, `state` always means "compiler-tracked mutable field" regardless of parent construct.

2. **`fanout`** — currently Shader blue. It's actually an L7 ownership construct (parallel write lanes on disjoint pointer regions). Should move to lime.

3. **`test`** — currently Shader blue. It's tooling, not GPU. Probably belongs in a `tooling` role or just L0 gray.

4. **Thin blue line problem** — L0 core (gray) + L0 type (steel) are adjacent hues. Will they be distinguishable at small font sizes? May need to push type slightly more blue or core slightly more neutral.

5. **Accessibility** — 12 hues is a lot. Need to ensure adjacent layers (amber/copper, teal/sea-green, rust/salmon) are distinguishable for color-blind users. The bold modifier on all keywords helps, but a deuteranopia simulation pass is warranted.

---

*Current lattice source: `crates/lattice/lattice.toml` + `crates/lattice/src/lib.rs`*  
*Proposed rework: replace `KeywordFamily` enum + add `SemanticRole` variants + update TOML hex values*  
*No parser/typechecker changes needed — purely a presentation-layer change.*
