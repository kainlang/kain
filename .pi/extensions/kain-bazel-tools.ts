/**
 * Kain Bazel Tools — Pi extension
 *
 * Gives the LLM direct access to Bazel build/test/sync/freshness operations
 * for the Kain repo. Streamlines the boilerplate so agents don't need to
 * memorize paths, config flags, and script locations.
 *
 * Tools provided:
 *   bazel_build       — Build any Bazel target (//:kain, //:kn, //runtime:all, etc.)
 *   bazel_test        — Run any Bazel test target
 *   bazel_server      — Check/start/stop the Bazel server (warm lifecycle)
 *   bazel_sync        — Check or sync native runtime builds
 *   bazel_binary_age  — Check when the kain/kn binary was last built
 *   bazel_freshness   — Full repo freshness audit (binary + sync + server + git)
 *
 * Commands provided:
 *   /bazel            — Quick status overview (server + binary age)
 *   /bazel-rebuild    — Build //:kain with dev config, notify on completion
 *
 * Installation:
 *   Drop this file in ~/.pi/agent/extensions/ (global) or .pi/extensions/ (project-local).
 *   It auto-loads on next pi start; use /reload if pi is already running.
 */

import type { ExtensionAPI } from "@earendil-works/pi-coding-agent";
import { Type } from "typebox";
import { existsSync, statSync } from "node:fs";
import { join, resolve, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

// ---------------------------------------------------------------------------
// Repo root detection — walk up from cwd or extension dir
// ---------------------------------------------------------------------------

function findRepoRoot(): string | null {
	// Try the most reliable source first: cwd (set by pi to project root for .pi/extensions/)
	let dir = process.cwd();
	for (let i = 0; i < 20; i++) {
		if (existsSync(join(dir, "stdlib", "stdlib.map.json"))) return dir;
		if (existsSync(join(dir, "AGENTS.md")) && existsSync(join(dir, "CATALOG.md"))) return dir;
		const parent = resolve(dir, "..");
		if (parent === dir) break;
		dir = parent;
	}
	// Fallback: derive from this file's location (X:/.pi/extensions/ -> X:/)
	try {
		const thisFile = fileURLToPath(import.meta.url);
		const extDir = dirname(thisFile);
		const candidate = resolve(extDir, "..", "..");
		if (existsSync(join(candidate, "AGENTS.md"))) return candidate;
	} catch {}
	return null;
}

const REPO_ROOT = findRepoRoot() ?? process.cwd();
const BAZEL_ON_BAT = join(REPO_ROOT, "tools/bazel/bazel_on.bat");
const BAZEL_OFF_BAT = join(REPO_ROOT, "tools/bazel/bazel_off.bat");
const SYNC_NATIVE_SCRIPT = join(REPO_ROOT, "tools/bazel/sync_native_runtime_builds.py");
const KAIN_BAZEL_SYNC_SCRIPT = join(REPO_ROOT, "scripts/python/kain_bazel_sync.py");
const KAIN_BIN_DIR = join(REPO_ROOT, ".kain/bin");
const KNOWN_BINARIES = ["kain.exe", "kn.exe", "kain", "kn"] as const;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function runCmd(cmd: string, args: string[], opts?: { cwd?: string }): { stdout: string; stderr: string; code: number } {
	const result = spawnSync(cmd, args, {
		cwd: opts?.cwd ?? REPO_ROOT,
		encoding: "utf-8",
		timeout: 120_000,
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
	const seconds = Math.floor(ms / 1000);
	if (seconds < 60) return `${seconds}s`;
	const minutes = Math.floor(seconds / 60);
	if (minutes < 60) return `${minutes}m ${seconds % 60}s`;
	const hours = Math.floor(minutes / 60);
	if (hours < 24) return `${hours}h ${minutes % 60}m`;
	const days = Math.floor(hours / 24);
	return `${days}d ${hours % 24}h`;
}

function formatTimestamp(ts: Date): string {
	return ts.toISOString().replace("T", " ").slice(0, 19);
}

function getLastGitCommit(): { hash: string; date: string; message: string } | null {
	const hash = runCmd("git", ["log", "-1", "--format=%H"]);
	if (hash.code !== 0) return null;
	const date = runCmd("git", ["log", "-1", "--format=%cI"]);
	const msg = runCmd("git", ["log", "-1", "--format=%s"]);
	return {
		hash: hash.stdout.slice(0, 12),
		date: date.stdout,
		message: msg.stdout,
	};
}

function getBinaryInfo(binary: string): { path: string; exists: boolean; ageMs: number | null; mtime: string | null } {
	const binPath = join(KAIN_BIN_DIR, binary);
	const age = getFileAgeMs(binPath);
	return {
		path: binPath,
		exists: age !== null,
		ageMs: age,
		mtime: age !== null ? formatTimestamp(new Date(Date.now() - age)) : null,
	};
}

function checkBazelServer(): { alive: boolean; pid: string | null; outputBase: string | null } {
	const result = runCmd("bazel", ["info", "server_pid", "--config=dev"], { cwd: REPO_ROOT });
	if (result.code === 0 && /^\d+$/.test(result.stdout)) {
		const outBase = runCmd("bazel", ["info", "output_base", "--config=dev"], { cwd: REPO_ROOT });
		return {
			alive: true,
			pid: result.stdout,
			outputBase: outBase.code === 0 ? outBase.stdout : null,
		};
	}
	return { alive: false, pid: null, outputBase: null };
}

// ---------------------------------------------------------------------------
// Common target presets for fast tab-completion in the LLM's head
// ---------------------------------------------------------------------------

const BUILD_PRESETS = [
	"//:kain",
	"//:kn",
	"//runtime:all",
	"//runtime:native_core_runtime",
	"//:kain --config=release",
	"//:developer_smoke_tests",
	"//:crate_tests",
	"//:key_crate_tests",
	"//runtime:native_runtime_tests",
] as const;

const TEST_PRESETS = [
	"//:developer_smoke_tests",
	"//:crate_tests",
	"//:key_crate_tests",
	"//runtime:native_runtime_tests",
] as const;

// ===========================================================================
// Tool: bazel_build
// ===========================================================================

const bazelBuildTool = {
	name: "bazel_build",
	label: "Bazel Build",
	description:
		"Build any Bazel target in the Kain repo. Supports common presets and custom targets. " +
		"Common: //:kain (CLI), //:kn (launcher), //runtime:all (native runtime), " +
		"//:kain --config=release (release). Defaults to --config=dev.",
	promptSnippet: "Build Bazel targets (//:kain, //runtime:all, etc.)",
	promptGuidelines: [
		"Use bazel_build with target '//:kain' when asked to build the Kain compiler.",
		"Use bazel_build with target '//runtime:all' when the native runtime needs rebuilding.",
		"After any source change to Rust/crates/ code, build //:kain --config=dev to verify compilation.",
		"Use target '//:kain --config=release' for release-optimized builds.",
	],
	parameters: Type.Object({
		target: Type.String({
			description:
				"Bazel target to build. Examples: '//:kain', '//:kn', '//runtime:all', " +
				"'//:kain --config=release', '//:developer_smoke_tests', or custom target.",
		}),
		config: Type.Optional(
			Type.String({
				description: "Bazel config. Usually 'dev' (default) or 'release'.",
				default: "dev",
			}),
		),
		watch: Type.Optional(
			Type.Boolean({
				description: "If true, the --config=... flag is omitted (for targets that already include it).",
				default: false,
			}),
		),
	}),
	async execute(
		_toolCallId: string,
		params: { target: string; config?: string; watch?: boolean },
		_signal: AbortSignal,
		_onUpdate: any,
		_ctx: any,
	) {
		const { target, config = "dev", watch = false } = params;

		let fullTarget = target;
		if (!watch && !target.includes("--config=")) {
			fullTarget = `${target} --config=${config}`;
		}

		const start = Date.now();
		const result = runCmd("bazel", ["build", ...fullTarget.split(/\s+/).filter(Boolean)], { cwd: REPO_ROOT });
		const elapsed = Date.now() - start;

		if (result.code !== 0) {
			return {
				content: [
					{
						type: "text",
						text:
							`## ❌ Bazel Build Failed\n\n` +
							`**Target:** \`${fullTarget}\`\n` +
							`**Duration:** ${formatDuration(elapsed)}\n` +
							`**Exit Code:** ${result.code}\n\n` +
							(result.stderr ? `### stderr\n\`\`\`\n${result.stderr.slice(0, 5000)}\n\`\`\`\n\n` : "") +
							(result.stdout ? `### stdout\n\`\`\`\n${result.stdout.slice(0, 2000)}\n\`\`\`\n` : ""),
					},
				],
				details: { success: false, target: fullTarget, exitCode: result.code, elapsedMs: elapsed },
				isError: true,
			};
		}

		return {
			content: [
				{
					type: "text",
					text:
						`## ✅ Bazel Build Succeeded\n\n` +
						`**Target:** \`${fullTarget}\`\n` +
						`**Duration:** ${formatDuration(elapsed)}\n\n` +
						(result.stdout ? `\`\`\`\n${result.stdout.slice(0, 1000)}\n\`\`\`\n` : ""),
				},
			],
			details: { success: true, target: fullTarget, elapsedMs: elapsed },
		};
	},
};

// ===========================================================================
// Tool: bazel_test
// ===========================================================================

const bazelTestTool = {
	name: "bazel_test",
	label: "Bazel Test",
	description:
		"Run any Bazel test target. " +
		"Common: //:developer_smoke_tests, //:crate_tests, //:key_crate_tests, //runtime:native_runtime_tests. " +
		"Defaults to --config=dev.",
	promptSnippet: "Run Bazel test targets",
	promptGuidelines: [
		"After building, run //:developer_smoke_tests as the primary validation suite.",
		"Use //:crate_tests for full crate-level unit tests.",
	],
	parameters: Type.Object({
		target: Type.String({
			description:
				"Bazel test target. Examples: '//:developer_smoke_tests', '//:crate_tests', " +
				"'//:key_crate_tests', '//runtime:native_runtime_tests', or custom target.",
		}),
		config: Type.Optional(
			Type.String({
				description: "Bazel config. Usually 'dev' (default).",
				default: "dev",
			}),
		),
	}),
	async execute(
		_toolCallId: string,
		params: { target: string; config?: string },
		_signal: AbortSignal,
		_onUpdate: any,
		_ctx: any,
	) {
		const { target, config = "dev" } = params;
		const fullTarget = target.includes("--config=") ? target : `${target} --config=${config}`;

		const start = Date.now();
		const result = runCmd("bazel", ["test", ...fullTarget.split(/\s+/).filter(Boolean)], { cwd: REPO_ROOT });
		const elapsed = Date.now() - start;

		if (result.code !== 0) {
			return {
				content: [
					{
						type: "text",
						text:
							`## ❌ Bazel Test Failed\n\n` +
							`**Target:** \`${fullTarget}\`\n` +
							`**Duration:** ${formatDuration(elapsed)}\n` +
							`**Exit Code:** ${result.code}\n\n` +
							(result.stderr ? `### stderr\n\`\`\`\n${result.stderr.slice(0, 5000)}\n\`\`\`\n\n` : "") +
							(result.stdout ? `### stdout\n\`\`\`\n${result.stdout.slice(0, 2000)}\n\`\`\`\n` : ""),
					},
				],
				details: { success: false, target: fullTarget, exitCode: result.code, elapsedMs: elapsed },
				isError: true,
			};
		}

		return {
			content: [
				{
					type: "text",
					text:
						`## ✅ Bazel Test Passed\n\n` +
						`**Target:** \`${fullTarget}\`\n` +
						`**Duration:** ${formatDuration(elapsed)}\n\n` +
						(result.stdout ? `\`\`\`\n${result.stdout.slice(0, 2000)}\n\`\`\`\n` : ""),
				},
			],
			details: { success: true, target: fullTarget, elapsedMs: elapsed },
		};
	},
};

// ===========================================================================
// Tool: bazel_server
// ===========================================================================

const bazelServerTool = {
	name: "bazel_server",
	label: "Bazel Server",
	description:
		"Manage the Bazel server lifecycle. 'status' checks if the server is alive, 'start' warms it up " +
		"(bazel_on.bat), 'stop' shuts it down (bazel_off.bat), 'restart' does stop-then-start. " +
		"A cold Bazel server adds 30-90s to every first command — keep it warm.",
	promptSnippet: "Check, start, or stop the Bazel server",
	promptGuidelines: [
		"Run bazel_server with action:'status' at the start of every session to check if the Bazel server is alive.",
		"If the server is cold (status fails), immediately run bazel_server with action:'start' to warm it up.",
		"Shut down the server with action:'stop' at the end of heavy sessions to free resources.",
	],
	parameters: Type.Object({
		action: Type.Enum(
			{ status: "status", start: "start", stop: "stop", restart: "restart" },
			{ description: "'status' to check, 'start' to warm, 'stop' to shutdown, 'restart' to cycle." },
		),
	}),
	async execute(
		_toolCallId: string,
		params: { action: "status" | "start" | "stop" | "restart" },
		_signal: AbortSignal,
		_onUpdate: any,
		_ctx: any,
	) {
		switch (params.action) {
			case "status": {
				const server = checkBazelServer();
				if (server.alive) {
					return {
						content: [
							{
								type: "text",
								text:
									`## ✅ Bazel Server is Running\n\n` +
									`**PID:** ${server.pid}\n` +
									(server.outputBase ? `**Output Base:** \`${server.outputBase}\`\n` : ""),
							},
						],
						details: { alive: true, pid: server.pid, outputBase: server.outputBase },
					};
				}
				return {
					content: [
						{
							type: "text",
							text:
								`## ❄️ Bazel Server is Cold\n\n` +
								`The server is not running. Every first Bazel command will pay a 30-90s startup cost. ` +
								`Use \`bazel_server with action:'start'\` to warm it up.`,
						},
					],
					details: { alive: false },
				};
			}

			case "start": {
				// Check if already alive
				const existing = checkBazelServer();
				if (existing.alive) {
					return {
						content: [
							{
								type: "text",
								text:
									`## ✅ Bazel Server Already Running\n\n` +
									`**PID:** ${existing.pid}\n` +
									(existing.outputBase ? `**Output Base:** \`${existing.outputBase}\`\n` : `No warm-up needed.`),
							},
						],
						details: { alive: true, pid: existing.pid, started: false },
					};
				}

				const start = Date.now();
				const result = runBat(BAZEL_ON_BAT);
				const elapsed = Date.now() - start;

				// Verify it actually started
				const after = checkBazelServer();
				if (after.alive) {
					return {
						content: [
							{
								type: "text",
								text:
									`## ✅ Bazel Server Started\n\n` +
									`**PID:** ${after.pid}\n` +
									`**Duration:** ${formatDuration(elapsed)}\n` +
									(after.outputBase ? `**Output Base:** \`${after.outputBase}\`\n` : ""),
							},
						],
						details: { alive: true, pid: after.pid, started: true, elapsedMs: elapsed },
					};
				}

				return {
					content: [
						{
							type: "text",
							text:
								`## ⚠️ Bazel Server Start Initiated\n\n` +
								`The start script completed (exit code: ${result.code}) but the server is not yet responsive. ` +
								"It may still be initializing. Try `bazel_server status` in a moment.\n\n" +
								(result.stderr ? `### stderr\n\`\`\`\n${result.stderr.slice(0, 2000)}\n\`\`\`\n` : ""),
						},
					],
					details: { alive: false, started: true, elapsedMs: elapsed, scriptExitCode: result.code },
				};
			}

			case "stop": {
				const result = runBat(BAZEL_OFF_BAT);
				const after = checkBazelServer();

				return {
					content: [
						{
							type: "text",
							text:
								after.alive
									? `## ⚠️ Bazel Server May Still Be Running\n\nServer PID ${after.pid} still detected. Try again or check manually.`
									: `## ✅ Bazel Server Stopped\n\nAll server processes shut down.`,
						},
					],
					details: { alive: after.alive, exitCode: result.code },
				};
			}

			case "restart": {
				runBat(BAZEL_OFF_BAT);
				const start = Date.now();
				const result = runBat(BAZEL_ON_BAT);
				const elapsed = Date.now() - start;
				const after = checkBazelServer();

				return {
					content: [
						{
							type: "text",
							text:
								after.alive
									? `## ✅ Bazel Server Restarted\n\n**PID:** ${after.pid} — **Duration:** ${formatDuration(elapsed)}`
									: `## ⚠️ Bazel Server Restart Pending\n\nScript completed but server not yet ready.`,
						},
					],
					details: { alive: after.alive, pid: after.pid, elapsedMs: elapsed },
				};
			}
		}
	},
};

// ===========================================================================
// Tool: bazel_sync
// ===========================================================================

const bazelSyncTool = {
	name: "bazel_sync",
	label: "Bazel Sync",
	description:
		"Check or run the native runtime Bazel sync. 'check' validates whether the generated BUILD.bazel files " +
		"match the runtime manifests (no drift). 'sync' regenerates them. " +
		"Uses sync_native_runtime_builds.py and kain_bazel_sync.py under the hood.",
	promptSnippet: "Check or sync native runtime Bazel build files",
	promptGuidelines: [
		"Run bazel_sync with mode:'check' when BUILD.bazel drift is suspected after runtime manifest changes.",
		"Run bazel_sync with mode:'sync' after changing runtime/native/ manifests to regenerate build files.",
	],
	parameters: Type.Object({
		mode: Type.Enum(
			{ check: "check", sync: "sync" },
			{ description: "'check' to validate no drift, 'sync' to regenerate build files." },
		),
	}),
	async execute(
		_toolCallId: string,
		params: { mode: "check" | "sync" },
		_signal: AbortSignal,
		_onUpdate: any,
		_ctx: any,
	) {
		if (params.mode === "check") {
			// Two check methods: native runtime sync + kain_bazel_sync status
			const nativeResult = runPy(SYNC_NATIVE_SCRIPT, ["--check"]);
			const statusResult = runPy(KAIN_BAZEL_SYNC_SCRIPT, ["status", "--json"]);

			const nativeOk = nativeResult.code === 0;
			const statusOk = statusResult.code === 0;

			let summary = `## Bazel Sync Status\n\n`;

			summary += `### Native Runtime Sync\n`;
			summary += nativeOk ? `✅ **PASS** — No drift detected.\n` : `❌ **DRIFT DETECTED**\n`;
			if (nativeResult.stdout) summary += `\`\`\`\n${nativeResult.stdout.slice(0, 1500)}\n\`\`\`\n`;
			if (nativeResult.stderr) summary += `stderr:\n\`\`\`\n${nativeResult.stderr.slice(0, 1500)}\n\`\`\`\n`;

			summary += `\n### kain_bazel_sync Status\n`;
			summary += statusOk
				? `✅ **PASS**${statusResult.stdout ? `\n\`\`\`\n${statusResult.stdout.slice(0, 1500)}\n\`\`\`\n` : "\n"}`
				: `❌ **FAIL**${statusResult.stderr ? `\n\`\`\`\n${statusResult.stderr.slice(0, 1500)}\n\`\`\`\n` : "\n"}`;

			return {
				content: [{ type: "text", text: summary }],
				details: {
					nativeSyncOk: nativeOk,
					kainBazelSyncOk: statusOk,
				},
			};
		}

		// Sync mode
		const start = Date.now();
		const result = runPy(SYNC_NATIVE_SCRIPT, []);
		const elapsed = Date.now() - start;

		if (result.code !== 0) {
			return {
				content: [
					{
						type: "text",
						text:
							`## ❌ Native Runtime Sync Failed\n\n` +
							`**Duration:** ${formatDuration(elapsed)}\n\n` +
							(result.stderr ? `### stderr\n\`\`\`\n${result.stderr.slice(0, 3000)}\n\`\`\`\n` : ""),
					},
				],
				details: { success: false, elapsedMs: elapsed },
				isError: true,
			};
		}

		return {
			content: [
				{
					type: "text",
					text:
						`## ✅ Native Runtime Sync Completed\n\n` +
						`**Duration:** ${formatDuration(elapsed)}\n\n` +
						(result.stdout ? `\`\`\`\n${result.stdout.slice(0, 2000)}\n\`\`\`\n` : ""),
				},
			],
			details: { success: true, elapsedMs: elapsed },
		};
	},
};

// ===========================================================================
// Tool: bazel_binary_age
// ===========================================================================

const bazelBinaryAgeTool = {
	name: "bazel_binary_age",
	label: "Bazel Binary Age",
	description:
		"Check when the kain/kn binaries in X:/.kain/bin/ were last built. " +
		"Returns the file timestamp, age, and compares against the latest git commit " +
		"so you can tell if the binary is stale relative to source changes.",
	promptSnippet: "Check when the Kain binary was last built",
	promptGuidelines: [
		"Run bazel_binary_age before assuming the binary is fresh — the launcher shim may be old.",
		"If the binary is stale (older than last source change), rebuild with bazel_build target '//:kain'.",
	],
	parameters: Type.Object({
		binary: Type.Optional(
			Type.Enum(
				{ kain: "kain", kn: "kn", both: "both" },
				{ description: "Which binary to inspect. Default is 'both'.", default: "both" },
			),
		),
	}),
	async execute(
		_toolCallId: string,
		params: { binary?: "kain" | "kn" | "both" },
		_signal: AbortSignal,
		_onUpdate: any,
		_ctx: any,
	) {
		const targets = params.binary === "both" ? ["kain.exe", "kn.exe"] : [`${params.binary ?? "kain"}.exe`];

		const git = getLastGitCommit();

		let report = `## Binary Freshness Report\n\n`;

		// Header
		if (git) {
			report += `**Last source change:** ${git.date} — \`${git.hash}\` — *${git.message}*\n\n`;
		}

		for (const bin of targets) {
			const info = getBinaryInfo(bin);
			if (!info.exists) {
				report += `### ${bin}\n❌ **Not found** at \`${info.path}\`\n\n`;
				continue;
			}

			const ageStr = formatDuration(info.ageMs!);
			report += `### ${bin}\n`;
			report += `**Path:** \`${info.path}\`\n`;
			report += `**Built:** ${info.mtime}\n`;
			report += `**Age:** ${ageStr}\n`;

			if (git) {
				const sourceDate = new Date(git.date).getTime();
				const binDate = new Date(info.mtime!).getTime();
				const staleMs = sourceDate - binDate;
				if (staleMs > 0) {
					report += `⚠️ **STALE** — source is ${formatDuration(staleMs)} newer than binary. Rebuild with \`bazel_build target:'//:kain'\`.\n`;
				} else {
					report += `✅ **Fresh** — built after last source change.\n`;
				}
			}
			report += `\n`;
		}

		return {
			content: [{ type: "text", text: report }],
			details: {
				lastGitCommit: git,
				binaries: Object.fromEntries(
					targets.map((bin) => {
						const info = getBinaryInfo(bin);
						return [bin, { exists: info.exists, ageMs: info.ageMs, mtime: info.mtime }];
					}),
				),
			},
		};
	},
};

// ===========================================================================
// Tool: bazel_freshness
// ===========================================================================

const bazelFreshnessTool = {
	name: "bazel_freshness",
	label: "Bazel Freshness",
	description:
		"Full repo freshness audit. Checks: (1) Bazel server status, (2) kain binary age vs source, " +
		"(3) native runtime sync status, (4) last git commit. Gives a single go/no-go signal " +
		"for whether the repo is ready to work with.",
	promptSnippet: "Full repo freshness audit (server + binary + sync + git)",
	promptGuidelines: [
		"Run bazel_freshness at the start of any agent session to get a complete picture of repo health.",
		"If freshness check shows any red flags, address them before starting significant work.",
	],
	parameters: Type.Object({}),
	async execute(
		_toolCallId: string,
		_params: Record<string, never>,
		_signal: AbortSignal,
		_onUpdate: any,
		_ctx: any,
	) {
		const git = getLastGitCommit();

		// Check server
		const server = checkBazelServer();

		// Check binaries
		const kainInfo = getBinaryInfo("kain.exe");
		const knInfo = getBinaryInfo("kn.exe");

		// Check sync
		const syncResult = runPy(SYNC_NATIVE_SCRIPT, ["--check"]);
		const syncOk = syncResult.code === 0;

		// Build report
		let report = `# 🔍 Kain Repo Freshness Report\n\n`;
		report += `**Repo:** \`${REPO_ROOT}\`\n`;
		report += `**Checked:** ${formatTimestamp(new Date())}\n\n`;

		// --- Git ---
		report += `## 1. Last Source Change\n`;
		if (git) {
			report += `**Date:** ${git.date}\n`;
			report += `**Hash:** \`${git.hash}\`\n`;
			report += `**Message:** ${git.message}\n\n`;
		} else {
			report += `❌ Could not read git history.\n\n`;
		}

		// --- Server ---
		report += `## 2. Bazel Server\n`;
		if (server.alive) {
			report += `✅ **Running** — PID ${server.pid}\n`;
			if (server.outputBase) report += `   Output base: \`${server.outputBase}\`\n`;
		} else {
			report += `❄️ **Cold** — server needs warm-up (\`bazel_server start\`)\n`;
		}
		report += `\n`;

		// --- Binary age ---
		report += `## 3. Binary Freshness\n`;
		for (const [name, info] of Object.entries({ kain: kainInfo, kn: knInfo })) {
			if (!info.exists) {
				report += `**${name}.exe:** ❌ Not found\n`;
				continue;
			}
			const ageStr = formatDuration(info.ageMs!);
			report += `**${name}.exe:** Built ${info.mtime} (${ageStr} ago)`;

			if (git) {
				const sourceDate = new Date(git.date).getTime();
				const binDate = new Date(info.mtime!).getTime();
				const staleMs = sourceDate - binDate;
				if (staleMs > 5000) {
					// 5s tolerance for clock skew
					report += ` — ⚠️ **STALE** (source ${formatDuration(staleMs)} newer)\n`;
				} else {
					report += ` — ✅ **Fresh**\n`;
				}
			} else {
				report += `\n`;
			}
		}
		report += `\n`;

		// --- Sync ---
		report += `## 4. Native Runtime Sync\n`;
		report += syncOk ? `✅ **Clean** — no drift\n` : `⚠️ **Drift detected** — run \`bazel_sync mode:'sync'\`\n`;
		if (!syncOk && syncResult.stderr) {
			report += `\`\`\`\n${syncResult.stderr.slice(0, 1000)}\n\`\`\`\n`;
		}
		report += `\n`;

		// --- Summary ---
		const issues: string[] = [];
		if (!server.alive) issues.push("Bazel server is cold (start it)");
		if (kainInfo.exists && git) {
			const sourceDate = new Date(git.date).getTime();
			if (new Date(kainInfo.mtime!).getTime() < sourceDate - 5000)
				issues.push("kain.exe is stale (rebuild //:kain)");
		} else if (!kainInfo.exists) {
			issues.push("kain.exe not found (build //:kain)");
		}
		if (!syncOk) issues.push("Native runtime sync drift (run bazel_sync)");

		report += `## Summary\n\n`;
		if (issues.length === 0) {
			report += `✅ **All clear.** Repo is fresh, server is warm, ready to work.\n`;
		} else {
			report += `⚠️ **${issues.length} issue(s) to address:**\n`;
			for (const issue of issues) {
				report += `- ${issue}\n`;
			}
		}

		return {
			content: [{ type: "text", text: report }],
			details: {
				serverAlive: server.alive,
				serverPid: server.pid,
				lastGitCommit: git,
				kainBinary: { exists: kainInfo.exists, ageMs: kainInfo.ageMs, mtime: kainInfo.mtime },
				knBinary: { exists: knInfo.exists, ageMs: knInfo.ageMs, mtime: knInfo.mtime },
				syncOk,
				issues,
			},
		};
	},
};

// ===========================================================================
// Command: /bazel — quick status
// ===========================================================================

function registerBazelCommand(pi: ExtensionAPI) {
	pi.registerCommand("bazel", {
		description: "Quick Bazel status overview — server health + binary age",
		handler: async (_args, ctx) => {
			const server = checkBazelServer();
			const git = getLastGitCommit();
			const kain = getBinaryInfo("kain.exe");

			let msg = `## Bazel Status\n\n`;
			msg += server.alive
				? `**Server:** ✅ Running (PID ${server.pid})\n`
				: `**Server:** ❄️ Cold\n`;
			msg += git
				? `**Last commit:** ${git.date} — ${git.message}\n`
				: `**Last commit:** unknown\n`;
			msg += kain.exists
				? `**kain.exe:** Built ${kain.mtime} (${formatDuration(kain.ageMs!)} ago)\n`
				: `**kain.exe:** Not found\n`;

			ctx.ui.notify(msg.replace(/\*\*/g, "").replace(/\n/g, " • "), "info");
		},
	});
}

// ===========================================================================
// Command: /bazel-rebuild — quick rebuild
// ===========================================================================

function registerBazelRebuildCommand(pi: ExtensionAPI) {
	pi.registerCommand("bazel-rebuild", {
		description: "Build //:kain with dev config and notify on completion",
		handler: async (_args, ctx) => {
			ctx.ui.notify("Building //:kain --config=dev...", "info");
			const start = Date.now();
			const result = runCmd("bazel", ["build", "//:kain", "--config=dev"], { cwd: REPO_ROOT });
			const elapsed = Date.now() - start;

			if (result.code === 0) {
				ctx.ui.notify(`✅ //:kain built in ${formatDuration(elapsed)}`, "info");
			} else {
				ctx.ui.notify(`❌ //:kain build failed in ${formatDuration(elapsed)}`, "error");
			}
		},
	});
}

// ===========================================================================
// Extension entry point
// ===========================================================================

export default function (pi: ExtensionAPI) {
	// Register all 6 tools
	pi.registerTool(bazelBuildTool);
	pi.registerTool(bazelTestTool);
	pi.registerTool(bazelServerTool);
	pi.registerTool(bazelSyncTool);
	pi.registerTool(bazelBinaryAgeTool);
	pi.registerTool(bazelFreshnessTool);

	// Register convenience commands for the human
	registerBazelCommand(pi);
	registerBazelRebuildCommand(pi);

	// Notify on load
	pi.on("session_start", async (_event, ctx) => {
		ctx.ui.notify("🧰 Kain Bazel Tools loaded — 6 tools, 2 commands", "info");
	});
}
