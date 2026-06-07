/**
 * git — Repository operations router
 *
 * Tool: git
 *   action: status    — Branch + dirty state + last commit
 *   action: diff      — Show diff + recent history per touched file
 *   action: log       — Commit history, optionally scoped to a path
 *   action: recent    — Recent activity feed grouped by day with area breakdown
 *
 * Commands:
 *   /git         — Quick status snapshot
 *   /git-today   — Recent activity in the last 24h
 */

import type { ExtensionAPI } from "@earendil-works/pi-coding-agent";
import { Type } from "typebox";
import { existsSync } from "node:fs";
import { join, resolve, relative } from "node:path";
import { spawnSync } from "node:child_process";

// ===========================================================================
// Repo root + git repo detection
// ===========================================================================

const REPO_ROOT = (() => {
  let dir = process.cwd();
  for (let i = 0; i < 20; i++) {
    if (existsSync(join(dir, "AGENTS.md"))) return dir;
    const p = resolve(dir, "..");
    if (p === dir) break;
    dir = p;
  }
  return process.cwd();
})();

function isGitRepo(cwd: string = REPO_ROOT): boolean {
  const r = spawnSync("git", ["rev-parse", "--show-toplevel"], { cwd, encoding: "utf-8", shell: false, windowsHide: true });
  return r.status === 0;
}

// ===========================================================================
// Helpers
// ===========================================================================

function runGit(args: string[], opts?: { cwd?: string; timeout?: number; maxBuffer?: number }): { stdout: string; stderr: string; code: number } {
  const result = spawnSync("git", args, {
    cwd: opts?.cwd ?? REPO_ROOT,
    encoding: "utf-8",
    timeout: opts?.timeout ?? 60_000,
    maxBuffer: opts?.maxBuffer ?? 10 * 1024 * 1024,
    // shell:false is required: with shell:true, cmd.exe on Windows would
    // expand `%cI` in --format strings as a variable, breaking log/recent.
    shell: false,
    windowsHide: true,
  });
  return {
    stdout: (result.stdout ?? "").replace(/\r\n/g, "\n").replace(/\n+$/, ""),
    stderr: (result.stderr ?? "").replace(/\r\n/g, "\n").trim(),
    code: result.status ?? -1,
  };
}

const KNOWN_AREAS = ["stdlib", "crates", "runtime", "blades", "benchmark", "smoketest", "src", ".agents", "guides", "docs", "attrition", "z3", "release"];
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

function classifyArea(filePath: string): string {
  const rel = relative(REPO_ROOT, filePath).replace(/\\/g, "/");
  if (rel.startsWith("..")) return "external";
  const top = rel.split("/")[0].toLowerCase();
  return KNOWN_AREAS.includes(top) ? top : "root";
}

function areaIcon(area: string): string {
  return AREA_ICONS[area] ?? "📁";
}

function truncate(text: string, maxLines: number, maxBytes: number): { content: string; truncated: boolean; totalLines: number } {
  const lines = text.split("\n");
  let out = lines.slice(0, maxLines).join("\n");
  let truncated = lines.length > maxLines;
  if (out.length > maxBytes) {
    out = out.slice(0, maxBytes);
    truncated = true;
  }
  return { content: out, truncated, totalLines: lines.length };
}

function formatRelative(isoDate: string, now: number = Date.now()): string {
  const t = new Date(isoDate).getTime();
  if (Number.isNaN(t)) return isoDate;
  const diff = now - t;
  const s = Math.floor(diff / 1000);
  if (s < 60) return `${s}s ago`;
  const m = Math.floor(s / 60);
  if (m < 60) return `${m}m ago`;
  const h = Math.floor(m / 60);
  if (h < 24) return `${h}h ago`;
  const d = Math.floor(h / 24);
  if (d < 30) return `${d}d ago`;
  const mo = Math.floor(d / 30);
  if (mo < 12) return `${mo}mo ago`;
  return `${Math.floor(mo / 12)}y ago`;
}

function formatStamp(isoDate: string): string {
  // Use the original ISO string slice — preserves the committer's local
  // timezone offset (which equals the user's wall clock) rather than
  // converting to UTC.  Input: "2026-06-06T22:05:32-04:00" → "2026-06-06 22:05"
  const m = isoDate.match(/^(\d{4}-\d{2}-\d{2})T(\d{2}:\d{2})/);
  if (m) return `${m[1]} ${m[2]}`;
  return isoDate;
}

function shortHash(hash: string): string {
  return hash.slice(0, 8);
}

// Parse `git log --name-only --pretty=format:"..."` output into commits with file lists.
// Each commit is "<header>\n<files...>\n\n" with header containing the unique separator.
function parseCommitsWithFiles(raw: string, headerPrefix: string): { header: string; files: string[] }[] {
  const out: { header: string; files: string[] }[] = [];
  const blocks = raw.split(/\n(?=\S)/); // rough split on new "block"
  for (const block of blocks) {
    const lines = block.split("\n");
    const headerLine = lines.find((l) => l.startsWith(headerPrefix));
    if (!headerLine) continue;
    const files = lines
      .filter((l) => l && !l.startsWith(headerPrefix) && !l.startsWith("===END==="))
      .map((l) => l.trim())
      .filter(Boolean);
    out.push({ header: headerLine.slice(headerPrefix.length), files });
  }
  return out;
}

// ===========================================================================
// Action: status
// ===========================================================================

function actionStatus(): { info: string; details: any } {
  const branch = runGit(["branch", "--show-current"]);
  const head = runGit(["log", "-1", "--format=%H%n%cI%n%an%n%s"]);
  const dirty = runGit(["status", "--porcelain"]);
  const aheadBehind = runGit(["rev-list", "--left-right", "--count", "HEAD...@{u}"]);

  const lines: string[] = ["# 📊 Git Status", ""];

  // Branch
  const br = branch.code === 0 ? branch.stdout || "(detached)" : "(unknown)";
  lines.push(`**Branch:** \`${br}\``);

  // Last commit
  if (head.code === 0 && head.stdout) {
    const [hash, date, author, subject] = head.stdout.split("\n");
    lines.push(`**Last commit:** \`${shortHash(hash)}\` — ${formatStamp(date)} (${formatRelative(date)})`);
    lines.push(`  → ${subject} — *${author}*`);
  } else {
    lines.push("**Last commit:** (none — empty repo?)");
  }

  // Ahead/behind upstream
  if (aheadBehind.code === 0 && aheadBehind.stdout) {
    const [ahead, behind] = aheadBehind.stdout.split(/\s+/);
    const ab: string[] = [];
    if (Number(ahead) > 0) ab.push(`↑${ahead} ahead`);
    if (Number(behind) > 0) ab.push(`↓${behind} behind`);
    if (ab.length) lines.push(`**Upstream:** ${ab.join(" / ")}`);
  }

  // Dirty state
  if (dirty.code === 0) {
    const files = dirty.stdout.split("\n").filter(Boolean);
    const staged: string[] = [];
    const unstaged: string[] = [];
    const untracked: string[] = [];
    for (const line of files) {
      const x = line[0];
      const y = line[1];
      const path = line.slice(3);
      if (x === "?" && y === "?") untracked.push(path);
      else {
        if (x !== " ") staged.push(`${x} ${path}`);
        if (y !== " ") unstaged.push(`${y} ${path}`);
      }
    }
    lines.push("");
    lines.push("**Working tree:**");
    if (staged.length === 0 && unstaged.length === 0 && untracked.length === 0) {
      lines.push("  ✅ clean");
    } else {
      if (staged.length) lines.push(`  📥 staged: ${staged.length}`);
      if (unstaged.length) lines.push(`  📝 unstaged: ${unstaged.length}`);
      if (untracked.length) lines.push(`  ❓ untracked: ${untracked.length}`);
      const sample = [...staged, ...unstaged, ...untracked].slice(0, 8);
      lines.push("  " + sample.map((s) => `\`${s}\``).join(", "));
      if (staged.length + unstaged.length + untracked.length > 8) {
        lines.push(`  …and ${staged.length + unstaged.length + untracked.length - 8} more`);
      }
    }
  }

  return {
    info: lines.join("\n"),
    details: {
      branch: br,
      head: head.stdout,
      stagedCount: dirty.stdout.split("\n").filter((l) => l[0] !== " " && l[0] !== "?").length,
      unstagedCount: dirty.stdout.split("\n").filter((l) => l[1] !== " " && l[1] !== "?").length,
      untrackedCount: dirty.stdout.split("\n").filter((l) => l.startsWith("??")).length,
    },
  };
}

// ===========================================================================
// Action: diff — showpiece: diff + recent history per touched file
// ===========================================================================

interface FileHistoryEntry {
  hash: string;
  date: string;
  author: string;
  subject: string;
}

function getFileHistory(file: string, limit: number = 5): FileHistoryEntry[] {
  const rel = relative(REPO_ROOT, file).replace(/\\/g, "/");
  const args = ["log", `-n${limit}`, "--follow", "--format=%H%n%cI%n%an%n%s", "--", rel];
  const result = runGit(args);
  if (result.code !== 0 || !result.stdout) return [];

  const entries: FileHistoryEntry[] = [];
  // Each commit is 4 lines: hash, date, author, subject
  const lines = result.stdout.split("\n");
  for (let i = 0; i + 3 < lines.length; i += 4) {
    entries.push({
      hash: shortHash(lines[i]),
      date: lines[i + 1],
      author: lines[i + 2],
      subject: lines[i + 3],
    });
  }
  return entries;
}

function actionDiff(params: { path?: string; staged?: boolean; against?: string; history_per_file?: number; context?: number }) {
  const path = params.path;
  const staged = params.staged ?? false;
  const against = params.against;
  const historyLimit = Math.max(0, Math.min(params.history_per_file ?? 3, 10));
  const contextLines = Math.max(0, Math.min(params.context ?? 3, 10));

  // Build diff command
  const diffArgs: string[] = ["diff"];
  if (against) {
    diffArgs.push(against);
    if (staged) diffArgs.push("--staged");
  } else if (staged) {
    diffArgs.push("--staged");
  }
  if (contextLines > 0) diffArgs.push(`--unified=${contextLines}`);
  if (path) diffArgs.push("--", path);

  const diffResult = runGit(diffArgs, { maxBuffer: 20 * 1024 * 1024 });
  if (diffResult.code !== 0) {
    return { info: `Diff failed: ${diffResult.stderr || "unknown error"}`, success: false };
  }

  // Get list of touched files (parallel call)
  const nameArgs: string[] = ["diff", "--name-only"];
  if (against) {
    nameArgs.push(against);
    if (staged) nameArgs.push("--staged");
  } else if (staged) {
    nameArgs.push("--staged");
  }
  if (path) nameArgs.push("--", path);
  const nameResult = runGit(nameArgs);

  const files = nameResult.code === 0 && nameResult.stdout ? nameResult.stdout.split("\n").filter(Boolean) : [];

  // Build header
  const scopeParts: string[] = [];
  if (path) scopeParts.push(`path: \`${path}\``);
  if (against) scopeParts.push(`vs \`${against}\``);
  if (staged) scopeParts.push("staged");
  const scope = scopeParts.length ? ` (${scopeParts.join(", ")})` : "";
  const lines: string[] = [`# 📝 Git Diff${scope}`, ""];

  if (!diffResult.stdout || files.length === 0) {
    lines.push("✅ No changes.");
    return { info: lines.join("\n"), details: { filesChanged: 0, files: [] } };
  }

  lines.push(`**Files changed:** ${files.length}`);
  lines.push("");

  // Truncate the diff content
  const MAX_DIFF_LINES = 400;
  const MAX_DIFF_BYTES = 80 * 1024;
  const trunc = truncate(diffResult.stdout, MAX_DIFF_LINES, MAX_DIFF_BYTES);

  lines.push("## Diff");
  lines.push("```diff");
  lines.push(trunc.content);
  lines.push("```");
  if (trunc.truncated) {
    lines.push("");
    lines.push(`*[Diff truncated: showing ${Math.min(trunc.totalLines, MAX_DIFF_LINES)} of ${trunc.totalLines} lines. Run \`git diff\` for the full output.]*`);
  }
  lines.push("");

  // Per-file history
  if (historyLimit > 0 && files.length > 0) {
    lines.push("## 📚 Recent history per file");
    lines.push("");
    for (const f of files) {
      const area = classifyArea(f);
      const icon = areaIcon(area);
      const hist = getFileHistory(f, historyLimit);
      lines.push(`### ${icon} \`${f}\` (${area})`);
      if (hist.length === 0) {
        lines.push("  *(no history — new file?)*");
      } else {
        for (const h of hist) {
          lines.push(`  - \`${h.hash}\` ${formatStamp(h.date)} *(${formatRelative(h.date)})* — ${h.subject} — *${h.author}*`);
        }
      }
      lines.push("");
    }
  }

  return {
    info: lines.join("\n"),
    details: { filesChanged: files.length, files, staged, against: against ?? null, path: path ?? null },
  };
}

// ===========================================================================
// Action: log — commit history, optionally scoped
// ===========================================================================

function actionLog(params: { path?: string; limit?: number; author?: string; since?: string }) {
  const limit = Math.max(1, Math.min(params.limit ?? 20, 200));
  const args: string[] = ["log", `-n${limit}`, "--format=%H|%cI|%an|%s"];
  if (params.author) args.push(`--author=${params.author}`);
  if (params.since) args.push(`--since=${params.since}`);
  if (params.path) args.push("--", params.path);

  const result = runGit(args);
  if (result.code !== 0) {
    return { info: `Log failed: ${result.stderr || "unknown error"}`, success: false };
  }
  if (!result.stdout) {
    return { info: "No commits found.", details: { count: 0, commits: [] } };
  }

  const commits = result.stdout.split("\n").filter(Boolean).map((line) => {
    const [hash, date, author, ...rest] = line.split("|");
    return { hash: shortHash(hash), date, author, subject: rest.join("|") };
  });

  const scope = params.path ? ` — path: \`${params.path}\`` : "";
  const filterParts: string[] = [];
  if (params.author) filterParts.push(`author: \`${params.author}\``);
  if (params.since) filterParts.push(`since: \`${params.since}\``);
  const filters = filterParts.length ? ` (${filterParts.join(", ")})` : "";

  const lines: string[] = [`# 📜 Git Log${scope}${filters}`, ""];
  lines.push(`**Showing ${commits.length} commit(s):**`);
  lines.push("");

  for (const c of commits) {
    lines.push(`- \`${c.hash}\` ${formatStamp(c.date)} *(${formatRelative(c.date)})* — ${c.subject}`);
    lines.push(`  *${c.author}*`);
  }

  return {
    info: lines.join("\n"),
    details: { count: commits.length, path: params.path ?? null, author: params.author ?? null, since: params.since ?? null, commits },
  };
}

// ===========================================================================
// Action: recent — activity feed grouped by day with area breakdown
// ===========================================================================

function actionRecent(params: { limit?: number; since?: string; area?: string }) {
  const limit = Math.max(1, Math.min(params.limit ?? 30, 200));
  const area = params.area?.toLowerCase();

  // Get commit metadata
  const logArgs: string[] = ["log", `-n${limit}`, "--format=%H|%cI|%an|%s"];
  if (params.since) logArgs.push(`--since=${params.since}`);
  const logResult = runGit(logArgs);
  if (logResult.code !== 0) {
    return { info: `Log failed: ${logResult.stderr || "unknown error"}`, success: false };
  }
  if (!logResult.stdout) {
    return { info: "No recent commits.", details: { count: 0, days: [] } };
  }

  const commits = logResult.stdout.split("\n").filter(Boolean).map((line) => {
    const [hash, date, author, ...rest] = line.split("|");
    return { hash, short: shortHash(hash), date, author, subject: rest.join("|"), files: [] as string[] };
  });

  // Per-commit file lists (for area breakdown)
  for (const c of commits) {
    const showResult = runGit(["show", "--name-only", "--format=", c.hash]);
    if (showResult.code === 0 && showResult.stdout) {
      c.files = showResult.stdout.split("\n").filter(Boolean);
    }
  }

  // Group by day
  const byDay = new Map<string, typeof commits>();
  for (const c of commits) {
    const day = c.date.slice(0, 10);
    if (!byDay.has(day)) byDay.set(day, []);
    byDay.get(day)!.push(c);
  }

  // Area aggregation across all commits
  const areaCount = new Map<string, number>();
  for (const c of commits) {
    const seen = new Set<string>();
    for (const f of c.files) {
      const a = classifyArea(f);
      seen.add(a);
    }
    for (const a of seen) {
      areaCount.set(a, (areaCount.get(a) ?? 0) + 1);
    }
  }

  // Optional area filter — drop commits with no file in that area
  const filtered = area
    ? commits.filter((c) => c.files.some((f) => classifyArea(f) === area))
    : commits;
  if (area && filtered.length === 0) {
    return { info: `No commits in the last ${limit} touched area \`${area}\`.`, details: { count: 0, area } };
  }

  // Re-group by day after filter
  const filteredByDay = new Map<string, typeof commits>();
  for (const c of filtered) {
    const day = c.date.slice(0, 10);
    if (!filteredByDay.has(day)) filteredByDay.set(day, []);
    filteredByDay.get(day)!.push(c);
  }

  const lines: string[] = ["# 🕒 Recent Activity"];
  if (area) lines.push(` *(filtered to area: \`${area}\`)*`);
  lines.push("");
  lines.push(`**Commits shown:** ${filtered.length} of ${commits.length} scanned (limit ${limit})`);

  if (!area) {
    lines.push("");
    lines.push("## Area activity (top dirs touched)");
    const sortedAreas = [...areaCount.entries()].sort((a, b) => b[1] - a[1]).slice(0, 8);
    for (const [a, n] of sortedAreas) {
      lines.push(`  ${areaIcon(a)} \`${a}\` — ${n} commit(s)`);
    }
  }
  lines.push("");

  for (const [day, dayCommits] of [...filteredByDay.entries()].sort((a, b) => b[0].localeCompare(a[0]))) {
    lines.push(`## 📅 ${day}`);
    for (const c of dayCommits) {
      // Aggregate touched areas for this commit
      const areas = [...new Set(c.files.map((f) => classifyArea(f)))]
        .filter((a) => a !== "external")
        .slice(0, 4)
        .map((a) => `${areaIcon(a)} ${a}`);
      const areaTag = areas.length ? ` — ${areas.join(", ")}` : "";
      lines.push(`- \`${c.short}\` ${c.date.slice(11, 16)} — ${c.subject}${areaTag}`);
      lines.push(`  *${c.author}*`);
    }
    lines.push("");
  }

  return {
    info: lines.join("\n").trimEnd(),
    details: {
      scanned: commits.length,
      shown: filtered.length,
      area: area ?? null,
      days: [...filteredByDay.keys()].sort(),
      areaBreakdown: Object.fromEntries(areaCount),
    },
  };
}

// ===========================================================================
// Tool: git
// ===========================================================================

export default function (pi: ExtensionAPI) {
  pi.registerTool({
    name: "git",
    label: "Git",
    description:
      "Repository operations router — git status, diff (with per-file history context), " +
      "commit log (scoped to file or folder), and a recent activity feed grouped by day " +
      "with area breakdown. All actions operate on the current git working tree.",
    promptSnippet: "Repository operations: status, diff+history, log, recent activity",
    promptGuidelines: [
      "Use git for any git repository operations — checking status, showing diffs with context, browsing history, or seeing recent activity.",
      "Use git action:'diff' for the 'what changed + why has it been changing' question — it pairs each diff hunk with recent history on the touched files.",
      "Use git action:'recent' to see what's been changing across the repo, grouped by day with area breakdown (stdlib, crates, runtime, blades, …).",
      "Use git action:'log' path:'X:/crates/foo/' to browse commit history scoped to a specific file or folder.",
      "Use git action:'status' for a quick branch + dirty + last-commit snapshot.",
    ],
    parameters: Type.Object({
      action: Type.Enum(
        { status: "status", diff: "diff", log: "log", recent: "recent" },
        {
          description:
            "'status' — branch + dirty + last commit | " +
            "'diff' — show diff with per-file history | " +
            "'log' — commit history (optionally scoped to a path) | " +
            "'recent' — activity feed grouped by day with area breakdown",
        },
      ),

      // Diff params
      path: Type.Optional(Type.String({ description: "Path scope (file or folder), relative to repo root or absolute. Used by 'diff' and 'log'." })),
      staged: Type.Optional(Type.Boolean({ description: "For 'diff': show staged changes instead of unstaged. Default false." })),
      against: Type.Optional(Type.String({ description: "For 'diff': show diff against this ref (branch, tag, commit). E.g. 'main', 'HEAD~3'." })),
      history_per_file: Type.Optional(Type.Number({ description: "For 'diff': recent history entries to show per touched file. 0 to disable. Default 3, max 10." })),
      context: Type.Optional(Type.Number({ description: "For 'diff': context lines around each hunk. Default 3, max 10." })),

      // Log params
      limit: Type.Optional(Type.Number({ description: "For 'log' and 'recent': max commits to show. Default 20 (log) or 30 (recent). Max 200." })),
      author: Type.Optional(Type.String({ description: "For 'log': filter by author (substring match)." })),
      since: Type.Optional(Type.String({ description: "For 'log' and 'recent': git date filter. E.g. '2 weeks ago', '2026-01-01', '1 month ago'." })),

      // Recent params
      area: Type.Optional(Type.String({ description: "For 'recent': filter to a top-level area like 'crates', 'stdlib', 'runtime', 'blades', 'benchmark'." })),
    }),

    async execute(_toolCallId: string, params: any, _signal: AbortSignal, _onUpdate: any, _ctx: any) {
      try {
        if (!isGitRepo()) {
          return {
            content: [{ type: "text", text: `## ❌ Not a git repository\n\nDetected repo root: \`${REPO_ROOT}\`\n\nRun from inside a git working tree.` }],
            details: { repoRoot: REPO_ROOT, isGitRepo: false },
            isError: true,
          };
        }

        let result: { info: string; details?: any; success?: boolean };
        switch (params.action) {
          case "status":
            result = actionStatus();
            break;
          case "diff":
            result = actionDiff({
              path: params.path,
              staged: params.staged,
              against: params.against,
              history_per_file: params.history_per_file,
              context: params.context,
            });
            break;
          case "log":
            result = actionLog({
              path: params.path,
              limit: params.limit,
              author: params.author,
              since: params.since,
            });
            break;
          case "recent":
            result = actionRecent({
              limit: params.limit,
              since: params.since,
              area: params.area,
            });
            break;
          default:
            return {
              content: [{ type: "text", text: `Unknown action '${params.action}'. Valid: status, diff, log, recent.` }],
              details: {},
              isError: true,
            };
        }

        const text = result.info ?? JSON.stringify(result.details ?? {});
        if (result.success === false) {
          return { content: [{ type: "text", text: `## ❌ ${text}` }], details: result.details ?? {}, isError: true };
        }
        return { content: [{ type: "text", text }], details: result.details ?? {} };
      } catch (e: any) {
        return { content: [{ type: "text", text: `Error: ${e.message}` }], details: {}, isError: true };
      }
    },
  });

  // ── Commands for the human ───────────────────────────────────────────
  pi.registerCommand("git", {
    description: "Quick git status — branch + last commit + dirty state",
    handler: async (_args, ctx) => {
      if (!isGitRepo()) {
        ctx.ui.notify(`Not a git repo (root: ${REPO_ROOT})`, "warning");
        return;
      }
      const branch = runGit(["branch", "--show-current"]);
      const head = runGit(["log", "-1", "--format=%s"]);
      const dirty = runGit(["status", "--porcelain"]);
      const count = dirty.stdout.split("\n").filter(Boolean).length;
      const br = branch.code === 0 ? branch.stdout || "(detached)" : "(unknown)";
      const summary = head.code === 0 ? head.stdout.split("\n")[0] : "";
      ctx.ui.notify(`${br} • ${count} dirty • ${summary}`, count === 0 ? "info" : "warning");
    },
  });

  pi.registerCommand("git-today", {
    description: "Recent commits in the last 24h",
    handler: async (_args, ctx) => {
      if (!isGitRepo()) {
        ctx.ui.notify(`Not a git repo (root: ${REPO_ROOT})`, "warning");
        return;
      }
      const r = actionRecent({ limit: 50, since: "24 hours ago" });
      ctx.ui.notify(`Recent activity (24h):\n${r.info.split("\n").slice(0, 8).join("\n")}`, "info");
    },
  });

  pi.on("session_start", async (_event, ctx) => {
    ctx.ui.notify("🌿 Git tools loaded — status, diff+history, log, recent (4 actions in 1 router)", "info");
  });
}
