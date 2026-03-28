import fs from "node:fs";
import http from "node:http";
import crypto from "node:crypto";
import { createRequire } from "node:module";
import path from "node:path";
import { pathToFileURL } from "node:url";

import matter from "gray-matter";
import MarkdownIt from "markdown-it";
import sanitizeHtml from "sanitize-html";

const require = createRequire(import.meta.url);

function readJson(filePath) {
  return JSON.parse(fs.readFileSync(filePath, "utf8"));
}

function readText(filePath) {
  return fs.readFileSync(filePath, "utf8");
}

function isLikelyAbsoluteUrl(value) {
  const raw = String(value || "").trim();
  return raw.startsWith("http://") || raw.startsWith("https://");
}

let markdownRenderer = null;

function getMarkdownRenderer() {
  if (markdownRenderer) return markdownRenderer;
  markdownRenderer = new MarkdownIt({
    html: false,
    linkify: true,
    typographer: true
  });
  return markdownRenderer;
}

function renderMarkdownToHtml(markdownText) {
  const renderer = getMarkdownRenderer();
  const rawHtml = renderer.render(String(markdownText || ""));
  return sanitizeHtml(rawHtml, {
    allowedTags: [
      "a",
      "b",
      "blockquote",
      "br",
      "code",
      "del",
      "em",
      "h1",
      "h2",
      "h3",
      "h4",
      "h5",
      "h6",
      "hr",
      "i",
      "img",
      "li",
      "ol",
      "p",
      "pre",
      "strong",
      "table",
      "thead",
      "tbody",
      "tr",
      "th",
      "td",
      "ul"
    ],
    allowedAttributes: {
      a: ["href", "title", "target", "rel"],
      img: ["src", "alt", "title", "width", "height"],
      code: ["class"]
    },
    allowedSchemes: ["http", "https", "mailto"],
    transformTags: {
      a: sanitizeHtml.simpleTransform("a", { rel: "noopener noreferrer", target: "_blank" })
    }
  });
}

function loadMarkdownDocument(rootDir, markdownPath) {
  const fullPath = resolveFrom(rootDir, markdownPath);
  const raw = readText(fullPath);
  const parsed = matter(raw);
  const attributes = parsed.data && typeof parsed.data === "object" ? parsed.data : {};
  const content = String(parsed.content || "");
  return {
    path: markdownPath,
    attributes,
    markdown: content,
    html: renderMarkdownToHtml(content)
  };
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
    loader: { ".ts": "ts", ".tsx": "tsx", ".ks": "js" }
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

function isPlainObject(value) {
  if (!value || typeof value !== "object") return false;
  const proto = Object.getPrototypeOf(value);
  return proto === Object.prototype || proto === null;
}

function deepMergeTemplateData(baseValue, overlayValue) {
  if (Array.isArray(baseValue) && Array.isArray(overlayValue)) {
    return [...baseValue, ...overlayValue];
  }
  if (isPlainObject(baseValue) && isPlainObject(overlayValue)) {
    const out = { ...baseValue };
    for (const [key, overlayChild] of Object.entries(overlayValue)) {
      if (key === "includes") continue;
      const baseChild = out[key];
      if (baseChild === undefined) {
        out[key] = overlayChild;
      } else {
        out[key] = deepMergeTemplateData(baseChild, overlayChild);
      }
    }
    return out;
  }
  return overlayValue === undefined ? baseValue : overlayValue;
}

function resolveContentIncludes(contentEntry, contentRegistry, visiting = new Set()) {
  if (!contentEntry || !Array.isArray(contentEntry.includes) || contentEntry.includes.length === 0) {
    return contentEntry;
  }

  if (visiting.has(contentEntry.id)) {
    throw new Error(`content include cycle detected at '${contentEntry.id}'`);
  }

  visiting.add(contentEntry.id);
  let merged = { ...contentEntry, includes: [...contentEntry.includes] };

  for (const includeId of contentEntry.includes) {
    const includeEntry = requireEntry(contentRegistry, includeId, "content include");
    const resolvedInclude = resolveContentIncludes(includeEntry, contentRegistry, visiting);
    merged = deepMergeTemplateData(resolvedInclude, merged);
  }

  visiting.delete(contentEntry.id);
  return merged;
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

function renderMarkdownPanel(value) {
  if (Array.isArray(value)) {
    return `<div class="markdown">${renderMarkdownToHtml(value.join("\n"))}</div>`;
  }
  if (typeof value === "string" && value.trim()) {
    return `<div class="markdown">${renderMarkdownToHtml(value)}</div>`;
  }
  return "";
}

function formatBlogDate(value) {
  const raw = String(value || "").trim();
  if (!raw) return "";
  const parsed = new Date(raw);
  if (!Number.isFinite(parsed.getTime())) return raw;
  return parsed.toISOString().slice(0, 10);
}

function renderBlogRoll(posts) {
  const entries = Array.isArray(posts) ? posts : [];
  if (entries.length === 0) {
    return `<p class="section-copy">No posts are registered yet. Add markdown files and wire them through content manifests.</p>`;
  }

  const cards = entries
    .map((post) => {
      const tags = (post.tags || []).slice(0, 6).map((tag) => `<span class="tag-pill">${escapeHtml(tag)}</span>`).join("");
      const dateLabel = formatBlogDate(post.published_at);
      const kicker = dateLabel ? `Blog · ${dateLabel}` : "Blog";
      const href = post.href || `blog/${escapeHtml(post.slug || "")}/`;
      return `<article class="doc-card blog-card">
  <p class="card-kicker">${escapeHtml(kicker)}</p>
  <h3><a href="${escapeHtml(href)}">${escapeHtml(post.title || post.slug || "Post")}</a></h3>
  <p>${escapeHtml(post.summary || "")}</p>
  ${tags ? `<div class="tag-row">${tags}</div>` : ""}
</article>`;
    })
    .join("");

  return `<div class="docs-grid blog-grid">${cards}</div>`;
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

function renderTeamGrid(members) {
  return `<div class="team-grid">${(members || [])
    .map(
      (member) => `<article class="team-card">
  <p class="card-kicker">${escapeHtml(member.focus || member.role || "Team")}</p>
  <h3>${escapeHtml(member.name || "Team member")}</h3>
  <p>${escapeHtml(member.summary || "")}</p>
  <p class="portfolio-stack">${escapeHtml(member.role || "")}</p>
</article>`
    )
    .join("")}</div>`;
}

function renderPartnerGrid(partners) {
  return `<div class="feature-grid">${(partners || [])
    .map(
      (partner) => `<article class="feature-card partner-card">
  <p class="card-kicker">${escapeHtml(partner.category || "Partner")}</p>
  <h3>${escapeHtml(partner.name || "Partner")}</h3>
  <p>${escapeHtml(partner.detail || partner.summary || "")}</p>
  ${partner.href ? `<a class="inline-link" href="${escapeHtml(partner.href)}">View</a>` : ""}
</article>`
    )
    .join("")}</div>`;
}

function renderSupportGrid(channels) {
  return `<div class="support-grid">${(channels || [])
    .map(
      (channel) => `<article class="support-card">
  <p class="card-kicker">${escapeHtml(channel.availability || "Support")}</p>
  <h3>${escapeHtml(channel.name || "Support channel")}</h3>
  <p>${escapeHtml(channel.detail || "")}</p>
  ${channel.href ? `<a class="inline-link" href="${escapeHtml(channel.href)}">Open</a>` : ""}
</article>`
    )
    .join("")}</div>`;
}

function renderGrowthStack(growth) {
  const campaigns = growth?.campaigns || [];
  const funnels = growth?.funnels || [];
  const campaignCards = campaigns
    .map(
      (campaign) => `<article class="feature-card growth-card">
  <p class="card-kicker">${escapeHtml(campaign.channel || "campaign")}</p>
  <h3>${escapeHtml(campaign.title || "Campaign")}</h3>
  <p>${escapeHtml(campaign.summary || "")}</p>
  <p class="portfolio-stack">${escapeHtml(campaign.status || "")}</p>
</article>`
    )
    .join("");
  const funnelRows = funnels.length
    ? `<div class="timeline-list">${funnels
        .map(
          (funnel) => `<article class="timeline-row funnel-row">
  <p class="timeline-label">${escapeHtml(funnel.stage || "Stage")}</p>
  <div>
    <h3>${escapeHtml(funnel.metric || "")}</h3>
    <p>${escapeHtml(funnel.owner || "")}</p>
  </div>
</article>`
        )
        .join("")}</div>`
    : "";
  return `<div class="growth-grid">${campaignCards}</div>${funnelRows}`;
}

function renderExperimentBoard(experiments) {
  const tests = experiments?.tests || [];
  return `<div class="feature-grid experiment-grid">${tests
    .map(
      (test) => `<article class="feature-card experiment-card">
  <p class="card-kicker">${escapeHtml(test.status || "experiment")}</p>
  <h3>${escapeHtml(test.name || "Experiment")}</h3>
  <p>${escapeHtml(test.hypothesis || "")}</p>
  <p class="portfolio-stack">${escapeHtml([test.metric, test.owner].filter(Boolean).join(" / "))}</p>
</article>`
    )
    .join("")}</div>`;
}

function renderServiceCatalog(serviceCatalog) {
  const services = serviceCatalog?.services || [];
  return `<div class="feature-grid service-grid">${services
    .map(
      (service) => `<article class="feature-card service-card">
  <p class="card-kicker">${escapeHtml(service.tier || "service")}</p>
  <h3>${escapeHtml(service.name || "Service")}</h3>
  <p>${escapeHtml(service.summary || "")}</p>
  <p class="portfolio-stack">${escapeHtml([service.sla, service.owner].filter(Boolean).join(" / "))}</p>
</article>`
    )
    .join("")}</div>`;
}

function renderSuccessPlaybooks(success) {
  const playbooks = success?.playbooks || [];
  return `<div class="timeline-list success-list">${playbooks
    .map(
      (playbook) => `<article class="timeline-row success-row">
  <p class="timeline-label">${escapeHtml(playbook.cadence || "Cadence")}</p>
  <div>
    <h3>${escapeHtml(playbook.title || "Playbook")}</h3>
    <p>${escapeHtml(playbook.goal || "")}</p>
    <p class="portfolio-stack">${escapeHtml(playbook.owner || "")}</p>
  </div>
</article>`
    )
    .join("")}</div>`;
}

function renderNotificationMatrix(notifications) {
  const channels = notifications?.channels || [];
  return `<div class="feature-grid notification-grid">${channels
    .map(
      (channel) => `<article class="feature-card notification-card">
  <p class="card-kicker">${escapeHtml(channel.transport || "notification")}</p>
  <h3>${escapeHtml(channel.name || "Channel")}</h3>
  <p>${escapeHtml(channel.purpose || "")}</p>
  <p class="portfolio-stack">${escapeHtml([channel.cadence, channel.owner].filter(Boolean).join(" / "))}</p>
</article>`
    )
    .join("")}</div>`;
}

function renderReleaseNotes(releaseNotes) {
  const entries = releaseNotes?.entries || [];
  const rows = entries.length
    ? `<div class="timeline-list release-list">${entries
        .map(
          (entry) => `<article class="timeline-row release-row">
  <p class="timeline-label">${escapeHtml([entry.version, entry.date].filter(Boolean).join(" / "))}</p>
  <div>
    <h3>${escapeHtml(entry.summary || "Release update")}</h3>
    <p>${escapeHtml((entry.highlights || []).join(" · "))}</p>
    <p class="portfolio-stack">${escapeHtml(entry.owner || "")}</p>
  </div>
</article>`
        )
        .join("")}</div>`
    : `<p class="section-copy">No release notes registered yet.</p>`;
  return `<section class="release-notes">
  <article class="hero-card">
    <p class="section-label">${escapeHtml(releaseNotes?.kicker || "Releases")}</p>
    <h3>${escapeHtml(releaseNotes?.title || "Release notes")}</h3>
    <p class="section-copy">${escapeHtml(releaseNotes?.body || "")}</p>
  </article>
  ${rows}
</section>`;
}

function renderFeatureFlags(featureFlags) {
  const flags = featureFlags?.flags || [];
  return `<section class="feature-flags">
  <article class="hero-card">
    <p class="section-label">${escapeHtml(featureFlags?.kicker || "Flags")}</p>
    <h3>${escapeHtml(featureFlags?.title || "Feature flags")}</h3>
    <p class="section-copy">${escapeHtml(featureFlags?.body || "")}</p>
  </article>
  <div class="feature-grid flag-grid">${flags
    .map(
      (flag) => `<article class="feature-card flag-card">
  <p class="card-kicker">${escapeHtml(flag.status || "flag")}</p>
  <h3>${escapeHtml(flag.name || "flag")}</h3>
  <p>${escapeHtml(flag.impact || flag.summary || "")}</p>
  <p class="portfolio-stack">${escapeHtml(flag.owner || "")}</p>
</article>`
    )
    .join("")}</div>
</section>`;
}

function renderIncidentResponse(incidentResponse) {
  const playbooks = incidentResponse?.playbooks || [];
  const rows = playbooks.length
    ? `<div class="timeline-list incident-list">${playbooks
        .map(
          (entry) => `<article class="timeline-row incident-row">
  <p class="timeline-label">${escapeHtml([entry.severity, entry.sla].filter(Boolean).join(" / "))}</p>
  <div>
    <h3>${escapeHtml(entry.title || "Incident playbook")}</h3>
    <p>${escapeHtml(entry.summary || entry.body || "")}</p>
    <p class="portfolio-stack">${escapeHtml(entry.owner || "")}</p>
  </div>
</article>`
        )
        .join("")}</div>`
    : `<p class="section-copy">No incident playbooks registered.</p>`;
  return `<section class="incident-response">
  <article class="hero-card">
    <p class="section-label">${escapeHtml(incidentResponse?.kicker || "Incident")}</p>
    <h3>${escapeHtml(incidentResponse?.title || "Incident response")}</h3>
    <p class="section-copy">${escapeHtml(incidentResponse?.body || "")}</p>
  </article>
  ${rows}
</section>`;
}

function renderCrmPipeline(crmPipeline) {
  const stages = crmPipeline?.stages || [];
  const rows = stages.length
    ? `<div class="timeline-list crm-list">${stages
        .map(
          (stage) => `<article class="timeline-row crm-row">
  <p class="timeline-label">${escapeHtml([stage.stage, stage.sla].filter(Boolean).join(" / "))}</p>
  <div>
    <h3>${escapeHtml(stage.goal || stage.title || "Stage")}</h3>
    <p>${escapeHtml(stage.summary || stage.detail || "")}</p>
    <p class="portfolio-stack">${escapeHtml(stage.owner || "")}</p>
  </div>
</article>`
        )
        .join("")}</div>`
    : `<p class="section-copy">No CRM stages registered yet.</p>`;
  return `<section class="crm-pipeline">
  <article class="hero-card">
    <p class="section-label">${escapeHtml(crmPipeline?.kicker || "CRM")}</p>
    <h3>${escapeHtml(crmPipeline?.title || "CRM pipeline")}</h3>
    <p class="section-copy">${escapeHtml(crmPipeline?.body || "")}</p>
  </article>
  ${rows}
</section>`;
}

function renderActorTopology(topology) {
  const nodes = topology?.nodes || [];
  const edges = topology?.edges || [];
  const nodeCards = nodes
    .map(
      (node) => `<article class="feature-card topology-card">
  <p class="card-kicker">${escapeHtml(node.role || "node")}</p>
  <h3>${escapeHtml(node.name || node.id || "Actor")}</h3>
  <p>${escapeHtml(node.channel || "")}</p>
</article>`
    )
    .join("");
  const edgeRows = edges.length
    ? `<div class="timeline-list topology-links">${edges
        .map(
          (edge) => `<article class="timeline-row topology-edge">
  <p class="timeline-label">${escapeHtml(edge.relation || "link")}</p>
  <div>
    <h3>${escapeHtml(edge.from || "")} → ${escapeHtml(edge.to || "")}</h3>
    <p>${escapeHtml(edge.detail || "")}</p>
  </div>
</article>`
        )
        .join("")}</div>`
    : "";
  return `<div class="feature-grid topology-grid">${nodeCards}</div>${edgeRows}`;
}

function renderCareersList(careers) {
  const roles = careers?.roles || [];
  return `<section class="careers-shell">
  <article class="hero-card">
    <p class="section-label">${escapeHtml(careers?.kicker || "Careers")}</p>
    <h3>${escapeHtml(careers?.title || "Open roles")}</h3>
    <p class="section-copy">${escapeHtml(careers?.body || "")}</p>
  </article>
  <div class="career-grid">${roles
    .map(
      (role) => `<article class="career-card">
  <p class="card-kicker">${escapeHtml(role.type || "Role")}</p>
  <h3>${escapeHtml(role.title || "Role")}</h3>
  <p>${escapeHtml(role.summary || "")}</p>
  <p class="portfolio-stack">${escapeHtml([role.location, ...(role.tags || [])].filter(Boolean).join(" / "))}</p>
  ${role.href ? `<a class="inline-link" href="${escapeHtml(role.href)}">Open role</a>` : ""}
</article>`
    )
    .join("")}</div>
</section>`;
}

function renderStatusBoard(status) {
  const services = status?.services || [];
  const incidents = status?.incidents || [];
  const serviceCards = services
    .map(
      (service) => `<article class="status-card" data-status="${escapeHtml(service.status || "")}">
  <p class="card-kicker">${escapeHtml(service.status || "status")}</p>
  <h3>${escapeHtml(service.name || "service")}</h3>
  <p>${escapeHtml(service.detail || "")}</p>
  <p class="portfolio-stack">${escapeHtml(service.uptime || "")}</p>
</article>`
    )
    .join("");
  const incidentList = incidents.length
    ? `<div class="timeline-list">${incidents
        .map(
          (incident) => `<article class="timeline-row status-incident">
  <p class="timeline-label">${escapeHtml(incident.phase || incident.label || "Incident")}</p>
  <div>
    <h3>${escapeHtml(incident.title || "")}</h3>
    <p>${escapeHtml(incident.body || incident.summary || "")}</p>
  </div>
</article>`
        )
        .join("")}</div>`
    : `<p class="section-copy">No incidents recorded.</p>`;
  return `<section class="status-board">
  <article class="hero-card">
    <p class="section-label">${escapeHtml(status?.kicker || "Status")}</p>
    <h3>${escapeHtml(status?.title || "Runtime status")}</h3>
    <p class="section-copy">${escapeHtml(status?.summary || "")}</p>
  </article>
  <div class="status-grid">${serviceCards}</div>
  <div class="status-incidents">
    <p class="section-label">Incidents</p>
    ${incidentList}
  </div>
  <div data-kain-island="status" data-site-data="site.data.json"></div>
</section>`;
}

function renderPressKit(press) {
  const assets = (press?.assets || [])
    .map(
      (asset) => `<article class="feature-card press-card">
  <p class="card-kicker">${escapeHtml(asset.detail || "Asset")}</p>
  <h3>${escapeHtml(asset.label || "Press asset")}</h3>
  ${asset.href ? `<a class="inline-link" href="${escapeHtml(asset.href)}">Download</a>` : ""}
</article>`
    )
    .join("");
  const contacts = (press?.contacts || [])
    .map(
      (contact) => `<article class="feature-card press-card">
  <p class="card-kicker">${escapeHtml(contact.role || "Contact")}</p>
  <h3>${escapeHtml(contact.name || "Press")}</h3>
  <p>${escapeHtml(contact.email || "")}</p>
</article>`
    )
    .join("");
  return `<section class="press-kit">
  <article class="hero-card">
    <p class="section-label">${escapeHtml(press?.kicker || "Press")}</p>
    <h3>${escapeHtml(press?.title || "Press kit")}</h3>
    <p class="section-copy">${escapeHtml(press?.body || "")}</p>
  </article>
  <div class="feature-grid">${assets}</div>
  ${contacts ? `<div class="feature-grid">${contacts}</div>` : ""}
</section>`;
}

function renderSecurityGrid(security) {
  return `<section class="security-shell">
  <article class="hero-card">
    <p class="section-label">${escapeHtml(security?.kicker || "Security")}</p>
    <h3>${escapeHtml(security?.title || "Security controls")}</h3>
    <p class="section-copy">${escapeHtml(security?.body || "")}</p>
  </article>
  <div class="feature-grid">${(security?.controls || [])
    .map(
      (control) => `<article class="feature-card security-card">
  <p class="card-kicker">${escapeHtml(control.status || "control")}</p>
  <h3>${escapeHtml(control.title || "Control")}</h3>
  <p>${escapeHtml(control.detail || "")}</p>
</article>`
    )
    .join("")}</div>
</section>`;
}

function renderCommunityHub(community) {
  const channels = community?.channels || [];
  return `<section class="community-hub">
  <article class="hero-card">
    <p class="section-label">${escapeHtml(community?.kicker || "Community")}</p>
    <h3>${escapeHtml(community?.title || "Community lanes")}</h3>
    <p class="section-copy">${escapeHtml(community?.body || "")}</p>
  </article>
  <div class="feature-grid">${channels
    .map(
      (channel) => `<article class="feature-card community-card">
  <p class="card-kicker">${escapeHtml(channel.platform || "Channel")}</p>
  <h3>${escapeHtml(channel.name || "Community")}</h3>
  <p>${escapeHtml(channel.summary || "")}</p>
  <p class="portfolio-stack">${escapeHtml([channel.members, channel.cadence].filter(Boolean).join(" / "))}</p>
  ${channel.href ? `<a class="inline-link" href="${escapeHtml(channel.href)}">Open</a>` : ""}
</article>`
    )
    .join("")}</div>
</section>`;
}

function renderEventSchedule(events, model) {
  const upcoming = events?.upcoming || [];
  const rows = upcoming.length
    ? `<div class="timeline-list">${upcoming
        .map(
          (entry) => `<article class="timeline-row event-row">
  <p class="timeline-label">${escapeHtml([entry.date, entry.format].filter(Boolean).join(" / "))}</p>
  <div>
    <h3>${escapeHtml(entry.title || "Event")}</h3>
    <p>${escapeHtml(entry.summary || "")}</p>
  </div>
</article>`
        )
        .join("")}</div>`
    : `<p class="section-copy">No events scheduled yet.</p>`;
  const rsvpFormId = events?.rsvp_form_id;
  const rsvpForm = rsvpFormId ? model?.content?.forms?.[rsvpFormId] : null;
  const rsvpHtml = rsvpForm ? renderFormPanel(rsvpForm) : "";
  return `<section class="event-schedule">
  <article class="hero-card">
    <p class="section-label">${escapeHtml(events?.kicker || "Events")}</p>
    <h3>${escapeHtml(events?.title || "Upcoming events")}</h3>
    <p class="section-copy">${escapeHtml(events?.body || "")}</p>
  </article>
  ${rows}
  ${rsvpHtml}
</section>`;
}

function renderNewsletterPanel(newsletter, model) {
  const topics = (newsletter?.topics || [])
    .map((topic) => `<span class="tag-pill">${escapeHtml(topic)}</span>`)
    .join("");
  const formId = newsletter?.form_id;
  const form = formId ? model?.content?.forms?.[formId] : null;
  const formHtml = form ? renderFormPanel(form) : "";
  return `<section class="newsletter-panel">
  <article class="hero-card">
    <p class="section-label">${escapeHtml(newsletter?.kicker || "Newsletter")}</p>
    <h3>${escapeHtml(newsletter?.title || "Newsletter")}</h3>
    <p class="section-copy">${escapeHtml(newsletter?.body || "")}</p>
    <p class="portfolio-stack">${escapeHtml(newsletter?.cadence || "")}</p>
    ${topics ? `<div class="tag-row">${topics}</div>` : ""}
  </article>
  ${formHtml}
</section>`;
}

function renderComplianceGrid(compliance) {
  const controls = compliance?.controls || [];
  return `<section class="compliance-grid">
  <article class="hero-card">
    <p class="section-label">${escapeHtml(compliance?.kicker || "Compliance")}</p>
    <h3>${escapeHtml(compliance?.title || "Compliance posture")}</h3>
    <p class="section-copy">${escapeHtml(compliance?.body || "")}</p>
  </article>
  <div class="feature-grid">${controls
    .map(
      (control) => `<article class="feature-card compliance-card">
  <p class="card-kicker">${escapeHtml(control.status || "control")}</p>
  <h3>${escapeHtml(control.title || "Control")}</h3>
  <p>${escapeHtml(control.detail || "")}</p>
</article>`
    )
    .join("")}</div>
</section>`;
}

function renderObservabilityStack(observability) {
  const signals = observability?.signals || [];
  return `<section class="observability-stack">
  <article class="hero-card">
    <p class="section-label">${escapeHtml(observability?.kicker || "Observability")}</p>
    <h3>${escapeHtml(observability?.title || "Operational signals")}</h3>
    <p class="section-copy">${escapeHtml(observability?.body || "")}</p>
  </article>
  <div class="feature-grid">${signals
    .map(
      (signal) => `<article class="feature-card observability-card">
  <p class="card-kicker">${escapeHtml(signal.owner || "signal")}</p>
  <h3>${escapeHtml(signal.title || "Signal")}</h3>
  <p>${escapeHtml(signal.detail || "")}</p>
  <p class="portfolio-stack">${escapeHtml(signal.cadence || "")}</p>
</article>`
    )
    .join("")}</div>
</section>`;
}

function renderInfrastructureStack(infrastructure) {
  const stack = infrastructure?.stack || [];
  return `<section class="infrastructure-stack">
  <article class="hero-card">
    <p class="section-label">${escapeHtml(infrastructure?.kicker || "Infrastructure")}</p>
    <h3>${escapeHtml(infrastructure?.title || "Infrastructure stack")}</h3>
    <p class="section-copy">${escapeHtml(infrastructure?.body || "")}</p>
  </article>
  <div class="feature-grid">${stack
    .map(
      (item) => `<article class="feature-card infrastructure-card">
  <p class="card-kicker">${escapeHtml(item.tier || "stack")}</p>
  <h3>${escapeHtml(item.title || "Component")}</h3>
  <p>${escapeHtml(item.detail || "")}</p>
  <p class="portfolio-stack">${escapeHtml(item.status || "")}</p>
</article>`
    )
    .join("")}</div>
</section>`;
}

function renderLocalizationGrid(localization) {
  const languages = localization?.languages || [];
  const regions = localization?.regions || [];
  const languageCards = languages
    .map(
      (entry) => `<article class="feature-card localization-card">
  <p class="card-kicker">${escapeHtml(entry.status || "language")}</p>
  <h3>${escapeHtml(entry.name || "Language")}</h3>
  <p>${escapeHtml(entry.coverage || "")}</p>
</article>`
    )
    .join("");
  const regionCards = regions
    .map(
      (entry) => `<article class="feature-card localization-card">
  <p class="card-kicker">${escapeHtml(entry.status || "region")}</p>
  <h3>${escapeHtml(entry.name || "Region")}</h3>
  <p>${escapeHtml(entry.timezone || "")}</p>
</article>`
    )
    .join("");
  return `<section class="localization-grid">
  <article class="hero-card">
    <p class="section-label">${escapeHtml(localization?.kicker || "Localization")}</p>
    <h3>${escapeHtml(localization?.title || "Localization")}</h3>
    <p class="section-copy">${escapeHtml(localization?.body || "")}</p>
  </article>
  <div class="feature-grid">${languageCards}${regionCards}</div>
</section>`;
}

function renderAccessibilityGrid(accessibility) {
  const checks = accessibility?.checks || [];
  return `<section class="accessibility-grid">
  <article class="hero-card">
    <p class="section-label">${escapeHtml(accessibility?.kicker || "Accessibility")}</p>
    <h3>${escapeHtml(accessibility?.title || "Accessibility")}</h3>
    <p class="section-copy">${escapeHtml(accessibility?.body || "")}</p>
  </article>
  <div class="feature-grid">${checks
    .map(
      (entry) => `<article class="feature-card accessibility-card">
  <p class="card-kicker">${escapeHtml(entry.status || "check")}</p>
  <h3>${escapeHtml(entry.title || "Check")}</h3>
  <p>${escapeHtml(entry.detail || "")}</p>
</article>`
    )
    .join("")}</div>
</section>`;
}

function renderPerformanceTargets(performance) {
  const targets = performance?.targets || [];
  return `<section class="performance-targets">
  <article class="hero-card">
    <p class="section-label">${escapeHtml(performance?.kicker || "Performance")}</p>
    <h3>${escapeHtml(performance?.title || "Performance targets")}</h3>
    <p class="section-copy">${escapeHtml(performance?.body || "")}</p>
  </article>
  <div class="feature-grid">${targets
    .map(
      (entry) => `<article class="feature-card performance-card">
  <p class="card-kicker">${escapeHtml(entry.target || "")}</p>
  <h3>${escapeHtml(entry.title || "Target")}</h3>
  <p>${escapeHtml(entry.detail || "")}</p>
</article>`
    )
    .join("")}</div>
</section>`;
}

function renderRoadmapTimeline(items) {
  return `<div class="timeline-list">${(items || [])
    .map(
      (item) => `<article class="timeline-row roadmap-row">
  <p class="timeline-label">${escapeHtml([item.phase, item.eta].filter(Boolean).join(" / "))}</p>
  <div>
    <h3>${escapeHtml(item.title || "")}</h3>
    <p>${escapeHtml(item.body || item.summary || "")}</p>
  </div>
</article>`
    )
    .join("")}</div>`;
}

function renderLegalLinks(entries) {
  return `<div class="legal-grid">${(entries || [])
    .map(
      (entry) => `<article class="doc-card legal-card">
  <p class="card-kicker">${escapeHtml(entry.kicker || "Policy")}</p>
  <h3>${escapeHtml(entry.title || "Policy")}</h3>
  <p>${escapeHtml(entry.summary || "")}</p>
  <a class="inline-link" href="${escapeHtml(entry.href || "#")}">${escapeHtml(entry.label || "Read")}</a>
</article>`
    )
    .join("")}</div>`;
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

function renderUiKit(kit) {
  const summary = kit || {};
  const componentCount = (summary.components || []).length;
  const layoutCount = (summary.layouts || []).length;
  const tokenCount = (summary.tokens || []).length;
  return `<section class="ui-kit-shell">
  <div class="logo-pill"><span>UI Kit</span><span>components + layouts + tokens</span></div>
  <div class="ui-kit-summary">
    <article class="metric-card">
      <p class="metric-value">${escapeHtml(String(componentCount))}</p>
      <p class="metric-label">components</p>
    </article>
    <article class="metric-card">
      <p class="metric-value">${escapeHtml(String(layoutCount))}</p>
      <p class="metric-label">layouts</p>
    </article>
    <article class="metric-card">
      <p class="metric-value">${escapeHtml(String(tokenCount))}</p>
      <p class="metric-label">tokens</p>
    </article>
  </div>
  <div data-kain-island="ui-kit" data-site-data="site.data.json"></div>
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
  } else if (kind === "team_grid") {
    bodyHtml = renderTeamGrid(getModelValue(model, normalized.source, []));
  } else if (kind === "partner_grid") {
    bodyHtml = renderPartnerGrid(getModelValue(model, normalized.source, []));
  } else if (kind === "support_grid") {
    bodyHtml = renderSupportGrid(getModelValue(model, normalized.source, []));
  } else if (kind === "portfolio_grid") {
    bodyHtml = renderPortfolio(getModelValue(model, normalized.source, []));
  } else if (kind === "timeline") {
    bodyHtml = `<div class="timeline-list">${renderTimeline(getModelValue(model, normalized.source, []))}</div>`;
  } else if (kind === "roadmap_timeline") {
    bodyHtml = renderRoadmapTimeline(getModelValue(model, normalized.source, []));
  } else if (kind === "status_board") {
    bodyHtml = renderStatusBoard(getModelValue(model, normalized.source, {}));
  } else if (kind === "scene_spotlight") {
    bodyHtml = renderScene(getModelValue(model, normalized.source || "scene", model.scene));
  } else if (kind === "chat_lab") {
    bodyHtml = renderChat(getModelValue(model, normalized.source, []), normalized);
  } else if (kind === "actor_mesh") {
    bodyHtml = `<div class="feature-grid">${renderActors(getModelValue(model, normalized.source, []))}</div>`;
  } else if (kind === "actor_topology") {
    bodyHtml = renderActorTopology(getModelValue(model, normalized.source, {}));
  } else if (kind === "route_grid") {
    bodyHtml = `<div class="feature-grid">${renderRoutes(getModelValue(model, normalized.source, []))}</div>`;
  } else if (kind === "growth_stack") {
    bodyHtml = renderGrowthStack(getModelValue(model, normalized.source, {}));
  } else if (kind === "experiment_board") {
    bodyHtml = renderExperimentBoard(getModelValue(model, normalized.source, {}));
  } else if (kind === "service_catalog") {
    bodyHtml = renderServiceCatalog(getModelValue(model, normalized.source, {}));
  } else if (kind === "success_playbooks") {
    bodyHtml = renderSuccessPlaybooks(getModelValue(model, normalized.source, {}));
  } else if (kind === "notification_matrix") {
    bodyHtml = renderNotificationMatrix(getModelValue(model, normalized.source, {}));
  } else if (kind === "release_notes") {
    bodyHtml = renderReleaseNotes(getModelValue(model, normalized.source, {}));
  } else if (kind === "feature_flags") {
    bodyHtml = renderFeatureFlags(getModelValue(model, normalized.source, {}));
  } else if (kind === "incident_response") {
    bodyHtml = renderIncidentResponse(getModelValue(model, normalized.source, {}));
  } else if (kind === "crm_pipeline") {
    bodyHtml = renderCrmPipeline(getModelValue(model, normalized.source, {}));
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
  } else if (kind === "blog_roll") {
    bodyHtml = renderBlogRoll(buildBlogPosts(model));
  } else if (kind === "press_kit") {
    bodyHtml = renderPressKit(getModelValue(model, normalized.source, {}));
  } else if (kind === "security_grid") {
    bodyHtml = renderSecurityGrid(getModelValue(model, normalized.source, {}));
  } else if (kind === "community_hub") {
    bodyHtml = renderCommunityHub(getModelValue(model, normalized.source, {}));
  } else if (kind === "event_schedule") {
    bodyHtml = renderEventSchedule(getModelValue(model, normalized.source, {}), model);
  } else if (kind === "newsletter_panel") {
    bodyHtml = renderNewsletterPanel(getModelValue(model, normalized.source, {}), model);
  } else if (kind === "compliance_grid") {
    bodyHtml = renderComplianceGrid(getModelValue(model, normalized.source, {}));
  } else if (kind === "observability_stack") {
    bodyHtml = renderObservabilityStack(getModelValue(model, normalized.source, {}));
  } else if (kind === "infrastructure_stack") {
    bodyHtml = renderInfrastructureStack(getModelValue(model, normalized.source, {}));
  } else if (kind === "localization_grid") {
    bodyHtml = renderLocalizationGrid(getModelValue(model, normalized.source, {}));
  } else if (kind === "accessibility_grid") {
    bodyHtml = renderAccessibilityGrid(getModelValue(model, normalized.source, {}));
  } else if (kind === "performance_targets") {
    bodyHtml = renderPerformanceTargets(getModelValue(model, normalized.source, {}));
  } else if (kind === "legal_links") {
    bodyHtml = renderLegalLinks(getModelValue(model, normalized.source, []));
  } else if (kind === "careers_list") {
    bodyHtml = renderCareersList(getModelValue(model, normalized.source, {}));
  } else if (kind === "form_panel") {
    bodyHtml = renderFormPanel(getModelValue(model, normalized.source, {}));
  } else if (kind === "search_panel") {
    bodyHtml = renderSearchPanel(getModelValue(model, normalized.source, {}));
  } else if (kind === "markdown_panel") {
    bodyHtml = renderMarkdownPanel(getModelValue(model, normalized.source, normalized.body || ""));
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
  } else if (kind === "ui_kit") {
    bodyHtml = renderUiKit({
      components: getModelValue(model, normalized.components_source || "content.ui_components", []),
      layouts: getModelValue(model, normalized.layouts_source || "content.ui_layouts", []),
      tokens: getModelValue(model, normalized.tokens_source || "content.ui_tokens", [])
    });
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
  for (const node of model.content.actor_topology?.nodes || []) {
    pushDocument("actor-node", node.name || node.id, node.channel || node.role, "#topology");
  }
  for (const campaign of model.content.growth?.campaigns || []) {
    pushDocument("growth", campaign.title, campaign.summary, "#growth");
  }
  for (const test of model.content.experiments?.tests || []) {
    pushDocument("experiment", test.name, test.hypothesis, "#experiments");
  }
  for (const service of model.content.service_catalog?.services || []) {
    pushDocument("service", service.name, service.summary, "#services");
  }
  for (const playbook of model.content.success?.playbooks || []) {
    pushDocument("success", playbook.title, playbook.goal, "#success");
  }
  for (const channel of model.content.notifications?.channels || []) {
    pushDocument("notification", channel.name, channel.purpose, "#notifications");
  }
  for (const entry of model.content.support_tickets || []) {
    pushDocument("support-ticket", entry.title || entry.name, entry.body || entry.summary || "", "#support-tickets");
  }
  for (const entry of model.content.feedback_loops || []) {
    pushDocument("feedback", entry.title || entry.name, entry.body || entry.summary || "", "#feedback");
  }
  for (const entry of model.content.survey_programs || []) {
    pushDocument("survey", entry.title || entry.name, entry.body || entry.summary || "", "#surveys");
  }
  for (const entry of model.content.messaging_stack || []) {
    pushDocument("messaging", entry.title || entry.name, entry.body || entry.summary || "", "#messaging");
  }
  for (const entry of model.content.payments_stack || []) {
    pushDocument("payments", entry.title || entry.name, entry.body || entry.summary || "", "#payments");
  }
  for (const entry of model.content.scheduling_stack || []) {
    pushDocument("scheduling", entry.title || entry.name, entry.body || entry.summary || "", "#scheduling");
  }
  for (const entry of model.content.privacy_requests || []) {
    pushDocument("privacy", entry.title || entry.name, entry.body || entry.summary || "", "#privacy");
  }
  for (const entry of model.content.release_notes?.entries || []) {
    pushDocument("release", entry.version, entry.summary, "#releases");
  }
  for (const flag of model.content.feature_flags?.flags || []) {
    pushDocument("flag", flag.name, flag.impact, "#flags");
  }
  for (const playbook of model.content.incident_response?.playbooks || []) {
    pushDocument("incident", playbook.title, playbook.summary, "#incidents");
  }
  for (const stage of model.content.crm_pipeline?.stages || []) {
    pushDocument("crm", stage.stage, stage.goal, "#crm");
  }
  for (const persona of model.content.chat_personas || []) {
    pushDocument("persona", persona.title || persona.name, persona.body || persona.summary || "", "#personas");
  }
  for (const mode of model.content.chat_modes || []) {
    pushDocument("chat-mode", mode.title || mode.name, mode.body || mode.summary || "", "#chat-modes");
  }
  for (const playbook of model.content.chat_playbooks || []) {
    pushDocument("chat-playbook", playbook.title, playbook.body, "#chat-workflows");
  }
  for (const tool of model.content.chat_tools || []) {
    pushDocument("chat-tool", tool.title, tool.body || tool.summary || "", "#chat-tools");
  }
  for (const memory of model.content.chat_memory || []) {
    pushDocument("chat-memory", memory.title, memory.body || memory.summary || "", "#chat-memory");
  }
  for (const playbook of model.content.actor_playbooks || []) {
    pushDocument("playbook", playbook.title || playbook.name, playbook.body || playbook.summary || "", "#playbooks");
  }
  for (const policy of model.content.actor_policies || []) {
    pushDocument("actor-policy", policy.title, policy.body || policy.summary || "", "#actor-policies");
  }
  for (const metric of model.content.actor_metrics || []) {
    pushDocument("actor-metric", metric.label, metric.value, "#actor-metrics");
  }
  for (const entry of model.content.frontend_stack || []) {
    pushDocument("frontend", entry.title || entry.kicker, entry.body || entry.summary || "", "#frontend-stack");
  }
  for (const entry of model.content.ui_runtime || []) {
    pushDocument("ui-runtime", entry.title || entry.kicker, entry.body || entry.summary || "", "#ui-runtime");
  }
  for (const entry of model.content.chat_runtime || []) {
    pushDocument("chat-runtime", entry.title || entry.kicker, entry.body || entry.summary || "", "#chat-runtime");
  }
  for (const entry of model.content.actor_runtime || []) {
    pushDocument("actor-runtime", entry.title || entry.kicker, entry.body || entry.summary || "", "#actor-runtime");
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
  for (const entry of model.content.edge_runtime || []) {
    pushDocument("edge-runtime", entry.title || entry.kicker, entry.body || entry.summary || "", "#edge-runtime");
  }
  for (const entry of model.content.worker_runtime || []) {
    pushDocument("worker-runtime", entry.title || entry.kicker, entry.body || entry.summary || "", "#worker-runtime");
  }
  for (const entry of model.content.api_gateway || []) {
    pushDocument("api-gateway", entry.title || entry.kicker, entry.body || entry.summary || "", "#api-gateway");
  }
  for (const entry of model.content.rate_limits || []) {
    pushDocument("rate-limits", entry.title || entry.kicker, entry.body || entry.summary || "", "#rate-limits");
  }
  for (const entry of model.content.cache_stack || []) {
    pushDocument("cache-stack", entry.title || entry.kicker, entry.body || entry.summary || "", "#cache-stack");
  }
  for (const entry of model.content.search_stack || []) {
    pushDocument("search-stack", entry.title || entry.kicker, entry.body || entry.summary || "", "#search-stack");
  }
  for (const entry of model.content.storage_stack || []) {
    pushDocument("storage-stack", entry.title || entry.kicker, entry.body || entry.summary || "", "#storage-stack");
  }
  for (const entry of model.content.session_store || []) {
    pushDocument("session-store", entry.title || entry.kicker, entry.body || entry.summary || "", "#session-store");
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
  for (const asset of model.content.scene_assets || []) {
    pushDocument("scene-asset", asset.title || asset.name, asset.body || asset.summary || "", "#scene-assets");
  }
  for (const material of model.content.material_library || []) {
    pushDocument("material", material.title || material.name, material.body || material.summary || "", "#materials");
  }
  for (const rig of model.content.lighting_rigs || []) {
    pushDocument("lighting", rig.title || rig.name, rig.body || rig.summary || "", "#lighting");
  }
  for (const rig of model.content.camera_rigs || []) {
    pushDocument("camera", rig.title || rig.name, rig.body || rig.summary || "", "#cameras");
  }
  for (const entry of model.content.animation_stack || []) {
    pushDocument("animation", entry.title || entry.name, entry.body || entry.summary || "", "#animation");
  }
  for (const entry of model.content.physics_stack || []) {
    pushDocument("physics", entry.title || entry.name, entry.body || entry.summary || "", "#physics");
  }
  for (const entry of model.content.spatial_audio || []) {
    pushDocument("audio", entry.title || entry.name, entry.body || entry.summary || "", "#audio");
  }
  for (const entry of model.content.xr_modes || []) {
    pushDocument("xr", entry.title || entry.name, entry.body || entry.summary || "", "#xr");
  }
  for (const entry of model.content.shader_stack || []) {
    pushDocument("shader", entry.title || entry.name, entry.body || entry.summary || "", "#shaders");
  }
  for (const entry of model.content.streaming_stack || []) {
    pushDocument("streaming", entry.title || entry.name, entry.body || entry.summary || "", "#streaming");
  }
  for (const entry of model.content.knowledge_sources || []) {
    pushDocument("knowledge", entry.title || entry.name, entry.body || entry.summary || "", "#knowledge");
  }
  for (const entry of model.content.memory_stores || []) {
    pushDocument("memory", entry.title || entry.name, entry.body || entry.summary || "", "#memory");
  }
  for (const entry of model.content.tool_registry || []) {
    pushDocument("tool", entry.title || entry.name, entry.body || entry.summary || "", "#tool-registry");
  }
  for (const entry of model.content.agent_workflows || []) {
    pushDocument("agent-flow", entry.title || entry.name, entry.body || entry.summary || "", "#agent-workflows");
  }
  for (const entry of model.content.actor_jobs || []) {
    pushDocument("actor-job", entry.title || entry.name, entry.body || entry.summary || "", "#actor-jobs");
  }
  for (const entry of model.content.actor_schedules || []) {
    pushDocument("actor-schedule", entry.title || entry.name, entry.body || entry.summary || "", "#actor-schedules");
  }
  for (const entry of model.content.actor_hosts || []) {
    pushDocument("actor-host", entry.title || entry.name, entry.body || entry.summary || "", "#actor-hosts");
  }
  for (const entry of model.content.runtime_hosts || []) {
    pushDocument("runtime-host", entry.title || entry.name, entry.body || entry.summary || "", "#runtime-hosts");
  }
  for (const entry of model.content.deployment_targets || []) {
    pushDocument("deploy", entry.title || entry.name, entry.body || entry.summary || "", "#deploy");
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
  for (const provider of model.content.identity?.providers || []) {
    documents.push({
      kind: "identity",
      title: provider.title,
      summary: provider.body || "",
      href: "#identity",
      tags: [provider.kicker].filter(Boolean)
    });
  }
  for (const role of model.content.identity?.roles || []) {
    documents.push({
      kind: "role",
      title: role.title,
      summary: role.body || "",
      href: "#identity-roles",
      tags: [role.kicker].filter(Boolean)
    });
  }
  for (const plan of model.content.billing?.plans || []) {
    documents.push({
      kind: "billing",
      title: plan.title,
      summary: plan.body || "",
      href: "#billing",
      tags: [plan.kicker].filter(Boolean)
    });
  }
  for (const tier of model.content.subscriptions?.tiers || []) {
    documents.push({
      kind: "subscription",
      title: tier.title,
      summary: tier.body || "",
      href: "#subscriptions",
      tags: [tier.kicker].filter(Boolean)
    });
  }
  for (const contentType of model.content.cms?.content_types || []) {
    documents.push({
      kind: "cms",
      title: contentType.title,
      summary: contentType.body || "",
      href: "#cms",
      tags: [contentType.kicker].filter(Boolean)
    });
  }
  for (const asset of model.content.media_library?.libraries || []) {
    documents.push({
      kind: "media",
      title: asset.title,
      summary: asset.body || "",
      href: "#media",
      tags: [asset.kicker].filter(Boolean)
    });
  }
  for (const flow of model.content.automation?.flows || []) {
    documents.push({
      kind: "automation",
      title: flow.title,
      summary: flow.body || "",
      href: "#automation",
      tags: [flow.kicker].filter(Boolean)
    });
  }
  for (const event of model.content.webhooks?.events || []) {
    documents.push({
      kind: "webhook",
      title: event.title,
      summary: event.body || "",
      href: "#webhooks",
      tags: [event.kicker].filter(Boolean)
    });
  }
  for (const endpoint of model.content.api_reference?.endpoints || []) {
    documents.push({
      kind: "api",
      title: endpoint.path,
      summary: endpoint.purpose || "",
      href: "#api",
      tags: [endpoint.method].filter(Boolean)
    });
  }
  for (const tool of model.content.developer_portal?.tools || []) {
    documents.push({
      kind: "developer",
      title: tool.title,
      summary: tool.body || "",
      href: "#developer",
      tags: [tool.kicker].filter(Boolean)
    });
  }
  for (const target of model.content.seo_stack?.targets || []) {
    documents.push({
      kind: "seo",
      title: target.title,
      summary: target.body || "",
      href: "#seo",
      tags: [target.kicker].filter(Boolean)
    });
  }
  for (const component of model.content.ui_components || []) {
    documents.push({
      kind: "ui-component",
      title: component.title,
      summary: component.body || "",
      href: "#ui-kit",
      tags: [component.kicker].filter(Boolean)
    });
  }
  for (const layout of model.content.ui_layouts || []) {
    documents.push({
      kind: "ui-layout",
      title: layout.title,
      summary: layout.body || "",
      href: "#ui-kit",
      tags: [layout.kicker].filter(Boolean)
    });
  }
  for (const token of model.content.ui_tokens || []) {
    documents.push({
      kind: "ui-token",
      title: token.title,
      summary: token.body || "",
      href: "#ui-kit",
      tags: [token.kicker].filter(Boolean)
    });
  }
  for (const agent of model.content.ai_agents?.agents || []) {
    documents.push({
      kind: "agent",
      title: agent.title,
      summary: agent.body || "",
      href: "#agents",
      tags: [agent.kicker].filter(Boolean)
    });
  }
  for (const tool of model.content.ai_agents?.tools || []) {
    documents.push({
      kind: "agent-tool",
      title: tool.title,
      summary: tool.body || "",
      href: "#agent-tools",
      tags: [tool.kicker].filter(Boolean)
    });
  }
  for (const workflow of model.content.ai_agents?.workflows || []) {
    documents.push({
      kind: "agent-workflow",
      title: workflow.title,
      summary: workflow.body || "",
      href: "#agent-workflows",
      tags: []
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
  for (const service of model.content.status?.services || []) {
    documents.push({
      kind: "status",
      title: service.name,
      summary: service.detail || "",
      href: "#status",
      tags: [service.status, service.uptime].filter(Boolean)
    });
  }
  for (const incident of model.content.status?.incidents || []) {
    documents.push({
      kind: "incident",
      title: incident.title,
      summary: incident.body || incident.summary || "",
      href: "#status",
      tags: [incident.phase, incident.started_at, incident.resolved_at].filter(Boolean)
    });
  }
  for (const milestone of model.content.roadmap || []) {
    documents.push({
      kind: "roadmap",
      title: milestone.title,
      summary: milestone.body || milestone.summary || "",
      href: "#roadmap",
      tags: [milestone.phase, milestone.eta].filter(Boolean)
    });
  }
  for (const member of model.content.team_members || []) {
    documents.push({
      kind: "team",
      title: member.name,
      summary: member.summary || "",
      href: "#team",
      tags: [member.role, member.focus].filter(Boolean)
    });
  }
  for (const role of model.content.careers?.roles || []) {
    documents.push({
      kind: "career",
      title: role.title,
      summary: role.summary || "",
      href: "#careers",
      tags: [role.location, role.type, ...(role.tags || [])].filter(Boolean)
    });
  }
  for (const channel of model.content.support_channels || []) {
    documents.push({
      kind: "support",
      title: channel.name,
      summary: channel.detail || "",
      href: "#support",
      tags: [channel.availability].filter(Boolean)
    });
  }
  for (const policy of model.content.legal || []) {
    documents.push({
      kind: "legal",
      title: policy.title,
      summary: policy.summary || "",
      href: "#legal",
      tags: [policy.kicker].filter(Boolean)
    });
  }
  for (const control of model.content.security?.controls || []) {
    documents.push({
      kind: "security",
      title: control.title,
      summary: control.detail || "",
      href: "#security",
      tags: [control.status].filter(Boolean)
    });
  }
  for (const channel of model.content.community?.channels || []) {
    documents.push({
      kind: "community",
      title: channel.name,
      summary: channel.summary || "",
      href: "#community",
      tags: [channel.platform, channel.members, channel.cadence].filter(Boolean)
    });
  }
  for (const event of model.content.events?.upcoming || []) {
    documents.push({
      kind: "event",
      title: event.title,
      summary: event.summary || "",
      href: "#events",
      tags: [event.date, event.format, event.focus].filter(Boolean)
    });
  }
  if (model.content.newsletter) {
    documents.push({
      kind: "newsletter",
      title: model.content.newsletter.title || "Newsletter",
      summary: model.content.newsletter.body || "",
      href: "#newsletter",
      tags: [model.content.newsletter.cadence].filter(Boolean)
    });
  }
  for (const control of model.content.compliance?.controls || []) {
    documents.push({
      kind: "compliance",
      title: control.title,
      summary: control.detail || "",
      href: "#compliance",
      tags: [control.status].filter(Boolean)
    });
  }
  for (const signal of model.content.observability?.signals || []) {
    documents.push({
      kind: "observability",
      title: signal.title,
      summary: signal.detail || "",
      href: "#observability",
      tags: [signal.owner, signal.cadence].filter(Boolean)
    });
  }
  for (const item of model.content.infrastructure?.stack || []) {
    documents.push({
      kind: "infrastructure",
      title: item.title,
      summary: item.detail || "",
      href: "#infrastructure",
      tags: [item.tier, item.status].filter(Boolean)
    });
  }
  for (const language of model.content.localization?.languages || []) {
    documents.push({
      kind: "localization",
      title: language.name,
      summary: language.coverage || "",
      href: "#localization",
      tags: [language.status].filter(Boolean)
    });
  }
  for (const region of model.content.localization?.regions || []) {
    documents.push({
      kind: "localization",
      title: region.name,
      summary: region.timezone || "",
      href: "#localization",
      tags: [region.status].filter(Boolean)
    });
  }
  for (const check of model.content.accessibility?.checks || []) {
    documents.push({
      kind: "accessibility",
      title: check.title,
      summary: check.detail || "",
      href: "#accessibility",
      tags: [check.status].filter(Boolean)
    });
  }
  for (const target of model.content.performance?.targets || []) {
    documents.push({
      kind: "performance",
      title: target.title,
      summary: target.detail || "",
      href: "#performance",
      tags: [target.target].filter(Boolean)
    });
  }
  for (const partner of model.content.partners || []) {
    documents.push({
      kind: "partner",
      title: partner.name,
      summary: partner.detail || partner.summary || "",
      href: "#partners",
      tags: [partner.category].filter(Boolean)
    });
  }
  for (const asset of model.content.press_kit?.assets || []) {
    documents.push({
      kind: "press",
      title: asset.label,
      summary: asset.detail || "",
      href: "#press",
      tags: []
    });
  }
  for (const post of model.content.blog_posts || []) {
    const slug = post.slug || slugify(post.title || post.id || "post");
    documents.push({
      kind: "blog",
      title: post.title || slug,
      summary: post.summary || "",
      href: `#blog`,
      tags: post.tags || []
    });
    documents.push({
      kind: "blog-page",
      title: post.title || slug,
      summary: post.summary || "",
      href: `blog/${slug}/`,
      tags: post.tags || []
    });
  }
  return documents;
}

function resolveSeoAssetUrl(baseUrl, outputSlug, assetPath) {
  const raw = String(assetPath || "").trim();
  const cleanedBase = String(baseUrl || "https://example.com").replace(/\/$/, "");
  const cleanedSlug = String(outputSlug || "").replace(/^\/+|\/+$/g, "");
  if (!raw) return `${cleanedBase}/${cleanedSlug}/social-card.svg`;
  if (isLikelyAbsoluteUrl(raw)) return raw;
  if (raw.startsWith("/")) return `${cleanedBase}${raw}`;
  return `${cleanedBase}/${cleanedSlug}/${raw.replace(/^\/+/, "")}`;
}

function buildBlogPosts(model) {
  if (model && Array.isArray(model.__cached_blog_posts)) {
    return model.__cached_blog_posts;
  }
  const entries = Array.isArray(model.content.blog_posts) ? model.content.blog_posts : [];
  const posts = [];
  for (const entry of entries) {
    if (!entry) continue;
    const fallbackSlug = slugify(entry.title || entry.id || "post");
    const slug = slugify(entry.slug || fallbackSlug) || fallbackSlug;
    const loaded = entry.markdown_path ? loadMarkdownDocument(model.context.root_dir, entry.markdown_path) : null;
    const frontmatter = loaded?.attributes || {};
    const title = String(entry.title || frontmatter.title || slug).trim();
    const summary = String(entry.summary || frontmatter.summary || "").trim();
    const publishedAt = String(entry.published_at || frontmatter.published_at || frontmatter.date || "").trim();
    const tags = Array.isArray(entry.tags) ? entry.tags : Array.isArray(frontmatter.tags) ? frontmatter.tags : [];
    posts.push({
      id: entry.id || slug,
      slug,
      title,
      summary,
      published_at: publishedAt || null,
      tags,
      href: `blog/${slug}/`,
      markdown_path: entry.markdown_path || null,
      markdown: loaded?.markdown || String(entry.markdown || ""),
      html: loaded?.html || (entry.markdown ? renderMarkdownToHtml(entry.markdown) : "")
    });
  }
  posts.sort((a, b) => String(b.published_at || "").localeCompare(String(a.published_at || "")));
  if (model) {
    model.__cached_blog_posts = posts;
  }
  return posts;
}

function buildSiteData(model) {
  const blogPosts = buildBlogPosts(model);
  const configuredSearchDocuments = model.content.search_documents || [];
  const searchDocuments = configuredSearchDocuments.length > 0
    ? configuredSearchDocuments
    : buildDerivedSearchDocuments(model);
  const forms = Object.values(model.content.forms || {});
  const clientBundle = getClientBundlePaths(model.context);
  const baseUrl = model.context.app.site?.base_url || "https://example.com";
  const resolvedSocialImage = resolveSeoAssetUrl(
    baseUrl,
    model.experience.output_slug,
    model.content.seo?.image || model.context.app.site?.default_social_image || "social-card.svg"
  );
  const newsUpdates = Array.isArray(model.content.news_items)
    ? model.content.news_items
    : Array.isArray(model.content.timeline)
      ? model.content.timeline
      : [];
  const blogUpdates = blogPosts.map((post) => ({
    title: post.title,
    summary: post.summary || "Blog post",
    href: post.href,
    published_at: post.published_at
  }));
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
      base_url: baseUrl,
      title: model.content.seo?.title || model.experience.page_title,
      description:
        model.content.seo?.description ||
        model.context.app.site?.default_description ||
        `${model.experience.page_title} built with the Kain universal web template.`,
      image: resolvedSocialImage
    },
    search_documents: searchDocuments,
    updates: [...newsUpdates, ...blogUpdates],
    blog: model.content.blog || null,
    blog_posts: blogPosts.map((post) => ({
      id: post.id,
      slug: post.slug,
      title: post.title,
      summary: post.summary,
      published_at: post.published_at,
      tags: post.tags,
      href: post.href
    })),
    client_features: model.context.app.site_runtime.client_features || [],
    prompt_presets: model.content.prompt_presets || [],
    chat_personas: model.content.chat_personas || [],
    chat_modes: model.content.chat_modes || [],
    chat_playbooks: model.content.chat_playbooks || [],
    chat_tools: model.content.chat_tools || [],
    chat_memory: model.content.chat_memory || [],
    actor_playbooks: model.content.actor_playbooks || [],
    actor_tools: model.content.actor_tools || [],
    actor_topology: model.content.actor_topology || null,
    actor_policies: model.content.actor_policies || [],
    actor_metrics: model.content.actor_metrics || [],
    actor_supervision: model.content.actor_supervision || [],
    actor_queues: model.content.actor_queues || [],
    actor_jobs: model.content.actor_jobs || [],
    actor_schedules: model.content.actor_schedules || [],
    actor_hosts: model.content.actor_hosts || [],
    blueprints: model.content.blueprints || [],
    capability_matrix: model.content.capability_matrix || null,
    auth: model.content.auth || null,
    identity: model.content.identity || null,
    identity_verification: model.content.identity_verification || [],
    fraud_risk: model.content.fraud_risk || [],
    consent_center: model.content.consent_center || [],
    audit_logs: model.content.audit_logs || [],
    data_exports: model.content.data_exports || [],
    marketplace_stack: model.content.marketplace_stack || [],
    content_syndication: model.content.content_syndication || [],
    billing: model.content.billing || null,
    subscriptions: model.content.subscriptions || null,
    cms: model.content.cms || null,
    media_library: model.content.media_library || null,
    scene_pipeline: model.content.scene_pipeline || [],
    render_stack: model.content.render_stack || [],
    interaction_modes: model.content.interaction_modes || [],
    device_profiles: model.content.device_profiles || [],
    scene_assets: model.content.scene_assets || [],
    material_library: model.content.material_library || [],
    lighting_rigs: model.content.lighting_rigs || [],
    camera_rigs: model.content.camera_rigs || [],
    animation_stack: model.content.animation_stack || [],
    physics_stack: model.content.physics_stack || [],
    spatial_audio: model.content.spatial_audio || [],
    xr_modes: model.content.xr_modes || [],
    shader_stack: model.content.shader_stack || [],
    streaming_stack: model.content.streaming_stack || [],
    automation: model.content.automation || null,
    webhooks: model.content.webhooks || null,
    api_reference: model.content.api_reference || null,
    developer_portal: model.content.developer_portal || null,
    seo_stack: model.content.seo_stack || null,
    brand_system: model.content.brand_system || [],
    social_presence: model.content.social_presence || [],
    content_calendar: model.content.content_calendar || [],
    release_pipeline: model.content.release_pipeline || [],
    qa_program: model.content.qa_program || [],
    domain_stack: model.content.domain_stack || [],
    trust_center: model.content.trust_center || [],
    ai_agents: model.content.ai_agents || null,
    knowledge_sources: model.content.knowledge_sources || [],
    memory_stores: model.content.memory_stores || [],
    tool_registry: model.content.tool_registry || [],
    agent_workflows: model.content.agent_workflows || [],
    model_stack: model.content.model_stack || [],
    voice_stack: model.content.voice_stack || [],
    moderation_stack: model.content.moderation_stack || [],
    ui_components: model.content.ui_components || [],
    ui_layouts: model.content.ui_layouts || [],
    ui_tokens: model.content.ui_tokens || [],
    frontend_stack: model.content.frontend_stack || [],
    ui_runtime: model.content.ui_runtime || [],
    chat_runtime: model.content.chat_runtime || [],
    actor_runtime: model.content.actor_runtime || [],
    commerce: model.content.commerce || null,
    uploads: model.content.uploads || null,
    analytics: model.content.analytics || null,
    app_modules: model.content.app_modules || [],
    integrations: model.content.integrations || [],
    realtime_channels: model.content.realtime_channels || [],
    data_collections: model.content.data_collections || [],
    growth: model.content.growth || null,
    experiments: model.content.experiments || null,
    service_catalog: model.content.service_catalog || null,
    success: model.content.success || null,
    notifications: model.content.notifications || null,
    release_notes: model.content.release_notes || null,
    feature_flags: model.content.feature_flags || null,
    incident_response: model.content.incident_response || null,
    crm_pipeline: model.content.crm_pipeline || null,
    status: model.content.status || null,
    roadmap: model.content.roadmap || [],
    team_members: model.content.team_members || [],
    partners: model.content.partners || [],
    press_kit: model.content.press_kit || null,
    careers: model.content.careers || null,
    support_channels: model.content.support_channels || [],
    support_tickets: model.content.support_tickets || [],
    feedback_loops: model.content.feedback_loops || [],
    survey_programs: model.content.survey_programs || [],
    messaging_stack: model.content.messaging_stack || [],
    payments_stack: model.content.payments_stack || [],
    scheduling_stack: model.content.scheduling_stack || [],
    privacy_requests: model.content.privacy_requests || [],
    legal: model.content.legal || [],
    security: model.content.security || null,
    community: model.content.community || null,
    events: model.content.events || null,
    newsletter: model.content.newsletter || null,
    compliance: model.content.compliance || null,
    data_governance: model.content.data_governance || [],
    backup_plan: model.content.backup_plan || [],
    observability: model.content.observability || null,
    infrastructure: model.content.infrastructure || null,
    edge_runtime: model.content.edge_runtime || [],
    worker_runtime: model.content.worker_runtime || [],
    api_gateway: model.content.api_gateway || [],
    rate_limits: model.content.rate_limits || [],
    cache_stack: model.content.cache_stack || [],
    search_stack: model.content.search_stack || [],
    storage_stack: model.content.storage_stack || [],
    session_store: model.content.session_store || [],
    runtime_hosts: model.content.runtime_hosts || [],
    deployment_targets: model.content.deployment_targets || [],
    localization: model.content.localization || null,
    accessibility: model.content.accessibility || null,
    performance: model.content.performance || null,
    enablement_programs: model.content.enablement_programs || [],
    onboarding_flows: model.content.onboarding_flows || [],
    data_retention: model.content.data_retention || [],
    reliability_slos: model.content.reliability_slos || [],
    incident_history: model.content.incident_history || []
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
  const rawContent = requireEntry(context.content, experience.content, "content");
  const content = resolveContentIncludes(rawContent, context.content);
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

function renderDocument(model, siteData, options = {}) {
  const { app, experience, theme, content } = {
    app: model.context.app,
    experience: model.experience,
    theme: model.theme,
    content: model.content
  };
  const description = siteData.seo.description;
  const canonicalBase = `${siteData.seo.base_url.replace(/\/$/, "")}/${escapeHtml(experience.output_slug)}/`;
  const canonicalPath = String(options.canonical_path || "").replace(/^\/+/, "");
  const canonicalUrl = canonicalPath ? `${canonicalBase}${canonicalPath}` : canonicalBase;
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
    .panel, .hero-card, .metric-card, .feature-card, .portfolio-card, .route-card, .actor-card, .timeline-row, .pricing-card, .testimonial-card, .doc-card, .link-card, .command-card, .logo-pill, .search-result, .process-card, .prompt-card, .team-card, .support-card, .status-card, .career-card, .legal-card, .press-card, .partner-card, .security-card {
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
    .metric-grid, .feature-grid, .growth-grid, .experiment-grid, .service-grid, .notification-grid, .topology-grid, .portfolio-grid, .docs-grid, .link-grid, .command-grid, .pricing-grid, .testimonial-grid, .process-grid, .prompt-grid, .team-grid, .support-grid, .status-grid, .career-grid, .legal-grid {
      display: grid;
      grid-template-columns: repeat(3, minmax(0, 1fr));
      gap: 14px;
    }
    .metric-card, .feature-card, .portfolio-card, .route-card, .actor-card, .pricing-card, .testimonial-card, .doc-card, .link-card, .command-card, .search-result, .process-card, .prompt-card, .team-card, .support-card, .status-card, .career-card, .legal-card, .press-card, .partner-card, .security-card {
      padding: 16px;
    }
    .status-card[data-status="operational"] { border-color: rgba(90, 228, 255, 0.5); }
    .status-card[data-status="degraded"] { border-color: rgba(255, 209, 102, 0.6); }
    .status-card[data-status="outage"] { border-color: rgba(255, 107, 107, 0.7); }
    .status-board, .careers-shell, .press-kit, .security-shell { display: grid; gap: 16px; }
    .status-incidents { display: grid; gap: 12px; }
    .career-card .portfolio-stack, .support-card .portfolio-stack { color: var(--muted); font-size: 12px; }
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
    .tag-row { display: flex; gap: 8px; flex-wrap: wrap; margin-top: 12px; }
    .tag-pill {
      display: inline-flex;
      align-items: center;
      padding: 6px 10px;
      border-radius: 999px;
      border: 1px solid rgba(255,255,255,0.14);
      background: rgba(255,255,255,0.03);
      color: var(--muted);
      font-size: 11px;
      letter-spacing: 0.16em;
      text-transform: uppercase;
    }
    .markdown { color: var(--muted); line-height: 1.7; }
    .markdown :is(h1, h2, h3, h4, h5, h6) { color: var(--text); margin: 18px 0 10px; }
    .markdown a { color: var(--accent-soft); text-decoration: underline; text-decoration-color: rgba(255,255,255,0.22); }
    .markdown code {
      font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", "Courier New", monospace;
      font-size: 0.95em;
      color: rgba(255,255,255,0.88);
      background: rgba(0,0,0,0.3);
      padding: 2px 6px;
      border-radius: 8px;
      border: 1px solid rgba(255,255,255,0.1);
    }
    .markdown pre {
      overflow-x: auto;
      padding: 14px 16px;
      border-radius: 18px;
      border: 1px solid rgba(255,255,255,0.12);
      background: rgba(0,0,0,0.38);
    }
    .markdown pre code { display: block; padding: 0; border: none; background: transparent; }
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
    .auth-shell, .app-shell, .ui-kit-shell { display: grid; gap: 14px; }
    .ui-kit-summary {
      display: grid;
      grid-template-columns: repeat(3, minmax(0, 1fr));
      gap: 12px;
    }
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
    .kain-status-grid { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 12px; }
    .kain-status-card { border-radius: 16px; border: 1px solid rgba(255,255,255,0.08); padding: 12px; }
    .kain-status-card.operational { border-color: rgba(90, 228, 255, 0.5); }
    .kain-status-card.degraded { border-color: rgba(255, 209, 102, 0.6); }
    .kain-status-card.outage { border-color: rgba(255, 107, 107, 0.7); }
    .kain-status-label { margin: 0 0 6px; font-size: 11px; letter-spacing: 0.2em; text-transform: uppercase; color: var(--accent-soft); }
    .kain-status-meta { margin: 8px 0 0; color: var(--muted); font-size: 12px; }
    .kain-island-actions button:disabled { opacity: 0.6; cursor: default; }
    .kain-island-status { color: var(--muted); font-size: 12px; }
    .kain-realtime-grid { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 12px; }
    .kain-realtime-card { border-radius: 18px; border: 1px solid rgba(255,255,255,0.08); padding: 14px; }
    .kain-realtime-kicker { margin: 0 0 6px; color: var(--accent-soft); font-size: 11px; letter-spacing: 0.18em; text-transform: uppercase; }
    .kain-realtime-title { margin: 0; font-family: var(--font-display); }
    .kain-realtime-copy { margin: 8px 0 0; color: var(--muted); line-height: 1.5; }
    .kain-realtime-meta { margin: 10px 0 0; color: var(--muted); font-size: 12px; }
    .kain-ui-kit-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
      gap: 12px;
    }
    .kain-ui-kit-card {
      border-radius: 18px;
      border: 1px solid rgba(255,255,255,0.08);
      padding: 14px;
      background: rgba(255,255,255,0.02);
    }
    .kain-ui-kit-card h4 { margin: 0 0 6px; font-family: var(--font-display); }
    .kain-ui-kit-card p { margin: 0; color: var(--muted); line-height: 1.5; }
    .kain-chat-log { max-height: 320px; overflow: auto; display: grid; gap: 10px; padding-right: 6px; }
    .kain-chat-bubble { border-radius: 18px; border: 1px solid rgba(255,255,255,0.08); padding: 12px 14px; }
    .kain-chat-bubble.user { background: rgba(255,255,255,0.02); }
    .kain-chat-bubble.assistant { background: rgba(90, 228, 255, 0.06); }
    .kain-chat-role { margin: 0 0 6px; color: var(--accent-soft); font-size: 11px; letter-spacing: 0.18em; text-transform: uppercase; }
    .kain-chat-text { margin: 0; color: var(--muted); line-height: 1.5; white-space: pre-wrap; }
    .kain-chat-controls { display: flex; flex-wrap: wrap; gap: 12px; margin-top: 10px; }
    .kain-chat-controls label { display: grid; gap: 6px; font-size: 12px; color: var(--muted); }
    .kain-chat-controls select {
      min-width: 180px;
      padding: 8px 12px;
      border-radius: 10px;
      border: 1px solid rgba(255,255,255,0.12);
      background: rgba(10, 20, 32, 0.65);
      color: var(--text);
    }
    .kain-chat-agents { margin-top: 12px; }
    .kain-chat-agents-label { margin: 0 0 6px; font-size: 11px; letter-spacing: 0.2em; text-transform: uppercase; color: var(--muted); }
    .kain-chat-agent-grid { display: flex; flex-wrap: wrap; gap: 8px; }
    .kain-chat-agent-pill {
      padding: 6px 10px;
      border-radius: 999px;
      background: rgba(90, 228, 255, 0.08);
      border: 1px solid rgba(90, 228, 255, 0.24);
      font-size: 12px;
      color: var(--text);
    }
    .kain-chat-systems { display: grid; gap: 16px; margin-top: 12px; }
    .kain-chat-system-title {
      margin: 0 0 8px;
      font-size: 11px;
      letter-spacing: 0.2em;
      text-transform: uppercase;
      color: var(--accent-soft);
    }
    .kain-chat-system-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
      gap: 12px;
    }
    .kain-chat-system-card {
      border-radius: 16px;
      border: 1px solid rgba(255,255,255,0.08);
      padding: 12px;
      background: rgba(255,255,255,0.03);
    }
    .kain-chat-system-card h4 { margin: 0 0 6px; font-family: var(--font-display); }
    .kain-chat-system-card p { margin: 0; color: var(--muted); line-height: 1.5; }
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
      .hero-grid, .scene-shell, .metric-grid, .feature-grid, .portfolio-grid, .docs-grid, .link-grid, .command-grid, .pricing-grid, .testimonial-grid, .form-grid, .team-grid, .support-grid, .status-grid, .career-grid, .legal-grid {
        grid-template-columns: 1fr;
      }
      .ui-kit-summary { grid-template-columns: 1fr; }
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

function buildSocialCardSvg(model, siteData) {
  const theme = model.theme;
  const title = String(siteData.seo.title || siteData.page_title || "Kain Web").slice(0, 140);
  const description = String(siteData.seo.description || "").slice(0, 220);
  const accent = theme.accent || "#5ae4ff";
  const soft = theme.accent_soft || "#b8f5ff";
  const bgTop = theme.background_top || "#06111a";
  const bgBottom = theme.background_bottom || "#0b1d2b";

  return `<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="630" viewBox="0 0 1200 630">
  <defs>
    <linearGradient id="bg" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0%" stop-color="${escapeHtml(bgTop)}"/>
      <stop offset="100%" stop-color="${escapeHtml(bgBottom)}"/>
    </linearGradient>
    <radialGradient id="glow" cx="50%" cy="40%" r="60%">
      <stop offset="0%" stop-color="${escapeHtml(soft)}" stop-opacity="0.55"/>
      <stop offset="70%" stop-color="${escapeHtml(accent)}" stop-opacity="0.18"/>
      <stop offset="100%" stop-color="${escapeHtml(accent)}" stop-opacity="0"/>
    </radialGradient>
    <filter id="blur" x="-20%" y="-20%" width="140%" height="140%">
      <feGaussianBlur stdDeviation="22"/>
    </filter>
  </defs>
  <rect width="1200" height="630" fill="url(#bg)"/>
  <circle cx="600" cy="260" r="330" fill="url(#glow)" filter="url(#blur)"/>
  <rect x="70" y="70" width="1060" height="490" rx="46" fill="rgba(255,255,255,0.03)" stroke="rgba(255,255,255,0.12)"/>
  <text x="120" y="185" fill="${escapeHtml(soft)}" font-family="${escapeHtml(theme.font_display || "system-ui")}" font-size="22" letter-spacing="6">KAIN WEB</text>
  <text x="120" y="260" fill="white" font-family="${escapeHtml(theme.font_display || "system-ui")}" font-size="64" font-weight="700">${escapeHtml(title)}</text>
  <text x="120" y="330" fill="rgba(255,255,255,0.72)" font-family="${escapeHtml(theme.font_body || "system-ui")}" font-size="28">${escapeHtml(description)}</text>
  <text x="120" y="520" fill="rgba(255,255,255,0.55)" font-family="${escapeHtml(theme.font_body || "system-ui")}" font-size="22">${escapeHtml(model.context.app.name)} · ${escapeHtml(model.experience.id)}</text>
  <circle cx="1040" cy="165" r="46" fill="${escapeHtml(accent)}" opacity="0.22"/>
  <circle cx="1040" cy="165" r="22" fill="${escapeHtml(accent)}" opacity="0.62"/>
</svg>`;
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
    social_card_path: path.join(model.output_dir, "social-card.svg"),
    server_port: model.context.app.site_runtime.default_port,
    output_dir: model.output_dir,
    route_count: (model.content.server_routes || []).length,
    actor_count: (model.content.actor_roles || []).length,
    form_count: Object.keys(model.content.forms || {}).length,
    search_document_count: (model.content.search_documents || []).length
  };
}

function buildSitemap(siteData) {
  const baseUrl = `${siteData.seo.base_url.replace(/\/$/, "")}/${siteData.output_slug}/`;
  const urls = [baseUrl];
  if (Array.isArray(siteData.blog_posts) && siteData.blog_posts.length > 0) {
    urls.push(`${baseUrl}blog/`);
    for (const post of siteData.blog_posts) {
      const slug = slugify(post.slug || post.id || post.title || "post");
      urls.push(`${baseUrl}blog/${slug}/`);
    }
  }
  return `<?xml version="1.0" encoding="UTF-8"?>
<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">
${urls.map((url) => `  <url>\n    <loc>${escapeHtml(url)}</loc>\n  </url>`).join("\n")}
</urlset>
`;
}

function buildRobots(siteData) {
  return `User-agent: *\nAllow: /\nSitemap: ${siteData.seo.base_url.replace(/\/$/, "")}/${siteData.output_slug}/sitemap.xml\n`;
}

function buildFeed(siteData) {
  const siteUrl = `${siteData.seo.base_url.replace(/\/$/, "")}/${siteData.output_slug}/`;
  const sourceItems = Array.isArray(siteData.blog_posts) && siteData.blog_posts.length > 0
    ? siteData.blog_posts
    : (siteData.updates || []);
  const items = sourceItems
    .slice(0, 8)
    .map((entry, index) => {
      const title = entry.title || entry.phase || `Update ${index + 1}`;
      const description = entry.body || entry.summary || "";
      const href = entry.href || "";
      const link = isLikelyAbsoluteUrl(href)
        ? href
        : href.startsWith("/")
          ? `${siteData.seo.base_url.replace(/\/$/, "")}${href}`
          : href
            ? `${siteUrl}${href.replace(/^\/+/, "")}`
            : siteUrl;
      const pubDate = entry.published_at ? `<pubDate>${escapeHtml(new Date(entry.published_at).toUTCString())}</pubDate>` : "";
      return `<item><title>${escapeHtml(title)}</title><link>${escapeHtml(link)}</link><description>${escapeHtml(description)}</description>${pubDate}</item>`;
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
    ui_kit: "/api/ui-kit",
    frontend_stack: "/api/frontend",
    ui_runtime: "/api/ui-runtime",
    chat_runtime: "/api/chat/runtime",
    actor_runtime: "/api/actors/runtime",
    chat_playbooks: "/api/chat/playbooks",
    chat_tools: "/api/chat/tools",
    chat_memory: "/api/chat/memory",
    chat_models: "/api/chat/models",
    voice: "/api/voice",
    moderation: "/api/moderation",
    seo_stack: "/api/seo",
    brand_system: "/api/brand",
    social_presence: "/api/social",
    content_calendar: "/api/content/calendar",
    release_pipeline: "/api/release/pipeline",
    qa_program: "/api/qa",
    domain_stack: "/api/domains",
    trust_center: "/api/trust",
    knowledge_sources: "/api/agents/knowledge",
    memory_stores: "/api/agents/memory",
    tool_registry: "/api/agents/tools",
    agent_workflows: "/api/agents/workflows",
    growth: "/api/growth",
    experiments: "/api/experiments",
    services: "/api/services",
    success: "/api/success",
    notifications: "/api/notifications",
    releases: "/api/releases",
    feature_flags: "/api/feature-flags",
    incident_response: "/api/incidents",
    crm_pipeline: "/api/crm",
    identity_verification: "/api/identity/verification",
    fraud_risk: "/api/risk",
    consent_center: "/api/consent",
    audit_logs: "/api/audit",
    data_exports: "/api/data-exports",
    marketplace_stack: "/api/marketplace",
    content_syndication: "/api/syndication",
    actor_topology: "/api/actors/topology",
    actor_policies: "/api/actors/policies",
    actor_metrics: "/api/actors/metrics",
    actor_supervision: "/api/actors/supervision",
    actor_queues: "/api/actors/queues",
    actor_jobs: "/api/actors/jobs",
    actor_schedules: "/api/actors/schedules",
    actor_hosts: "/api/actors/hosts",
    scene_pipeline: "/api/3d/pipeline",
    render_stack: "/api/3d/render",
    interaction_modes: "/api/3d/interaction",
    device_profiles: "/api/3d/devices",
    scene_assets: "/api/3d/assets",
    material_library: "/api/3d/materials",
    lighting_rigs: "/api/3d/lighting",
    camera_rigs: "/api/3d/cameras",
    animation_stack: "/api/3d/animation",
    physics_stack: "/api/3d/physics",
    spatial_audio: "/api/3d/audio",
    xr_modes: "/api/3d/xr",
    shader_stack: "/api/3d/shaders",
    streaming_stack: "/api/streaming",
    status: "/api/status",
    roadmap: "/api/roadmap",
    support: "/api/support",
    support_tickets: "/api/support/tickets",
    feedback: "/api/feedback",
    surveys: "/api/surveys",
    messaging: "/api/messaging",
    payments: "/api/payments",
    scheduling: "/api/scheduling",
    privacy_requests: "/api/privacy/requests",
    legal: "/api/legal",
    security: "/api/security",
    community: "/api/community",
    events: "/api/events",
    newsletter: "/api/newsletter",
    compliance: "/api/compliance",
    data_governance: "/api/data-governance",
    backup_plan: "/api/backups",
    observability: "/api/observability",
    infrastructure: "/api/infrastructure",
    edge_runtime: "/api/runtime/edge",
    worker_runtime: "/api/runtime/workers",
    api_gateway: "/api/runtime/gateway",
    rate_limits: "/api/runtime/rate-limits",
    cache_stack: "/api/runtime/cache",
    search_stack: "/api/runtime/search",
    storage_stack: "/api/runtime/storage",
    session_store: "/api/runtime/sessions",
    runtime_hosts: "/api/runtime/hosts",
    deployment_targets: "/api/runtime/deployments",
    localization: "/api/localization",
    accessibility: "/api/accessibility",
    performance: "/api/performance",
    enablement_programs: "/api/enablement",
    onboarding_flows: "/api/onboarding",
    data_retention: "/api/data-retention",
    reliability_slos: "/api/reliability",
    incident_history: "/api/incidents/history",
    team: "/api/team",
    partners: "/api/partners",
    press: "/api/press",
    careers: "/api/careers",
    auth: siteData.auth || null,
    identity: siteData.identity || null,
    identity_verification: siteData.identity_verification || [],
    fraud_risk: siteData.fraud_risk || [],
    consent_center: siteData.consent_center || [],
    audit_logs: siteData.audit_logs || [],
    data_exports: siteData.data_exports || [],
    marketplace_stack: siteData.marketplace_stack || [],
    content_syndication: siteData.content_syndication || [],
    billing: siteData.billing || null,
    subscriptions: siteData.subscriptions || null,
    cms: siteData.cms || null,
    media_library: siteData.media_library || null,
    scene_pipeline: siteData.scene_pipeline || [],
    render_stack: siteData.render_stack || [],
    interaction_modes: siteData.interaction_modes || [],
    device_profiles: siteData.device_profiles || [],
    scene_assets: siteData.scene_assets || [],
    material_library: siteData.material_library || [],
    lighting_rigs: siteData.lighting_rigs || [],
    camera_rigs: siteData.camera_rigs || [],
    animation_stack: siteData.animation_stack || [],
    physics_stack: siteData.physics_stack || [],
    spatial_audio: siteData.spatial_audio || [],
    xr_modes: siteData.xr_modes || [],
    shader_stack: siteData.shader_stack || [],
    streaming_stack: siteData.streaming_stack || [],
    automation: siteData.automation || null,
    webhooks: siteData.webhooks || null,
    api_reference: siteData.api_reference || null,
    developer_portal: siteData.developer_portal || null,
    seo_stack: siteData.seo_stack || null,
    brand_system: siteData.brand_system || [],
    social_presence: siteData.social_presence || [],
    content_calendar: siteData.content_calendar || [],
    release_pipeline: siteData.release_pipeline || [],
    qa_program: siteData.qa_program || [],
    domain_stack: siteData.domain_stack || [],
    trust_center: siteData.trust_center || [],
    ai_agents: siteData.ai_agents || null,
    knowledge_sources: siteData.knowledge_sources || [],
    memory_stores: siteData.memory_stores || [],
    tool_registry: siteData.tool_registry || [],
    agent_workflows: siteData.agent_workflows || [],
    ui_components: siteData.ui_components || [],
    ui_layouts: siteData.ui_layouts || [],
    ui_tokens: siteData.ui_tokens || [],
    frontend_stack: siteData.frontend_stack || [],
    ui_runtime: siteData.ui_runtime || [],
    chat_runtime: siteData.chat_runtime || [],
    actor_runtime: siteData.actor_runtime || [],
    commerce: siteData.commerce || null,
    app_modules: siteData.app_modules || [],
    integrations: siteData.integrations || [],
    realtime_channels: siteData.realtime_channels || [],
    data_collections: siteData.data_collections || [],
    growth: siteData.growth || null,
    experiments: siteData.experiments || null,
    service_catalog: siteData.service_catalog || null,
    success: siteData.success || null,
    notifications: siteData.notifications || null,
    release_notes: siteData.release_notes || null,
    feature_flags: siteData.feature_flags || null,
    incident_response: siteData.incident_response || null,
    crm_pipeline: siteData.crm_pipeline || null,
    status_data: siteData.status || null,
    roadmap_items: siteData.roadmap || [],
    support_channels: siteData.support_channels || [],
    support_tickets: siteData.support_tickets || [],
    feedback_loops: siteData.feedback_loops || [],
    survey_programs: siteData.survey_programs || [],
    messaging_stack: siteData.messaging_stack || [],
    payments_stack: siteData.payments_stack || [],
    scheduling_stack: siteData.scheduling_stack || [],
    privacy_requests: siteData.privacy_requests || [],
    legal: siteData.legal || [],
    security: siteData.security || null,
    community: siteData.community || null,
    events: siteData.events || null,
    newsletter: siteData.newsletter || null,
    compliance: siteData.compliance || null,
    observability: siteData.observability || null,
    infrastructure: siteData.infrastructure || null,
    localization: siteData.localization || null,
    accessibility: siteData.accessibility || null,
    performance: siteData.performance || null,
    enablement_programs: siteData.enablement_programs || [],
    onboarding_flows: siteData.onboarding_flows || [],
    data_retention: siteData.data_retention || [],
    reliability_slos: siteData.reliability_slos || [],
    incident_history: siteData.incident_history || [],
    actor_policies: siteData.actor_policies || [],
    actor_metrics: siteData.actor_metrics || [],
    actor_supervision: siteData.actor_supervision || [],
    actor_queues: siteData.actor_queues || [],
    actor_jobs: siteData.actor_jobs || [],
    actor_schedules: siteData.actor_schedules || [],
    actor_hosts: siteData.actor_hosts || [],
    actor_jobs: siteData.actor_jobs || [],
    actor_schedules: siteData.actor_schedules || [],
    actor_hosts: siteData.actor_hosts || [],
    team_members: siteData.team_members || [],
    partners: siteData.partners || [],
    press_kit: siteData.press_kit || null,
    careers: siteData.careers || null,
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
    if (kind === "status_board") return "status";
    if (kind === "auth_session") return "auth-session";
    if (kind === "uploads_lab") return "uploads";
    if (kind === "analytics_lab") return "analytics";
    if (kind === "ui_kit") return "ui-kit";
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
                : island === "status"
                  ? { status: "/api/status" }
                : island === "auth-session"
                  ? { me: "/api/auth/session", login: "/api/auth/session/login", logout: "/api/auth/session/logout" }
                : island === "uploads"
                  ? { upload: "/api/uploads", serve_prefix: "/uploads/" }
                  : island === "analytics"
                    ? { event: "/api/analytics/event", events: "/api/analytics/events" }
                    : island === "ui-kit"
                      ? { ui_kit: "/api/ui-kit" }
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
      })),
      frontend_stack: (siteData.frontend_stack || []).length,
      ui_runtime: (siteData.ui_runtime || []).length,
      chat_runtime: (siteData.chat_runtime || []).length,
      actor_runtime: (siteData.actor_runtime || []).length,
      growth_campaigns: (siteData.growth?.campaigns || []).length,
      experiment_count: (siteData.experiments?.tests || []).length,
      service_count: (siteData.service_catalog?.services || []).length,
      success_playbooks: (siteData.success?.playbooks || []).length,
      notification_channels: (siteData.notifications?.channels || []).length,
      release_notes: (siteData.release_notes?.entries || []).length,
      feature_flags: (siteData.feature_flags?.flags || []).length,
      incident_playbooks: (siteData.incident_response?.playbooks || []).length,
      crm_stages: (siteData.crm_pipeline?.stages || []).length,
      brand_system: (siteData.brand_system || []).length,
      social_presence: (siteData.social_presence || []).length,
      content_calendar: (siteData.content_calendar || []).length,
      release_pipeline: (siteData.release_pipeline || []).length,
      qa_program: (siteData.qa_program || []).length,
      domain_stack: (siteData.domain_stack || []).length,
      trust_center: (siteData.trust_center || []).length,
      identity_verification: (siteData.identity_verification || []).length,
      fraud_risk: (siteData.fraud_risk || []).length,
      consent_center: (siteData.consent_center || []).length,
      audit_logs: (siteData.audit_logs || []).length,
      data_exports: (siteData.data_exports || []).length,
      marketplace_stack: (siteData.marketplace_stack || []).length,
      content_syndication: (siteData.content_syndication || []).length,
      actor_nodes: (siteData.actor_topology?.nodes || []).length,
      actor_supervision: (siteData.actor_supervision || []).length,
      actor_queues: (siteData.actor_queues || []).length,
      status_services: (siteData.status?.services || []).length,
      roadmap_items: (siteData.roadmap || []).length,
      support_channels: (siteData.support_channels || []).length,
      support_tickets: (siteData.support_tickets || []).length,
      feedback_loops: (siteData.feedback_loops || []).length,
      survey_programs: (siteData.survey_programs || []).length,
      messaging_stack: (siteData.messaging_stack || []).length,
      payments_stack: (siteData.payments_stack || []).length,
      scheduling_stack: (siteData.scheduling_stack || []).length,
      privacy_requests: (siteData.privacy_requests || []).length,
      team_members: (siteData.team_members || []).length,
      careers: (siteData.careers?.roles || []).length,
      legal_links: (siteData.legal || []).length,
      community_channels: (siteData.community?.channels || []).length,
      event_count: (siteData.events?.upcoming || []).length,
      newsletter_enabled: siteData.newsletter ? 1 : 0,
      compliance_controls: (siteData.compliance?.controls || []).length,
      data_governance: (siteData.data_governance || []).length,
      backup_plan: (siteData.backup_plan || []).length,
      observability_signals: (siteData.observability?.signals || []).length,
      infrastructure_stack: (siteData.infrastructure?.stack || []).length,
      edge_runtime: (siteData.edge_runtime || []).length,
      worker_runtime: (siteData.worker_runtime || []).length,
      api_gateway: (siteData.api_gateway || []).length,
      rate_limits: (siteData.rate_limits || []).length,
      cache_stack: (siteData.cache_stack || []).length,
      search_stack: (siteData.search_stack || []).length,
      storage_stack: (siteData.storage_stack || []).length,
      session_store: (siteData.session_store || []).length,
      localization_languages: (siteData.localization?.languages || []).length,
      accessibility_checks: (siteData.accessibility?.checks || []).length,
      performance_targets: (siteData.performance?.targets || []).length,
      enablement_programs: (siteData.enablement_programs || []).length,
      onboarding_flows: (siteData.onboarding_flows || []).length,
      data_retention: (siteData.data_retention || []).length,
      reliability_slos: (siteData.reliability_slos || []).length,
      incident_history: (siteData.incident_history || []).length,
      scene_pipeline: (siteData.scene_pipeline || []).length,
      render_stack: (siteData.render_stack || []).length,
      interaction_modes: (siteData.interaction_modes || []).length,
      device_profiles: (siteData.device_profiles || []).length,
      scene_assets: (siteData.scene_assets || []).length,
      material_library: (siteData.material_library || []).length,
      lighting_rigs: (siteData.lighting_rigs || []).length,
      camera_rigs: (siteData.camera_rigs || []).length,
      animation_stack: (siteData.animation_stack || []).length,
      physics_stack: (siteData.physics_stack || []).length,
      spatial_audio: (siteData.spatial_audio || []).length,
      xr_modes: (siteData.xr_modes || []).length,
      shader_stack: (siteData.shader_stack || []).length,
      streaming_stack: (siteData.streaming_stack || []).length,
      model_stack: (siteData.model_stack || []).length,
      voice_stack: (siteData.voice_stack || []).length,
      moderation_stack: (siteData.moderation_stack || []).length,
      knowledge_sources: (siteData.knowledge_sources || []).length,
      memory_stores: (siteData.memory_stores || []).length,
      tool_registry: (siteData.tool_registry || []).length,
      agent_workflows: (siteData.agent_workflows || []).length,
      actor_jobs: (siteData.actor_jobs || []).length,
      actor_schedules: (siteData.actor_schedules || []).length,
      actor_hosts: (siteData.actor_hosts || []).length,
      runtime_hosts: (siteData.runtime_hosts || []).length,
      deployment_targets: (siteData.deployment_targets || []).length
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
  const blogPosts = buildBlogPosts(model);

  const pages = [];

  if (blogPosts.length > 0) {
    const blogIndexSiteData = {
      ...siteData,
      page_title: `${siteData.page_title} · Blog`,
      seo: {
        ...siteData.seo,
        title: `${siteData.seo.title} · Blog`,
        description: siteData.blog?.body || siteData.seo.description
      }
    };
    const blogIndexModel = {
      ...model,
      experience: {
        ...model.experience,
        eyebrow: "Blog",
        sections: [
          { id: "blog", kind: "blog_roll", eyebrow: siteData.blog?.kicker || "Blog", title: siteData.blog?.title || "Blog", source: "content.blog_posts" }
        ]
      },
      content: {
        ...model.content,
        hero: {
          kicker: siteData.blog?.kicker || "Blog",
          title: siteData.blog?.title || "Blog",
          body: siteData.blog?.body || "Markdown-driven posts emitted as first-class artifacts.",
          actions: [{ label: "Back to site", href: "../", style: "secondary" }]
        },
        metrics: [],
        nav: [
          { label: "Home", href: "../" },
          { label: "Blog", href: "./" }
        ],
        footer: "blog index"
      }
    };
    pages.push({
      route: "/blog/",
      output_path: path.join(model.output_dir, "blog", "index.html"),
      html: renderDocument(blogIndexModel, blogIndexSiteData, { canonical_path: "blog/" })
    });

    for (const post of blogPosts) {
      const slug = slugify(post.slug || post.id || post.title || "post");
      const postSiteData = {
        ...siteData,
        page_title: post.title,
        seo: {
          ...siteData.seo,
          title: `${post.title} · ${siteData.page_title}`,
          description: post.summary || siteData.seo.description
        }
      };
      const postModel = {
        ...model,
        experience: {
          ...model.experience,
          eyebrow: "Blog",
          sections: [
            {
              id: "post",
              kind: "markdown_panel",
              eyebrow: post.published_at ? `Published ${formatBlogDate(post.published_at)}` : "Post",
              title: post.title,
              body: post.markdown || ""
            }
          ]
        },
        content: {
          ...model.content,
          hero: {
            kicker: "Blog post",
            title: post.title,
            body: post.summary || siteData.seo.description,
            actions: [{ label: "Back to blog", href: "../", style: "secondary" }, { label: "Back to site", href: "../../", style: "secondary" }]
          },
          metrics: [],
          nav: [
            { label: "Home", href: "../../" },
            { label: "Blog", href: "../" }
          ],
          footer: `blog post ${slug}`
        }
      };
      pages.push({
        route: `/blog/${slug}/`,
        output_path: path.join(model.output_dir, "blog", slug, "index.html"),
        html: renderDocument(postModel, postSiteData, { canonical_path: `blog/${slug}/` })
      });
    }
  }

  const assets = [
    {
      route: "/social-card.svg",
      output_path: summary.social_card_path,
      content_type: "image/svg+xml; charset=utf-8",
      text: buildSocialCardSvg(model, siteData)
    }
  ];

  return {
    ...summary,
    html: renderDocument(model, siteData),
    pages,
    assets,
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

function buildChatReply(bundle, plan, prompt, options = {}) {
  const lowered = String(prompt || "").toLowerCase();
  const persona = String(options.persona || "").trim();
  const mode = String(options.mode || "").trim();
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
  const agentCount = (bundle.site_data.ai_agents?.agents || []).length;
  const personaNote = persona ? ` Persona: ${persona}.` : "";
  const modeNote = mode ? ` Mode: ${mode}.` : "";
  const suggestion = suggestedPrompt ? ` Suggested prompt: "${suggestedPrompt}".` : "";
  return `Local Kain web runtime received '${prompt}'.${personaNote}${modeNote} Route this request through ${nextLane}. Current experience '${bundle.id}' exposes ${routeCount} routes, ${actorCount} actors, ${agentCount} agents, and forms [${formIds}].${suggestion}`;
}

function buildApiRoutes(model, siteData) {
  const builtInRoutes = [
    { method: "GET", path: "/", purpose: "serves the experience shell", actor: "site_renderer" },
    { method: "GET", path: "/site.data.json", purpose: "returns the flattened site data payload", actor: "site_renderer" },
    { method: "GET", path: "/sitemap.xml", purpose: "returns sitemap output for the current experience", actor: "site_renderer" },
    { method: "GET", path: "/robots.txt", purpose: "returns crawler policy", actor: "site_renderer" },
    { method: "GET", path: "/feed.xml", purpose: "returns the local update feed", actor: "site_renderer" },
    { method: "GET", path: "/social-card.svg", purpose: "returns the generated social-card SVG", actor: "site_renderer" },
    { method: "GET", path: "/api/runtime", purpose: "returns active runtime metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/catalog", purpose: "returns the available experience catalog", actor: "runtime_reporter" },
    { method: "GET", path: "/api/routes", purpose: "returns the route contract", actor: "mesh_supervisor" },
    { method: "GET", path: "/api/site", purpose: "returns site data and seo metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/scene", purpose: "returns the current scene descriptor", actor: "site_renderer" },
    { method: "GET", path: "/api/3d/pipeline", purpose: "returns 3D scene pipeline metadata", actor: "site_renderer" },
    { method: "GET", path: "/api/3d/render", purpose: "returns render stack metadata", actor: "site_renderer" },
    { method: "GET", path: "/api/3d/interaction", purpose: "returns 3D interaction mode metadata", actor: "site_renderer" },
    { method: "GET", path: "/api/3d/devices", purpose: "returns device profile metadata", actor: "site_renderer" },
    { method: "GET", path: "/api/3d/assets", purpose: "returns scene asset metadata", actor: "site_renderer" },
    { method: "GET", path: "/api/3d/materials", purpose: "returns material library metadata", actor: "site_renderer" },
    { method: "GET", path: "/api/3d/lighting", purpose: "returns lighting rig metadata", actor: "site_renderer" },
    { method: "GET", path: "/api/3d/cameras", purpose: "returns camera rig metadata", actor: "site_renderer" },
    { method: "GET", path: "/api/3d/animation", purpose: "returns animation stack metadata", actor: "site_renderer" },
    { method: "GET", path: "/api/3d/physics", purpose: "returns physics stack metadata", actor: "site_renderer" },
    { method: "GET", path: "/api/3d/audio", purpose: "returns spatial audio metadata", actor: "site_renderer" },
    { method: "GET", path: "/api/3d/xr", purpose: "returns XR mode metadata", actor: "site_renderer" },
    { method: "GET", path: "/api/3d/shaders", purpose: "returns shader stack metadata", actor: "site_renderer" },
    { method: "GET", path: "/api/streaming", purpose: "returns streaming stack metadata", actor: "signal_broker" },
    { method: "GET", path: "/api/forms", purpose: "returns the available form contracts", actor: "intake_collector" },
    { method: "GET", path: "/api/blog/posts", purpose: "returns the blog post registry metadata", actor: "site_renderer" },
    { method: "GET", path: "/api/search/documents", purpose: "returns the local search document index", actor: "search_indexer" },
    { method: "GET", path: "/api/search", purpose: "queries the local search document index", actor: "search_indexer" },
    { method: "GET", path: "/api/chat", purpose: "returns chat seed messages or a local reply", actor: "chat_seed_router" },
    { method: "POST", path: "/api/chat", purpose: "accepts a prompt payload and returns a local reply", actor: "chat_seed_router" },
    { method: "GET", path: "/api/chat/stream", purpose: "returns a server-sent event preview for local chat pipelines", actor: "chat_seed_router" },
    { method: "GET", path: "/api/chat/models", purpose: "returns model stack metadata", actor: "chat_seed_router" },
    { method: "GET", path: "/api/voice", purpose: "returns voice stack metadata", actor: "chat_seed_router" },
    { method: "GET", path: "/api/moderation", purpose: "returns moderation policy metadata", actor: "chat_seed_router" },
    { method: "GET", path: "/api/app", purpose: "returns the app module manifest for react-like workspace shells", actor: "runtime_reporter" },
    { method: "GET", path: "/api/auth", purpose: "returns authentication strategy metadata", actor: "auth_gateway" },
    { method: "GET", path: "/api/auth/session", purpose: "returns the current session identity (cookie-backed)", actor: "auth_gateway" },
    { method: "POST", path: "/api/auth/session/login", purpose: "creates a local session identity (dev-only)", actor: "auth_gateway" },
    { method: "POST", path: "/api/auth/session/logout", purpose: "clears the active session identity", actor: "auth_gateway" },
    { method: "GET", path: "/api/identity", purpose: "returns identity providers, roles, and policy metadata", actor: "auth_gateway" },
    { method: "GET", path: "/api/identity/verification", purpose: "returns identity verification metadata", actor: "auth_gateway" },
    { method: "GET", path: "/api/risk", purpose: "returns fraud and risk metadata", actor: "mesh_supervisor" },
    { method: "GET", path: "/api/consent", purpose: "returns consent and preference metadata", actor: "auth_gateway" },
    { method: "GET", path: "/api/audit", purpose: "returns audit log metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/data-exports", purpose: "returns data export metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/marketplace", purpose: "returns marketplace stack metadata", actor: "commerce_orchestrator" },
    { method: "GET", path: "/api/syndication", purpose: "returns content syndication metadata", actor: "content_keeper" },
    { method: "GET", path: "/api/billing", purpose: "returns billing plans and invoice metadata", actor: "commerce_orchestrator" },
    { method: "GET", path: "/api/subscriptions", purpose: "returns subscription tier metadata", actor: "commerce_orchestrator" },
    { method: "GET", path: "/api/cms", purpose: "returns content type and workflow metadata", actor: "content_keeper" },
    { method: "GET", path: "/api/media", purpose: "returns media library metadata", actor: "media_keeper" },
    { method: "GET", path: "/api/automation", purpose: "returns automation flow metadata", actor: "automation_orchestrator" },
    { method: "GET", path: "/api/webhooks", purpose: "returns webhook event metadata", actor: "signal_broker" },
    { method: "GET", path: "/api/api-reference", purpose: "returns the API reference registry", actor: "runtime_reporter" },
    { method: "GET", path: "/api/developer", purpose: "returns developer portal metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/seo", purpose: "returns SEO target metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/agents", purpose: "returns chat agent roster metadata", actor: "chat_seed_router" },
    { method: "GET", path: "/api/ui-kit", purpose: "returns UI kit components, layouts, and tokens", actor: "runtime_reporter" },
    { method: "GET", path: "/api/frontend", purpose: "returns frontend stack metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/ui-runtime", purpose: "returns UI runtime metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/chat/runtime", purpose: "returns chat runtime metadata", actor: "chat_seed_router" },
    { method: "GET", path: "/api/actors/runtime", purpose: "returns actor runtime metadata", actor: "mesh_supervisor" },
    { method: "GET", path: "/api/chat/playbooks", purpose: "returns chat playbook metadata", actor: "chat_seed_router" },
    { method: "GET", path: "/api/chat/tools", purpose: "returns chat tool metadata", actor: "chat_seed_router" },
    { method: "GET", path: "/api/chat/memory", purpose: "returns chat memory lane metadata", actor: "chat_seed_router" },
    { method: "GET", path: "/api/agents/knowledge", purpose: "returns knowledge source metadata", actor: "chat_seed_router" },
    { method: "GET", path: "/api/agents/memory", purpose: "returns agent memory store metadata", actor: "chat_seed_router" },
    { method: "GET", path: "/api/agents/tools", purpose: "returns agent tool registry metadata", actor: "chat_seed_router" },
    { method: "GET", path: "/api/agents/workflows", purpose: "returns agent workflow metadata", actor: "chat_seed_router" },
    { method: "GET", path: "/api/actors/policies", purpose: "returns actor policy metadata", actor: "mesh_supervisor" },
    { method: "GET", path: "/api/actors/metrics", purpose: "returns actor metric metadata", actor: "mesh_supervisor" },
    { method: "GET", path: "/api/actors/supervision", purpose: "returns actor supervision metadata", actor: "mesh_supervisor" },
    { method: "GET", path: "/api/actors/queues", purpose: "returns actor queue metadata", actor: "mesh_supervisor" },
    { method: "GET", path: "/api/actors/jobs", purpose: "returns actor job metadata", actor: "mesh_supervisor" },
    { method: "GET", path: "/api/actors/schedules", purpose: "returns actor schedule metadata", actor: "mesh_supervisor" },
    { method: "GET", path: "/api/actors/hosts", purpose: "returns actor host metadata", actor: "mesh_supervisor" },
    { method: "GET", path: "/api/commerce", purpose: "returns sellable offers and membership metadata", actor: "commerce_orchestrator" },
    { method: "GET", path: "/api/data", purpose: "returns typed collection and persistence metadata", actor: "data_keeper" },
    { method: "GET", path: "/api/data-governance", purpose: "returns data governance metadata", actor: "data_keeper" },
    { method: "GET", path: "/api/backups", purpose: "returns backup plan metadata", actor: "data_keeper" },
    { method: "GET", path: "/api/growth", purpose: "returns growth campaign and funnel metadata", actor: "growth_ops" },
    { method: "GET", path: "/api/experiments", purpose: "returns experiment and A/B test metadata", actor: "growth_ops" },
    { method: "GET", path: "/api/services", purpose: "returns service catalog and SLA metadata", actor: "service_manager" },
    { method: "GET", path: "/api/success", purpose: "returns customer success playbooks", actor: "success_lead" },
    { method: "GET", path: "/api/notifications", purpose: "returns notification channel metadata", actor: "signal_broker" },
    { method: "GET", path: "/api/releases", purpose: "returns release notes and changelog metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/feature-flags", purpose: "returns feature flag registry metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/incidents", purpose: "returns incident response playbooks", actor: "mesh_supervisor" },
    { method: "GET", path: "/api/crm", purpose: "returns CRM pipeline metadata", actor: "growth_ops" },
    { method: "GET", path: "/api/integrations", purpose: "returns upstream system connectors and transports", actor: "integration_router" },
    { method: "GET", path: "/api/status", purpose: "returns status board metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/roadmap", purpose: "returns roadmap milestones", actor: "runtime_reporter" },
    { method: "GET", path: "/api/support", purpose: "returns support channels", actor: "runtime_reporter" },
    { method: "GET", path: "/api/support/tickets", purpose: "returns support ticket queue metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/feedback", purpose: "returns feedback loop metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/surveys", purpose: "returns survey program metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/messaging", purpose: "returns messaging stack metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/payments", purpose: "returns payments stack metadata", actor: "commerce_orchestrator" },
    { method: "GET", path: "/api/scheduling", purpose: "returns scheduling stack metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/privacy/requests", purpose: "returns privacy request metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/legal", purpose: "returns legal and policy links", actor: "runtime_reporter" },
    { method: "GET", path: "/api/security", purpose: "returns security control metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/community", purpose: "returns community channels and cohorts", actor: "runtime_reporter" },
    { method: "GET", path: "/api/events", purpose: "returns upcoming event schedule", actor: "runtime_reporter" },
    { method: "GET", path: "/api/newsletter", purpose: "returns newsletter metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/compliance", purpose: "returns compliance and governance metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/observability", purpose: "returns operational signals metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/infrastructure", purpose: "returns infrastructure stack metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/runtime/edge", purpose: "returns edge runtime metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/runtime/workers", purpose: "returns worker runtime metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/runtime/gateway", purpose: "returns API gateway metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/runtime/rate-limits", purpose: "returns rate limit policy metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/runtime/cache", purpose: "returns cache stack metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/runtime/search", purpose: "returns search stack metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/runtime/storage", purpose: "returns storage stack metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/runtime/sessions", purpose: "returns session store metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/runtime/hosts", purpose: "returns runtime host metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/runtime/deployments", purpose: "returns deployment target metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/localization", purpose: "returns localization metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/accessibility", purpose: "returns accessibility metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/performance", purpose: "returns performance target metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/team", purpose: "returns team metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/partners", purpose: "returns partner metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/press", purpose: "returns press kit metadata", actor: "runtime_reporter" },
    { method: "GET", path: "/api/careers", purpose: "returns careers metadata", actor: "runtime_reporter" },
    { method: "POST", path: "/api/uploads", purpose: "accepts base64 uploads and persists them under the runtime folder", actor: "upload_gate" },
    { method: "GET", path: "/uploads/*", purpose: "serves uploaded files from the runtime uploads folder (local server only)", actor: "upload_gate" },
    { method: "POST", path: "/api/analytics/event", purpose: "captures client analytics events to JSONL", actor: "analytics_sentinel" },
    { method: "GET", path: "/api/analytics/events", purpose: "returns recent analytics events (local server only)", actor: "analytics_sentinel" },
    { method: "GET", path: "/api/realtime", purpose: "returns live channel descriptors and event cadence", actor: "signal_broker" },
    { method: "GET", path: "/api/realtime/stream", purpose: "returns a server-sent event preview for realtime channels", actor: "signal_broker" },
    { method: "WS", path: "/ws/realtime", purpose: "websocket stream for realtime channels", actor: "signal_broker" },
    { method: "WS", path: "/ws/chat", purpose: "websocket message lane for chat experiments", actor: "chat_seed_router" },
    { method: "GET", path: "/api/actors/topology", purpose: "returns actor mesh topology nodes and edges", actor: "mesh_supervisor" },
    { method: "GET", path: "/api/system.contract.json", purpose: "returns the complete website system contract", actor: "runtime_reporter" },
    { method: "GET", path: "/api/ui.schema.json", purpose: "returns the UI composition schema", actor: "runtime_reporter" },
    { method: "GET", path: "/api/actors", purpose: "returns actor topology and role descriptions", actor: "mesh_supervisor" },
    { method: "GET", path: "/healthz", purpose: "simple health response for local supervision", actor: "mesh_supervisor" }
  ];

  if (Array.isArray(siteData.blog_posts) && siteData.blog_posts.length > 0) {
    builtInRoutes.push({ method: "GET", path: "/blog/", purpose: "returns the blog index page", actor: "site_renderer" });
    builtInRoutes.push({ method: "GET", path: "/blog/*", purpose: "serves generated blog post pages", actor: "site_renderer" });
  }

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
    actor_playbooks: siteData.actor_playbooks || [],
    actor_tools: siteData.actor_tools || [],
    actor_topology: siteData.actor_topology || null,
    actor_policies: siteData.actor_policies || [],
    actor_metrics: siteData.actor_metrics || [],
    actor_supervision: siteData.actor_supervision || [],
    actor_queues: siteData.actor_queues || [],
    chat_personas: siteData.chat_personas || [],
    chat_modes: siteData.chat_modes || [],
    chat_playbooks: siteData.chat_playbooks || [],
    chat_tools: siteData.chat_tools || [],
    chat_memory: siteData.chat_memory || [],
    model_stack: siteData.model_stack || [],
    voice_stack: siteData.voice_stack || [],
    moderation_stack: siteData.moderation_stack || [],
    forms: siteData.forms || [],
    auth: siteData.auth || null,
    identity: siteData.identity || null,
    billing: siteData.billing || null,
    subscriptions: siteData.subscriptions || null,
    cms: siteData.cms || null,
    media_library: siteData.media_library || null,
    scene_pipeline: siteData.scene_pipeline || [],
    render_stack: siteData.render_stack || [],
    interaction_modes: siteData.interaction_modes || [],
    device_profiles: siteData.device_profiles || [],
    scene_assets: siteData.scene_assets || [],
    material_library: siteData.material_library || [],
    lighting_rigs: siteData.lighting_rigs || [],
    camera_rigs: siteData.camera_rigs || [],
    animation_stack: siteData.animation_stack || [],
    physics_stack: siteData.physics_stack || [],
    spatial_audio: siteData.spatial_audio || [],
    xr_modes: siteData.xr_modes || [],
    shader_stack: siteData.shader_stack || [],
    streaming_stack: siteData.streaming_stack || [],
    automation: siteData.automation || null,
    webhooks: siteData.webhooks || null,
    api_reference: siteData.api_reference || null,
    developer_portal: siteData.developer_portal || null,
    seo_stack: siteData.seo_stack || null,
    ai_agents: siteData.ai_agents || null,
    knowledge_sources: siteData.knowledge_sources || [],
    memory_stores: siteData.memory_stores || [],
    tool_registry: siteData.tool_registry || [],
    agent_workflows: siteData.agent_workflows || [],
    model_stack: siteData.model_stack || [],
    voice_stack: siteData.voice_stack || [],
    moderation_stack: siteData.moderation_stack || [],
    ui_components: siteData.ui_components || [],
    ui_layouts: siteData.ui_layouts || [],
    ui_tokens: siteData.ui_tokens || [],
    commerce: siteData.commerce || null,
    app_modules: siteData.app_modules || [],
    integrations: siteData.integrations || [],
    realtime_channels: siteData.realtime_channels || [],
    data_collections: siteData.data_collections || [],
    growth: siteData.growth || null,
    experiments: siteData.experiments || null,
    service_catalog: siteData.service_catalog || null,
    success: siteData.success || null,
    notifications: siteData.notifications || null,
    release_notes: siteData.release_notes || null,
    feature_flags: siteData.feature_flags || null,
    incident_response: siteData.incident_response || null,
    crm_pipeline: siteData.crm_pipeline || null,
    status: siteData.status || null,
    roadmap: siteData.roadmap || [],
    support_channels: siteData.support_channels || [],
    support_tickets: siteData.support_tickets || [],
    feedback_loops: siteData.feedback_loops || [],
    survey_programs: siteData.survey_programs || [],
    messaging_stack: siteData.messaging_stack || [],
    payments_stack: siteData.payments_stack || [],
    scheduling_stack: siteData.scheduling_stack || [],
    privacy_requests: siteData.privacy_requests || [],
    legal: siteData.legal || [],
    security: siteData.security || null,
    community: siteData.community || null,
    events: siteData.events || null,
    newsletter: siteData.newsletter || null,
    compliance: siteData.compliance || null,
    data_governance: siteData.data_governance || [],
    backup_plan: siteData.backup_plan || [],
    observability: siteData.observability || null,
    infrastructure: siteData.infrastructure || null,
    edge_runtime: siteData.edge_runtime || [],
    worker_runtime: siteData.worker_runtime || [],
    api_gateway: siteData.api_gateway || [],
    rate_limits: siteData.rate_limits || [],
    cache_stack: siteData.cache_stack || [],
    search_stack: siteData.search_stack || [],
    storage_stack: siteData.storage_stack || [],
    session_store: siteData.session_store || [],
    runtime_hosts: siteData.runtime_hosts || [],
    deployment_targets: siteData.deployment_targets || [],
    runtime_hosts: siteData.runtime_hosts || [],
    deployment_targets: siteData.deployment_targets || [],
    localization: siteData.localization || null,
    accessibility: siteData.accessibility || null,
    performance: siteData.performance || null,
    team_members: siteData.team_members || [],
    partners: siteData.partners || [],
    press_kit: siteData.press_kit || null,
    careers: siteData.careers || null,
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
      page_count: (entry.pages || []).length + 1,
      asset_count: (entry.assets || []).length,
      files: {
        html: path.basename(entry.html_path),
        manifest: path.basename(entry.manifest_path),
        actor_server: path.basename(entry.actor_server_path),
        site_data: path.basename(entry.site_data_path),
        system_contract: path.basename(entry.system_contract_path),
        ui_schema: path.basename(entry.ui_schema_path),
        sitemap: path.basename(entry.sitemap_path),
        robots: path.basename(entry.robots_path),
        feed: path.basename(entry.feed_path),
        social_card: path.basename(entry.social_card_path)
      },
      pages: (entry.pages || []).map((page) => ({
        route: page.route,
        file: path.relative(entry.output_dir, page.output_path).replaceAll("\\", "/")
      })),
      assets: (entry.assets || []).map((asset) => ({
        route: asset.route,
        file: path.relative(entry.output_dir, asset.output_path).replaceAll("\\", "/"),
        content_type: asset.content_type
      }))
    }))
  };
}

export function buildMatrix(appManifestPath) {
  const context = loadAppConfig(appManifestPath);
  const clientBundleEnabled = Boolean(getClientBundlePaths(context));
  const experienceIds = context.app.build.experiences || Object.keys(context.experiences);
  const experiences = experienceIds.map((id) => buildExperience(appManifestPath, id));
  const experienceArtifacts = experiences.reduce(
    (total, entry) => total + 9 + (entry.pages || []).length + (entry.assets || []).length,
    0
  );
  return {
    default_experience: context.app.default_experience,
    output_root: context.app.output_root,
    experience_count: experiences.length,
    artifact_count: experienceArtifacts + 2 + (clientBundleEnabled ? 2 : 0),
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
    for (const page of entry.pages || []) {
      writeText(page.output_path, page.html);
    }
    for (const asset of entry.assets || []) {
      if (asset.bytes) {
        writeBinary(asset.output_path, asset.bytes);
      } else {
        writeText(asset.output_path, asset.text || "");
      }
    }
  }
  const experienceArtifacts = built.reduce(
    (total, entry) => total + 9 + (entry.pages || []).length + (entry.assets || []).length,
    0
  );
  const summary = {
    default_experience: context.app.default_experience,
    output_root: context.app.output_root,
    experience_count: built.length,
    artifact_count: experienceArtifacts + 2 + (getClientBundlePaths(context) ? 2 : 0),
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
  if (ext === ".svg") return "image/svg+xml; charset=utf-8";
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
  const pagesByRoute = new Map((bundle.pages || []).map((page) => [page.route, page]));
  const assetsByRoute = new Map((bundle.assets || []).map((asset) => [asset.route, asset]));
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
    if (request.method === "GET" && !pathname.endsWith("/") && pagesByRoute.has(`${pathname}/`)) {
      response.writeHead(302, { location: `${pathname}/` });
      response.end();
      return;
    }
    if (request.method === "GET" && pagesByRoute.has(pathname)) {
      sendHtml(response, pagesByRoute.get(pathname).html);
      return;
    }
    if (request.method === "GET" && assetsByRoute.has(pathname)) {
      const asset = assetsByRoute.get(pathname);
      if (asset.bytes) {
        response.writeHead(200, { "content-type": asset.content_type || "application/octet-stream" });
        response.end(asset.bytes);
        return;
      }
      sendText(response, 200, asset.text || "", asset.content_type || "text/plain; charset=utf-8");
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
    if (request.method === "GET" && pathname === "/api/3d/pipeline") {
      sendJson(response, 200, bundle.site_data.scene_pipeline || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/3d/render") {
      sendJson(response, 200, bundle.site_data.render_stack || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/3d/interaction") {
      sendJson(response, 200, bundle.site_data.interaction_modes || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/3d/devices") {
      sendJson(response, 200, bundle.site_data.device_profiles || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/3d/assets") {
      sendJson(response, 200, bundle.site_data.scene_assets || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/3d/materials") {
      sendJson(response, 200, bundle.site_data.material_library || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/3d/lighting") {
      sendJson(response, 200, bundle.site_data.lighting_rigs || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/3d/cameras") {
      sendJson(response, 200, bundle.site_data.camera_rigs || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/3d/animation") {
      sendJson(response, 200, bundle.site_data.animation_stack || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/3d/physics") {
      sendJson(response, 200, bundle.site_data.physics_stack || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/3d/audio") {
      sendJson(response, 200, bundle.site_data.spatial_audio || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/3d/xr") {
      sendJson(response, 200, bundle.site_data.xr_modes || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/3d/shaders") {
      sendJson(response, 200, bundle.site_data.shader_stack || []);
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
    if (request.method === "GET" && pathname === "/api/identity") {
      sendJson(response, 200, bundle.site_data.identity || {});
      return;
    }
    if (request.method === "GET" && pathname === "/api/identity/verification") {
      sendJson(response, 200, bundle.site_data.identity_verification || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/risk") {
      sendJson(response, 200, bundle.site_data.fraud_risk || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/consent") {
      sendJson(response, 200, bundle.site_data.consent_center || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/audit") {
      sendJson(response, 200, bundle.site_data.audit_logs || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/data-exports") {
      sendJson(response, 200, bundle.site_data.data_exports || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/marketplace") {
      sendJson(response, 200, bundle.site_data.marketplace_stack || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/syndication") {
      sendJson(response, 200, bundle.site_data.content_syndication || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/billing") {
      sendJson(response, 200, bundle.site_data.billing || {});
      return;
    }
    if (request.method === "GET" && pathname === "/api/subscriptions") {
      sendJson(response, 200, bundle.site_data.subscriptions || {});
      return;
    }
    if (request.method === "GET" && pathname === "/api/cms") {
      sendJson(response, 200, bundle.site_data.cms || {});
      return;
    }
    if (request.method === "GET" && pathname === "/api/media") {
      sendJson(response, 200, bundle.site_data.media_library || {});
      return;
    }
    if (request.method === "GET" && pathname === "/api/automation") {
      sendJson(response, 200, bundle.site_data.automation || {});
      return;
    }
    if (request.method === "GET" && pathname === "/api/webhooks") {
      sendJson(response, 200, bundle.site_data.webhooks || {});
      return;
    }
    if (request.method === "GET" && pathname === "/api/api-reference") {
      sendJson(response, 200, bundle.site_data.api_reference || {});
      return;
    }
    if (request.method === "GET" && pathname === "/api/developer") {
      sendJson(response, 200, bundle.site_data.developer_portal || {});
      return;
    }
    if (request.method === "GET" && pathname === "/api/seo") {
      sendJson(response, 200, bundle.site_data.seo_stack || {});
      return;
    }
    if (request.method === "GET" && pathname === "/api/brand") {
      sendJson(response, 200, bundle.site_data.brand_system || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/social") {
      sendJson(response, 200, bundle.site_data.social_presence || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/content/calendar") {
      sendJson(response, 200, bundle.site_data.content_calendar || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/release/pipeline") {
      sendJson(response, 200, bundle.site_data.release_pipeline || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/qa") {
      sendJson(response, 200, bundle.site_data.qa_program || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/domains") {
      sendJson(response, 200, bundle.site_data.domain_stack || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/trust") {
      sendJson(response, 200, bundle.site_data.trust_center || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/agents") {
      sendJson(response, 200, bundle.site_data.ai_agents || {});
      return;
    }
    if (request.method === "GET" && pathname === "/api/ui-kit") {
      sendJson(response, 200, {
        components: bundle.site_data.ui_components || [],
        layouts: bundle.site_data.ui_layouts || [],
        tokens: bundle.site_data.ui_tokens || []
      });
      return;
    }
    if (request.method === "GET" && pathname === "/api/frontend") {
      sendJson(response, 200, bundle.site_data.frontend_stack || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/ui-runtime") {
      sendJson(response, 200, bundle.site_data.ui_runtime || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/chat/runtime") {
      sendJson(response, 200, bundle.site_data.chat_runtime || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/actors/runtime") {
      sendJson(response, 200, bundle.site_data.actor_runtime || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/chat/playbooks") {
      sendJson(response, 200, bundle.site_data.chat_playbooks || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/chat/tools") {
      sendJson(response, 200, bundle.site_data.chat_tools || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/chat/memory") {
      sendJson(response, 200, bundle.site_data.chat_memory || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/agents/knowledge") {
      sendJson(response, 200, bundle.site_data.knowledge_sources || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/agents/memory") {
      sendJson(response, 200, bundle.site_data.memory_stores || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/agents/tools") {
      sendJson(response, 200, bundle.site_data.tool_registry || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/agents/workflows") {
      sendJson(response, 200, bundle.site_data.agent_workflows || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/actors/policies") {
      sendJson(response, 200, bundle.site_data.actor_policies || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/actors/metrics") {
      sendJson(response, 200, bundle.site_data.actor_metrics || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/actors/supervision") {
      sendJson(response, 200, bundle.site_data.actor_supervision || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/actors/queues") {
      sendJson(response, 200, bundle.site_data.actor_queues || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/actors/jobs") {
      sendJson(response, 200, bundle.site_data.actor_jobs || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/actors/schedules") {
      sendJson(response, 200, bundle.site_data.actor_schedules || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/actors/hosts") {
      sendJson(response, 200, bundle.site_data.actor_hosts || []);
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
    if (request.method === "GET" && pathname === "/api/data-governance") {
      sendJson(response, 200, bundle.site_data.data_governance || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/backups") {
      sendJson(response, 200, bundle.site_data.backup_plan || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/growth") {
      sendJson(response, 200, bundle.site_data.growth || {});
      return;
    }
    if (request.method === "GET" && pathname === "/api/experiments") {
      sendJson(response, 200, bundle.site_data.experiments || {});
      return;
    }
    if (request.method === "GET" && pathname === "/api/services") {
      sendJson(response, 200, bundle.site_data.service_catalog || {});
      return;
    }
    if (request.method === "GET" && pathname === "/api/success") {
      sendJson(response, 200, bundle.site_data.success || {});
      return;
    }
    if (request.method === "GET" && pathname === "/api/notifications") {
      sendJson(response, 200, bundle.site_data.notifications || {});
      return;
    }
    if (request.method === "GET" && pathname === "/api/releases") {
      sendJson(response, 200, bundle.site_data.release_notes || {});
      return;
    }
    if (request.method === "GET" && pathname === "/api/feature-flags") {
      sendJson(response, 200, bundle.site_data.feature_flags || {});
      return;
    }
    if (request.method === "GET" && pathname === "/api/incidents") {
      sendJson(response, 200, bundle.site_data.incident_response || {});
      return;
    }
    if (request.method === "GET" && pathname === "/api/crm") {
      sendJson(response, 200, bundle.site_data.crm_pipeline || {});
      return;
    }
    if (request.method === "GET" && pathname === "/api/integrations") {
      sendJson(response, 200, bundle.site_data.integrations || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/status") {
      sendJson(response, 200, bundle.site_data.status || {});
      return;
    }
    if (request.method === "GET" && pathname === "/api/roadmap") {
      sendJson(response, 200, bundle.site_data.roadmap || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/support") {
      sendJson(response, 200, bundle.site_data.support_channels || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/support/tickets") {
      sendJson(response, 200, bundle.site_data.support_tickets || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/feedback") {
      sendJson(response, 200, bundle.site_data.feedback_loops || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/surveys") {
      sendJson(response, 200, bundle.site_data.survey_programs || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/messaging") {
      sendJson(response, 200, bundle.site_data.messaging_stack || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/payments") {
      sendJson(response, 200, bundle.site_data.payments_stack || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/scheduling") {
      sendJson(response, 200, bundle.site_data.scheduling_stack || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/privacy/requests") {
      sendJson(response, 200, bundle.site_data.privacy_requests || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/legal") {
      sendJson(response, 200, bundle.site_data.legal || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/security") {
      sendJson(response, 200, bundle.site_data.security || {});
      return;
    }
    if (request.method === "GET" && pathname === "/api/community") {
      sendJson(response, 200, bundle.site_data.community || {});
      return;
    }
    if (request.method === "GET" && pathname === "/api/events") {
      sendJson(response, 200, bundle.site_data.events || {});
      return;
    }
    if (request.method === "GET" && pathname === "/api/newsletter") {
      sendJson(response, 200, bundle.site_data.newsletter || {});
      return;
    }
    if (request.method === "GET" && pathname === "/api/compliance") {
      sendJson(response, 200, bundle.site_data.compliance || {});
      return;
    }
    if (request.method === "GET" && pathname === "/api/observability") {
      sendJson(response, 200, bundle.site_data.observability || {});
      return;
    }
    if (request.method === "GET" && pathname === "/api/infrastructure") {
      sendJson(response, 200, bundle.site_data.infrastructure || {});
      return;
    }
    if (request.method === "GET" && pathname === "/api/runtime/edge") {
      sendJson(response, 200, bundle.site_data.edge_runtime || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/runtime/workers") {
      sendJson(response, 200, bundle.site_data.worker_runtime || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/runtime/gateway") {
      sendJson(response, 200, bundle.site_data.api_gateway || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/runtime/rate-limits") {
      sendJson(response, 200, bundle.site_data.rate_limits || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/runtime/cache") {
      sendJson(response, 200, bundle.site_data.cache_stack || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/runtime/search") {
      sendJson(response, 200, bundle.site_data.search_stack || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/runtime/storage") {
      sendJson(response, 200, bundle.site_data.storage_stack || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/runtime/sessions") {
      sendJson(response, 200, bundle.site_data.session_store || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/runtime/hosts") {
      sendJson(response, 200, bundle.site_data.runtime_hosts || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/runtime/deployments") {
      sendJson(response, 200, bundle.site_data.deployment_targets || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/localization") {
      sendJson(response, 200, bundle.site_data.localization || {});
      return;
    }
    if (request.method === "GET" && pathname === "/api/accessibility") {
      sendJson(response, 200, bundle.site_data.accessibility || {});
      return;
    }
    if (request.method === "GET" && pathname === "/api/performance") {
      sendJson(response, 200, bundle.site_data.performance || {});
      return;
    }
    if (request.method === "GET" && pathname === "/api/enablement") {
      sendJson(response, 200, bundle.site_data.enablement_programs || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/onboarding") {
      sendJson(response, 200, bundle.site_data.onboarding_flows || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/data-retention") {
      sendJson(response, 200, bundle.site_data.data_retention || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/reliability") {
      sendJson(response, 200, bundle.site_data.reliability_slos || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/incidents/history") {
      sendJson(response, 200, bundle.site_data.incident_history || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/team") {
      sendJson(response, 200, bundle.site_data.team_members || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/partners") {
      sendJson(response, 200, bundle.site_data.partners || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/press") {
      sendJson(response, 200, bundle.site_data.press_kit || {});
      return;
    }
    if (request.method === "GET" && pathname === "/api/careers") {
      sendJson(response, 200, bundle.site_data.careers || {});
      return;
    }
    if (request.method === "GET" && pathname === "/api/realtime") {
      sendJson(response, 200, bundle.site_data.realtime_channels || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/actors/topology") {
      sendJson(response, 200, bundle.site_data.actor_topology || {});
      return;
    }
    if (request.method === "GET" && pathname === "/api/actors") {
      sendJson(response, 200, plan);
      return;
    }
    if (request.method === "GET" && pathname === "/api/chat/models") {
      sendJson(response, 200, bundle.site_data.model_stack || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/voice") {
      sendJson(response, 200, bundle.site_data.voice_stack || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/moderation") {
      sendJson(response, 200, bundle.site_data.moderation_stack || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/streaming") {
      sendJson(response, 200, bundle.site_data.streaming_stack || []);
      return;
    }
    if (request.method === "GET" && pathname === "/api/chat") {
      const prompt = requestUrl.searchParams.get("prompt");
      const persona = requestUrl.searchParams.get("persona");
      const mode = requestUrl.searchParams.get("mode");
      if (!prompt) {
        sendJson(response, 200, chatSeed);
        return;
      }
      sendJson(response, 200, { reply: buildChatReply(bundle, plan, prompt, { persona, mode }) });
      return;
    }
    if (request.method === "POST" && pathname === "/api/chat") {
      const payload = await parseRequestBody(request);
      const prompt = payload.prompt || payload.message || payload.text;
      const persona = payload.persona || null;
      const mode = payload.mode || null;
      if (!prompt) {
        sendJson(response, 200, { reply: "missing prompt" });
        return;
      }
      sendJson(response, 200, { reply: buildChatReply(bundle, plan, String(prompt), { persona, mode }) });
      return;
    }
    if (request.method === "GET" && pathname === "/api/chat/stream") {
      response.writeHead(200, {
        "content-type": "text/event-stream; charset=utf-8",
        "cache-control": "no-cache",
        connection: "keep-alive"
      });
      const prompt = (requestUrl.searchParams.get("prompt") || "").trim();
      const persona = requestUrl.searchParams.get("persona");
      const mode = requestUrl.searchParams.get("mode");
      response.write(`event: ready\n`);
      response.write(`data: ${JSON.stringify({ experience: bundle.id, actors: plan.actors.length, routes: plan.routes.length, persona, mode })}\n\n`);
      if (!prompt) {
        response.write(`event: seed\n`);
        response.write(`data: ${JSON.stringify(chatSeed)}\n\n`);
        response.write(`event: done\n`);
        response.write(`data: ok\n\n`);
        response.end();
        return;
      }
      const reply = buildChatReply(bundle, plan, prompt, { persona, mode });
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
    if (request.method === "GET" && pathname === "/api/blog/posts") {
      sendJson(response, 200, { ok: true, posts: bundle.site_data.blog_posts || [] });
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
