/**
 * Z3 Solver (MCP) Tools — Router for the locally-installed z3-mcp server
 *
 * Single router tool for the z3-mcp server (installed via `uv tool install`
 * from X:/mcp/polytools/z3-mcp). Spawns the `z3-mcp` stdio JSON-RPC server
 * on first call, keeps it alive for the session, and forwards each action to
 * the matching MCP tool. Covers source analysis, SMT-LIB v2 checking,
 * structured proof cases, proof-pack admin, and counterexample-to-regression
 * test scaffolding.
 *
 * The z3-mcp stdio transport uses newline-delimited JSON (one JSON object per
 * line, `\n` delimiter) — NOT the Content-Length framed transport some MCP
 * clients expect. This was verified empirically against z3-mcp 1.27.2.
 *
 * Tool: z3
 *   action: analyze   — analyze_source_file        (scan source for proof candidates)
 *   action: extract   — extract_source_proof_cases (materialize proof cases from source)
 *   action: check     — check_smt2                 (run Z3 on raw SMT-LIB v2 input)
 *   action: prove     — prove                      (run a structured proof case)
 *   action: admin     — tool_router                (low-frequency admin: init pack, list templates, …)
 *   action: regress   — counterexample_to_test     (Z3 counterexample → regression test skeleton)
 *
 * Env vars (read at first call):
 *   Z3_MCP_ROOT            — z3-mcp project root (default X:/mcp/polytools/z3-mcp)
 *   Z3_MCP_POLYTOOLS_ROOT  — polytools workspace root (default X:/mcp/polytools)
 *   Z3_MCP_BINARY          — override the z3-mcp executable path (default: whatever `z3-mcp` resolves to on PATH)
 *   Z3_MCP_DEFAULT_TIMEOUT_MS — per-call timeout in ms (default 60000)
 */

import type { ExtensionAPI } from "@earendil-works/pi-coding-agent";
import { Type } from "typebox";
import { spawn, ChildProcess } from "node:child_process";
import { existsSync } from "node:fs";
import { join, resolve } from "node:path";

// ===========================================================================
// Config — env-driven, with sensible defaults
// ===========================================================================

const DEFAULT_ROOT = "X:/mcp/polytools/z3-mcp";
const DEFAULT_POLYTOOLS = "X:/mcp/polytools";
const DEFAULT_TIMEOUT_MS = 60_000;

function getRoot(): string {
  return process.env.Z3_MCP_ROOT?.replace(/\\/g, "/") || DEFAULT_ROOT;
}
function getPolytools(): string {
  return process.env.Z3_MCP_POLYTOOLS_ROOT?.replace(/\\/g, "/") || DEFAULT_POLYTOOLS;
}
function getBinary(): string {
  return process.env.Z3_MCP_BINARY || "z3-mcp";
}
function getDefaultTimeout(): number {
  const v = parseInt(process.env.Z3_MCP_DEFAULT_TIMEOUT_MS || "", 10);
  return Number.isFinite(v) && v > 0 ? v : DEFAULT_TIMEOUT_MS;
}

// ===========================================================================
// JSON-RPC over stdio (newline-delimited, NOT Content-Length framed)
// ===========================================================================

interface Pending {
  resolve: (v: any) => void;
  reject: (e: Error) => void;
  method: string;
  timer: NodeJS.Timeout;
}

let proc: ChildProcess | null = null;
let nextId = 1;
const pending = new Map<number, Pending>();
let stderrBuf = "";
let initPromise: Promise<void> | null = null;

function logStderr(chunk: Buffer) {
  stderrBuf += chunk.toString("utf8");
  // Keep buffer bounded
  if (stderrBuf.length > 8192) stderrBuf = stderrBuf.slice(-4096);
}

function rejectAll(err: Error) {
  for (const [, p] of pending) {
    clearTimeout(p.timer);
    p.reject(err);
  }
  pending.clear();
}

function startServer(): Promise<void> {
  if (initPromise) return initPromise;
  initPromise = (async () => {
    const bin = getBinary();
    const env: NodeJS.ProcessEnv = {
      ...process.env,
      Z3_MCP_ROOT: getRoot(),
      Z3_MCP_POLYTOOLS_ROOT: getPolytools(),
    };
    proc = spawn(bin, [], {
      env,
      stdio: ["pipe", "pipe", "pipe"],
      windowsHide: true,
    });

    let buf = "";
    proc.stdout!.on("data", (chunk: Buffer) => {
      buf += chunk.toString("utf8");
      // NDJSON: split on \n, last partial line stays in buffer
      let nl: number;
      while ((nl = buf.indexOf("\n")) >= 0) {
        const line = buf.slice(0, nl).trim();
        buf = buf.slice(nl + 1);
        if (!line) continue;
        let msg: any;
        try {
          msg = JSON.parse(line);
        } catch {
          continue;
        }
        if (msg.id != null && pending.has(msg.id)) {
          const p = pending.get(msg.id)!;
          pending.delete(msg.id);
          clearTimeout(p.timer);
          if (msg.error) p.reject(new Error(formatRpcError(msg.error)));
          else p.resolve(msg.result);
        }
        // Notifications (no id) — ignored for now
      }
    });

    proc.stderr!.on("data", logStderr);
    proc.on("exit", (code, signal) => {
      const err = new Error(
        `z3-mcp server exited (code=${code}, signal=${signal}). ` +
          `Last stderr: ${stderrBuf.slice(-400) || "<empty>"}`,
      );
      proc = null;
      initPromise = null;
      rejectAll(err);
    });
    proc.on("error", (e) => {
      const err = new Error(
        `Failed to spawn z3-mcp (binary='${bin}'). ` +
          `Is it installed? Run \`uv tool install ${getRoot()}\` or set Z3_MCP_BINARY. Underlying: ${e.message}`,
      );
      proc = null;
      initPromise = null;
      rejectAll(err);
    });

    // MCP initialize handshake
    await rpc("initialize", {
      protocolVersion: "2024-11-05",
      capabilities: {},
      clientInfo: { name: "pi-z3-router", version: "0.1.0" },
    });
    // initialized notification (no id, no response expected)
    send({ jsonrpc: "2.0", method: "notifications/initialized" });
  })();
  return initPromise;
}

function formatRpcError(err: any): string {
  if (typeof err === "string") return err;
  if (err?.message) {
    let msg = `${err.code ?? "?"}: ${err.message}`;
    if (err.data) msg += ` | data: ${JSON.stringify(err.data).slice(0, 400)}`;
    return msg;
  }
  return JSON.stringify(err);
}

function send(msg: any) {
  if (!proc || !proc.stdin?.writable) {
    throw new Error("z3-mcp server is not running");
  }
  proc.stdin.write(JSON.stringify(msg) + "\n");
}

function rpc(method: string, params: any, timeoutMs?: number): Promise<any> {
  return new Promise((resolve, reject) => {
    if (!proc) {
      reject(new Error("z3-mcp server is not running"));
      return;
    }
    const id = nextId++;
    const timer = setTimeout(() => {
      if (pending.has(id)) {
        pending.delete(id);
        reject(new Error(`z3-mcp call '${method}' timed out after ${timeoutMs ?? getDefaultTimeout()}ms`));
      }
    }, timeoutMs ?? getDefaultTimeout());
    pending.set(id, { resolve, reject, method, timer });
    try {
      send({ jsonrpc: "2.0", id, method, params });
    } catch (e: any) {
      clearTimeout(timer);
      pending.delete(id);
      reject(e);
    }
  });
}

async function callTool(name: string, args: any, timeoutMs?: number): Promise<any> {
  await startServer();
  return rpc("tools/call", { name, arguments: args ?? {} }, timeoutMs);
}

// ===========================================================================
// Action → MCP tool mapping
// ===========================================================================

const ACTIONS: Record<string, { mcpTool: string; description: string }> = {
  analyze: { mcpTool: "analyze_source_file",       description: "Analyze a source file for proof opportunities" },
  extract: { mcpTool: "extract_source_proof_cases", description: "Extract structured proof cases from a source file" },
  check:   { mcpTool: "check_smt2",                description: "Run Z3 on raw SMT-LIB v2 input" },
  prove:   { mcpTool: "prove",                     description: "Run a structured proof case (kind + case dict)" },
  admin:   { mcpTool: "tool_router",               description: "Low-frequency admin actions (init pack, list templates, …)" },
  regress: { mcpTool: "counterexample_to_test",    description: "Turn a Z3 counterexample into a regression test skeleton" },
};

const ACTION_NAMES = Object.keys(ACTIONS);

// ===========================================================================
// Output formatting
// ===========================================================================

function mcpContentToText(result: any): string {
  // MCP tool results are { content: [{type:'text', text:'...'}], isError?: boolean }
  if (!result || typeof result !== "object") return JSON.stringify(result, null, 2);
  if (Array.isArray(result.content)) {
    const parts: string[] = [];
    for (const c of result.content) {
      if (c?.type === "text" && typeof c.text === "string") parts.push(c.text);
      else parts.push(JSON.stringify(c));
    }
    const body = parts.join("\n");
    if (result.isError) return `⚠️  z3-mcp returned isError=true\n\n${body}`;
    return body;
  }
  return JSON.stringify(result, null, 2);
}

function truncate(text: string, maxBytes = 50_000): { text: string; truncated: boolean; totalBytes: number } {
  const buf = Buffer.from(text, "utf8");
  if (buf.length <= maxBytes) return { text, truncated: false, totalBytes: buf.length };
  return { text: buf.slice(0, maxBytes).toString("utf8") + "\n…[truncated]…", truncated: true, totalBytes: buf.length };
}

// ===========================================================================
// Tool: z3
// ===========================================================================

export default function (pi: ExtensionAPI) {
  pi.registerTool({
    name: "z3",
    label: "Z3",
    description:
      "Router for the locally-installed z3-mcp server (uv tool install). " +
      "Spawns the `z3-mcp` stdio JSON-RPC server on first call, keeps it alive " +
      "for the session, and forwards each action to the matching MCP tool. " +
      "Covers source analysis, SMT-LIB v2 checking, structured proof cases, " +
      "proof-pack admin, and counterexample-to-regression-test scaffolding.",

    promptSnippet: "Solver-backed proof authoring: analyze source, check SMT2, run structured proofs, scaffold regression tests",
    promptGuidelines: [
      "Use z3 action:'check' for raw SMT-LIB v2 input (smt2 string, optional timeout_ms).",
      "Use z3 action:'prove' for structured proof cases (kind + case dict, e.g. size_add_ok, range_check, state_machine_check).",
      "Use z3 action:'analyze' to scan a source file (path, optional symbol/line) and surface proof opportunities.",
      "Use z3 action:'extract' to materialize proof cases from a source file (path, optional symbol/line, save=true to write to a pack).",
      "Use z3 action:'admin' for low-frequency ops: init_proof_pack, list_templates, discover_proof_packs, etc. (action + payload dict).",
      "Use z3 action:'regress' to turn a Z3 counterexample into a regression test skeleton (template, counterexample, test_name?).",
      "All actions accept an opaque `args` object that the z3-mcp server validates. When in doubt, call z3 action:'admin' with args={action:'list_templates'} first to see what kinds/cases are available.",
      "Forward slashes in paths on Windows (X:/mcp/...). The server requires Z3_MCP_ROOT and Z3_MCP_POLYTOOLS_ROOT env vars; this extension sets them automatically.",
      "Set timeout_ms per call to override the 60s default. Z3 can be slow on hard problems.",
    ],

    parameters: Type.Object({
      action: Type.String({
        description: "Which MCP tool to route to",
        enum: ACTION_NAMES,
      }),
      args: Type.Optional(
        Type.Record(Type.String(), Type.Any(), {
          description:
            "Opaque pass-through to the underlying MCP tool. Schema is enforced by z3-mcp, not by this router. " +
            "See manifest descriptions for the expected shape per action.",
        }),
      ),
      timeout_ms: Type.Optional(
        Type.Number({
          description: "Per-call timeout in ms. Defaults to Z3_MCP_DEFAULT_TIMEOUT_MS (60s).",
          minimum: 1_000,
          maximum: 600_000,
        }),
      ),
    }),

    async execute(_id, params, _signal, _onUpdate, _ctx) {
      const { action, args = {}, timeout_ms } = params as {
        action: string;
        args?: Record<string, any>;
        timeout_ms?: number;
      };

      const meta = ACTIONS[action];
      if (!meta) {
        throw new Error(
          `Unknown z3 action '${action}'. Valid: ${ACTION_NAMES.join(", ")}`,
        );
      }

      const result = await callTool(meta.mcpTool, args, timeout_ms);
      const text = mcpContentToText(result);
      const trunc = truncate(text);

      let out = trunc.text;
      if (trunc.truncated) {
        out += `\n\n[Output truncated: ${trunc.totalBytes} bytes total → ${trunc.text.length} bytes shown. ` +
          `Re-run with a tighter problem or save full result to disk via z3 action:'admin' args={action:'save_proof_case', payload:{...}}.]`;
      }

      return {
        content: [{ type: "text", text: out }],
        details: { mcp_tool: meta.mcpTool, action, isError: !!result?.isError, totalBytes: trunc.totalBytes },
      };
    },
  });

  // ── Slash command: /z3-status — quick health check ──
  pi.registerCommand("z3-status", {
    description: "Check whether the z3-mcp server can be spawned and responds to initialize",
    handler: async (_args, ctx) => {
      try {
        await startServer();
        ctx.ui.notify(
          `z3-mcp up (binary='${getBinary()}', root='${getRoot()}', polytools='${getPolytools()}')`,
          "info",
        );
      } catch (e: any) {
        ctx.ui.notify(`z3-mcp start failed: ${e.message}`, "error");
      }
    },
  });

  // ── Shutdown: clean kill the persistent server ──
  pi.on("session_shutdown", async () => {
    if (proc) {
      try {
        proc.stdin?.end();
      } catch {}
      try {
        proc.kill();
      } catch {}
      proc = null;
      initPromise = null;
    }
  });
}
