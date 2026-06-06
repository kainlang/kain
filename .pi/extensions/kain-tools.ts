/**
 * Kain Stdlib Tools — Zero-cost Kain standard library access
 *
 * Single router tool that dispatches to 8 sub-actions for querying Kain's
 * complete standard library: modules, symbols, signatures, docs, source
 * code, keyword reference, and semantic code search.
 *
 * Ported from ~/.pi/agent/extensions/kain-stdlib.ts, refactored into a
 * clean router pattern with a shared data layer.
 *
 * Tool: kain_stdlib
 *   action: list_modules    — List all modules with symbol counts
 *   action: get_symbols     — List symbols in a module
 *   action: search_symbols  — Fuzzy-search across stdlib
 *   action: get_details     — Full docs + signature for a symbol
 *   action: get_source      — Read the actual .kn source
 *   action: list_keywords   — Full Kain keyword reference
 *   action: get_keyword     — Help for a specific keyword
 *   action: search_examples — Semantic PyTorch/CUDA code search
 */

import type { ExtensionAPI } from "@earendil-works/pi-coding-agent";
import { Type } from "typebox";
import { readFileSync, existsSync } from "node:fs";
import { join, resolve } from "node:path";
import { spawnSync } from "node:child_process";

// ===========================================================================
// Data layer — reads stdlib/stdlib.map.json and CATALOG.md
// ===========================================================================

function findRepoRoot(): string | null {
  let dir = process.cwd();
  for (let i = 0; i < 20; i++) {
    if (existsSync(join(dir, "stdlib", "stdlib.map.json"))) return dir;
    const parent = resolve(dir, "..");
    if (parent === dir) break;
    dir = parent;
  }
  return null;
}

let _repoRoot: string | null = null;
let _stdlibData: any = null;
let _catalogKeywords: Record<string, string[]> | null = null;

function getRepoRoot(): string {
  if (!_repoRoot) _repoRoot = findRepoRoot();
  if (!_repoRoot) throw new Error("Cannot find stdlib/stdlib.map.json — are you in the Kain repo root?");
  return _repoRoot;
}

function getStdlibData(): any {
  if (!_stdlibData) {
    _stdlibData = JSON.parse(readFileSync(join(getRepoRoot(), "stdlib", "stdlib.map.json"), "utf-8"));
  }
  return _stdlibData;
}

function normalizeModule(value: string): string {
  let v = value.trim();
  if (v.startsWith("std::")) v = v.replace(/^std::/, "");
  return v.replace(/\//g, "::");
}

function iterModules(data: any): any[] {
  return data?.modules ?? [];
}

function findModule(data: any, name: string): any {
  const needle = normalizeModule(name);
  for (const mod of iterModules(data)) {
    if (mod.name === needle || mod.import_path === `std::${needle}`) return mod;
  }
  const available = iterModules(data).map((m: any) => m.import_path || m.name).join(", ");
  throw new Error(`Unknown module '${name}'. Available: ${available}`);
}

function modulePublicCounts(mod: any): [number, number] {
  const syms = mod.symbols ?? [];
  return [syms.filter((s: any) => s.visibility === "public").length, syms.length - syms.filter((s: any) => s.visibility === "public").length];
}

function searchSymbol(symbol: any, query: string): boolean {
  const haystack = ["name", "qualified_name", "signature", "source_path", "kind", "visibility", "docs"]
    .map((f) => String(symbol[f] ?? ""))
    .join(" ")
    .toLowerCase();
  const q = query.toLowerCase().trim();
  if (haystack.includes(q)) return true;
  const expanded = haystack.replace(/[_\-]/g, " ");
  if (expanded.includes(q)) return true;
  const tokens = q.split(/\s+/);
  if (tokens.length > 1 && tokens.every((t) => expanded.includes(t))) return true;
  try {
    return new RegExp(`(?<![a-z])${q.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")}(?![a-z])`).test(expanded);
  } catch {
    return false;
  }
}

function extractSymbolSource(repoRoot: string, data: any, symbol: any, contextBefore: number = 2): string {
  const sourcePath = symbol.source_path;
  if (!sourcePath) return "No source path found.";
  const filePath = join(repoRoot, sourcePath);
  if (!existsSync(filePath)) return `Source file not found: ${sourcePath}`;
  const lines = readFileSync(filePath, "utf-8").split("\n");
  const lineNum = symbol.line;
  if (!lineNum || lineNum > lines.length) return `Invalid line ${lineNum}`;
  let nextLine = lines.length + 1;
  for (const mod of iterModules(data)) {
    for (const s of mod.symbols ?? []) {
      const sp = s.source_path || mod.source_path;
      if (sp === sourcePath && s.line && s.line > lineNum && s.line < nextLine) nextLine = s.line;
    }
  }
  const start = Math.max(0, lineNum - 1 - contextBefore);
  const end = Math.min(lines.length, nextLine - 1);
  return lines.slice(start, end).map((line, i) => {
    const idx = start + i + 1;
    return `${idx === lineNum ? "->" : "  "} ${String(idx).padStart(4)} | ${line}`;
  }).join("\n");
}

function synthesizeDocs(repoRoot: string, data: any, module: any, symbol: any): string {
  const sourcePath = symbol.source_path || module.source_path || "";
  const lineNum = symbol.line;
  if (sourcePath && lineNum) {
    const filePath = join(repoRoot, sourcePath);
    if (existsSync(filePath)) {
      const lines = readFileSync(filePath, "utf-8").split("\n");
      const start = Math.max(0, lineNum - 9);
      const commentLines: string[] = [];
      for (let i = start; i < Math.min(lines.length, lineNum); i++) {
        const stripped = lines[i].trim();
        if (stripped.startsWith("///") || stripped.startsWith("//")) commentLines.push(stripped.replace(/^\/\/+/, "").trim());
        else if (stripped) commentLines.length = 0;
      }
      if (commentLines.length > 0) return commentLines.join("\n");
    }
  }
  const kind = symbol.kind || "symbol";
  const modPath = module.import_path || "std::?";
  const name = symbol.name || "?";
  const phrases: Record<string, string> = {
    function: `Function \`${name}\` in \`${modPath}\`.`,
    fn: `Function \`${name}\` in \`${modPath}\`.`,
    struct: `Struct \`${name}\` in \`${modPath}\`.`,
    actor: `Actor \`${name}\` in \`${modPath}\`.`,
    const: `Constant \`${name}\` in \`${modPath}\`.`,
    enum: `Enum \`${name}\` in \`${modPath}\`.`,
    trait: `Trait \`${name}\` in \`${modPath}\`.`,
  };
  return `*[synthesized]* ${phrases[kind] || \`\`\${kind}\` item \`\${name}\` in \`\${modPath}\`.\`}\n\nSignature: \`\${symbol.signature || symbol.name}\`\n\nUse \`kain_stdlib → get_source\` for the full implementation.`;
}

// Keyword data
const KAIN_KEYWORDS: Record<string, { summary: string; description: string; syntax: string }> = {
  world: { summary: "Declares a compiler-owned state graph.", description: "A 'world' acts as a boundary for state management with state variables and UI surface mappings.", syntax: "world Authority:\n    state count: Int = 0\n    surface native_ui => Panel" },
  entangle: { summary: "Synchronizes state variables reactively between worlds.", description: "Declares a reactive link between states of different worlds with a synchronization policy.", syntax: 'entangle Authority.count <-> Mirror.count_copy with single_writer' },
  converge: { summary: "Multi-lane function with reference + fast lanes.", description: "Defines a function that converges on one behavior but selects optimized lanes at runtime via CPUID.", syntax: 'converge compute(value: Int) -> Int:\n    spec reference:\n        return scalar_mix(value)\n    fast avx2_lane when capability("cpu.x86.avx2"):\n        return simd_mix(value)\n    verify random(8)' },
  actor: { summary: "Declares a concurrent message-passing actor.", description: "An actor contains isolated state and processes messages asynchronously from a mailbox.", syntax: 'actor Worker:\n    state budget: Int = 100\n    on Process(reply_to: P, request: Int):\n        self.budget = self.budget - 1\n        send reply_to.Reply(value = request * 17)' },
  shatter: { summary: "Zero-copy layout struct for world crossing.", description: "Compiler-aligned for zero-copy transfers across world boundaries.", syntax: 'shatter struct Shard:\n    bias: Int\n    phase: Int' },
  teleport: { summary: "Zero-copy moves a shattered struct across worlds.", description: "Ownership transfer between worlds via runtime bus.", syntax: "let moved = teleport s from Authority to Mirror via bus" },
  pulse: { summary: "Periodic execution clock block.", description: "Code executed periodically at a set interval with optional jitter.", syntax: "pulse clock every 8ms jitter 1ms:\n    let s = Shard { bias: 1, phase: 2 }\n    let moved = teleport s from Authority to Mirror via bus" },
  law: { summary: "Compile-time invariant predicate (Z3).", description: "Safety predicate or invariant verified by Z3 during compilation.", syntax: "law value_in_range(v: Int) -> Bool:\n    return v >= 0 and v < 1000000007" },
  patch: { summary: "Transactional mutation on world state.", description: "Mutation function that updates world state while ensuring invariants.", syntax: 'patch update(target: Authority, v: Int) -> Int:\n    target.count = v\n    return target.count' },
  collapse: { summary: "Raw memory borrow scope (lifetime-checked).", description: "Exclusive write/read access to raw pointers, verified at compile time.", syntax: 'collapse cells:\n    var i: Int = 0\n    while i < 1024:\n        mem_store(ptr_offset(cells, i, "Int"), i * 3, "Int")\n        i = i + 1\n    0' },
  observe: { summary: "Read-only borrow on raw memory.", description: "Opens a read-only view on pointer memory under compiler checks.", syntax: 'let head: Int = observe cells:\n    mem_load(ptr_offset(cells, 0, "Int"), "Int")' },
  decay: { summary: "Deallocates raw memory.", description: "Explicitly deallocates a raw memory buffer.", syntax: "decay cells" },
  shader: { summary: "GPU vertex/fragment/compute kernel.", description: "Compiled directly to SPIR-V or CUDA PTX.", syntax: 'shader fragment FieldFrag(uv: Vec2) -> Vec4:\n    uniform accent: Vec3 @0\n    let ring: Float = fbm2(uv, 4)\n    return vec4(accent.x * ring, accent.y, accent.z, 1.0)' },
  orchestrate: { summary: "Multi-language coordination pipeline.", description: "Declares a pipeline across Kain, C, Rust, or Python.", syntax: 'orchestrate pipeline(value: Int) -> Int:\n    let mixed: Int = kain compute(value)\n    let bridged: Int = c c_abi.mix(value, 19)\n    let staged: Int = rust compute(value)\n    return staged' },
  resonate: { summary: "Tripwire state-to-execution handler.", description: "Reactive trigger on a world state slot with optional dampen period.", syntax: "resonate World.field dampen 16ms:\n    // handler on state change" },
  axiom: { summary: "Solver constraint / compile-time capability.", description: "Static assertions checked by Z3 during pipeline selection.", syntax: 'axiom smoke_machine_truth:\n    when target("llvm")\n    when capability("memory.shatter")\n    guarantee "smoke lane active"\n    fallback scalar_lane' },
  stage: { summary: "Orchestration pipeline step config.", description: "Capabilities, GPU memory transfers, residency, latency policies.", syntax: 'stage result: gpu kernel(value)\n    when capability("gpu.compute")\n    residency device\n    transfer host_to_device\n    fallback degrade cpu_seed\n    policy telemetry_prefer_gpu' },
  include: { summary: "Foreign C FFI header import.", description: "Binds native API declarations from C headers.", syntax: 'include "native_helper.h" as c_abi' },
  component: { summary: "UI component definition.", description: "Elements in native_ui or web surface layout.", syntax: "component Panel(width: Int, height: Int) { }" },
  spawn: { summary: "Instantiates a concurrent actor.", description: "Spawns a new actor on the runtime substrate.", syntax: "let worker = spawn Worker(budget = 50)" },
  send: { summary: "Async message to actor mailbox.", description: "Dispatches a message without blocking.", syntax: "send worker.Process(reply_to = self, request = 42)" },
  receive: { summary: "Actor message-matching block.", description: "Matches incoming messages in an actor mailbox.", syntax: 'receive:\n    on Reply(value: Int) => value' },
  emit: { summary: "Raises a reactive state event.", description: "Triggers an event from a component or actor.", syntax: "emit value_changed(new_value = 10)" },
  comptime: { summary: "Compile-time evaluation.", description: "Evaluates the expression statically at compile time.", syntax: "let config = comptime load_config_file()" },
  dispatch: { summary: "Launches GPU compute kernel.", description: "Launches a compute shader with grid dimensions.", syntax: 'dispatch "FieldFrag" [256, 256, 1]' },
  single_writer: { summary: "Single-writer entanglement policy.", description: "Unidirectional state propagation with one active writer.", syntax: "entangle A.val <-> B.val with single_writer" },
};

function getCatalogKeywords(): Record<string, string[]> {
  if (_catalogKeywords) return _catalogKeywords;
  _catalogKeywords = {};
  const catalogPath = join(getRepoRoot(), "CATALOG.md");
  if (!existsSync(catalogPath)) return _catalogKeywords;
  const content = readFileSync(catalogPath, "utf-8");
  let currentCategory = "General";
  for (const sec of content.split("## ")) {
    if (sec.startsWith("4. Flat Master List") || sec.startsWith("5.") || sec.startsWith("6.")) continue;
    for (const line of sec.split("\n")) {
      const t = line.trim();
      if (t.startsWith("### ")) currentCategory = t.replace(/^###\s+/, "").trim();
      else if (t.startsWith("`") || t.startsWith("- `")) {
        const found = t.match(/`([^`]+)`/g);
        if (found) {
          const words = found.map((m) => m.replace(/`/g, "").trim()).filter((w) => /^[a-zA-Z_][a-zA-Z0-9_]*$/.test(w));
          if (words.length > 0) {
            if (!_catalogKeywords[currentCategory]) _catalogKeywords[currentCategory] = [];
            _catalogKeywords[currentCategory].push(...words);
          }
        }
      }
    }
  }
  for (const cat of Object.keys(_catalogKeywords)) _catalogKeywords[cat] = [...new Set(_catalogKeywords[cat])].sort();
  return _catalogKeywords;
}

// ===========================================================================
// Action handlers
// ===========================================================================

function handleListModules() {
  const data = getStdlibData();
  const lines = ["### Kain Standard Library Modules", "", "| Module | Public | Private | Source |", "| :--- | ---: | ---: | :--- |"];
  for (const mod of iterModules(data)) {
    const [pub, priv] = modulePublicCounts(mod);
    lines.push(`| \`std::${mod.name}\` | ${pub} | ${priv} | \`${mod.source_path}\` |`);
  }
  return lines.join("\n");
}

function handleGetSymbols(moduleName: string, includePrivate: boolean) {
  const data = getStdlibData();
  const mod = findModule(data, moduleName);
  const [pub, priv] = modulePublicCounts(mod);
  const lines = [`### Module \`std::${mod.name}\``, `**Source:** \`${mod.source_path}\` — **Public:** ${pub} — **Private:** ${priv}`, "", "| Symbol | Kind | Signature |", "| :--- | :--- | :--- |"];
  for (const sym of mod.symbols ?? []) {
    if (!includePrivate && sym.visibility !== "public") continue;
    lines.push(`| \`${sym.name}\` | \`${sym.kind}\` | \`${sym.signature || sym.name}\` |`);
  }
  return lines.join("\n");
}

function handleSearchSymbols(query: string, moduleName?: string, kind?: string, includePrivate?: boolean, limit?: number) {
  const data = getStdlibData();
  const modules = moduleName ? [findModule(data, moduleName)] : iterModules(data);
  const pairs: [any, any][] = [];
  for (const mod of modules) {
    for (const sym of mod.symbols ?? []) {
      if (!includePrivate && sym.visibility !== "public") continue;
      if (kind && sym.kind !== kind) continue;
      if (searchSymbol(sym, query)) pairs.push([mod, sym]);
    }
  }
  if (pairs.length === 0) return `No symbols matching '${query}'.`;
  const cap = limit ?? 50;
  const limited = pairs.slice(0, cap);
  const lines = [`Found ${pairs.length} matching symbol(s) (showing ${Math.min(cap, pairs.length)}):`, "", "| Module | Symbol | Kind | Signature |", "| :--- | :--- | :--- | :--- |"];
  for (const [mod, sym] of limited) lines.push(`| \`${mod.import_path}\` | \`${sym.name}\` | \`${sym.kind}\` | \`${sym.signature || sym.name}\` |`);
  if (pairs.length > cap) lines.push(`\n*${pairs.length - cap} more — refine query or increase limit.*`);
  return lines.join("\n");
}

function handleGetDetails(moduleName: string, symbolName: string) {
  const root = getRepoRoot();
  const data = getStdlibData();
  const mod = findModule(data, moduleName);
  const sym = mod.symbols?.find((s: any) => s.name === symbolName);
  if (!sym) return `Symbol '${symbolName}' not found in '${moduleName}'.`;
  const lines = [`## Symbol \`${mod.import_path}::${sym.name}\``, "", `**Kind:** \`${sym.kind}\` — **Visibility:** \`${sym.visibility}\``, `**Location:** \`${sym.source_path}:${sym.line}\``, "", "### Signature", "```kn", sym.signature || sym.name, "```"];
  if (sym.docs) lines.push("", "### Documentation", Array.isArray(sym.docs) ? sym.docs.join("\n") : String(sym.docs));
  else lines.push("", "### Documentation", synthesizeDocs(root, data, mod, sym));
  return lines.join("\n");
}

function handleGetSource(moduleName: string, symbolName: string, contextBefore?: number) {
  const root = getRepoRoot();
  const data = getStdlibData();
  const mod = findModule(data, moduleName);
  const sym = mod.symbols?.find((s: any) => s.name === symbolName);
  if (!sym) return `Symbol '${symbolName}' not found in '${moduleName}'.`;
  const source = extractSymbolSource(root, data, sym, contextBefore ?? 2);
  return `Source for \`${mod.import_path}::${symbolName}\` (${sym.source_path}:${sym.line}):\n\n\`\`\`kn\n${source}\n\`\`\``;
}

function handleListKeywords() {
  const root = getRepoRoot();
  const categories = getCatalogKeywords();
  const semanticKeys = new Set(Object.keys(KAIN_KEYWORDS));
  const lines = ["# Kain Language Keywords", "", "## 1. Compiler-Owned Constructs", ""];
  for (const [kw, d] of Object.entries(KAIN_KEYWORDS).sort()) {
    lines.push(`### \`${kw}\``, `> ${d.summary}`, `> ${d.description}`, "```kn", d.syntax, "```", "");
  }
  lines.push("---", "", "## 2. Standard Keywords", "");
  for (const [cat, words] of Object.entries(categories).sort()) {
    const std = words.filter((w) => !semanticKeys.has(w.toLowerCase()));
    if (std.length > 0) lines.push(`- **${cat}:** ${std.map((w) => `\`${w}\``).join(", ")}`);
  }
  lines.push("", "Use `kain_stdlib → get_keyword` for details on any keyword.");
  return lines.join("\n");
}

function handleGetKeyword(keyword: string) {
  const root = getRepoRoot();
  const catalog = getCatalogKeywords();
  const all: Record<string, { summary: string; description: string; syntax: string }> = {};
  for (const words of Object.values(catalog)) {
    for (const w of words) {
      all[w.toLowerCase()] = { summary: "Kain language keyword.", description: `Standard keyword '${w}' from CATALOG.md.`, syntax: `// See CATALOG.md for '${w}' syntax.` };
    }
  }
  for (const [k, v] of Object.entries(KAIN_KEYWORDS)) all[k.toLowerCase()] = v;
  const details = all[keyword.toLowerCase().trim()];
  if (!details) return `Unknown keyword '${keyword}'.`;
  return `## Keyword \`${keyword}\`\n\n**Summary:** ${details.summary}\n**Description:** ${details.description}\n\n### Syntax\n\`\`\`kn\n${details.syntax}\n\`\`\``;
}

function handleSearchExamples(query: string, limit?: number) {
  const pyBin = process.platform === "win32" ? "py" : "python3";
  const pyCode = [
    "from kaindev.smart_search import smart_search; import json",
    "results = smart_search(" + JSON.stringify(query) + ", limit=" + (limit ?? 3) + ")",
    "print(json.dumps([[r['source'], r['score'], r['text'], r['kind'], r['symbol'], r['line_start'], r['line_end']] for r in results]))",
  ].join("; ");
  const proc = spawnSync(pyBin, ["-3", "-c", pyCode], { cwd: "X:/mcp", encoding: "utf-8", timeout: 120000, maxBuffer: 4 * 1024 * 1024, stdio: ["pipe", "pipe", "ignore"] });
  const raw = (proc.stdout ?? "").trim();
  if (!raw) {
    const errMsg = (proc.stderr ?? "").slice(0, 500);
    return `Search failed: ${errMsg || "empty response (timeout?)"}`;
  }
  try {
    const results: [string, number, string, string, string, number, number][] = JSON.parse(raw);
    if (results.length === 0) return `No examples found for "${query}".`;
    const lines = [`### Kain Code Examples for '${query}'`, ""];
    for (const [source, score, text, kind, symbol, lineStart, lineEnd] of results) {
      lines.push(`#### [${results.indexOf([source, score, text, kind, symbol, lineStart, lineEnd]) + 1}] \`${kind}\` ${symbol} (Score: ${score.toFixed(3)}) — ${source}:${lineStart}`);
      lines.push("```kn", text || "(empty)", "```", "");
    }
    return lines.join("\n");
  } catch (e: any) {
    return `Search parse error: ${e.message}`;
  }
}

// ===========================================================================
// Router tool definition
// ===========================================================================

export default function (pi: ExtensionAPI) {
  pi.registerTool({
    name: "kain_stdlib",
    label: "Kain Stdlib",
    description:
      "Query Kain's complete standard library — modules, symbols, signatures, docs, " +
      "source code, keyword reference, and semantic code search. " +
      "8 actions cover everything from listing modules to searching 11,500 code chunks with PyTorch/CUDA.",
    promptSnippet: "Query Kain stdlib modules, symbols, docs, keywords, and examples",
    promptGuidelines: [
      "Use kain_stdlib for all Kain standard library lookups — finding symbols, checking function signatures, reading source, looking up language keywords, and semantically searching for code examples.",
    ],
    parameters: Type.Object({
      action: Type.Enum(
        {
          list_modules: "list_modules",
          get_symbols: "get_symbols",
          search_symbols: "search_symbols",
          get_details: "get_details",
          get_source: "get_source",
          list_keywords: "list_keywords",
          get_keyword: "get_keyword",
          search_examples: "search_examples",
        },
        {
          description:
            "'list_modules' — all modules with stats | " +
            "'get_symbols' — list symbols in a module | " +
            "'search_symbols' — fuzzy-search across stdlib | " +
            "'get_details' — full docs for a symbol | " +
            "'get_source' — read the actual source | " +
            "'list_keywords' — keyword reference | " +
            "'get_keyword' — help for one keyword | " +
            "'search_examples' — semantic code search",
        },
      ),
      module_name: Type.Optional(Type.String({ description: "Module name for get_symbols, get_details, get_source (e.g. 'math', 'os', 'json')." })),
      symbol_name: Type.Optional(Type.String({ description: "Symbol name for get_details, get_source (e.g. 'sin', 'os_mmap_anon')." })),
      query: Type.Optional(Type.String({ description: "Search term for search_symbols or search_examples." })),
      kind: Type.Optional(Type.String({ description: "Optional kind filter for search_symbols: 'function', 'struct', 'actor', 'const', 'enum', 'trait'." })),
      keyword: Type.Optional(Type.String({ description: "Keyword name for get_keyword (e.g. 'world', 'teleport', 'fn', 'struct')." })),
      include_private: Type.Optional(Type.Boolean({ description: "Include private symbols (default false). Used with get_symbols, search_symbols." })),
      limit: Type.Optional(Type.Number({ description: "Max results for search_symbols (default 50) or search_examples (default 3)." })),
      context_before: Type.Optional(Type.Number({ description: "Lines of context before the definition for get_source (default 2)." })),
    }),
    async execute(_toolCallId: string, params: any, _signal: AbortSignal, _onUpdate: any, _ctx: any) {
      try {
        let result: string;

        switch (params.action) {
          case "list_modules":
            result = handleListModules();
            break;

          case "get_symbols":
            if (!params.module_name) return { content: [{ type: "text", text: "Provide `module_name` (e.g. 'math', 'os')." }], details: {}, isError: true };
            result = handleGetSymbols(params.module_name, params.include_private ?? false);
            break;

          case "search_symbols":
            if (!params.query) return { content: [{ type: "text", text: "Provide `query` to search." }], details: {}, isError: true };
            result = handleSearchSymbols(params.query, params.module_name, params.kind, params.include_private, params.limit);
            break;

          case "get_details":
            if (!params.module_name || !params.symbol_name) return { content: [{ type: "text", text: "Provide `module_name` and `symbol_name`." }], details: {}, isError: true };
            result = handleGetDetails(params.module_name, params.symbol_name);
            break;

          case "get_source":
            if (!params.module_name || !params.symbol_name) return { content: [{ type: "text", text: "Provide `module_name` and `symbol_name`." }], details: {}, isError: true };
            result = handleGetSource(params.module_name, params.symbol_name, params.context_before);
            break;

          case "list_keywords":
            result = handleListKeywords();
            break;

          case "get_keyword":
            if (!params.keyword) return { content: [{ type: "text", text: "Provide `keyword` (e.g. 'world', 'teleport', 'fn')." }], details: {}, isError: true };
            result = handleGetKeyword(params.keyword);
            break;

          case "search_examples":
            if (!params.query) return { content: [{ type: "text", text: "Provide `query` for semantic search." }], details: {}, isError: true };
            result = handleSearchExamples(params.query, params.limit);
            break;

          default:
            return { content: [{ type: "text", text: `Unknown action '${params.action}'. Valid: list_modules, get_symbols, search_symbols, get_details, get_source, list_keywords, get_keyword, search_examples.` }], details: {}, isError: true };
        }

        return { content: [{ type: "text", text: result }], details: {} };
      } catch (e: any) {
        return { content: [{ type: "text", text: `Error: ${e.message}` }], details: {}, isError: true };
      }
    },
  });

  pi.on("session_start", async (_event, ctx) => {
    ctx.ui.notify("📚 Kain Stdlib tools loaded — 8 actions in 1 router", "info");
  });
}
