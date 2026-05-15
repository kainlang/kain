from __future__ import annotations

import json
import re
import sys
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Tuple

TOOLS_ROOT = Path(__file__).resolve().parents[1] / "tools"
if str(TOOLS_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOLS_ROOT))

from ouroboros_pathing import discover_workspace_context

CONTEXT = discover_workspace_context(__file__)
REPO_ROOT = CONTEXT.repo_root
OUT_ROOT = CONTEXT.ouroboros_root / "docs" / "selfhost"
INVENTORY_DIR = OUT_ROOT / "inventories"
METADATA_DIR = OUT_ROOT / "metadata"

CRATES = {
    "kain-core": REPO_ROOT / "crates" / "kain-core" / "src",
    "kain-import": REPO_ROOT / "crates" / "kain-import" / "src",
    "kain-sys-codegen": REPO_ROOT / "crates" / "kain-sys-codegen" / "src",
    "cli": REPO_ROOT / "crates" / "cli" / "src",
}

BANG_MACRO_RE = re.compile(r"(?<![A-Za-z0-9_])([A-Za-z_][A-Za-z0-9_]*)!\s*(?:\(|\{|\[)")
ATTRIBUTE_RE = re.compile(r"#\s*\[\s*([A-Za-z_][A-Za-z0-9_]*)")
DERIVE_RE = re.compile(r"#\s*\[\s*derive\s*\((.*?)\)\s*\]")
TRAIT_DEF_RE = re.compile(r"^\s*(?:pub\s+)?trait\s+([A-Za-z_][A-Za-z0-9_]*)", re.MULTILINE)
IMPL_RE = re.compile(r"^\s*impl(?:\s*<[^\n>]+>)?\s+(.+?)\s*\{", re.MULTILINE)
DYN_RE = re.compile(r"\bdyn\s+[A-Za-z_][A-Za-z0-9_:<>]*")
MOD_RE = re.compile(r"^\s*(?:pub\s+)?mod\s+([A-Za-z_][A-Za-z0-9_]*)\s*;", re.MULTILINE)
ENUM_RE_TEMPLATE = r"pub\s+enum\s+{name}\s*\{{"
ADD_FN_RE = re.compile(r'add_fn\("([^"]+)"')
RESERVED_RE = re.compile(r"pub\s+const\s+RESERVED_KEYWORDS:\s*&\[&str\]\s*=\s*&\[(.*?)\];", re.DOTALL)
STRING_RE = re.compile(r'"((?:\\.|[^"\\])*)"')

CLASSIFICATION = {
    "format": "lower_directly",
    "vec": "lower_directly",
    "matches": "lower_directly",
    "write": "lower_directly",
    "writeln": "lower_directly",
    "print": "lower_directly",
    "println": "lower_directly",
    "eprint": "lower_directly",
    "eprintln": "lower_directly",
    "concat": "lower_directly",
    "stringify": "preserve",
    "assert": "reject",
    "assert_eq": "reject",
    "assert_ne": "reject",
    "debug_assert": "reject",
    "debug_assert_eq": "reject",
    "debug_assert_ne": "reject",
    "panic": "reject",
    "todo": "reject",
    "unimplemented": "reject",
    "unreachable": "reject",
    "dbg": "reject",
    "cfg": "preserve",
    "derive": "preserve",
    "test": "preserve",
    "command": "preserve",
    "arg": "preserve",
    "error": "preserve",
    "from": "preserve",
    "subcommand": "preserve",
    "clap": "preserve",
}

INITIAL_SELFHOST_SLICE = ["kain-core", "kain-import"]


def ensure_dirs() -> None:
    INVENTORY_DIR.mkdir(parents=True, exist_ok=True)
    METADATA_DIR.mkdir(parents=True, exist_ok=True)


def rust_files(root: Path) -> List[Path]:
    return sorted(root.rglob("*.rs"))


def strip_comment_only_lines(text: str) -> str:
    kept = []
    for line in text.splitlines():
        stripped = line.lstrip()
        if stripped.startswith("//") or stripped.startswith("/*") or stripped.startswith("*"):
            continue
        kept.append(line)
    return "\n".join(kept)


def extract_bang_macros(text: str) -> List[str]:
    return BANG_MACRO_RE.findall(strip_comment_only_lines(text))


def extract_attributes(text: str) -> List[str]:
    cleaned = strip_comment_only_lines(text)
    names = ATTRIBUTE_RE.findall(cleaned)
    derived = []
    for payload in DERIVE_RE.findall(cleaned):
        for item in payload.split(","):
            name = item.strip()
            if name:
                derived.append(f"derive::{name}")
    return names + derived


def classify_macro(name: str) -> str:
    base = name.split("::", 1)[0]
    if name.startswith("derive::"):
        return "preserve"
    return CLASSIFICATION.get(base, "review")


def gather_macro_inventory() -> Dict[str, object]:
    crates = {}
    global_counts = Counter()
    file_hits: Dict[str, Dict[str, List[str]]] = defaultdict(lambda: defaultdict(list))

    for crate, src_root in CRATES.items():
        bang_counts = Counter()
        attr_counts = Counter()
        for file_path in rust_files(src_root):
            text = file_path.read_text(encoding="utf-8", errors="ignore")
            rel = file_path.relative_to(REPO_ROOT).as_posix()
            for macro in extract_bang_macros(text):
                bang_counts[macro] += 1
                global_counts[macro] += 1
                if rel not in file_hits[crate][macro]:
                    file_hits[crate][macro].append(rel)
            for macro in extract_attributes(text):
                attr_counts[macro] += 1
                global_counts[macro] += 1
                if rel not in file_hits[crate][macro]:
                    file_hits[crate][macro].append(rel)
        crates[crate] = {
            "bang_macros": dict(sorted(bang_counts.items())),
            "attribute_macros": dict(sorted(attr_counts.items())),
            "files": {k: sorted(v) for k, v in sorted(file_hits[crate].items())},
        }

    classifications = {}
    for macro, count in sorted(global_counts.items()):
        classifications[macro] = {
            "count": count,
            "classification": classify_macro(macro),
        }

    return {
        "crates": crates,
        "global": dict(sorted(global_counts.items())),
        "classifications": classifications,
    }


def gather_trait_inventory() -> Dict[str, object]:
    crates = {}
    for crate, src_root in CRATES.items():
        trait_defs = []
        trait_impls = []
        dyn_usages = []
        for file_path in rust_files(src_root):
            text = file_path.read_text(encoding="utf-8", errors="ignore")
            rel = file_path.relative_to(REPO_ROOT).as_posix()
            for name in TRAIT_DEF_RE.findall(text):
                trait_defs.append({"trait": name, "file": rel})
            for body in IMPL_RE.findall(text):
                if " for " in body:
                    trait_impls.append({"impl": " ".join(body.split()), "file": rel})
            for usage in sorted(set(DYN_RE.findall(text))):
                dyn_usages.append({"usage": usage, "file": rel})
        crates[crate] = {
            "trait_defs": trait_defs,
            "trait_impls": trait_impls,
            "dyn_usages": dyn_usages,
        }
    return {"crates": crates}


def gather_module_map() -> Dict[str, object]:
    crates = {}
    for crate, src_root in CRATES.items():
        root_file = src_root / ("lib.rs" if (src_root / "lib.rs").exists() else "main.rs")
        root_text = root_file.read_text(encoding="utf-8", errors="ignore")
        root_modules = MOD_RE.findall(root_text)
        nested = {}
        for file_path in rust_files(src_root):
            rel = file_path.relative_to(src_root).as_posix()
            if rel in {"lib.rs", "main.rs"}:
                continue
            text = file_path.read_text(encoding="utf-8", errors="ignore")
            mods = MOD_RE.findall(text)
            if mods:
                nested[rel] = mods
        crates[crate] = {
            "root": root_file.relative_to(REPO_ROOT).as_posix(),
            "root_modules": root_modules,
            "nested_modules": nested,
            "initial_selfhost_candidate": crate in INITIAL_SELFHOST_SLICE,
        }
    return {"crates": crates, "initial_slice": INITIAL_SELFHOST_SLICE}


def split_top_level_variants(body: str) -> List[str]:
    parts: List[str] = []
    start = 0
    paren = 0
    brace = 0
    bracket = 0
    i = 0
    while i < len(body):
        ch = body[i]
        if ch == "(":
            paren += 1
        elif ch == ")":
            paren = max(0, paren - 1)
        elif ch == "{":
            brace += 1
        elif ch == "}":
            brace = max(0, brace - 1)
        elif ch == "[":
            bracket += 1
        elif ch == "]":
            bracket = max(0, bracket - 1)
        elif ch == "," and paren == 0 and brace == 0 and bracket == 0:
            chunk = body[start:i].strip()
            if chunk:
                parts.append(chunk)
            start = i + 1
        i += 1
    tail = body[start:].strip()
    if tail:
        parts.append(tail)
    return parts


def find_enum_variants(text: str, enum_name: str) -> List[str]:
    marker = re.search(ENUM_RE_TEMPLATE.format(name=re.escape(enum_name)), text)
    if not marker:
        return []
    start = marker.end()
    depth = 1
    i = start
    while i < len(text) and depth > 0:
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
        i += 1
    body = text[start:i - 1]
    variants = []
    for chunk in split_top_level_variants(body):
        stripped = chunk.strip()
        if not stripped or stripped.startswith("///") or stripped.startswith("//") or stripped.startswith("#"):
            continue
        if stripped.startswith("pub "):
            stripped = stripped[4:]
        match = re.match(r"([A-Za-z_][A-Za-z0-9_]*)", stripped)
        if match:
            variants.append(match.group(1))
    return variants


def gather_kain_core_metadata() -> Dict[str, object]:
    ast_text = (CRATES["kain-core"] / "ast.rs").read_text(encoding="utf-8", errors="ignore")
    effects_text = (CRATES["kain-core"] / "effects.rs").read_text(encoding="utf-8", errors="ignore")
    parser_text = (CRATES["kain-core"] / "parser.rs").read_text(encoding="utf-8", errors="ignore")
    stdlib_text = (CRATES["kain-core"] / "stdlib.rs").read_text(encoding="utf-8", errors="ignore")

    reserved_block = RESERVED_RE.search(parser_text)
    reserved_words = []
    if reserved_block:
        reserved_words = STRING_RE.findall(reserved_block.group(1))

    builtins = ADD_FN_RE.findall(stdlib_text)

    return {
        "item_kinds": find_enum_variants(ast_text, "Item"),
        "expr_kinds": find_enum_variants(ast_text, "Expr"),
        "type_kinds": find_enum_variants(ast_text, "Type"),
        "effects": find_enum_variants(effects_text, "Effect"),
        "reserved_words": reserved_words,
        "builtins": builtins,
    }


def build_allowlist(macro_inventory: Dict[str, object], trait_inventory: Dict[str, object], module_map: Dict[str, object]) -> Dict[str, object]:
    macros = macro_inventory["classifications"]
    hard_fail_macros = sorted([name for name, meta in macros.items() if meta["classification"] == "reject"])
    preserve_macros = sorted([name for name, meta in macros.items() if meta["classification"] == "preserve"])
    lower_macros = sorted([name for name, meta in macros.items() if meta["classification"] == "lower_directly"])
    phase1_required_direct_lowering = [
        "vec",
        "matches",
        "format",
        "write",
        "writeln",
    ]

    dyn_usage_count = sum(len(crate["dyn_usages"]) for crate in trait_inventory["crates"].values())

    acceptable_phase1 = [
        "Unsupported attribute macros may be preserved as inert metadata if they do not affect semantic lowering.",
        "Trait impls may be imported lossy if methods are preserved and trait identity is recorded for diagnostics.",
        "Clap/thiserror/test/cfg style attribute macros may be preserved without full execution in phase 1.",
        "Directly lowerable self-host macros in the required list must not remain as preserved macro calls in strict mode.",
        "Non-required macros outside the initial slice may remain preserved temporarily if they do not affect semantic correctness.",
    ]
    hard_fail = [
        "panic!/todo!/unimplemented!/unreachable! survive into imported self-host output without explicit lowering policy.",
        "Trait object (dyn) usage requires semantics we cannot represent and is silently erased.",
        "A crate outside the initial slice is made mandatory for phase-1 bootstrap.",
        "A phase1-required direct-lowering macro remains preserved in strict self-host mode.",
        "Macro expansion changes control flow or data layout and is imported as plain text without metadata.",
    ]

    return {
        "initial_slice": module_map["initial_slice"],
        "phase1_required_direct_lowering": phase1_required_direct_lowering,
        "phase1_acceptable_diagnostics": acceptable_phase1,
        "hard_fail_conditions": hard_fail,
        "macro_policy": {
            "lower_directly": lower_macros,
            "preserve": preserve_macros,
            "reject": hard_fail_macros,
        },
        "trait_object_usage_count": dyn_usage_count,
    }


def write_json(path: Path, data: Dict[str, object]) -> None:
    path.write_text(json.dumps(data, indent=2), encoding="utf-8")


def write_macro_markdown(path: Path, data: Dict[str, object]) -> None:
    lines = ["# Self-Host Macro Inventory", "", "## Global Classification", ""]
    lines.append("| Macro | Count | Classification |")
    lines.append("|---|---:|---|")
    for macro, meta in data["classifications"].items():
        lines.append(f"| `{macro}` | {meta['count']} | {meta['classification']} |")
    for crate, payload in data["crates"].items():
        lines.extend(["", f"## {crate}", "", "### Bang macros", ""])
        if payload["bang_macros"]:
            for name, count in payload["bang_macros"].items():
                lines.append(f"- `{name}!` — {count}")
        else:
            lines.append("- none")
        lines.extend(["", "### Attribute macros", ""])
        if payload["attribute_macros"]:
            for name, count in payload["attribute_macros"].items():
                lines.append(f"- `{name}` — {count}")
        else:
            lines.append("- none")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_trait_markdown(path: Path, data: Dict[str, object]) -> None:
    lines = ["# Self-Host Trait Surface Inventory", ""]
    for crate, payload in data["crates"].items():
        lines.extend([f"## {crate}", "", f"- **Trait defs:** {len(payload['trait_defs'])}", f"- **Trait impls:** {len(payload['trait_impls'])}", f"- **Trait-object usages:** {len(payload['dyn_usages'])}", ""])
        if payload["trait_defs"]:
            lines.append("### Trait definitions")
            lines.append("")
            for item in payload["trait_defs"]:
                lines.append(f"- `{item['trait']}` — `{item['file']}`")
            lines.append("")
        if payload["dyn_usages"]:
            lines.append("### dyn usages")
            lines.append("")
            for item in payload["dyn_usages"]:
                lines.append(f"- `{item['usage']}` — `{item['file']}`")
            lines.append("")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_module_markdown(path: Path, data: Dict[str, object]) -> None:
    lines = ["# Initial Self-Host Module / Crate Map", "", f"Initial slice: `{', '.join(data['initial_slice'])}`", ""]
    for crate, payload in data["crates"].items():
        lines.extend([f"## {crate}", "", f"- **Root:** `{payload['root']}`", f"- **Initial slice candidate:** {'yes' if payload['initial_selfhost_candidate'] else 'no'}", "", "### Root modules", ""])
        for mod_name in payload["root_modules"]:
            lines.append(f"- `{mod_name}`")
        if not payload["root_modules"]:
            lines.append("- none")
        if payload["nested_modules"]:
            lines.extend(["", "### Nested module declarations", ""])
            for file_name, mods in sorted(payload["nested_modules"].items()):
                lines.append(f"- `{file_name}` -> {', '.join(f'`{m}`' for m in mods)}")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_allowlist_markdown(path: Path, data: Dict[str, object]) -> None:
    lines = ["# Self-Host Phase 1 Allowlist", "", f"Initial slice: `{', '.join(data['initial_slice'])}`", "", "## Acceptable diagnostics in phase 1", ""]
    for item in data["phase1_acceptable_diagnostics"]:
        lines.append(f"- {item}")
    lines.extend(["", "## Phase 1 required direct lowering", ""])
    for item in data["phase1_required_direct_lowering"]:
        lines.append(f"- `{item}`")
    lines.extend(["", "## Immediate hard fail conditions", ""])
    for item in data["hard_fail_conditions"]:
        lines.append(f"- {item}")
    lines.extend(["", "## Macro policy", ""])
    for bucket in ["lower_directly", "preserve", "reject"]:
        lines.append(f"### {bucket}")
        lines.append("")
        for macro in data["macro_policy"][bucket]:
            lines.append(f"- `{macro}`")
        if not data["macro_policy"][bucket]:
            lines.append("- none")
        lines.append("")
    lines.append(f"Trait-object usage count across initial scan: **{data['trait_object_usage_count']}**")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_metadata_markdown(path: Path, data: Dict[str, object]) -> None:
    lines = ["# kain-core Metadata Snapshot", ""]
    for key, title in [
        ("item_kinds", "Item kinds"),
        ("expr_kinds", "Expr kinds"),
        ("type_kinds", "Type kinds"),
        ("effects", "Effects"),
        ("reserved_words", "Reserved words"),
        ("builtins", "Builtins"),
    ]:
        lines.extend([f"## {title}", ""])
        for value in data[key]:
            lines.append(f"- `{value}`")
        if not data[key]:
            lines.append("- none")
        lines.append("")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    ensure_dirs()
    macro_inventory = gather_macro_inventory()
    trait_inventory = gather_trait_inventory()
    module_map = gather_module_map()
    metadata = gather_kain_core_metadata()
    allowlist = build_allowlist(macro_inventory, trait_inventory, module_map)

    write_json(INVENTORY_DIR / "macro_inventory.json", macro_inventory)
    write_json(INVENTORY_DIR / "trait_inventory.json", trait_inventory)
    write_json(INVENTORY_DIR / "module_map.json", module_map)
    write_json(INVENTORY_DIR / "selfhost_allowlist.json", allowlist)
    write_json(METADATA_DIR / "kain_core_metadata.json", metadata)

    write_macro_markdown(INVENTORY_DIR / "macro_inventory.md", macro_inventory)
    write_trait_markdown(INVENTORY_DIR / "trait_inventory.md", trait_inventory)
    write_module_markdown(INVENTORY_DIR / "module_map.md", module_map)
    write_allowlist_markdown(INVENTORY_DIR / "selfhost_allowlist.md", allowlist)
    write_metadata_markdown(METADATA_DIR / "kain_core_metadata.md", metadata)

    summary = {
        "macro_count": len(macro_inventory["global"]),
        "trait_def_count": sum(len(crate["trait_defs"]) for crate in trait_inventory["crates"].values()),
        "trait_impl_count": sum(len(crate["trait_impls"]) for crate in trait_inventory["crates"].values()),
        "dyn_usage_count": sum(len(crate["dyn_usages"]) for crate in trait_inventory["crates"].values()),
        "initial_slice": module_map["initial_slice"],
        "builtin_count": len(metadata["builtins"]),
        "reserved_word_count": len(metadata["reserved_words"]),
    }
    write_json(OUT_ROOT / "summary.json", summary)
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
