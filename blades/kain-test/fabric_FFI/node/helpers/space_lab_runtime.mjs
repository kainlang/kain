import { spawn } from "node:child_process";
import { promises as fs } from "node:fs";
import http from "node:http";
import path from "node:path";
import { fileURLToPath } from "node:url";

import { build as esbuildBuild } from "esbuild";

const runtimeFilePath = fileURLToPath(import.meta.url);
const runtimeDirectory = path.dirname(runtimeFilePath);
const defaultManifestPath = "manifests/app.json";

const contentTypesByExtension = {
  ".css": "text/css; charset=utf-8",
  ".hybrid": "application/json; charset=utf-8",
  ".html": "text/html; charset=utf-8",
  ".js": "text/javascript; charset=utf-8",
  ".json": "application/json; charset=utf-8",
  ".mjs": "text/javascript; charset=utf-8",
  ".ts": "text/plain; charset=utf-8",
  ".wasm": "application/wasm",
};

function resolveProjectRoot(projectRoot = ".") {
  return path.resolve(projectRoot);
}

function resolveInsideProject(projectRoot, relativePath) {
  return path.resolve(resolveProjectRoot(projectRoot), relativePath);
}

function toPosixPath(filePath) {
  return filePath.split(path.sep).join("/");
}

async function readJsonFile(filePath) {
  const source = await fs.readFile(filePath, "utf8");
  return JSON.parse(source);
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
    scene_name: model.scene.name,
    server_url: `http://${model.server.host}:${model.server.port}`,
    output_root: model.output_root_absolute,
    html_entry: model.html_entry_absolute,
    client_bundle: model.client_bundle_absolute,
    hybrid_descriptor: model.hybrid_pipeline?.descriptor_absolute ?? null,
    hybrid_js_bundle: model.hybrid_pipeline?.public_js_path ?? null,
    hybrid_wasm_bundle: model.hybrid_pipeline?.public_wasm_path ?? null,
    wasm_bundle: model.wasm_pipeline?.public_path ?? null,
    hybrid_export_count: model.hybrid_pipeline?.wasm_exports?.length ?? 0,
    viewport_mode_count: model.viewport_profiles?.modes?.length ?? 0,
    sculpt_tool_count: model.sculpt_suite?.tools?.length ?? 0,
    star_count: model.scene.environment.star_count,
    beacon_count: model.scene.environment.beacon_count,
  };
}

async function loadRegistries(projectRoot, appConfig) {
  const registryEntries = Object.entries(appConfig.registries ?? {});
  const loadedEntries = await Promise.all(
    registryEntries.map(async ([registryName, registryPath]) => {
      const absolutePath = resolveInsideProject(projectRoot, registryPath);
      return [registryName, await readJsonFile(absolutePath)];
    }),
  );
  return Object.fromEntries(loadedEntries);
}

function createRuntimeModel(projectRoot, appConfig, registries, manifestPath) {
  const outputRootAbsolute = resolveInsideProject(projectRoot, appConfig.output_root);
  const clientBundleDirectory = path.join(outputRootAbsolute, appConfig.client_bundle.out_dir);
  const clientBundleAbsolute = path.join(clientBundleDirectory, appConfig.client_bundle.out_file);
  const clientStyleAbsolute = path.join(clientBundleDirectory, "three-space.css");
  const htmlEntryAbsolute = path.join(outputRootAbsolute, "index.html");
  const hybridPipeline = registries.hybrid_pipeline ?? null;
  const hybridOutputDirectoryAbsolute = hybridPipeline
    ? resolveInsideProject(projectRoot, hybridPipeline.output_dir)
    : null;
  const hybridDescriptorAbsolute = hybridPipeline
    ? path.join(hybridOutputDirectoryAbsolute, hybridPipeline.descriptor_file)
    : null;
  const hybridJsAbsolute = hybridPipeline
    ? path.join(hybridOutputDirectoryAbsolute, hybridPipeline.js_file)
    : null;
  const hybridTsAbsolute = hybridPipeline
    ? path.join(hybridOutputDirectoryAbsolute, hybridPipeline.ts_file)
    : null;
  const hybridWasmAbsolute = hybridPipeline
    ? path.join(hybridOutputDirectoryAbsolute, hybridPipeline.wasm_file)
    : null;
  const wasmPipeline = registries.wasm_pipeline ?? null;
  const wasmOutputDirectoryAbsolute = wasmPipeline
    ? resolveInsideProject(projectRoot, wasmPipeline.output_dir)
    : null;
  const wasmBundleAbsolute = wasmPipeline
    ? path.join(wasmOutputDirectoryAbsolute, wasmPipeline.output_file)
    : null;

  return {
    ...appConfig,
    manifest_path: manifestPath,
    project_root: resolveProjectRoot(projectRoot),
    output_root_absolute: outputRootAbsolute,
    client_bundle_absolute: clientBundleAbsolute,
    client_style_absolute: clientStyleAbsolute,
    html_entry_absolute: htmlEntryAbsolute,
    scene: registries.scene,
    sculpt_suite: registries.sculpt_suite,
    viewport_profiles: registries.viewport_profiles,
    hybrid_pipeline: hybridPipeline
      ? {
          ...hybridPipeline,
          output_dir_absolute: hybridOutputDirectoryAbsolute,
          descriptor_absolute: hybridDescriptorAbsolute,
          js_absolute: hybridJsAbsolute,
          ts_absolute: hybridTsAbsolute,
          wasm_absolute: hybridWasmAbsolute,
          wasm_exports: [],
        }
      : null,
    wasm_pipeline: wasmPipeline
      ? {
          ...wasmPipeline,
          output_dir_absolute: wasmOutputDirectoryAbsolute,
          output_file_absolute: wasmBundleAbsolute,
        }
      : null,
  };
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
  await ensureDirectory(path.dirname(model.client_bundle_absolute));

  await esbuildBuild({
    absWorkingDir: model.project_root,
    bundle: true,
    entryPoints: [clientEntryAbsolute],
    format: model.client_bundle.format,
    loader: { ".ts": "ts" },
    minify: Boolean(model.client_bundle.minify),
    outfile: model.client_bundle_absolute,
    platform: "browser",
    sourcemap: false,
    target: model.client_bundle.target ?? "es2022",
  });

  const clientStyleSource = path.join(runtimeDirectory, "client", "style.css");
  await copyFileIfPresent(clientStyleSource, model.client_style_absolute);

  return {
    bundle_path: model.client_bundle_absolute,
    style_path: model.client_style_absolute,
  };
}

async function buildHybridBundle(model) {
  if (!model.hybrid_pipeline) {
    return null;
  }

  const pipeline = model.hybrid_pipeline;
  const entryAbsolute = resolveInsideProject(model.project_root, pipeline.entry);

  await ensureDirectory(pipeline.output_dir_absolute);
  await runProcess(
    "cargo",
    [
      "run",
      "-q",
      "-p",
      "cli",
      "--bin",
      "kain",
      "--",
      "build",
      entryAbsolute,
      "-t",
      pipeline.target,
      "-o",
      pipeline.descriptor_absolute,
    ],
    { cwd: model.project_root },
  );

  const descriptor = await readJsonFile(pipeline.descriptor_absolute);
  pipeline.wasm_exports = Array.isArray(descriptor.wasm_exports) ? descriptor.wasm_exports : [];

  return {
    entry_path: entryAbsolute,
    descriptor_path: pipeline.descriptor_absolute,
    js_path: pipeline.js_absolute,
    ts_path: pipeline.ts_absolute,
    wasm_path: pipeline.wasm_absolute,
    public_descriptor_path: pipeline.public_descriptor_path,
    public_js_path: pipeline.public_js_path,
    public_wasm_path: pipeline.public_wasm_path,
    wasm_exports: pipeline.wasm_exports,
  };
}

async function runProcess(command, args, options = {}) {
  const child = spawn(command, args, {
    cwd: options.cwd,
    env: { ...process.env, ...options.env },
    stdio: ["ignore", "pipe", "pipe"],
  });

  let stdout = "";
  let stderr = "";

  child.stdout.on("data", (chunk) => {
    stdout += String(chunk);
  });

  child.stderr.on("data", (chunk) => {
    stderr += String(chunk);
  });

  const exitCode = await new Promise((resolve, reject) => {
    child.on("error", reject);
    child.on("close", resolve);
  });

  if (exitCode !== 0) {
    throw new Error(
      [
        `Command failed: ${command} ${args.join(" ")}`,
        stdout.trim() ? `stdout:\n${stdout.trim()}` : "",
        stderr.trim() ? `stderr:\n${stderr.trim()}` : "",
      ]
        .filter(Boolean)
        .join("\n\n"),
    );
  }

  return { stdout, stderr };
}

async function buildWasmBundle(model) {
  if (!model.wasm_pipeline) {
    return null;
  }

  const pipeline = model.wasm_pipeline;
  const manifestAbsolute = resolveInsideProject(model.project_root, pipeline.manifest_path);
  const artifactProfileDirectory = pipeline.profile === "debug" ? "debug" : "release";
  const cargoArguments = [
    "build",
    "--manifest-path",
    manifestAbsolute,
    "--target",
    pipeline.target,
  ];

  if (artifactProfileDirectory === "release") {
    cargoArguments.push("--release");
  }

  await runProcess("cargo", cargoArguments, { cwd: model.project_root });

  const builtWasmAbsolute = path.join(
    path.dirname(manifestAbsolute),
    "target",
    pipeline.target,
    artifactProfileDirectory,
    `${pipeline.crate_name}.wasm`,
  );

  await ensureDirectory(pipeline.output_dir_absolute);
  await fs.copyFile(builtWasmAbsolute, pipeline.output_file_absolute);

  return {
    crate_name: pipeline.crate_name,
    manifest_path: manifestAbsolute,
    target: pipeline.target,
    artifact_path: pipeline.output_file_absolute,
    public_path: pipeline.public_path,
  };
}

function createHtmlDocument(model) {
  const serializedModel = JSON.stringify(model, null, 2).replace(/</g, "\\u003c");
  const hybridRelativePath = model.hybrid_pipeline
    ? toPosixPath(path.relative(model.output_root_absolute, model.hybrid_pipeline.js_absolute))
    : null;
  const hybridBridgeScript = model.hybrid_pipeline
    ? `
    <script>
      window.__KAIN_HYBRID_BINDINGS__ = {
${(model.hybrid_pipeline.global_binding_names ?? [])
  .map(
    (bindingName) =>
      `        ${JSON.stringify(bindingName)}: typeof ${bindingName} === "function" ? ${bindingName} : null`,
  )
  .join(",\n")}
      };
    </script>`
    : "";
  const clientRelativePath = toPosixPath(
    path.relative(model.output_root_absolute, model.client_bundle_absolute),
  );
  const styleRelativePath = toPosixPath(
    path.relative(model.output_root_absolute, model.client_style_absolute),
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
    ${hybridRelativePath ? `<script src="./${hybridRelativePath}"></script>` : ""}
    ${hybridBridgeScript}
    <script>
      window.__KAIN_THREE_SPACE_MODEL__ = ${serializedModel};
    </script>
    <script type="module" src="./${clientRelativePath}"></script>
  </body>
</html>
`;
}

async function emitRuntimeOutputs(model, clientArtifacts) {
  await ensureDirectory(model.output_root_absolute);

  const appModelPath = path.join(model.output_root_absolute, "app-model.json");
  const buildReportPath = path.join(model.output_root_absolute, "build-report.json");
  const summary = buildSummary(model);

  await fs.writeFile(appModelPath, JSON.stringify(model, null, 2) + "\n", "utf8");
  await fs.writeFile(buildReportPath, JSON.stringify({ summary, clientArtifacts }, null, 2) + "\n", "utf8");
  await fs.writeFile(model.html_entry_absolute, createHtmlDocument(model), "utf8");

  return summary;
}

export async function buildApp(projectRoot = ".", manifestPath = defaultManifestPath) {
  const model = await loadAppConfig(projectRoot, manifestPath);
  const hybridArtifacts = await buildHybridBundle(model);
  const wasmArtifacts = await buildWasmBundle(model);
  const clientArtifacts = await bundleClient(model);
  return emitRuntimeOutputs(model, {
    client: clientArtifacts,
    hybrid: hybridArtifacts,
    wasm: wasmArtifacts,
  });
}

export async function buildWasmOnly(projectRoot = ".", manifestPath = defaultManifestPath) {
  const model = await loadAppConfig(projectRoot, manifestPath);
  const wasmArtifacts = await buildWasmBundle(model);
  console.log(JSON.stringify(wasmArtifacts, null, 2));
  return wasmArtifacts;
}

export async function buildHybridOnly(projectRoot = ".", manifestPath = defaultManifestPath) {
  const model = await loadAppConfig(projectRoot, manifestPath);
  const hybridArtifacts = await buildHybridBundle(model);
  console.log(JSON.stringify(hybridArtifacts, null, 2));
  return hybridArtifacts;
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
  const normalizedPathname = new URL(requestPath ?? "/", "http://localhost").pathname;
  const relativeRequestPath = normalizedPathname === "/"
    ? "index.html"
    : normalizedPathname.replace(/^\/+/, "");
  const safeRequestPath = path.normalize(relativeRequestPath).replace(/^(\.\.(\/|\\|$))+/, "");
  const filePath = path.join(outputsRoot, safeRequestPath);
  return {
    body: await fs.readFile(filePath),
    contentType: chooseContentType(filePath),
  };
}

export async function serveApp(projectRoot = ".", manifestPath = defaultManifestPath) {
  const model = await loadAppConfig(projectRoot, manifestPath);
  await buildApp(projectRoot, manifestPath);

  const host = model.server.host ?? "127.0.0.1";
  const port = Number(model.server.port ?? 4192);
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

  await new Promise((resolve) => server.listen(port, host, resolve));
  const address = `http://${host}:${port}`;
  console.log(`${model.name} serving at ${address}`);
  return {
    address,
    host,
    port,
    output_root: outputsRoot,
  };
}

async function runFromCli() {
  const command = process.argv[2] ?? "build";
  const manifestPath = process.argv[3] ?? defaultManifestPath;
  const projectRoot = ".";

  if (command === "build") {
    const summary = await buildApp(projectRoot, manifestPath);
    console.log(JSON.stringify(summary, null, 2));
    return;
  }

  if (command === "build-wasm") {
    await buildWasmOnly(projectRoot, manifestPath);
    return;
  }

  if (command === "build-hybrid") {
    await buildHybridOnly(projectRoot, manifestPath);
    return;
  }

  if (command === "print") {
    await printSummary(projectRoot, manifestPath);
    return;
  }

  if (command === "serve") {
    await serveApp(projectRoot, manifestPath);
    return;
  }

  throw new Error(
    `Unknown command "${command}". Expected build, build-hybrid, build-wasm, print, or serve.`,
  );
}

if (process.argv[1] && path.resolve(process.argv[1]) === runtimeFilePath) {
  runFromCli().catch((error) => {
    console.error(error);
    process.exitCode = 1;
  });
}
