#!/usr/bin/env python3
"""Generate service key state CSV."""
import os, struct

def rotl64(v, s):
    return ((v << (s & 63)) | (v >> (64 - (s & 63)))) & 0xFFFFFFFFFFFFFFFF

def hash_state(w0, w1, w2, w3, length):
    magic = 0x64170d358aa115a1
    lane1 = 0x9e3779b97f4a7c15
    lane2 = 0xbf58476d1ce4e5b9
    lane3 = 0x94d049bb133111eb
    lane4 = 0xd6e8feb86659fd93
    f0 = ((w0 ^ length) * magic) & 0xFFFFFFFFFFFFFFFF
    f1 = ((w1 ^ rotl64(magic, 13)) * lane1) & 0xFFFFFFFFFFFFFFFF
    f2 = ((w2 ^ rotl64(magic, 27)) * lane2) & 0xFFFFFFFFFFFFFFFF
    f3 = ((w3 ^ (magic ^ lane3)) * lane4) & 0xFFFFFFFFFFFFFFFF
    s = (f0 ^ f1 ^ f2 ^ f3) & 0xFFFFFFFFFFFFFFFF
    r = (((s ^ (s >> 33)) * 0xff51afd7ed558ccd) & 0xFFFFFFFFFFFFFFFF) ^ (s >> 29)
    return r & 0xFFFFFFFFFFFFFFFF

def compute_key_state(key_str):
    kb = key_str.encode('ascii')
    kl = len(kb)
    pl = min(kl, 32)
    fd = bytearray(32)
    for i in range(pl):
        b = kb[i]
        fd[i] = b + 32 if 65 <= b <= 90 else b
    w0 = struct.unpack('<Q', fd[0:8])[0]
    w1 = struct.unpack('<Q', fd[8:16])[0]
    w2 = struct.unpack('<Q', fd[16:24])[0]
    w3 = struct.unpack('<Q', fd[24:32])[0]
    return hash_state(w0, w1, w2, w3, kl)

KEYS = [
    ('base.memory', 'KAIN_SERVICE_KEY_BASE_MEMORY'),
    ('memory.ownership', 'KAIN_SERVICE_KEY_MEMORY_OWNERSHIP'),
    ('base.diagnostics', 'KAIN_SERVICE_KEY_BASE_DIAGNOSTICS'),
    ('contract', 'KAIN_SERVICE_KEY_CONTRACT'),
    ('reflection', 'KAIN_SERVICE_KEY_REFLECTION'),
    ('actor.runtime', 'KAIN_SERVICE_KEY_ACTOR_RUNTIME'),
    ('actor.registry', 'KAIN_SERVICE_KEY_ACTOR_REGISTRY'),
    ('async.runtime', 'KAIN_SERVICE_KEY_ASYNC_RUNTIME'),
    ('async.timers', 'KAIN_SERVICE_KEY_ASYNC_TIMERS'),
    ('io.net', 'KAIN_SERVICE_KEY_IO_NET'),
    ('io.process', 'KAIN_SERVICE_KEY_IO_PROCESS'),
    ('audio.device', 'KAIN_SERVICE_KEY_AUDIO_DEVICE'),
    ('audio.midi', 'KAIN_SERVICE_KEY_AUDIO_MIDI'),
    ('platform.app-host', 'KAIN_SERVICE_KEY_PLATFORM_APP_HOST'),
    ('platform.input', 'KAIN_SERVICE_KEY_PLATFORM_INPUT'),
    ('gfx.viewport', 'KAIN_SERVICE_KEY_GFX_VIEWPORT'),
    ('gfx.raw-native', 'KAIN_SERVICE_KEY_GFX_RAW_NATIVE'),
    ('gfx.backend.vulkan', 'KAIN_SERVICE_KEY_GFX_BACKEND_VULKAN'),
    ('gfx.backend.d3d12', 'KAIN_SERVICE_KEY_GFX_BACKEND_D3D12'),
    ('gfx.shader.spirv', 'KAIN_SERVICE_KEY_GFX_SHADER_SPIRV'),
    ('gfx.compute', 'KAIN_SERVICE_KEY_GFX_COMPUTE'),
    ('scene.runtime', 'KAIN_SERVICE_KEY_SCENE_RUNTIME'),
    ('scene.query', 'KAIN_SERVICE_KEY_SCENE_QUERY'),
    ('scene.mutation', 'KAIN_SERVICE_KEY_SCENE_MUTATION'),
    ('runtime.inspection', 'KAIN_SERVICE_KEY_RUNTIME_INSPECTION'),
    ('device.reflection', 'KAIN_SERVICE_KEY_DEVICE_REFLECTION'),
    ('ui.bundle', 'KAIN_SERVICE_KEY_UI_BUNDLE'),
    ('ui.component', 'KAIN_SERVICE_KEY_UI_COMPONENT'),
    ('asset.gltf', 'KAIN_SERVICE_KEY_ASSET_GLTF'),
    ('asset.ingestion', 'KAIN_SERVICE_KEY_ASSET_INGESTION'),
    ('asset.realtime', 'KAIN_SERVICE_KEY_ASSET_REALTIME'),
    ('host.bridge', 'KAIN_SERVICE_KEY_HOST_BRIDGE'),
    ('compatibility', 'KAIN_SERVICE_KEY_COMPATIBILITY'),
]

ALIASES = [
    ('native.app-host', 'platform.app-host'),
    ('native.input', 'platform.input'),
    ('native.viewport', 'gfx.viewport'),
    ('native.graphics', 'gfx.raw-native'),
    ('native.scene', 'scene.runtime'),
    ('native.scene.query', 'scene.query'),
    ('native.scene.mutation', 'scene.mutation'),
    ('native.runtime.inspection', 'runtime.inspection'),
    ('native.device.reflection', 'device.reflection'),
    ('native.asset.gltf', 'asset.gltf'),
    ('native.asset.ingestion', 'asset.ingestion'),
    ('native.ui.compiled-bundle', 'ui.bundle'),
    ('native.compute', 'gfx.compute'),
    ('native.shader.spirv', 'gfx.shader.spirv'),
    ('native.vulkan', 'gfx.backend.vulkan'),
    ('native.dx12', 'gfx.backend.d3d12'),
    ('native.d3d12', 'gfx.backend.d3d12'),
]

outdir = 'X:/runtime/native/src/core/z3/data'
os.makedirs(outdir, exist_ok=True)

with open(os.path.join(outdir, 'service_key_states.csv'), 'w') as f:
    f.write('key,type,macro,token\n')
    for key, macro in KEYS:
        state = compute_key_state(key)
        f.write('"{}","canonical","{}",0x{:016x}\n'.format(key, macro, state))
    for alias, target in ALIASES:
        state = compute_key_state(alias)
        f.write('"{}","alias ->{}","",0x{:016x}\n'.format(alias, target, state))

print('Wrote CSV: {} canonical + {} alias entries'.format(len(KEYS), len(ALIASES)))
