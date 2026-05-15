#!/usr/bin/env python3
"""Mine C header ABI shapes for Kain foreign ABI coverage work.

The miner is intentionally conservative: it does not claim to parse C. It scans
headers for the raw shapes that stress an FFI boundary, then emits JSON that can
drive focused implementation work and regression tests.
"""

from __future__ import annotations

import argparse
import json
import re
from collections import Counter, defaultdict
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable


HEADER_SUFFIXES = {".h", ".hh", ".hpp", ".hxx"}
SCALAR_NAMES = {
    "void",
    "bool",
    "_Bool",
    "char",
    "signed char",
    "unsigned char",
    "short",
    "unsigned short",
    "int",
    "unsigned int",
    "long",
    "unsigned long",
    "long long",
    "unsigned long long",
    "float",
    "double",
    "size_t",
    "ptrdiff_t",
    "intptr_t",
    "uintptr_t",
    "int8_t",
    "uint8_t",
    "int16_t",
    "uint16_t",
    "int32_t",
    "uint32_t",
    "int64_t",
    "uint64_t",
}
BYTE_SCALARS = {"uint8_t", "unsigned char", "int8_t"}


TYPEDEF_CALLBACK_RE = re.compile(
    r"typedef\s+(?P<ret>[^;()]+?)\(\s*\*\s*(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*\)\s*\((?P<args>[^;]*)\)\s*;",
    re.MULTILINE | re.DOTALL,
)
TYPEDEF_POINTER_RE = re.compile(
    r"typedef\s+(?P<base>(?:struct|enum)?\s*[A-Za-z_][A-Za-z0-9_\s]*?)\s*(?P<stars>\*+)\s*(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*;",
    re.MULTILINE,
)
PROTOTYPE_RE = re.compile(
    r"(?:^|\n)\s*(?:extern\s+)?(?P<ret>[A-Za-z_][A-Za-z0-9_\s\*]*?)\s+(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*\((?P<args>[^;{}()]*(?:\([^)]*\)[^;{}()]*)*)\)\s*;",
    re.MULTILINE | re.DOTALL,
)


@dataclass(frozen=True)
class ShapeSample:
    file: str
    symbol: str
    shape: str
    text: str


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("roots", nargs="+", type=Path, help="Header files or directories to scan")
    parser.add_argument("--out", type=Path, help="Optional JSON output path")
    parser.add_argument("--max-samples", type=int, default=24)
    args = parser.parse_args()

    headers = sorted(discover_headers(args.roots))
    summary = mine_headers(headers, max_samples=args.max_samples)
    payload = json.dumps(summary, indent=2, sort_keys=True)
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(payload + "\n", encoding="utf-8")
    else:
        print(payload)
    return 0


def discover_headers(roots: Iterable[Path]) -> Iterable[Path]:
    for root in roots:
        if root.is_file() and root.suffix in HEADER_SUFFIXES:
            yield root
            continue
        if root.is_dir():
            yield from (
                path
                for path in root.rglob("*")
                if path.is_file() and path.suffix in HEADER_SUFFIXES
            )


def mine_headers(headers: list[Path], max_samples: int) -> dict[str, object]:
    counts: Counter[str] = Counter()
    samples: dict[str, list[ShapeSample]] = defaultdict(list)
    callback_typedefs: dict[str, str] = {}
    pointer_typedefs: dict[str, int] = {}

    for header in headers:
        source = strip_comments(header.read_text(encoding="utf-8", errors="ignore"))
        for match in TYPEDEF_CALLBACK_RE.finditer(source):
            name = match.group("name")
            callback_typedefs[name] = compact(match.group(0))
            observe(counts, samples, "callback_typedef", header, name, match.group(0), max_samples)
        for match in TYPEDEF_POINTER_RE.finditer(source):
            name = match.group("name")
            depth = len(match.group("stars"))
            pointer_typedefs[name] = depth
            observe(counts, samples, "pointer_typedef", header, name, match.group(0), max_samples)

    for header in headers:
        source = strip_comments(header.read_text(encoding="utf-8", errors="ignore"))
        for prototype in PROTOTYPE_RE.finditer(source):
            symbol = prototype.group("name")
            return_type = compact(prototype.group("ret"))
            args = split_top_level_commas(prototype.group("args"))
            classify_type(return_type, callback_typedefs, pointer_typedefs, "return", counts, samples, header, symbol, max_samples)
            for index, arg in enumerate(args, start=1):
                if not arg or arg == "void":
                    continue
                classify_type(arg, callback_typedefs, pointer_typedefs, f"arg{index}", counts, samples, header, symbol, max_samples)

    return {
        "schema": "kain-foreign-abi-shape-miner-v1",
        "header_count": len(headers),
        "counts": dict(sorted(counts.items())),
        "callback_typedef_count": len(callback_typedefs),
        "pointer_typedef_count": len(pointer_typedefs),
        "samples": {
            key: [asdict(sample) for sample in value]
            for key, value in sorted(samples.items())
        },
    }


def classify_type(
    raw: str,
    callback_typedefs: dict[str, str],
    pointer_typedefs: dict[str, int],
    position: str,
    counts: Counter[str],
    samples: dict[str, list[ShapeSample]],
    header: Path,
    symbol: str,
    max_samples: int,
) -> None:
    normalized = compact(raw)
    if "(*" in normalized:
        observe(counts, samples, "function_pointer_inline", header, symbol, normalized, max_samples)
        return

    base = base_type_name(normalized)
    pointer_depth = normalized.count("*") + pointer_typedefs.get(base, 0)
    has_array = "[" in normalized and "]" in normalized
    if base in callback_typedefs:
        observe(counts, samples, "function_pointer_typedef_ref", header, symbol, normalized, max_samples)
    if pointer_depth > 1:
        observe(counts, samples, "multi_level_pointer", header, symbol, normalized, max_samples)
    if pointer_depth > 0 and base in SCALAR_NAMES and base not in BYTE_SCALARS and base != "char":
        observe(counts, samples, "raw_scalar_pointer", header, symbol, normalized, max_samples)
    if position == "return" and pointer_depth > 0 and base in BYTE_SCALARS:
        observe(counts, samples, "byte_buffer_return", header, symbol, normalized, max_samples)
    if has_array:
        observe(counts, samples, "array_declarator", header, symbol, normalized, max_samples)
    if pointer_depth == 0 and base not in SCALAR_NAMES and base not in callback_typedefs:
        observe(counts, samples, "by_value_named_type", header, symbol, normalized, max_samples)


def observe(
    counts: Counter[str],
    samples: dict[str, list[ShapeSample]],
    shape: str,
    header: Path,
    symbol: str,
    text: str,
    max_samples: int,
) -> None:
    counts[shape] += 1
    if len(samples[shape]) < max_samples:
        samples[shape].append(
            ShapeSample(
                file=str(header),
                symbol=symbol,
                shape=shape,
                text=compact(text),
            )
        )


def base_type_name(raw: str) -> str:
    cleaned = re.sub(r"\[[^\]]*\]", " ", raw)
    cleaned = cleaned.replace("*", " ")
    cleaned = re.sub(r"\b(const|volatile|restrict|extern|static|inline)\b", " ", cleaned)
    tokens = [token for token in cleaned.split() if token.isidentifier() or token == "_Bool"]
    if not tokens:
        return ""
    if tokens[-1] not in SCALAR_NAMES and len(tokens) > 1:
        return tokens[-2] if tokens[-1].startswith("p") else tokens[-1]
    return " ".join(tokens)


def split_top_level_commas(source: str) -> list[str]:
    parts: list[str] = []
    depth = 0
    start = 0
    for index, ch in enumerate(source):
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth = max(0, depth - 1)
        elif ch == "," and depth == 0:
            parts.append(source[start:index].strip())
            start = index + 1
    tail = source[start:].strip()
    if tail:
        parts.append(tail)
    return parts


def strip_comments(source: str) -> str:
    source = re.sub(r"/\*.*?\*/", " ", source, flags=re.DOTALL)
    return re.sub(r"//.*", "", source)


def compact(source: str) -> str:
    return " ".join(source.replace("\t", " ").split())


if __name__ == "__main__":
    raise SystemExit(main())
