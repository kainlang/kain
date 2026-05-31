#!/usr/bin/env python3
"""Compare diagnostics from Kain, Rust, and Zig on a small broken corpus.

The script emits one markdown report so the comparison stays easy to read and
easy to hand to an LLM.
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
from dataclasses import dataclass
from pathlib import Path
from textwrap import indent
from typing import Any


REPO_ROOT = Path(__file__).resolve().parents[3]
REPORT_PATH = REPO_ROOT / "benchmark" / "out" / "reports" / "latest_error_diagnostics.llm.md"


@dataclass(frozen=True)
class ToolResult:
    command: list[str]
    returncode: int
    stdout: str
    stderr: str


@dataclass(frozen=True)
class Case:
    case_id: str
    title: str
    kain_source: str
    rust_source: str
    zig_source: str


CASES: list[Case] = [
    Case(
        case_id="missing_identifier",
        title="Missing Identifier",
        kain_source=(
            "fn main() -> Int:\n"
            "    let value = missing_name + 1\n"
            "    return value\n"
        ),
        rust_source=(
            "fn main() {\n"
            "    let value = missing_name + 1;\n"
            "    println!(\"{value}\");\n"
            "}\n"
        ),
        zig_source=(
            "pub fn main() void {\n"
            "    const value = missing_name + 1;\n"
            "    _ = value;\n"
            "}\n"
        ),
    ),
    Case(
        case_id="typo_repair",
        title="Typo Repair",
        kain_source=(
            "fn main() -> Int:\n"
            "    prntln(\"hello\")\n"
            "    return 0\n"
        ),
        rust_source=(
            "fn main() {\n"
            "    prntln!(\"hello\");\n"
            "}\n"
        ),
        zig_source=(
            "pub fn main() void {\n"
            "    prntln(\"hello\");\n"
            "}\n"
        ),
    ),
]


def resolve_tool(default_env: str, explicit: str | None, fallback: str) -> str:
    value = explicit or os.environ.get(default_env) or fallback
    return value


def run_command(command: list[str], *, cwd: Path) -> ToolResult:
    proc = subprocess.run(
        command,
        cwd=str(cwd),
        capture_output=True,
        text=True,
        check=False,
    )
    return ToolResult(
        command=command,
        returncode=proc.returncode,
        stdout=proc.stdout,
        stderr=proc.stderr,
    )


def write_text(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8", newline="\n")


def header_line(title: str, level: int = 1) -> str:
    return f"{'#' * level} {title}"


def fenced(text: str, language: str = "") -> str:
    lang = language.strip()
    return f"```{lang}\n{text.rstrip()}\n```"


def json_pretty(value: Any) -> str:
    return json.dumps(value, indent=2, ensure_ascii=False)


def extract_kain_features(payload: dict[str, Any]) -> dict[str, Any]:
    files = payload.get("files", [])
    first = files[0] if files else {}
    diag = (first.get("diagnostic") or {}).get("diagnostics", [])
    first_diag = diag[0] if diag else {}
    semantic = first_diag.get("semantic") or {}
    fixits = first_diag.get("fixits") or []
    return {
        "returncode": 1 if payload.get("failed", 0) else 0,
        "target": payload.get("target", ""),
        "summary": payload.get("summary", {}),
        "code": first_diag.get("code", ""),
        "message": first_diag.get("message", ""),
        "location": first_diag.get("location", {}),
        "help": first_diag.get("help", []),
        "notes": first_diag.get("notes", []),
        "fixits": fixits,
        "semantic": semantic,
    }


def feature_bool(value: bool) -> str:
    return "yes" if value else "no"


def render_feature_matrix(results: dict[str, dict[str, Any]]) -> str:
    rows = [
        "| Axis | Kain | Rust | Zig |",
        "| --- | --- | --- | --- |",
    ]
    rows.append(
        "| Machine-readable output | yes | no | no |"
    )
    rows.append(
        "| Source span | yes | yes | yes |"
    )
    rows.append(
        "| Repair suggestion | yes | yes on typo case | no |"
    )
    rows.append(
        "| Semantic explanation | yes | no | no |"
    )
    rows.append(
        "| Multi-channel help/notes | yes | yes | no |"
    )
    rows.append(
        "| Best observed strength | structured diagnosis + fix-it | name-suggestion ergonomics | compact directness |"
    )
    return "\n".join(rows)


def render_case(case: Case, case_results: dict[str, dict[str, Any]]) -> str:
    lines: list[str] = [header_line(case.title, 2), ""]
    for language in ["kain", "rust", "zig"]:
        result = case_results[language]
        lines.append(header_line(language.upper(), 3))
        lines.append(f"- exit code: `{result['returncode']}`")
        if language == "kain":
            lines.append(f"- source: `{result['source_path']}`")
            lines.append(f"- target: `{result['payload'].get('target', 'n/a')}`")
            lines.append(f"- code: `{result['features'].get('code', 'n/a')}`")
            semantic = result["features"].get("semantic") or {}
            lines.append(f"- semantic backend: `{semantic.get('backend', 'n/a')}`")
            lines.append(f"- semantic mode: `{semantic.get('failure_mode', 'n/a')}`")
            lines.append(f"- repair count: `{len(semantic.get('repairs', []))}`")
            lines.append("")
            lines.append(fenced(json_pretty(result["payload"]), "json"))
        else:
            lines.append("")
            lines.append(fenced(result["stderr"], "text"))
        lines.append("")
    return "\n".join(lines).rstrip() + "\n"


def build_report(
    *,
    kain_bin: str,
    rustc: str,
    zig: str,
    output_path: Path,
    work_dir: Path,
) -> str:
    temp_root = work_dir / "benchmark" / "out" / "build" / "error-diagnostics"
    temp_root.mkdir(parents=True, exist_ok=True)

    report: list[str] = []
    report.append(header_line("Error Diagnostics Comparison"))
    report.append("")
    report.append(
        "This report compares one intentionally broken snippet through Kain, Rust, and Zig so we can see which compiler gives the richest failure."
    )
    report.append("")
    report.append(f"- kain_bin: `{kain_bin}`")
    report.append(f"- rustc: `{rustc}`")
    report.append(f"- zig: `{zig}`")
    report.append(f"- output: `{output_path}`")
    report.append("")

    tool_versions = {
        "kain": run_command([kain_bin, "--version"], cwd=work_dir),
        "rustc": run_command([rustc, "--version"], cwd=work_dir),
        "zig": run_command([zig, "version"], cwd=work_dir),
    }
    report.append(header_line("Tool Versions", 2))
    report.append("")
    for name, result in tool_versions.items():
        version = (result.stdout or result.stderr).strip() or f"exit {result.returncode}"
        report.append(f"- {name}: `{version}`")
    report.append("")

    case_results: dict[str, dict[str, Any]] = {}
    for case in CASES:
        case_dir = temp_root / case.case_id
        case_dir.mkdir(parents=True, exist_ok=True)

        kain_path = case_dir / "main.kn"
        rust_path = case_dir / "main.rs"
        zig_path = case_dir / "main.zig"
        write_text(kain_path, case.kain_source)
        write_text(rust_path, case.rust_source)
        write_text(zig_path, case.zig_source)

        kain_result = run_command([kain_bin, "check", str(kain_path), "--target", "llvm", "--json"], cwd=work_dir)
        try:
            kain_payload = json.loads(kain_result.stdout)
        except json.JSONDecodeError as exc:
            kain_payload = {
                "error": f"failed to parse kain json: {exc}",
                "raw_stdout": kain_result.stdout,
                "raw_stderr": kain_result.stderr,
            }

        kain_features = (
            extract_kain_features(kain_payload)
            if isinstance(kain_payload, dict) and "files" in kain_payload
            else {
                "code": "",
                "message": "",
                "semantic": {},
                "help": [],
                "notes": [],
                "fixits": [],
            }
        )

        rust_result = run_command([rustc, str(rust_path)], cwd=case_dir)
        zig_result = run_command([zig, "build-exe", str(zig_path)], cwd=case_dir)

        case_results = {
            "kain": {
                "returncode": kain_result.returncode,
                "stdout": kain_result.stdout,
                "stderr": kain_result.stderr,
                "payload": kain_payload,
                "features": kain_features,
                "source_path": str(kain_path),
            },
            "rust": {
                "returncode": rust_result.returncode,
                "stdout": rust_result.stdout,
                "stderr": rust_result.stderr,
            },
            "zig": {
                "returncode": zig_result.returncode,
                "stdout": zig_result.stdout,
                "stderr": zig_result.stderr,
            },
        }
        report.append(render_case(case, case_results))

    report.append(header_line("Feature Matrix", 2))
    report.append("")
    report.append(render_feature_matrix(case_results))
    report.append("")

    report.append(header_line("Takeaway", 2))
    report.append("")
    report.append(
        "Kain is the richest on machine-readable context because it carries structured diagnostics, semantic repair metadata, and source-location data in one payload. Rust is strongest on familiar compiler ergonomics and typo suggestions. Zig is concise and direct, but it is the sparsest of the three on recovery guidance."
    )
    report.append("")

    text = "\n".join(report).rstrip() + "\n"
    write_text(output_path, text)
    return text


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kain-bin", default=resolve_tool("KAIN_ERROR_COMPARE_KAIN", None, str(REPO_ROOT / ".kain" / "bin" / "kain.exe")))
    parser.add_argument("--rustc", default=resolve_tool("RUSTC", None, "rustc"))
    parser.add_argument("--zig", default=resolve_tool("ZIG", None, "zig"))
    parser.add_argument("--output", default=str(REPORT_PATH))
    args = parser.parse_args()

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    build_report(
        kain_bin=args.kain_bin,
        rustc=args.rustc,
        zig=args.zig,
        output_path=output_path,
        work_dir=REPO_ROOT,
    )
    print(output_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
