/**
 * rg — God-mode code search for the Kain repo
 *
 * Precision-first ripgrep with repo-aware scoping, intelligent
 * summarization, and clean output that won't nuke your context window.
 *
 * Five output modes, each tuned for a specific hunting posture:
 *   smart   (default) — Stats + area breakdown + top matches + compact file list
 *   files              — Only matching file paths
 *   count              — Per-file match counts, sorted high→low
 *   content            — Traditional grouped display with context lines
 *   json               — Raw rg JSON stream passthrough
 *
 * Scope presets (auto-set path + glob):
 *   stdlib, crates, runtime, blades, benchmark, smoketest
 */

import type { ExtensionAPI } from "@earendil-works/pi-coding-agent";
import { Type } from "typebox";
import { existsSync } from "node:fs";
import { join, resolve, relative, sep } from "node:path";
import { spawnSync } from "node:child_process";

// ===========================================================================
// Repo root
// ===========================================================================

const ROOT = (() => {
  let dir = process.cwd();
  for (let i = 0; i < 20; i++) {
    if (existsSync(join(dir, "AGENTS.md"))) return dir;
    const p = resolve(dir, "..");
    if (p === dir) break;
    dir = p;
  }
  return process.cwd();
})();

// ===========================================================================
// Scope presets — repo-aware fast lanes
// ===========================================================================

const SCOPE_PRESETS: Record<string, { path: string; glob: string; label: string; desc: string }> = {
  stdlib:    { path: "stdlib",    glob: "*.kn",        label: "Standard Library", desc: "stdlib/*.kn (65+ modules)" },
  crates:    { path: "crates",    glob: "*.rs",        label: "Rust Crates",      desc: "crates/**/*.rs (compiler internals)" },
  runtime:   { path: "runtime",   glob: "*.{c,h}",     label: "Native Runtime",   desc: "runtime/**/*.{c,h} (C substrate)" },
  blades:    { path: "blades",    glob: "*.kn",        label: "Blades",           desc: "blades/**/*.kn (dogfood workspaces)" },
  benchmark: { path: "benchmark", glob: "*.kn",        label: "Benchmarks",       desc: "benchmark/**/*.kn (perf cases)" },
  smoketest: { path: "smoketest", glob: "*.kn",        label: "Smoke Tests",      desc: "smoketest/**/*.kn (regression suite)" },
  selfhost:  { path: "src",       glob: "*.kn",        label: "Selfhost Source",  desc: "src/**/*.kn (Kain in Kain)" },
  agents:    { path: ".agents",   glob: "*.md",        label: "Agent Skills",     desc: ".agents/**/*.md (skill docs)" },
  guides:    { path: "guides",    glob: "*.md",        label: "Guides",           desc: "guides/**/*.md (long-form docs)" },
};

// ===========================================================================
// Area classifier — which part of the repo does a file live in?
// ===========================================================================

function classifyArea(relPath: string): string {
  // rg emits / paths even on Windows, so split on both separators
  const top = relPath.split(/[\/\\]/)[0].toLowerCase();
  const known = ["stdlib", "crates", "runtime", "blades", "benchmark", "smoketest", "src", ".agents", "guides", "docs", "attrition", "z3", "release"];
  if (known.includes(top)) return top;
  return "root";
}

const AREA_ICONS: Record<string, string> = {
  stdlib:    "📚",
  crates:    "🦀",
  runtime:   "⚙️",
  blades:    "🗡️",
  benchmark: "🏎️",
  smoketest: "🧪",
  src:       "🔮",
  ".agents": "🤖",
  guides:    "📖",
  docs:      "📄",
  attrition: "💀",
  z3:        "🧩",
  release:   "📦",
  root:      "📁",
};

// ===========================================================================
// rg runner — invokes ripgrep, returns structured results
// ===========================================================================

interface RgMatch {
  file: string;        // absolute path
  relPath: string;     // repo-relative path
  line: number;
  column: number;
  content: string;     // the matching line text
  matchText: string;   // the actual matched substring
  contextBefore: string[];
  contextAfter: string[];
}

interface RgFileGroup {
  relPath: string;
  area: string;
  matches: RgMatch[];
}

interface RgResults {
  pattern: string;
  elapsedMs: number;
  totalMatches: number;
  totalFiles: number;
  files: RgFileGroup[];
  allRelPaths: string[];   // sorted
}

function runRg(params: {
  pattern: string;
  searchRoot: string;
  glob?: string;
  ignoreCase?: boolean;
  fixed?: boolean;
  maxCount?: number;
  contextLines?: number;
  mode: "json" | "count" | "files";
}): { stdout: string; stderr: string; code: number; elapsedMs: number } {
  const args: string[] = ["--color", "never", "--no-config"];

  if (params.ignoreCase) args.push("--ignore-case");
  if (params.fixed) args.push("--fixed-strings");
  if (params.maxCount) args.push("--max-count", String(params.maxCount));

  if (params.mode === "json") {
    args.push("--json");
  } else if (params.mode === "count") {
    args.push("--count");
  } else if (params.mode === "files") {
    args.push("--files-with-matches");
  }

  if (params.glob) {
    for (const g of params.glob.split(",").map((s) => s.trim()).filter(Boolean)) {
      args.push("--glob", g);
    }
  }

  if (params.contextLines && params.contextLines > 0 && params.mode === "json") {
    args.push("--before-context", String(params.contextLines));
    args.push("--after-context", String(params.contextLines));
  }

  args.push(params.pattern);
  args.push(params.searchRoot);

  const start = Date.now();
  const result = spawnSync("rg", args, {
    cwd: ROOT,
    encoding: "utf-8",
    timeout: 30_000,
    maxBuffer: 10 * 1024 * 1024,
  });
  return {
    stdout: (result.stdout ?? "").trim(),
    stderr: (result.stderr ?? "").trim(),
    code: result.status ?? -1,
    elapsedMs: Date.now() - start,
  };
}

// ===========================================================================
// JSON parser — rg --json → structured matches
// ===========================================================================

function parseRgJson(jsonOutput: string, contextLines: number = 0): RgMatch[] {
  const lines = jsonOutput.split("\n").filter(Boolean);
  const matches: RgMatch[] = [];
  let currentFile = "";
  let contextBefore: string[] = [];
  let lineNum = 0;
  let colNum = 0;
  let matchText = "";
  let inCtxBefore = false;
  let inCtxAfter = false;
  let ctxAfterRemaining = 0;
  let contextAfter: string[] = [];
  let pendingMatch: { line: number; column: number; content: string; matchText: string } | null = null;

  for (const line of lines) {
    try {
      const obj = JSON.parse(line);

      if (obj.type === "begin") {
        currentFile = obj.data?.path?.text ?? currentFile;
        contextBefore = [];
        inCtxBefore = true;
        inCtxAfter = false;
        pendingMatch = null;
      } else if (obj.type === "match") {
        currentFile = obj.data?.path?.text ?? currentFile;
        lineNum = obj.data?.line_number ?? lineNum;
        const content = obj.data?.lines?.text?.replace(/\n$/, "") ?? "";
        const sub = obj.data?.submatches?.[0];
        colNum = sub?.start ?? 0;
        matchText = sub?.match?.text ?? "";

        if (inCtxBefore && contextLines > 0) {
          // The lines before this match were context_before, we already captured them.
          // Now push the pending match (if any) and the current context.
          inCtxBefore = false;
        }

        if (pendingMatch) {
          matches.push({
            file: currentFile,
            relPath: toRelPath(currentFile),
            line: pendingMatch.line,
            column: pendingMatch.column,
            content: pendingMatch.content,
            matchText: pendingMatch.matchText,
            contextBefore: [...contextBefore],
            contextAfter: [],
          });
          pendingMatch = null;
          contextBefore = [];
        }

        pendingMatch = {
          line: lineNum,
          column: colNum,
          content,
          matchText,
        };

        inCtxAfter = true;
        ctxAfterRemaining = contextLines;
        contextAfter = [];
      } else if (obj.type === "context") {
        const ctxContent = obj.data?.lines?.text?.replace(/\n$/, "") ?? "";
        if (inCtxBefore && contextLines > 0) {
          contextBefore.push(ctxContent);
        } else if (inCtxAfter && ctxAfterRemaining > 0) {
          contextAfter.push(ctxContent);
          ctxAfterRemaining--;
        }
      } else if (obj.type === "end") {
        // Flush pending match
        if (pendingMatch) {
          matches.push({
            file: currentFile,
            relPath: toRelPath(currentFile),
            line: pendingMatch.line,
            column: pendingMatch.column,
            content: pendingMatch.content,
            matchText: pendingMatch.matchText,
            contextBefore: [...contextBefore],
            contextAfter: [...contextAfter],
          });
          pendingMatch = null;
        }
        currentFile = "";
        contextBefore = [];
        contextAfter = [];
        inCtxBefore = false;
        inCtxAfter = false;
      }
    } catch {
      // Skip malformed JSON lines
    }
  }

  // Final flush
  if (pendingMatch) {
    matches.push({
      file: currentFile,
      relPath: toRelPath(currentFile),
      line: pendingMatch.line,
      column: pendingMatch.column,
      content: pendingMatch.content,
      matchText: pendingMatch.matchText,
      contextBefore: [...contextBefore],
      contextAfter: [...contextAfter],
    });
  }

  return matches;
}

function toRelPath(absPath: string): string {
  try {
    let r = relative(ROOT, absPath);
    if (r.startsWith("..")) {
      // Not under repo root — strip drive letter and leading slashes
      r = absPath.replace(/^[A-Za-z]:/, "").replace(/^[\/\\]+/, "");
    }
    return r.replace(/\\/g, "/");
  } catch {
    return absPath.replace(/\\/g, "/");
  }
}

function groupMatches(matches: RgMatch[]): RgFileGroup[] {
  const map = new Map<string, RgMatch[]>();
  for (const m of matches) {
    const key = m.relPath;
    if (!map.has(key)) map.set(key, []);
    map.get(key)!.push(m);
  }
  const groups: RgFileGroup[] = [];
  for (const [relPath, ms] of map) {
    // Sort by line number
    ms.sort((a, b) => a.line - b.line);
    groups.push({ relPath, area: classifyArea(relPath), matches: ms });
  }
  // Sort by match count desc, then path
  groups.sort((a, b) => b.matches.length - a.matches.length || a.relPath.localeCompare(b.relPath));
  return groups;
}

// ===========================================================================
// Formatters — each mode produces clean markdown
// ===========================================================================

function formatSmart(results: RgResults, pattern: string, searchRoot: string, scopeLabel?: string): string {
  const lines: string[] = [];
  const rootNote = searchRoot !== ROOT ? ` in ${toRelPath(searchRoot)}/` : "";
  const scopeNote = scopeLabel ? ` [${scopeLabel}]` : "";

  // ── Header ──
  lines.push(`## 🔍 \`${pattern}\`${rootNote}${scopeNote} — ${results.totalMatches} matches in ${results.totalFiles} files (${results.elapsedMs}ms)`);
  lines.push("");

  // ── Area Distribution ──
  if (results.files.length > 0) {
    const areaMap = new Map<string, { files: number; matches: number }>();
    for (const g of results.files) {
      const a = areaMap.get(g.area) || { files: 0, matches: 0 };
      a.files++;
      a.matches += g.matches.length;
      areaMap.set(g.area, a);
    }
    const sortedAreas = [...areaMap.entries()].sort((a, b) => b[1].matches - a[1].matches);

    lines.push("### 📊 Distribution");
    lines.push("| Area | Files | Matches |");
    lines.push("| :--- | ---: | ---: |");
    for (const [area, stats] of sortedAreas) {
      const icon = AREA_ICONS[area] ?? "📁";
      const pct = results.totalMatches > 0 ? Math.round(stats.matches / results.totalMatches * 100) : 0;
      lines.push(`| ${icon} ${area} | ${stats.files} | ${stats.matches} (${pct}%) |`);
    }
    lines.push("");
  }

  // ── Top Matches (first 6 files, up to 5 matches each) ──
  const topCount = Math.min(6, results.files.length);
  if (topCount > 0) {
    lines.push("### 🏆 Top Matches");
    lines.push("");

    for (const group of results.files.slice(0, topCount)) {
      const showCount = Math.min(5, group.matches.length);
      lines.push(`**\`${group.relPath}\`** (${group.matches.length} match${group.matches.length > 1 ? "es" : ""})`);
      lines.push("```");

      // Show best matches: prefer ones with more context, then earlier lines
      for (const m of group.matches.slice(0, showCount)) {
        const margin = " ".repeat(Math.max(0, 4 - String(m.line).length));
        lines.push(`${margin}${m.line}: ${m.content.trim()}`);
      }
      if (group.matches.length > showCount) {
        lines.push(`  ... and ${group.matches.length - showCount} more match${group.matches.length - showCount > 1 ? "es" : ""}`);
      }
      lines.push("```");
      lines.push("");
    }
  }

  // ── All Files (compact) ──
  if (results.files.length > topCount) {
    lines.push("### 📁 All Files");
    const remaining = results.files.slice(topCount);
    const fileList = remaining.map((g) => `\`${g.relPath}\` (${g.matches.length})`).join(" · ");
    lines.push(fileList);
    lines.push("");
  } else if (results.files.length <= topCount && results.files.length > 0) {
    lines.push("### 📁 All Files");
    const fileList = results.files.map((g) => `\`${g.relPath}\` (${g.matches.length})`).join(" · ");
    lines.push(fileList);
    lines.push("");
  }

  // ── Refine Suggestions ──
  lines.push("### 🎯 Refine");
  const suggestions: string[] = [];

  // Suggest narrowing by glob
  const distinctGlobs = new Set(results.files.map((g) => {
    const ext = g.relPath.split(".").pop()?.toLowerCase();
    return ext ? `*.${ext}` : null;
  }).filter(Boolean));
  if (distinctGlobs.size > 1) {
    suggestions.push(`Narrow by file type: \`glob:"${[...distinctGlobs].slice(0, 3).join(",")}"\``);
  }

  // Suggest scope presets based on dominant area
  const dominantArea = results.files[0]?.area;
  if (dominantArea && dominantArea !== "root" && results.files.length > 5) {
    const preset = Object.entries(SCOPE_PRESETS).find(([, v]) => v.path === dominantArea);
    if (preset) suggestions.push(`Scope to \`${preset[0]}\`: \`scope:"${preset[0]}"\``);
  }

  // Suggest context mode for deep diving
  if (results.totalFiles <= 5 && results.totalMatches <= 30) {
    suggestions.push(`Deep dive: \`mode:"content" context:3\` to see surrounding code`);
  }

  if (results.totalFiles > 20) {
    suggestions.push(`Too many results? Try more specific pattern or add \`fixed:true\` for literal search`);
  }

  for (const s of suggestions) lines.push(`- ${s}`);

  return lines.join("\n");
}

function formatFiles(results: RgResults, pattern: string, searchRoot: string, scopeLabel?: string): string {
  const scopeNote = scopeLabel ? ` [${scopeLabel}]` : "";
  const rootNote = searchRoot !== ROOT ? ` in ${toRelPath(searchRoot)}/` : "";
  const lines = [`## 📄 \`${pattern}\`${rootNote}${scopeNote} — ${results.totalFiles} file(s) (${results.elapsedMs}ms)`, ""];

  // Group by area for readability
  const byArea = new Map<string, string[]>();
  for (const g of results.files) {
    if (!byArea.has(g.area)) byArea.set(g.area, []);
    byArea.get(g.area)!.push(g.relPath);
  }

  for (const [area, paths] of [...byArea.entries()].sort((a, b) => b[1].length - a[1].length)) {
    const icon = AREA_ICONS[area] ?? "📁";
    if (byArea.size > 1) lines.push(`**${icon} ${area}/** (${paths.length} files)`);
    for (const p of paths.slice(0, 50)) lines.push(`- \`${p}\``);
    if (paths.length > 50) lines.push(`- ... and ${paths.length - 50} more`);
    lines.push("");
  }

  return lines.join("\n");
}

function formatCount(results: RgResults, pattern: string, searchRoot: string, scopeLabel?: string): string {
  const scopeNote = scopeLabel ? ` [${scopeLabel}]` : "";
  const rootNote = searchRoot !== ROOT ? ` in ${toRelPath(searchRoot)}/` : "";
  const lines = [`## 📊 \`${pattern}\`${rootNote}${scopeNote} — ${results.totalMatches} matches in ${results.totalFiles} files (${results.elapsedMs}ms)`, ""];
  lines.push("| File | Matches | Area |");
  lines.push("| :--- | ---: | :--- |");

  for (const g of results.files) {
    const truncated = g.relPath.length > 60 ? "..." + g.relPath.slice(-57) : g.relPath;
    lines.push(`| \`${truncated}\` | ${g.matches.length} | ${AREA_ICONS[g.area] ?? ""} ${g.area} |`);
  }

  lines.push("");
  return lines.join("\n");
}

function formatContent(results: RgResults, pattern: string, contextLines: number, searchRoot: string, scopeLabel?: string): string {
  const scopeNote = scopeLabel ? ` [${scopeLabel}]` : "";
  const rootNote = searchRoot !== ROOT ? ` in ${toRelPath(searchRoot)}/` : "";
  const lines = [`## 📝 \`${pattern}\`${rootNote}${scopeNote} — ${results.totalMatches} matches in ${results.totalFiles} files (${results.elapsedMs}ms)`, ""];

  const showFiles = Math.min(25, results.files.length);
  for (const group of results.files.slice(0, showFiles)) {
    const areaIcon = AREA_ICONS[group.area] ?? "";
    lines.push(`### ${areaIcon} \`${group.relPath}\` (${group.matches.length} match${group.matches.length > 1 ? "es" : ""})`);
    lines.push("");

    const showMatches = Math.min(15, group.matches.length);
    let prevLine = -999;

    for (const m of group.matches.slice(0, showMatches)) {
      // Show context before (if not overlapping with previous match)
      if (contextLines > 0 && m.contextBefore.length > 0) {
        const startCtx = m.line - m.contextBefore.length;
        for (let i = 0; i < m.contextBefore.length; i++) {
          const ctxLine = startCtx + i;
          if (ctxLine > prevLine) {
            lines.push(`  ${String(ctxLine).padStart(4)} │ ${m.contextBefore[i].trim()}`);
          }
        }
      }

      // Show the match line (highlighted)
      lines.push(`→ ${String(m.line).padStart(4)} │ ${m.content.trim()}`);
      prevLine = m.line;

      // Show context after
      if (contextLines > 0 && m.contextAfter.length > 0) {
        for (let i = 0; i < m.contextAfter.length; i++) {
          lines.push(`  ${String(m.line + i + 1).padStart(4)} │ ${m.contextAfter[i].trim()}`);
        }
        prevLine = m.line + m.contextAfter.length;
      }
    }

    if (group.matches.length > showMatches) {
      lines.push(`  ... and ${group.matches.length - showMatches} more matches`);
    }
    lines.push("");
  }

  if (results.files.length > showFiles) {
    lines.push(`*... and ${results.files.length - showFiles} more files. Use mode:"smart" for overview or mode:"files" for full listing.*`);
    lines.push("");
  }

  return lines.join("\n");
}

// ===========================================================================
// Mode: JSON passthrough
// ===========================================================================

function formatJson(rawJson: string, matchCount: number, elapsedMs: number): string {
  // Just pass through the raw JSON, maybe with a comment about structure
  return rawJson;
}

// ===========================================================================
// Main search orchestrator
// ===========================================================================

function doSearch(params: {
  pattern: string;
  path?: string;
  glob?: string;
  scope?: string;
  mode?: string;
  ignoreCase?: boolean;
  fixed?: boolean;
  context?: number;
  max?: number;
  json?: boolean;
}): { text: string; details: any; isError?: boolean } {
  // ── Resolve scope ──
  let scopeLabel: string | undefined;
  let scopePath: string | undefined;
  let scopeGlob: string | undefined;

  if (params.scope) {
    const preset = SCOPE_PRESETS[params.scope.toLowerCase().trim()];
    if (preset) {
      scopePath = preset.path;
      scopeGlob = preset.glob;
      scopeLabel = preset.label;
    } else {
      const valid = Object.keys(SCOPE_PRESETS).join(", ");
      return {
        text: `## ❌ Unknown scope "${params.scope}". Valid: ${valid}`,
        details: {},
        isError: true,
      };
    }
  }

  // ── Resolve search root — absolute paths pass through, relative join to ROOT ──
  const searchRoot = (() => {
    if (params.path) {
      // Absolute path (starts with drive letter like X: or X:/, or Unix root /)
      if (/^[A-Za-z]:[\/\\]/.test(params.path) || params.path.startsWith("/")) {
        // Normalize to forward slashes for rg
        return params.path.replace(/\\/g, "/");
      }
      return join(ROOT, params.path);
    }
    if (scopePath) return join(ROOT, scopePath);
    return ROOT;
  })();

  if (!existsSync(searchRoot)) {
    return {
      text: `## ❌ Path not found: \`${relative(ROOT, searchRoot) || searchRoot}\``,
      details: {},
      isError: true,
    };
  }

  // ── Determine mode ──
  const mode = params.json ? "json" : (params.mode ?? "smart");
  const contextLines = params.context ?? 0;

  // ── Build glob ──
  const globs: string[] = [];
  if (scopeGlob) globs.push(scopeGlob);
  if (params.glob) globs.push(params.glob);
  const glob = globs.length > 0 ? globs.join(",") : undefined;

  // ── For files/count modes, use specialized rg flags ──
  if (mode === "files" || mode === "count") {
    const rgResult = runRg({
      pattern: params.pattern,
      searchRoot,
      glob,
      ignoreCase: params.ignoreCase,
      fixed: params.fixed,
      maxCount: params.max ?? 500,
      mode: mode === "files" ? "files" : "count",
    });

    if (rgResult.code === 2) {
      return {
        text: `## ❌ rg error\n\n${rgResult.stderr.slice(0, 2000)}`,
        details: {},
        isError: true,
      };
    }

    if (rgResult.code === 1 || !rgResult.stdout) {
      // Diagnostic: include raw output in details for debugging
      const diag = { 
        pattern: params.pattern, 
        code: rgResult.code, 
        stdoutLen: (rgResult.stdout || "").length, 
        stderr: (rgResult.stderr || "").slice(0, 500),
        searchRoot: relative(ROOT, searchRoot) || searchRoot,
        elapsedMs: rgResult.elapsedMs,
      };
      // If there's stderr, show it
      const extra = rgResult.stderr ? `\n\n### rg stderr\n\`\`\`\n${rgResult.stderr.slice(0, 500)}\n\`\`\`` : "";
      return {
        text: `## ∅ No matches for \`${params.pattern}\`${scopeLabel ? ` in ${scopeLabel}` : ""}${extra}`,
        details: diag,
      };
    }

    if (mode === "files") {
      const fileList = rgResult.stdout.split("\n").filter(Boolean).map(toRelPath).sort();
      const groups: RgFileGroup[] = [];
      const areaMap = new Map<string, string[]>();
      for (const f of fileList) {
        const area = classifyArea(f);
        if (!areaMap.has(area)) areaMap.set(area, []);
        areaMap.get(area)!.push(f);
      }
      for (const [area, paths] of areaMap) {
        groups.push({ relPath: paths[0], area, matches: paths.map((p) => ({
          file: join(ROOT, p), relPath: p, line: 0, column: 0, content: "", matchText: "", contextBefore: [], contextAfter: [],
        })) });
      }

      const results: RgResults = {
        pattern: params.pattern,
        elapsedMs: rgResult.elapsedMs,
        totalMatches: fileList.length,
        totalFiles: fileList.length,
        files: fileList.map((p) => ({
          relPath: p, area: classifyArea(p),
          matches: [{ file: join(ROOT, p), relPath: p, line: 0, column: 0, content: "", matchText: "", contextBefore: [], contextAfter: [] }],
        })),
        allRelPaths: fileList,
      };

      // Count mode — just one match per file in count mode
      return { text: formatFiles(results, params.pattern, searchRoot, scopeLabel), details: results };
    } else {
      // Count mode — parse rg --count output
      const countLines = rgResult.stdout.split("\n").filter(Boolean);
      const fileCounts: { relPath: string; count: number; area: string }[] = [];
      for (const line of countLines) {
        // Format: "N:filepath" or "filepath:N" depending on platform
        const parts = line.split(":");
        if (parts.length >= 2) {
          const count = parseInt(parts[parts.length - 1], 10);
          const pathPart = parts.slice(0, -1).join(":");
          const relPath = toRelPath(pathPart);
          if (!isNaN(count)) fileCounts.push({ relPath, count, area: classifyArea(relPath) });
        }
      }
      fileCounts.sort((a, b) => b.count - a.count || a.relPath.localeCompare(b.relPath));

      const results: RgResults = {
        pattern: params.pattern,
        elapsedMs: rgResult.elapsedMs,
        totalMatches: fileCounts.reduce((s, f) => s + f.count, 0),
        totalFiles: fileCounts.length,
        files: fileCounts.map((f) => ({
          relPath: f.relPath, area: f.area,
          matches: Array(f.count).fill(null).map((_, i) => ({
            file: join(ROOT, f.relPath), relPath: f.relPath, line: 0, column: 0, content: "", matchText: "", contextBefore: [], contextAfter: [],
          })),
        })),
        allRelPaths: fileCounts.map((f) => f.relPath),
      };

      return { text: formatCount(results, params.pattern, searchRoot, scopeLabel), details: results };
    }
  }

  // ── For smart/content/json modes, use JSON output ──
  const maxCount = mode === "json" ? (params.max ?? 100) : (params.max ?? 200);
  const rgResult = runRg({
    pattern: params.pattern,
    searchRoot,
    glob,
    ignoreCase: params.ignoreCase,
    fixed: params.fixed,
    maxCount,
    contextLines: mode === "smart" ? 0 : contextLines,
    mode: "json",
  });

  if (rgResult.code === 2) {
    return {
      text: `## ❌ rg error\n\n${rgResult.stderr.slice(0, 2000)}`,
      details: {},
      isError: true,
    };
  }

  if (rgResult.code === 1 || !rgResult.stdout) {
    return {
      text: `## ∅ No matches for \`${params.pattern}\`${scopeLabel ? ` in ${scopeLabel}` : ""}`,
      details: { pattern: params.pattern, totalMatches: 0, totalFiles: 0, elapsedMs: rgResult.elapsedMs },
    };
  }

  // ── JSON passthrough mode ──
  if (mode === "json") {
    const matchCount = rgResult.stdout.split("\n").filter((l) => l.includes('"type":"match"')).length;
    return {
      text: rgResult.stdout.slice(0, 40000),
      details: { pattern: params.pattern, totalMatches: matchCount, elapsedMs: rgResult.elapsedMs, raw: true },
    };
  }

  // ── Parse structured matches ──
  const matches = parseRgJson(rgResult.stdout, mode === "content" ? contextLines : 0);
  const groups = groupMatches(matches);

  // Deduplicate matches within files (same line, same content)
  const deduped: RgFileGroup[] = groups.map((g) => {
    const seen = new Set<string>();
    const unique: RgMatch[] = [];
    for (const m of g.matches) {
      const key = `${m.line}:${m.content}`;
      if (!seen.has(key)) {
        seen.add(key);
        unique.push(m);
      }
    }
    return { ...g, matches: unique };
  }).filter((g) => g.matches.length > 0);

  const totalMatches = deduped.reduce((s, g) => s + g.matches.length, 0);

  const results: RgResults = {
    pattern: params.pattern,
    elapsedMs: rgResult.elapsedMs,
    totalMatches,
    totalFiles: deduped.length,
    files: deduped,
    allRelPaths: deduped.map((g) => g.relPath),
  };

  // ── Format ──
  let text: string;
  if (mode === "content") {
    text = formatContent(results, params.pattern, contextLines, searchRoot, scopeLabel);
  } else {
    // smart (default)
    text = formatSmart(results, params.pattern, searchRoot, scopeLabel);
  }

  return { text, details: results };
}

// ===========================================================================
// Tool registration
// ===========================================================================

export default function (pi: ExtensionAPI) {
  pi.registerTool({
    name: "rg",
    label: "Ripgrep",
    description:
      "Blazing-fast code search using ripgrep. Searches file contents by regex or literal string. " +
      "Automatically respects .gitignore. Shows results grouped by file with line numbers. " +
      "The go-to tool for finding where things are used, defined, or referenced in the codebase.",

    promptSnippet: "Search file contents instantly with ripgrep",
    promptGuidelines: [
      "Use rg when you need to find all references, definitions, or usages of a symbol, function, or pattern in the codebase.",
      "Default mode is 'smart' — gives you stats, area breakdown, top matches, and refinement suggestions without flooding context.",
      "Pass absolute paths directly: path:'X:/crates/ownership/' works, no need to relativize. Relative paths are joined to repo root.",
      "Use scope presets for fast repo-aware search: 'stdlib' (stdlib/*.kn), 'crates' (crates/*.rs), 'runtime' (runtime/*.{c,h}), 'blades' (blades/*.kn), 'benchmark', 'smoketest'.",
      "Use mode:'files' to just list matching files, mode:'count' for per-file hit counts sorted by frequency, mode:'content' for full match display with context.",
      "Specify glob to narrow results: '*.kn' for Kain files, '*.rs' for Rust files, '*.ts' for TypeScript.",
      "Use ignore-case:true for case-insensitive identifier search, fixed:true for literal strings with special regex chars.",
    ],

    parameters: Type.Object({
      pattern: Type.String({ description: "Regex pattern or literal string to search for." }),
      path: Type.Optional(Type.String({ description: "Search root directory. Absolute paths (X:/crates/) pass through as-is. Relative paths (crates/) resolve from repo root. Default: repo root." })),
      glob: Type.Optional(Type.String({ description: "File glob filter, e.g. '*.kn', '*.rs', '*.{ts,tsx}'." })),
      scope: Type.Optional(Type.Enum(
        {
          stdlib: "stdlib",
          crates: "crates",
          runtime: "runtime",
          blades: "blades",
          benchmark: "benchmark",
          smoketest: "smoketest",
          selfhost: "selfhost",
          agents: "agents",
          guides: "guides",
        },
        { description: "Repo-aware scope preset. Auto-sets path + glob. 'stdlib' = stdlib/*.kn, 'crates' = crates/*.rs, etc." },
      )),
      mode: Type.Optional(Type.Enum(
        {
          smart: "smart",
          files: "files",
          count: "count",
          content: "content",
        },
        {
          description:
            "Output mode. 'smart' (default) — stats + area breakdown + top matches. " +
            "'files' — just filenames. 'count' — per-file match counts. " +
            "'content' — traditional display with context lines.",
        },
      )),
      context: Type.Optional(Type.Number({ description: "Lines of context before AND after each match (default 0, max 5)." })),
      "ignore-case": Type.Optional(Type.Boolean({ description: "Case insensitive search (default false)." })),
      fixed: Type.Optional(Type.Boolean({ description: "Treat pattern as literal string, not regex (default false)." })),
      max: Type.Optional(Type.Number({ description: "Max total matches to return (default 200 for smart, 100 for json, 500 for files/count)." })),
      json: Type.Optional(Type.Boolean({ description: "Return raw rg JSON stream. Overrides mode." })),
    }),

    async execute(_toolCallId: string, params: any, _signal: AbortSignal, _onUpdate: any, _ctx: any) {
      try {
        if (!params.pattern) {
          return { content: [{ type: "text", text: "Provide a `pattern` to search for." }], details: {}, isError: true };
        }

        try {
          const result = doSearch(params);
          return {
            content: [{ type: "text", text: result.text }],
            details: result.details,
            isError: result.isError ?? false,
          };
        } catch (e: any) {
          return {
            content: [{ type: "text", text: `## ❌ doSearch threw\n\n\`\`\`\n${e.stack || e.message}\n\`\`\`` }],
            details: {},
            isError: true,
          };
        }
      } catch (e: any) {
        return {
          content: [{ type: "text", text: `Error: ${e.message}` }],
          details: {},
          isError: true,
        };
      }
    },
  });

  pi.on("session_start", async (_event, ctx) => {
    ctx.ui.notify("⚡ rg god-mode loaded — smart search with repo-aware scoping", "info");
  });
}
