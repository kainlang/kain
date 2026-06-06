/**
 * tools — The Brain
 *
 * Tool discovery and navigation. Lists every available tool, searches by
 * keyword, and recommends the right tool + action for any given task.
 *
 * This is the meta-tool — the one you call when you don't know which tool
 * to call. It reads `.pi/tools/manifest.json` to discover the full arsenal.
 *
 * Tool: tools
 *   action: list   — List all tools grouped by domain
 *   action: search — Search tools by keyword  
 *   action: which  — Recommend tool + action for a task
 */

import type { ExtensionAPI } from "@earendil-works/pi-coding-agent";
import { Type } from "typebox";
import { readFileSync, existsSync } from "node:fs";
import { join, resolve } from "node:path";

// ---------------------------------------------------------------------------
// Manifest loader
// ---------------------------------------------------------------------------

let _manifest: any = null;

function getManifest(): any {
  if (_manifest) return _manifest;
  const paths = [join(process.cwd(), ".pi/tools/manifest.json"), join(resolve(process.cwd(), "../.."), ".pi/tools/manifest.json")];
  for (const p of paths) {
    if (existsSync(p)) {
      _manifest = JSON.parse(readFileSync(p, "utf-8"));
      return _manifest;
    }
  }
  throw new Error("Cannot find .pi/tools/manifest.json — are you in the repo root?");
}

function getAllDomains(): any[] {
  return getManifest()?.domains ?? [];
}

// ===========================================================================
// Tool: tools — the brain
// ===========================================================================

export default function (pi: ExtensionAPI) {
  pi.registerTool({
    name: "tools",
    label: "Tools",
    description:
      "Discover, search, and navigate the full pi agent tool arsenal. " +
      "Lists every available tool grouped by domain, searches by keyword, " +
      "and recommends the best tool + action for any given task. " +
      "Your starting point when you're not sure which tool to use.",

    promptSnippet: "Discover and navigate all available agent tools",
    promptGuidelines: [
      "Run tools action:'list' at the start of any session to see the full tool arsenal with descriptions.",
      "Use tools action:'which' when you have a task but aren't sure which tool to call.",
    ],

    parameters: Type.Object({
      action: Type.Enum(
        {
          list: "list",
          search: "search",
          which: "which",
        },
        { description: "'list' to see all tools, 'search' to find by keyword, 'which' to get a recommendation." },
      ),
      query: Type.Optional(
        Type.String({
          description: "Search query (required for 'search' and 'which' actions). For 'search': keywords to match. For 'which': a description of the task you want to accomplish.",
        }),
      ),
    }),

    async execute(
      _toolCallId: string,
      params: { action: "list" | "search" | "which"; query?: string },
      _signal: AbortSignal,
      _onUpdate: any,
      _ctx: any,
    ) {
      try {
        const domains = getAllDomains();

        switch (params.action) {
          // ============================================================
          // LIST — Show every tool grouped by domain
          // ============================================================
          case "list": {
            const lines = ["# 🧰 Kain Agent Tool Arsenal", ""];
            for (const domain of domains) {
              const actions = Object.entries(domain.actions ?? {}) as [string, any][];
              lines.push(
                `## \`${domain.id}\``,
                `> ${domain.description}`,
                "",
                `**File:** \`${domain.file}\``,
                `**Actions (${actions.length}):**`,
                "",
              );
              for (const [actionId, action] of actions) {
                lines.push(`- **\`${actionId}\`** — ${action.description}`);
              }
              lines.push("");
            }
            return { content: [{ type: "text", text: lines.join("\n") }], details: {} };
          }

          // ============================================================
          // SEARCH — Find tools by keyword
          // ============================================================
          case "search": {
            if (!params.query) {
              return {
                content: [{ type: "text", text: "Provide a `query` parameter to search for tools." }],
                details: {},
                isError: true,
              };
            }
            const q = params.query.toLowerCase();
            const results: { domain: string; actionId: string; label: string; description: string; score: number }[] = [];

            for (const domain of domains) {
              const actions = Object.entries(domain.actions ?? {}) as [string, any][];
              for (const [actionId, action] of actions) {
                const haystack = `${domain.id} ${domain.description} ${actionId} ${action.label} ${action.description}`.toLowerCase();
                // Score: direct substring match
                if (haystack.includes(q)) {
                  results.push({
                    domain: domain.id,
                    actionId,
                    label: action.label,
                    description: action.description,
                    score: haystack.indexOf(q) === 0 ? 3 : haystack.includes(` ${q} `) ? 2 : 1,
                  });
                }
              }
            }

            results.sort((a, b) => b.score - a.score);

            if (results.length === 0) {
              return {
                content: [{ type: "text", text: `No tools found matching "${params.query}". Try broader terms or use \`tools action:'list'\` to see everything.` }],
                details: {},
              };
            }

            const lines = [`## 🔍 Search Results for "${params.query}"`, "", `Found ${results.length} matching tool action(s):`, ""];
            for (const r of results) {
              lines.push(`### \`${r.domain} → ${r.actionId}\``, `> ${r.description}`, "");
            }
            lines.push("---", "", `Call the tool above with the appropriate action. E.g.: \`${results[0].domain} action:'${results[0].actionId}'\``);
            return { content: [{ type: "text", text: lines.join("\n") }], details: { results } };
          }

          // ============================================================
          // WHICH — Recommend tool for a task
          // ============================================================
          case "which": {
            if (!params.query) {
              return {
                content: [{ type: "text", text: "Describe what you want to do in the `query` parameter, and I'll recommend the right tool." }],
                details: {},
                isError: true,
              };
            }
            const q = params.query.toLowerCase();

            // Keyword-based routing heuristics
            const routes: { keywords: string[]; domain: string; action: string; rationale: string }[] = [
              { keywords: ["module", "symbol", "stdlib", "keyword", "function signature", "docs", "source code", "how do i use"], domain: "kain_stdlib", action: "search_symbols", rationale: "The Kain stdlib tool has comprehensive symbol search across all modules." },
              { keywords: ["list all module", "what modules"], domain: "kain_stdlib", action: "list_modules", rationale: "Shows every stdlib module with symbol counts." },
              { keywords: ["keyword", "language keyword", "world", "teleport", "entangle", "actor", "shader", "converge", "shatter", "pulse", "resonate", "orchestrate", "law", "patch", "axiom"], domain: "kain_stdlib", action: "get_keyword", rationale: "The Kain stdlib tool has a complete keyword reference." },
              { keywords: ["example", "code example", "how to write", "idiom", "pattern"], domain: "kain_stdlib", action: "search_examples", rationale: "Semantic search over 11,500 real Kain code chunks with PyTorch/CUDA." },
              { keywords: ["build", "compile", "bazel build", "make", "rebuild"], domain: "kain_bazel", action: "build", rationale: "The Bazel tool handles all build targets (compiler, runtime, launcher)." },
              { keywords: ["test", "smoke test", "crate test", "run test"], domain: "kain_bazel", action: "test", rationale: "The Bazel tool runs test suites with the right config automatically." },
              { keywords: ["server", "bazel warm", "cold server", "slow bazel"], domain: "kain_bazel", action: "server", rationale: "Manage the Bazel server lifecycle to avoid cold-start delays." },
              { keywords: ["fresh", "stale", "binary age", "out of date", "freshness"], domain: "kain_bazel", action: "freshness", rationale: "Full repo freshness audit checks binary age vs source changes." },
              { keywords: ["sync", "runtime build", "native runtime", "build drift"], domain: "kain_bazel", action: "sync", rationale: "The sync action validates and regenerates native runtime build files." },
              { keywords: ["kain check", "typecheck", "kain build", "llvm", "compile to native", "kain run", "execute"], domain: "kain_lang", action: "build", rationale: "The Kain lang tool handles compilation, checking, and running .kn files." },
              { keywords: ["amalgamate", "single file", "bundle"], domain: "kain_lang", action: "amalgamate", rationale: "Combine blade workspaces into single files." },
              { keywords: ["shader", "spirv", "ptx", "gpu", "compute"], domain: "kain_lang", action: "gpu_artifacts", rationale: "Compile GPU shaders to SPIR-V, PTX, etc." },
            ];

            for (const route of routes) {
              if (route.keywords.some((kw) => q.includes(kw))) {
                return {
                  content: [{
                    type: "text",
                    text:
                      `## 🎯 Recommended: \`${route.domain}\` → \`${route.action}\`\n\n` +
                      `**Rationale:** ${route.rationale}\n\n` +
                      `\`\`\`\n${route.domain} action:'${route.action}'${route.keywords[0].includes("module") ? " query:'<your search>'" : ""}\n\`\`\`\n\n` +
                      `**Not what you need?** Try \`tools action:'search' query:'${params.query}'\` for more options.`,
                  }],
                  details: { recommendation: route },
                };
              }
            }

            // Fallback: do a full search and suggest the top result
            const searchResult = getAllDomains()
              .flatMap((d) =>
                Object.entries(d.actions ?? {}).map(([id, a]: [string, any]) => ({
                  domain: d.id,
                  action: id,
                  description: `${d.description} — ${a.description}`,
                  score: `${d.id} ${d.description} ${id} ${a.label} ${a.description}`.toLowerCase().includes(q) ? 1 : 0,
                })),
              )
              .filter((r) => r.score > 0)
              .sort((a, b) => b.score - a.score);

            if (searchResult.length > 0) {
              const top = searchResult[0];
              return {
                content: [{
                  type: "text",
                  text:
                    `## 🤔 Best Guess: \`${top.domain}\` → \`${top.action}\`\n\n` +
                    `Based on a broad search, this looks like the closest match. Try it, or use \`tools action:'search' query:'${params.query}'\` to see all matches.`,
                }],
                details: { recommendation: top },
              };
            }

            return {
              content: [{
                type: "text",
                text:
                  `## 🤷 No Strong Match\n\n` +
                  `Couldn't find a clear tool match for "${params.query}". Try:\n\n` +
                  `- \`tools action:'list'\` to browse everything available\n` +
                  `- \`tools action:'search' query:'${params.query}'\` for broader matching`,
              }],
              details: {},
            };
          }
        }
      } catch (e: any) {
        return { content: [{ type: "text", text: `Error: ${e.message}` }], details: {}, isError: true };
      }
    },
  });

  // Notify on load
  pi.on("session_start", async (_event, ctx) => {
    ctx.ui.notify("🧠 Tools brain loaded — list, search, which are live", "info");
  });
}
