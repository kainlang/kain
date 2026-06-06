/**
 * Kain Lang Tools — Compile, check, run, test, amalgamate, GPU artifacts
 *
 * Single router tool for all Kain language operations. Supports --json
 * structured output for agent-friendly diagnostics.
 *
 * Tool: kain_lang
 *   action: check          — Typecheck a .kn file or directory
 *   action: build          — Compile to a target backend
 *   action: run            — Compile and execute
 *   action: test           — Run Kain test fixtures
 *   action: amalgamate     — Combine workspace into single file
 *   action: gpu_artifacts  — Compile shaders to SPIR-V/PTX
 */

import type { ExtensionAPI } from "@earendil-works/pi-coding-agent";
import { Type } from "typebox";
import { existsSync } from "node:fs";
import { join, resolve } from "node:path";
import { spawnSync } from "node:child_process";

// ===========================================================================
// Kain CLI runner
// ===========================================================================

interface KainResult {
  stdout: string;
  stderr: string;
  code: number;
  /** The exact command + args that were executed */
  command: string;
  cwd: string;
  /** If the binary itself couldn't be found */
  binaryNotFound: boolean;
}

function runKain(args: string[], cwd?: string): KainResult {
  const cwdPath = cwd ?? process.cwd();
  const result = spawnSync("kain", args, {
    cwd: cwdPath,
    encoding: "utf-8",
    timeout: 300_000,
    shell: true,
  });

  const binaryNotFound =
    result.error !== undefined &&
    (result.error as any)?.code === "ENOENT";

  return {
    stdout: (result.stdout ?? "").trim(),
    stderr: (result.stderr ?? "").trim(),
    code: result.status ?? (binaryNotFound ? -1 : -1),
    command: `kain ${args.join(" ")}`,
    cwd: cwdPath,
    binaryNotFound,
  };
}

function findKainProject(dir: string): string | null {
  for (const candidate of [dir, resolve(dir, "..")]) {
    if (existsSync(join(candidate, "build.kn"))) return candidate;
    if (existsSync(join(candidate, "KAIN.toml"))) return candidate;
  }
  return null;
}

// ===========================================================================
// Output formatting
// ===========================================================================

function formatSuccess(result: KainResult): string {
  const lines = [`## ✅ \`${result.command}\` succeeded\n`];
  if (result.stdout) lines.push(result.stdout.slice(0, 4000));
  if (result.stderr) lines.push(`\n### stderr\n\`\`\`\n${result.stderr.slice(0, 1000)}\n\`\`\``);
  return lines.join("\n");
}

function formatFailure(result: KainResult): string {
  if (result.binaryNotFound) {
    return (
      `## ❌ \`kain\` binary not found\n\n` +
      `The \`kain\` command is not on PATH or not installed. ` +
      `Build it with \`kain_bazel build(target:'//:kain')\` first.`
    );
  }

  const lines = [`## ❌ \`${result.command}\` failed (exit ${result.code})\n`];

  // stderr is where the compiler puts diagnostics
  if (result.stderr) {
    lines.push(result.stderr.slice(0, 8000));
  }

  // stdout sometimes also has useful info
  if (result.stdout && !result.stderr) {
    lines.push(result.stdout.slice(0, 4000));
  }

  // If genuinely empty, note that
  if (!result.stderr && !result.stdout) {
    lines.push(
      "*(No output captured — empty response from kain)*\n\n" +
      `**CWD:** \`${result.cwd}\`\n` +
      `**Command:** \`${result.command}\`\n` +
      "Possible causes: timeout, crash, or the kain binary needs rebuilding."
    );
  } else {
    lines.push(`\n**CWD:** \`${result.cwd}\``);
  }

  return lines.join("\n");
}

// ===========================================================================
// JSON output formatter — parses `kain ... --json` and renders cleanly
// ===========================================================================

function formatJsonDiagnostics(result: KainResult): { text: string; diagnostics: any[] } {
  // Try to parse stdout as JSON array of diagnostics
  const raw = result.stdout || result.stderr;
  let diagnostics: any[] = [];
  let parseError: string | null = null;

  if (raw) {
    // The JSON output could be an object with an "errors" field, or a raw array
    try {
      const parsed = JSON.parse(raw);
      if (Array.isArray(parsed)) {
        diagnostics = parsed;
      } else if (parsed.errors && Array.isArray(parsed.errors)) {
        diagnostics = parsed.errors;
      } else {
        diagnostics = [parsed];
      }
    } catch {
      parseError = raw.slice(0, 2000);
    }
  }

  if (diagnostics.length === 0 && !parseError) {
    return {
      text: result.code === 0
        ? `## ✅ \`${result.command}\` — clean (no diagnostics)\n`
        : `## ❌ \`${result.command}\` failed but no JSON diagnostics were parsed. Check details.errorRaw.`,
      diagnostics: [],
    };
  }

  const errorCount = diagnostics.filter((d: any) => d.level === "error" || !d.level).length;
  const warningCount = diagnostics.filter((d: any) => d.level === "warning" || d.level === "warn").length;

  const lines = [
    `## ${result.code === 0 ? "✅" : "❌"} \`${result.command}\``,
    `**Errors:** ${errorCount} — **Warnings:** ${warningCount}`,
    "",
  ];

  for (const diag of diagnostics.slice(0, 25)) {
    const loc = diag.location || diag.span || diag.range || {};
    const file = loc.file || loc.filename || diag.file || "?";
    const line = loc.line || loc.start_line || diag.line || 0;
    const col = loc.column || loc.start_column || diag.column || 0;
    const code = diag.code || diag.error_code || diag.id || "";
    const level = diag.level === "warning" ? "⚠️" : diag.level === "warn" ? "⚠️" : "❌";
    const msg = diag.message || diag.detail || diag.text || "(no message)";

    lines.push(`${level} \`${code}\` — ${msg}`);
    if (file !== "?") lines.push(`   ${file}:${line}:${col}`);
    // suggestion / help
    if (diag.suggestion || diag.help) {
      lines.push(`   💡 ${diag.suggestion || diag.help}`);
    }
    lines.push("");
  }

  if (diagnostics.length > 25) {
    lines.push(`*... and ${diagnostics.length - 25} more diagnostics*`);
  }

  if (parseError) {
    lines.push("### Raw output (JSON parse failed)", "```", parseError, "```");
  }

  return {
    text: lines.join("\n"),
    diagnostics,
  };
}

// ===========================================================================
// Extension
// ===========================================================================

export default function (pi: ExtensionAPI) {
  pi.registerTool({
    name: "kain_lang",
    label: "Kain Lang",
    description:
      "Compile, check, run, and test Kain source files and projects — " +
      "typecheck workspaces, compile to LLVM/Rust/C++/WASM/SPIR-V/CUDA, " +
      "execute blades, run inline tests, amalgamate projects, and generate " +
      "GPU shader artifacts. Supports --json for structured diagnostics.",
    promptSnippet: "Compile, check, run, and test Kain source files and projects",
    promptGuidelines: [
      "Use kain_lang when you need to compile Kain files to native code, run blades, check syntax/type correctness, or generate GPU shader artifacts.",
      "Set json:true for structured error diagnostics that are easier to act on programmatically.",
    ],
    parameters: Type.Object({
      action: Type.Enum(
        { check: "check", build: "build", run: "run", test: "test", amalgamate: "amalgamate", gpu_artifacts: "gpu_artifacts" },
        {
          description:
            "'check' — typecheck a file/directory | " +
            "'build' — compile to a target (llvm, rust, cpp, wasm, spirv, cuda) | " +
            "'run' — compile & execute | " +
            "'test' — run test fixtures | " +
            "'amalgamate' — merge workspace into single file | " +
            "'gpu_artifacts' — compile GPU shaders",
        },
      ),
      target: Type.Optional(Type.String({ description: "File or directory path. Defaults to nearest build.kn project." })),
      build_target: Type.Optional(Type.Enum({ llvm: "llvm", rust: "rust", cpp: "cpp", wasm: "wasm", spirv: "spirv", cuda: "cuda" }, { description: "Compilation target for 'build' action (default 'llvm')." })),
      output: Type.Optional(Type.String({ description: "Output directory for gpu_artifacts." })),
      json: Type.Optional(Type.Boolean({ description: "Output diagnostics as structured JSON (supported by check and build). Set to true for agent-friendly error details.", default: false })),
    }),
    async execute(_toolCallId: string, params: any, _signal: AbortSignal, _onUpdate: any, _ctx: any) {
      try {
        const target = params.target ?? findKainProject(process.cwd()) ?? process.cwd();
        let result: KainResult;

        const useJson = params.json === true;

        switch (params.action) {
          case "check":
            result = runKain(useJson ? ["check", target, "--json"] : ["check", target]);
            break;

          case "build":
            result = runKain(useJson
              ? ["build", target, "--target", params.build_target ?? "llvm", "--json"]
              : ["build", target, "--target", params.build_target ?? "llvm"]);
            break;

          case "run":
            result = runKain(["run", target]);
            break;

          case "test":
            result = runKain(["test", target]);
            break;

          case "amalgamate":
            result = runKain(["amalgamate", target]);
            break;

          case "gpu_artifacts":
            result = runKain(["gpu-artifacts", target, ...(params.output ? ["--output", params.output] : [])]);
            break;

          default:
            return { content: [{ type: "text", text: `Unknown action '${params.action}'.` }], details: {}, isError: true };
        }

        // Build the response
        if (result.binaryNotFound) {
          return {
            content: [{ type: "text", text: formatFailure(result) }],
            details: { action: params.action, binaryNotFound: true, command: result.command, cwd: result.cwd },
            isError: true,
          };
        }

        // JSON mode: parse and present structured diagnostics
        if (useJson) {
          const formatted = formatJsonDiagnostics(result);
          return {
            content: [{ type: "text", text: formatted.text }],
            details: {
              action: params.action,
              exitCode: result.code,
              command: result.command,
              cwd: result.cwd,
              diagnostics: formatted.diagnostics,
              rawJson: (result.stdout || result.stderr).slice(0, 20000),
            },
          };
        }

        // Normal text mode
        if (result.code !== 0) {
          return {
            content: [{ type: "text", text: formatFailure(result) }],
            details: {
              action: params.action,
              exitCode: result.code,
              command: result.command,
              cwd: result.cwd,
              stdout: result.stdout.slice(0, 10000),
              stderr: result.stderr.slice(0, 10000),
            },
            isError: true,
          };
        }

        return {
          content: [{ type: "text", text: formatSuccess(result) }],
          details: {
            action: params.action,
            exitCode: 0,
            command: result.command,
            cwd: result.cwd,
          },
        };
      } catch (e: any) {
        return {
          content: [{ type: "text", text: `## ❌ \`kain ${params.action}\` threw\n\n${e.message}\n\`\`\`\n${e.stack?.slice(0, 1000)}\n\`\`\`` }],
          details: { action: params.action, error: e.message },
          isError: true,
        };
      }
    },
  });

  pi.on("session_start", async (_event, ctx) => {
    ctx.ui.notify("🔧 Kain Lang tools loaded — --json support, better diagnostics", "info");
  });
}
