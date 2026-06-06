/**
 * Kain Bazel Tools — Build system management
 *
 * Single router tool for all Bazel operations: compile targets, run tests,
 * manage the Bazel server lifecycle, sync native runtime builds, and check
 * binary freshness.
 *
 * Tool: kain_bazel
 *   action: build        — Build any Bazel target
 *   action: test         — Run any Bazel test target
 *   action: server       — Manage Bazel server lifecycle
 *   action: sync         — Check or sync native runtime builds
 *   action: binary_age   — Check binary staleness vs source
 *   action: freshness    — Full repo freshness audit
 *
 * Commands:
 *   /bazel               — Quick status overview
 *   /bazel-rebuild       — One-shot //:kain build
 */

import type { ExtensionAPI } from "@earendil-works/pi-coding-agent";
import { Type } from "typebox";
import { existsSync, statSync } from "node:fs";
import { join, resolve, dirname } from "node:url";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

// ===========================================================================
// Repo root detection
// ===========================================================================

function findRepoRoot(): string | null {
  let dir = process.cwd();
  for (let i = 0; i < 20; i++) {
    if (existsSync(join(dir, "AGENTS.md")) && existsSync(join(dir, "CATALOG.md"))) return dir;
    const parent = resolve(dir, "..");
    if (parent === dir) break;
    dir = parent;
  }
  return null;
}

const REPO_ROOT = findRepoRoot() ?? process.cwd();

// ===========================================================================
// Helpers
// ===========================================================================

function runCmd(cmd: string, args: string[], opts?: { cwd?: string; timeout?: number }): { stdout: string; stderr: string; code: number } {
  const result = spawnSync(cmd, args, {
    cwd: opts?.cwd ?? REPO_ROOT,
    encoding: "utf-8",
    timeout: opts?.timeout ?? 120_000,
    shell: true,
  });
  return {
    stdout: (result.stdout ?? "").trim(),
    stderr: (result.stderr ?? "").trim(),
    code: result.status ?? -1,
  };
}

function runBat(batPath: string): { stdout: string; stderr: string; code: number } {
  return runCmd("cmd.exe", ["/c", batPath]);
}

function runPy(script: string, args: string[]): { stdout: string; stderr: string; code: number } {
  return runCmd("py", ["-3", script, ...args]);
}

function getFileAgeMs(filePath: string): number | null {
  try {
    if (!existsSync(filePath)) return null;
    return Date.now() - statSync(filePath).mtimeMs;
  } catch {
    return null;
  }
}

function formatDuration(ms: number): string {
  const s = Math.floor(ms / 1000);
  if (s < 60) return `${s}s`;
  const m = Math.floor(s / 60);
  if (m < 60) return `${m}m ${s % 60}s`;
  const h = Math.floor(m / 60);
  if (h < 24) return `${h}h ${m % 60}m`;
  return `${Math.floor(h / 24)}d ${h % 24}h`;
}

function formatTimestamp(ts: Date): string {
  return ts.toISOString().replace("T", " ").slice(0, 19);
}

function getLastGitCommit(): { hash: string; date: string; message: string } | null {
  const hash = runCmd("git", ["log", "-1", "--format=%H"]);
  if (hash.code !== 0) return null;
  const date = runCmd("git", ["log", "-1", "--format=%cI"]);
  const msg = runCmd("git", ["log", "-1", "--format=%s"]);
  return { hash: hash.stdout.slice(0, 12), date: date.stdout, message: msg.stdout };
}

function getBinaryInfo(binary: string): { exists: boolean; ageMs: number | null; mtime: string | null } {
  const binPath = join(REPO_ROOT, ".kain/bin", binary);
  const age = getFileAgeMs(binPath);
  return { exists: age !== null, ageMs: age, mtime: age !== null ? formatTimestamp(new Date(Date.now() - age)) : null };
}

function checkBazelServer(): { alive: boolean; pid: string | null; outputBase: string | null } {
  const result = runCmd("bazel", ["info", "server_pid", "--config=dev"], { cwd: REPO_ROOT });
  if (result.code === 0 && /^\d+$/.test(result.stdout)) {
    const outBase = runCmd("bazel", ["info", "output_base", "--config=dev"], { cwd: REPO_ROOT });
    return { alive: true, pid: result.stdout, outputBase: outBase.code === 0 ? outBase.stdout : null };
  }
  return { alive: false, pid: null, outputBase: null };
}

// ===========================================================================
// Action handlers
// ===========================================================================

function actionBuild(target: string, config?: string, watch?: boolean) {
  let fullTarget = target;
  if (!watch && !target.includes("--config=")) fullTarget = `${target} --config=${config ?? "dev"}`;
  const start = Date.now();
  const result = runCmd("bazel", ["build", ...fullTarget.split(/\s+/).filter(Boolean)], { cwd: REPO_ROOT, timeout: 600_000 });
  const elapsed = Date.now() - start;
  if (result.code !== 0) {
    return {
      info: `Build failed after ${formatDuration(elapsed)} (exit: ${result.code})`,
      success: false,
      stderr: result.stderr.slice(0, 5000),
      stdout: result.stdout.slice(0, 2000),
    };
  }
  return { info: `✅ Build succeeded in ${formatDuration(elapsed)}`, success: true };
}

function actionTest(target: string, config?: string) {
  const fullTarget = target.includes("--config=") ? target : `${target} --config=${config ?? "dev"}`;
  const start = Date.now();
  const result = runCmd("bazel", ["test", ...fullTarget.split(/\s+/).filter(Boolean)], { cwd: REPO_ROOT, timeout: 600_000 });
  const elapsed = Date.now() - start;
  if (result.code !== 0) {
    return {
      info: `Tests failed after ${formatDuration(elapsed)} (exit: ${result.code})`,
      success: false,
      stderr: result.stderr.slice(0, 5000),
      stdout: result.stdout.slice(0, 2000),
    };
  }
  return { info: `✅ All tests passed in ${formatDuration(elapsed)}`, success: true };
}

function actionServer(action: string) {
  switch (action) {
    case "status": {
      const s = checkBazelServer();
      return s.alive
        ? { info: `✅ Server running (PID ${s.pid})${s.outputBase ? ` — output: ${s.outputBase}` : ""}`, alive: true, pid: s.pid }
        : { info: "❄️ Server cold — use `start` to warm it up", alive: false };
    }
    case "start": {
      const existing = checkBazelServer();
      if (existing.alive) return { info: `✅ Already running (PID ${existing.pid})`, alive: true, pid: existing.pid, started: false };
      const start = Date.now();
      runBat(join(REPO_ROOT, "tools/bazel/bazel_on.bat"));
      const elapsed = Date.now() - start;
      const after = checkBazelServer();
      if (after.alive) return { info: `✅ Started in ${formatDuration(elapsed)} (PID ${after.pid})`, alive: true, pid: after.pid, started: true };
      return { info: "⚠️ Start initiated but server not yet responsive", alive: false, started: true };
    }
    case "stop": {
      runBat(join(REPO_ROOT, "tools/bazel/bazel_off.bat"));
      const after = checkBazelServer();
      return after.alive
        ? { info: "⚠️ Server may still be running (PID ${after.pid})", alive: true }
        : { info: "✅ Server stopped", alive: false };
    }
    case "restart": {
      runBat(join(REPO_ROOT, "tools/bazel/bazel_off.bat"));
      const start = Date.now();
      runBat(join(REPO_ROOT, "tools/bazel/bazel_on.bat"));
      const elapsed = Date.now() - start;
      const after = checkBazelServer();
      return after.alive
        ? { info: `✅ Restarted in ${formatDuration(elapsed)} (PID ${after.pid})`, alive: true, pid: after.pid }
        : { info: "⚠️ Restart initiated, waiting for server...", alive: false };
    }
    default:
      return { info: `Unknown server action '${action}'` };
  }
}

function actionSync(mode: string) {
  if (mode === "check") {
    const nativeResult = runPy(join(REPO_ROOT, "tools/bazel/sync_native_runtime_builds.py"), ["--check"]);
    const statusResult = runPy(join(REPO_ROOT, "scripts/python/kain_bazel_sync.py"), ["status", "--json"]);
    const nativeOk = nativeResult.code === 0;
    const statusOk = statusResult.code === 0;
    const lines = ["## Sync Status", ""];
    lines.push(`Native runtime: ${nativeOk ? "✅ clean" : "⚠️ drift detected"}`);
    if (!nativeOk && nativeResult.stdout) lines.push(`\`${nativeResult.stdout.slice(0, 800)}\``);
    lines.push(`kain_bazel_sync: ${statusOk ? "✅ ok" : "⚠️ failed"}`);
    if (!statusOk && statusResult.stderr) lines.push(`\`${statusResult.stderr.slice(0, 800)}\``);
    return { info: lines.join("\n"), nativeOk, statusOk };
  }

  const start = Date.now();
  const result = runPy(join(REPO_ROOT, "tools/bazel/sync_native_runtime_builds.py"), []);
  const elapsed = Date.now() - start;
  if (result.code !== 0) return { info: `Sync failed after ${formatDuration(elapsed)}`, success: false, stderr: result.stderr.slice(0, 2000) };
  return { info: `✅ Sync completed in ${formatDuration(elapsed)}`, success: true };
}

function actionBinaryAge(binary: string) {
  const targets = binary === "both" || !binary ? ["kain.exe", "kn.exe"] : [`${binary}.exe`];
  const git = getLastGitCommit();
  const lines = [];
  if (git) lines.push(`Last source change: ${git.date} — ${git.hash} — ${git.message}`);
  for (const bin of targets) {
    const info = getBinaryInfo(bin);
    if (!info.exists) {
      lines.push(`${bin}: ❌ not found at .kain/bin/${bin}`);
      continue;
    }
    const age = formatDuration(info.ageMs!);
    if (git) {
      const sourceDate = new Date(git.date).getTime();
      const binDate = new Date(info.mtime!).getTime();
      lines.push(`${bin}: built ${info.mtime} (${age} ago) — ${sourceDate - binDate > 5000 ? "⚠️ STALE" : "✅ fresh"}`);
    } else {
      lines.push(`${bin}: built ${info.mtime} (${age} ago)`);
    }
  }
  return { info: lines.join("\n") };
}

function actionFreshness() {
  const git = getLastGitCommit();
  const server = checkBazelServer();
  const kainInfo = getBinaryInfo("kain.exe");
  const knInfo = getBinaryInfo("kn.exe");
  const syncResult = runPy(join(REPO_ROOT, "tools/bazel/sync_native_runtime_builds.py"), ["--check"]);
  const syncOk = syncResult.code === 0;

  const lines = ["# 🔍 Kain Repo Freshness", "", `Checked: ${formatTimestamp(new Date())}`, ""];
  lines.push("## 1. Last Source Change");
  lines.push(git ? `${git.date} — ${git.hash} — ${git.message}` : "❌ unknown");
  lines.push("");
  lines.push("## 2. Bazel Server");
  lines.push(server.alive ? `✅ PID ${server.pid}` : "❄️ cold");
  lines.push("");
  lines.push("## 3. Binary Freshness");
  for (const [name, info] of Object.entries({ kain: kainInfo, kn: knInfo })) {
    if (!info.exists) { lines.push(`${name}.exe: ❌ not found`); continue; }
    const age = formatDuration(info.ageMs!);
    if (git && new Date(info.mtime!).getTime() < new Date(git.date).getTime() - 5000) lines.push(`${name}.exe: ⚠️ STALE (${info.mtime}, ${age} ago)`);
    else lines.push(`${name}.exe: ✅ fresh (${info.mtime}, ${age} ago)`);
  }
  lines.push("");
  lines.push("## 4. Runtime Sync");
  lines.push(syncOk ? "✅ clean" : "⚠️ drift detected");
  lines.push("");
  const issues: string[] = [];
  if (!server.alive) issues.push("Start Bazel server");
  if (git && kainInfo.exists && new Date(kainInfo.mtime!).getTime() < new Date(git.date).getTime() - 5000) issues.push("Rebuild //:kain (binary stale)");
  if (!syncOk) issues.push("Run bazel_sync to fix drift");
  lines.push("## Summary");
  lines.push(issues.length === 0 ? "✅ All clear" : `⚠️ ${issues.map((i) => `\n  - ${i}`).join("")}`);
  return { info: lines.join("\n"), issues };
}

// ===========================================================================
// Tool + Commands
// ===========================================================================

const BAZEL_ON_BAT = join(REPO_ROOT, "tools/bazel/bazel_on.bat");
const BAZEL_OFF_BAT = join(REPO_ROOT, "tools/bazel/bazel_off.bat");

export default function (pi: ExtensionAPI) {
  // ── Tool: kain_bazel ─────────────────────────────────────────────────
  pi.registerTool({
    name: "kain_bazel",
    label: "Kain Bazel",
    description:
      "Build, test, and manage the Kain repo Build system — compile targets, run tests, " +
      "manage the Bazel server lifecycle, sync runtime builds, and check binary freshness. " +
      "6 actions cover the full build pipeline.",
    promptSnippet: "Build, test, and manage the Kain Bazel build system",
    promptGuidelines: [
      "Use kain_bazel for all Bazel operations: building the compiler, running smoke tests, managing server lifecycle, syncing runtime builds, and checking binary staleness.",
    ],
    parameters: Type.Object({
      action: Type.Enum(
        { build: "build", test: "test", server: "server", sync: "sync", binary_age: "binary_age", freshness: "freshness" },
        {
          description:
            "'build' — compile a Bazel target | " +
            "'test' — run tests | " +
            "'server' — manage server lifecycle | " +
            "'sync' — check/sync native runtime builds | " +
            "'binary_age' — check binary staleness | " +
            "'freshness' — full repo audit",
        },
      ),
      target: Type.Optional(Type.String({ description: "Bazel target for build/test (e.g. '//:kain', '//runtime:all', '//:developer_smoke_tests')." })),
      config: Type.Optional(Type.String({ description: "Config for build/test: 'dev' (default) or 'release'.", default: "dev" })),
      watch: Type.Optional(Type.Boolean({ description: "Omit --config flag (for targets that supply their own)." })),
      server_action: Type.Optional(Type.Enum({ status: "status", start: "start", stop: "stop", restart: "restart" }, { description: "Server action. Required when action='server'." })),
      sync_mode: Type.Optional(Type.Enum({ check: "check", sync: "sync" }, { description: "Sync mode: 'check' or 'sync'. Default 'check'." })),
      binary: Type.Optional(Type.Enum({ kain: "kain", kn: "kn", both: "both" }, { description: "Binary to inspect for binary_age (default 'both')." })),
    }),
    async execute(_toolCallId: string, params: any, _signal: AbortSignal, _onUpdate: any, _ctx: any) {
      try {
        let result: any;

        switch (params.action) {
          case "build":
            if (!params.target) return { content: [{ type: "text", text: "Provide `target` (e.g. '//:kain')." }], details: {}, isError: true };
            result = actionBuild(params.target, params.config, params.watch);
            break;

          case "test":
            if (!params.target) return { content: [{ type: "text", text: "Provide `target` (e.g. '//:developer_smoke_tests')." }], details: {}, isError: true };
            result = actionTest(params.target, params.config);
            break;

          case "server":
            if (!params.server_action) return { content: [{ type: "text", text: "Provide `server_action`: 'status', 'start', 'stop', or 'restart'." }], details: {}, isError: true };
            result = actionServer(params.server_action);
            break;

          case "sync":
            result = actionSync(params.sync_mode ?? "check");
            break;

          case "binary_age":
            result = actionBinaryAge(params.binary ?? "both");
            break;

          case "freshness":
            result = actionFreshness();
            break;

          default:
            return { content: [{ type: "text", text: `Unknown action '${params.action}'. Valid: build, test, server, sync, binary_age, freshness.` }], details: {}, isError: true };
        }

        const text = result.info ?? JSON.stringify(result);
        if (result.success === false) {
          let full = `## ❌ ${text}\n`;
          if (result.stderr) full += `\n### stderr\n\`\`\`\n${result.stderr}\n\`\`\`\n`;
          if (result.stdout) full += `\n### stdout\n\`\`\`\n${result.stdout}\n\`\`\`\n`;
          return { content: [{ type: "text", text: full }], details: result, isError: true };
        }

        return { content: [{ type: "text", text: text.startsWith("#") || text.startsWith("##") ? text : `## ${text}` }], details: result };
      } catch (e: any) {
        return { content: [{ type: "text", text: `Error: ${e.message}` }], details: {}, isError: true };
      }
    },
  });

  // ── Commands for the human ───────────────────────────────────────────
  pi.registerCommand("bazel", {
    description: "Quick Bazel status — server + binary freshness + last commit",
    handler: async (_args, ctx) => {
      const server = checkBazelServer();
      const git = getLastGitCommit();
      const kain = getBinaryInfo("kain.exe");
      const parts = [
        server.alive ? `Server: ✅ PID ${server.pid}` : "Server: ❄️ cold",
        git ? `Commit: ${git.hash}` : "",
        kain.exists ? `kain.exe: ${kain.mtime}` : "kain.exe: not found",
      ];
      ctx.ui.notify(parts.filter(Boolean).join(" • "), "info");
    },
  });

  pi.registerCommand("bazel-rebuild", {
    description: "Build //:kain --config=dev with notification",
    handler: async (_args, ctx) => {
      ctx.ui.notify("Building //:kain --config=dev...", "info");
      const start = Date.now();
      const result = runCmd("bazel", ["build", "//:kain", "--config=dev"], { cwd: REPO_ROOT });
      const elapsed = Date.now() - start;
      ctx.ui.notify(result.code === 0 ? `✅ Built in ${formatDuration(elapsed)}` : `❌ Failed in ${formatDuration(elapsed)}`, result.code === 0 ? "info" : "error");
    },
  });

  pi.on("session_start", async (_event, ctx) => {
    ctx.ui.notify("⚙️ Kain Bazel tools loaded — 6 actions in 1 router", "info");
  });
}
