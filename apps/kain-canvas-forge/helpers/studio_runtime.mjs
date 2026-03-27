import { promises as fs } from "node:fs";
import http from "node:http";
import path from "node:path";
import { fileURLToPath } from "node:url";

import { build as esbuildBuild } from "esbuild";

const runtimeFile = fileURLToPath(import.meta.url);
const runtimeDirectory = path.dirname(runtimeFile);
const defaultManifestPath = "manifests/app.json";

const contentTypesByExtension = {
  ".css": "text/css; charset=utf-8",
  ".html": "text/html; charset=utf-8",
  ".js": "text/javascript; charset=utf-8",
  ".json": "application/json; charset=utf-8",
  ".mjs": "text/javascript; charset=utf-8",
  ".png": "image/png",
  ".svg": "image/svg+xml",
};

function resolveProjectRoot(projectRoot = ".") {
  return path.resolve(projectRoot);
}

function resolveInsideProject(projectRoot, relativePath) {
  return path.resolve(resolveProjectRoot(projectRoot), relativePath);
}

function toPosixPath(inputPath) {
  return inputPath.split(path.sep).join("/");
}

async function readJsonFile(filePath) {
  const content = await fs.readFile(filePath, "utf8");
  return JSON.parse(content);
}

async function ensureDirectory(directoryPath) {
  await fs.mkdir(directoryPath, { recursive: true });
}

async function copyFileIfPresent(sourcePath, destinationPath) {
  try {
    await ensureDirectory(path.dirname(destinationPath));
    await fs.copyFile(sourcePath, destinationPath);
    return true;
  } catch (error) {
    if (error && error.code === "ENOENT") {
      return false;
    }
    throw error;
  }
}

function buildSummary(model) {
  return {
    id: model.id,
    name: model.name,
    output_root: model.output_root_absolute,
    client_bundle: model.client_bundle_absolute,
    desktop_entry: model.desktop_entry_absolute,
    html_entry: model.html_entry_absolute,
    workspace_count: model.workspaces.length,
    tool_count: model.tools.length,
    brush_count: model.brushes.length,
    panel_count: model.panels.length,
    scene_count: model.scenes.length,
  };
}

function createRuntimeModel(projectRoot, appConfig, registries, manifestPath) {
  const outputRootAbsolute = resolveInsideProject(projectRoot, appConfig.output_root);
  const clientBundleDirectory = path.join(outputRootAbsolute, appConfig.client_bundle.out_dir);
  const clientBundleAbsolute = path.join(clientBundleDirectory, appConfig.client_bundle.out_file);
  const clientStylesAbsolute = path.join(clientBundleDirectory, "canvas-forge.css");
  const htmlEntryAbsolute = path.join(outputRootAbsolute, "index.html");
  const desktopEntryAbsolute = resolveInsideProject(projectRoot, "desktop/main.mjs");

  return {
    ...appConfig,
    manifest_path: manifestPath,
    project_root: resolveProjectRoot(projectRoot),
    output_root_absolute: outputRootAbsolute,
    client_bundle_absolute: clientBundleAbsolute,
    client_styles_absolute: clientStylesAbsolute,
    html_entry_absolute: htmlEntryAbsolute,
    desktop_entry_absolute: desktopEntryAbsolute,
    workspaces: registries.workspaces ?? [],
    tools: registries.tools ?? [],
    brushes: registries.brushes ?? [],
    panels: registries.panels ?? [],
    scenes: registries.scenes ?? [],
  };
}

async function loadRegistries(projectRoot, appConfig) {
  const entries = Object.entries(appConfig.registries ?? {});
  const loadedEntries = await Promise.all(
    entries.map(async ([registryName, registryPath]) => {
      const absolutePath = resolveInsideProject(projectRoot, registryPath);
      const registryValue = await readJsonFile(absolutePath);
      return [registryName, registryValue];
    }),
  );

  return Object.fromEntries(loadedEntries);
}

export async function loadAppConfig(projectRoot = ".", manifestPath = defaultManifestPath) {
  const absoluteManifestPath = resolveInsideProject(projectRoot, manifestPath);
  const appConfig = await readJsonFile(absoluteManifestPath);
  const registries = await loadRegistries(projectRoot, appConfig);
  return createRuntimeModel(projectRoot, appConfig, registries, manifestPath);
}

async function bundleClient(model) {
  if (!model.client_bundle?.enabled) {
    return null;
  }

  const clientEntryAbsolute = resolveInsideProject(model.project_root, model.client_bundle.entry);
  const clientBundleDirectory = path.dirname(model.client_bundle_absolute);

  await ensureDirectory(clientBundleDirectory);

  await esbuildBuild({
    absWorkingDir: model.project_root,
    bundle: true,
    entryPoints: [clientEntryAbsolute],
    format: model.client_bundle.format,
    jsx: "automatic",
    jsxImportSource: "preact",
    loader: { ".tsx": "tsx", ".ts": "ts" },
    minify: Boolean(model.client_bundle.minify),
    outfile: model.client_bundle_absolute,
    platform: "browser",
    sourcemap: false,
    target: model.client_bundle.target ?? "es2022",
  });

  const clientStyleSource = path.join(runtimeDirectory, "client", "style.css");
  await copyFileIfPresent(clientStyleSource, model.client_styles_absolute);

  return {
    bundle_path: model.client_bundle_absolute,
    style_path: model.client_styles_absolute,
  };
}

function createHtmlDocument(model) {
  const serializedModel = JSON.stringify(model, null, 2).replace(/</g, "\\u003c");
  const clientRelativePath = toPosixPath(
    path.relative(model.output_root_absolute, model.client_bundle_absolute),
  );
  const styleRelativePath = toPosixPath(
    path.relative(model.output_root_absolute, model.client_styles_absolute),
  );

  return `<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <title>${model.name}</title>
    <meta name="description" content="${model.summary}" />
    <link rel="stylesheet" href="./${styleRelativePath}" />
  </head>
  <body>
    <div id="app-root"></div>
    <script>
      window.__KAIN_CANVAS_FORGE_MODEL__ = ${serializedModel};
    </script>
    <script type="module" src="./${clientRelativePath}"></script>
  </body>
</html>
`;
}

async function emitRuntimeOutputs(model, clientArtifacts) {
  await ensureDirectory(model.output_root_absolute);

  const runtimeModelPath = path.join(model.output_root_absolute, "app-model.json");
  const buildReportPath = path.join(model.output_root_absolute, "build-report.json");
  const htmlDocument = createHtmlDocument(model);
  const summary = buildSummary(model);

  await fs.writeFile(runtimeModelPath, JSON.stringify(model, null, 2) + "\n", "utf8");
  await fs.writeFile(buildReportPath, JSON.stringify({ summary, clientArtifacts }, null, 2) + "\n", "utf8");
  await fs.writeFile(model.html_entry_absolute, htmlDocument, "utf8");

  return summary;
}

export async function buildApp(projectRoot = ".", manifestPath = defaultManifestPath) {
  const model = await loadAppConfig(projectRoot, manifestPath);
  const clientArtifacts = await bundleClient(model);
  return emitRuntimeOutputs(model, clientArtifacts);
}

export async function printSummary(projectRoot = ".", manifestPath = defaultManifestPath) {
  const model = await loadAppConfig(projectRoot, manifestPath);
  const summary = buildSummary(model);
  console.log(JSON.stringify(summary, null, 2));
  return summary;
}

function chooseContentType(filePath) {
  return contentTypesByExtension[path.extname(filePath).toLowerCase()] ?? "application/octet-stream";
}

async function readStaticAsset(outputsRoot, requestPath) {
  const relativeRequestPath = requestPath === "/" ? "index.html" : requestPath.replace(/^\/+/, "");
  const safeRequestPath = path.normalize(relativeRequestPath).replace(/^(\.\.(\/|\\|$))+/, "");
  const filePath = path.join(outputsRoot, safeRequestPath);
  const fileBuffer = await fs.readFile(filePath);
  return {
    body: fileBuffer,
    contentType: chooseContentType(filePath),
  };
}

export async function serveApp(projectRoot = ".", manifestPath = defaultManifestPath) {
  const model = await loadAppConfig(projectRoot, manifestPath);
  await buildApp(projectRoot, manifestPath);

  const host = model.server?.host ?? "127.0.0.1";
  const port = Number(model.server?.port ?? 4178);
  const outputsRoot = model.output_root_absolute;

  const server = http.createServer(async (request, response) => {
    try {
      const payload = await readStaticAsset(outputsRoot, request.url ?? "/");
      response.writeHead(200, { "Content-Type": payload.contentType });
      response.end(payload.body);
    } catch (error) {
      const statusCode = error && error.code === "ENOENT" ? 404 : 500;
      response.writeHead(statusCode, { "Content-Type": "text/plain; charset=utf-8" });
      response.end(statusCode === 404 ? "Not found" : `Server error: ${String(error)}`);
    }
  });

  await new Promise((resolve) => {
    server.listen(port, host, resolve);
  });

  const serveSummary = {
    ...buildSummary(model),
    server_url: `http://${host}:${port}`,
  };

  console.log(`Serving ${model.name} at ${serveSummary.server_url}`);
  return serveSummary;
}

async function runCli() {
  const command = process.argv[2] ?? "print";
  const manifestPath = process.argv[3] ?? defaultManifestPath;

  if (command === "build") {
    await buildApp(".", manifestPath);
    return;
  }

  if (command === "bundle-client") {
    const model = await loadAppConfig(".", manifestPath);
    await bundleClient(model);
    return;
  }

  if (command === "print") {
    await printSummary(".", manifestPath);
    return;
  }

  if (command === "serve") {
    await serveApp(".", manifestPath);
    return;
  }

  throw new Error(`Unknown studio runtime command: ${command}`);
}

if (process.argv[1] === runtimeFile) {
  runCli().catch((error) => {
    console.error(error);
    process.exitCode = 1;
  });
}
