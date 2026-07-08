# Models — GLB/GLTF Sourcing for the Electron Rig

Place `.glb` or `.gltf` files here for `load_glb()` in `electron_rig.kn`.

## Included Models

- **`rig1.glb`** — test rig (added by user)

## Where to Get Free GLB Models

| Source | URL | License |
|--------|-----|---------|
| **Sketchfab** | https://sketchfab.com/3d-models?features=downloadable&sort_by=-date | Various (CC-* / Standard) |
| **Mixamo** | https://www.mixamo.com/ | Adobe TOS (free for most use) |
| **Ready Player Me** | https://readyplayer.me/ | Custom |
| **Quaternius** | https://quaternius.com/ | CC0 (public domain) |
| **Google Poly** | https://poly.pizza/ | CC-BY / CC0 |
| **Three.js Examples** | https://threejs.org/examples/#webgl_animation_skinning_morph | MIT |

## Exporting from Blender

1. Model your character with an armature (skeleton) + vertex groups
2. Select all meshes, then **File → Export → glTF 2.0 (.glb/.gltf)**
3. Settings:
   - **Include**: Selected Objects ✓
   - **Transform**: +Y Up ✓
   - **Geometry**: Compression (optional) ✗
   - **Animation**: All Actions ✓ (if using baked animations)
   - **Skinning**: ✓ (export skinning data for skeleton)
4. Copy the `.glb` file to `models/`
5. Reference from Kain:
   ```
   load_glb("my_char", "models/your_model.glb")
   ```

## Converting Other Formats

Use `obj2gltf` (Node.js CLI):
```
npm install -g obj2gltf
obj2gltf -i model.obj -o model.glb
```

For FBX → GLB, use Blender (File → Import → FBX → Export → glTF 2.0).

## Notes

- The renderer uses `THREE.GLTFLoader` for loading.
- Draco-compressed GLBs are NOT supported in the initial version (add `DRACOLoader` if needed).
- Models with animation clips (`AnimationClip` array) are supported via `play_animation()`.
- Models without a skeleton (static meshes) can still be loaded but the rig commands will have no bones to animate.
