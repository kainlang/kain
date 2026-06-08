/**
 * tree-kn — God-mode directory tree explorer for the PI agent system.
 *
 * Kain-native ONLY. No Node.js fallback — we dogfood our own compiler.
 * Routes every action through `kain run src/tree_main.kn --target llvm -- <args>`.
 *
 * Actions:
 *   tree    — Visual recursive tree with box-drawing characters
 *   list    — Flat listing with metadata
 *   summary — Directory/file counts, total size, language breakdown
 *   info    — Single-path metadata (via kain)
 */

import type { ExtensionAPI } from "@earendil-works/pi-coding-agent";
import { Type } from "typebox";
import * as fs from "fs";
import * as path from "path";
import { execSync } from "child_process";

// ---------------------------------------------------------------------------
// Path resolution
// ---------------------------------------------------------------------------

function resolveExtensionPath(relativePath: string): string {
  if (typeof __dirname !== "undefined") {
    return path.resolve(__dirname, relativePath);
  }
  const home = process.env.USERPROFILE || process.env.HOME || "~";
  return path.resolve(home, ".pi", "extensions", "tree-kn", relativePath);
}

function normalizePath(p: string): string {
  return p.replace(/\\/g, "/");
}

// ---------------------------------------------------------------------------
// Kain binary detection
// ---------------------------------------------------------------------------

interface KainAvailableCache {
  checked: boolean;
  available: boolean;
  kainPath: string;
}

let kainCache: KainAvailableCache = { checked: false, available: false, kainPath: "kain" };

function checkKainAvailable(): KainAvailableCache {
  if (kainCache.checked) return kainCache;

  const candidates = [
    path.join(process.env.USERPROFILE || "~", ".kain", "bin", "kain.exe"),
    path.join(process.env.HOME || "~", ".kain", "bin", "kain"),
    "kain",
  ];

  for (const candidate of candidates) {
    try {
      execSync(`"${candidate}" --version`, {
        stdio: "pipe",
        timeout: 5000,
        windowsHide: true,
      });
      kainCache = { checked: true, available: true, kainPath: candidate };
      return kainCache;
    } catch {
      // Try next
    }
  }

  kainCache = { checked: true, available: false, kainPath: "kain" };
  return kainCache;
}

// ---------------------------------------------------------------------------
// Execute via Kain (the ONLY path)
// ---------------------------------------------------------------------------

interface TreeConfig {
  action: string;
  rootPath: string;
  depth: number;
  pattern: string;
  exclude: string[];
  includeHidden: boolean;
  format: string;
  maxEntries: number;
}

function executeTreeKn(config: TreeConfig): { stdout: string; stderr: string } {
  // Path A — use pre-built .exe (fast, no build output leak)
  const exeFile = resolveExtensionPath("tree-kn.exe");
  if (fs.existsSync(exeFile)) {
    const cliArgs: string[] = [config.rootPath];
    if (config.action !== "tree") { cliArgs.push("--action"); cliArgs.push(config.action); }
    if (config.depth > 0)         { cliArgs.push("--depth"); cliArgs.push(String(config.depth)); }
    if (config.pattern)           { cliArgs.push("--pattern"); cliArgs.push(config.pattern); }
    if (config.includeHidden)     { cliArgs.push("--hidden"); }
    if (config.format !== "tree") { cliArgs.push("--format"); cliArgs.push(config.format); }
    if (config.maxEntries > 0)    { cliArgs.push("--max-entries"); cliArgs.push(String(config.maxEntries)); }
    for (const excl of config.exclude) { cliArgs.push("--exclude"); cliArgs.push(excl); }

    const cmd = [exeFile, ...cliArgs].map(a => a.includes(" ") ? `"${a}"` : a).join(" ");
    const stdout = execSync(cmd, {
      stdio: ["pipe", "pipe", "pipe"],
      timeout: 30000,
      windowsHide: true,
      encoding: "utf8",
    });
    return { stdout, stderr: "" };
  }

  // Path B — fallback to kain run if .exe not built yet
  const mainFile = resolveExtensionPath(path.join("src", "tree_main.kn"));
  if (!fs.existsSync(mainFile)) {
    throw new Error(`tree-kn: neither .exe nor source found at ${resolveExtensionPath(".")}`);
  }

  const kain = checkKainAvailable();
  if (!kain.available) {
    throw new Error(
      "tree-kn .exe not built and Kain binary not found. Run kain build in " +
      resolveExtensionPath(".") + " to build the executable."
    );
  }

  const cliArgs: string[] = [config.rootPath];
  if (config.action !== "tree") { cliArgs.push("--action"); cliArgs.push(config.action); }
  if (config.depth > 0)         { cliArgs.push("--depth"); cliArgs.push(String(config.depth)); }
  if (config.pattern)           { cliArgs.push("--pattern"); cliArgs.push(config.pattern); }
  if (config.includeHidden)     { cliArgs.push("--hidden"); }
  if (config.format !== "tree") { cliArgs.push("--format"); cliArgs.push(config.format); }
  if (config.maxEntries > 0)    { cliArgs.push("--max-entries"); cliArgs.push(String(config.maxEntries)); }
  for (const excl of config.exclude) { cliArgs.push("--exclude"); cliArgs.push(excl); }

  const allArgs = [kain.kainPath, "run", mainFile, "--target", "llvm", "--", ...cliArgs];
  const cmd = allArgs.map(a => a.includes(" ") ? `"${a}"` : a).join(" ");

  const stdout = execSync(cmd, {
    cwd: path.dirname(mainFile),
    stdio: ["pipe", "pipe", "pipe"],
    timeout: 30000,
    windowsHide: true,
    encoding: "utf8",
  });
  return { stdout, stderr: "" };
}

// ---------------------------------------------------------------------------
// Config builder
// ---------------------------------------------------------------------------

interface ExecuteParams {
  action?: string;
  path?: string;
  depth?: number;
  pattern?: string;
  exclude?: string[];
  include_hidden?: boolean;
  format?: string;
  max_entries?: number;
}

function buildConfig(params: ExecuteParams): TreeConfig {
  const action = (params.action || "tree").toLowerCase();
  const rootPath = path.resolve(params.path || ".");
  const depth = typeof params.depth === "number" ? params.depth : 0;
  const pattern = params.pattern || "";
  const exclude = params.exclude || [];
  const includeHidden = params.include_hidden === true;
  const format = (params.format || "tree").toLowerCase();
  const maxEntries = typeof params.max_entries === "number" ? params.max_entries : 0;

  const validActions = ["tree", "list", "summary", "info"];
  if (!validActions.includes(action)) {
    throw new Error(`Invalid action "${action}". Must be: ${validActions.join(", ")}`);
  }

  const validFormats = ["tree", "flat", "json"];
  if (!validFormats.includes(format)) {
    throw new Error(`Invalid format "${format}". Must be: ${validFormats.join(", ")}`);
  }

  return { action, rootPath, depth, pattern, exclude, includeHidden, format, maxEntries };
}

// ---------------------------------------------------------------------------
// Output capping
// ---------------------------------------------------------------------------

const MAX_OUTPUT_BYTES = 48_000;

function capOutput(output: string): string {
  if (Buffer.byteLength(output, "utf8") <= MAX_OUTPUT_BYTES) return output;

  const truncated = output.slice(0, MAX_OUTPUT_BYTES);
  const lines = truncated.split("\n");
  lines.pop(); // drop partial line
  lines.push("");
  lines.push(
    `[OUTPUT TRUNCATED — ${(Buffer.byteLength(output, "utf8") / 1024).toFixed(1)} KB total. ` +
    `Reduce depth or set max_entries for smaller output.]`
  );
  return lines.join("\n");
}

// ---------------------------------------------------------------------------
// Tool registration
// ---------------------------------------------------------------------------

interface TreeKnDetails {
  action: string;
  rootPath: string;
  format: string;
  kain: boolean;
  error: boolean;
}

const TreeKnParams = Type.Object({
  action: Type.Optional(
    Type.String({
      description:
        "Action: 'tree' (visual tree, default), 'list' (flat listing), 'summary' (stats only), 'info' (metadata for a single path)",
    }),
  ),
  path: Type.Optional(
    Type.String({
      description: "Directory or file path (default: current working directory)",
    }),
  ),
  depth: Type.Optional(
    Type.Number({
      description: "Max recursion depth (0 = unlimited, default: 0)",
    }),
  ),
  pattern: Type.Optional(
    Type.String({
      description: "Filter filenames by substring match (case-insensitive)",
    }),
  ),
  exclude: Type.Optional(
    Type.Array(Type.String(), {
      description:
        "Patterns to exclude from results (regex or plain substring)",
    }),
  ),
  include_hidden: Type.Optional(
    Type.Boolean({
      description: "Include hidden files and directories (default: false)",
    }),
  ),
  format: Type.Optional(
    Type.String({
      description: "Output format: 'tree' (default), 'flat', or 'json'",
    }),
  ),
  max_entries: Type.Optional(
    Type.Number({
      description: "Maximum entries to return (0 = unlimited, default: 0)",
    }),
  ),
});

export default function treeKnExtension(pi: ExtensionAPI) {
  pi.registerTool({
    name: "tree_kn",
    label: "Tree KN",
    description: `God-mode directory tree explorer — Kain-native only. 
      See the full structure of any directory with file sizes, types, language detection, 
      and recursive tree output. Use this INSTEAD of ls, find, or bash ls.
      Actions: tree (visual tree), list (flat listing), summary (stats only), info (single path metadata).`,
    promptSnippet:
      "Explore directory tree with metadata, language detection, and recursive output (Kain-native)",
    promptGuidelines: [
      "Use tree_kn instead of ls/find/bash-ls for exploring directory structures. It gives richer output with sizes, types, and language detection.",
      "Use tree_kn action:'summary' for quick directory stats without listing every file.",
      "Use tree_kn action:'tree' depth:2 for a high-level overview of a directory.",
      "Use tree_kn action:'info' for metadata about a single file or directory.",
    ],
    parameters: TreeKnParams,

    async execute(_toolCallId, rawParams, _signal, _onUpdate, _ctx) {
      const params = rawParams as ExecuteParams;

      try {
        const config = buildConfig(params);
        const { stdout } = executeTreeKn(config);
        const output = capOutput(stdout);

        return {
          content: [{ type: "text", text: output }],
          details: {
            action: config.action,
            rootPath: normalizePath(config.rootPath),
            format: config.format,
            kain: true,
            error: false,
          } satisfies TreeKnDetails,
        };
      } catch (err: any) {
        const errorMsg = err.stderr
          ? `tree_kn failed: ${err.message}\n\nStderr:\n${err.stderr}`
          : `tree_kn failed: ${err.message}`;

        return {
          content: [{ type: "text", text: errorMsg }],
          details: {
            action: params.action || "tree",
            rootPath: normalizePath(params.path || "."),
            format: params.format || "tree",
            kain: false,
            error: true,
          } satisfies TreeKnDetails,
        };
      }
    },
  });
}
