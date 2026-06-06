// blades/lsp/pack_vsix_impl.js
// Node.js implementation script for VSIX packaging.
// Called from pack_vsix.kn via the JS bridge's node_require.
//
// Bundles: kain.exe (the Kain runtime/interpreter) + LSP blade source
// into a self-contained VS Code extension.
//
// Returns: { ok: true, vsix: "path/to/kain-lsp-0.1.0.vsix" }
//          or { ok: false, error: "message" }

"use strict";

const fs = require("fs");
const path = require("path");
const child_process = require("child_process");

const EXT_NAME = "kain-lsp";
const EXT_VERSION = "0.1.0";

// ═══════════════════════════════════════════════════════════
// Source file builders
// ═══════════════════════════════════════════════════════════

function buildPackageJson() {
  return JSON.stringify({
    name: "kain-lsp",
    displayName: "Kain Language Server",
    description: "Kain language support for VS Code — diagnostics, completions, hover, go-to-definition, references, formatting, semantic tokens, code actions, code lens. Bundles the Kain runtime + LSP server.",
    version: EXT_VERSION,
    publisher: "kain-lang",
    license: "MIT",
    engines: { vscode: "^1.85.0" },
    categories: ["Programming Languages", "Linters", "Formatters"],
    keywords: ["kain", "lsp", "language-server", "mcp"],
    activationEvents: ["onLanguage:kain"],
    main: "./extension.js",
    contributes: {
      languages: [
        {
          id: "kain",
          extensions: [".kn"],
          aliases: ["Kain", "kain"],
          configuration: "./language-configuration.json",
        },
      ],
      grammars: [
        {
          language: "kain",
          scopeName: "source.kain",
          path: "./syntaxes/kain.tmLanguage.json",
        },
      ],
    },
    scripts: {},
    dependencies: {
      "vscode-languageclient": "^9.0.1",
    },
    devDependencies: {
      "@types/vscode": "^1.85.0",
    },
  }, null, 2);
}

function buildExtensionJs() {
  return `// Kain Language Server — VS Code extension client
// Spawns the bundled Kain interpreter running the LSP blade.
"use strict";

const path = require("path");
const { LanguageClient, TransportKind } = require("vscode-languageclient/node");

/** @type {LanguageClient | null} */
let client = null;

/**
 * Activate: spawn the Kain LSP server via kain.exe run.
 * @param {import("vscode").ExtensionContext} context
 */
function activate(context) {
  // Paths relative to the extension root
  const kainExe = context.asAbsolutePath(path.join("server", "kain.exe"));
  const bladeDir = context.asAbsolutePath(".");
  const lspEntry = "src/main.kn";

  const serverOptions = {
    command: kainExe,
    args: ["run", lspEntry],
    options: {
      cwd: bladeDir,
      env: {
        ...process.env,
        // Ensure Kain can find its runtime manifest
        KAIN_RUNTIME_MANIFEST_PATH: context.asAbsolutePath(
          path.join("server", "native_core_runtime.toml")
        ),
      },
    },
    transport: TransportKind.stdio,
  };

  const clientOptions = {
    documentSelector: [
      { scheme: "file", language: "kain" },
      { scheme: "untitled", language: "kain" },
    ],
    outputChannelName: "Kain Language Server",
    traceOutputChannelName: "Kain Language Server (Trace)",
  };

  client = new LanguageClient(
    "kain-lsp",
    "Kain Language Server",
    serverOptions,
    clientOptions
  );

  client.start();
}

/**
 * Deactivate: stop the language server.
 * @returns {Thenable<void> | undefined}
 */
function deactivate() {
  if (client) {
    return client.stop();
  }
  return undefined;
}

module.exports = { activate, deactivate };
`;
}

function buildLanguageConfig() {
  return JSON.stringify({
    comments: { lineComment: "//" },
    brackets: [["{", "}"], ["[", "]"], ["(", ")"]],
    autoClosingPairs: [
      { open: "{", close: "}" },
      { open: "[", close: "]" },
      { open: "(", close: ")" },
      { open: '"', close: '"' },
      { open: "'", close: "'" },
    ],
    surroundingPairs: [
      { open: "{", close: "}" },
      { open: "[", close: "]" },
      { open: "(", close: ")" },
      { open: '"', close: '"' },
    ],
    indentationRules: {
      increaseIndentPattern: '^.*\\{[^}"\']*$',
      decreaseIndentPattern: '^\\s*\\}',
    },
    wordPattern: "[A-Za-z_][A-Za-z0-9_]*",
  }, null, 2);
}

function buildTextmateGrammar() {
  return JSON.stringify({
    $schema: "https://raw.githubusercontent.com/martinring/tmlanguage/master/tmlanguage.json",
    name: "Kain",
    scopeName: "source.kain",
    fileTypes: ["kn"],
    patterns: [
      { include: "#comments" },
      { include: "#strings" },
      { include: "#numbers" },
      { include: "#keywords" },
      { include: "#types" },
      { include: "#operators" },
      { include: "#functions" },
      { include: "#attributes" },
      { include: "#constants" },
    ],
    repository: {
      comments: {
        patterns: [
          { name: "comment.line.double-slash.kain", match: "//.*$" },
        ],
      },
      strings: {
        patterns: [
          {
            name: "string.quoted.double.kain",
            begin: '"',
            end: '"',
            patterns: [
              { name: "constant.character.escape.kain", match: "\\\\." },
            ],
          },
        ],
      },
      numbers: {
        patterns: [
          { name: "constant.numeric.hex.kain",    match: "\\b0[xX][0-9a-fA-F_]+\\b" },
          { name: "constant.numeric.float.kain",  match: "\\b[0-9][0-9_]*\\.[0-9][0-9_]*([eE][+-]?[0-9]+)?\\b" },
          { name: "constant.numeric.integer.kain", match: "\\b[0-9][0-9_]*\\b" },
        ],
      },
      keywords: {
        patterns: [
          {
            name: "keyword.control.kain",
            match: "\\b(fn|struct|enum|trait|actor|world|use|pub|let|var|if|elif|else|while|for|loop|match|return|include|as|is|in|spawn|send|ask|on|await|break|continue|with|orchestrate|converge|shatter|teleport|collapse|observe|decay|patch|law|entangle|resonate|pulse|axiom|shader|component|build|test|bench|prove|spec|impl|self|Self|dyn|move|ref|box|unsafe)\\b",
          },
        ],
      },
      types: {
        patterns: [
          {
            name: "entity.name.type.kain",
            match: "\\b(Int|Float|Bool|String|Any|Unit|JsonValue|JsonObject|JsonArray|Array|Map|Set|Option|Result|Location|Range|Span|Document|Workspace|Diagnostic|SemanticToken|Symbol|Completion|File|CompileTarget|BuildGraph|BladePackage|Void)\\b",
          },
        ],
      },
      operators: {
        patterns: [
          { name: "keyword.operator.kain", match: "(->|=>|::|[!=]==|[<>]=?|\\\\+|\\\\-|\\\\*|/|%|&&|\\|\\||!)" },
        ],
      },
      functions: {
        patterns: [
          { name: "entity.name.function.kain", match: "\\b([a-z_][A-Za-z0-9_]*)\\s*(?=\\()" },
        ],
      },
      attributes: {
        patterns: [
          { name: "entity.name.tag.kain", match: "@[A-Za-z_][A-Za-z0-9_]*" },
        ],
      },
      constants: {
        patterns: [
          { name: "constant.language.boolean.kain", match: "\\b(true|false)\\b" },
          { name: "constant.language.null.kain", match: "\\bnull\\b" },
        ],
      },
    },
  }, null, 2);
}

function buildVscodeignore() {
  return `.vscode
.vscode-test
.git
*.vsix
.DS_Store
Thumbs.db
node_modules/.cache
node_modules/.package-lock.json
# Exclude generated/cache dirs from the blade
.kain
generated
*.ll
*.obj
*.json
!package.json
!language-configuration.json
!**/kain.tmLanguage.json
`;
}

// ═══════════════════════════════════════════════════════════
// File copy helpers
// ═══════════════════════════════════════════════════════════

function copyDirRecursive(src, dest) {
  if (!fs.existsSync(dest)) {
    fs.mkdirSync(dest, { recursive: true });
  }
  const entries = fs.readdirSync(src, { withFileTypes: true });
  for (const entry of entries) {
    const srcPath = path.join(src, entry.name);
    const destPath = path.join(dest, entry.name);
    if (entry.isDirectory()) {
      // Skip cache/build dirs
      if (entry.name === ".kain" || entry.name === "generated" || entry.name === "node_modules") {
        continue;
      }
      copyDirRecursive(srcPath, destPath);
    } else {
      // Skip large artifacts
      if (entry.name.endsWith(".ll") || entry.name.endsWith(".obj") ||
          entry.name.endsWith(".vsix") || entry.name.endsWith(".exe")) {
        continue;
      }
      fs.copyFileSync(srcPath, destPath);
    }
  }
}

// ═══════════════════════════════════════════════════════════
// Main pack function
// ═══════════════════════════════════════════════════════════

/**
 * @param {string[]} args - [blade_root]
 * @returns {{ ok: boolean, vsix?: string, error?: string }}
 */
function pack(args) {
  const bladeRoot = (args && args.length > 0) ? String(args[0]) : ".";
  const bladeRootAbs = path.resolve(bladeRoot);

  console.error("[vsix-pack] blade root: " + bladeRootAbs);

  // ── Resolve paths ────────────────────────────────────────
  const extRoot = path.join(bladeRootAbs, "vscode-extension");
  const extServerDir = path.join(extRoot, "server");
  const extSyntaxesDir = path.join(extRoot, "syntaxes");
  const vsixName = path.join(bladeRootAbs, `kain-lsp-${EXT_VERSION}.vsix`);

  // ── Find kain.exe ────────────────────────────────────────
  const kainExePath = findKainExe(bladeRootAbs);
  if (!kainExePath) {
    return { ok: false, error: "kain.exe not found. Expected at X:\\.kain\\bin\\kain.exe or on PATH" };
  }
  console.error("[vsix-pack] kain.exe: " + kainExePath);

  // ── Find runtime manifest ────────────────────────────────
  const runtimeManifest = findRuntimeManifest(bladeRootAbs);
  console.error("[vsix-pack] runtime manifest: " + (runtimeManifest || "(not found, will skip)"));

  // ── Clean previous extension dir ─────────────────────────
  if (fs.existsSync(extRoot)) {
    console.error("[vsix-pack] removing previous extension dir...");
    fs.rmSync(extRoot, { recursive: true, force: true });
  }

  // ── Create directory structure ───────────────────────────
  console.error("[vsix-pack] creating extension directory tree...");
  fs.mkdirSync(extRoot, { recursive: true });
  fs.mkdirSync(extServerDir, { recursive: true });
  fs.mkdirSync(extSyntaxesDir, { recursive: true });

  // ── Write extension files ────────────────────────────────
  console.error("[vsix-pack] writing package.json...");
  fs.writeFileSync(path.join(extRoot, "package.json"), buildPackageJson(), "utf8");

  console.error("[vsix-pack] writing extension.js...");
  fs.writeFileSync(path.join(extRoot, "extension.js"), buildExtensionJs(), "utf8");

  console.error("[vsix-pack] writing language-configuration.json...");
  fs.writeFileSync(path.join(extRoot, "language-configuration.json"), buildLanguageConfig(), "utf8");

  console.error("[vsix-pack] writing syntaxes/kain.tmLanguage.json...");
  fs.writeFileSync(path.join(extSyntaxesDir, "kain.tmLanguage.json"), buildTextmateGrammar(), "utf8");

  console.error("[vsix-pack] writing .vscodeignore...");
  fs.writeFileSync(path.join(extRoot, ".vscodeignore"), buildVscodeignore(), "utf8");

  // ── Copy kain.exe ────────────────────────────────────────
  console.error("[vsix-pack] copying kain.exe into extension/server/...");
  const kainDest = path.join(extServerDir, "kain.exe");
  fs.copyFileSync(kainExePath, kainDest);

  // ── Copy runtime manifest if found ───────────────────────
  if (runtimeManifest && fs.existsSync(runtimeManifest)) {
    const manifestDest = path.join(extServerDir, "native_core_runtime.toml");
    fs.copyFileSync(runtimeManifest, manifestDest);
    console.error("[vsix-pack] copied runtime manifest");
  }

  // ── Copy LSP blade source (src/ + build.kn) ──────────────
  console.error("[vsix-pack] copying blade source files...");
  // Copy src/ directory
  const srcDir = path.join(bladeRootAbs, "src");
  if (fs.existsSync(srcDir)) {
    copyDirRecursive(srcDir, path.join(extRoot, "src"));
  } else {
    return { ok: false, error: "src/ directory not found in blade root" };
  }
  // Copy build.kn
  const buildKn = path.join(bladeRootAbs, "build.kn");
  if (fs.existsSync(buildKn)) {
    fs.copyFileSync(buildKn, path.join(extRoot, "build.kn"));
  }

  // ── npm install (production deps) ────────────────────────
  console.error("[vsix-pack] running npm install...");
  try {
    const npmOut = child_process.execSync(
      "npm install --production --no-audit --no-fund --loglevel=error",
      { cwd: extRoot, encoding: "utf8", stdio: "pipe" }
    );
    console.error("[vsix-pack] npm: " + npmOut.trim());
  } catch (e) {
    console.error("[vsix-pack] npm WARNING: " + (e.stderr || e.message));
  }

  // ── vsce package ─────────────────────────────────────────
  console.error("[vsix-pack] packaging VSIX (this may take a moment)...");
  try {
    const vsceOut = child_process.execSync(
      `npx --yes @vscode/vsce@latest package --out "${vsixName}"`,
      { cwd: extRoot, encoding: "utf8", stdio: "pipe" }
    );
    console.error("[vsix-pack] vsce: " + vsceOut.trim());
  } catch (e) {
    console.error("[vsix-pack] vsce failed: " + (e.stderr || e.message));
    console.error("[vsix-pack] falling back to manual ZIP...");
    const zipped = manualVsix(extRoot, vsixName);
    if (!zipped) {
      return { ok: false, error: "vsce and manual ZIP both failed" };
    }
  }

  // ── Verify output ────────────────────────────────────────
  if (!fs.existsSync(vsixName)) {
    return { ok: false, error: "VSIX was not created at " + vsixName };
  }

  const stats = fs.statSync(vsixName);
  const sizeKB = (stats.size / 1024).toFixed(1);
  const sizeMB = (stats.size / (1024 * 1024)).toFixed(1);
  console.error(`[vsix-pack] SUCCESS: ${vsixName} (${sizeKB} KB / ${sizeMB} MB)`);
  console.error(`[vsix-pack] Install: code --install-extension "${vsixName}"`);

  return { ok: true, vsix: vsixName, size_bytes: stats.size };
}

// ═══════════════════════════════════════════════════════════
// Discovery helpers
// ═══════════════════════════════════════════════════════════

function findKainExe(bladeRoot) {
  // Check common locations
  const candidates = [
    path.join(process.env.LOCALAPPDATA || "", "kain", "bin", "kain.exe"),
    path.join(process.env.USERPROFILE || "", ".kain", "bin", "kain.exe"),
    "X:\\.kain\\bin\\kain.exe",
    "C:\\Program Files\\kain\\bin\\kain.exe",
  ];

  for (const candidate of candidates) {
    if (fs.existsSync(candidate)) {
      return candidate;
    }
  }

  // Try PATH
  try {
    const which = child_process.execSync('where kain 2>nul', { encoding: "utf8", stdio: "pipe" }).trim();
    if (which && fs.existsSync(which.split("\n")[0].trim())) {
      return which.split("\n")[0].trim();
    }
  } catch (e) {
    // Not on PATH
  }

  return null;
}

function findRuntimeManifest(bladeRoot) {
  const candidates = [
    path.join(bladeRoot, "..", "..", "runtime", "native_core_runtime.toml"),
    path.join(bladeRoot, "..", "runtime", "native_core_runtime.toml"),
    "X:\\runtime\\native_core_runtime.toml",
  ];

  for (const candidate of candidates) {
    if (fs.existsSync(candidate)) {
      return candidate;
    }
  }
  return null;
}

// ═══════════════════════════════════════════════════════════
// Manual VSIX ZIP fallback
// ═══════════════════════════════════════════════════════════

function manualVsix(extRoot, vsixPath) {
  try {
    if (process.platform === "win32") {
      const psCmd = `Compress-Archive -Path "${extRoot}\\*" -DestinationPath "${vsixPath}" -Force`;
      child_process.execSync(`powershell -NoProfile -Command "${psCmd}"`, {
        encoding: "utf8", stdio: "pipe",
      });
    } else {
      child_process.execSync(`cd "${extRoot}" && zip -r "${vsixPath}" .`, {
        encoding: "utf8", stdio: "pipe",
      });
    }
    console.error("[vsix-pack] manual ZIP fallback succeeded");
    return true;
  } catch (e) {
    console.error("[vsix-pack] manual ZIP also failed: " + (e.stderr || e.message));
    return false;
  }
}

module.exports = { pack };
