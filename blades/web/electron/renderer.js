// =====================================================================
//  renderer.js — Kain → 3D Engine: SceneManager command interpreter
//
//  Receives JSON commands from Kain programs (via main.js IPC bridge)
//  and executes them against a Three.js scene. No hardcoded content —
//  the Kain program owns the scene graph. The renderer is a pure
//  execution engine.
//
//  COMMANDS:
//    create  — mesh, instanced, particles, lines, buffer_geo
//    update  — positions, transforms, matrices, colors, points
//    remove  — delete by id
//    clear   — wipe scene
//    camera  — reposition
//    telemetry — HUD counters
// =====================================================================

const { ipcRenderer } = require('electron');
const fs = require('fs');
const path = require('path');

(async function init() {
  const THREE = await import('three');
  const { OrbitControls } = await import('three/addons/controls/OrbitControls.js');

  // ── Scene environment ────────────────────────────────────────────
  const scene = new THREE.Scene();
  scene.background = new THREE.Color(0x000008);
  // No fog — space is infinite, black holes are big

  const camera = new THREE.PerspectiveCamera(55, window.innerWidth / window.innerHeight, 0.1, 100);
  camera.position.set(4, 4, 10);

  const renderer = new THREE.WebGLRenderer({ antialias: true, alpha: false });
  renderer.setSize(window.innerWidth, window.innerHeight);
  renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
  renderer.shadowMap.enabled = true;
  renderer.shadowMap.type = THREE.PCFSoftShadowMap;
  document.getElementById('canvas-container').appendChild(renderer.domElement);

  // Kill default drag/copy behavior on the canvas — stops blue highlight hell
  renderer.domElement.addEventListener('dragstart', (e) => e.preventDefault());
  renderer.domElement.addEventListener('selectstart', (e) => e.preventDefault());
  renderer.domElement.addEventListener('contextmenu', (e) => e.preventDefault());

  // Orbit controls
  const controls = new OrbitControls(camera, renderer.domElement);
  controls.enableDamping = true;
  controls.dampingFactor = 0.08;
  controls.target.set(0, 0, 0);


  controls.update();

  // Lights
  scene.add(new THREE.AmbientLight(0x404060, 0.6));
  const key = new THREE.DirectionalLight(0xffffff, 2.5);
  key.position.set(5, 8, 5);
  key.castShadow = true;
  key.shadow.mapSize.set(1024, 1024);
  scene.add(key);
  scene.add(new THREE.DirectionalLight(0x4488ff, 0.5).translateX(-3));

  // No grid — this is deep space

  // ── HUD — status in PROPS tab, FPS + hints only on canvas ────
  const statusPanel = createHUDPanel('top:16px;right:16px;text-align:right');
  statusPanel.innerHTML = '<span style="color:#8b949e;">⏳</span> <span id="fps-display" style="color:#58a6ff;font-size:16px;font-weight:700;">FPS --</span>';
  document.body.appendChild(statusPanel);

  const hintPanel = createHUDPanel('bottom:16px;left:50%;transform:translateX(-50%)');
  hintPanel.innerHTML = '🖱️ <b>Drag</b> orbit · <b>Scroll</b> zoom · <b>Space</b> auto-rotate · <b>Ctrl+O</b> load .kn';
  document.body.appendChild(hintPanel);

  // Props tab — where telemetry and status go
  const propsInner = document.getElementById('props-inner');
  const propsData = {};  // key → value store for telemetry
  // ── Tab switching ──────────────────────────────────────────────
  document.querySelectorAll('.tab').forEach(tab => {
    tab.addEventListener('click', () => {
      document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
      document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
      tab.classList.add('active');
      document.getElementById('tab-' + tab.dataset.tab).classList.add('active');
    });
  });

  // Keyboard shortcuts
  window.addEventListener('keydown', (e) => {
    if (e.code === 'Space') {
      e.preventDefault();
      controls.autoRotate = !controls.autoRotate;
    }
    // Ctrl+O: Load a .kn file
    if (e.code === 'KeyO' && e.ctrlKey) {
      e.preventDefault();
      loadKainFile();
    }
  });

  // Resize
  window.addEventListener('resize', () => {
    camera.aspect = window.innerWidth / window.innerHeight;
    camera.updateProjectionMatrix();
    renderer.setSize(window.innerWidth, window.innerHeight);
  });

  // ── Sidebar: file scanning, load, resize ──────────────────────
  const sidebarList = document.getElementById('sidebar-list');
  const loadBtn = document.getElementById('load-btn');
  const resizeHandle = document.getElementById('resize-handle');
  const sidebar = document.getElementById('sidebar');

  let activeFile = null;
  const demosDir = path.join(__dirname, 'demos');

  function scanKnFiles() {
    sidebarList.innerHTML = '';
    try {
      if (!fs.existsSync(demosDir)) {
        sidebarList.innerHTML = '<div class="file-item" style="color:#8b949e;">demos/ not found</div>';
        return;
      }
      const files = fs.readdirSync(demosDir).filter(f => f.endsWith('.kn')).sort();
      if (files.length === 0) {
        sidebarList.innerHTML = '<div class="file-item" style="color:#8b949e;">No .kn files</div>';
        return;
      }
      for (const file of files) {
        const baseName = file.replace('.kn', '');
        const exePath = path.join(demosDir, baseName + '.exe');
        const hasExe = fs.existsSync(exePath);

        const item = document.createElement('div');
        item.className = 'file-item';
        item.innerHTML = '<span class="icon">' + (hasExe ? '✅' : '📄') + '</span><span class="name">' + file + '</span><span class="status">' + (hasExe ? ' ready' : '') + '</span>';

        // Click: run directly if exe exists, build+run if not
        item.addEventListener('click', () => {
          if (hasExe) {
            switchToSource(baseName);
          } else {
            loadKnFile(path.join(demosDir, file), item);
          }
        });

        // Rebuild button — forces kain build even if exe exists
        const rebuildBtn = document.createElement('span');
        rebuildBtn.innerHTML = '🔄';
        rebuildBtn.title = 'Rebuild ' + file;
        rebuildBtn.style.cssText = 'cursor:pointer;opacity:0.4;font-size:12px;padding:0 4px;';
        rebuildBtn.addEventListener('mouseenter', () => { rebuildBtn.style.opacity = '1'; });
        rebuildBtn.addEventListener('mouseleave', () => { rebuildBtn.style.opacity = '0.4'; });
        rebuildBtn.addEventListener('click', (e) => {
          e.stopPropagation();  // don't trigger the file click
          rebuildKnFile(path.join(demosDir, file), item);
        });
        item.appendChild(rebuildBtn);

        sidebarList.appendChild(item);
      }
    } catch (e) {
      sidebarList.innerHTML = '<div class="file-item" style="color:#ff7b72;">Error: ' + e.message + '</div>';
    }
  }

  async function loadKnFile(filePath, itemElement) {
    if (activeFile) activeFile.classList.remove('active');
    if (itemElement) { itemElement.classList.add('active', 'building'); activeFile = itemElement; }
    try {
      const result = await ipcRenderer.invoke('load-kain-file', filePath);
      if (result.ok && itemElement) {
        itemElement.classList.remove('building');
        itemElement.querySelector('.status').textContent = '● LIVE';
      } else if (itemElement) {
        itemElement.classList.remove('building', 'active');
        itemElement.querySelector('.status').textContent = '❌';
        itemElement.style.color = '#ff7b72';
      }
    } catch (e) {
      if (itemElement) { itemElement.classList.remove('building', 'active'); itemElement.style.color = '#ff7b72'; }
    }
  }

  // Rebuild only — forces kain build, re-runs if was the active file
  async function rebuildKnFile(filePath, itemElement) {
    if (itemElement) {
      itemElement.classList.add('building');
      itemElement.querySelector('.status').textContent = '🔨';
    }
    console.log('[rebuild] building:', filePath);
    try {
      const result = await ipcRenderer.invoke('load-kain-file', filePath);
      if (result.ok && itemElement) {
        itemElement.classList.remove('building');
        itemElement.querySelector('.status').textContent = '● LIVE';
      } else if (itemElement) {
        itemElement.classList.remove('building');
        itemElement.querySelector('.status').textContent = '❌';
      }
    } catch (e) {
      if (itemElement) { itemElement.classList.remove('building'); }
    }
  }

  // Run directly — spawn existing exe without rebuilding
  async function switchToSource(name) {
    if (activeFile) activeFile.classList.remove('active');
    console.log('[switch] running:', name);
    try {
      const result = await ipcRenderer.invoke('switch-source', name);
      if (result.ok) {
        const items = sidebarList.querySelectorAll('.file-item');
        for (const it of items) {
          if (it.querySelector('.name').textContent === name + '.kn') {
            it.classList.add('active');
            it.querySelector('.status').textContent = '● LIVE';
            activeFile = it;
          }
        }
      }
    } catch (e) {
      console.error('[switch] failed:', e);
    }
  }

  scanKnFiles();

  loadBtn.addEventListener('click', () => {
    const input = document.createElement('input');
    input.type = 'file'; input.accept = '.kn'; input.style.display = 'none';
    input.addEventListener('change', () => { const f = input.files[0]; if (f) loadKnFile(f.path, null); input.remove(); });
    document.body.appendChild(input); input.click();
  });

  // Resize handle
  let resizing = false;
  resizeHandle.addEventListener('mousedown', (e) => { resizing = true; resizeHandle.classList.add('dragging'); e.preventDefault(); });
  document.addEventListener('mousemove', (e) => {
    if (!resizing) return;
    sidebar.style.width = Math.max(180, Math.min(500, e.clientX)) + 'px';
  });
  document.addEventListener('mouseup', () => {
    if (resizing) {
      resizing = false; resizeHandle.classList.remove('dragging');
      renderer.setSize(window.innerWidth - sidebar.offsetWidth - 4, window.innerHeight);
      camera.aspect = (window.innerWidth - sidebar.offsetWidth - 4) / window.innerHeight;
      camera.updateProjectionMatrix();
    }
  });

  // ── Geometry cache — high-resolution for visual quality ────────
  const GEOMETRIES = {
    box:           new THREE.BoxGeometry(0.3, 0.3, 0.3),
    sphere:        new THREE.SphereGeometry(0.5, 64, 64),
    cylinder:      new THREE.CylinderGeometry(0.3, 0.3, 0.7, 32),
    cone:          new THREE.ConeGeometry(0.3, 0.8, 32),
    torus:         new THREE.TorusGeometry(0.4, 0.1, 24, 48),
    torusKnot:     new THREE.TorusKnotGeometry(0.4, 0.08, 64, 16),
    ring:          new THREE.RingGeometry(0.2, 0.5, 48),
    plane:         new THREE.PlaneGeometry(0.8, 0.8),
    tetrahedron:   new THREE.TetrahedronGeometry(0.5, 0),
    octahedron:    new THREE.OctahedronGeometry(0.5, 0),
    icosahedron:   new THREE.IcosahedronGeometry(0.5, 0),
    dodecahedron:  new THREE.DodecahedronGeometry(0.5, 0),
    circle:        new THREE.CircleGeometry(0.45, 24),
    tube:          new THREE.TubeGeometry(new THREE.CatmullRomCurve3([
                     new THREE.Vector3(-0.3, -0.3, 0),
                     new THREE.Vector3(0, 0.3, 0),
                     new THREE.Vector3(0.3, -0.3, 0)
                   ]), 24, 0.08, 8, false),
    tetrahedron:   new THREE.TetrahedronGeometry(0.45),
    octahedron:    new THREE.OctahedronGeometry(0.45),
    icosahedron:   new THREE.IcosahedronGeometry(0.45),
    dodecahedron:  new THREE.DodecahedronGeometry(0.45),
  };

  // ==================================================================
  //  SCENE MANAGER — The command interpreter
  // ==================================================================
  class SceneManager {
    constructor(scene) {
      this.scene = scene;
      this.objects = new Map();      // id → { type, mesh, material }
      this.instanced = new Map();    // id → { mesh, count, dummy }
      this.lights = new Map();       // id → THREE.Light
      this._dummy = new THREE.Object3D();
    }

    // ── Public: execute a parsed JSON command ────────────────────
    execute(cmd) {
      if (!cmd || !cmd.cmd) return;
      switch (cmd.cmd) {
        case 'create':    this._create(cmd); break;
        case 'update':    this._update(cmd); break;
        case 'remove':    this._remove(cmd); break;
        case 'clear':     this._clear(); break;
        case 'camera':    this._camera(cmd); break;
        case 'telemetry': this._telemetry(cmd); break;
        case 'material': this._material(cmd); break;
        case 'ambient':  this._ambient(cmd); break;
        case 'light':    this._light(cmd); break;
        case 'fog':      this._fog(cmd); break;
        case 'background': this._background(cmd); break;
        case 'auto_rotate': this._autoRotate(cmd); break;
        case 'background':this._background(cmd); break;
        case 'fog':       this._fog(cmd); break;
        case 'auto_rotate':this._autoRotate(cmd); break;
        case 'light':     this._light(cmd); break;
        case 'ambient':   this._ambient(cmd); break;
      }
    }

    // ── CREATE ──────────────────────────────────────────────────
    _create(cmd) {
      const { id, type, geo, count, color, size, emissive, emissive_intensity, pos, rot } = cmd;
      if (!id) return;

      switch (type) {
        case 'mesh': {
          const geometry = GEOMETRIES[geo] || GEOMETRIES.sphere;
          const material = new THREE.MeshStandardMaterial({
            color: parseHex(color || '#4488ff'),
            roughness: 0.3, metalness: 0.6,
            emissive: parseHex(emissive || '#000000'),
            emissiveIntensity: emissive_intensity || 0,
          });
          const mesh = new THREE.Mesh(geometry, material);
          if (pos) mesh.position.set(pos.x || 0, pos.y || 0, pos.z || 0);
          if (rot) mesh.rotation.set(rot.x || 0, rot.y || 0, rot.z || 0);
          mesh.castShadow = true;
          mesh.receiveShadow = true;
          this.scene.add(mesh);
          this.objects.set(id, { type: 'mesh', mesh, material });
          break;
        }
        case 'instanced': {
          const geometry = GEOMETRIES[geo] || GEOMETRIES.box;
          const material = new THREE.MeshStandardMaterial({
            color: parseHex(color || '#ff4444'),
            roughness: 0.3, metalness: 0.6,
            emissive: parseHex(emissive || '#000000'),
            emissiveIntensity: emissive_intensity || 0,
          });
          const im = new THREE.InstancedMesh(geometry, material, count || 100);
          im.castShadow = true;
          im.receiveShadow = true;
          this.scene.add(im);
          this.instanced.set(id, { mesh: im, count: count || 100, material });
          break;
        }
        case 'particles': {
          const n = count || 1000;
          const positions = new Float32Array(n * 3);
          const geo = new THREE.BufferGeometry();
          geo.setAttribute('position', new THREE.BufferAttribute(positions, 3));
          const mat = new THREE.PointsMaterial({
            color: parseHex(color || '#44aaff'),
            size: size || 0.08,
            transparent: true,
            opacity: 0.8,
          });
          const pts = new THREE.Points(geo, mat);
          this.scene.add(pts);
          this.objects.set(id, { type: 'particles', mesh: pts, material: mat });
          break;
        }
        case 'lines': {
          const geo = new THREE.BufferGeometry();
          geo.setAttribute('position', new THREE.Float32BufferAttribute([0, 0, 0, 1, 0, 0], 3));
          const mat = new THREE.LineBasicMaterial({
            color: parseHex(color || '#44ddff'),
            transparent: true,
            opacity: 0.7,
          });
          const line = new THREE.Line(geo, mat);
          this.scene.add(line);
          this.objects.set(id, { type: 'lines', mesh: line, material: mat });
          break;
        }
        case 'buffer_geo': {
          // Raw vertex + index buffer from Kain
          const verts = cmd.verts || [];
          const indices = cmd.indices || [];
          const flatVerts = [];
          for (const v of verts) {
            flatVerts.push(v.x || 0, v.y || 0, v.z || 0);
          }
          const geo = new THREE.BufferGeometry();
          geo.setAttribute('position', new THREE.Float32BufferAttribute(flatVerts, 3));
          if (indices.length > 0) {
            geo.setIndex(indices);
          }
          geo.computeVertexNormals();
          const mat = new THREE.MeshStandardMaterial({
            color: parseHex(color || '#ff8844'),
            roughness: 0.4, metalness: 0.5,
            side: THREE.DoubleSide,
          });
          const mesh = new THREE.Mesh(geo, mat);
          mesh.castShadow = true;
          this.scene.add(mesh);
          this.objects.set(id, { type: 'buffer_geo', mesh, material: mat });
          }
          break;
        case 'custom_mesh': {
          // Raw flat vertex + index arrays from Kain: [x0,y0,z0, x1,y1,z1, ...]
          // indices: [i0,i1,i2, i3,i4,i5, ...] — 3 per triangle
          const verts = cmd.verts || [];
          const indices = cmd.indices || [];
          const geo = new THREE.BufferGeometry();
          geo.setAttribute('position', new THREE.Float32BufferAttribute(verts, 3));
          if (indices.length > 0) geo.setIndex(indices);
          geo.computeVertexNormals();
          const mat = new THREE.MeshStandardMaterial({
            color: parseHex(color || '#ff8844'),
            roughness: 0.4, metalness: 0.5,
            side: THREE.DoubleSide,
          });
          const mesh = new THREE.Mesh(geo, mat);
          mesh.castShadow = true;
          mesh.receiveShadow = true;
          this.scene.add(mesh);
          this.objects.set(id, { type: 'custom_mesh', mesh, material: mat });
          break;
        }
      }
    }

    // ── UPDATE
    _update(cmd) {
      const { id, pos, scale, rot, color, emissive, emissive_intensity, size, matrices, positions, points, flat } = cmd;

      // Instanced transforms (hot path for 10K+)
      if (matrices && this.instanced.has(id)) {
        const group = this.instanced.get(id);
        const len = Math.min(matrices.length, group.count);
        for (let i = 0; i < len; i++) {
          const m = matrices[i];
          if (!m) continue;
          this._dummy.position.set(m.x || 0, m.y || 0, m.z || 0);
          this._dummy.scale.set(m.sx || 1, m.sy || 1, m.sz || 1);
          this._dummy.updateMatrix();
          this._dummy.rotation.set(m.rx || 0, m.ry || 0, m.rz || 0);
          group.mesh.setMatrixAt(i, this._dummy.matrix);
        }
        group.mesh.instanceMatrix.needsUpdate = true;
        // Color update for instanced
        if (color && group.material) {
          group.material.color.set(parseHex(color));
        }
        return;
      }

      // Named object
      const obj = this.objects.get(id);
      if (!obj) return;

      if (pos && obj.mesh) {
        obj.mesh.position.set(pos.x || 0, pos.y || 0, pos.z || 0);
      }
      if (scale && obj.mesh) {
        obj.mesh.scale.set(scale.x || 1, scale.y || 1, scale.z || 1);
      if (rot && obj.mesh) {
        obj.mesh.rotation.set(rot.x || 0, rot.y || 0, rot.z || 0);
      }
      }
      if (color && obj.material) {
        obj.material.color.set(parseHex(color));
      if (emissive && obj.material) {
        obj.material.emissive.set(parseHex(emissive));
        if (emissive_intensity !== undefined) obj.material.emissiveIntensity = emissive_intensity;
      }
      }

      // Particle positions
      if (positions && obj.type === 'particles') {
        const arr = obj.mesh.geometry.attributes.position.array;
        const len = Math.min(positions.length, arr.length / 3);
        for (let i = 0; i < len; i++) {
          const p = positions[i];
          arr[i * 3]     = p.x || 0;
          arr[i * 3 + 1] = p.y || 0;
          arr[i * 3 + 2] = p.z || 0;
        }
        obj.mesh.geometry.attributes.position.needsUpdate = true;
      }
      // Particle size
      if (size !== undefined && obj.material && obj.material.isPointsMaterial) {
        obj.material.size = size;
      }

      // Flat float array (compact format: [x0,y0,z0, x1,y1,z1, ...])
      if (flat && obj.type === 'particles') {
        const arr = obj.mesh.geometry.attributes.position.array;
        const len = Math.min(flat.length / 3, arr.length / 3);
        for (let i = 0; i < len * 3; i++) {
          arr[i] = flat[i] || 0;
        }
        obj.mesh.geometry.attributes.position.needsUpdate = true;
      }

      // Custom mesh verts update (flat float array + type check)
      if (cmd.type === 'custom_mesh' && cmd.verts && obj.type === 'custom_mesh') {
        const arr = obj.mesh.geometry.attributes.position.array;
        const v = cmd.verts;
        const len = Math.min(v.length, arr.length);
        for (let i = 0; i < len; i++) arr[i] = v[i] || 0;
        obj.mesh.geometry.attributes.position.needsUpdate = true;
        obj.mesh.geometry.computeVertexNormals();
      }

      // Line points
      if (points && obj.type === 'lines') {
        const flat = [];
        for (const p of points) {
          flat.push(p.x || 0, p.y || 0, p.z || 0);
        }
        obj.mesh.geometry.setAttribute('position',
          new THREE.Float32BufferAttribute(flat, 3));
        obj.mesh.geometry.computeBoundingSphere();
      }
    }

    // ── REMOVE ──────────────────────────────────────────────────
    _remove(cmd) {
      const { id } = cmd;
      this._disposeObject(id);
    }

    // ── CLEAR ───────────────────────────────────────────────────
    _clear() {
      for (const id of this.objects.keys()) {
        this._disposeObject(id);
      }
      for (const [id, group] of this.instanced) {
        this.scene.remove(group.mesh);
        group.mesh.geometry.dispose();
        group.material.dispose();
      }
      this.objects.clear();
      this.instanced.clear();
      for (const [id, light] of this.lights) {
        this.scene.remove(light);
      }
      this.lights.clear();
    }

    // ── CAMERA ──────────────────────────────────────────────────
    _camera(cmd) {
      if (cmd.pos) {
        camera.position.set(cmd.pos.x || 0, cmd.pos.y || 0, cmd.pos.z || 8);
      }
      if (cmd.target) {
        controls.target.set(cmd.target.x || 0, cmd.target.y || 0, cmd.target.z || 0);
        controls.update();
      }
    }

    // ── TELEMETRY ───────────────────────────────────────────────
    _telemetry(cmd) {
      const { key, value } = cmd;
      if (!key) return;
      propsData[key] = value;
      renderProps();
    }

    // ── BACKGROUND ───────────────────────────────────────────────
    _background(cmd) {
      if (cmd.color) {
        this.scene.background = new THREE.Color(parseHex(cmd.color));
      }
    }

    // ── FOG ───────────────────────────────────────────────────────
    _fog(cmd) {
      if (cmd.color) {
        this.scene.fog = new THREE.Fog(parseHex(cmd.color), cmd.near || 0.1, cmd.far || 50);
      }
    }

    // ── AUTO ROTATE ───────────────────────────────────────────────
    _autoRotate(cmd) {
      if (cmd.enabled !== undefined) {
        controls.autoRotate = cmd.enabled;
      }
      if (cmd.speed !== undefined) {
        controls.autoRotateSpeed = cmd.speed;
      }
    }

    // ── LIGHT ─────────────────────────────────────────────────────
    _light(cmd) {
      const { id, type: op, light_type, color, intensity, pos } = cmd;
      if (op === 'create') {
        if (this.lights.has(id)) {
          this.scene.remove(this.lights.get(id));
          this.lights.delete(id);
        }
        let light;
        switch (light_type) {
          case 'ambient':
            light = new THREE.AmbientLight(parseHex(color || '#ffffff'), intensity || 1);
            break;
          case 'directional':
            light = new THREE.DirectionalLight(parseHex(color || '#ffffff'), intensity || 1);
            if (pos) light.position.set(pos.x || 0, pos.y || 0, pos.z || 0);
            break;
          case 'point':
            light = new THREE.PointLight(parseHex(color || '#ffffff'), intensity || 1, 50);
            if (pos) light.position.set(pos.x || 0, pos.y || 0, pos.z || 0);
            break;
          case 'spot':
            light = new THREE.SpotLight(parseHex(color || '#ffffff'), intensity || 1);
            if (pos) light.position.set(pos.x || 0, pos.y || 0, pos.z || 0);
            break;
          default:
            light = new THREE.DirectionalLight(parseHex(color || '#ffffff'), intensity || 1);
            if (pos) light.position.set(pos.x || 0, pos.y || 0, pos.z || 0);
        }
        this.scene.add(light);
        this.lights.set(id, light);
      } else if (op === 'update' && this.lights.has(id)) {
        const light = this.lights.get(id);
        if (color) light.color.set(parseHex(color));
        if (intensity !== undefined) light.intensity = intensity;
        if (pos) light.position.set(pos.x || 0, pos.y || 0, pos.z || 0);
      } else if (op === 'remove') {
        if (this.lights.has(id)) {
          this.scene.remove(this.lights.get(id));
          this.lights.delete(id);
        }
      }
    }

    // ── AMBIENT ───────────────────────────────────────────────────
    _ambient(cmd) {
      if (cmd.color) {
        this._ambientLight = this._ambientLight || new THREE.AmbientLight(0x404060, 0.6);
        this._ambientLight.color.set(parseHex(cmd.color));
        if (cmd.intensity !== undefined) this._ambientLight.intensity = cmd.intensity;
        if (!this._ambientLight.parent) this.scene.add(this._ambientLight);
      }
    }

    // ── Internal helpers ────────────────────────────────────────
    _disposeObject(id) {
      const obj = this.objects.get(id);
      if (!obj) {
        // Try instanced
        const group = this.instanced.get(id);
        if (group) {
          this.scene.remove(group.mesh);
          group.mesh.geometry.dispose();
          group.material.dispose();
          this.instanced.delete(id);
        }
        return;
      }
      this.scene.remove(obj.mesh);
      if (obj.mesh.geometry) obj.mesh.geometry.dispose();
      if (obj.material) obj.material.dispose();
      this.objects.delete(id);
    }

    dispose() {
      this._clear();
    }
  }

  // ── Props tab renderer ──────────────────────────────────────────
  function renderProps() {
    let html = '<div style="color:#58a6ff;font-weight:700;margin-bottom:10px;">📊 Live Telemetry</div>';
    const keys = Object.keys(propsData);
    if (keys.length === 0) {
      html += '<div style="color:#8b949e;">No telemetry yet. Run a demo.</div>';
    } else {
      for (const k of keys.sort()) {
        html += '<div class="prop-row"><span class="prop-key">' + k + '</span><span class="prop-val">' + formatVal(propsData[k]) + '</span></div>';
      }
    }
    propsInner.innerHTML = html;
  }

  // ── Instantiate the engine ──────────────────────────────────────
  const engine = new SceneManager(scene);

  // ── IPC: data from Kain (via main.js child_process stdout) ─────
  ipcRenderer.on('kain-data', (_event, payload) => {
    try {
      // payload can be { source: "...", data: {...} } from bridge-manager
      // or raw JSON from the old direct IPC path
      const data = payload.data || payload;
      if (typeof data === 'object' && data.cmd) {
        engine.execute(data);
      }
      // Legacy support: old bridge/demo format
      if (typeof data === 'object' && data.source !== undefined && data.mirror !== undefined) {
        updateLegacyEntangle(data);
      }
    } catch (e) {
      console.error('[engine] command error:', e);
    }
  });

  // Bridge status updates
  ipcRenderer.on('bridge-status', (_event, status) => {
    if (status.state === 'connected') {
      statusPanel.innerHTML = `<div style="color:#7ee787;">🟢 LIVE · ${status.source || 'kain'}</div>`
        + '<div id="fps-display" style="color:#58a6ff;font-size:18px;font-weight:700;margin-top:4px;">FPS --</div>';
    } else if (status.state === 'disconnected') {
      statusPanel.innerHTML = `<div style="color:#${status.code === 0 ? '7ee787' : 'ff7b72'};">${status.label || '🔴 EXITED'}</div>`
        + '<div id="fps-display" style="color:#58a6ff;font-size:18px;font-weight:700;margin-top:4px;">FPS --</div>';
    } else if (status.state === 'missing') {
      statusPanel.innerHTML = '<div style="color:#ffa657;">⚠️ NO BRIDGE</div>';
    } else if (status.state === 'building') {
      statusPanel.innerHTML = '<div style="color:#ffa657;">🔨 Building ' + (status.source || '...') + '</div>';
    } else if (status.state === 'error') {
      statusPanel.innerHTML = '<div style="color:#ff7b72;">❌ BUILD FAILED</div><div style="color:#8b949e;font-size:10px;">' + (status.error || '') + '</div>';
    }
  });

  // ── File loader: Ctrl+O opens a .kn file, builds it, and runs it ─
  async function loadKainFile() {
    const { dialog } = require('@electron/remote') || {};
    // Fallback: use HTML file input (works without @electron/remote)
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = '.kn';
    input.style.display = 'none';
    input.addEventListener('change', async () => {
      const file = input.files[0];
      if (!file) return;
      const filePath = file.path;
      console.log('[loader] loading:', filePath);
      statusPanel.innerHTML = '<div style="color:#ffa657;">🔨 Building...</div>';
      try {
        const result = await ipcRenderer.invoke('load-kain-file', filePath);
        if (result.ok) {
          console.log('[loader] spawned:', result.source);
        } else {
          statusPanel.innerHTML = `<div style="color:#ff7b72;">❌ BUILD FAILED</div><div style="color:#8b949e;font-size:11px;">${result.error || 'unknown'}</div>`;
        }
      } catch (e) {
        statusPanel.innerHTML = `<div style="color:#ff7b72;">❌ ERROR</div><div style="color:#8b949e;font-size:11px;">${e.message}</div>`;
      }
      input.remove();
    });
    document.body.appendChild(input);
    input.click();
  }

  // ── Legacy entangle visual (for old bridge.kn / pulse.kn demos) ─
  let legacySourceSphere, legacyMirrorSphere;
  function updateLegacyEntangle(data) {
    if (!legacySourceSphere) {
      // Lazy-create legacy spheres on first legacy data
      const sg = new THREE.SphereGeometry(0.9, 64, 64);
      const sm = new THREE.MeshStandardMaterial({ color: 0x3a86ff, emissive: 0x3a86ff, emissiveIntensity: 0.6, roughness: 0.15, metalness: 0.85 });
      legacySourceSphere = new THREE.Mesh(sg, sm);
      legacySourceSphere.position.set(-2.2, 0.5, 0);
      legacySourceSphere.castShadow = true;
      scene.add(legacySourceSphere);

      const mg = new THREE.SphereGeometry(0.9, 64, 64);
      const mm = new THREE.MeshStandardMaterial({ color: 0xaa66ff, emissive: 0xaa66ff, emissiveIntensity: 0.45, roughness: 0.2, metalness: 0.75 });
      legacyMirrorSphere = new THREE.Mesh(mg, mm);
      legacyMirrorSphere.position.set(2.2, 0.5, 0);
      legacyMirrorSphere.castShadow = true;
      scene.add(legacyMirrorSphere);
    }
    const src = data.source | 0;
    const mir = data.mirror | 0;
    const tick = data.tick | 0;
    const srcX = ((src % 40) / 40) * 5 - 2.5;
    const mirX = ((mir % 40) / 40) * 5 - 2.5;
    legacySourceSphere.position.set(srcX, 0.5 + Math.sin(tick * 0.3) * 0.7, Math.cos(tick * 0.2) * 0.6);
    legacyMirrorSphere.position.set(mirX, 0.5 + Math.sin(tick * 0.3 + Math.PI) * 0.7, Math.cos(tick * 0.2 + Math.PI) * 0.6);
    const srcScale = 0.5 + (src % 60) / 60 * 1.5;
    const mirScale = 0.5 + (mir % 60) / 60 * 1.5;
    legacySourceSphere.scale.setScalar(srcScale);
    legacyMirrorSphere.scale.setScalar(mirScale);
  }
  // ── Interaction events → Kain stdin bridge ─────────────────────
  // These power electron_interact.kn — mouse, click, keyboard events
  // sent to the Kain process via main.js stdin backchannel.
  // Mousemove is throttled to ~60fps to avoid flooding the pipe.
  let lastMoveTime = 0;
  const MOVE_THROTTLE = 16; // ms

  document.addEventListener('mousemove', (e) => {
    const now = performance.now();
    if (now - lastMoveTime < MOVE_THROTTLE) return;
    lastMoveTime = now;
    ipcRenderer.send('bridge-event', {
      type: 'mousemove', x: e.clientX, y: e.clientY
    });
  });

  document.addEventListener('click', (e) => {
    // Send raw screen click for non-3D interaction
    ipcRenderer.send('bridge-event', {
      type: 'click', x: e.clientX, y: e.clientY
    });

    // ── 3D RAYCASTER: find which object was clicked in the scene ──
    const rect = renderer.domElement.getBoundingClientRect();
    const mx = ((e.clientX - rect.left) / rect.width) * 2 - 1;
    const my = -((e.clientY - rect.top) / rect.height) * 2 + 1;
    const mouse = new THREE.Vector2(mx, my);

    const raycaster = new THREE.Raycaster();
    raycaster.setFromCamera(mouse, camera);

    // Collect all scene objects for hit testing
    const targets = [];
    for (const [, obj] of engine.objects) {
      if (obj.mesh) targets.push(obj.mesh);
    }
    for (const [, group] of engine.instanced) {
      if (group.mesh) targets.push(group.mesh);
    }

    const intersects = raycaster.intersectObjects(targets, false);

    if (intersects.length > 0) {
      const hit = intersects[0];
      // Find the Kain-side ID of this object
      let hitId = 'unknown';
      for (const [id, obj] of engine.objects) {
        if (obj.mesh === hit.object) { hitId = id; break; }
      }
      if (hitId === 'unknown') {
        for (const [id, group] of engine.instanced) {
          if (group.mesh === hit.object) { hitId = id; break; }
        }
      }
      const normal = hit.face ? hit.face.normal : new THREE.Vector3(0, 1, 0);
      ipcRenderer.send('bridge-event', {
        type: 'raycast', hit: true, id: hitId,
        x: hit.point.x, y: hit.point.y, z: hit.point.z,
        nx: normal.x, ny: normal.y, nz: normal.z,
        distance: hit.distance,
      });
    } else {
      ipcRenderer.send('bridge-event', {
        type: 'raycast', hit: false
      });
    }
  });

  document.addEventListener('keydown', (e) => {
    if (e.repeat) return;
    // Don't steal Space from auto-rotate toggle or Ctrl+O from file load
    if (e.code === 'Space' || (e.ctrlKey && e.code === 'KeyO')) return;
    ipcRenderer.send('bridge-event', {
      type: 'keydown', key: e.key
    });
  });

  document.addEventListener('keyup', (e) => {
    ipcRenderer.send('bridge-event', {
      type: 'keyup', key: e.key
    });
  });

  // ── Animation loop ─────────────────────────────────────────────
  let frameCount = 0;
  let lastFPSTime = performance.now();
  const clock = new THREE.Clock();

  function animate() {
    requestAnimationFrame(animate);
    controls.update();
    frameCount++;

    // FPS
    const now = performance.now();
    if (now - lastFPSTime > 500) {
      const fps = Math.round((frameCount * 1000) / (now - lastFPSTime));
      const fpsEl = document.getElementById('fps-display');
      if (fpsEl) fpsEl.textContent = `FPS ${fps}`;
      frameCount = 0;
      lastFPSTime = now;
    }

    // Gentle spin for any mesh objects without explicit animation
    for (const [, obj] of engine.objects) {
      if (obj.type === 'mesh' && obj.mesh) {
        obj.mesh.rotation.y += 0.005;
      }
    }

    renderer.render(scene, camera);
  }
  animate();

  console.log('[engine] ready · Kain SceneManager active · awaiting commands');

})().catch((err) => {
  console.error('[engine] init failed:', err);
  const errEl = document.createElement('div');
  errEl.style.cssText = 'position:fixed;top:50%;left:50%;transform:translate(-50%,-50%);color:#f44;font:13px monospace;background:#000;padding:20px;border:1px solid #f44;border-radius:8px;max-width:80%;z-index:9999;white-space:pre-wrap;';
  errEl.textContent = 'Engine init failed:\n' + err.message + '\n\n' + err.stack;
  document.body.appendChild(errEl);
});

// ── Helpers ───────────────────────────────────────────────────────
function parseHex(hex) {
  if (typeof hex === 'number') return hex;
  if (typeof hex === 'string') {
    if (hex.startsWith('#')) return parseInt(hex.slice(1), 16);
    return parseInt(hex, 16);
  }
  return 0x4488ff;
}

function createHUDPanel(extraCss) {
  const el = document.createElement('div');
  el.style.cssText = `
    position:fixed;${extraCss};
    color:#e6edf3;font-family:'Courier New',monospace;
    font-size:13px;line-height:1.7;
    background:rgba(6,8,15,0.92);
    border:1px solid #1e293b;
    border-radius:8px;padding:14px 18px;
    z-index:100;pointer-events:none;
    backdrop-filter:blur(6px);
  `;
  return el;
}

function formatVal(v) {
  if (v === null || v === undefined) return 'nil';
  if (typeof v === 'number') {
    if (Number.isInteger(v)) return String(v);
    return v.toFixed(3);
  }
  if (typeof v === 'string') return v;
  return String(v);
}
