#!/usr/bin/env python3
"""Generate Kain's embedded TypeScript ambient manifest.

The input is the TypeScript reference lib directory checked into this repo at
reference/TypeScript-main/src/lib. The output is a compact JSON manifest that the
Rust importer embeds at compile time, so normal compiler runs do not need the
reference checkout on disk.
"""

from __future__ import annotations

import argparse
import json
import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any


VALUE_DECL_RE = re.compile(
    r"^\s*declare\s+(?P<kind>var|let|const|function|class|namespace)\s+"
    r"(?P<name>[A-Za-z_$][\w$]*)\b"
)
TYPE_DECL_RE = re.compile(
    r"^\s*(?:declare\s+)?(?P<kind>interface|type|class|enum)\s+"
    r"(?P<name>[A-Za-z_$][\w$]*)\b"
)

EXTRA_KAIN_KEYWORDS = {
    "and",
    "or",
    "shader",
    "state",
    "vertex",
    "fragment",
    "Pure",
    "IO",
    "GPU",
    "Reactive",
    "Unsafe",
}

# Keep this aligned with the parser-visible keywords that cannot be emitted as
# imported identifiers. It is intentionally conservative; the Rust importer also
# sanitizes identifiers, but the manifest should be directly inspectable.
KAIN_RESERVED_WORDS = EXTRA_KAIN_KEYWORDS | {
    "actor",
    "as",
    "async",
    "await",
    "break",
    "case",
    "component",
    "comptime",
    "const",
    "continue",
    "else",
    "enum",
    "false",
    "fn",
    "for",
    "from",
    "if",
    "impl",
    "in",
    "let",
    "loop",
    "macro",
    "match",
    "mod",
    "mut",
    "none",
    "orchestrate",
    "patch",
    "pub",
    "return",
    "self",
    "static",
    "struct",
    "test",
    "trait",
    "true",
    "type",
    "unsafe",
    "use",
    "while",
    "world",
    "yield",
}


@dataclass
class DeclarationAccumulator:
    kind: str
    source_files: set[str] = field(default_factory=set)
    declaration_count: int = 0

    def add(self, source_file: str) -> None:
        self.source_files.add(source_file)
        self.declaration_count += 1


def sanitize_identifier_base(raw: str) -> str:
    out: list[str] = []
    previous_was_underscore = False
    for char in raw.strip():
        mapped = char if char.isascii() and (char.isalnum() or char == "_") else "_"
        if mapped == "_":
            if not previous_was_underscore:
                out.append("_")
            previous_was_underscore = True
        else:
            out.append(mapped)
            previous_was_underscore = False

    candidate = "".join(out).strip("_") or "ts_id"
    if candidate == "_":
        candidate = "ts_id"
    if candidate[0].isdigit():
        candidate = f"ts_{candidate}"
    if candidate in KAIN_RESERVED_WORDS:
        candidate = f"{candidate}_"
    return candidate


def read_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def collect_declarations(lib_dir: Path) -> tuple[dict[str, DeclarationAccumulator], dict[str, DeclarationAccumulator], list[str]]:
    value_declarations: dict[str, DeclarationAccumulator] = {}
    type_declarations: dict[str, DeclarationAccumulator] = {}
    lib_files: list[str] = []

    for source_file in sorted(lib_dir.glob("*.d.ts")):
        lib_files.append(source_file.name)
        for line in source_file.read_text(encoding="utf-8", errors="replace").splitlines():
            value_match = VALUE_DECL_RE.match(line)
            if value_match:
                name = value_match.group("name")
                kind = f"typescript_{value_match.group('kind')}"
                value_declarations.setdefault(name, DeclarationAccumulator(kind)).add(source_file.name)

            type_match = TYPE_DECL_RE.match(line)
            if type_match:
                name = type_match.group("name")
                kind = f"typescript_{type_match.group('kind')}"
                type_declarations.setdefault(name, DeclarationAccumulator(kind)).add(source_file.name)

    return value_declarations, type_declarations, lib_files


def override_aliases(overrides: dict[str, Any]) -> dict[str, str]:
    return {
        entry["ts_name"]: entry["kain_name"]
        for entry in overrides.get("value_aliases", [])
    }


def make_symbol(name: str, declaration: DeclarationAccumulator, aliases: dict[str, str]) -> dict[str, Any]:
    return {
        "ts_name": name,
        "kain_name": aliases.get(name, sanitize_identifier_base(name)),
        "kind": declaration.kind,
        "source_files": sorted(declaration.source_files),
        "declaration_count": declaration.declaration_count,
    }


def make_synthetic_symbol(entry: dict[str, Any], source_name: str) -> dict[str, Any]:
    return {
        "ts_name": entry["ts_name"],
        "kain_name": entry.get("kain_name", sanitize_identifier_base(entry["ts_name"])),
        "kind": entry.get("kind", "synthetic"),
        "source_files": [source_name],
        "declaration_count": 1,
        "reason": entry.get("reason", ""),
    }


def generate_manifest(lib_dir: Path, overrides_path: Path) -> dict[str, Any]:
    overrides = read_json(overrides_path)
    if overrides.get("schema_version") != 1:
        raise ValueError(f"Unsupported overrides schema_version in {overrides_path}")

    value_declarations, type_declarations, lib_files = collect_declarations(lib_dir)
    aliases = override_aliases(overrides)
    suppressed_type_names = set(overrides.get("suppressed_type_names", []))

    value_symbols = [
        make_symbol(name, declaration, aliases)
        for name, declaration in sorted(value_declarations.items())
    ]
    known_value_names = {symbol["ts_name"] for symbol in value_symbols}
    for entry in overrides.get("synthetic_values", []):
        if entry["ts_name"] not in known_value_names:
            value_symbols.append(make_synthetic_symbol(entry, overrides_path.as_posix()))

    type_symbols = [
        make_symbol(name, declaration, {})
        for name, declaration in sorted(type_declarations.items())
        if name not in suppressed_type_names
    ]
    known_type_names = {symbol["ts_name"] for symbol in type_symbols}
    for entry in overrides.get("synthetic_types", []):
        if entry["ts_name"] not in known_type_names and entry["ts_name"] not in suppressed_type_names:
            type_symbols.append(make_synthetic_symbol(entry, overrides_path.as_posix()))

    value_symbols.sort(key=lambda symbol: symbol["kain_name"])
    type_symbols.sort(key=lambda symbol: symbol["kain_name"])

    return {
        "schema_version": 1,
        "source": {
            "typescript_lib_dir": lib_dir.as_posix(),
            "override_file": overrides_path.as_posix(),
            "lib_file_count": len(lib_files),
            "lib_files": lib_files,
        },
        "value_aliases": sorted(overrides.get("value_aliases", []), key=lambda entry: entry["ts_name"]),
        "suppressed_type_names": sorted(suppressed_type_names),
        "type_names_lowered_to_any": sorted(overrides.get("type_names_lowered_to_any", [])),
        "value_symbols": value_symbols,
        "type_symbols": type_symbols,
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--typescript-lib-dir",
        type=Path,
        default=Path("reference/TypeScript-main/src/lib"),
        help="Directory containing TypeScript .d.ts lib files.",
    )
    parser.add_argument(
        "--overrides",
        type=Path,
        default=Path("tools/typescript_import/typescript_ambient_overrides.json"),
        help="Kain-specific alias/synthetic declaration policy.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("crates/import/src/typescript/data/typescript_ambient_manifest.json"),
        help="Manifest JSON path embedded by the Rust importer.",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    lib_dir = args.typescript_lib_dir
    overrides_path = args.overrides
    output_path = args.output

    if not lib_dir.is_dir():
        raise FileNotFoundError(f"TypeScript lib directory not found: {lib_dir}")
    if not overrides_path.is_file():
        raise FileNotFoundError(f"Overrides file not found: {overrides_path}")

    manifest = generate_manifest(lib_dir, overrides_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", encoding="utf-8", newline="\n") as handle:
        json.dump(manifest, handle, indent=2, sort_keys=False)
        handle.write("\n")

    print(
        "generated "
        f"{output_path} with "
        f"{len(manifest['value_symbols'])} value symbols and "
        f"{len(manifest['type_symbols'])} type symbols"
    )


if __name__ == "__main__":
    main()
