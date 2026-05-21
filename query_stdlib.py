#!/usr/bin/env python3
"""Query Kain's generated stdlib map without loading the full atlas."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any


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
    haystack = " ".join(
        str(symbol.get(field, ""))
        for field in ("name", "qualified_name", "signature", "source_path", "kind", "visibility")
    ).lower()
    return query.lower() in haystack


def symbol_line(module: dict[str, Any], symbol: dict[str, Any]) -> str:
    visibility = symbol.get("visibility", "?")
    kind = symbol.get("kind", "?")
    name = symbol.get("name", "?")
    line = symbol.get("line", "?")
    source = symbol.get("source_path", module.get("source_path", "?"))
    signature = symbol.get("signature") or name
    return f"{module.get('import_path')} {visibility} {kind} {source}:{line} | {signature}"


def print_summary(data: dict[str, Any]) -> None:
    summary = data.get("summary", {})
    print(
        "summary: "
        f"modules={summary.get('module_count')} "
        f"public_symbols={summary.get('public_symbol_count')} "
        f"total_symbols={summary.get('symbol_count')} "
        f"rust_builtins={summary.get('builtin_count')} "
        f"native_services={summary.get('native_service_count')}"
    )
    for module in iter_modules(data):
        public, private = module_public_counts(module)
        print(f"{module.get('import_path'):<24} public={public:<4} private={private:<4} source={module.get('source_path')}")


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


def main(argv: list[str] | None = None) -> int:
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
    args = parser.parse_args(argv)

    if args.repo:
        repo_root = args.repo.resolve()
    else:
        try:
            repo_root = find_repo_root(Path.cwd().resolve())
        except SystemExit:
            repo_root = find_repo_root(Path(__file__).resolve())
    data = load_map(repo_root)

    if args.summary or not any([args.imports, args.module, args.search]):
        print_summary(data)
        return 0

    if args.imports:
        print_imports(data)

    pairs: list[tuple[dict[str, Any], dict[str, Any]]] = []
    if args.module:
        module = find_module(data, args.module)
        for symbol in module.get("symbols", []):
            if include_symbol(symbol, args):
                pairs.append((module, symbol))

    if args.search:
        for module in iter_modules(data):
            for symbol in module.get("symbols", []):
                if not args.private and symbol.get("visibility") != "public":
                    continue
                if args.kind and symbol.get("kind") != args.kind:
                    continue
                if search_symbol(symbol, args.search):
                    pairs.append((module, symbol))

    if pairs:
        emit_symbols(pairs, args)
    elif args.module or args.search:
        print("no matching symbols", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
