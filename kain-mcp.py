#!/usr/bin/env python3
"""Query Kain's generated stdlib map and interact with it via CLI or MCP."""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from pathlib import Path
from typing import Any

# Safely attempt to import FastMCP for MCP server capability
try:
    from fastmcp import FastMCP
except ImportError:
    FastMCP = None

# Safely attempt to import PyTorch for semantic search capability
try:
    import torch
except ImportError:
    torch = None

# ANSI colors for styling the output on interactive terminals
class Colors:
    CYAN = "\033[96m"
    GREEN = "\033[92m"
    YELLOW = "\033[93m"
    GRAY = "\033[90m"
    BOLD = "\033[1m"
    RESET = "\033[0m"
    RED = "\033[91m"

# Enable colors if stdout is a TTY
if sys.stdout.isatty():
    if os.name == "nt":
        # Enable ANSI codes in Windows 10+ Command Prompt
        os.system("")
    C_CYAN = Colors.CYAN
    C_GREEN = Colors.GREEN
    C_YELLOW = Colors.YELLOW
    C_GRAY = Colors.GRAY
    C_BOLD = Colors.BOLD
    C_RESET = Colors.RESET
    C_RED = Colors.RED
else:
    C_CYAN = ""
    C_GREEN = ""
    C_YELLOW = ""
    C_GRAY = ""
    C_BOLD = ""
    C_RESET = ""
    C_RED = ""


# --- Kain Unique & Semantic Keywords Reference Database ---
# The catalog list dynamically extends this with generic keywords.
KAIN_KEYWORDS = {
    "world": {
        "summary": "Declares a compiler-owned state graph or shared namespace.",
        "description": (
            "A 'world' acts as a boundary for state management. It defines state variables "
            "and UI/surface mappings. Worlds are synchronized reactively using entanglements."
        ),
        "syntax": (
            "world Authority:\n"
            "    state count: Int = 0\n"
            "    surface native_ui => Panel"
        )
    },
    "entangle": {
        "summary": "Synchronizes state variables reactively between different worlds.",
        "description": (
            "Declares a reactive link between states of different worlds, establishing "
            "a synchronization policy (e.g., single_writer)."
        ),
        "syntax": "entangle Authority.count <-> Mirror.count_copy with single_writer"
    },
    "converge": {
        "summary": "Declares a multi-lane function with reference implementation and target-specific fast lanes.",
        "description": (
            "Defines a function that converges on a single semantic behavior but chooses "
            "optimized execution lanes at runtime based on the target system capabilities."
        ),
        "syntax": (
            "converge compute(value: Int) -> Int:\n"
            "    spec reference:\n"
            "        return scalar_mix(value)\n"
            "    fast closed_form_lane when target(\"llvm\"):\n"
            "        return (value * 31 + 7) % MODULUS\n"
            "    verify random(8)"
        )
    },
    "actor": {
        "summary": "Declares an actor component for concurrent message passing.",
        "description": (
            "An actor contains isolated state and processes messages asynchronously from a mailbox."
        ),
        "syntax": (
            "actor Worker:\n"
            "    state budget: Int = 100\n"
            "    on Process(reply_to: P, request: Int):\n"
            "        self.budget = self.budget - 1\n"
            "        send reply_to.Reply(value = request * 17)"
        )
    },
    "shatter": {
        "summary": "Defines a structured type optimized for zero-copy layout and crossing.",
        "description": (
            "A 'shatter struct' is compiler-aligned for zero-copy transfers across world "
            "boundaries and memory grids."
        ),
        "syntax": (
            "shatter struct Shard:\n"
            "    bias: Int\n"
            "    phase: Int"
        )
    },
    "teleport": {
        "summary": "Zero-copy moves a shattered struct across world boundaries.",
        "description": (
            "Transfers ownership of a shattered object from one world to another via "
            "a runtime bus/bridge."
        ),
        "syntax": "let moved = teleport s from Authority to Mirror via bus"
    },
    "pulse": {
        "summary": "Registers a periodic execution clock block.",
        "description": (
            "Defines a block of code that is executed periodically at a specific interval "
            "with optional jitter."
        ),
        "syntax": (
            "pulse clock every 8ms jitter 1ms:\n"
            "    let s = Shard { bias: 1, phase: 2 }\n"
            "    let moved = teleport s from Authority to Mirror via bus"
        )
    },
    "law": {
        "summary": "Defines a compile-time invariant predicate checked by the solver.",
        "description": (
            "Specifies a safety predicate or invariant verified by Z3 during compilation."
        ),
        "syntax": (
            "law value_in_range(v: Int) -> Bool:\n"
            "    return v >= 0 and v < 1000000007"
        )
    },
    "patch": {
        "summary": "Declares a transactional mutation on a world's state.",
        "description": (
            "A transactional mutation function that updates world state while ensuring invariants."
        ),
        "syntax": (
            "patch update(target: Authority, v: Int) -> Int:\n"
            "    target.count = v\n"
            "    return target.count"
        )
    },
    "collapse": {
        "summary": "Enters a raw memory borrow scope verifying lifetime constraints.",
        "description": (
            "Defines a block with exclusive write/read access to raw pointers, verified at compile time."
        ),
        "syntax": (
            "collapse cells:\n"
            "    var i: Int = 0\n"
            "    while i < 1024:\n"
            "        mem_store(ptr_offset(cells, i, \"Int\"), i * 3, \"Int\")\n"
            "        i = i + 1\n"
            "    0"
        )
    },
    "observe": {
        "summary": "Performs a read-only borrow on raw memory within a lifetime check.",
        "description": (
            "Opens a read-only view on pointer memory under compiler checks."
        ),
        "syntax": (
            "let head: Int = observe cells:\n"
            "    mem_load(ptr_offset(cells, 0, \"Int\"), \"Int\")"
        )
    },
    "decay": {
        "summary": "Deallocates raw memory, terminating its lifetime.",
        "description": (
            "Explicitly deallocates a raw memory buffer."
        ),
        "syntax": "decay cells"
    },
    "shader": {
        "summary": "Declares native GPU vertex, fragment, or compute kernels.",
        "description": (
            "Defines compute or rendering shaders compiled directly to SPIR-V or CUDA PTX."
        ),
        "syntax": (
            "shader fragment FieldFrag(uv: Vec2) -> Vec4:\n"
            "    uniform accent: Vec3 @0\n"
            "    let ring: Float = fbm2(uv, 4)\n"
            "    return vec4(accent.x * ring, accent.y, accent.z, 1.0)"
        )
    },
    "orchestrate": {
        "summary": "Defines multi-language coordination pipelines.",
        "description": (
            "Declares a pipeline coordinating execution across Kain, C, Rust, or Python."
        ),
        "syntax": (
            "orchestrate pipeline(value: Int) -> Int:\n"
            "    let mixed: Int = kain compute(value)\n"
            "    let bridged: Int = c c_abi.mix(value, 19)\n"
            "    let staged: Int = rust compute(value)\n"
            "    return staged"
        )
    },
    "resonate": {
        "summary": "Declares a tripwire state-to-execution handler.",
        "description": (
            "Registers a reactive trigger on a world state slot with optional dampen period. "
            "The compiler inserts tripwire hooks and validates target bounds."
        ),
        "syntax": (
            "resonate World.field dampen 16ms:\n"
            "    // handler code triggered on state change"
        )
    },
    "axiom": {
        "summary": "Defines a solver constraint or compile-time capability check.",
        "description": (
            "Specifies static assertions, environment parameters, or compile-time "
            "targets checked by Z3 during compiler pipeline selection."
        ),
        "syntax": (
            "axiom smoke_machine_truth:\n"
            "    when target(\"llvm\")\n"
            "    when capability(\"memory.shatter\")\n"
            "    guarantee \"smoke lane active\"\n"
            "    fallback scalar_lane"
        )
    },
    "stage": {
        "summary": "Specifies execution parameters for orchestration pipeline steps.",
        "description": (
            "Declares configuration metadata (e.g. capabilities, GPU memory transfers, "
            "residency, and latency policies) for steps inside orchestrated workflows."
        ),
        "syntax": (
            "stage result: gpu kernel(value)\n"
            "    when capability(\"gpu.compute\")\n"
            "    residency device\n"
            "    transfer host_to_device\n"
            "    fallback degrade cpu_seed\n"
            "    policy telemetry_prefer_gpu"
        )
    },
    "include": {
        "summary": "Imports foreign C FFI headers.",
        "description": (
            "Binds native API declarations from C header files or system registries (Vulkan, POSIX, WinSDK)."
        ),
        "syntax": (
            "include \"native_helper.h\" as c_abi\n"
            "include <stdio.h> as cstdio"
        )
    },
    "component": {
        "summary": "Declares a UI component definition.",
        "description": (
            "UI components represent elements in the Kain native_ui or web surface layout."
        ),
        "syntax": (
            "component Panel(width: Int, height: Int) {\n"
            "    // UI definition\n"
            "}"
        )
    },
    "spawn": {
        "summary": "Instantiates and runs a concurrent actor instance.",
        "description": (
            "Spawns a new concurrent actor component running on the actor runtime substrate."
        ),
        "syntax": "let worker = spawn Worker(budget = 50)"
    },
    "send": {
        "summary": "Asynchronously sends a message to an actor's mailbox.",
        "description": (
            "Dispatches a message to the target actor's mailbox without blocking execution."
        ),
        "syntax": "send worker.Process(reply_to = self, request = 42)"
    },
    "receive": {
        "summary": "Defines actor message-matching block.",
        "description": (
            "Blocks or registers handler to match incoming messages in an actor mailbox."
        ),
        "syntax": (
            "receive:\n"
            "    on Reply(value: Int) => value"
        )
    },
    "emit": {
        "summary": "Raises a reactive state event.",
        "description": (
            "Signals an event trigger from a component or actor to bound surfaces."
        ),
        "syntax": "emit value_changed(new_value = 10)"
    },
    "comptime": {
        "summary": "Enforces compile-time evaluation of a block or expression.",
        "description": (
            "Instructs the compiler to evaluate the expression statically at compile time."
        ),
        "syntax": "let config = comptime load_config_file()"
    },
    "dispatch": {
        "summary": "Launches a GPU compute shader kernel.",
        "description": (
            "Launches a compute shader with target grid dimensions."
        ),
        "syntax": "dispatch \"FieldFrag\" [256, 256, 1]"
    },
    "single_writer": {
        "summary": "Specifies a single-writer entanglement synchronization policy.",
        "description": (
            "Enforces at compile time that state changes propagate unidirectionally with only one active writer."
        ),
        "syntax": "entangle A.val <-> B.val with single_writer"
    }
}


def find_repo_root(start: Path) -> Path:
    for candidate in [start, *start.parents]:
        if (candidate / "stdlib" / "stdlib.map.json").is_file():
            return candidate
    raise SystemExit("could not find stdlib/stdlib.map.json from current directory")


def normalize_module(value: str) -> str:
    value = value.strip()
    if value.startswith("std::"):
        value = value.removeprefix("std::")
    value = value.replace("/", "::")
    if value == "graphics::shared":
        return "graphics::shared"
    return value


def load_map(repo_root: Path) -> dict[str, Any]:
    path = repo_root / "stdlib" / "stdlib.map.json"
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        raise SystemExit(f"missing generated map: {path}") from None
    except json.JSONDecodeError as exc:
        raise SystemExit(f"invalid generated map {path}: {exc}") from exc


def module_public_counts(module: dict[str, Any]) -> tuple[int, int]:
    symbols = module.get("symbols", [])
    public = sum(1 for symbol in symbols if symbol.get("visibility") == "public")
    private = len(symbols) - public
    return public, private


def iter_modules(data: dict[str, Any]) -> list[dict[str, Any]]:
    return list(data.get("modules", []))


def find_module(data: dict[str, Any], name: str) -> dict[str, Any]:
    needle = normalize_module(name)
    for module in iter_modules(data):
        if module.get("name") == needle or module.get("import_path") == f"std::{needle}":
            return module
    available = ", ".join(module.get("import_path", module.get("name", "")) for module in iter_modules(data))
    raise SystemExit(f"unknown module '{name}'. available: {available}")


def include_symbol(symbol: dict[str, Any], args: argparse.Namespace) -> bool:
    if not args.private and symbol.get("visibility") != "public":
        return False
    if args.kind and symbol.get("kind") != args.kind:
        return False
    if args.contains:
        haystack = " ".join(
            str(symbol.get(field, ""))
            for field in ("name", "qualified_name", "signature", "source_path", "kind", "visibility")
        ).lower()
        if args.contains.lower() not in haystack:
            return False
    return True


def search_symbol(symbol: dict[str, Any], query: str) -> bool:
    """Multi-strategy symbol search: exact substring OR all query tokens present in the haystack.
    
    This handles cases like:
    - query='sin' matches 'fast_sin', 'sin_cos' (token split on underscore)
    - query='fast sin' matches 'fast_sin' (all words present)
    - query='vec3 cross' matches 'vec3_cross' (token-aware)
    """
    haystack = " ".join(
        str(symbol.get(field, ""))
        for field in ("name", "qualified_name", "signature", "source_path", "kind", "visibility", "docs")
    ).lower()
    q = query.lower().strip()
    
    # Strategy 1: direct substring hit
    if q in haystack:
        return True
    
    # Strategy 2: expand haystack tokens by splitting underscored identifiers so
    # 'sin' matches 'fast_sin', 'sin_cos', and 'vec3_sin_angle'
    expanded = re.sub(r'[_\-]', ' ', haystack)
    if q in expanded:
        return True
    
    # Strategy 3: all query tokens must appear in the expanded haystack
    tokens = q.split()
    if len(tokens) > 1:
        return all(tok in expanded for tok in tokens)
    
    # Strategy 4: single short token — match as word boundary in identifiers
    return bool(re.search(r'(?<![a-z])' + re.escape(q) + r'(?![a-z])', expanded))


def symbol_line(module: dict[str, Any], symbol: dict[str, Any]) -> str:
    visibility = symbol.get("visibility", "?")
    kind = symbol.get("kind", "?")
    name = symbol.get("name", "?")
    line = symbol.get("line", "?")
    source = symbol.get("source_path", module.get("source_path", "?"))
    signature = symbol.get("signature") or name
    
    vis_color = C_GREEN if visibility == "public" else C_GRAY
    return (
        f"{C_CYAN}{module.get('import_path')}{C_RESET} "
        f"{vis_color}{visibility:<7}{C_RESET} "
        f"{C_YELLOW}{kind:<15}{C_RESET} "
        f"{C_GRAY}{source}:{line}{C_RESET} | "
        f"{C_BOLD}{signature}{C_RESET}"
    )


def print_summary(data: dict[str, Any]) -> None:
    summary = data.get("summary", {})
    print(
        f"{C_BOLD}summary:{C_RESET} "
        f"modules={C_CYAN}{summary.get('module_count')}{C_RESET} "
        f"public_symbols={C_GREEN}{summary.get('public_symbol_count')}{C_RESET} "
        f"total_symbols={C_GREEN}{summary.get('symbol_count')}{C_RESET} "
        f"rust_builtins={C_YELLOW}{summary.get('builtin_count')}{C_RESET} "
        f"native_services={C_YELLOW}{summary.get('native_service_count')}{C_RESET}"
    )
    for module in iter_modules(data):
        public, private = module_public_counts(module)
        print(f"{C_CYAN}{module.get('import_path'):<24}{C_RESET} public={C_GREEN}{public:<4}{C_RESET} private={C_GRAY}{private:<4}{C_RESET} source={C_GRAY}{module.get('source_path')}{C_RESET}")


def print_imports(data: dict[str, Any]) -> None:
    for module in iter_modules(data):
        print(f"use {module.get('import_path')}")


def emit_symbols(module_symbols: list[tuple[dict[str, Any], dict[str, Any]]], args: argparse.Namespace) -> None:
    limited = module_symbols[: args.limit]
    if args.json:
        print(json.dumps([{"module": module.get("import_path"), **symbol} for module, symbol in limited], indent=2))
    else:
        for module, symbol in limited:
            print(symbol_line(module, symbol))
        if len(module_symbols) > args.limit:
            print(f"... truncated {len(module_symbols) - args.limit} more; raise --limit for more", file=sys.stderr)


def extract_symbol_source(repo_root: Path, data: dict[str, Any], symbol: dict[str, Any], context_before: int = 2) -> str:
    """Extracts the actual code block representing the symbol definition from the source file."""
    source_path = symbol.get("source_path")
    if not source_path:
        return "No source path found for this symbol."
    
    file_path = repo_root / source_path
    if not file_path.is_file():
        return f"Source file not found at: {source_path}"
        
    try:
        lines = file_path.read_text(encoding="utf-8").splitlines()
    except Exception as e:
        return f"Failed to read source file {source_path}: {e}"
        
    line_num = symbol.get("line")
    if not line_num or line_num > len(lines):
        return f"Invalid line number {line_num} (file has {len(lines)} lines)"
        
    # Find all symbols sharing this source file to locate where the next one starts
    file_syms = []
    for m in iter_modules(data):
        for s in m.get("symbols", []):
            s_path = s.get("source_path") or m.get("source_path")
            if s_path == source_path:
                file_syms.append(s)
                
    # Find the next symbol's line number in this file
    next_line = len(lines) + 1
    target_line = line_num
    for s in file_syms:
        s_line = s.get("line")
        if s_line and s_line > target_line and s_line < next_line:
            next_line = s_line
            
    start_idx = max(0, target_line - 1 - context_before)
    end_idx = min(len(lines), next_line - 1)
    
    sliced = lines[start_idx:end_idx]
    formatted_lines = []
    for idx, line in enumerate(sliced, start=start_idx + 1):
        marker = "-> " if idx == target_line else "   "
        formatted_lines.append(f"{marker}{idx:4d} | {line}")
        
    return "\n".join(formatted_lines)


# --- Semantic Indexer Subsystem (using PyTorch + optional CUDA) ---
def tokenize(text: str) -> list[str]:
    return re.findall(r'\b\w+\b', text.lower())


class SemanticIndexer:
    def __init__(self, vocab_size: int = 15000):
        self.vocab_size = vocab_size
        self.vocab: dict[str, int] = {}
        self.inv_vocab: list[str] = []
        self.idf: torch.Tensor | None = None
        self.doc_vectors: torch.Tensor | None = None
        self.docs: list[dict[str, Any]] = []

    def build(self, documents: list[dict[str, Any]]):
        if torch is None:
            return
        self.docs = documents
        
        # 1. Count word frequencies
        word_counts = {}
        doc_words = []
        for doc in documents:
            words = tokenize(doc.get("text", ""))
            doc_words.append(words)
            for w in words:
                word_counts[w] = word_counts.get(w, 0) + 1
                
        # Sort words and select top V
        sorted_words = sorted(word_counts.items(), key=lambda x: x[1], reverse=True)
        top_words = [w for w, _ in sorted_words[:self.vocab_size]]
        
        self.vocab = {w: idx for idx, w in enumerate(top_words)}
        self.inv_vocab = top_words
        
        # 2. Compute Term Frequency (TF) and Document Frequency (DF)
        num_docs = len(documents)
        df = torch.zeros(len(self.vocab), dtype=torch.float32)
        tf_data = []
        
        for words in doc_words:
            word_tf = {}
            for w in words:
                if w in self.vocab:
                    word_tf[w] = word_tf.get(w, 0) + 1
            tf_data.append(word_tf)
            for w in word_tf.keys():
                df[self.vocab[w]] += 1.0
                
        # 3. Compute Inverse Document Frequency (IDF)
        self.idf = torch.log(torch.tensor(num_docs, dtype=torch.float32) / (df + 1.0)) + 1.0
        
        # 4. Populate TF-IDF document vectors
        self.doc_vectors = torch.zeros((num_docs, len(self.vocab)), dtype=torch.float32)
        for doc_idx, word_tf in enumerate(tf_data):
            for w, count in word_tf.items():
                w_idx = self.vocab[w]
                self.doc_vectors[doc_idx, w_idx] = count * self.idf[w_idx]
                
        # Normalize vectors to L2 length for fast cosine similarity via dot product
        norms = torch.norm(self.doc_vectors, dim=1, keepdim=True)
        self.doc_vectors = self.doc_vectors / (norms + 1e-8)

    def search(self, query: str, limit: int = 5) -> list[tuple[dict[str, Any], float]]:
        if torch is None or self.doc_vectors is None:
            return []
            
        words = tokenize(query)
        query_tf = {}
        for w in words:
            if w in self.vocab:
                query_tf[w] = query_tf.get(w, 0) + 1
                
        if not query_tf:
            return []
            
        query_vec = torch.zeros(len(self.vocab), dtype=torch.float32)
        for w, count in query_tf.items():
            w_idx = self.vocab[w]
            query_vec[w_idx] = count * self.idf[w_idx]
            
        query_vec = query_vec / (torch.norm(query_vec) + 1e-8)
        
        # Select device: CUDA (GPU) if available, otherwise CPU
        device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
        
        # Run calculation on CUDA/CPU
        doc_vecs_dev = self.doc_vectors.to(device)
        query_vec_dev = query_vec.to(device)
        
        similarities = torch.matmul(doc_vecs_dev, query_vec_dev).cpu()
        
        top_scores, top_indices = torch.topk(similarities, k=min(limit, len(self.docs)))
        
        results = []
        for score, idx in zip(top_scores.tolist(), top_indices.tolist()):
            if score > 0.0:
                results.append((self.docs[idx], score))
        return results


def find_latest_pretrain_file() -> Path:
    processed_dir = Path("X:/ml/processed")
    if processed_dir.is_dir():
        files = list(processed_dir.glob("kain_pretrain_*.jsonl"))
        if files:
            files.sort(key=lambda x: x.name)
            return files[-1]
    return Path("X:/ml/processed/kain_pretrain_2026-06-05T020601320Z.jsonl")


def load_semantic_documents() -> list[dict[str, Any]]:
    jsonl_path = find_latest_pretrain_file()
    raw_dir = Path("X:/ml/raw")
    repo_root = get_repo_root()
    stdlib_dir = repo_root / "stdlib"
    
    docs = []
    
    # 1. Load standard library source files
    if stdlib_dir.is_dir():
        for p in stdlib_dir.rglob("*.kn"):
            try:
                text = p.read_text(encoding="utf-8")
                if text.strip():
                    docs.append({
                        "text": text,
                        "source": f"stdlib/{p.relative_to(stdlib_dir)}",
                        "size": len(text)
                    })
            except Exception:
                continue
                
    # 2. Check and load JSONL file
    if jsonl_path.is_file():
        try:
            with open(jsonl_path, "r", encoding="utf-8") as f:
                for line in f:
                    if not line.strip():
                        continue
                    try:
                        obj = json.loads(line)
                        text = obj.get("text", "")
                        if text and obj.get("size", 0) > 0:
                            docs.append({
                                "text": text,
                                "source": obj.get("source", "unknown"),
                                "size": obj.get("size", 0)
                            })
                    except Exception:
                        continue
        except Exception as e:
            print(f"Error reading JSONL: {e}", file=sys.stderr)
            
    # 3. Fallback/additional load from X:\ml\raw folder
    elif raw_dir.is_dir():
        for p in raw_dir.rglob("*.kn"):
            try:
                text = p.read_text(encoding="utf-8")
                if text.strip():
                    docs.append({
                        "text": text,
                        "source": f"ml/raw/{p.relative_to(raw_dir)}",
                        "size": len(text)
                    })
            except Exception:
                continue
                
    return docs


_cached_indexer: SemanticIndexer | None = None

def get_example_indexer() -> SemanticIndexer:
    global _cached_indexer
    if _cached_indexer is not None:
        return _cached_indexer
        
    jsonl_path = find_latest_pretrain_file()
    cache_jsonl = jsonl_path.with_suffix(".pt")
    raw_dir = Path("X:/ml/raw")
    cache_raw = Path("X:/ml/raw_index.pt")
    
    indexer = SemanticIndexer()
    if torch is None:
        return indexer
        
    # Try JSONL index
    if jsonl_path.is_file():
        if cache_jsonl.is_file() and cache_jsonl.stat().st_mtime > jsonl_path.stat().st_mtime:
            try:
                state = torch.load(cache_jsonl, map_location="cpu")
                indexer.vocab = state["vocab"]
                indexer.inv_vocab = state["inv_vocab"]
                indexer.idf = state["idf"]
                indexer.doc_vectors = state["doc_vectors"]
                indexer.docs = state["docs"]
                _cached_indexer = indexer
                return indexer
            except Exception as e:
                print(f"Warning: failed to load JSONL index cache: {e}. Rebuilding...", file=sys.stderr)
                
        # Recompile JSONL index
        docs = load_semantic_documents()
        if docs:
            indexer.build(docs)
            try:
                torch.save({
                    "vocab": indexer.vocab,
                    "inv_vocab": indexer.inv_vocab,
                    "idf": indexer.idf,
                    "doc_vectors": indexer.doc_vectors,
                    "docs": indexer.docs
                }, cache_jsonl)
            except Exception as e:
                print(f"Warning: failed to save JSONL index cache: {e}", file=sys.stderr)
        _cached_indexer = indexer
        return indexer
        
    # Try raw folder index
    if raw_dir.is_dir():
        if cache_raw.is_file():
            try:
                state = torch.load(cache_raw, map_location="cpu")
                indexer.vocab = state["vocab"]
                indexer.inv_vocab = state["inv_vocab"]
                indexer.idf = state["idf"]
                indexer.doc_vectors = state["doc_vectors"]
                indexer.docs = state["docs"]
                _cached_indexer = indexer
                return indexer
            except Exception as e:
                print(f"Warning: failed to load raw index cache: {e}. Rebuilding...", file=sys.stderr)
                
        # Recompile raw folder index
        docs = load_semantic_documents()
        if docs:
            indexer.build(docs)
            try:
                torch.save({
                    "vocab": indexer.vocab,
                    "inv_vocab": indexer.inv_vocab,
                    "idf": indexer.idf,
                    "doc_vectors": indexer.doc_vectors,
                    "docs": indexer.docs
                }, cache_raw)
            except Exception as e:
                print(f"Warning: failed to save raw index cache: {e}", file=sys.stderr)
        _cached_indexer = indexer
        return indexer
        
    _cached_indexer = indexer
    return indexer


# --- Dynamic Keyword Catalog Loader (from CATALOG.md) ---
def load_keywords_from_catalog(repo_root: Path) -> list[str]:
    categories = load_keywords_by_category(repo_root)
    all_kws = []
    for words in categories.values():
        all_kws.extend(words)
    return sorted(list(set(all_kws)))


def load_keywords_by_category(repo_root: Path) -> dict[str, list[str]]:
    catalog_path = repo_root / "CATALOG.md"
    if not catalog_path.is_file():
        return {}
    try:
        content = catalog_path.read_text(encoding="utf-8")
        categories = {}
        current_category = "General"
        
        # We only want to parse keywords from sections 1, 2, and 3
        sections = content.split("## ")
        for sec in sections:
            # Skip Flat Master List (sec 4), Excludes (sec 5), and Practical Notes (sec 6)
            if sec.startswith("4. Flat Master List") or sec.startswith("5. What This Catalog") or sec.startswith("6. Practical"):
                continue
            
            lines = sec.splitlines()
            for line in lines:
                line = line.strip()
                if line.startswith("### "):
                    current_category = line.removeprefix("### ").strip()
                elif line.startswith("`") or line.startswith("- `"):
                    found = re.findall(r"`([^`]+)`", line)
                    words = []
                    for item in found:
                        item = item.strip()
                        # Only include valid alphabetic/identifier keywords
                        if re.match(r'^[a-zA-Z_][a-zA-Z0-9_]*$', item):
                            words.append(item)
                    if words:
                        categories.setdefault(current_category, []).extend(words)
                        
        cleaned = {}
        for cat, w_list in categories.items():
            words = sorted(list(set(w_list)))
            if words:
                cleaned[cat] = words
        return cleaned
    except Exception as e:
        print(f"Warning parsing CATALOG.md categories: {e}", file=sys.stderr)
    return {}


def get_all_keywords(repo_root: Path) -> dict[str, dict[str, str]]:
    catalog_kws = load_keywords_from_catalog(repo_root)
    merged = {}
    
    # Pre-populate all keywords parsed from CATALOG.md with default/fallback summaries
    for kw in catalog_kws:
        kw_lower = kw.lower()
        merged[kw_lower] = {
            "summary": "Kain language keyword (control, type, or module boundary).",
            "description": f"Standard language keyword '{kw}' defined in the Kain Keyword Catalog (CATALOG.md).",
            "syntax": f"// Refer to CATALOG.md for '{kw}' syntax and usage."
        }
        
    # Overlay our rich descriptions for the unique semantic keywords
    for kw, details in KAIN_KEYWORDS.items():
        merged[kw.lower()] = details
        
    return merged


# --- Global state helpers for MCP server execution ---
_cached_repo_root: Path | None = None

def get_repo_root() -> Path:
    global _cached_repo_root
    if _cached_repo_root is None:
        try:
            _cached_repo_root = find_repo_root(Path(__file__).resolve())
        except SystemExit:
            _cached_repo_root = find_repo_root(Path.cwd().resolve())
    return _cached_repo_root


def get_stdlib_data() -> dict[str, Any]:
    return load_map(get_repo_root())


# --- Print formatting helpers for keywords ---
def print_keywords(repo_root: Path) -> None:
    keywords = get_all_keywords(repo_root)
    print(f"\n{C_BOLD}=== Kain Language Keywords ({len(keywords)}) ==={C_RESET}")
    for kw, details in sorted(keywords.items()):
        print(f"  {C_CYAN}{kw:<15}{C_RESET} - {details['summary']}")
    print(f"\nUse {C_GREEN}--keyword <name>{C_RESET} or REPL {C_GREEN}info <keyword>{C_RESET} for details and syntax.\n")


def print_keyword_detail(repo_root: Path, kw: str) -> int:
    keywords = get_all_keywords(repo_root)
    kw_lower = kw.lower().strip()
    if kw_lower not in keywords:
        print(f"{C_RED}Error:{C_RESET} Unknown keyword '{kw}'. Available keywords: {', '.join(sorted(keywords.keys()))}", file=sys.stderr)
        return 1
    details = keywords[kw_lower]
    print(f"\n{C_BOLD}Keyword:{C_RESET} {C_CYAN}{kw_lower}{C_RESET}")
    print(f"{C_BOLD}Summary:{C_RESET} {details['summary']}")
    print(f"{C_BOLD}Description:{C_RESET}\n  {details['description']}")
    print(f"\n{C_BOLD}Syntax Example:{C_RESET}")
    print(f"```kn\n{details['syntax']}\n```\n")
    return 0


def _synthesize_docs(repo_root: Path, data: dict[str, Any], module: dict[str, Any], symbol: dict[str, Any]) -> str:
    """Attempt to extract docstring comments from source, then fall back to signature-derived prose.
    
    Extraction strategy:
      1. Read the source file and grab the comment block immediately before/after the definition line.
      2. Scan for '//' or '///' comment lines near the symbol definition.
      3. If nothing found, synthesize a minimal description from the signature and kind.
    """
    source_path = symbol.get("source_path") or module.get("source_path", "")
    line_num = symbol.get("line")
    
    if source_path and line_num:
        file_path = repo_root / source_path
        if file_path.is_file():
            try:
                lines = file_path.read_text(encoding="utf-8").splitlines()
                # Scan up to 8 lines before the definition for comment lines
                start = max(0, line_num - 9)
                end = min(len(lines), line_num)
                comment_lines = []
                for raw in lines[start:end]:
                    stripped = raw.strip()
                    if stripped.startswith("///") or stripped.startswith("//"):
                        comment_lines.append(stripped.lstrip("/").strip())
                    elif stripped and not stripped.startswith("//"):
                        # Non-comment, non-empty line resets the block
                        comment_lines = []
                
                if comment_lines:
                    return "\n".join(comment_lines)
                    
                # Also check the line immediately after the definition for inline comments
                if line_num < len(lines):
                    after = lines[line_num].strip()
                    if after.startswith("//"):
                        return after.lstrip("/").strip()
            except Exception:
                pass
    
    # Fallback: synthesize from signature
    sig = symbol.get("signature") or symbol.get("name", "?")
    kind = symbol.get("kind", "symbol")
    mod_path = module.get("import_path", "std::?")
    name = symbol.get("name", "?")
    
    kind_phrases = {
        "function": f"Function `{name}` defined in `{mod_path}`.",
        "fn":       f"Function `{name}` defined in `{mod_path}`.",
        "struct":   f"Struct type `{name}` defined in `{mod_path}`. Fields and layout are described by the signature.",
        "actor":    f"Actor `{name}` in `{mod_path}`. Processes messages concurrently with isolated state.",
        "const":    f"Compile-time constant `{name}` in `{mod_path}`.",
        "type":     f"Type alias `{name}` in `{mod_path}`.",
        "enum":     f"Enum `{name}` in `{mod_path}`. Use pattern matching to destructure variants.",
        "trait":    f"Trait `{name}` in `{mod_path}`. Implement to provide this interface.",
    }
    base = kind_phrases.get(kind, f"`{kind}` item `{name}` in `{mod_path}`.")
    return f"*[synthesized]* {base}\n\nSignature: `{sig}`\n\nUse `get_symbol_source` to view the full implementation."


# --- MCP Server definition and registration ---
if FastMCP is not None:
    mcp = FastMCP(
        "Kain Stdlib",
        instructions=(
            "Provides rocket-powered access to Kain's Standard Library modules, symbols, signatures, documentation, and source code, "
            "as well as a database of custom language keywords and semantically searchable code examples. "
            "Use search_stdlib_symbols to find items, get_symbol_source to inspect implementation, list_kain_keywords for keyword summaries, "
            "get_keyword_help for syntax models, and search_kain_examples for instant ML-powered examples (actors, teleport, shaders, etc.)."
        )
    )
    
    @mcp.tool(name="list_stdlib_modules")
    def list_stdlib_modules_tool() -> str:
        """List all standard library modules in Kain, with their symbol counts and source paths.
        
        Returns:
            A markdown-formatted table of all standard library modules.
        """
        try:
            data = get_stdlib_data()
        except SystemExit as e:
            return f"Error: {e}"
            
        lines = [
            "### Kain Standard Library Modules",
            "",
            "| Module | Public Symbols | Private Symbols | Source File |",
            "| :--- | :---: | :---: | :--- |"
        ]
        for module in iter_modules(data):
            public, private = module_public_counts(module)
            lines.append(f"| `std::{module.get('name')}` | {public} | {private} | `{module.get('source_path')}` |")
        return "\n".join(lines)

    @mcp.tool(name="get_module_symbols")
    def get_module_symbols_tool(module_name: str, include_private: bool = False) -> str:
        """Get all symbols defined in a specific module.
        
        Args:
            module_name: The name of the module (e.g. 'math', 'std::math', 'graphics::shared').
            include_private: Whether to include private symbols (default is False).
            
        Returns:
            A markdown summary and list of symbols.
        """
        try:
            data = get_stdlib_data()
            module = find_module(data, module_name)
        except SystemExit as e:
            return f"Error: {e}"
            
        public, private = module_public_counts(module)
        lines = [
            f"### Module `std::{module.get('name')}`",
            f"- **Source file:** `{module.get('source_path')}`",
            f"- **Public symbols:** {public}",
            f"- **Private symbols:** {private}",
            "",
            "| Symbol | Kind | Visibility | Signature |",
            "| :--- | :--- | :--- | :--- |"
        ]
        for symbol in module.get("symbols", []):
            vis = symbol.get("visibility", "private")
            if not include_private and vis != "public":
                continue
            name = symbol.get("name")
            kind = symbol.get("kind")
            sig = symbol.get("signature") or name
            lines.append(f"| `{name}` | `{kind}` | `{vis}` | `{sig}` |")
            
        return "\n".join(lines)

    @mcp.tool(name="search_stdlib_symbols")
    def search_stdlib_symbols_tool(
        query: str,
        module_name: str | None = None,
        kind: str | None = None,
        include_private: bool = False,
        limit: int = 50
    ) -> str:
        """Search for symbols across the entire standard library or a specific module.
        
        Args:
            query: The search term (matches symbol name, signature, docs, etc.).
            module_name: Optional module name to narrow the search (e.g., 'math').
            kind: Optional kind filter (e.g., 'function', 'struct', 'actor', 'const').
            include_private: Whether to include private symbols (default is False).
            limit: Maximum number of matches to return (default 50).
            
        Returns:
            A markdown-formatted list of matching symbols.
        """
        try:
            data = get_stdlib_data()
        except SystemExit as e:
            return f"Error: {e}"
            
        pairs = []
        modules_to_search = []
        if module_name:
            try:
                modules_to_search = [find_module(data, module_name)]
            except SystemExit as e:
                return f"Error: {e}"
        else:
            modules_to_search = iter_modules(data)
            
        for module in modules_to_search:
            for symbol in module.get("symbols", []):
                if not include_private and symbol.get("visibility") != "public":
                    continue
                if kind and symbol.get("kind") != kind:
                    continue
                if search_symbol(symbol, query):
                    pairs.append((module, symbol))
                    
        if not pairs:
            return f"No symbols matching '{query}' found."
            
        limited = pairs[:limit]
        lines = [
            f"Found {len(pairs)} matching symbols (showing up to {limit}):",
            "",
            "| Module | Symbol | Kind | Signature | Location |",
            "| :--- | :--- | :--- | :--- | :--- |"
        ]
        for mod, sym in limited:
            m_path = mod.get("import_path")
            name = sym.get("name")
            s_kind = sym.get("kind")
            sig = sym.get("signature") or name
            loc = f"{sym.get('source_path')}:{sym.get('line')}"
            lines.append(f"| `{m_path}` | `{name}` | `{s_kind}` | `{sig}` | `{loc}` |")
            
        if len(pairs) > limit:
            lines.append(f"\n*Truncated {len(pairs) - limit} additional results. Refine your query or increase limit.*")
            
        return "\n".join(lines)

    @mcp.tool(name="get_symbol_details")
    def get_symbol_details_tool(module_name: str, symbol_name: str) -> str:
        """Get full details of a specific standard library symbol, including documentation.
        
        Args:
            module_name: The name of the module (e.g. 'math').
            symbol_name: The name of the symbol (e.g. 'sin').
            
        Returns:
            A markdown document detailing the symbol, its docstrings, attributes, and location.
        """
        try:
            repo_root = get_repo_root()
            data = get_stdlib_data()
            module = find_module(data, module_name)
        except SystemExit as e:
            return f"Error: {e}"
            
        symbol = None
        for sym in module.get("symbols", []):
            if sym.get("name") == symbol_name:
                symbol = sym
                break
                
        if not symbol:
            return f"Symbol '{symbol_name}' not found in module '{module_name}'."
            
        lines = [
            f"## Symbol `{module.get('import_path')}::{symbol.get('name')}`",
            "",
            f"- **Kind:** `{symbol.get('kind')}`",
            f"- **Visibility:** `{symbol.get('visibility')}`",
            f"- **Location:** `{symbol.get('source_path')}:{symbol.get('line')}`"
        ]
        
        if symbol.get("attributes"):
            attrs = ", ".join(f"`{a}`" for a in symbol.get("attributes"))
            lines.append(f"- **Attributes:** {attrs}")
            
        if symbol.get("target_notes"):
            notes = ", ".join(symbol.get("target_notes"))
            lines.append(f"- **Target Notes:** {notes}")
            
        lines.extend([
            "",
            "### Signature",
            "```kn",
            symbol.get("signature") or symbol.get("name"),
            "```"
        ])
        
        docs = symbol.get("docs")
        if docs:
            lines.extend([
                "",
                "### Documentation",
                "\n".join(docs) if isinstance(docs, list) else str(docs)
            ])
        else:
            # Synthesize documentation from signature + source context
            synthesized = _synthesize_docs(repo_root, data, module, symbol)
            lines.extend([
                "",
                "### Documentation",
                synthesized
            ])
            
        return "\n".join(lines)

    @mcp.tool(name="get_symbol_source")
    def get_symbol_source_tool(module_name: str, symbol_name: str, context_before: int = 2) -> str:
        """Get the actual Kain source code implementation of a standard library symbol.
        
        Args:
            module_name: The name of the module (e.g. 'math').
            symbol_name: The name of the symbol (e.g. 'sin').
            context_before: Number of lines of context to show before the symbol definition (for annotations/comments).
            
        Returns:
            A code block showing the actual Kain source code of the symbol.
        """
        try:
            repo_root = get_repo_root()
            data = get_stdlib_data()
            module = find_module(data, module_name)
        except SystemExit as e:
            return f"Error: {e}"
            
        symbol = None
        for sym in module.get("symbols", []):
            if sym.get("name") == symbol_name:
                symbol = sym
                break
                
        if not symbol:
            return f"Symbol '{symbol_name}' not found in module '{module_name}'."
            
        source = extract_symbol_source(repo_root, data, symbol, context_before)
        
        return (
            f"Source code for `{module.get('import_path')}::{symbol_name}` "
            f"({symbol.get('source_path')}:{symbol.get('line')}):\n\n"
            f"```kn\n"
            f"{source}\n"
            f"```"
        )

    @mcp.tool(name="list_kain_keywords")
    def list_kain_keywords_tool() -> str:
        """List all core Kain language keywords, providing rich descriptions for unique semantic constructs and compact lists for standard ones."""
        try:
            repo_root = get_repo_root()
            categories = load_keywords_by_category(repo_root)
            keywords = get_all_keywords(repo_root)
        except Exception as e:
            return f"Error loading keywords: {e}"
            
        if not categories:
            return "No keywords found in CATALOG.md."
            
        semantic_keys = set(k.lower() for k in KAIN_KEYWORDS.keys())
        
        lines = [
            "# Kain Language Keywords Reference Manual",
            "This manual documents Kain's unique compiler-owned state, actor, shader, ownership, and FFI constructs in detail, followed by a compact list of standard language keywords.",
            "",
            "## 1. Core Compiler-Owned Intents & Machine Stones (All Killer No Filler)",
            "These are custom Kain-specific constructs verified by the solver/compiler and backed by the native runtime.",
            ""
        ]
        
        for kw in sorted(KAIN_KEYWORDS.keys()):
            details = KAIN_KEYWORDS[kw]
            lines.append(f"### `{kw}`")
            lines.append(f"- **Summary:** {details['summary']}")
            lines.append(f"- **Description:** {details['description']}")
            lines.append("- **Syntax Example:**")
            lines.append("```kn")
            lines.append(details['syntax'])
            lines.append("```")
            lines.append("")
            
        lines.append("---")
        lines.append("")
        lines.append("## 2. Standard & Utility Keywords")
        lines.append("These are standard control, binding, type, and module syntax words similar to other programming languages. They do not require additional ceremony:")
        lines.append("")
        
        for cat, w_list in sorted(categories.items()):
            # Filter out the semantic ones from the category list
            std_in_cat = [w for w in w_list if w.lower() not in semantic_keys]
            if std_in_cat:
                kw_string = ", ".join(f"`{w}`" for w in std_in_cat)
                lines.append(f"- **{cat}**: {kw_string}")
                
        lines.append("")
        lines.append("Use `get_keyword_help` to query details for any standard keyword individually.")
        return "\n".join(lines)

    @mcp.tool(name="get_keyword_help")
    def get_keyword_help_tool(keyword: str) -> str:
        """Get detailed help, descriptions, and syntax models for a specific Kain language keyword.
        
        Args:
            keyword: The keyword to inspect (e.g. 'world', 'teleport', 'fn', 'struct').
        """
        try:
            repo_root = get_repo_root()
            keywords = get_all_keywords(repo_root)
        except Exception as e:
            return f"Error loading keywords: {e}"
            
        kw_lower = keyword.lower().strip()
        if kw_lower not in keywords:
            return f"Unknown keyword '{keyword}'. Available keywords: {', '.join(sorted(keywords.keys()))}"
            
        details = keywords[kw_lower]
        return (
            f"## Keyword `{kw_lower}`\n\n"
            f"- **Summary:** {details['summary']}\n"
            f"- **Description:** {details['description']}\n\n"
            f"### Syntax Example\n"
            f"```kn\n"
            f"{details['syntax']}\n"
            f"```"
        )

    @mcp.tool(name="search_kain_examples")
    def search_kain_examples_tool(query: str, limit: int = 3) -> str:
        """Semantically search the pretraining dataset (~4,500 entries) of instruction and source code examples using PyTorch/CUDA.
        
        Args:
            query: The features or examples you want (e.g. 'actor supervision mailbox', 'teleport zero copy', 'shader fragment').
            limit: Maximum examples to return (default 3).
        """
        if torch is None:
            return "Error: PyTorch is not installed in the server environment. Semantic search is disabled."
            
        indexer = get_example_indexer()
        # Fetch more candidates than needed so deduplication doesn't starve results
        raw_results = indexer.search(query, limit=limit * 6)
        if not raw_results:
            return f"No examples matching '{query}' found."
        
        # Deduplicate by canonical source bucket.
        # Normalise away benchmark/cases_v2 variant paths and duplicate raw copies:
        #   benchmark/cases_v2/file_copy_raw_1234.kn  -> benchmark/cases_v2/file_copy_raw
        #   ml/raw/blades_foo_bar.kn                  -> ml/raw/blades_foo_bar  (first seen wins)
        def _source_bucket(src: str) -> str:
            # Strip trailing numbers that distinguish identical variant files
            bucket = re.sub(r'[_\-]\d+(\.kn)?$', '', src)
            bucket = re.sub(r'\.kn$', '', bucket)
            return bucket
        
        seen_buckets: set[str] = set()
        deduped: list[tuple[dict[str, Any], float]] = []
        for doc, score in raw_results:
            bucket = _source_bucket(doc.get("source", ""))
            if bucket not in seen_buckets:
                seen_buckets.add(bucket)
                deduped.append((doc, score))
            if len(deduped) >= limit:
                break
            
        lines = [f"### Kain Code Examples for '{query}'\n"]
        for idx, (doc, score) in enumerate(deduped, 1):
            lines.extend([
                f"#### [{idx}] Source: `{doc.get('source')}` (Score: {score:.3f})",
                "```kn",
                doc.get("text", "").strip(),
                "```",
                ""
            ])
        return "\n".join(lines)


def interactive_loop(repo_root: Path, data: dict[str, Any]) -> None:
    """An interactive command-line shell to browse standard library symbols and keywords."""
    # Attempt to load readline for better command history
    try:
        import readline
    except ImportError:
        pass

    print(f"\n{C_BOLD}=== Kain Standard Library & Keyword Interactive Explorer ==={C_RESET}")
    print("Type 'help' for available commands, 'exit' to quit.\n")
    
    while True:
        try:
            line = input(f"{C_CYAN}stdlib>{C_RESET} ").strip()
        except (KeyboardInterrupt, EOFError):
            print()
            break
            
        if not line:
            continue
            
        parts = line.split(maxsplit=2)
        cmd = parts[0].lower()
        args = parts[1:]
        
        if cmd in ("exit", "quit", "q"):
            break
        elif cmd == "help":
            print(f"{C_BOLD}Available Commands:{C_RESET}")
            print(f"  {C_GREEN}modules / ls{C_RESET}                    List all stdlib modules")
            print(f"  {C_GREEN}show <module>{C_RESET}                   Show all symbols in <module> (e.g. 'show math')")
            print(f"  {C_GREEN}search <query>{C_RESET}                  Search stdlib symbols across all modules (e.g. 'search FNV')")
            print(f"  {C_GREEN}keywords{C_RESET}                        List all Kain custom language keywords")
            print(f"  {C_GREEN}info <keyword>{C_RESET}                  Show syntax and details for a keyword (e.g. 'info teleport')")
            print(f"  {C_GREEN}info <module> <symbol>{C_RESET}          Show details and documentation for a symbol")
            print(f"  {C_GREEN}source <module> <symbol> [before]{C_RESET} Show source implementation of a symbol")
            print(f"  {C_GREEN}example <query>{C_RESET}                 Semantically search code examples using PyTorch (e.g. 'example actor')")
            print(f"  {C_GREEN}exit / quit / q{C_RESET}                 Exit the shell")
        elif cmd in ("modules", "ls"):
            print_summary(data)
        elif cmd == "keywords":
            print_keywords(repo_root)
        elif cmd == "show":
            if not args:
                print("Usage: show <module>")
                continue
            module_name = args[0]
            try:
                module = find_module(data, module_name)
            except SystemExit as e:
                print(e)
                continue
            public, private = module_public_counts(module)
            print(f"{C_BOLD}Module std::{module.get('name')}{C_RESET} ({C_GRAY}{module.get('source_path')}{C_RESET})")
            print(f"Public: {C_GREEN}{public}{C_RESET}, Private: {C_GRAY}{private}{C_RESET}")
            for symbol in module.get("symbols", []):
                print(symbol_line(module, symbol))
        elif cmd == "search":
            if not args:
                print("Usage: search <query>")
                continue
            query = args[0]
            if len(args) > 1:
                query = query + " " + args[1]
            pairs = []
            for module in iter_modules(data):
                for symbol in module.get("symbols", []):
                    if search_symbol(symbol, query):
                        pairs.append((module, symbol))
            if not pairs:
                print("No matching symbols.")
            else:
                for module, symbol in pairs[:60]:
                    print(symbol_line(module, symbol))
                if len(pairs) > 60:
                    print(f"... and {C_YELLOW}{len(pairs) - 60}{C_RESET} more matches. Narrow down with a more specific query.")
        elif cmd == "info":
            info_args = []
            if args:
                info_args = args[0].split(maxsplit=1)
            if not info_args:
                print("Usage: info <module> <symbol>  OR  info <keyword>")
                continue
                
            first_arg = info_args[0].lower().strip()
            keywords = get_all_keywords(repo_root)
            # If it's a keyword info check
            if first_arg in keywords and len(info_args) == 1:
                print_keyword_detail(repo_root, first_arg)
                continue
                
            if len(info_args) < 2:
                print("Usage: info <module> <symbol>  OR  info <keyword>")
                continue
                
            module_name, symbol_name = info_args[0], info_args[1]
            try:
                module = find_module(data, module_name)
            except SystemExit as e:
                print(e)
                continue
            symbol = next((s for s in module.get("symbols", []) if s.get("name") == symbol_name), None)
            if not symbol:
                print(f"Symbol '{symbol_name}' not found in module '{module_name}'.")
                continue
            print(f"\n{C_BOLD}Symbol:{C_RESET} {C_CYAN}{module.get('import_path')}{C_RESET}::{C_BOLD}{symbol.get('name')}{C_RESET}")
            print(f"{C_BOLD}Kind:{C_RESET} {C_YELLOW}{symbol.get('kind')}{C_RESET}")
            print(f"{C_BOLD}Visibility:{C_RESET} {symbol.get('visibility')}")
            print(f"{C_BOLD}Location:{C_RESET} {C_GRAY}{symbol.get('source_path')}:{symbol.get('line')}{C_RESET}")
            if symbol.get("attributes"):
                print(f"{C_BOLD}Attributes:{C_RESET} {', '.join(symbol.get('attributes'))}")
            if symbol.get("target_notes"):
                print(f"{C_BOLD}Target Notes:{C_RESET} {', '.join(symbol.get('target_notes'))}")
            print(f"\n{C_BOLD}Signature:{C_RESET}\n  {C_GREEN}{symbol.get('signature') or symbol.get('name')}{C_RESET}")
            if symbol.get("docs"):
                print(f"\n{C_BOLD}Documentation:{C_RESET}")
                for doc_line in symbol.get("docs"):
                    print(f"  {doc_line}")
            print()
        elif cmd == "source":
            src_args = []
            if args:
                src_args = args[0].split()
            if len(src_args) < 2:
                print("Usage: source <module> <symbol> [context_before]")
                continue
            module_name, symbol_name = src_args[0], src_args[1]
            context = 2
            if len(src_args) > 2:
                try:
                    context = int(src_args[2])
                except ValueError:
                    pass
            try:
                module = find_module(data, module_name)
            except SystemExit as e:
                print(e)
                continue
            symbol = next((s for s in module.get("symbols", []) if s.get("name") == symbol_name), None)
            if not symbol:
                print(f"Symbol '{symbol_name}' not found in module '{module_name}'.")
                continue
            print(f"\nSource code for `{module.get('import_path')}::{symbol_name}` ({symbol.get('source_path')}:{symbol.get('line')}):")
            print(extract_symbol_source(repo_root, data, symbol, context_before=context))
            print()
        elif cmd in ("example", "examples", "ex"):
            if not args:
                print("Usage: example <query>")
                continue
            query = args[0]
            if len(args) > 1:
                query = query + " " + args[1]
                
            if torch is None:
                print("Error: PyTorch is required for semantic search.")
                continue
                
            print(f"Searching examples for '{query}'...")
            indexer = get_example_indexer()
            results = indexer.search(query, limit=3)
            if not results:
                print("No matching examples found.")
            else:
                for idx, (doc, score) in enumerate(results, 1):
                    print(f"\n{C_BOLD}[{idx}] Example from {doc.get('source')} (Score: {score:.3f}){C_RESET}")
                    print("```kn")
                    print(doc.get("text").strip())
                    print("```")
        else:
            print(f"Unknown command '{cmd}'. Type 'help' for list of commands.")


def main(argv: list[str] | None = None) -> int:
    if argv is None:
        argv = sys.argv[1:]

    # Check for MCP mode
    if argv and argv[0] in ("mcp", "--mcp"):
        if FastMCP is None:
            print("Error: 'fastmcp' package is not installed. Run 'pip install fastmcp'.", file=sys.stderr)
            return 1
        print("Starting Kain Stdlib MCP server on stdio...", file=sys.stderr)
        # Clear sys.argv so FastMCP doesn't see 'mcp'/'--mcp'
        sys.argv = [sys.argv[0]]
        mcp.run()
        return 0

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=None, help="repo root; defaults to nearest parent with stdlib/stdlib.map.json")
    parser.add_argument("--summary", action="store_true", help="print module counts and sources")
    parser.add_argument("--imports", action="store_true", help="print current root import list")
    parser.add_argument("--module", help="module name such as math, std::math, or graphics::shared")
    parser.add_argument("--search", help="search symbols across all modules")
    parser.add_argument("--contains", help="filter symbols by substring when used with --module")
    parser.add_argument("--kind", help="filter by symbol kind such as function, const, struct, actor, extern_function")
    parser.add_argument("--private", action="store_true", help="include private symbols")
    parser.add_argument("--limit", type=int, default=80, help="maximum symbols to print")
    parser.add_argument("--json", action="store_true", help="emit selected symbols as JSON")
    parser.add_argument("--source", action="store_true", help="show source code of matching symbol(s)")
    parser.add_argument("-i", "--interactive", action="store_true", help="start the interactive stdlib explorer shell")
    parser.add_argument("--keywords", action="store_true", help="list all Kain language keywords dynamically parsed from CATALOG.md")
    parser.add_argument("--keyword", help="get detailed help for a specific Kain keyword")
    parser.add_argument("-e", "--example", help="semantically search for Kain code examples")
    args = parser.parse_args(argv)

    if args.repo:
        repo_root = args.repo.resolve()
    else:
        try:
            repo_root = find_repo_root(Path.cwd().resolve())
        except SystemExit:
            repo_root = find_repo_root(Path(__file__).resolve())
    data = load_map(repo_root)

    # 1. Process keyword helper requests
    if args.keywords:
        print_keywords(repo_root)
        return 0

    if args.keyword:
        return print_keyword_detail(repo_root, args.keyword)

    # 2. Process example semantic search requests
    if args.example:
        if torch is None:
            print("Error: PyTorch is required for semantic search.", file=sys.stderr)
            return 1
        print(f"Searching examples for '{args.example}'...", file=sys.stderr)
        indexer = get_example_indexer()
        results = indexer.search(args.example, limit=3)
        if not results:
            print("No matching examples found.", file=sys.stderr)
            return 1
        for idx, (doc, score) in enumerate(results, 1):
            print(f"\n{C_BOLD}[{idx}] Example from {doc.get('source')} (Score: {score:.3f}){C_RESET}")
            print("```kn")
            print(doc.get("text").strip())
            print("```")
        return 0

    # 3. Process interactive explorer shell
    if args.interactive:
        interactive_loop(repo_root, data)
        return 0

    if args.summary or (not any([args.imports, args.module, args.search]) and not args.source):
        print_summary(data)
        return 0

    if args.imports:
        print_imports(data)
        return 0

    pairs: list[tuple[dict[str, Any], dict[str, Any]]] = []
    if args.module:
        try:
            module = find_module(data, args.module)
            for symbol in module.get("symbols", []):
                if include_symbol(symbol, args):
                    pairs.append((module, symbol))
        except SystemExit as e:
            if not args.search:
                raise
                
    if args.search:
        for module in iter_modules(data):
            for symbol in module.get("symbols", []):
                if not args.private and symbol.get("visibility") != "public":
                    continue
                if args.kind and symbol.get("kind") != args.kind:
                    continue
                if search_symbol(symbol, args.search):
                    # Filter by module if module is also specified
                    if args.module:
                        m_norm = normalize_module(args.module)
                        if module.get("name") != m_norm and module.get("import_path") != f"std::{m_norm}":
                            continue
                    pairs.append((module, symbol))

    if args.source:
        if not (args.module or args.search):
            print(f"{C_RED}Error:{C_RESET} --source requires --module or --search to specify which symbol to show.", file=sys.stderr)
            return 1
        if not pairs:
            print("No matching symbols found.", file=sys.stderr)
            return 1
        if len(pairs) > 3 and not args.json:
            print(f"Found {len(pairs)} matching symbols. Please narrow down your query. Matches:", file=sys.stderr)
            for m, s in pairs[:10]:
                print(f"  {C_CYAN}{m.get('import_path')}{C_RESET}::{C_BOLD}{s.get('name')}{C_RESET}")
            if len(pairs) > 10:
                print(f"  ... and {len(pairs) - 10} more")
            return 1
            
        for m, s in pairs:
            print(f"=== {C_BOLD}Source of {m.get('import_path')}::{s.get('name')}{C_RESET} ({C_GRAY}{s.get('source_path')}:{s.get('line')}{C_RESET}) ===")
            print(extract_symbol_source(repo_root, data, s, context_before=2))
            print()
        return 0

    if pairs:
        emit_symbols(pairs, args)
    elif args.module or args.search:
        print("no matching symbols", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
