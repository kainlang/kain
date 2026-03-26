import fs from "node:fs";
import http from "node:http";
import crypto from "node:crypto";
import { createRequire } from "node:module";
import path from "node:path";
import { pathToFileURL } from "node:url";

const require = createRequire(import.meta.url);

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

function writeText(filePath, value) {
  ensureDir(path.dirname(filePath));
  fs.writeFileSync(filePath, value);
}

function writeBinary(filePath, value) {
  ensureDir(path.dirname(filePath));
  fs.writeFileSync(filePath, value);
}

function appendText(filePath, value) {
  ensureDir(path.dirname(filePath));
  fs.appendFileSync(filePath, value);
}

function readJsonIfExists(filePath, fallbackValue) {
  if (!fs.existsSync(filePath)) return fallbackValue;
  try {
    return readJson(filePath);
  } catch {
    return fallbackValue;
  }
}

function getClientBundleConfig(app) {
  const config = app.client_bundle || null;
  if (!config || config.enabled !== true) {
    return null;
  }
  return {
    entry: config.entry || "helpers/client/main.tsx",
    out_dir: config.out_dir || "client",
    out_file: config.out_file || "kain-client.bundle.js",
    format: config.format || "esm",
    target: config.target || "es2022",
    minify: config.minify === true
  };
}

function getClientBundlePaths(context) {
  const config = getClientBundleConfig(context.app);
  if (!config) return null;
  const outDirAbs = path.resolve(context.root_dir, context.app.output_root, config.out_dir);
  const outFileAbs = path.resolve(outDirAbs, config.out_file);
  const metaFileAbs = path.resolve(outDirAbs, `${config.out_file}.meta.json`);
  return {
    config,
    out_dir_abs: outDirAbs,
    out_file_abs: outFileAbs,
    meta_file_abs: metaFileAbs,
    href_from_experience: `../${config.out_dir}/${config.out_file}`,
    href_from_server_root: `/client/${config.out_file}`
  };
}

function ensureClientBundle(context, options = {}) {
  const paths = getClientBundlePaths(context);
  if (!paths) return null;
  ensureDir(paths.out_dir_abs);
  const shouldRebuild = options.force === true || !fs.existsSync(paths.out_file_abs);
  if (!shouldRebuild) {
    return paths;
  }

  let esbuild;
  try {
    esbuild = require("esbuild");
  } catch (error) {
    throw new Error(
      `client bundle is enabled but 'esbuild' is missing. Run 'npm install' in the template first. (${error?.message || error})`
    );
  }

  const entryAbs = path.resolve(context.root_dir, paths.config.entry);
  const startedAt = new Date().toISOString();

  esbuild.buildSync({
    entryPoints: [entryAbs],
    bundle: true,
    platform: "browser",
    format: paths.config.format,
    target: paths.config.target,
    outfile: paths.out_file_abs,
    sourcemap: false,
    minify: paths.config.minify,
    jsxFactory: "h",
    jsxFragment: "Fragment",
    logLevel: "silent",
    loader: { ".ts": "ts", ".tsx": "tsx" }
  });

  writeJson(paths.meta_file_abs, {
    schema_version: 1,
    generated_at: startedAt,
    entry: paths.config.entry,
    out_file: path.basename(paths.out_file_abs),
    format: paths.config.format,
    target: paths.config.target
  });

  return paths;
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

function requireEntry(table, key, kind) {
  const value = table[key];
  if (!value) {
    throw new Error(`missing ${kind} '${key}' in universal web template`);
  }
  return value;
}

function slugify(value) {
  return String(value || "section")
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "-")
    .replace(/^-+|-+$/g, "")
    .slice(0, 80);
}

function uniqueStrings(values) {
  return [...new Set((values || []).filter(Boolean).map((value) => String(value)))];
}

function getByPath(rootValue, sourcePath, fallbackValue = undefined) {
  if (!sourcePath) {
    return rootValue ?? fallbackValue;
  }
  const segments = String(sourcePath)
    .split(".")
    .map((segment) => segment.trim())
    .filter(Boolean);
  let currentValue = rootValue;
  for (const segment of segments) {
    if (currentValue == null) {
      return fallbackValue;
    }
    currentValue = currentValue[segment];
  }
  return currentValue == null ? fallbackValue : currentValue;
}

function getModelValue(model, sourcePath, fallbackValue = undefined) {
  if (!sourcePath) {
    return fallbackValue;
  }
  if (sourcePath === "theme") return model.theme;
  if (sourcePath === "scene") return model.scene;
  if (sourcePath === "experience") return model.experience;
  if (sourcePath.startsWith("content.")) {
    return getByPath(model.content, sourcePath.slice("content.".length), fallbackValue);
  }
  if (sourcePath.startsWith("scene.")) {
    return getByPath(model.scene, sourcePath.slice("scene.".length), fallbackValue);
  }
  if (sourcePath.startsWith("theme.")) {
    return getByPath(model.theme, sourcePath.slice("theme.".length), fallbackValue);
  }
  if (sourcePath.startsWith("experience.")) {
    return getByPath(model.experience, sourcePath.slice("experience.".length), fallbackValue);
  }
  if (sourcePath.startsWith("app.")) {
    return getByPath(model.context.app, sourcePath.slice("app.".length), fallbackValue);
  }
  return getByPath(model.content, sourcePath, fallbackValue);
}

function escapeHtml(value) {
  return String(value ?? "")
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;");
}

function renderActionButtons(actions) {
  return (actions || [])
    .map(
      (action) =>
        `<a class="action ${escapeHtml(action.style || "primary")}" href="${escapeHtml(action.href || "#")}">${escapeHtml(action.label)}</a>`
    )
    .join("");
}

function renderRichText(value) {
  if (Array.isArray(value)) {
    return value.map((entry) => `<p>${escapeHtml(entry)}</p>`).join("");
  }
  if (typeof value === "string" && value.trim()) {
    return `<p>${escapeHtml(value)}</p>`;
  }
  return "";
}

function renderMetrics(metrics) {
  return (metrics || [])
    .map(
      (metric) => `<article class="metric-card" data-kain-component="metric-card">
  <p class="metric-value" data-target-value="${escapeHtml(metric.value)}">${escapeHtml(metric.value)}</p>
  <p class="metric-label">${escapeHtml(metric.label)}</p>
</article>`
    )
    .join("");
}

function renderCards(cards) {
  return (cards || [])
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
  const tags = uniqueStrings((entries || []).flatMap((entry) => entry.tags || []));
  const buttons = tags
    .map(
      (tag, index) =>
        `<button class="tag-filter" data-filter="${escapeHtml(tag)}"${index === 0 ? ' data-active="true"' : ""}>${escapeHtml(tag)}</button>`
    )
    .join("");
  const cards = (entries || [])
    .map(
      (entry) => `<article class="portfolio-card" data-tags="${escapeHtml((entry.tags || []).join(" "))}">
  <p class="card-kicker">${escapeHtml(entry.year || entry.kicker || "")}</p>
  <h3>${escapeHtml(entry.title)}</h3>
  <p>${escapeHtml(entry.summary || entry.body || "")}</p>
  <p class="portfolio-stack">${escapeHtml((entry.tags || []).join(" / "))}</p>
</article>`
    )
    .join("");
  return `<div class="portfolio-filters" data-kain-component="portfolio-filter">${buttons}</div><div class="portfolio-grid">${cards}</div>`;
}

function renderTimeline(items) {
  return (items || [])
    .map(
      (item) => `<article class="timeline-row">
  <p class="timeline-label">${escapeHtml(item.phase || item.label || "")}</p>
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
      <p class="card-kicker">${escapeHtml(scene.kicker || "")}</p>
      <h3>${escapeHtml(scene.title || "")}</h3>
      <p>${escapeHtml(scene.summary || "")}</p>
    </div>
  </div>
  <div class="scene-layers">${layers}</div>
  <div data-kain-island="scene" data-site-data="site.data.json"></div>
</section>`;
}

function renderChat(messages, panel = {}) {
  const seed = (messages || [])
    .map(
      (message) => `<article class="chat-bubble ${escapeHtml(message.role || "assistant")}">
  <p class="chat-role">${escapeHtml(message.role || "assistant")}</p>
  <p>${escapeHtml(message.text)}</p>
</article>`
    )
    .join("");
  return `<section class="chat-shell" data-kain-component="chat-lab">
  <div class="chat-seed">${seed}</div>
  <form class="chat-form" data-chat-endpoint="${escapeHtml(panel.endpoint || "/api/chat")}">
    <input name="prompt" type="text" placeholder="${escapeHtml(panel.placeholder || "Ask the site orchestrator for a launch plan")}" />
    <button type="submit">${escapeHtml(panel.button_label || "Send")}</button>
  </form>
  <div data-kain-island="chat" data-site-data="site.data.json"></div>
</section>`;
}

function renderActors(actors) {
  return (actors || [])
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
  return (routes || [])
    .map(
      (route) => `<article class="route-card">
  <p class="card-kicker">${escapeHtml(route.method || "GET")}</p>
  <h3>${escapeHtml(route.path)}</h3>
  <p>${escapeHtml(route.purpose || route.handler || "")}</p>
</article>`
    )
    .join("");
}

function renderPricing(tiers) {
  return `<div class="pricing-grid">${(tiers || [])
    .map(
      (tier) => `<article class="pricing-card">
  <p class="card-kicker">${escapeHtml(tier.kicker || "")}</p>
  <h3>${escapeHtml(tier.name)}</h3>
  <p class="pricing-price">${escapeHtml(tier.price)}</p>
  <p>${escapeHtml(tier.summary || "")}</p>
  <ul class="pricing-list">${(tier.features || []).map((feature) => `<li>${escapeHtml(feature)}</li>`).join("")}</ul>
  <div class="action-row">${renderActionButtons(tier.actions || [])}</div>
</article>`
    )
    .join("")}</div>`;
}

function renderTestimonials(entries) {
  return `<div class="testimonial-grid">${(entries || [])
    .map(
      (entry) => `<article class="testimonial-card">
  <p class="quote-mark">"</p>
  <p>${escapeHtml(entry.quote)}</p>
  <h3>${escapeHtml(entry.name)}</h3>
  <p class="metric-label">${escapeHtml(entry.role || "")}</p>
</article>`
    )
    .join("")}</div>`;
}

function renderFaq(items) {
  return `<div class="faq-list" data-kain-component="faq-list">${(items || [])
    .map(
      (item, index) => `<article class="faq-item"${index === 0 ? ' data-open="true"' : ""}>
  <button class="faq-question" type="button">${escapeHtml(item.question)}</button>
  <div class="faq-answer">${renderRichText(item.answer)}</div>
</article>`
    )
    .join("")}</div>`;
}

function renderLogoStrip(logos) {
  return `<div class="logo-strip">${(logos || [])
    .map(
      (logo) => `<article class="logo-pill">
  <span>${escapeHtml(logo.label || logo.name)}</span>
  <span>${escapeHtml(logo.detail || "")}</span>
</article>`
    )
    .join("")}</div>`;
}

function renderCommandLines(commands) {
  return `<div class="command-grid">${(commands || [])
    .map(
      (command) => `<article class="command-card">
  <p class="card-kicker">${escapeHtml(command.kicker || "Command")}</p>
  <h3>${escapeHtml(command.title || command.command)}</h3>
  <pre><code>${escapeHtml(command.command || "")}</code></pre>
  <p>${escapeHtml(command.body || "")}</p>
</article>`
    )
    .join("")}</div>`;
}

function renderLinkGroups(groups) {
  return `<div class="link-grid">${(groups || [])
    .map(
      (group) => `<article class="link-card">
  <p class="card-kicker">${escapeHtml(group.kicker || "")}</p>
  <h3>${escapeHtml(group.title)}</h3>
  <p>${escapeHtml(group.body || "")}</p>
  <div class="link-list">${(group.links || [])
    .map((link) => `<a class="inline-link" href="${escapeHtml(link.href || "#")}">${escapeHtml(link.label)}</a>`)
    .join("")}</div>
</article>`
    )
    .join("")}</div>`;
}

function renderDocsLinks(entries) {
  return `<div class="docs-grid">${(entries || [])
    .map(
      (entry) => `<article class="doc-card">
  <p class="card-kicker">${escapeHtml(entry.kicker || "Doc")}</p>
  <h3>${escapeHtml(entry.title)}</h3>
  <p>${escapeHtml(entry.summary || "")}</p>
  <a class="inline-link" href="${escapeHtml(entry.href || "#")}">${escapeHtml(entry.label || "Open")}</a>
</article>`
    )
    .join("")}</div>`;
}

function renderFormPanel(form) {
  const fields = (form.fields || [])
    .map((field) => {
      if (field.type === "textarea") {
        return `<label class="form-field">
  <span>${escapeHtml(field.label)}</span>
  <textarea name="${escapeHtml(field.name)}" rows="${escapeHtml(field.rows || 5)}" placeholder="${escapeHtml(field.placeholder || "")}"></textarea>
</label>`;
      }
      return `<label class="form-field">
  <span>${escapeHtml(field.label)}</span>
  <input name="${escapeHtml(field.name)}" type="${escapeHtml(field.type || "text")}" placeholder="${escapeHtml(field.placeholder || "")}" />
</label>`;
    })
    .join("");
  return `<section class="form-panel" data-kain-component="form-panel">
  <div class="form-copy">
    <p class="card-kicker">${escapeHtml(form.kicker || "Intake")}</p>
    <h3>${escapeHtml(form.title)}</h3>
    <p>${escapeHtml(form.body || "")}</p>
  </div>
  <form data-form-id="${escapeHtml(form.id || "contact")}" action="${escapeHtml(form.action || `/api/forms/${form.id || "contact"}`)}">
    <div class="form-grid">${fields}</div>
    <div class="action-row">
      <button class="action primary form-submit" type="submit">${escapeHtml(form.submit_label || "Submit")}</button>
      <span class="form-status" aria-live="polite"></span>
    </div>
  </form>
</section>`;
}

function renderSearchPanel(panel) {
  return `<section class="search-panel" data-kain-component="search-panel">
  <div class="form-copy">
    <p class="card-kicker">${escapeHtml(panel.kicker || "Search")}</p>
    <h3>${escapeHtml(panel.title || "Search the site system")}</h3>
    <p>${escapeHtml(panel.body || "Query docs, sections, routes, and experience notes from one local index.")}</p>
  </div>
  <form class="search-form" data-search-endpoint="${escapeHtml(panel.endpoint || "/api/search")}">
    <input name="query" type="text" placeholder="${escapeHtml(panel.placeholder || "search docs, routes, launches, actors...")}" />
    <button type="submit">${escapeHtml(panel.button_label || "Search")}</button>
  </form>
  <div class="search-results"></div>
</section>`;
}

function renderProcessSteps(steps) {
  return `<div class="process-grid">${(steps || [])
    .map(
      (step, index) => `<article class="process-card">
  <p class="card-kicker">Step ${index + 1}</p>
  <h3>${escapeHtml(step.title || step.name || `Step ${index + 1}`)}</h3>
  <p>${escapeHtml(step.body || step.summary || "")}</p>
</article>`
    )
    .join("")}</div>`;
}

function renderCapabilityMatrix(matrix) {
  const columns = matrix?.columns || [];
  const rows = matrix?.rows || [];
  const header = columns.map((column) => `<th>${escapeHtml(column)}</th>`).join("");
  const body = rows
    .map((row) => {
      const cells = (row.values || [])
        .map((value) => `<td>${escapeHtml(value)}</td>`)
        .join("");
      return `<tr><th>${escapeHtml(row.label || row.name || "")}</th>${cells}</tr>`;
    })
    .join("");
  return `<div class="matrix-shell">
  <table class="capability-matrix">
    <thead>
      <tr><th>Capability</th>${header}</tr>
    </thead>
    <tbody>${body}</tbody>
  </table>
</div>`;
}

function renderBlueprintGrid(blueprints) {
  return `<div class="feature-grid">${(blueprints || [])
    .map(
      (entry) => `<article class="feature-card blueprint-card">
  <p class="card-kicker">${escapeHtml(entry.kicker || "System")}</p>
  <h3>${escapeHtml(entry.title || entry.name)}</h3>
  <p>${escapeHtml(entry.body || entry.summary || "")}</p>
  <p class="portfolio-stack">${escapeHtml((entry.owned_by || entry.tags || []).join(" / "))}</p>
</article>`
    )
    .join("")}</div>`;
}

function renderPromptDeck(prompts) {
  return `<div class="prompt-grid" data-kain-component="prompt-deck">${(prompts || [])
    .map(
      (entry) => `<article class="prompt-card">
  <p class="card-kicker">${escapeHtml(entry.kicker || "Prompt")}</p>
  <h3>${escapeHtml(entry.title || entry.prompt)}</h3>
  <p>${escapeHtml(entry.body || entry.summary || "")}</p>
  <button class="action secondary prompt-launch" type="button" data-prompt-value="${escapeHtml(entry.prompt || entry.title || "")}">${escapeHtml(entry.button_label || "Try prompt")}</button>
</article>`
    )
    .join("")}</div>`;
}

function renderIntegrationGrid(integrations) {
  return `<div class="feature-grid">${(integrations || [])
    .map(
      (entry) => `<article class="feature-card integration-card">
  <p class="card-kicker">${escapeHtml(entry.category || "Integration")}</p>
  <h3>${escapeHtml(entry.name)}</h3>
  <p>${escapeHtml(entry.detail || entry.summary || "")}</p>
  <p class="portfolio-stack">${escapeHtml([entry.transport, entry.status].filter(Boolean).join(" / "))}</p>
</article>`
    )
    .join("")}</div>`;
}

function renderRealtimeChannels(channels) {
  const list = `<div class="timeline-list">${(channels || [])
    .map(
      (channel) => `<article class="timeline-row realtime-row">
  <p class="timeline-label">${escapeHtml(channel.protocol || channel.cadence || "Stream")}</p>
  <div>
    <h3>${escapeHtml(channel.name)}</h3>
    <p>${escapeHtml(channel.summary || "")}</p>
    <p class="portfolio-stack">${escapeHtml([channel.producer, ...(channel.consumers || [])].filter(Boolean).join(" / "))}</p>
  </div>
</article>`
    )
    .join("")}</div>`;
  return `<section class="realtime-shell">
  ${list}
  <div data-kain-island="realtime" data-site-data="site.data.json"></div>
</section>`;
}

function renderAuthPanel(auth) {
  const methods = (auth?.methods || [])
    .map(
      (method) => `<article class="feature-card auth-card">
  <p class="card-kicker">${escapeHtml(method.scope || "Auth")}</p>
  <h3>${escapeHtml(method.label)}</h3>
  <p>${escapeHtml(method.detail || "")}</p>
  <p class="portfolio-stack">${escapeHtml(method.status || "")}</p>
</article>`
    )
    .join("");
  return `<section class="auth-shell">
  <article class="hero-card">
    <p class="section-label">${escapeHtml(auth?.kicker || "Auth")}</p>
    <h3>${escapeHtml(auth?.title || "Authentication surface")}</h3>
    <p class="section-copy">${escapeHtml(auth?.body || "")}</p>
  </article>
  <div class="feature-grid">${methods}</div>
</section>`;
}

function renderAuthSession(auth) {
  return `<section class="auth-session-shell">
  <article class="hero-card">
    <p class="section-label">Session</p>
    <h3>${escapeHtml(auth?.session_title || "Local session preview")}</h3>
    <p class="section-copy">${escapeHtml(
      auth?.session_body ||
        "This is a local-only session endpoint so chat, uploads, and operator surfaces can bind identity without needing a full auth provider yet."
    )}</p>
  </article>
  <div data-kain-island="auth-session" data-site-data="site.data.json"></div>
</section>`;
}

function renderCommerceStack(commerce) {
  return `<div class="pricing-grid">${(commerce?.offers || [])
    .map(
      (offer) => `<article class="pricing-card commerce-card">
  <p class="card-kicker">${escapeHtml(offer.kicker || offer.cadence || "Offer")}</p>
  <h3>${escapeHtml(offer.name)}</h3>
  <p class="pricing-price">${escapeHtml(offer.price || offer.value || "Custom")}</p>
  <p>${escapeHtml(offer.summary || "")}</p>
  <ul class="pricing-list">${(offer.features || [])
    .map((feature) => `<li>${escapeHtml(feature)}</li>`)
    .join("")}</ul>
  <div class="action-row">${renderActionButtons(offer.actions || [])}</div>
</article>`
    )
    .join("")}</div>`;
}

function renderUploadsLab(uploads) {
  return `<section class="uploads-shell">
  <article class="hero-card">
    <p class="section-label">${escapeHtml(uploads?.kicker || "Uploads")}</p>
    <h3>${escapeHtml(uploads?.title || "File uploads (base64 local runtime)")}</h3>
    <p class="section-copy">${escapeHtml(
      uploads?.body || "Use this lane to capture images, attachments, or proofs before wiring a real storage backend."
    )}</p>
  </article>
  <div data-kain-island="uploads" data-site-data="site.data.json"></div>
</section>`;
}

function renderAnalyticsLab(analytics) {
  return `<section class="analytics-shell">
  <article class="hero-card">
    <p class="section-label">${escapeHtml(analytics?.kicker || "Analytics")}</p>
    <h3>${escapeHtml(analytics?.title || "Local analytics events")}</h3>
    <p class="section-copy">${escapeHtml(
      analytics?.body || "Capture client events into JSONL so operator lanes can reason about usage before integrating external analytics."
    )}</p>
  </article>
  <div data-kain-island="analytics" data-site-data="site.data.json"></div>
</section>`;
}

function renderDataCollections(collections) {
  return `<div class="feature-grid">${(collections || [])
    .map(
      (collection) => `<article class="feature-card data-card">
  <p class="card-kicker">${escapeHtml(collection.retention || "Data")}</p>
  <h3>${escapeHtml(collection.name)}</h3>
  <p>${escapeHtml(collection.purpose || collection.summary || "")}</p>
  <p class="portfolio-stack">${escapeHtml([collection.schema, collection.actor].filter(Boolean).join(" / "))}</p>
</article>`
    )
    .join("")}</div>`;
}

function renderAppShell(modules) {
  const cards = (modules || [])
    .map(
      (module) => `<article class="feature-card app-module-card">
  <p class="card-kicker">${escapeHtml(module.route || "module")}</p>
  <h3>${escapeHtml(module.name)}</h3>
  <p>${escapeHtml(module.summary || "")}</p>
  <p class="portfolio-stack">${escapeHtml((module.tags || []).join(" / "))}</p>
</article>`
    )
    .join("");
  return `<section class="app-shell">
  <div class="logo-pill"><span>React-esque UI lanes</span><span>manifest + island contract</span></div>
  <div class="feature-grid">${cards}</div>
  <div data-kain-island="app-shell" data-site-data="site.data.json"></div>
</section>`;
}

function renderSectionIntro(section) {
  const eyebrow = section.eyebrow || section.label || section.kicker;
  const title = section.title;
  const body = section.body;
  return `${eyebrow ? `<p class="section-label">${escapeHtml(eyebrow)}</p>` : ""}${title ? `<h2>${escapeHtml(title)}</h2>` : ""}${body ? `<p class="section-copy">${escapeHtml(body)}</p>` : ""}`;
}

const LEGACY_SECTION_MAP = {
  features: { kind: "card_grid", eyebrow: "Systems", title: "Reusable launch systems", source: "content.feature_cards" },
  story: { kind: "card_grid", eyebrow: "Narrative", title: "Story rails", source: "content.story_cards" },
  portfolio: { kind: "portfolio_grid", eyebrow: "Portfolio", title: "Case studies and work capsules", source: "content.portfolio_entries" },
  timeline: { kind: "timeline", eyebrow: "Timeline", title: "Build sequence", source: "content.timeline" },
  scene: { kind: "scene_spotlight", eyebrow: "Scene", title: "Immersive 3D block", source: "scene" },
  chat: { kind: "chat_lab", eyebrow: "Chat", title: "Conversation-first surface", source: "content.chat_seed" },
  actors: { kind: "actor_mesh", eyebrow: "Actors", title: "Actor mesh", source: "content.actor_roles" },
  server: { kind: "route_grid", eyebrow: "Server", title: "Route contract", source: "content.server_routes" },
  cta: { kind: "cta", eyebrow: "CTA", title_source: "content.cta.title", body_source: "content.cta.body", actions_source: "content.cta.actions" }
};

function normalizeSection(section, index) {
  if (typeof section === "string") {
    const mapped = LEGACY_SECTION_MAP[section] || { kind: "rich_text", title: section };
    return {
      id: slugify(section),
      ...mapped
    };
  }
  return {
    id: section.id || slugify(section.title || section.kind || `section-${index + 1}`),
    ...section
  };
}

function renderSectionBlock(section, model) {
  const normalized = normalizeSection(section, 0);
  const kind = normalized.kind || "rich_text";
  const introHtml = renderSectionIntro(normalized);
  let bodyHtml = "";

  if (kind === "metric_grid") {
    bodyHtml = `<div class="metric-grid">${renderMetrics(getModelValue(model, normalized.source, []))}</div>`;
  } else if (kind === "card_grid") {
    bodyHtml = `<div class="feature-grid">${renderCards(getModelValue(model, normalized.source, []))}</div>`;
  } else if (kind === "portfolio_grid") {
    bodyHtml = renderPortfolio(getModelValue(model, normalized.source, []));
  } else if (kind === "timeline") {
    bodyHtml = `<div class="timeline-list">${renderTimeline(getModelValue(model, normalized.source, []))}</div>`;
  } else if (kind === "scene_spotlight") {
    bodyHtml = renderScene(getModelValue(model, normalized.source || "scene", model.scene));
  } else if (kind === "chat_lab") {
    bodyHtml = renderChat(getModelValue(model, normalized.source, []), normalized);
  } else if (kind === "actor_mesh") {
    bodyHtml = `<div class="feature-grid">${renderActors(getModelValue(model, normalized.source, []))}</div>`;
  } else if (kind === "route_grid") {
    bodyHtml = `<div class="feature-grid">${renderRoutes(getModelValue(model, normalized.source, []))}</div>`;
  } else if (kind === "pricing") {
    bodyHtml = renderPricing(getModelValue(model, normalized.source, []));
  } else if (kind === "testimonials") {
    bodyHtml = renderTestimonials(getModelValue(model, normalized.source, []));
  } else if (kind === "faq") {
    bodyHtml = renderFaq(getModelValue(model, normalized.source, []));
  } else if (kind === "logo_strip") {
    bodyHtml = renderLogoStrip(getModelValue(model, normalized.source, []));
  } else if (kind === "command_lines") {
    bodyHtml = renderCommandLines(getModelValue(model, normalized.source, []));
  } else if (kind === "link_groups") {
    bodyHtml = renderLinkGroups(getModelValue(model, normalized.source, []));
  } else if (kind === "docs_grid") {
    bodyHtml = renderDocsLinks(getModelValue(model, normalized.source, []));
  } else if (kind === "form_panel") {
    bodyHtml = renderFormPanel(getModelValue(model, normalized.source, {}));
  } else if (kind === "search_panel") {
    bodyHtml = renderSearchPanel(getModelValue(model, normalized.source, {}));
  } else if (kind === "process_steps") {
    bodyHtml = renderProcessSteps(getModelValue(model, normalized.source, []));
  } else if (kind === "capability_matrix") {
    bodyHtml = renderCapabilityMatrix(getModelValue(model, normalized.source, {}));
  } else if (kind === "blueprint_grid") {
    bodyHtml = renderBlueprintGrid(getModelValue(model, normalized.source, []));
  } else if (kind === "prompt_deck") {
    bodyHtml = renderPromptDeck(getModelValue(model, normalized.source, []));
  } else if (kind === "integration_grid") {
    bodyHtml = renderIntegrationGrid(getModelValue(model, normalized.source, []));
  } else if (kind === "realtime_channels") {
    bodyHtml = renderRealtimeChannels(getModelValue(model, normalized.source, []));
  } else if (kind === "auth_panel") {
    bodyHtml = renderAuthPanel(getModelValue(model, normalized.source, {}));
  } else if (kind === "auth_session") {
    bodyHtml = renderAuthSession(getModelValue(model, normalized.source, {}));
  } else if (kind === "commerce_stack") {
    bodyHtml = renderCommerceStack(getModelValue(model, normalized.source, {}));
  } else if (kind === "uploads_lab") {
    bodyHtml = renderUploadsLab(getModelValue(model, normalized.source, {}));
  } else if (kind === "analytics_lab") {
    bodyHtml = renderAnalyticsLab(getModelValue(model, normalized.source, {}));
  } else if (kind === "data_collections") {
    bodyHtml = renderDataCollections(getModelValue(model, normalized.source, []));
  } else if (kind === "app_shell") {
    bodyHtml = renderAppShell(getModelValue(model, normalized.source, []));
  } else if (kind === "cta") {
    const title = getModelValue(model, normalized.title_source, normalized.title || "");
    const body = getModelValue(model, normalized.body_source, normalized.body || "");
    const actions = getModelValue(model, normalized.actions_source, normalized.actions || []);
    bodyHtml = `<section class="cta-panel">
  <h2>${escapeHtml(title)}</h2>
  <p>${escapeHtml(body)}</p>
  <div class="action-row">${renderActionButtons(actions)}</div>
</section>`;
  } else {
    bodyHtml = renderRichText(getModelValue(model, normalized.source, normalized.body || ""));
  }

  return `<section id="${escapeHtml(normalized.id)}" class="panel panel-${escapeHtml(kind)}">${introHtml}${bodyHtml}</section>`;
}

function buildDerivedSearchDocuments(model) {
  const documents = [];
  const pushDocument = (kind, title, summary, href) => {
    if (!title && !summary) return;
    documents.push({
      kind,
      title: title || kind,
      summary: summary || "",
      href: href || "#",
      tags: []
    });
  };

  for (const card of model.content.feature_cards || []) {
    pushDocument("feature", card.title, card.body, "#systems");
  }
  for (const entry of model.content.portfolio_entries || []) {
    documents.push({
      kind: "portfolio",
      title: entry.title,
      summary: entry.summary || "",
      href: "#work",
      tags: entry.tags || []
    });
  }
  for (const item of model.content.faq_items || []) {
    pushDocument("faq", item.question, Array.isArray(item.answer) ? item.answer.join(" ") : item.answer, "#faq");
  }
  for (const route of model.content.server_routes || []) {
    pushDocument("route", route.path, route.purpose, "#routes");
  }
  for (const actor of model.content.actor_roles || []) {
    pushDocument("actor", actor.name, actor.responsibility, "#actors");
  }
  for (const doc of model.content.docs_links || []) {
    documents.push({
      kind: "doc",
      title: doc.title,
      summary: doc.summary || "",
      href: doc.href || "#docs",
      tags: doc.tags || []
    });
  }
  for (const step of model.content.process_steps || []) {
    pushDocument("process", step.title, step.body, "#process");
  }
  for (const blueprint of model.content.blueprints || []) {
    documents.push({
      kind: "blueprint",
      title: blueprint.title || blueprint.name,
      summary: blueprint.body || blueprint.summary || "",
      href: "#blueprints",
      tags: blueprint.owned_by || blueprint.tags || []
    });
  }
  if (model.content.capability_matrix?.rows) {
    for (const row of model.content.capability_matrix.rows) {
      documents.push({
        kind: "capability",
        title: row.label || row.name,
        summary: (row.values || []).join(" | "),
        href: "#capabilities",
        tags: []
      });
    }
  }
  for (const module of model.content.app_modules || []) {
    documents.push({
      kind: "module",
      title: module.name,
      summary: module.summary || "",
      href: module.route || "#app",
      tags: module.tags || []
    });
  }
  for (const integration of model.content.integrations || []) {
    documents.push({
      kind: "integration",
      title: integration.name,
      summary: integration.detail || integration.summary || "",
      href: "#integrations",
      tags: [integration.category, integration.transport, integration.status].filter(Boolean)
    });
  }
  for (const channel of model.content.realtime_channels || []) {
    documents.push({
      kind: "realtime",
      title: channel.name,
      summary: channel.summary || "",
      href: "#realtime",
      tags: [channel.protocol, channel.cadence, channel.producer].filter(Boolean)
    });
  }
  for (const collection of model.content.data_collections || []) {
    documents.push({
      kind: "data",
      title: collection.name,
      summary: collection.purpose || collection.summary || "",
      href: "#data",
      tags: [collection.schema, collection.retention, collection.actor].filter(Boolean)
    });
  }
  for (const method of model.content.auth?.methods || []) {
    documents.push({
      kind: "auth",
      title: method.label,
      summary: method.detail || "",
      href: "#auth",
      tags: [method.scope, method.status].filter(Boolean)
    });
  }
  for (const offer of model.content.commerce?.offers || []) {
    documents.push({
      kind: "commerce",
      title: offer.name,
      summary: offer.summary || "",
      href: "#commerce",
      tags: [offer.kicker, offer.cadence].filter(Boolean)
    });
  }
  return documents;
}

function buildSiteData(model) {
  const configuredSearchDocuments = model.content.search_documents || [];
  const searchDocuments = configuredSearchDocuments.length > 0
    ? configuredSearchDocuments
    : buildDerivedSearchDocuments(model);
  const forms = Object.values(model.content.forms || {});
  const clientBundle = getClientBundlePaths(model.context);
  return {
    experience_id: model.experience.id,
    mode: model.experience.mode,
    output_slug: model.experience.output_slug,
    page_title: model.experience.page_title,
    theme: model.theme,
    scene: model.scene,
    client_bundle: clientBundle
      ? {
          enabled: true,
          href: clientBundle.href_from_experience,
          server_href: clientBundle.href_from_server_root,
          out_dir: clientBundle.config.out_dir,
          out_file: clientBundle.config.out_file
        }
      : { enabled: false },
    nav: model.content.nav || [],
    forms,
    actors: model.content.actor_roles || [],
    routes: model.content.server_routes || [],
    seo: {
      base_url: model.context.app.site?.base_url || "https://example.com",
      title: model.content.seo?.title || model.experience.page_title,
      description:
        model.content.seo?.description ||
        model.context.app.site?.default_description ||
        `${model.experience.page_title} built with the Kain universal web template.`,
      image: model.content.seo?.image || model.context.app.site?.default_social_image || "/social-card.png"
    },
    search_documents: searchDocuments,
    updates: model.content.news_items || model.content.timeline || [],
    client_features: model.context.app.site_runtime.client_features || [],
    prompt_presets: model.content.prompt_presets || [],
    blueprints: model.content.blueprints || [],
    capability_matrix: model.content.capability_matrix || null,
    auth: model.content.auth || null,
    commerce: model.content.commerce || null,
    uploads: model.content.uploads || null,
    analytics: model.content.analytics || null,
    app_modules: model.content.app_modules || [],
    integrations: model.content.integrations || [],
    realtime_channels: model.content.realtime_channels || [],
    data_collections: model.content.data_collections || []
  };
}

function buildExperienceCatalogEntries(context) {
  return Object.values(context.experiences).map((experience) => ({
    id: experience.id,
    mode: experience.mode,
    page_title: experience.page_title,
    output_slug: experience.output_slug,
    theme: experience.theme,
    content: experience.content,
    scene: experience.scene
  }));
}

export function loadJson(filePath) {
  return readJson(path.resolve(filePath));
}

export function loadAppConfig(appManifestPath) {
  const fullPath = path.resolve(appManifestPath);
  const app = readJson(fullPath);
  const manifestDir = path.dirname(fullPath);
  const rootDir = path.basename(manifestDir).toLowerCase() === "manifests" ? path.dirname(manifestDir) : manifestDir;
  return {
    app,
    root_dir: rootDir,
    manifest_dir: manifestDir,
    themes: loadRegistry(rootDir, app.registries.themes),
    content: loadRegistry(rootDir, app.registries.content),
    scenes: loadRegistry(rootDir, app.registries.scenes),
    experiences: loadRegistry(rootDir, app.registries.experiences)
  };
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

function renderClientRuntime(model, siteData) {
  const chatSeed = JSON.stringify(model.content.chat_seed || []);
  const searchDocuments = JSON.stringify(siteData.search_documents || []);
  const promptPresets = JSON.stringify(siteData.prompt_presets || []);
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
    const buttons = [...filterRoot.querySelectorAll('button')];
    const firstButton = buttons[0];
    if (firstButton) {
      const firstTag = firstButton.dataset.filter || '';
      for (const card of cards) {
        const tags = card.dataset.tags || '';
        card.style.display = tags.includes(firstTag) ? '' : 'none';
      }
    }
    for (const button of buttons) {
      button.addEventListener('click', () => {
        const tag = button.dataset.filter || '';
        for (const candidate of buttons) candidate.removeAttribute('data-active');
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
    const promptPresets = ${promptPresets};
    const seedBox = chatRoot.querySelector('.chat-seed');
    const form = chatRoot.querySelector('.chat-form');
    const input = form?.querySelector('input[name="prompt"]');
    const promptButtons = [...document.querySelectorAll('[data-kain-component="prompt-deck"] [data-prompt-value]')];
    for (const button of promptButtons) {
      button.addEventListener('click', () => {
        if (input) input.value = button.getAttribute('data-prompt-value') || '';
        form?.requestSubmit();
      });
    }
    if (input && promptPresets.length > 0 && !input.placeholder) {
      input.placeholder = promptPresets[0].prompt || input.placeholder || '';
    }
    form?.addEventListener('submit', async (event) => {
      event.preventDefault();
      const prompt = input?.value?.trim();
      if (!prompt) return;
      seedBox.insertAdjacentHTML('beforeend', '<article class="chat-bubble user"><p class="chat-role">user</p><p>' + prompt.replaceAll('<', '&lt;') + '</p></article>');
      const endpoint = form.getAttribute('data-chat-endpoint') || '/api/chat';
      try {
        const response = await fetch(endpoint + '?prompt=' + encodeURIComponent(prompt));
        const payload = await response.json();
        const text = payload.reply || payload.text || 'Template runtime is ready for custom actor-backed chat flows.';
        seedBox.insertAdjacentHTML('beforeend', '<article class="chat-bubble assistant"><p class="chat-role">assistant</p><p>' + String(text).replaceAll('<', '&lt;') + '</p></article>');
      } catch (error) {
        const fallback = seed[(seed.length - 1) % Math.max(seed.length, 1)] || { text: 'Template runtime is ready for custom actor-backed chat flows.' };
        seedBox.insertAdjacentHTML('beforeend', '<article class="chat-bubble assistant"><p class="chat-role">assistant</p><p>' + String(fallback.text).replaceAll('<', '&lt;') + '</p></article>');
      }
      if (input) input.value = '';
    });
  }

  for (const item of document.querySelectorAll('[data-kain-component="faq-list"] .faq-item')) {
    const button = item.querySelector('.faq-question');
    button?.addEventListener('click', () => {
      if (item.getAttribute('data-open') === 'true') {
        item.removeAttribute('data-open');
      } else {
        item.setAttribute('data-open', 'true');
      }
    });
  }

  for (const panel of document.querySelectorAll('[data-kain-component="form-panel"] form')) {
    panel.addEventListener('submit', async (event) => {
      event.preventDefault();
      const status = panel.querySelector('.form-status');
      const formData = new FormData(panel);
      const payload = Object.fromEntries(formData.entries());
      try {
        const response = await fetch(panel.getAttribute('action') || '/api/forms/contact', {
          method: 'POST',
          headers: { 'content-type': 'application/json' },
          body: JSON.stringify(payload)
        });
        const result = await response.json();
        if (status) status.textContent = result.message || 'Received';
        panel.reset();
      } catch (error) {
        if (status) status.textContent = 'Local form handler unavailable';
      }
    });
  }

  const searchRoot = document.querySelector('[data-kain-component="search-panel"]');
  if (searchRoot) {
    const form = searchRoot.querySelector('.search-form');
    const results = searchRoot.querySelector('.search-results');
    const localIndex = ${searchDocuments};
    form?.addEventListener('submit', async (event) => {
      event.preventDefault();
      const input = form.querySelector('input[name="query"]');
      const query = input?.value?.trim() || '';
      if (!query) return;
      let payload = { items: [] };
      try {
        const endpoint = form.getAttribute('data-search-endpoint') || '/api/search';
        const response = await fetch(endpoint + '?q=' + encodeURIComponent(query));
        payload = await response.json();
      } catch (error) {
        const lowered = query.toLowerCase();
        payload.items = localIndex.filter((entry) => {
          const haystack = [entry.title, entry.summary, ...(entry.tags || [])].join(' ').toLowerCase();
          return haystack.includes(lowered);
        }).slice(0, 6);
      }
      const items = payload.items || [];
      results.innerHTML = items.map((entry) => '<article class="search-result"><p class="card-kicker">' + String(entry.kind || 'result').replaceAll('<', '&lt;') + '</p><h3>' + String(entry.title || '').replaceAll('<', '&lt;') + '</h3><p>' + String(entry.summary || '').replaceAll('<', '&lt;') + '</p></article>').join('');
    });
  }
})();
</script>`;
}

function renderDocument(model, siteData) {
  const { app, experience, theme, content } = {
    app: model.context.app,
    experience: model.experience,
    theme: model.theme,
    content: model.content
  };
  const description = siteData.seo.description;
  const canonicalUrl = `${siteData.seo.base_url.replace(/\/$/, "")}/${escapeHtml(experience.output_slug)}/`;
  const sections = (experience.sections || []).map((section, index) => renderSectionBlock(normalizeSection(section, index), model)).join("");
  return `<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>${escapeHtml(siteData.seo.title)}</title>
  <meta name="description" content="${escapeHtml(description)}" />
  <link rel="canonical" href="${canonicalUrl}" />
  <meta property="og:title" content="${escapeHtml(siteData.seo.title)}" />
  <meta property="og:description" content="${escapeHtml(description)}" />
  <meta property="og:type" content="website" />
  <meta property="og:image" content="${escapeHtml(siteData.seo.image)}" />
  <meta name="theme-color" content="${escapeHtml(theme.accent)}" />
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
    html { scroll-behavior: smooth; }
    body {
      margin: 0;
      min-height: 100vh;
      background:
        radial-gradient(circle at top, var(--bg-top), rgba(0, 0, 0, 0) 48%),
        linear-gradient(180deg, var(--bg-top), var(--bg-bottom) 68%);
      color: var(--text);
      font-family: var(--font-body);
    }
    a { color: inherit; text-decoration: none; }
    button, input, textarea { font: inherit; }
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
    .nav-links, .link-list { display: flex; gap: 14px; flex-wrap: wrap; color: var(--muted); }
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
    .panel, .hero-card, .metric-card, .feature-card, .portfolio-card, .route-card, .actor-card, .timeline-row, .pricing-card, .testimonial-card, .doc-card, .link-card, .command-card, .logo-pill, .search-result, .process-card, .prompt-card {
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
    .section-copy, .hero-copy, .metric-label, .feature-card p, .portfolio-card p, .route-card p, .actor-card p, .timeline-row p, .footer-row, .testimonial-card p, .doc-card p, .link-card p, .search-result p {
      color: var(--muted);
      line-height: 1.5;
    }
    .metric-grid, .feature-grid, .portfolio-grid, .docs-grid, .link-grid, .command-grid, .pricing-grid, .testimonial-grid, .process-grid, .prompt-grid {
      display: grid;
      grid-template-columns: repeat(3, minmax(0, 1fr));
      gap: 14px;
    }
    .metric-card, .feature-card, .portfolio-card, .route-card, .actor-card, .pricing-card, .testimonial-card, .doc-card, .link-card, .command-card, .search-result, .process-card, .prompt-card {
      padding: 16px;
    }
    .blueprint-card { min-height: 220px; }
    .matrix-shell {
      overflow-x: auto;
      border-radius: 24px;
      border: 1px solid var(--line);
      background: rgba(255,255,255,0.02);
    }
    .capability-matrix {
      width: 100%;
      border-collapse: collapse;
      min-width: 720px;
    }
    .capability-matrix th, .capability-matrix td {
      padding: 14px 16px;
      border-bottom: 1px solid rgba(255,255,255,0.08);
      text-align: left;
    }
    .capability-matrix th {
      color: var(--text);
      font-family: var(--font-display);
      font-weight: 600;
    }
    .capability-matrix td { color: var(--muted); }
    .metric-value { margin: 0; font-size: clamp(1.8rem, 4vw, 3rem); color: var(--highlight); font-family: var(--font-display); }
    .action, .chat-form button, .search-form button, .faq-question {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      min-height: 44px;
      padding: 0 16px;
      border-radius: 999px;
      border: 1px solid var(--line);
      cursor: pointer;
    }
    .action.primary, .chat-form button, .search-form button {
      background: var(--accent);
      color: #06111a;
      border-color: transparent;
    }
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
    .scene-layers, .faq-list, .search-results { display: grid; gap: 10px; }
    .scene-layer, .logo-pill, .faq-item {
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
    .chat-shell, .form-panel, .search-panel { display: grid; gap: 14px; }
    .chat-seed { display: grid; gap: 10px; }
    .chat-bubble {
      padding: 14px 16px;
      border-radius: 18px;
      border: 1px solid var(--line);
      background: rgba(255,255,255,0.03);
    }
    .chat-bubble.user { border-color: rgba(255,255,255,0.18); }
    .chat-form, .search-form { display: flex; gap: 10px; flex-wrap: wrap; }
    .chat-form input, .search-form input, .form-field input, .form-field textarea {
      flex: 1 1 320px;
      min-height: 46px;
      padding: 12px 14px;
      border-radius: 16px;
      border: 1px solid var(--line);
      background: rgba(255,255,255,0.03);
      color: var(--text);
    }
    .footer-row { margin-top: 18px; color: var(--muted); }
    .pricing-price { font-size: 2rem; margin: 8px 0 10px; font-family: var(--font-display); color: var(--highlight); }
    .pricing-list {
      margin: 0;
      padding-left: 18px;
      color: var(--muted);
      display: grid;
      gap: 8px;
    }
    .quote-mark { font-size: 2.4rem; line-height: 1; color: var(--highlight); margin: 0 0 12px; }
    .faq-item { display: grid; }
    .faq-question {
      justify-content: space-between;
      width: 100%;
      background: transparent;
      color: var(--text);
    }
    .faq-answer { display: none; padding: 0 4px 8px; }
    .faq-item[data-open="true"] .faq-answer { display: block; }
    .inline-link { color: var(--accent-soft); }
    .command-card pre {
      margin: 0;
      padding: 14px;
      border-radius: 18px;
      overflow-x: auto;
      background: rgba(0,0,0,0.25);
      border: 1px solid rgba(255,255,255,0.06);
    }
    .prompt-card {
      display: grid;
      gap: 12px;
      align-content: start;
    }
    .auth-shell, .app-shell { display: grid; gap: 14px; }
    .kain-island {
      border-radius: 24px;
      border: 1px solid rgba(255,255,255,0.12);
      background: rgba(0, 0, 0, 0.22);
      padding: 16px;
      display: grid;
      gap: 14px;
    }
    .kain-island-header { display: grid; gap: 6px; }
    .kain-island-eyebrow {
      margin: 0;
      font-size: 10px;
      letter-spacing: 0.24em;
      text-transform: uppercase;
      color: var(--accent-soft);
    }
    .kain-island-title {
      margin: 0;
      font-family: var(--font-display);
      letter-spacing: -0.03em;
    }
    .kain-island-copy {
      margin: 0;
      color: var(--muted);
      line-height: 1.5;
    }
    .kain-island-body { display: grid; gap: 14px; }
    .kain-island-tabs { display: flex; gap: 10px; flex-wrap: wrap; }
    .kain-island-tab {
      min-height: 38px;
      padding: 0 14px;
      border-radius: 999px;
      border: 1px solid var(--line);
      background: transparent;
      color: var(--muted);
      cursor: pointer;
    }
    .kain-island-tab.active { background: var(--accent); color: #06111a; border-color: transparent; }
    .kain-island-panel { border-radius: 18px; border: 1px solid rgba(255,255,255,0.08); padding: 14px; }
    .kain-island-panel-kicker { margin: 0 0 6px; color: var(--accent-soft); font-size: 11px; letter-spacing: 0.18em; text-transform: uppercase; }
    .kain-island-panel-title { margin: 0; font-family: var(--font-display); }
    .kain-island-panel-copy { margin: 8px 0 0; color: var(--muted); line-height: 1.5; }
    .kain-island-panel-tags { margin: 10px 0 0; color: var(--muted); font-size: 12px; }
    .kain-island-panel-hint { margin-top: 12px; color: var(--muted); font-size: 12px; line-height: 1.5; }
    .kain-island-actions { display: flex; gap: 12px; align-items: center; flex-wrap: wrap; }
    .kain-island-actions button {
      min-height: 38px;
      padding: 0 14px;
      border-radius: 999px;
      border: 1px solid var(--line);
      background: rgba(255,255,255,0.04);
      color: var(--text);
      cursor: pointer;
    }
    .kain-island-actions button:disabled { opacity: 0.6; cursor: default; }
    .kain-island-status { color: var(--muted); font-size: 12px; }
    .kain-realtime-grid { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 12px; }
    .kain-realtime-card { border-radius: 18px; border: 1px solid rgba(255,255,255,0.08); padding: 14px; }
    .kain-realtime-kicker { margin: 0 0 6px; color: var(--accent-soft); font-size: 11px; letter-spacing: 0.18em; text-transform: uppercase; }
    .kain-realtime-title { margin: 0; font-family: var(--font-display); }
    .kain-realtime-copy { margin: 8px 0 0; color: var(--muted); line-height: 1.5; }
    .kain-realtime-meta { margin: 10px 0 0; color: var(--muted); font-size: 12px; }
    .kain-chat-log { max-height: 320px; overflow: auto; display: grid; gap: 10px; padding-right: 6px; }
    .kain-chat-bubble { border-radius: 18px; border: 1px solid rgba(255,255,255,0.08); padding: 12px 14px; }
    .kain-chat-bubble.user { background: rgba(255,255,255,0.02); }
    .kain-chat-bubble.assistant { background: rgba(90, 228, 255, 0.06); }
    .kain-chat-role { margin: 0 0 6px; color: var(--accent-soft); font-size: 11px; letter-spacing: 0.18em; text-transform: uppercase; }
    .kain-chat-text { margin: 0; color: var(--muted); line-height: 1.5; white-space: pre-wrap; }
    .kain-chat-form { display: flex; gap: 10px; align-items: center; }
    .kain-chat-form input {
      flex: 1;
      min-height: 44px;
      border-radius: 999px;
      border: 1px solid rgba(255,255,255,0.12);
      background: rgba(0,0,0,0.25);
      color: var(--text);
      padding: 0 14px;
    }
    .kain-chat-form button { min-height: 44px; padding: 0 16px; border-radius: 999px; }
    .kain-scene-mount { min-height: 320px; border-radius: 22px; border: 1px solid rgba(255,255,255,0.1); overflow: hidden; }
    .realtime-row { align-items: start; }
    .integration-card, .auth-card, .data-card, .app-module-card, .commerce-card { min-height: 220px; }
    .form-grid {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 12px;
    }
    .form-field {
      display: grid;
      gap: 6px;
      color: var(--muted);
    }
    .form-field textarea { min-height: 140px; resize: vertical; }
    .form-status { min-height: 24px; color: var(--accent-soft); }
    .search-results:empty::before {
      content: "Search results will appear here.";
      color: var(--muted);
    }
    @media (max-width: 1080px) {
      .hero-grid, .scene-shell, .metric-grid, .feature-grid, .portfolio-grid, .docs-grid, .link-grid, .command-grid, .pricing-grid, .testimonial-grid, .form-grid {
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
  ${siteData.client_bundle?.enabled ? `<script type="module" src="${escapeHtml(siteData.client_bundle.href)}" data-kain-client-bundle="true"></script>` : ""}
  ${renderClientRuntime(model, siteData)}
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
    site_data_path: path.join(model.output_dir, "site.data.json"),
    system_contract_path: path.join(model.output_dir, "system.contract.json"),
    ui_schema_path: path.join(model.output_dir, "ui.schema.json"),
    sitemap_path: path.join(model.output_dir, "sitemap.xml"),
    robots_path: path.join(model.output_dir, "robots.txt"),
    feed_path: path.join(model.output_dir, "feed.xml"),
    server_port: model.context.app.site_runtime.default_port,
    output_dir: model.output_dir,
    route_count: (model.content.server_routes || []).length,
    actor_count: (model.content.actor_roles || []).length,
    form_count: Object.keys(model.content.forms || {}).length,
    search_document_count: (model.content.search_documents || []).length
  };
}

function buildSitemap(siteData) {
  const url = `${siteData.seo.base_url.replace(/\/$/, "")}/${siteData.output_slug}/`;
  return `<?xml version="1.0" encoding="UTF-8"?>
<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">
  <url>
    <loc>${escapeHtml(url)}</loc>
  </url>
</urlset>
`;
}

function buildRobots(siteData) {
  return `User-agent: *\nAllow: /\nSitemap: ${siteData.seo.base_url.replace(/\/$/, "")}/${siteData.output_slug}/sitemap.xml\n`;
}

function buildFeed(siteData) {
  const siteUrl = `${siteData.seo.base_url.replace(/\/$/, "")}/${siteData.output_slug}/`;
  const items = (siteData.updates || [])
    .slice(0, 8)
    .map((entry, index) => {
      const title = entry.title || entry.phase || `Update ${index + 1}`;
      const description = entry.body || entry.summary || "";
      return `<item><title>${escapeHtml(title)}</title><link>${escapeHtml(siteUrl)}</link><description>${escapeHtml(description)}</description></item>`;
    })
    .join("");
  return `<?xml version="1.0" encoding="UTF-8"?>
<rss version="2.0">
  <channel>
    <title>${escapeHtml(siteData.page_title)}</title>
    <link>${escapeHtml(siteUrl)}</link>
    <description>${escapeHtml(siteData.seo.description)}</description>
    ${items}
  </channel>
</rss>
`;
}

function buildSystemContract(model, siteData, actorServerPlan) {
  return {
    template: model.context.app.name,
    experience: {
      id: model.experience.id,
      mode: model.experience.mode,
      output_slug: model.experience.output_slug,
      page_title: model.experience.page_title
    },
    client_bundle: siteData.client_bundle || { enabled: false },
    ui_preview: {
      native_preview_entrypoint: "src/native_preview.kn",
      client_features: siteData.client_features || []
    },
    streaming: {
      chat: { http: "/api/chat", sse: "/api/chat/stream", ws: "/ws/chat" },
      realtime: { sse: "/api/realtime/stream", ws: "/ws/realtime" }
    },
    sessions: {
      me: "/api/auth/session",
      login: "/api/auth/session/login",
      logout: "/api/auth/session/logout"
    },
    uploads: {
      upload: "/api/uploads",
      serve_prefix: "/uploads/"
    },
    analytics: {
      event: "/api/analytics/event",
      events: "/api/analytics/events"
    },
    auth: siteData.auth || null,
    commerce: siteData.commerce || null,
    app_modules: siteData.app_modules || [],
    integrations: siteData.integrations || [],
    realtime_channels: siteData.realtime_channels || [],
    data_collections: siteData.data_collections || [],
    routes: actorServerPlan.routes,
    actors: actorServerPlan.actors,
    forms: actorServerPlan.forms
  };
}

function buildUiSchema(model, siteData) {
  const islandKindForSectionKind = (kind) => {
    if (kind === "app_shell") return "app-shell";
    if (kind === "realtime_channels") return "realtime";
    if (kind === "scene_spotlight") return "scene";
    if (kind === "chat_lab") return "chat";
    if (kind === "auth_session") return "auth-session";
    if (kind === "uploads_lab") return "uploads";
    if (kind === "analytics_lab") return "analytics";
    return null;
  };

  return {
    schema_version: 2,
    experience_id: model.experience.id,
    page_title: model.experience.page_title,
    react_like_runtime: {
      engine: "preact",
      bundler: "esbuild",
      island_attribute: "data-kain-island",
      site_data_path: "site.data.json",
      client_bundle: siteData.client_bundle || { enabled: false }
    },
    sections: (model.experience.sections || []).map((section, index) => {
      const normalized = normalizeSection(section, index);
      const island = islandKindForSectionKind(normalized.kind || "rich_text");
      return {
        id: normalized.id,
        kind: normalized.kind || "rich_text",
        title: normalized.title || null,
        source: normalized.source || null,
        island
      };
    }),
    island_contract: {
      mount_points: (model.experience.sections || []).map((section, index) => {
        const normalized = normalizeSection(section, index);
        const island = islandKindForSectionKind(normalized.kind || "rich_text");
        if (!island) return null;
        return {
          section_id: normalized.id,
          island_kind: island,
          site_data: "site.data.json",
          server_endpoints: island === "chat"
            ? { chat: "/api/chat", stream: "/api/chat/stream", ws: "/ws/chat" }
            : island === "realtime"
              ? { stream: "/api/realtime/stream", ws: "/ws/realtime" }
              : island === "scene"
                ? { scene: "/api/scene" }
                : island === "auth-session"
                  ? { me: "/api/auth/session", login: "/api/auth/session/login", logout: "/api/auth/session/logout" }
                  : island === "uploads"
                    ? { upload: "/api/uploads", serve_prefix: "/uploads/" }
                    : island === "analytics"
                      ? { event: "/api/analytics/event", events: "/api/analytics/events" }
                      : { app: "/api/app" }
        };
      }).filter(Boolean)
    },
    components: {
      hero: {
        actions: (model.content.hero?.actions || []).length
      },
      forms: (siteData.forms || []).map((form) => ({
        id: form.id,
        action: form.action || `/api/forms/${form.id}`,
        field_count: (form.fields || []).length
      })),
      app_modules: (siteData.app_modules || []).map((entry) => ({
        name: entry.name,
        route: entry.route || null,
        tags: entry.tags || []
      })),
      integrations: (siteData.integrations || []).map((entry) => ({
        name: entry.name,
        category: entry.category || null,
        transport: entry.transport || null
      }))
    }
  };
}

export function buildSiteSystemContract(appManifestPath, experienceId) {
  const model = buildModel(appManifestPath, experienceId);
  const siteData = buildSiteData(model);
  const actorServerPlan = buildActorServerPlan(appManifestPath, model.experience.id);
  return buildSystemContract(model, siteData, actorServerPlan);
}

export function buildExperienceUiSchema(appManifestPath, experienceId) {
  const model = buildModel(appManifestPath, experienceId);
  const siteData = buildSiteData(model);
  return buildUiSchema(model, siteData);
}

export function buildExperience(appManifestPath, experienceId) {
  const model = buildModel(appManifestPath, experienceId);
  const siteData = buildSiteData(model);
  const summary = buildSummary(model);
  const actorServerPlan = buildActorServerPlan(appManifestPath, model.experience.id);
  return {
    ...summary,
    html: renderDocument(model, siteData),
    manifest: {
      experience: model.experience,
      theme: model.theme,
      content: model.content,
      scene: model.scene
    },
    actor_server: actorServerPlan,
    site_data: siteData,
    system_contract: buildSystemContract(model, siteData, actorServerPlan),
    ui_schema: buildUiSchema(model, siteData),
    sitemap_xml: buildSitemap(siteData),
    robots_txt: buildRobots(siteData),
    feed_xml: buildFeed(siteData)
  };
}

export function buildCatalog(appManifestPath) {
  const context = loadAppConfig(appManifestPath);
  return {
    template: context.app.name,
    default_experience: context.app.default_experience,
    output_root: context.app.output_root,
    experiences: buildExperienceCatalogEntries(context)
  };
}

function buildChatReply(bundle, plan, prompt) {
  const lowered = String(prompt || "").toLowerCase();
  const laneMatches = [
    { keywords: ["business", "pricing", "marketing", "landing"], experience: "business_launch", label: "business launch" },
    { keywords: ["portfolio", "case study", "work"], experience: "portfolio_signal", label: "portfolio" },
    { keywords: ["3d", "immersive", "scene", "webgpu"], experience: "immersive_luminous", label: "immersive 3D" },
    { keywords: ["chat", "assistant", "conversation", "prompt"], experience: "chat_orbit", label: "chat-first" },
    { keywords: ["actor", "server", "route", "mesh"], experience: "actor_mesh_foundry", label: "actor server" },
    { keywords: ["docs", "knowledge", "search", "guide"], experience: "knowledge_atlas", label: "knowledge hub" },
    { keywords: ["command", "control", "operations", "ops", "dashboard", "deploy"], experience: "operator_foundry", label: "operator hub" },
    { keywords: ["app", "portal", "workspace", "dashboard app", "members"], experience: "app_foundry", label: "product app" },
    { keywords: ["commerce", "checkout", "pricing page", "sell", "membership"], experience: "commerce_signal", label: "commerce system" },
    { keywords: ["realtime", "stream", "websocket", "live", "incident"], experience: "realtime_constellation", label: "realtime ops" }
  ];
  const matchedLane = laneMatches.find((entry) => entry.keywords.some((keyword) => lowered.includes(keyword)));
  const matchedPrompts = (bundle.site_data.prompt_presets || []).filter((entry) =>
    [entry.title, entry.prompt, entry.body].join(" ").toLowerCase().includes(lowered)
  );
  const formIds = (bundle.site_data.forms || []).map((form) => form.id).join(", ") || "none";
  const nextLane = matchedLane ? `${matchedLane.label} via '${matchedLane.experience}'` : `hybrid via '${bundle.id}'`;
  const suggestedPrompt = matchedPrompts[0]?.prompt || null;
  const routeCount = plan.routes.length;
  const actorCount = plan.actors.length;
  const suggestion = suggestedPrompt ? ` Suggested prompt: "${suggestedPrompt}".` : "";
  return `Local Kain web runtime received '${prompt}'. Route this request through ${nextLane}. Current experience '${bundle.id}' exposes ${routeCount} routes, ${actorCount} actors, and forms [${formIds}].${suggestion}`;
}

function buildApiRoutes(model, siteData) {
  const builtInRoutes = [
    { method: "GET", path: "/", purpose: "serves the experience shell", actor: "site_renderer" },
    { method: "GET", path: "/site.data.json", purpose: "returns the flattened site data payload", actor: "site_renderer" },
    { method: "GET", path: "/sitemap.xml", purpose: "returns sitemap output for the current experience", actor: "site_renderer" },
    { method: "GET", path: "/robots.txt", purpose: "returns crawler policy", actor: "site_renderer" },
    { method: "GET", path: "/feed.xml", purpose: "returns the local update feed", actor: "site_renderer" },
    { method: "GET", path: "/api/runtime", purpose: "returns active runtime metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/catalog", purpose: "returns the available experience catalog", actor: "runtime_reporter" },
    { method: "GET", path: "/api/routes", purpose: "returns the route contract", actor: "mesh_supervisor" },
    { method: "GET", path: "/api/site", purpose: "returns site data and seo metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/scene", purpose: "returns the current scene descriptor", actor: "site_renderer" },
    { method: "GET", path: "/api/forms", purpose: "returns the available form contracts", actor: "intake_collector" },
    { method: "GET", path: "/api/search/documents", purpose: "returns the local search document index", actor: "search_indexer" },
    { method: "GET", path: "/api/search", purpose: "queries the local search document index", actor: "search_indexer" },
    { method: "GET", path: "/api/chat", purpose: "returns chat seed messages or a local reply", actor: "chat_seed_router" },
    { method: "POST", path: "/api/chat", purpose: "accepts a prompt payload and returns a local reply", actor: "chat_seed_router" },
    { method: "GET", path: "/api/chat/stream", purpose: "returns a server-sent event preview for local chat pipelines", actor: "chat_seed_router" },
    { method: "GET", path: "/api/app", purpose: "returns the app module manifest for react-like workspace shells", actor: "runtime_reporter" },
    { method: "GET", path: "/api/auth", purpose: "returns authentication strategy metadata", actor: "auth_gateway" },
    { method: "GET", path: "/api/auth/session", purpose: "returns the current session identity (cookie-backed)", actor: "auth_gateway" },
    { method: "POST", path: "/api/auth/session/login", purpose: "creates a local session identity (dev-only)", actor: "auth_gateway" },
    { method: "POST", path: "/api/auth/session/logout", purpose: "clears the active session identity", actor: "auth_gateway" },
    { method: "GET", path: "/api/commerce", purpose: "returns sellable offers and membership metadata", actor: "commerce_orchestrator" },
    { method: "GET", path: "/api/data", purpose: "returns typed collection and persistence metadata", actor: "data_keeper" },
    { method: "GET", path: "/api/integrations", purpose: "returns upstream system connectors and transports", actor: "integration_router" },
    { method: "POST", path: "/api/uploads", purpose: "accepts base64 uploads and persists them under the runtime folder", actor: "upload_gate" },
    { method: "GET", path: "/uploads/*", purpose: "serves uploaded files from the runtime uploads folder (local server only)", actor: "upload_gate" },
    { method: "POST", path: "/api/analytics/event", purpose: "captures client analytics events to JSONL", actor: "analytics_sentinel" },
    { method: "GET", path: "/api/analytics/events", purpose: "returns recent analytics events (local server only)", actor: "analytics_sentinel" },
    { method: "GET", path: "/api/realtime", purpose: "returns live channel descriptors and event cadence", actor: "signal_broker" },
    { method: "GET", path: "/api/realtime/stream", purpose: "returns a server-sent event preview for realtime channels", actor: "signal_broker" },
    { method: "WS", path: "/ws/realtime", purpose: "websocket stream for realtime channels", actor: "signal_broker" },
    { method: "WS", path: "/ws/chat", purpose: "websocket message lane for chat experiments", actor: "chat_seed_router" },
    { method: "GET", path: "/api/system.contract.json", purpose: "returns the complete website system contract", actor: "runtime_reporter" },
    { method: "GET", path: "/api/ui.schema.json", purpose: "returns the UI composition schema", actor: "runtime_reporter" },
    { method: "GET", path: "/api/actors", purpose: "returns actor topology and role descriptions", actor: "mesh_supervisor" },
    { method: "GET", path: "/healthz", purpose: "simple health response for local supervision", actor: "mesh_supervisor" }
  ];

  if (siteData.client_bundle?.enabled && siteData.client_bundle.out_file) {
    builtInRoutes.push({
      method: "GET",
      path: `/client/${siteData.client_bundle.out_file}`,
      purpose: "serves the bundled Preact + Three.js client islands",
      actor: "site_renderer"
    });
    builtInRoutes.push({
      method: "GET",
      path: `/client/${siteData.client_bundle.out_file}.meta.json`,
      purpose: "serves metadata for the bundled client islands",
      actor: "site_renderer"
    });
  }
  const formRoutes = (siteData.forms || []).map((form) => ({
    method: "POST",
    path: form.action || `/api/forms/${form.id}`,
    purpose: form.body || `accepts submissions for ${form.title}`,
    actor: "intake_collector"
  }));
  return [...builtInRoutes, ...formRoutes, ...(model.content.server_routes || [])];
}

export function buildActorServerPlan(appManifestPath, experienceId) {
  const model = buildModel(appManifestPath, experienceId);
  const siteData = buildSiteData(model);
  return {
    id: model.experience.id,
    port: model.context.app.site_runtime.default_port,
    host: model.context.app.site_runtime.host,
    routes: buildApiRoutes(model, siteData),
    actors: model.content.actor_roles || [],
    forms: siteData.forms || [],
    auth: siteData.auth || null,
    commerce: siteData.commerce || null,
    app_modules: siteData.app_modules || [],
    integrations: siteData.integrations || [],
    realtime_channels: siteData.realtime_channels || [],
    data_collections: siteData.data_collections || [],
    catalog: buildExperienceCatalogEntries(model.context),
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
    `form_count: ${plan.forms.length}`,
    `output_slug: ${plan.output_slug}`
  ].join("\n");
}

function buildWrittenCatalog(context, built) {
  return {
    template: context.app.name,
    default_experience: context.app.default_experience,
    output_root: context.app.output_root,
    experiences: built.map((entry) => ({
      id: entry.id,
      mode: entry.mode,
      output_slug: entry.output_slug,
      page_title: entry.page_title,
      files: {
        html: path.basename(entry.html_path),
        manifest: path.basename(entry.manifest_path),
        actor_server: path.basename(entry.actor_server_path),
        site_data: path.basename(entry.site_data_path),
        system_contract: path.basename(entry.system_contract_path),
        ui_schema: path.basename(entry.ui_schema_path),
        sitemap: path.basename(entry.sitemap_path),
        robots: path.basename(entry.robots_path),
        feed: path.basename(entry.feed_path)
      }
    }))
  };
}

export function buildMatrix(appManifestPath) {
  const context = loadAppConfig(appManifestPath);
  const clientBundleEnabled = Boolean(getClientBundlePaths(context));
  const experienceIds = context.app.build.experiences || Object.keys(context.experiences);
  const experiences = experienceIds.map((id) => buildExperience(appManifestPath, id));
  return {
    default_experience: context.app.default_experience,
    output_root: context.app.output_root,
    experience_count: experiences.length,
    artifact_count: experiences.length * 9 + 2 + (clientBundleEnabled ? 2 : 0),
    server_port: context.app.site_runtime.default_port,
    experience_ids: experiences.map((entry) => entry.id),
    client_features: context.app.site_runtime.client_features || [],
    modes: experiences.map((entry) => entry.mode),
    client_bundle: clientBundleEnabled ? { enabled: true } : { enabled: false }
  };
}

export function writeMatrix(appManifestPath) {
  const context = loadAppConfig(appManifestPath);
  const outputRoot = path.resolve(context.root_dir, context.app.output_root);
  ensureDir(outputRoot);
  ensureClientBundle(context, { force: false });
  const experienceIds = context.app.build.experiences || Object.keys(context.experiences);
  const built = experienceIds.map((id) => buildExperience(appManifestPath, id));
  for (const entry of built) {
    ensureDir(path.dirname(entry.html_path));
    writeText(entry.html_path, entry.html);
    writeJson(entry.manifest_path, entry.manifest);
    writeJson(entry.actor_server_path, entry.actor_server);
    writeJson(entry.site_data_path, entry.site_data);
    writeJson(entry.system_contract_path, entry.system_contract);
    writeJson(entry.ui_schema_path, entry.ui_schema);
    writeText(entry.sitemap_path, entry.sitemap_xml);
    writeText(entry.robots_path, entry.robots_txt);
    writeText(entry.feed_path, entry.feed_xml);
  }
  const summary = {
    default_experience: context.app.default_experience,
    output_root: context.app.output_root,
    experience_count: built.length,
    artifact_count: built.length * 9 + 2 + (getClientBundlePaths(context) ? 2 : 0),
    server_port: context.app.site_runtime.default_port,
    experience_ids: built.map((entry) => entry.id),
    client_features: context.app.site_runtime.client_features || [],
    modes: built.map((entry) => entry.mode)
  };
  writeJson(path.join(outputRoot, "matrix.summary.json"), summary);
  writeJson(path.join(outputRoot, "experience-catalog.json"), buildWrittenCatalog(context, built));
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

function sendText(response, statusCode, payload, contentType) {
  response.writeHead(statusCode, { "content-type": contentType });
  response.end(payload);
}

function sendFile(response, filePath, contentType) {
  const payload = fs.readFileSync(filePath);
  response.writeHead(200, { "content-type": contentType });
  response.end(payload);
}

function contentTypeForPath(filePath) {
  const ext = path.extname(filePath).toLowerCase();
  if (ext === ".js" || ext === ".mjs") return "text/javascript; charset=utf-8";
  if (ext === ".json") return "application/json; charset=utf-8";
  if (ext === ".map") return "application/json; charset=utf-8";
  if (ext === ".css") return "text/css; charset=utf-8";
  return "application/octet-stream";
}

function parseRequestBody(request) {
  return new Promise((resolve, reject) => {
    const chunks = [];
    request.on("data", (chunk) => chunks.push(chunk));
    request.on("end", () => {
      const body = Buffer.concat(chunks).toString("utf8");
      if (!body) {
        resolve({});
        return;
      }
      try {
        resolve(JSON.parse(body));
      } catch (error) {
        resolve({ raw: body });
      }
    });
    request.on("error", reject);
  });
}

function persistSubmission(bundle, formId, payload) {
  const logPath = path.join(bundle.output_dir || path.dirname(bundle.html_path), "runtime", "submissions", `${formId}.jsonl`);
  appendText(logPath, JSON.stringify({ received_at: new Date().toISOString(), payload }) + "\n");
  return logPath;
}

function runtimeRootForBundle(bundle) {
  return path.join(bundle.output_dir || path.dirname(bundle.html_path), "runtime");
}

function parseCookies(headerValue) {
  const out = new Map();
  const raw = String(headerValue || "");
  if (!raw.trim()) return out;
  for (const part of raw.split(";")) {
    const [name, ...rest] = part.split("=");
    const key = String(name || "").trim();
    if (!key) continue;
    out.set(key, decodeURIComponent(rest.join("=").trim() || ""));
  }
  return out;
}

function formatSetCookie(name, value, options = {}) {
  const encodedValue = encodeURIComponent(String(value ?? ""));
  const segments = [`${name}=${encodedValue}`];
  segments.push(`Path=${options.path || "/"}`);
  if (options.httpOnly !== false) segments.push("HttpOnly");
  if (options.sameSite) segments.push(`SameSite=${options.sameSite}`);
  if (options.maxAgeSeconds != null) segments.push(`Max-Age=${Number(options.maxAgeSeconds)}`);
  if (options.secure === true) segments.push("Secure");
  return segments.join("; ");
}

function sanitizedFileName(value) {
  return String(value || "upload.bin")
    .replaceAll(/[^a-zA-Z0-9._-]+/g, "-")
    .replaceAll(/-+/g, "-")
    .replaceAll(/^-+|-+$/g, "")
    .slice(0, 96) || "upload.bin";
}

function persistAnalyticsEvent(bundle, eventPayload) {
  const root = runtimeRootForBundle(bundle);
  const logPath = path.join(root, "analytics", "events.jsonl");
  const entry = { received_at: new Date().toISOString(), ...eventPayload };
  appendText(logPath, JSON.stringify(entry) + "\n");
  return { logPath, entry };
}

function persistUpload(bundle, uploadPayload) {
  const root = runtimeRootForBundle(bundle);
  const today = new Date().toISOString().slice(0, 10);
  const folder = path.join(root, "uploads", today);
  ensureDir(folder);

  const fileName = sanitizedFileName(uploadPayload.filename || uploadPayload.name || "upload.bin");
  const id = crypto.randomUUID();
  const storedName = `${id}-${fileName}`;
  const absPath = path.join(folder, storedName);

  const rawBase64 = String(uploadPayload.content_base64 || uploadPayload.base64 || "");
  const base64 = rawBase64.includes(",") ? rawBase64.split(",").slice(1).join(",") : rawBase64;
  const bytes = Buffer.from(base64, "base64");
  writeBinary(absPath, bytes);

  const relativeHref = `/uploads/${today}/${storedName}`;
  return {
    abs_path: absPath,
    href: relativeHref,
    byte_length: bytes.byteLength,
    content_type: String(uploadPayload.content_type || uploadPayload.type || "application/octet-stream"),
    filename: fileName
  };
}

function loadSessionStore(bundle) {
  const storePath = path.join(runtimeRootForBundle(bundle), "auth", "sessions.json");
  const store = readJsonIfExists(storePath, { sessions: {} });
  return { storePath, store };
}

function saveSessionStore(storePath, store) {
  writeJson(storePath, store);
}

function createSession(bundle, payload) {
  const { storePath, store } = loadSessionStore(bundle);
  const sessionId = crypto.randomUUID();
  store.sessions = store.sessions || {};
  store.sessions[sessionId] = {
    id: sessionId,
    created_at: new Date().toISOString(),
    email: payload.email || null,
    invite_code: payload.invite_code || null,
    roles: payload.roles || []
  };
  saveSessionStore(storePath, store);
  return store.sessions[sessionId];
}

function getSession(bundle, request) {
  const cookies = parseCookies(request.headers.cookie);
  const sessionId = cookies.get("kain_session_id") || null;
  if (!sessionId) return null;
  const { store } = loadSessionStore(bundle);
  return store.sessions?.[sessionId] || null;
}

async function serveExperience(appManifestPath, experienceId) {
  const context = loadAppConfig(appManifestPath);
  const clientBundle = ensureClientBundle(context, { force: false });
  const bundle = buildExperience(appManifestPath, experienceId);
  const plan = bundle.actor_server;
  const searchIndex = bundle.site_data.search_documents || [];
  const chatSeed = bundle.manifest.content.chat_seed || [];
  const formsByPath = new Map(
    (bundle.site_data.forms || []).map((form) => [form.action || `/api/forms/${form.id}`, form])
  );
  const server = http.createServer(async (request, response) => {
    const requestUrl = new URL(request.url || "/", `http://${plan.host}:${plan.port}`);
    const pathname = requestUrl.pathname;
    if (request.method === "GET" && pathname.startsWith("/client/") && clientBundle) {
      const relative = pathname.slice("/client/".length);
      const normalized = path.normalize(relative).replace(/^([/\\\\])+/, "");
      const filePath = path.resolve(clientBundle.out_dir_abs, normalized);
      if (!filePath.startsWith(path.resolve(clientBundle.out_dir_abs))) {
        sendJson(response, 403, { error: "forbidden" });
        return;
      }
      if (!fs.existsSync(filePath)) {
        sendJson(response, 404, { error: "not_found" });
        return;
      }
      sendFile(response, filePath, contentTypeForPath(filePath));
      return;
    }
    if (request.method === "GET" && pathname === "/") {
      sendHtml(response, bundle.html);
      return;
    }
    if (request.method === "GET" && pathname === "/site.data.json") {
      sendJson(response, 200, bundle.site_data);
      return;
    }
    if (request.method === "GET" && pathname === "/api/system.contract.json") {
      sendJson(response, 200, bundle.system_contract);
      return;
    }
    if (request.method === "GET" && pathname === "/api/ui.schema.json") {
      sendJson(response, 200, bundle.ui_schema);
      return;
    }
    if (request.method === "GET" && pathname === "/sitemap.xml") {
      sendText(response, 200, bundle.sitemap_xml, "application/xml; charset=utf-8");
      return;
    }
    if (request.method === "GET" && pathname === "/robots.txt") {
      sendText(response, 200, bundle.robots_txt, "text/plain; charset=utf-8");
      return;
    }
    if (request.method === "GET" && pathname === "/feed.xml") {
      sendText(response, 200, bundle.feed_xml, "application/rss+xml; charset=utf-8");
      return;
    }
    if (request.method === "GET" && pathname === "/api/runtime") {
      sendJson(response, 200, {
        experience: bundle.id,
        mode: bundle.mode,
        page_title: bundle.page_title,
        output_slug: bundle.output_slug
      });
      return;
    }
    if (request.method === "GET" && pathname === "/api/site") {
      sendJson(response, 200, bundle.site_data);
      return;
    }
    if (request.method === "GET" && pathname === "/api/catalog") {
      sendJson(response, 200, { experiences: plan.catalog, default_experience: bundle.id });
      return;
    }
    if (request.method === "GET" && pathname === "/api/routes") {
      sendJson(response, 200, plan.routes);
      return;
    }
    if (request.method === "GET" && pathname === "/api/scene") {
      sendJson(response, 200, bundle.manifest.scene);
      return;
    }
    if (request.method === "GET" && pathname === "/api/forms") {
      sendJson(response, 200, bundle.site_data.forms || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/app") {
      sendJson(response, 200, bundle.site_data.app_modules || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/auth") {
      sendJson(response, 200, bundle.site_data.auth || {});
      return;
    }
    if (request.method === "GET" && pathname === "/api/auth/session") {
      sendJson(response, 200, { ok: true, session: getSession(bundle, request) });
      return;
    }
    if (request.method === "POST" && pathname === "/api/auth/session/login") {
      const payload = await parseRequestBody(request);
      const email = String(payload.email || "").trim();
      if (!email) {
        sendJson(response, 400, { ok: false, error: "missing_email" });
        return;
      }
      const session = createSession(bundle, {
        email,
        invite_code: payload.invite_code || null,
        roles: Array.isArray(payload.roles) ? payload.roles : []
      });
      response.setHeader("set-cookie", formatSetCookie("kain_session_id", session.id, { sameSite: "Lax", httpOnly: true }));
      sendJson(response, 200, { ok: true, session });
      return;
    }
    if (request.method === "POST" && pathname === "/api/auth/session/logout") {
      response.setHeader("set-cookie", formatSetCookie("kain_session_id", "", { sameSite: "Lax", httpOnly: true, maxAgeSeconds: 0 }));
      sendJson(response, 200, { ok: true });
      return;
    }
    if (request.method === "GET" && pathname === "/api/commerce") {
      sendJson(response, 200, bundle.site_data.commerce || {});
      return;
    }
    if (request.method === "GET" && pathname === "/api/data") {
      sendJson(response, 200, bundle.site_data.data_collections || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/integrations") {
      sendJson(response, 200, bundle.site_data.integrations || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/realtime") {
      sendJson(response, 200, bundle.site_data.realtime_channels || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/actors") {
      sendJson(response, 200, plan);
      return;
    }
    if (request.method === "GET" && pathname === "/api/chat") {
      const prompt = requestUrl.searchParams.get("prompt");
      if (!prompt) {
        sendJson(response, 200, chatSeed);
        return;
      }
      sendJson(response, 200, { reply: buildChatReply(bundle, plan, prompt) });
      return;
    }
    if (request.method === "POST" && pathname === "/api/chat") {
      const payload = await parseRequestBody(request);
      const prompt = payload.prompt || payload.message || payload.text;
      if (!prompt) {
        sendJson(response, 200, { reply: "missing prompt" });
        return;
      }
      sendJson(response, 200, { reply: buildChatReply(bundle, plan, String(prompt)) });
      return;
    }
    if (request.method === "GET" && pathname === "/api/chat/stream") {
      response.writeHead(200, {
        "content-type": "text/event-stream; charset=utf-8",
        "cache-control": "no-cache",
        connection: "keep-alive"
      });
      const prompt = (requestUrl.searchParams.get("prompt") || "").trim();
      response.write(`event: ready\n`);
      response.write(`data: ${JSON.stringify({ experience: bundle.id, actors: plan.actors.length, routes: plan.routes.length })}\n\n`);
      if (!prompt) {
        response.write(`event: seed\n`);
        response.write(`data: ${JSON.stringify(chatSeed)}\n\n`);
        response.write(`event: done\n`);
        response.write(`data: ok\n\n`);
        response.end();
        return;
      }
      const reply = buildChatReply(bundle, plan, prompt);
      const tokens = String(reply).split(/(\s+)/).filter((token) => token.length > 0);
      let index = 0;
      const interval = setInterval(() => {
        if (index >= tokens.length) {
          clearInterval(interval);
          response.write(`event: done\n`);
          response.write(`data: ok\n\n`);
          response.end();
          return;
        }
        const token = tokens[index++];
        response.write(`event: token\n`);
        response.write(`data: ${token.replaceAll("\n", " ")}\n\n`);
      }, 35);
      request.on("close", () => clearInterval(interval));
      return;
    }
    if (request.method === "GET" && pathname === "/api/realtime/stream") {
      response.writeHead(200, {
        "content-type": "text/event-stream; charset=utf-8",
        "cache-control": "no-cache",
        connection: "keep-alive"
      });
      const channels = bundle.site_data.realtime_channels || [];
      response.write(`event: channels\n`);
      response.write(`data: ${JSON.stringify({ channels, tick: 0, at: new Date().toISOString() })}\n\n`);
      let tick = 0;
      const interval = setInterval(() => {
        tick += 1;
        response.write(`event: tick\n`);
        response.write(`data: ${JSON.stringify({ channels, tick, at: new Date().toISOString() })}\n\n`);
      }, 1400);
      request.on("close", () => clearInterval(interval));
      return;
    }
    if (request.method === "GET" && pathname === "/api/search/documents") {
      sendJson(response, 200, searchIndex);
      return;
    }
    if (request.method === "GET" && pathname === "/api/search") {
      const query = (requestUrl.searchParams.get("q") || "").trim().toLowerCase();
      const items = !query
        ? searchIndex.slice(0, 8)
        : searchIndex.filter((entry) => {
            const haystack = [entry.title, entry.summary, ...(entry.tags || [])].join(" ").toLowerCase();
            return haystack.includes(query);
          }).slice(0, 8);
      sendJson(response, 200, { query, items });
      return;
    }
    if (request.method === "POST" && pathname === "/api/analytics/event") {
      const payload = await parseRequestBody(request);
      const eventName = String(payload.name || payload.event || "").trim();
      if (!eventName) {
        sendJson(response, 400, { ok: false, error: "missing_event_name" });
        return;
      }
      const { logPath, entry } = persistAnalyticsEvent(bundle, {
        name: eventName,
        properties: payload.properties || {},
        path: payload.path || null,
        client_at: payload.client_at || null,
        session: getSession(bundle, request)
      });
      sendJson(response, 200, { ok: true, event: entry, log_path: logPath });
      return;
    }
    if (request.method === "GET" && pathname === "/api/analytics/events") {
      const limit = Math.min(Math.max(Number(requestUrl.searchParams.get("limit") || 30), 1), 200);
      const logPath = path.join(runtimeRootForBundle(bundle), "analytics", "events.jsonl");
      const entries = [];
      if (fs.existsSync(logPath)) {
        const lines = fs.readFileSync(logPath, "utf8").trim().split("\n").filter(Boolean);
        for (const line of lines.slice(-limit)) {
          try {
            entries.push(JSON.parse(line));
          } catch {
            entries.push({ received_at: null, raw: line });
          }
        }
      }
      sendJson(response, 200, { ok: true, items: entries });
      return;
    }
    if (request.method === "POST" && pathname === "/api/uploads") {
      const payload = await parseRequestBody(request);
      const maxBytes = 10 * 1024 * 1024;
      const rawBase64 = String(payload.content_base64 || payload.base64 || "");
      const base64 = rawBase64.includes(",") ? rawBase64.split(",").slice(1).join(",") : rawBase64;
      if (!base64.trim()) {
        sendJson(response, 400, { ok: false, error: "missing_content_base64" });
        return;
      }
      const approxBytes = Math.floor((base64.length * 3) / 4);
      if (approxBytes > maxBytes) {
        sendJson(response, 413, { ok: false, error: "payload_too_large", max_bytes: maxBytes });
        return;
      }
      const stored = persistUpload(bundle, payload);
      sendJson(response, 200, { ok: true, file: stored });
      return;
    }
    if (request.method === "GET" && pathname.startsWith("/uploads/")) {
      const relative = pathname.slice("/uploads/".length);
      const normalized = path.normalize(relative).replace(/^([/\\\\])+/, "");
      const uploadsRoot = path.join(runtimeRootForBundle(bundle), "uploads");
      const filePath = path.resolve(uploadsRoot, normalized);
      if (!filePath.startsWith(path.resolve(uploadsRoot))) {
        sendJson(response, 403, { error: "forbidden" });
        return;
      }
      if (!fs.existsSync(filePath)) {
        sendJson(response, 404, { error: "not_found" });
        return;
      }
      sendFile(response, filePath, contentTypeForPath(filePath));
      return;
    }
    if (request.method === "GET" && pathname === "/healthz") {
      sendJson(response, 200, { ok: true, experience: bundle.id, route_count: plan.routes.length });
      return;
    }
    if (request.method === "POST" && formsByPath.has(pathname)) {
      const form = formsByPath.get(pathname);
      const payload = await parseRequestBody(request);
      const logPath = persistSubmission(bundle, form.id, payload);
      sendJson(response, 200, {
        ok: true,
        form_id: form.id,
        message: `${form.title} submission captured`,
        log_path: logPath
      });
      return;
    }
    sendJson(response, 404, { error: "not_found", path: pathname });
  });

  const wsRuntime = (() => {
    try {
      return require("ws");
    } catch {
      return null;
    }
  })();

  if (wsRuntime?.WebSocketServer) {
    const wss = new wsRuntime.WebSocketServer({ noServer: true });
    server.on("upgrade", (request, socket, head) => {
      try {
        const requestUrl = new URL(request.url || "/", "http://localhost");
        if (requestUrl.pathname !== "/ws/realtime" && requestUrl.pathname !== "/ws/chat") {
          socket.destroy();
          return;
        }
        wss.handleUpgrade(request, socket, head, (ws) => {
          wss.emit("connection", ws, requestUrl);
        });
      } catch {
        socket.destroy();
      }
    });

    wss.on("connection", (ws, requestUrl) => {
      if (requestUrl.pathname === "/ws/realtime") {
        const channels = bundle.site_data.realtime_channels || [];
        let tick = 0;
        ws.send(JSON.stringify({ event: "channels", channels, tick, at: new Date().toISOString() }));
        const interval = setInterval(() => {
          tick += 1;
          ws.send(JSON.stringify({ event: "tick", channels, tick, at: new Date().toISOString() }));
        }, 1200);
        ws.on("close", () => clearInterval(interval));
        return;
      }

      if (requestUrl.pathname === "/ws/chat") {
        ws.send(JSON.stringify({ event: "ready", experience: bundle.id }));
        ws.on("message", (raw) => {
          try {
            const payload = JSON.parse(String(raw || "{}"));
            const prompt = String(payload.prompt || payload.message || "");
            const reply = buildChatReply(bundle, plan, prompt);
            ws.send(JSON.stringify({ event: "reply", reply }));
          } catch {
            ws.send(JSON.stringify({ event: "error", error: "invalid_payload" }));
          }
        });
      }
    });
  }
  server.listen(plan.port, plan.host, () => {
    process.stdout.write(`kain-web runtime serving ${bundle.id} at http://${plan.host}:${plan.port}\n`);
  });
}

function runCli() {
  const [command = "print", appManifestPath = "manifests/app.json", experienceId] = process.argv.slice(2);
  if (command === "bundle-client") {
    const context = loadAppConfig(appManifestPath);
    const result = ensureClientBundle(context, { force: true });
    process.stdout.write(JSON.stringify({ ok: true, bundle: result?.href_from_server_root || null }, null, 2) + "\n");
    return;
  }
  if (command === "build") {
    process.stdout.write(JSON.stringify(writeMatrix(appManifestPath), null, 2) + "\n");
    return;
  }
  if (command === "serve") {
    serveExperience(appManifestPath, experienceId);
    return;
  }
  if (command === "catalog") {
    process.stdout.write(JSON.stringify(buildCatalog(appManifestPath), null, 2) + "\n");
    return;
  }
  if (command === "experience") {
    process.stdout.write(JSON.stringify(buildExperience(appManifestPath, experienceId), null, 2) + "\n");
    return;
  }
  if (command === "actor-plan") {
    process.stdout.write(JSON.stringify(buildActorServerPlan(appManifestPath, experienceId), null, 2) + "\n");
    return;
  }
  if (command === "system-contract") {
    process.stdout.write(JSON.stringify(buildSiteSystemContract(appManifestPath, experienceId), null, 2) + "\n");
    return;
  }
  if (command === "ui-schema") {
    process.stdout.write(JSON.stringify(buildExperienceUiSchema(appManifestPath, experienceId), null, 2) + "\n");
    return;
  }
  if (command === "actor-report") {
    process.stdout.write(actorServerReport(appManifestPath, experienceId) + "\n");
    return;
  }
  if (command === "print") {
    process.stdout.write(JSON.stringify(buildMatrix(appManifestPath), null, 2) + "\n");
    return;
  }
  process.stderr.write(`unknown command '${command}'. expected bundle-client, build, serve, catalog, experience, actor-plan, system-contract, ui-schema, actor-report, or print\n`);
  process.exitCode = 1;
}

if (process.argv[1] && import.meta.url === pathToFileURL(path.resolve(process.argv[1])).href) {
  runCli();
}
