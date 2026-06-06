/**
 * Kain Lang Tools — Compile, check, run, test, amalgamate, GPU artifacts
 *
 * Single router tool for all Kain language operations: typechecking,
 * compilation to LLVM/Rust/C++/WASM/SPIR-V/CUDA, running blades,
 * workspace testing, amalgamation, and shader compilation.
 *
 * Tool: kain_lang
 *   action: check          — Typecheck a .kn file or directory
 *   action: build          — Compile to a target backend
 *   action: run            — Compile and execute
 *   action: test           — Run Kain test fixtures
 *   action: amalgamate     — Combine workspace into single file
 *   action: gpu_artifacts  — Compile shaders to SPIR-V/PTX
 *
 * Status: SCAFFOLD — actions shell out to `kain` CLI. Will be fleshed
 * out with richer output parsing on first real use.
 */

import type { ExtensionAPI } from "@earendil-works/pi-coding-agent";
import { Type } from "typebox";
import { existsSync } from "node:fs";
import { join, resolve } from "node:path";
import { spawnSync } from "node:child_process";

// ===========================================================================
// Helpers
// ===========================================================================

function runKain(args: string[], cwd?: string): { stdout: string; stderr: string; code: number } {
  const result = spawnSync("kain", args, {
    cwd: cwd ?? process.cwd(),
    encoding: "utf-8",
    timeout: 300_000,
    shell: true,
  });
  return {
    stdout: (result.stdout ?? "").trim(),
    stderr: (result.stderr ?? "").trim(),
    code: result.status ?? -1,
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
      "GPU shader artifacts. Shells out to the installed `kain` CLI.",
    promptSnippet: "Compile, check, run, and test Kain source files and projects",
    promptGuidelines: [
      "Use kain_lang when you need to compile Kain files to native code, run blades, check syntax/type correctness, or generate GPU shader artifacts.",
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
      target: Type.Optional(Type.String({ description: "File or directory path for the action. Defaults to current directory or nearest build.kn project." })),
      build_target: Type.Optional(Type.Enum({ llvm: "llvm", rust: "rust", cpp: "cpp", wasm: "wasm", spirv: "spirv", cuda: "cuda" }, { description: "Compilation target for 'build' action. Default 'llvm'." })),
      output: Type.Optional(Type.String({ description: "Output directory for gpu_artifacts." })),
    }),
    async execute(_toolCallId: string, params: any, _signal: AbortSignal, _onUpdate: any, _ctx: any) {
      try {
        const target = params.target ?? findKainProject(process.cwd()) ?? process.cwd();
        let result: { stdout: string; stderr: string; code: number };

        switch (params.action) {
          case "check":
            result = runKain(["check", target]);
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
            result = runKain(["gpu-artifacts", target, ...(params.output ? ["--output", params.output] : [])]);
            break;

          default:
            return { content: [{ type: "text", text: `Unknown action '${params.action}'.` }], details: {}, isError: true };
        }

        if (result.code !== 0) {
          return {
            content: [{ type: "text", text: `## ❌ \`kain ${params.action}\` failed (exit ${result.code})\n\n${result.stderr.slice(0, 3000) || result.stdout.slice(0, 3000)}` }],
            details: { action: params.action, exitCode: result.code },
            isError: true,
          };
        }

        return {
          content: [{ type: "text", text: `## ✅ \`kain ${params.action}\` succeeded\n\n${result.stdout.slice(0, 4000)}${result.stderr ? `\n\n### stderr\n\`\`\`\n${result.stderr.slice(0, 1000)}\n\`\`\`` : ""}` }],
          details: { action: params.action, exitCode: 0 },
        };
      } catch (e: any) {
        return { content: [{ type: "text", text: `Error: ${e.message}` }], details: {}, isError: true };
      }
    },
  });

  pi.on("session_start", async (_event, ctx) => {
    ctx.ui.notify("🔧 Kain Lang tools loaded — 6 compile/check/run actions", "info");
  });
}
