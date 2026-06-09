/**
 * kain-sync — Update the Kain binary from Bazel's output base
 *
 * Single tool + command that finds the latest Bazel-built kain.exe/kn.exe
 * and copies it to $KAIN_HOME/bin/ (preferred) or ~/.kain/bin/ (fallback).
 * No PowerShell escaping nightmares.
 *
 * Tool: kain_sync_binary
 *   Syncs kain + kn binaries from Bazel output to KAIN_HOME/bin/
 *
 * Command: /kain-sync
 *   Same thing but callable interactively
 */

import type { ExtensionAPI } from "@earendil-works/pi-coding-agent";
import { Type } from "typebox";
import { existsSync, copyFileSync, renameSync, mkdirSync, readdirSync, statSync } from "node:fs";
import { join, resolve, sep } from "node:path";
import { spawnSync } from "node:child_process";

// ===========================================================================
// Repo root
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

// ===========================================================================
// Helpers
// ===========================================================================

function run(cmd: string, args: string[], opts?: { cwd?: string; timeout?: number }): { stdout: string; stderr: string; code: number } {
  const result = spawnSync(cmd, args, {
    cwd: opts?.cwd ?? REPO_ROOT,
    encoding: "utf-8",
    timeout: opts?.timeout ?? 60_000,
    shell: false,
  });
  return {
    stdout: (result.stdout ?? "").trim(),
    stderr: (result.stderr ?? "").trim(),
    code: result.status ?? -1,
  };
}

/** Get the Bazel output base directory */
function getOutputBase(): string | null {
  const result = run("bazel", ["info", "output_base", "--config=dev"], { timeout: 30_000 });
  if (result.code !== 0) return null;
  // Strip ANSI escape codes (bazel wraps info output)
  const clean = result.stdout.replace(/\u001b\[[0-9;]*m/g, "").trim();
  return clean || null;
}

/** Find the bazel-built binary path from the output base */
function findBazelBinary(outputBase: string, binary: "kain" | "kn"): string | null {
  // Bazel output layout: {outputBase}/execroot/_main/bazel-out/x64_windows-dbg/bin/crates/cli/{binary}.exe
  const candidate = join(outputBase, "execroot", "_main", "bazel-out", "x64_windows-dbg", "bin", "crates", "cli", `${binary}.exe`);
  if (existsSync(candidate)) return candidate;

  // Fallback: search the bin dir for the most recent
  const binDir = join(outputBase, "execroot", "_main", "bazel-out", "x64_windows-dbg", "bin", "crates", "cli");
  if (!existsSync(binDir)) return null;

  try {
    const files = readdirSync(binDir);
    const matches = files
      .filter(f => f === `${binary}.exe`)
      .map(f => join(binDir, f))
      .filter(existsSync);
    if (matches.length > 0) return matches[0];
  } catch { /* ignore */ }

  return null;
}

/** Build the binary via Bazel */
function buildBinary(): { success: boolean; output: string } {
  const result = run("bazel", ["build", "//:kain", "--config=dev"], { timeout: 600_000 });
  if (result.code !== 0) {
    return {
      success: false,
      output: `Build failed (exit ${result.code}):\n${result.stderr.slice(0, 3000)}`,
    };
  }
  return { success: true, output: result.stdout.slice(0, 2000) };
}

/** Resolve Kain home directory — respects KAIN_HOME env var, falls back to ~/.kain */
function kainHomeDir(): string {
  if (process.env.KAIN_HOME) return process.env.KAIN_HOME;
  const userHome = process.env.USERPROFILE || process.env.HOME || "C:\\Users\\zenta";
  return join(userHome, ".kain");
}

/** Sync a binary from source to target with backup */
function syncBinary(source: string, binary: "kain" | "kn"): { success: boolean; target: string; message: string } {
  const kainHome = kainHomeDir();
  const binDir = join(kainHome, "bin");
  const target = join(binDir, `${binary}.exe`);
  const bak = join(binDir, `${binary}.exe.bak`);

  // Ensure bin dir exists
  mkdirSync(binDir, { recursive: true });

  try {
    // Backup existing
    if (existsSync(target)) {
      // Remove old backup if it exists
      try { renameSync(target, bak); } catch { /* best-effort backup */ }
    }

    // Copy new binary
    copyFileSync(source, target);
    const size = statSync(target).size;

    return {
      success: true,
      target,
      message: `✅ ${binary}.exe syncd (${(size / 1024 / 1024).toFixed(1)} MB)\n  From: ${source}\n  To:   ${target}`,
    };
  } catch (e: any) {
    return {
      success: false,
      target: target,
      message: `❌ Failed to copy ${binary}.exe: ${e.message}`,
    };
  }
}

/** Format duration */
function fmtDuration(ms: number): string {
  if (ms < 1000) return `${ms}ms`;
  const s = Math.floor(ms / 1000);
  if (s < 60) return `${s}s`;
  return `${Math.floor(s / 60)}m ${s % 60}s`;
}

/** Format a timestamp nicely */
function fmtTime(date: Date): string {
  return date.toLocaleString("en-US", {
    month: "short", day: "numeric",
    hour: "2-digit", minute: "2-digit", second: "2-digit",
    timeZoneName: "short",
  });
}

/** Format bytes human-readable */
function fmtBytes(bytes: number): string {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
}

// ===========================================================================
// Status logic
// ===========================================================================

function gatherStatus(): {
  serverAlive: boolean;
  outputBase: string | null;
  bazelBinary: { exists: boolean; path: string | null; mtime: Date | null; age: string | null };
  activeBinary: { exists: boolean; path: string; mtime: Date | null; age: string | null; size: string | null };
  bakBinary: { exists: boolean; age: string | null };
  git: { commit: string | null; date: string | null; message: string | null };
  syncStamp: any;
} {
  const kainHome = kainHomeDir();
  const activePath = join(kainHome, "bin", "kain.exe");
  const bakPath = join(kainHome, "bin", "kain.exe.bak");
  const stampPath = join(kainHome, "state", "state", "kain_sync_stamp.json");

  // Bazel server
  const serverResult = run("bazel", ["info", "server_pid", "--config=dev"], { timeout: 15_000 });
  const serverAlive = serverResult.code === 0 && /^\d+$/.test(serverResult.stdout.trim());

  // Output base
  let outputBase: string | null = null;
  if (serverAlive) {
    const obResult = run("bazel", ["info", "output_base", "--config=dev"], { timeout: 15_000 });
    if (obResult.code === 0) {
      outputBase = obResult.stdout.replace(/\u001b\[[0-9;]*m/g, "").trim();
    }
  }

  // Bazel-built binary
  let bazelBinPath: string | null = null;
  let bazelBinMtime: Date | null = null;
  if (outputBase) {
    bazelBinPath = findBazelBinary(outputBase, "kain");
    if (bazelBinPath) {
      try { bazelBinMtime = statSync(bazelBinPath).mtime; } catch {}
    }
  }

  // Active binary
  let activeMtime: Date | null = null;
  let activeSize: string | null = null;
  if (existsSync(activePath)) {
    const s = statSync(activePath);
    activeMtime = s.mtime;
    activeSize = fmtBytes(s.size);
  }

  // Backup binary
  let bakMtime: Date | null = null;
  if (existsSync(bakPath)) {
    try { bakMtime = statSync(bakPath).mtime; } catch {}
  }

  // Git
  const gitHash = run("git", ["log", "-1", "--format=%H"], { cwd: REPO_ROOT });
  const gitDate = run("git", ["log", "-1", "--format=%cI"], { cwd: REPO_ROOT });
  const gitMsg = run("git", ["log", "-1", "--format=%s"], { cwd: REPO_ROOT });

  // Sync stamp
  let syncStamp: any = null;
  try {
    if (existsSync(stampPath)) {
      syncStamp = JSON.parse(require("node:fs").readFileSync(stampPath, "utf-8"));
    }
  } catch {}

  const now = Date.now();
  return {
    serverAlive,
    outputBase,
    bazelBinary: {
      exists: bazelBinPath !== null,
      path: bazelBinPath,
      mtime: bazelBinMtime,
      age: bazelBinMtime ? fmtDuration(now - bazelBinMtime.getTime()) : null,
    },
    activeBinary: {
      exists: existsSync(activePath),
      path: activePath,
      mtime: activeMtime,
      age: activeMtime ? fmtDuration(now - activeMtime.getTime()) : null,
      size: activeSize,
    },
    bakBinary: {
      exists: existsSync(bakPath),
      age: bakMtime ? fmtDuration(now - bakMtime.getTime()) : null,
    },
    git: {
      commit: gitHash.code === 0 ? gitHash.stdout.slice(0, 12) : null,
      date: gitDate.code === 0 ? gitDate.stdout : null,
      message: gitMsg.code === 0 ? gitMsg.stdout : null,
    },
    syncStamp,
  };
}

function formatStatus(s: ReturnType<typeof gatherStatus>): string {
  const lines: string[] = [];

  lines.push("╔══ KAIN BINARY STATUS ═══════════════════════════════");
  lines.push("║");

  // Active binary
  if (s.activeBinary.exists) {
    lines.push(`║  📦 Active:   ${s.activeBinary.path}`);
    lines.push(`║     Built:    ${s.activeBinary.mtime ? fmtTime(s.activeBinary.mtime) : "unknown"}`);
    lines.push(`║     Age:      ${s.activeBinary.age}`);
    lines.push(`║     Size:     ${s.activeBinary.size}`);
  } else {
    lines.push("║  📦 Active:   NOT FOUND");
  }
  lines.push("║");

  // Backup
  if (s.bakBinary.exists) {
    lines.push(`║  📎 Backup:   ${s.bakBinary.age} old`);
    lines.push("║");
  }

  // Bazel-built binary
  if (s.bazelBinary.exists) {
    lines.push(`║  🏗️  Bazel:    ${s.bazelBinary.path}`);
    lines.push(`║     Built:    ${s.bazelBinary.mtime ? fmtTime(s.bazelBinary.mtime) : "unknown"}`);
    lines.push(`║     Age:      ${s.bazelBinary.age}`);
    if (s.activeBinary.mtime && s.bazelBinary.mtime) {
      const diff = s.bazelBinary.mtime.getTime() - s.activeBinary.mtime.getTime();
      if (Math.abs(diff) > 1000) {
        const freshness = diff > 0 ? "NEWER" : "OLDER";
        lines.push(`║     vs active: ${fmtDuration(Math.abs(diff))} ${freshness}`);
      } else {
        lines.push(`║     vs active: same age`);
      }
    }
  } else {
    lines.push(`║  🏗️  Bazel:    no built binary found`);
  }
  lines.push("║");

  // Bazel server
  lines.push(`║  🔧 Server:   ${s.serverAlive ? "✅ alive" : "❄️  cold"}${s.outputBase ? ` (${s.outputBase})` : ""}`);
  lines.push("║");

  // Git
  if (s.git.commit) {
    lines.push(`║  📝 Git:      ${s.git.commit} — ${s.git.message}`);
    if (s.git.date) lines.push(`║     Date:     ${s.git.date.slice(0, 19).replace("T", " ")}`);
  }
  lines.push("║");

  // Sync stamp
  if (s.syncStamp) {
    const stamp = s.syncStamp;
    const syncedAt = stamp.synced_at_unix ? fmtTime(new Date(stamp.synced_at_unix * 1000)) : "unknown";
    lines.push(`║  📋 Sync:     repo_sha=${stamp.repo_sha?.slice(0, 12) ?? "?"} runtime=${stamp.runtime_stamp?.slice(0, 8) ?? "?"}`);
    lines.push(`║     Status:   ${stamp.source_dirty_count > 0 ? `⚠️ ${stamp.source_dirty_count} dirty source(s)` : "✅ clean"}`);
    lines.push(`║     Last:     ${syncedAt}`);
  }

  lines.push("║");
  lines.push("╚══════════════════════════════════════════════════════");
  lines.push("");
  lines.push("Commands: /kain-sync to freshen  |  BAZEL.md for troubleshooting");

  return lines.join("\n");
}

// ===========================================================================
// Core sync logic (shared by tool + command)
// ===========================================================================

async function runSync(ctx?: any): Promise<{ content: { type: string; text: string }[]; details: any; isError?: boolean }> {
  const lines: string[] = [];
  let anyError = false;

  const notify = (msg: string, kind: "info" | "error" | "warning" = "info") => {
    lines.push(msg);
    if (kind === "error") anyError = true;
    if (ctx && ctx.ui) ctx.ui.notify(msg, kind);
  };

  // Step 1: Check Bazel server
  notify("🔍 Checking Bazel server...", "info");
  const outputBase = getOutputBase();
  if (!outputBase) {
    notify("❌ Bazel server is not responding. Run `kain_bazel server action:'start'` first.", "error");
    notify("📖 See BAZEL.md for the full Bazel workflow guide.", "warning");
    return formatResult(lines, true);
  }
  notify(`  Output base: ${outputBase}`, "info");

  // Step 2: Try to find existing binary
  notify("🔍 Looking for existing bazel-built binary...", "info");
  const existingKain = findBazelBinary(outputBase, "kain");
  const existingKn = findBazelBinary(outputBase, "kn");

  if (!existingKain) {
    notify("  No existing binary found. Building //:kain...", "warning");
    const buildResult = buildBinary();
    if (!buildResult.success) {
      notify(`❌ ${buildResult.output}`, "error");
      notify("📖 See BAZEL.md for build troubleshooting.", "warning");
      return formatResult(lines, true);
    }
    notify("  Build succeeded.", "info");
  } else {
    notify(`  Found: ${existingKain}`, "info");
  }

  // Step 3: Re-find after build
  const kainSource = findBazelBinary(outputBase, "kain");
  const knSource = findBazelBinary(outputBase, "kn");

  if (!kainSource) {
    notify("❌ Could not find kain.exe in Bazel output after build.", "error");
    notify("📖 See BAZEL.md for troubleshooting.", "warning");
    return formatResult(lines, true);
  }

  // Step 4: Sync kain
  notify("📦 Syncing kain.exe...", "info");
  const kainResult = syncBinary(kainSource, "kain");
  if (!kainResult.success) {
    notify(kainResult.message, "error");
    notify("📖 See BAZEL.md for troubleshooting.", "warning");
    anyError = true;
  } else {
    notify(kainResult.message, "info");
  }

  // Step 5: Sync kn if available
  if (knSource) {
    notify("📦 Syncing kn.exe...", "info");
    const knResult = syncBinary(knSource, "kn");
    if (!knResult.success) {
      notify(knResult.message, "warning");
    } else {
      notify(knResult.message, "info");
    }
  }

  // Step 6: Verify
  notify("🔍 Verifying...", "info");
  const verify = run("kain", ["doctor"], { timeout: 15_000 });
  const isFresh = verify.stdout.includes("Binary Match:") && !verify.stdout.includes("drift");
  notify(`  ${isFresh ? "✅ Binary is up to date." : "⚠️ There may still be drift. Run `kain doctor` to check."}`, isFresh ? "info" : "warning");

  // Summary
  if (anyError) {
    notify("\n⚠️ Sync completed with errors. See BAZEL.md for the full build/sync guide.", "warning");
  } else {
    notify("\n✅ Sync complete! The agent is now using the freshest binary.", "info");
  }

  return formatResult(lines, anyError);
}

function formatResult(lines: string[], isError: boolean): { content: { type: string; text: string }[]; details: any; isError?: boolean } {
  const result: any = {
    content: [{ type: "text", text: lines.join("\n") }],
    details: { steps: lines },
  };
  if (isError) result.isError = true;
  return result;
}

// ===========================================================================
// Registration
// ===========================================================================

export default function (pi: ExtensionAPI) {
  // ── Command: /kain-sync ──
  pi.registerCommand("kain-sync", {
    description: "Sync latest Bazel-built binary to $KAIN_HOME/bin/ (or ~/.kain/bin/)",
    handler: async (_args, ctx) => {
      const result = await runSync(ctx);
      if (result.isError) {
        ctx.ui.notify("Sync had errors. See BAZEL.md for troubleshooting.", "error");
      }
    },
  });

  // ── Tool: kain_sync_binary ──
  pi.registerTool({
    name: "kain_sync_binary",
    label: "Sync Kain Binary",
    description: "Copy the latest Bazel-built kain.exe into $KAIN_HOME/bin/ (or ~/.kain/bin/) so `kain build/run/check` use the freshest compiler. If this fails, read BAZEL.md for the full workflow guide.",
    promptSnippet: "Sync the latest Bazel-built Kain binary to KAIN_HOME/bin/",
    promptGuidelines: [
      "Use kain_sync_binary when the user reports stale binary issues, 'kain doctor' shows drift, or the compiler seems out of date.",
      "If kain_sync_binary fails, read BAZEL.md for the full Bazel workflow, server lifecycle, and build/sync guide.",
      "Run 'kain_sync_binary' after 'kain_bazel build //:kain' to ensure the freshest binary is in use.",
    ],
    parameters: Type.Object({}),
    async execute() {
      return await runSync();
    },
  });

  // ── Command: /kain-status ──
  pi.registerCommand("kain-status", {
    description: "Show binary status: build time, age, Bazel server, git info",
    handler: async (_args, ctx) => {
      try {
        const status = gatherStatus();
        const text = formatStatus(status);
        ctx.ui.notify(text.split("\n")[0], "info");
        // Print via bash so the agent sees it in context
        console.log(text);
      } catch (e: any) {
        ctx.ui.notify(`Status error: ${e.message}`, "error");
      }
    },
  });

  // ── Tool: kain_status ──
  pi.registerTool({
    name: "kain_status",
    label: "Kain Binary Status",
    description: "Show when the kain.exe binary was last built, its age, Bazel server state, git info, and sync stamp. Use this before deciding whether to sync.",
    promptSnippet: "Show Kain binary status: build time, age, Bazel server, git info",
    promptGuidelines: [
      "Use kain_status before kain_sync_binary to check if the binary is actually stale.",
      "Check kain_status when diagnosing compiler issues — it shows Bazel server state, binary age, and git head in one place.",
    ],
    parameters: Type.Object({}),
    async execute() {
      try {
        const status = gatherStatus();
        const text = formatStatus(status);
        return {
          content: [{ type: "text", text }],
          details: status,
        };
      } catch (e: any) {
        return {
          content: [{ type: "text", text: `Error gathering status: ${e.message}` }],
          details: {},
          isError: true,
        };
      }
    },
  });

  // Notify on load
  pi.on("session_start", async (_event, ctx) => {
    ctx.ui.notify("🔄 kain-sync + /kain-status loaded — sync or check binary health", "info");
  });
}
