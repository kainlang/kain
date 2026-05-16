#!/usr/bin/env python3
"""Inventory Kain-prefixed native runtime identifiers.

This is a refactor-prep tool, not a rename tool. It emits a deterministic JSON
inventory and a compact Markdown summary that can seed a later surgical rename
manifest.
"""

from __future__ import annotations

import argparse
import json
import re
from collections import Counter, defaultdict
from datetime import datetime, timezone
from pathlib import Path
from typing import Iterable


DEFAULT_EXTENSIONS = (".c", ".h", ".cc", ".cpp", ".hpp", ".inc")
DEFAULT_EXCLUDED_DIRS = {
    ".git",
    ".kain",
    "target",
    "build",
    "dist",
    "out",
    "reports",
    "__pycache__",
}
SYMBOL_RE = re.compile(r"\b(?:kain_(?:native|runtime)[A-Za-z0-9_]*|KAIN_(?:NATIVE|RUNTIME)[A-Za-z0-9_]*)\b")


def repo_root_from_script() -> Path:
    return Path(__file__).resolve().parents[2]


def build_parser() -> argparse.ArgumentParser:
    repo_root = repo_root_from_script()
    default_root = repo_root / "runtime" / "native"
    parser = argparse.ArgumentParser(
        description="List every kain_native/kain_runtime/KAIN_NATIVE/KAIN_RUNTIME identifier under runtime/native."
    )
    parser.add_argument("--root", default=str(default_root), help="Runtime tree to scan.")
    parser.add_argument(
        "--json-out",
        default=str(default_root / "kain_prefixed_symbol_inventory.json"),
        help="Path for the exact JSON occurrence inventory.",
    )
    parser.add_argument(
        "--md-out",
        default=str(default_root / "kain_prefixed_symbol_inventory.md"),
        help="Path for the human-readable Markdown summary.",
    )
    parser.add_argument(
        "--extensions",
        default=",".join(DEFAULT_EXTENSIONS),
        help="Comma-separated file extensions to scan.",
    )
    parser.add_argument(
        "--all-text",
        action="store_true",
        help="Scan every UTF-8-decodable text file instead of only C-ish extensions.",
    )
    return parser


def iter_source_files(root: Path, extensions: set[str], all_text: bool) -> Iterable[Path]:
    for path in sorted(root.rglob("*")):
        if not path.is_file():
            continue
        if any(part in DEFAULT_EXCLUDED_DIRS for part in path.relative_to(root).parts):
            continue
        if all_text or path.suffix.lower() in extensions:
            yield path


def prefix_group(symbol: str) -> str:
    if symbol.startswith("kain_native"):
        return "kain_native"
    if symbol.startswith("kain_runtime"):
        return "kain_runtime"
    if symbol.startswith("KAIN_NATIVE"):
        return "KAIN_NATIVE"
    return "KAIN_RUNTIME"


def suggested_clean_name(symbol: str) -> str:
    if symbol.startswith("kain_native_"):
        return symbol[len("kain_native_") :]
    if symbol.startswith("kain_runtime_"):
        tail = symbol[len("kain_runtime_") :]
        if tail in {"init", "shutdown", "heap_validate", "abi_version", "version"}:
            return "runtime_" + tail
        return tail
    if symbol.startswith("KAIN_NATIVE_"):
        return symbol[len("KAIN_NATIVE_") :]
    if symbol.startswith("KAIN_RUNTIME_"):
        tail = symbol[len("KAIN_RUNTIME_") :]
        if tail in {"INIT", "SHUTDOWN", "HEAP_VALIDATE", "ABI_VERSION", "VERSION"}:
            return "RUNTIME_" + tail
        return tail
    return symbol


def classify_context(line: str, symbol: str, match_end: int) -> str:
    stripped = line.strip()
    if stripped.startswith("#include"):
        return "include_path"
    if stripped.startswith("#ifndef") or stripped.startswith("#define") or stripped.startswith("#endif"):
        if re.match(rf"^#\s*define\s+{re.escape(symbol)}\b", stripped):
            return "macro_define"
        return "preprocessor_reference"
    if symbol.isupper():
        return "macro_reference"
    after = line[match_end:]
    if after.lstrip().startswith("("):
        if stripped.endswith(";"):
            return "function_declaration"
        if "{" in stripped:
            return "function_definition_or_inline_call"
        return "function_call_or_definition"
    return "identifier_reference"


def scan(root: Path, extensions: set[str], all_text: bool) -> dict:
    symbols: dict[str, dict] = {}
    files_scanned = 0
    occurrence_count = 0
    group_counts: Counter[str] = Counter()
    context_counts: Counter[str] = Counter()

    for path in iter_source_files(root, extensions, all_text):
        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue
        files_scanned += 1
        rel = path.relative_to(root).as_posix()
        for line_no, line in enumerate(text.splitlines(), start=1):
            for match in SYMBOL_RE.finditer(line):
                symbol = match.group(0)
                group = prefix_group(symbol)
                context = classify_context(line, symbol, match.end())
                occurrence = {
                    "file": rel,
                    "line": line_no,
                    "column": match.start() + 1,
                    "context": context,
                    "source": line.rstrip(),
                }
                entry = symbols.setdefault(
                    symbol,
                    {
                        "symbol": symbol,
                        "prefix_group": group,
                        "suggested_clean_name": suggested_clean_name(symbol),
                        "occurrence_count": 0,
                        "files": [],
                        "contexts": {},
                        "occurrences": [],
                    },
                )
                entry["occurrence_count"] += 1
                entry["occurrences"].append(occurrence)
                occurrence_count += 1
                group_counts[group] += 1
                context_counts[context] += 1

    for entry in symbols.values():
        entry["files"] = sorted({occ["file"] for occ in entry["occurrences"]})
        entry["contexts"] = dict(sorted(Counter(occ["context"] for occ in entry["occurrences"]).items()))
        entry["occurrences"].sort(key=lambda occ: (occ["file"], occ["line"], occ["column"]))

    ordered_symbols = dict(sorted(symbols.items(), key=lambda item: item[0]))
    return {
        "generated_at_utc": datetime.now(timezone.utc).replace(microsecond=0).isoformat(),
        "root": str(root),
        "extensions": sorted(extensions) if not all_text else ["<all-text>"],
        "files_scanned": files_scanned,
        "unique_symbol_count": len(ordered_symbols),
        "occurrence_count": occurrence_count,
        "prefix_group_counts": dict(sorted(group_counts.items())),
        "context_counts": dict(sorted(context_counts.items())),
        "symbols": ordered_symbols,
    }


def write_markdown(inventory: dict, md_out: Path, root: Path) -> None:
    lines: list[str] = [
        "# Kain-Prefixed Native Runtime Symbol Inventory",
        "",
        f"- Generated UTC: `{inventory['generated_at_utc']}`",
        f"- Root: `{root}`",
        f"- Files scanned: `{inventory['files_scanned']}`",
        f"- Unique symbols: `{inventory['unique_symbol_count']}`",
        f"- Occurrences: `{inventory['occurrence_count']}`",
        "",
        "## Prefix Groups",
        "",
    ]
    for group, count in inventory["prefix_group_counts"].items():
        lines.append(f"- `{group}`: `{count}`")
    lines.extend(["", "## Contexts", ""])
    for context, count in inventory["context_counts"].items():
        lines.append(f"- `{context}`: `{count}`")
    lines.extend(["", "## Symbols", ""])
    for symbol, entry in inventory["symbols"].items():
        lines.append(
            f"- `{symbol}` -> `{entry['suggested_clean_name']}` "
            f"({entry['occurrence_count']} occurrences, {len(entry['files'])} files)"
        )
    lines.extend(["", "## Exact Occurrences", ""])
    for symbol, entry in inventory["symbols"].items():
        lines.append(f"### `{symbol}`")
        lines.append("")
        lines.append(f"- Suggested clean seed: `{entry['suggested_clean_name']}`")
        lines.append(f"- Prefix group: `{entry['prefix_group']}`")
        lines.append(f"- Occurrences: `{entry['occurrence_count']}`")
        lines.append("")
        for occ in entry["occurrences"]:
            source = occ["source"].replace("`", "\\`")
            lines.append(
                f"- `{occ['file']}:{occ['line']}:{occ['column']}` "
                f"`{occ['context']}` `{source}`"
            )
        lines.append("")
    md_out.write_text("\n".join(lines).rstrip() + "\n", encoding="utf-8")


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    root = Path(args.root).resolve()
    json_out = Path(args.json_out).resolve()
    md_out = Path(args.md_out).resolve()
    extensions = {ext if ext.startswith(".") else "." + ext for ext in args.extensions.split(",") if ext}

    inventory = scan(root, extensions, args.all_text)
    json_out.parent.mkdir(parents=True, exist_ok=True)
    md_out.parent.mkdir(parents=True, exist_ok=True)
    json_out.write_text(json.dumps(inventory, indent=2, sort_keys=False) + "\n", encoding="utf-8")
    write_markdown(inventory, md_out, root)

    print(f"scanned={inventory['files_scanned']}")
    print(f"unique_symbols={inventory['unique_symbol_count']}")
    print(f"occurrences={inventory['occurrence_count']}")
    print(f"json={json_out}")
    print(f"markdown={md_out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
