/**
 * Kain Lang Tools — Compile, check, run, test, amalgamate, GPU artifacts
 *
 * Single router tool for all Kain language operations. Returns raw stdout/stderr
 * so agents can read CLI output like a terminal. Supports --json for structured
 * diagnostics on `check` (on `build`/`run` it falls back to raw output).
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
  command: string;
  cwd: string;
  binaryNotFound: boolean;
  timedOut: boolean;
}

function runKain(args: string[], cwd?: string): KainResult {
  const cwdPath = cwd ?? process.cwd();
  const result = spawnSync("kain", args, {
    cwd: cwdPath,
    encoding: "utf-8",
    timeout: 300_000,
    shell: true,
  });

  const error = result.error as any;
  const binaryNotFound = error?.code === "ENOENT";
  const timedOut = error?.code === "ETIMEDOUT" || error?.code === "TIMEOUT";

  return {
    stdout: (result.stdout ?? "").trim(),
    stderr: (result.stderr ?? "").trim(),
    code: result.status ?? (binaryNotFound ? -127 : timedOut ? -124 : -1),
    command: `kain ${args.join(" ")}`,
    cwd: cwdPath,
    binaryNotFound,
    timedOut,
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
// Single output formatter — raw terminal output, no smart parsing
// ===========================================================================

function formatOutput(result: KainResult): string {
  const icon = result.code === 0 ? "✅" : "❌";
  const lines = [`${icon} \`${result.command}\``];

  // Error conditions
  if (result.binaryNotFound) {
    lines.push("", "kain binary not found on PATH. Build it with:", "", "  kain_bazel build(target:'//:kain')");
    return lines.join("\n");
  }
  if (result.timedOut) {
    lines.push("Command timed out after 300s.");
    return lines.join("\n");
  }

  // Exit code
  if (result.code !== 0) {
    lines.push(`exit ${result.code}`);
  }

  // stdout
  if (result.stdout) {
    lines.push("", "── stdout ──────────────────────", result.stdout.slice(0, 30000));
  }

  // stderr  
  if (result.stderr) {
    lines.push("", "── stderr ──────────────────────", result.stderr.slice(0, 30000));
  }

  // Empty response note
  if (!result.stdout && !result.stderr) {
    lines.push("(empty response — command produced no output)");
  }

  return lines.join("\n");
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
      "GPU shader artifacts.",
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
      json: Type.Optional(Type.Boolean({ description: "Request structured JSON diagnostics (supported by check). When the CLI doesn't support --json, falls back to raw output.", default: false })),
    }),
    async execute(_toolCallId: string, params: any, _signal: AbortSignal, _onUpdate: any, _ctx: any) {
      try {
        const target = params.target ?? findKainProject(process.cwd()) ?? process.cwd();
        let result: KainResult;

        const useJson = params.json === true;

        switch (params.action) {
          case "check":
            // --json is only supported by check
            result = runKain(useJson ? ["check", target, "--json"] : ["check", target]);
            break;
          case "build":
            result = runKain(["build", target, "--target", params.build_target ?? "llvm"]);
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
            result = runKain([
              "gpu-artifacts",
              target,
              ...(params.output ? ["--output", params.output] : []),
            ]);
            break;
          default:
            return { content: [{ type: "text", text: `Unknown action '${params.action}'.` }], details: {}, isError: true };
        }

        // Parse JSON if requested — silently falls back if not parseable
        let parsedJson: any = null;
        if (useJson) {
          const raw = result.stdout || result.stderr;
          if (raw) {
            try {
              parsedJson = JSON.parse(raw);
            } catch {
              /* not JSON — that's fine, show raw */
            }
          }
        }

        const text = formatOutput(result);

        return {
          content: [{ type: "text", text }],
          details: {
            action: params.action,
            exitCode: result.code,
            command: result.command,
            cwd: result.cwd,
            binaryNotFound: result.binaryNotFound,
            timedOut: result.timedOut,
            parsedJson,
            rawStdout: result.stdout.slice(0, 50000),
            rawStderr: result.stderr.slice(0, 50000),
          },
        };
      } catch (e: any) {
        return {
          content: [{ type: "text", text: `Error: ${e.message}` }],
          details: { action: params.action, error: e.message },
          isError: true,
        };
      }
    },
  });

  pi.on("session_start", async (_event, ctx) => {
    ctx.ui.notify("🔧 Kain Lang loaded — raw CLI output + --json support", "info");
  });
}
