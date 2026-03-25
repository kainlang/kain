import fs from "node:fs";
import http from "node:http";
import path from "node:path";
import { pathToFileURL } from "node:url";

function readJson(filePath) {
  return JSON.parse(fs.readFileSync(filePath, "utf8"));
}

function resolveFrom(baseDir, relativePath) {
  return path.isAbsolute(relativePath)
    ? relativePath
    : path.resolve(baseDir, relativePath);
}

function ensureDir(dirPath) {
  fs.mkdirSync(dirPath, { recursive: true });
}

function writeJson(filePath, value) {
  ensureDir(path.dirname(filePath));
  fs.writeFileSync(filePath, JSON.stringify(value, null, 2));
}

function loadRegistry(appRoot, registryPath) {
  const fullPath = resolveFrom(appRoot, registryPath);
  const registry = readJson(fullPath);
  const registryRoot = path.dirname(fullPath);
  const entries = {};
  for (const [id, relativeFile] of Object.entries(registry.entries || {})) {
    const payload = readJson(resolveFrom(registryRoot, relativeFile));
    entries[id] = { id, ...payload };
  }
  return entries;
}

export function loadJson(filePath) {
  return readJson(path.resolve(filePath));
}

export function loadAppConfig(appManifestPath) {
  const fullPath = path.resolve(appManifestPath);
  const app = readJson(fullPath);
  const rootDir = path.dirname(fullPath);
  return {
    app,
    root_dir: rootDir,
    themes: loadRegistry(rootDir, app.registries.themes),
    content: loadRegistry(rootDir, app.registries.content),
    scenes: loadRegistry(rootDir, app.registries.scenes),
    experiences: loadRegistry(rootDir, app.registries.experiences)
  };
}

function requireEntry(table, key, kind) {
  const value = table[key];
  if (!value) {
    throw new Error(`missing ${kind} '${key}' in universal web template`);
  }
  return value;
}

function buildModel(appManifestPath, experienceId) {
  const context = loadAppConfig(appManifestPath);
  const selectedId = experienceId || context.app.default_experience;
  const experience = requireEntry(context.experiences, selectedId, "experience");
  const theme = requireEntry(context.themes, experience.theme, "theme");
  const content = requireEntry(context.content, experience.content, "content");
  const scene = requireEntry(context.scenes, experience.scene, "scene");
  return {
    context,
    experience,
    theme,
    content,
    scene,
    output_dir: path.resolve(context.root_dir, context.app.output_root, experience.output_slug)
  };
}

function escapeHtml(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;");
}

function renderActionButtons(actions) {
  return actions
    .map(
      (action) =>
        `<a class="action ${escapeHtml(action.style || "primary")}" href="${escapeHtml(action.href || "#")}">${escapeHtml(action.label)}</a>`
    )
    .join("");
}

function renderMetrics(metrics) {
  return metrics
    .map(
      (metric) => `<article class="metric-card" data-kain-component="metric-card">
  <p class="metric-value" data-target-value="${escapeHtml(metric.value)}">${escapeHtml(metric.value)}</p>
  <p class="metric-label">${escapeHtml(metric.label)}</p>
</article>`
    )
    .join("");
}

function renderCards(cards) {
  return cards
    .map(
      (card) => `<article class="feature-card">
  <p class="card-kicker">${escapeHtml(card.kicker || "")}</p>
  <h3>${escapeHtml(card.title)}</h3>
  <p>${escapeHtml(card.body)}</p>
</article>`
    )
    .join("");
}

function renderPortfolio(entries) {
  const tags = [...new Set(entries.flatMap((entry) => entry.tags || []))];
  const buttons = tags
    .map((tag, index) => `<button class="tag-filter" data-filter="${escapeHtml(tag)}"${index === 0 ? " data-active='true'" : ""}>${escapeHtml(tag)}</button>`)
    .join("");
  const cards = entries
    .map(
      (entry) => `<article class="portfolio-card" data-tags="${escapeHtml((entry.tags || []).join(" "))}">
  <p class="card-kicker">${escapeHtml(entry.year)}</p>
  <h3>${escapeHtml(entry.title)}</h3>
  <p>${escapeHtml(entry.summary)}</p>
  <p class="portfolio-stack">${escapeHtml((entry.tags || []).join(" / "))}</p>
</article>`
    )
    .join("");
  return `<div class="portfolio-filters" data-kain-component="portfolio-filter">${buttons}</div><div class="portfolio-grid">${cards}</div>`;
}

function renderTimeline(items) {
  return items
    .map(
      (item) => `<article class="timeline-row">
  <p class="timeline-label">${escapeHtml(item.phase)}</p>
  <div>
    <h3>${escapeHtml(item.title)}</h3>
    <p>${escapeHtml(item.body)}</p>
  </div>
</article>`
    )
    .join("");
}

function renderScene(scene) {
  const layers = (scene.layers || [])
    .map(
      (layer) => `<div class="scene-layer">
  <span>${escapeHtml(layer.name)}</span>
  <span>${escapeHtml(layer.detail)}</span>
</div>`
    )
    .join("");
  return `<section class="scene-shell">
  <div class="scene-stage">
    <div class="scene-core"></div>
    <div class="scene-ring"></div>
    <div class="scene-copy">
      <p class="card-kicker">${escapeHtml(scene.kicker)}</p>
      <h3>${escapeHtml(scene.title)}</h3>
      <p>${escapeHtml(scene.summary)}</p>
    </div>
  </div>
  <div class="scene-layers">${layers}</div>
</section>`;
}

function renderChat(messages) {
  const seed = messages
    .map(
      (message) => `<article class="chat-bubble ${escapeHtml(message.role)}">
  <p class="chat-role">${escapeHtml(message.role)}</p>
  <p>${escapeHtml(message.text)}</p>
</article>`
    )
    .join("");
  return `<section class="chat-shell" data-kain-component="chat-lab">
  <div class="chat-seed">${seed}</div>
  <form class="chat-form">
    <input name="prompt" type="text" placeholder="Ask the site orchestrator for a launch plan" />
    <button type="submit">Send</button>
  </form>
</section>`;
}

function renderActors(actors) {
  return actors
    .map(
      (actor) => `<article class="actor-card">
  <h3>${escapeHtml(actor.name)}</h3>
  <p>${escapeHtml(actor.responsibility)}</p>
  <p class="actor-channel">${escapeHtml(actor.channel)}</p>
</article>`
    )
    .join("");
}

function renderRoutes(routes) {
  return routes
    .map(
      (route) => `<article class="route-card">
  <p class="card-kicker">${escapeHtml(route.method)}</p>
  <h3>${escapeHtml(route.path)}</h3>
  <p>${escapeHtml(route.purpose)}</p>
</article>`
    )
    .join("");
}

function renderSection(sectionId, model) {
  const { content, scene } = model;
  switch (sectionId) {
    case "features":
      return `<section class="panel"><p class="section-label">Systems</p><h2>Reusable launch systems</h2><div class="feature-grid">${renderCards(content.feature_cards || [])}</div></section>`;
    case "story":
      return `<section class="panel"><p class="section-label">Narrative</p><h2>Story rails</h2><div class="feature-grid">${renderCards(content.story_cards || [])}</div></section>`;
    case "portfolio":
      return `<section class="panel"><p class="section-label">Portfolio</p><h2>Case studies and work capsules</h2>${renderPortfolio(content.portfolio_entries || [])}</section>`;
    case "timeline":
      return `<section class="panel"><p class="section-label">Timeline</p><h2>Build sequence</h2><div class="timeline-list">${renderTimeline(content.timeline || [])}</div></section>`;
    case "scene":
      return `<section class="panel"><p class="section-label">Scene</p><h2>Immersive 3D block</h2>${renderScene(scene)}</section>`;
    case "chat":
      return `<section class="panel"><p class="section-label">Chat</p><h2>Conversation-first surface</h2>${renderChat(content.chat_seed || [])}</section>`;
    case "actors":
      return `<section class="panel"><p class="section-label">Actors</p><h2>Actor mesh</h2><div class="feature-grid">${renderActors(content.actor_roles || [])}</div></section>`;
    case "server":
      return `<section class="panel"><p class="section-label">Server</p><h2>Route contract</h2><div class="feature-grid">${renderRoutes(content.server_routes || [])}</div></section>`;
    case "cta":
      return `<section class="panel cta-panel"><p class="section-label">CTA</p><h2>${escapeHtml(content.cta.title)}</h2><p>${escapeHtml(content.cta.body)}</p><div class="action-row">${renderActionButtons(content.cta.actions || [])}</div></section>`;
    default:
      return "";
  }
}

function renderClientRuntime(model) {
  const chatSeed = JSON.stringify(model.content.chat_seed || []);
  return `<script>
(() => {
  const metricCards = document.querySelectorAll('[data-kain-component="metric-card"] .metric-value');
  for (const metric of metricCards) {
    const raw = metric.getAttribute('data-target-value') || metric.textContent || '';
    const parsed = Number.parseInt(raw.replace(/[^0-9]/g, ''), 10);
    if (!Number.isFinite(parsed)) continue;
    let current = 0;
    const suffix = raw.replace(/[0-9]/g, '');
    const tick = () => {
      current = Math.min(parsed, current + Math.max(1, Math.ceil(parsed / 24)));
      metric.textContent = String(current) + suffix;
      if (current < parsed) requestAnimationFrame(tick);
    };
    requestAnimationFrame(tick);
  }

  const filterRoot = document.querySelector('[data-kain-component="portfolio-filter"]');
  if (filterRoot) {
    const cards = [...document.querySelectorAll('.portfolio-card')];
    for (const button of filterRoot.querySelectorAll('button')) {
      button.addEventListener('click', () => {
        const tag = button.dataset.filter || '';
        for (const candidate of filterRoot.querySelectorAll('button')) candidate.removeAttribute('data-active');
        button.setAttribute('data-active', 'true');
        for (const card of cards) {
          const tags = card.dataset.tags || '';
          card.style.display = tags.includes(tag) ? '' : 'none';
        }
      });
    }
  }

  const chatRoot = document.querySelector('[data-kain-component="chat-lab"]');
  if (chatRoot) {
    const seed = ${chatSeed};
    const seedBox = chatRoot.querySelector('.chat-seed');
    const form = chatRoot.querySelector('.chat-form');
    form?.addEventListener('submit', (event) => {
      event.preventDefault();
      const input = form.querySelector('input[name="prompt"]');
      const prompt = input?.value?.trim();
      if (!prompt) return;
      seedBox.insertAdjacentHTML('beforeend', '<article class="chat-bubble user"><p class="chat-role">user</p><p>' + prompt.replaceAll('<', '&lt;') + '</p></article>');
      const response = seed[(seed.length - 1) % Math.max(seed.length, 1)] || { text: 'Template runtime is ready for custom actor-backed chat flows.' };
      seedBox.insertAdjacentHTML('beforeend', '<article class="chat-bubble assistant"><p class="chat-role">assistant</p><p>' + String(response.text).replaceAll('<', '&lt;') + '</p></article>');
      if (input) input.value = '';
    });
  }
})();
</script>`;
}

function renderDocument(model) {
  const { app, experience, theme, content } = {
    app: model.context.app,
    experience: model.experience,
    theme: model.theme,
    content: model.content
  };
  const sections = (experience.sections || []).map((section) => renderSection(section, model)).join("");
  return `<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>${escapeHtml(experience.page_title)}</title>
  <style>
    :root {
      color-scheme: dark;
      --bg-top: ${theme.background_top};
      --bg-bottom: ${theme.background_bottom};
      --surface: ${theme.surface};
      --surface-alt: ${theme.surface_alt};
      --line: ${theme.line};
      --accent: ${theme.accent};
      --accent-soft: ${theme.accent_soft};
      --highlight: ${theme.highlight};
      --text: ${theme.text};
      --muted: ${theme.muted};
      --font-display: ${theme.font_display};
      --font-body: ${theme.font_body};
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      background: radial-gradient(circle at top, var(--bg-top), var(--bg-bottom) 68%);
      color: var(--text);
      font-family: var(--font-body);
    }
    a { color: inherit; text-decoration: none; }
    .shell {
      width: min(96vw, 1380px);
      margin: 24px auto;
      padding: 20px;
      border-radius: 32px;
      border: 1px solid var(--line);
      background: rgba(5, 8, 16, 0.76);
      box-shadow: 0 38px 140px rgba(0, 0, 0, 0.42);
      backdrop-filter: blur(16px);
    }
    .topbar, .action-row, .footer-row { display: flex; gap: 16px; flex-wrap: wrap; align-items: center; justify-content: space-between; }
    .nav-links { display: flex; gap: 14px; flex-wrap: wrap; color: var(--muted); }
    .brand-kicker, .section-label, .card-kicker, .timeline-label, .chat-role, .actor-channel {
      margin: 0 0 8px;
      font-size: 11px;
      letter-spacing: 0.24em;
      text-transform: uppercase;
      color: var(--accent-soft);
    }
    .hero-grid {
      display: grid;
      grid-template-columns: minmax(0, 1.2fr) minmax(320px, 0.8fr);
      gap: 18px;
      margin-top: 18px;
    }
    .panel, .hero-card, .metric-card, .feature-card, .portfolio-card, .route-card, .actor-card, .timeline-row {
      border-radius: 24px;
      border: 1px solid var(--line);
      background: linear-gradient(180deg, rgba(255,255,255,0.04), rgba(255,255,255,0.02));
    }
    .hero-card, .panel { padding: 18px; }
    .hero-title, h1, h2, h3 {
      margin: 0;
      font-family: var(--font-display);
      letter-spacing: -0.04em;
    }
    .hero-title { font-size: clamp(2.4rem, 6vw, 5rem); max-width: 12ch; }
    .hero-copy, .metric-label, .feature-card p, .portfolio-card p, .route-card p, .actor-card p, .timeline-row p, .footer-row { color: var(--muted); line-height: 1.5; }
    .metric-grid, .feature-grid, .portfolio-grid {
      display: grid;
      grid-template-columns: repeat(3, minmax(0, 1fr));
      gap: 14px;
    }
    .metric-card, .feature-card, .portfolio-card, .route-card, .actor-card { padding: 16px; }
    .metric-value { margin: 0; font-size: clamp(1.8rem, 4vw, 3rem); color: var(--highlight); font-family: var(--font-display); }
    .action {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      min-height: 44px;
      padding: 0 16px;
      border-radius: 999px;
      border: 1px solid var(--line);
    }
    .action.primary { background: var(--accent); color: #06111a; border-color: transparent; }
    .action.secondary { background: rgba(255,255,255,0.04); }
    .scene-shell { display: grid; grid-template-columns: minmax(0, 1fr) 320px; gap: 16px; }
    .scene-stage {
      position: relative;
      min-height: 340px;
      overflow: hidden;
      border-radius: 26px;
      background: radial-gradient(circle at center, rgba(255,255,255,0.1), rgba(0,0,0,0) 48%), linear-gradient(160deg, rgba(255,255,255,0.04), rgba(255,255,255,0.01));
      border: 1px solid var(--line);
    }
    .scene-core {
      position: absolute;
      inset: 22% auto auto 50%;
      width: 140px;
      height: 140px;
      transform: translateX(-50%);
      border-radius: 999px;
      background: radial-gradient(circle, var(--accent-soft), rgba(255,255,255,0));
      filter: blur(12px);
    }
    .scene-ring {
      position: absolute;
      inset: 50% auto auto 50%;
      width: 280px;
      height: 280px;
      transform: translate(-50%, -50%);
      border: 1px solid rgba(255,255,255,0.12);
      border-radius: 999px;
    }
    .scene-copy { position: absolute; left: 22px; bottom: 22px; max-width: 480px; }
    .scene-layers { display: grid; gap: 10px; }
    .scene-layer {
      display: flex;
      justify-content: space-between;
      gap: 12px;
      padding: 12px 14px;
      border-radius: 18px;
      border: 1px solid var(--line);
      background: rgba(255,255,255,0.03);
      color: var(--muted);
    }
    .portfolio-filters { display: flex; gap: 10px; flex-wrap: wrap; margin-bottom: 14px; }
    .tag-filter {
      min-height: 38px;
      padding: 0 14px;
      border-radius: 999px;
      border: 1px solid var(--line);
      background: transparent;
      color: var(--muted);
      cursor: pointer;
    }
    .tag-filter[data-active="true"] { background: var(--accent); color: #06111a; border-color: transparent; }
    .timeline-list { display: grid; gap: 12px; }
    .timeline-row {
      display: grid;
      grid-template-columns: 120px minmax(0, 1fr);
      gap: 16px;
      padding: 16px;
    }
    .chat-shell { display: grid; gap: 14px; }
    .chat-seed { display: grid; gap: 10px; }
    .chat-bubble {
      padding: 14px 16px;
      border-radius: 18px;
      border: 1px solid var(--line);
      background: rgba(255,255,255,0.03);
    }
    .chat-bubble.user { border-color: rgba(255,255,255,0.18); }
    .chat-form { display: flex; gap: 10px; flex-wrap: wrap; }
    .chat-form input {
      flex: 1 1 320px;
      min-height: 46px;
      padding: 0 14px;
      border-radius: 16px;
      border: 1px solid var(--line);
      background: rgba(255,255,255,0.03);
      color: var(--text);
    }
    .chat-form button {
      min-height: 46px;
      padding: 0 18px;
      border-radius: 16px;
      border: none;
      background: var(--accent);
      color: #06111a;
      cursor: pointer;
    }
    .footer-row { margin-top: 18px; color: var(--muted); }
    @media (max-width: 1080px) {
      .hero-grid, .scene-shell, .metric-grid, .feature-grid, .portfolio-grid {
        grid-template-columns: 1fr;
      }
      .timeline-row { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <main class="shell">
    <header class="topbar">
      <div>
        <p class="brand-kicker">${escapeHtml(experience.eyebrow)}</p>
        <h1>${escapeHtml(content.brand)}</h1>
      </div>
      <nav class="nav-links">${(content.nav || []).map((item) => `<a href="${escapeHtml(item.href)}">${escapeHtml(item.label)}</a>`).join("")}</nav>
    </header>
    <section class="hero-grid">
      <article class="hero-card">
        <p class="brand-kicker">${escapeHtml(content.hero.kicker)}</p>
        <h2 class="hero-title">${escapeHtml(content.hero.title)}</h2>
        <p class="hero-copy">${escapeHtml(content.hero.body)}</p>
        <div class="action-row">${renderActionButtons(content.hero.actions || [])}</div>
      </article>
      <aside class="hero-card">
        <p class="section-label">Metrics</p>
        <div class="metric-grid">${renderMetrics(content.metrics || [])}</div>
      </aside>
    </section>
    ${sections}
    <footer class="footer-row">
      <span>${escapeHtml(app.name)}</span>
      <span>${escapeHtml(experience.id)}</span>
      <span>${escapeHtml(content.footer)}</span>
    </footer>
  </main>
  ${renderClientRuntime(model)}
</body>
</html>`;
}

function buildSummary(model) {
  return {
    id: model.experience.id,
    mode: model.experience.mode,
    output_slug: model.experience.output_slug,
    page_title: model.experience.page_title,
    html_path: path.join(model.output_dir, "index.html"),
    manifest_path: path.join(model.output_dir, "site.manifest.json"),
    actor_server_path: path.join(model.output_dir, "actor-server.plan.json"),
    server_port: model.context.app.site_runtime.default_port
  };
}

export function buildExperience(appManifestPath, experienceId) {
  const model = buildModel(appManifestPath, experienceId);
  const html = renderDocument(model);
  const summary = buildSummary(model);
  return {
    ...summary,
    html,
    manifest: {
      experience: model.experience,
      theme: model.theme,
      content: model.content,
      scene: model.scene
    },
    actor_server: buildActorServerPlan(appManifestPath, model.experience.id)
  };
}

export function buildActorServerPlan(appManifestPath, experienceId) {
  const model = buildModel(appManifestPath, experienceId);
  return {
    id: model.experience.id,
    port: model.context.app.site_runtime.default_port,
    host: model.context.app.site_runtime.host,
    routes: model.content.server_routes || [],
    actors: model.content.actor_roles || [],
    page_title: model.experience.page_title,
    output_slug: model.experience.output_slug
  };
}

export function actorServerReport(appManifestPath, experienceId) {
  const plan = buildActorServerPlan(appManifestPath, experienceId);
  return [
    "Kain universal web actor-server report",
    `experience: ${plan.id}`,
    `host: ${plan.host}`,
    `port: ${plan.port}`,
    `route_count: ${plan.routes.length}`,
    `actor_count: ${plan.actors.length}`,
    `output_slug: ${plan.output_slug}`
  ].join("\n");
}

export function buildMatrix(appManifestPath) {
  const context = loadAppConfig(appManifestPath);
  const experienceIds = context.app.build.experiences || Object.keys(context.experiences);
  const experiences = experienceIds.map((id) => buildExperience(appManifestPath, id));
  return {
    default_experience: context.app.default_experience,
    output_root: context.app.output_root,
    experience_count: experiences.length,
    artifact_count: experiences.length * 3 + 1,
    server_port: context.app.site_runtime.default_port,
    experience_ids: experiences.map((entry) => entry.id)
  };
}

export function writeMatrix(appManifestPath) {
  const context = loadAppConfig(appManifestPath);
  const outputRoot = path.resolve(context.root_dir, context.app.output_root);
  ensureDir(outputRoot);
  const experienceIds = context.app.build.experiences || Object.keys(context.experiences);
  const built = experienceIds.map((id) => buildExperience(appManifestPath, id));
  for (const entry of built) {
    ensureDir(path.dirname(entry.html_path));
    fs.writeFileSync(entry.html_path, entry.html);
    writeJson(entry.manifest_path, entry.manifest);
    writeJson(entry.actor_server_path, entry.actor_server);
  }
  const summary = {
    default_experience: context.app.default_experience,
    output_root: context.app.output_root,
    experience_count: built.length,
    artifact_count: built.length * 3 + 1,
    server_port: context.app.site_runtime.default_port,
    experience_ids: built.map((entry) => entry.id)
  };
  writeJson(path.join(outputRoot, "matrix.summary.json"), summary);
  return summary;
}

function sendJson(response, statusCode, payload) {
  response.writeHead(statusCode, { "content-type": "application/json; charset=utf-8" });
  response.end(JSON.stringify(payload, null, 2));
}

function sendHtml(response, html) {
  response.writeHead(200, { "content-type": "text/html; charset=utf-8" });
  response.end(html);
}

function serveExperience(appManifestPath, experienceId) {
  const bundle = buildExperience(appManifestPath, experienceId);
  const plan = bundle.actor_server;
  const server = http.createServer((request, response) => {
    const url = request.url || "/";
    if (url === "/") {
      sendHtml(response, bundle.html);
      return;
    }
    if (url === "/api/runtime") {
      sendJson(response, 200, {
        experience: bundle.id,
        mode: bundle.mode,
        page_title: bundle.page_title
      });
      return;
    }
    if (url === "/api/actors") {
      sendJson(response, 200, plan);
      return;
    }
    if (url === "/api/chat") {
      sendJson(response, 200, bundle.manifest.content.chat_seed || []);
      return;
    }
    sendJson(response, 404, { error: "not_found", path: url });
  });
  server.listen(plan.port, plan.host, () => {
    process.stdout.write(`kain-web runtime serving ${bundle.id} at http://${plan.host}:${plan.port}\n`);
  });
}

function runCli() {
  const [command = "print", appManifestPath = "manifests/app.json", experienceId] = process.argv.slice(2);
  if (command === "build") {
    process.stdout.write(JSON.stringify(writeMatrix(appManifestPath), null, 2) + "\n");
    return;
  }
  if (command === "serve") {
    serveExperience(appManifestPath, experienceId);
    return;
  }
  if (command === "print") {
    process.stdout.write(JSON.stringify(buildMatrix(appManifestPath), null, 2) + "\n");
    return;
  }
  process.stderr.write(`unknown command '${command}'. expected build, serve, or print\n`);
  process.exitCode = 1;
}

if (process.argv[1] && import.meta.url === pathToFileURL(path.resolve(process.argv[1])).href) {
  runCli();
}
