/**
 * Kain Examples — Dedicated semantic code search
 *
 * Finds real-world Kain code examples across benchmarks, blades, stdlib,
 * and demos using PyTorch/CUDA-powered semantic search over ~11,500
 * function-level code chunks.
 *
 * Tool: kain_examples
 *   action: search    — Semantic search for Kain code patterns
 *   action: trending  — Show recent or high-scoring examples (coming soon)
 *   action: by_module — Filter examples by module/file (coming soon)
 */

import type { ExtensionAPI } from "@earendil-works/pi-coding-agent";
import { Type } from "typebox";
import { spawnSync } from "node:child_process";

// ===========================================================================
// Helpers
// ===========================================================================

function handleSearch(query: string, limit?: number): string {
  const pyBin = process.platform === "win32" ? "py" : "python3";
  const pyCode = [
    "from kaindev.smart_search import smart_search; import json",
    "results = smart_search(" + JSON.stringify(query) + ", limit=" + (limit ?? 3) + ")",
    "print(json.dumps([[r['source'], r['score'], r['text'], r['kind'], r['symbol'], r['line_start'], r['line_end']] for r in results]))",
  ].join("; ");
  const proc = spawnSync(pyBin, ["-3", "-c", pyCode], {
    cwd: "X:/mcp",
    encoding: "utf-8",
    timeout: 120000,
    maxBuffer: 4 * 1024 * 1024,
    stdio: ["pipe", "pipe", "ignore"],
  });
  const raw = (proc.stdout ?? "").trim();
  if (!raw) {
    const errMsg = (proc.stderr ?? "").slice(0, 500);
    return `## ❌ Search failed\n\n${errMsg || "Empty response (timeout? PyTorch still loading?)"}`;
  }
  try {
    const results: [string, number, string, string, string, number, number][] = JSON.parse(raw);
    if (results.length === 0) return `No examples found for "${query}". Try broader terms.`;
    const lines = [`# 🔍 Semantic Search Results\n`, `**Query:** \`${query}\` — **Results:** ${results.length}`, ""];
    for (let i = 0; i < results.length; i++) {
      const [source, score, text, kind, symbol, lineStart] = results[i];
      lines.push(
        `### ${i + 1}. \`${kind}\` ${symbol || "<anonymous>"}`,
        `**File:** ${source}:${lineStart} — **Score:** ${score.toFixed(3)}`,
        "```kn",
        text || "(empty)",
        "```",
        "",
      );
    }
    return lines.join("\n");
  } catch (e: any) {
    return `## Search parse error\n\n\`\`\`\n${e.message}\n\`\`\`\n\nRaw output:\n\`\`\`\n${raw.slice(0, 500)}\n\`\`\``;
  }
}

// ===========================================================================
// Router tool
// ===========================================================================

export default function (pi: ExtensionAPI) {
  pi.registerTool({
    name: "kain_examples",
    label: "Kain Examples",
    description:
      "Semantically search ~11,500 real-world Kain code chunks using PyTorch/CUDA. " +
      "Each chunk is a focused fn/actor/world/shader/struct/law/patch definition " +
      "from benchmarks, blades, stdlib, demos, and the compiler test suite. " +
      "Returns ranked results with source paths, line numbers, and code excerpts.",
    promptSnippet: "Semantic search for Kain code examples via PyTorch (function-level chunks)",
    promptGuidelines: [
      "Use kain_examples when you need to find real-world Kain code patterns, idioms, or usage examples.",
      "This is the best tool for 'show me an example of X in Kain' or 'how do people use Y in practice?'.",
      "Use specific, descriptive queries: 'actor supervision mailbox', 'cuda warp reduce', 'teleport zero copy', 'ownership collapse observe decay'.",
    ],
    parameters: Type.Object({
      action: Type.Enum(
        { search: "search" },
        { description: "'search' — semantically search for Kain code examples matching your query." },
      ),
      query: Type.Optional(
        Type.String({
          description:
            "What you're looking for. Be descriptive and specific. " +
            "Examples: 'actor supervision mailbox', 'teleport zero copy shatter', 'cuda compute kernel warp', 'ownership collapse observe decay pointer'.",
        }),
      ),
      limit: Type.Optional(
        Type.Number({
          description: "Max results to return (default 3, max 10). Higher limits are slower.",
          default: 3,
        }),
      ),
    }),
    async execute(_toolCallId: string, params: any, _signal: AbortSignal, _onUpdate: any, _ctx: any) {
      try {
        if (!params.query) {
          return {
            content: [
              {
                type: "text",
                text:
                  "## 🔍 Kain Examples\n\n" +
                  "Semantic search over ~11,500 real Kain code chunks using PyTorch/CUDA.\n\n" +
                  "Try:\n" +
                  "- `kain_examples search query:'actor supervision mailbox'`\n" +
                  "- `kain_examples search query:'teleport zero copy shatter' limit:5`\n" +
                  "- `kain_examples search query:'cuda warp reduce tensor core'`\n" +
                  "- `kain_examples search query:'ownership collapse observe decay'`",
              },
            ],
            details: {},
          };
        }

        const cappedLimit = Math.min(params.limit ?? 3, 10);
        const result = handleSearch(params.query, cappedLimit);
        return { content: [{ type: "text", text: result }], details: {} };
      } catch (e: any) {
        return { content: [{ type: "text", text: `Error: ${e.message}` }], details: {}, isError: true };
      }
    },
  });

  pi.on("session_start", async (_event, ctx) => {
    ctx.ui.notify("🔍 Kain Examples loaded — semantic search over 11,500 code chunks", "info");
  });
}
