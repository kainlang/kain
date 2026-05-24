from __future__ import annotations

import json
import re
from dataclasses import dataclass
from pathlib import Path


PACK_ROOT = Path(__file__).resolve().parents[1]
SOURCE_PATH = PACK_ROOT.parent / "src" / "codegen_llvm" / "mod.rs"
GENERATED_DIR = PACK_ROOT / "generated"
JSON_PATH = GENERATED_DIR / "float_semantic_audit.json"
MARKDOWN_PATH = GENERATED_DIR / "float_semantic_audit.md"

PATTERNS = {
    "fptosi_double": re.compile(r"fptosi double"),
    "fcmp_oeq_double": re.compile(r"fcmp oeq double"),
    "fcmp_one_double": re.compile(r"fcmp one double"),
}


@dataclass
class FunctionSpan:
    name: str
    start_line: int
    end_line: int


def parse_functions(lines: list[str]) -> list[FunctionSpan]:
    pattern = re.compile(r"^\s*fn\s+([A-Za-z0-9_]+)\s*\(")
    matches: list[tuple[str, int]] = []
    for index, line in enumerate(lines, start=1):
        match = pattern.match(line)
        if match:
            matches.append((match.group(1), index))

    spans: list[FunctionSpan] = []
    for current, (name, start_line) in enumerate(matches):
        end_line = matches[current + 1][1] - 1 if current + 1 < len(matches) else len(lines)
        spans.append(FunctionSpan(name=name, start_line=start_line, end_line=end_line))
    return spans


def find_owner(functions: list[FunctionSpan], line_no: int) -> FunctionSpan | None:
    for function in functions:
        if function.start_line <= line_no <= function.end_line:
            return function
    return None


def collect_rows(lines: list[str], functions: list[FunctionSpan]) -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    for line_no, line in enumerate(lines, start=1):
        for kind, pattern in PATTERNS.items():
            if pattern.search(line):
                owner = find_owner(functions, line_no)
                context_start = max(1, line_no - 2)
                context_end = min(len(lines), line_no + 2)
                context = [
                    {
                        "line": current,
                        "text": lines[current - 1],
                    }
                    for current in range(context_start, context_end + 1)
                ]
                rows.append(
                    {
                        "kind": kind,
                        "line": line_no,
                        "code": line.strip(),
                        "function": owner.name if owner else None,
                        "function_start_line": owner.start_line if owner else None,
                        "function_end_line": owner.end_line if owner else None,
                        "context": context,
                    }
                )
    return rows


def render_markdown(rows: list[dict[str, object]]) -> str:
    lines = [
        "# LLVM Float Semantic Audit",
        "",
        f"Source: `{SOURCE_PATH}`",
        "",
        "| Kind | Line | Function | Code |",
        "| --- | ---: | --- | --- |",
    ]
    for row in rows:
        function = row["function"] or "-"
        code = str(row["code"]).replace("|", "\\|")
        lines.append(f"| `{row['kind']}` | {row['line']} | `{function}` | `{code}` |")

    lines.append("")
    lines.append("Context:")
    lines.append("")
    for row in rows:
        lines.append(f"## `{row['kind']}` at line {row['line']}")
        function = row["function"] or "-"
        lines.append(f"- Function: `{function}`")
        for context in row["context"]:  # type: ignore[index]
            marker = ">>" if context["line"] == row["line"] else "  "
            lines.append(f"{marker} {context['line']}: {context['text']}")
        lines.append("")
    return "\n".join(lines)


def main() -> None:
    GENERATED_DIR.mkdir(parents=True, exist_ok=True)
    lines = SOURCE_PATH.read_text(encoding="utf-8").splitlines()
    functions = parse_functions(lines)
    rows = collect_rows(lines, functions)
    payload = {
        "source_path": str(SOURCE_PATH),
        "row_count": len(rows),
        "rows": rows,
    }
    JSON_PATH.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    MARKDOWN_PATH.write_text(render_markdown(rows), encoding="utf-8")
    print(f"Wrote {JSON_PATH}")
    print(f"Wrote {MARKDOWN_PATH}")


if __name__ == "__main__":
    main()
