/**
 * kain-native — Zero-friction native binary emitter
 *
 * Bridges 'kain build --target llvm' → clang → native binary.
 * The compiler proper should absorb this into 'kain build' itself.
 * Until then, this tool gives you the seamless UX.
 *
 * Usage (via pi agent):
 *   kain-native action:'build' target:'src/file.kn' emit:'exe'
 *   kain-native action:'build' target:'src/file.kn' emit:'sharedlib' output:'src/file.dll'
 */

import type { ExtensionAPI } from "@earendil-works/pi-coding-agent";
import { Type } from "typebox";
import { existsSync } from "node:fs";
import { join, resolve } from "node:path";
import { spawnSync } from "node:child_process";

const SCRIPT = "X:/tools/kain_native.py";

export default function (pi: ExtensionAPI) {
  pi.registerTool({
    name: "kain_native",
    label: "Kain Native",
    description:
      "Zero-friction native binary emitter: compile Kain to .exe, .dll, .lib, .obj in one command. " +
      "Bridges kain build (LLVM IR) through bundled clang to produce native binaries. " +
      "Will be absorbed into 'kain build' when the compiler proper adds --emit support.",

    promptSnippet: "Compile Kain to native binaries (.exe, .dll, .lib, .obj) in one shot",
    promptGuidelines: [
      "Use kain_native to compile Kain source to native binaries without manual IR editing or clang invocation.",
      "Default emit is 'exe' — produces a standalone executable next to the source file.",
      "Use emit:'sharedlib' to produce a .dll/.so for FFI interop with Python, Node.js, Rust, C, etc.",
      "Use emit:'object' for .obj/.o files, emit:'llvm-ir' for .ll files (passthrough).",
      "The wrapper auto-detects whether the source needs libc (runtime functions) and links accordingly.",
    ],

    parameters: Type.Object({
      target: Type.String({ description: "Path to Kain source file (.kn)." }),
      emit: Type.Optional(
        Type.Enum(
          { exe: "exe", sharedlib: "sharedlib", staticlib: "staticlib", object: "object", "llvm-ir": "llvm-ir" },
          { description: "Output artifact type. 'exe' (default), 'sharedlib' (.dll/.so), 'staticlib' (.lib/.a), 'object' (.obj/.o), 'llvm-ir' (.ll)." },
        ),
      ),
      output: Type.Optional(Type.String({ description: "Output path override (default: derived from source name)." })),
    }),

    async execute(_toolCallId: string, params: any, _signal: AbortSignal, _onUpdate: any, _ctx: any) {
      try {
        if (!params.target) {
          return { content: [{ type: "text", text: "Provide `target` — path to Kain source file." }], details: {}, isError: true };
        }

        const source = resolve(params.target);
        if (!existsSync(source)) {
          return { content: [{ type: "text", text: `Source not found: ${source}` }], details: {}, isError: true };
        }

        const args = [SCRIPT, source];
        if (params.emit) args.push("--emit", params.emit);
        if (params.output) args.push("-o", resolve(params.output));

        const result = spawnSync("py", ["-3", "-X", "utf8", ...args], {
          cwd: process.cwd(),
          encoding: "utf-8", timeout: 120_000, maxBuffer: 1 * 1024 * 1024,
        });

        const text = result.stdout || "";

        if (result.status !== 0) {
          return {
            content: [{ type: "text", text: `## ❌ kain-native failed\n\n\`\`\`\n${text}\n${result.stderr || ""}\n\`\`\`` }],
            details: { exitCode: result.status },
            isError: true,
          };
        }

        return { content: [{ type: "text", text }], details: { emit: params.emit || "exe" } };
      } catch (e: any) {
        return { content: [{ type: "text", text: `Error: ${e.message}` }], details: {}, isError: true };
      }
    },
  });

  pi.registerCommand("kain-native", {
    description: "Quick native build: kain-native <file.kn> [--emit exe|sharedlib|staticlib|object]",
    handler: async (args, ctx) => {
      ctx.ui.notify(`kain-native: use the tool with action:'build' target:'${args || "..."}'`, "info");
    },
  });

  pi.on("session_start", async (_event, ctx) => {
    ctx.ui.notify("🔧 kain-native loaded — zero-friction native builds", "info");
  });
}
