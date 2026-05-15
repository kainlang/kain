import * as fs from 'node:fs';

function escapeHtml(value) {
  return String(value)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;');
}

function base64For(path) {
  return fs.readFileSync(path).toString('base64');
}

function viewerScript(id, base64, accent, label) {
  const fnName = `mount_${id.replace(/[^a-zA-Z0-9]/g, '_')}`;
  return `
    async function ${fnName}() {
      const container = document.getElementById('${id}');
      if (!container) {
        return;
      }
      const { Scene, PerspectiveCamera, WebGLRenderer, Color, HemisphereLight, DirectionalLight, Box3, Vector3 } = THREE;
      const scene = new Scene();
      scene.background = new Color(0x0a1220);
      const camera = new PerspectiveCamera(48, container.clientWidth / container.clientHeight, 0.1, 400);
      const renderer = new WebGLRenderer({ antialias: true, alpha: true });
      renderer.setPixelRatio(window.devicePixelRatio || 1);
      renderer.setSize(container.clientWidth, container.clientHeight);
      renderer.outputColorSpace = THREE.SRGBColorSpace;
      container.appendChild(renderer.domElement);

      const hemi = new HemisphereLight(0x98d8ff, 0x04070f, 1.55);
      scene.add(hemi);
      const sun = new DirectionalLight(${accent}, 2.4);
      sun.position.set(9, 12, 7);
      scene.add(sun);

      const controls = new OrbitControls(camera, renderer.domElement);
      controls.enableDamping = true;
      controls.target.set(0, 2.0, 0);

      const binary = Uint8Array.from(atob('${base64}'), (char) => char.charCodeAt(0));
      const model = await new Promise((resolve, reject) => {
        const loader = new GLTFLoader();
        loader.parse(binary.buffer, '', resolve, reject);
      });
      scene.add(model.scene);

      const box = new Box3().setFromObject(model.scene);
      const size = box.getSize(new Vector3());
      const center = box.getCenter(new Vector3());
      const radius = Math.max(size.x, size.y, size.z) * 0.72 + 5.0;
      camera.position.set(center.x + radius, center.y + radius * 0.6, center.z + radius);
      controls.target.copy(center);
      controls.update();

      const onResize = () => {
        const width = container.clientWidth;
        const height = container.clientHeight;
        camera.aspect = width / Math.max(height, 1);
        camera.updateProjectionMatrix();
        renderer.setSize(width, height);
      };
      window.addEventListener('resize', onResize);

      const animate = () => {
        requestAnimationFrame(animate);
        model.scene.rotation.y += ${label === 'mutated' ? '0.0016' : '0.001'};
        controls.update();
        renderer.render(scene, camera);
      };
      animate();
    }
    ${fnName}();
  `;
}

export function makeWorkflowCityDocument(title, baseGlbPath, mutatedGlbPath, signature, overviewHtml, modulesHtml) {
  const base64Base = base64For(baseGlbPath);
  const base64Mutated = base64For(mutatedGlbPath);
  const text = `<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>${escapeHtml(title)}</title>
  <style>
    :root {
      color-scheme: dark;
      --bg0: #04060c;
      --bg1: #0b1220;
      --card: rgba(12, 18, 32, 0.84);
      --line: rgba(126, 242, 255, 0.18);
      --cyan: #7ef2ff;
      --amber: #ffd166;
      --text: #eef6ff;
      --muted: #a4c3dd;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      background:
        radial-gradient(circle at 20% 20%, rgba(0, 165, 255, 0.14), transparent 32%),
        radial-gradient(circle at 80% 0%, rgba(255, 133, 76, 0.14), transparent 26%),
        linear-gradient(180deg, var(--bg1), var(--bg0));
      color: var(--text);
      font-family: "Segoe UI", "Helvetica Neue", sans-serif;
    }
    main {
      width: min(96vw, 1520px);
      margin: 28px auto 52px;
      padding: 26px;
      border-radius: 28px;
      background: rgba(5, 8, 16, 0.78);
      border: 1px solid rgba(126, 242, 255, 0.15);
      box-shadow: 0 28px 110px rgba(0, 0, 0, 0.45);
    }
    .hero {
      display: grid;
      grid-template-columns: 1.3fr 0.7fr;
      gap: 18px;
      align-items: start;
    }
    .eyebrow {
      margin: 0 0 8px;
      font-size: 11px;
      letter-spacing: 0.28em;
      text-transform: uppercase;
      color: var(--cyan);
    }
    h1 {
      margin: 0 0 10px;
      font-size: clamp(32px, 3.5vw, 54px);
      line-height: 0.95;
      letter-spacing: -0.04em;
    }
    .hero-copy {
      color: var(--muted);
      font-size: 15px;
      line-height: 1.6;
      max-width: 68ch;
    }
    .sig {
      display: inline-flex;
      align-items: center;
      gap: 10px;
      margin-top: 14px;
      padding: 10px 14px;
      border-radius: 999px;
      border: 1px solid rgba(255, 209, 102, 0.28);
      background: rgba(255, 209, 102, 0.08);
      color: var(--amber);
      font-family: Consolas, "Courier New", monospace;
      font-size: 12px;
    }
    .hero-panel,
    .viewer-card,
    .module-board {
      border: 1px solid var(--line);
      background: var(--card);
      border-radius: 24px;
    }
    .hero-panel {
      padding: 18px;
      color: var(--muted);
      line-height: 1.6;
    }
    .viewer-grid {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 18px;
      margin-top: 24px;
    }
    .viewer-card {
      overflow: hidden;
    }
    .viewer-top {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 16px 18px 0;
    }
    .viewer-top strong {
      font-size: 12px;
      text-transform: uppercase;
      letter-spacing: 0.26em;
      color: var(--amber);
    }
    .viewer-top span {
      color: var(--muted);
      font-size: 12px;
    }
    .viewport {
      width: 100%;
      aspect-ratio: 16 / 10;
      min-height: 420px;
      background:
        radial-gradient(circle at top, rgba(75, 139, 255, 0.18), transparent 40%),
        linear-gradient(180deg, #0a1220 0%, #060912 100%);
    }
    .overview {
      margin-top: 24px;
    }
    .overview-grid {
      display: grid;
      grid-template-columns: repeat(4, minmax(0, 1fr));
      gap: 14px;
    }
    .overview-card {
      padding: 14px;
      border-radius: 18px;
      background: rgba(255, 255, 255, 0.03);
      border: 1px solid rgba(255, 255, 255, 0.06);
    }
    .overview-card small {
      display: block;
      margin-bottom: 6px;
      font-size: 11px;
      text-transform: uppercase;
      letter-spacing: 0.2em;
      color: var(--cyan);
    }
    .overview-card strong {
      display: block;
      font-size: 22px;
      letter-spacing: -0.03em;
    }
    .overview-card p {
      margin: 8px 0 0;
      color: var(--muted);
      font-size: 13px;
      line-height: 1.5;
    }
    .module-board {
      margin-top: 24px;
      padding: 20px;
    }
    .module-grid {
      display: grid;
      grid-template-columns: repeat(3, minmax(0, 1fr));
      gap: 16px;
    }
    .workflow-card {
      --accent: #7ef2ff;
      padding: 16px;
      border-radius: 20px;
      border: 1px solid color-mix(in srgb, var(--accent) 26%, transparent);
      background: linear-gradient(180deg, color-mix(in srgb, var(--accent) 10%, rgba(255,255,255,0.02)), rgba(255,255,255,0.02));
    }
    .workflow-card .header {
      display: flex;
      justify-content: space-between;
      align-items: baseline;
      gap: 12px;
      margin-bottom: 12px;
    }
    .workflow-card h3 {
      margin: 0;
      font-size: 18px;
      color: var(--accent);
      letter-spacing: 0.02em;
    }
    .workflow-card .count {
      font-size: 12px;
      text-transform: uppercase;
      letter-spacing: 0.18em;
      color: var(--muted);
    }
    .workflow-card ul {
      list-style: none;
      padding: 0;
      margin: 0;
      display: grid;
      gap: 10px;
    }
    .workflow-card li {
      display: flex;
      justify-content: space-between;
      align-items: center;
      gap: 10px;
      padding: 10px 12px;
      border-radius: 14px;
      background: rgba(255, 255, 255, 0.04);
    }
    .workflow-card li span {
      color: var(--muted);
      font-size: 12px;
      font-family: Consolas, "Courier New", monospace;
    }
    @media (max-width: 1100px) {
      .hero,
      .viewer-grid,
      .module-grid,
      .overview-grid {
        grid-template-columns: 1fr;
      }
    }
  </style>
</head>
<body>
  <main>
    <section class="hero">
      <div>
        <p class="eyebrow">Kain 3D Mixed Runtime Smoke</p>
        <h1>Zen Workflow City</h1>
        <p class="hero-copy">Python generated the procedural scene geometry, Rust computed the workflow district layout, Kain sculpted the final mesh and authored the dossier, C validated the exported GLB through cgltf, and Node packed this interactive viewer into one deliverable.</p>
        <div class="sig">${escapeHtml(signature)}</div>
      </div>
      <aside class="hero-panel">${overviewHtml}</aside>
    </section>

    <section class="viewer-grid">
      <article class="viewer-card">
        <div class="viewer-top"><strong>Base Scene</strong><span>Python + Rust layout</span></div>
        <div id="base-viewport" class="viewport"></div>
      </article>
      <article class="viewer-card">
        <div class="viewer-top"><strong>Mutated Scene</strong><span>Kain-authored sculpt pass</span></div>
        <div id="mutated-viewport" class="viewport"></div>
      </article>
    </section>

    <section class="module-board">
      <p class="eyebrow">Workflow Dossier</p>
      <div class="module-grid">${modulesHtml}</div>
    </section>
  </main>
  <script type="module">
    import * as THREE from 'https://unpkg.com/three@0.180.0/build/three.module.js';
    import { OrbitControls } from 'https://unpkg.com/three@0.180.0/examples/jsm/controls/OrbitControls.js';
    import { GLTFLoader } from 'https://unpkg.com/three@0.180.0/examples/jsm/loaders/GLTFLoader.js';
    ${viewerScript('base-viewport', base64Base, '0x7ef2ff', 'base')}
    ${viewerScript('mutated-viewport', base64Mutated, '0xffd166', 'mutated')}
  </script>
</body>
</html>`;

  return {
    kind: 'document',
    title,
    mime_type: 'text/html',
    extension: 'html',
    text,
  };
}
