/**
 * repo — The Kain repo itself as a navigable surface
 *
 * Tool: repo
 *   action: map          — Show full repo topology
 *   action: map crates   — Layout + descriptions for every crate
 *   action: map runtime  — Layout + descriptions for every runtime C file
 *   action: map stdlib   — Stdlib module listing (from kain_stdlib)
 *   action: map blades   — Blade workspace listing
 *   action: update       — Regenerate all MAP.md + JSON from disk
 *   action: status       — Quick repo snapshot (counts, filled descriptions)
 */

import type { ExtensionAPI } from "@earendil-works/pi-coding-agent";
import { Type } from "typebox";
import { readFileSync, existsSync, readdirSync, statSync } from "node:fs";
import { join, resolve } from "node:path";
import { spawnSync } from "node:child_process";

// ===========================================================================
// Paths
// ===========================================================================

const ROOT = (() => {
  let dir = process.cwd();
  for (let i = 0; i < 20; i++) {
    if (existsSync(join(dir, "AGENTS.md")) && existsSync(join(dir, "CATALOG.md"))) return dir;
    const parent = resolve(dir, "..");
    if (parent === dir) break;
    dir = parent;
  }
  return process.cwd();
})();

const GENERATED_MAP = join(ROOT, "generated", "repo_map.json");
const GENERATOR = join(ROOT, "tools", "repo_map_gen.py");
const CONFIG_PATH = join(ROOT, "mapconfig.json");

// Co-located description files
const DESCRIPTIONS: Record<string, string> = {
  Root: join(ROOT, "map.json"),
  Crates: join(ROOT, "crates", "map.json"),
  "Runtime Native Source": join(ROOT, "runtime", "native", "src", "map.json"),
};

// ===========================================================================
// Data layer
// ===========================================================================

let _mapData: any = null;
function getMap(): any {
  if (_mapData) return _mapData;
  if (!existsSync(GENERATED_MAP)) throw new Error("repo_map.json not found. Run `repo update` first.");
  _mapData = JSON.parse(readFileSync(GENERATED_MAP, "utf-8"));
  return _mapData;
}

function getArea(name: string): any {
  const areas = getMap().areas ?? [];
  return areas.find((a: any) => a.name === name);
}

function getDescriptions(areaName: string): Record<string, string> {
  const path = DESCRIPTIONS[areaName];
  if (!path || !existsSync(path)) return {};
  return JSON.parse(readFileSync(path, "utf-8"));
}

function padRight(s: string, n: number): string {
  return s.length < n ? s + " ".repeat(n - s.length) : s;
}

// ===========================================================================
// Actions
// ===========================================================================

function actionFullMap(): string {
  const map = getMap();
  const areas = map.areas ?? [];
  const total = areas.reduce((s: number, a: any) => s + a.entry_count, 0);
  const lines: string[] = [];

  lines.push("═══ KAIN REPO ═══════════════════════════════════════════");
  lines.push(`  ${areas.length} areas  ·  ${total} total entries`);
  lines.push(`  Generated: ${(map.generated_at ?? "").slice(0, 19).replace("T", " ")}`);
  lines.push("");

  for (const area of areas) {
    const descs = getDescriptions(area.name);
    const typeIcon = area.scan_mode === "folders" ? "📁" : "📄";
    lines.push(`── ${area.name} (${area.entry_count} ${area.scan_mode}) ─${"─".repeat(25)}`);
    lines.push(`  ${area.scan_path}/`);
    lines.push("");

    for (const entry of area.entries.slice(0, 60)) {
      const desc = descs[entry.name] || "";
      const display = entry.name.endsWith("/") ? entry.name : entry.name;
      if (desc) {
        lines.push(`  ${padRight(display, 30)} ${desc}`);
      } else {
        lines.push(`  ${display}`);
      }
    }
    if (area.entry_count > 60) {
      lines.push(`  ... and ${area.entry_count - 60} more`);
    }
    lines.push("");
  }

  lines.push("══════════════════════════════════════════════════════════");
  lines.push("  repo map crates   — detailed crate view");
  lines.push("  repo map runtime  — detailed runtime view");
  lines.push("  repo update       — regenerate from filesystem");

  return lines.join("\n");
}

function actionAreaMap(areaName: string, label: string): string {
  const area = getArea(areaName);
  if (!area) return `## No area '${areaName}' found.`;

  const descs = getDescriptions(areaName);
  const lines: string[] = [];

  lines.push(`═══ ${label} ═${"═".repeat(45)}`);
  lines.push(`  ${area.entry_count} entries under ${area.scan_path}/`);
  lines.push("");

  for (const entry of area.entries) {
    const desc = descs[entry.name] || "";
    if (desc) {
      lines.push(`  ${entry.name}`);
      lines.push(`      ${desc}`);
    } else {
      lines.push(`  ${entry.name}  (no description)`);
    }
    lines.push("");
  }

  const descFile = DESCRIPTIONS[areaName] ?? "(unknown)";
  lines.push("───");
  lines.push(`  Edit descriptions in ${descFile.replace(ROOT, ".")}`);

  return lines.join("\n");
}

function actionBladesMap(): string {
  // Derived: find blades in the root area listing or scan blades/ directly
  const rootArea = getArea("Root");
  const blades = rootArea?.entries?.filter((e: any) => e.name.startsWith("b") && e.path.includes("blades")) ?? [];
  if (blades.length === 0) {
    // Fallback: try to list blades/ manually
    const bladesDir = join(ROOT, "blades");
    if (existsSync(bladesDir)) {
      const items = readdirSync(bladesDir).filter((n: string) => {
        try { return statSync(join(bladesDir, n)).isDirectory(); } catch { return false; }
      });
      return (
        "═══ BLADES ══════════════════════════════════════════════\n\n" +
        items.map((n: string) => `  ${n}/`).join("\n") +
        "\n\n  Found by direct scan. Run `repo update` to register in the map."
      );
    }
    return "No blades area found.";
  }
  const lines = ["═══ BLADES ══════════════════════════════════════════════", ""];
  for (const b of blades) lines.push(`  ${b.name}/`);
  return lines.join("\n");
}

function actionStdlibMap(): string {
  return (
    "═══ STDLIB ══════════════════════════════════════════════\n\n" +
    "  66 modules under stdlib/\n\n" +
    "  Use kain_stdlib → list_modules for the full listing.\n" +
    "  Use kain_stdlib → get_symbols for module details.\n\n" +
    "───\n" +
    "  This data is owned by kain_stdlib, not repo."
  );
}

function actionUpdate(): string {
  // Try py -3 first, fall back to python
  const pythonCmds = [["py", "-3"], ["python3"], ["python"]];
  let result: any = null;
  for (const [cmd, ...args] of pythonCmds) {
    result = spawnSync(cmd, [...args, GENERATOR], {
      cwd: ROOT, encoding: "utf-8", timeout: 60_000, shell: true,
      env: { ...process.env },
    });
    if (result.status === 0) break;
  }
  if (!result || result.status !== 0) {
    const err = (result?.stderr || result?.stdout || "No Python found").slice(0, 1000);
    const fullOut = (result?.stdout || "").slice(0, 1000);
    return `## ❌ Update failed\n\n${err}\n\nstdout:\n${fullOut}`;
  }
  _mapData = null;
  const map = getMap();
  const total = (map.areas ?? []).reduce((s: number, a: any) => s + a.entry_count, 0);
  return (
    "## ✅ Repo maps regenerated\n\n" +
    `${map.areas.length} areas, ${total} total entries\n` +
    `Generated at: ${(map.generated_at ?? "").slice(0, 19).replace("T", " ")}\n\n` +
    "Edit descriptions in each area's map.json."
  );
}

function actionStatus(): string {
  try {
    const map = getMap();
    const areas = map.areas ?? [];
    const total = areas.reduce((s: number, a: any) => s + a.entry_count, 0);

    let filledTotal = 0;
    let entriesTotal = 0;
    const detailLines: string[] = [];

    for (const area of areas) {
      const descs = getDescriptions(area.name);
      const filled = Object.values(descs).filter((v: any) => v !== "").length;
      const totalEntries = area.entry_count;
      filledTotal += filled;
      entriesTotal += totalEntries;
      const pct = totalEntries > 0 ? Math.round(filled / totalEntries * 100) : 0;
      detailLines.push(`  ${area.name.padEnd(25)} ${filled}/${totalEntries} (${pct}%)`);
    }

    return (
      "## 📊 Repo Status\n\n" +
      `| Metric | Value |\n` +
      `| :--- | ---: |\n` +
      `| Areas | ${areas.length} |\n` +
      `| Total entries | ${total} |\n` +
      `| Descriptions filled | ${filledTotal}/${entriesTotal} |\n` +
      `| Last generated | ${(map.generated_at ?? "").slice(0, 19).replace("T", " ") || "never"} |\n\n` +
      detailLines.join("\n") +
      (filledTotal < entriesTotal
        ? "\n\n⚠️ Some descriptions missing — edit each area's map.json"
        : "\n\n✅ All descriptions filled")
    );
  } catch (e: any) {
    return `## ❌ No map data\n\nRun \`repo update\` first.\n\n${e.message}`;
  }
}

// ===========================================================================
// Tool registration
// ===========================================================================

export default function (pi: ExtensionAPI) {
  pi.registerTool({
    name: "repo",
    label: "Repo",
    description:
      "Navigate the Kain repo itself — topology, crate layout, runtime files, " +
      "stdlib modules, and blade workspaces. Maps are auto-generated from the " +
      "filesystem and filtered by .gitignore. Descriptions live in co-located " +
      "map.json files next to each MAP.md.",
    promptSnippet: "Navigate and understand the Kain repo layout",
    promptGuidelines: [
      "Use repo to understand the repo's physical layout — what crates exist, what runtime C files are where, and how the stdlib is organized.",
      "Run 'repo map' at the start of any session to orient yourself.",
      "Run 'repo update' when you add new crates or runtime files to regenerate the maps.",
      "Edit descriptions in crates/map.json and runtime/native/src/map.json.",
    ],
    parameters: Type.Object({
      action: Type.Enum(
        { map: "map", update: "update", status: "status" },
        {
          description:
            "'map' — Show repo topology (default: full view). " +
            "Append a target: 'crates', 'runtime', 'stdlib', 'blades'. " +
            "| 'update' — Regenerate all MAP.md + JSON from disk. " +
            "| 'status' — Quick repo snapshot.",
        },
      ),
      target: Type.Optional(
        Type.String({
          description:
            "Optional sub-view for 'map': 'crates', 'runtime', 'stdlib', 'blades'. " +
            "When omitted, shows the full repo topology.",
        }),
      ),
    }),
    async execute(_toolCallId: string, params: any, _signal: AbortSignal, _onUpdate: any, _ctx: any) {
      try {
        switch (params.action) {
          case "update":
            return { content: [{ type: "text", text: actionUpdate() }], details: {} };
          case "status":
            return { content: [{ type: "text", text: actionStatus() }], details: {} };
          case "map": {
            const target = (params.target ?? "").toLowerCase().trim();
            if (!target || target === "all" || target === "full") {
              return { content: [{ type: "text", text: actionFullMap() }], details: {} };
            }
            if (target === "crates" || target === "crate") {
              return { content: [{ type: "text", text: actionAreaMap("Crates", "CRATES") }], details: {} };
            }
            if (target === "runtime") {
              return { content: [{ type: "text", text: actionAreaMap("Runtime Native Source", "RUNTIME NATIVE SRC") }], details: {} };
            }
            if (target === "stdlib") {
              return { content: [{ type: "text", text: actionStdlibMap() }], details: {} };
            }
            if (target === "blades" || target === "blade") {
              return { content: [{ type: "text", text: actionBladesMap() }], details: {} };
            }
            return { content: [{ type: "text", text: `Unknown target '${target}'. Try: crates, runtime, stdlib, blades.` }], details: {}, isError: true };
          }
          default:
            return { content: [{ type: "text", text: `Unknown action '${params.action}'.` }], details: {}, isError: true };
        }
      } catch (e: any) {
        return { content: [{ type: "text", text: `Error: ${e.message}` }], details: {}, isError: true };
      }
    },
  });

  pi.registerCommand("repo-update", {
    description: "Regenerate all repo MAP.md files and JSON from disk",
    handler: async (_args, ctx) => {
      ctx.ui.notify("Regenerating repo maps...", "info");
      const result = actionUpdate();
      ctx.ui.notify(result.includes("✅") ? "✅ Maps regenerated" : "❌ Update failed", "info");
    },
  });

  pi.on("session_start", async (_event, ctx) => {
    ctx.ui.notify("🗺️ Repo tool loaded — `repo map` to orient, `repo update` to regenerate", "info");
  });
}
