from __future__ import annotations

import json
import re
from dataclasses import dataclass
from pathlib import Path


PACK_ROOT = Path(__file__).resolve().parents[1]
SOURCE_PATH = PACK_ROOT.parent / "src" / "codegen_llvm" / "mod.rs"
GENERATED_DIR = PACK_ROOT / "generated"
JSON_PATH = GENERATED_DIR / "codegen_llvm_target_inventory.json"
MARKDOWN_PATH = GENERATED_DIR / "codegen_llvm_target_inventory.md"

KEYWORD_GROUPS = {
    "control": [
        "next_label",
        "next_reg",
        "phi ",
        "br label",
        "emit_label",
        "guard_fail",
        "label_no_match",
    ],
    "layout": [
        "align_abi_size",
        "abi_layout_for_ty",
        "alloca ",
        "getelementptr",
        "s.len() + 1",
        "struct_defs",
    ],
    "memory": [
        "__kain_mem_load",
        "__kain_mem_store",
        "ptrtoint",
        "inttoptr",
        "payload_size",
        "bitcast",
    ],
    "casts": [
        "sext ",
        "zext ",
        "fptosi",
        "sitofp",
        "uitofp",
        "icmp ne",
        "fcmp one",
    ],
}


@dataclass
class FunctionSpan:
    name: str
    start_line: int
    end_line: int
    text: str


def parse_functions(source_text: str) -> list[FunctionSpan]:
    lines = source_text.splitlines()
    pattern = re.compile(r"^\s*fn\s+([A-Za-z0-9_]+)\s*\(")
    matches: list[tuple[str, int]] = []
    for index, line in enumerate(lines, start=1):
        match = pattern.match(line)
        if match:
            matches.append((match.group(1), index))

    functions: list[FunctionSpan] = []
    for current_index, (name, start_line) in enumerate(matches):
        end_line = matches[current_index + 1][1] - 1 if current_index + 1 < len(matches) else len(lines)
        snippet = "\n".join(lines[start_line - 1 : end_line])
        functions.append(FunctionSpan(name=name, start_line=start_line, end_line=end_line, text=snippet))
    return functions


def collect_scores(function: FunctionSpan) -> dict[str, object]:
    lowered = function.text.lower()
    category_hits: dict[str, int] = {}
    keyword_hits: dict[str, list[str]] = {}

    for category, keywords in KEYWORD_GROUPS.items():
        hits = [keyword for keyword in keywords if keyword.lower() in lowered]
        if hits:
            category_hits[category] = len(hits)
            keyword_hits[category] = hits

    total_score = sum(category_hits.values())
    return {
        "name": function.name,
        "start_line": function.start_line,
        "end_line": function.end_line,
        "score": total_score,
        "categories": category_hits,
        "keywords": keyword_hits,
    }


def render_markdown(rows: list[dict[str, object]]) -> str:
    lines = [
        "# LLVM Codegen Target Inventory",
        "",
        f"Source: `{SOURCE_PATH}`",
        "",
        "| Function | Lines | Score | Categories |",
        "| --- | ---: | ---: | --- |",
    ]
    for row in rows:
        categories = ", ".join(
            f"{name}:{count}" for name, count in sorted(row["categories"].items())  # type: ignore[arg-type]
        )
        lines.append(
            f"| `{row['name']}` | {row['start_line']}-{row['end_line']} | {row['score']} | {categories or '-'} |"
        )
    lines.append("")
    lines.append("Keyword detail:")
    lines.append("")
    for row in rows:
        lines.append(f"## `{row['name']}`")
        for category, hits in sorted(row["keywords"].items()):  # type: ignore[arg-type]
            lines.append(f"- `{category}`: {', '.join(hits)}")
        lines.append("")
    return "\n".join(lines)


def main() -> None:
    GENERATED_DIR.mkdir(parents=True, exist_ok=True)
    source_text = SOURCE_PATH.read_text(encoding="utf-8")
    functions = parse_functions(source_text)
    ranked = [collect_scores(function) for function in functions]
    ranked.sort(key=lambda row: (-int(row["score"]), int(row["start_line"])))

    payload = {
        "source_path": str(SOURCE_PATH),
        "function_count": len(functions),
        "ranked_functions": ranked,
    }

    JSON_PATH.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    MARKDOWN_PATH.write_text(render_markdown(ranked), encoding="utf-8")

    print(f"Wrote {JSON_PATH}")
    print(f"Wrote {MARKDOWN_PATH}")


if __name__ == "__main__":
    main()
