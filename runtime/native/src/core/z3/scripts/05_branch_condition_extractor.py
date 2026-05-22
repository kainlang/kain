"""
05_branch_condition_extractor.py
──────────────────────────────────
Extracts all conditional branch range-narrowing logic from verifier.c.
These are the functions that update register bounds after JGT, JGE,
JLT, JLE, JEQ, JNE instructions — historically the richest source of
BPF verifier escapes.

For each branch condition found, outputs:
  - The full condition expression
  - Which range fields are updated in true/false branches
  - Whether both branches are handled
  - A Z3-ready JSON representation for manual/automated proofs

Outputs:
  data/branch_conditions.json
  data/branch_conditions.csv

Usage:
  python 05_branch_condition_extractor.py [--verifier PATH]
"""

import re
import json
import csv
import argparse
from pathlib import Path
from dataclasses import dataclass, field, asdict
from rich.console import Console
from rich.table import Table
from rich import box

VERIFIER_PATH = Path(__file__).parent.parent.parent / "verifier.c"
DATA_DIR = Path(__file__).parent.parent / "data"

# BPF comparison opcodes
BPF_JMP_OPS = {
    "BPF_JEQ":  "==",
    "BPF_JNE":  "!=",
    "BPF_JGT":  ">",  "BPF_JSGT": ">s",
    "BPF_JGE":  ">=", "BPF_JSGE": ">=s",
    "BPF_JLT":  "<",  "BPF_JSLT": "<s",
    "BPF_JLE":  "<=", "BPF_JSLE": "<=s",
    "BPF_JSET": "&",
}

# Functions that perform branch-triggered range updates
BRANCH_NARROWING_FUNCTIONS = [
    "reg_set_min_max",
    "reg_set_min_max_inv",
    "__reg_set_min_max",
    "find_equal_scalars",
    "find_eq_cond_range",
    "__update_reg_bounds",
    "reg_bounds_sync",
]

BOUND_FIELDS = [
    "smin_value", "smax_value",
    "umin_value", "umax_value",
    "s32_min_value", "s32_max_value",
    "u32_min_value", "u32_max_value",
]


@dataclass
class BranchArm:
    condition_type: str    # "true" or "false"
    fields_updated: list[str]
    expressions: list[str]


@dataclass
class BranchCondition:
    function_name: str
    start_line: int
    jmp_op: str            # BPF_JGT, BPF_JSGE, etc.
    math_op: str           # >, >=, <, <=, ==, !=
    src_is_const: bool
    true_arm: BranchArm
    false_arm: BranchArm
    has_both_arms: bool
    asymmetric: bool       # true arm updates different fields than false arm
    risk_score: int
    risk_reasons: list[str]
    raw_snippet: str


def extract_function_bodies(source: str) -> list[tuple[str, int, str]]:
    """Returns (function_name, start_line, body) for every function."""
    lines = source.splitlines()
    func_re = re.compile(r"^(?:static\s+)?(?:[\w\*]+\s+)+(\w+)\s*\([^)]*\)\s*$")
    results = []
    i = 0
    while i < len(lines):
        m = func_re.match(lines[i].rstrip())
        if m:
            name = m.group(1)
            brace_line = i
            while brace_line < len(lines) and "{" not in lines[brace_line]:
                brace_line += 1
            if brace_line >= len(lines):
                i += 1
                continue
            depth = 0
            start = brace_line
            end = start
            for j in range(start, len(lines)):
                depth += lines[j].count("{") - lines[j].count("}")
                if depth == 0 and j > start:
                    end = j
                    break
            body = "\n".join(lines[start:end+1])
            results.append((name, i + 1, body))
            i = end + 1
        else:
            i += 1
    return results


def extract_branch_arms(body: str, start_line: int) -> tuple[BranchArm, BranchArm]:
    """
    Try to identify true/false branch arms by scanning for if/else blocks
    and which bound fields they update.
    """
    body_lines = body.splitlines()
    true_fields = []
    false_fields = []
    true_exprs = []
    false_exprs = []

    in_else = False
    brace_depth = 0

    for rel, line in enumerate(body_lines):
        stripped = line.strip()
        abs_line = start_line + rel

        if "else" in stripped and "{" in stripped:
            in_else = True
        elif stripped == "{" and in_else:
            pass
        elif stripped in ("}", "};"):
            if in_else and brace_depth == 0:
                in_else = False

        brace_depth += stripped.count("{") - stripped.count("}")

        # Track bound field assignments
        for f in BOUND_FIELDS:
            if f in stripped and ("=" in stripped or "+=" in stripped or "-=" in stripped):
                if in_else:
                    if f not in false_fields:
                        false_fields.append(f)
                    false_exprs.append(stripped[:80])
                else:
                    if f not in true_fields:
                        true_fields.append(f)
                    true_exprs.append(stripped[:80])

    return (
        BranchArm("true", true_fields, true_exprs[:10]),
        BranchArm("false", false_fields, false_exprs[:10]),
    )


def analyze_branch_function(name: str, start_line: int, body: str) -> list[BranchCondition]:
    """
    Scan a function body for BPF jump opcode handling and extract
    the range narrowing logic.
    """
    conditions = []
    body_lines = body.splitlines()

    for rel, line in enumerate(body_lines):
        stripped = line.strip()
        for jmp_op, math_op in BPF_JMP_OPS.items():
            if jmp_op not in stripped:
                continue
            # Find this as a switch case or if condition
            if "case" not in stripped and "==" not in stripped and jmp_op not in stripped:
                continue

            abs_line = start_line + rel

            # Extract a window around this line for context
            window_start = max(0, rel - 2)
            window_end = min(len(body_lines), rel + 60)
            window_body = "\n".join(body_lines[window_start:window_end])
            window_start_line = start_line + window_start

            true_arm, false_arm = extract_branch_arms(window_body, window_start_line)

            src_is_const = "K" in stripped or "imm" in stripped or "const" in stripped.lower()
            has_both = bool(true_arm.fields_updated) and bool(false_arm.fields_updated)

            # Asymmetry: true arm updates different bound types than false arm
            true_set = set(true_arm.fields_updated)
            false_set = set(false_arm.fields_updated)
            asymmetric = true_set != false_set and bool(true_set) and bool(false_set)

            # Risk scoring
            score = 0
            reasons = []

            if not has_both:
                score += 40
                reasons.append("Only one branch arm updates bounds (missing arm may leave stale range)")

            if asymmetric:
                score += 30
                reasons.append(f"True arm updates {sorted(true_set)} but false arm updates {sorted(false_set)}")

            signed_only_true = all("s" in f and "u" not in f for f in true_set) if true_set else False
            unsigned_only_false = all("u" in f and "s" not in f for f in false_set) if false_set else False
            if signed_only_true and unsigned_only_false:
                score += 25
                reasons.append("True branch updates signed bounds, false updates unsigned (cross-domain risk)")

            if "JSGT" in jmp_op or "JSGE" in jmp_op or "JSLT" in jmp_op or "JSLE" in jmp_op:
                score += 10
                reasons.append("Signed comparison — sign bit semantics complex")

            conditions.append(BranchCondition(
                function_name=name,
                start_line=abs_line,
                jmp_op=jmp_op,
                math_op=math_op,
                src_is_const=src_is_const,
                true_arm=true_arm,
                false_arm=false_arm,
                has_both_arms=has_both,
                asymmetric=asymmetric,
                risk_score=score,
                risk_reasons=reasons,
                raw_snippet="\n".join(body_lines[max(0, rel-1):rel+8])[:400],
            ))

    return conditions


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--verifier", default=str(VERIFIER_PATH))
    parser.add_argument("--focus", default="", help="Only show functions matching this substring")
    args = parser.parse_args()

    console = Console()
    verifier_path = Path(args.verifier)
    source = verifier_path.read_text(encoding="utf-8", errors="replace")

    console.print(f"\n[bold cyan]BPF Verifier Branch Condition Extractor[/bold cyan]")
    console.print(f"Source: [dim]{verifier_path}[/dim]\n")

    console.print("[yellow]Extracting function bodies...[/yellow]")
    all_funcs = extract_function_bodies(source)
    console.print(f"Found {len(all_funcs)} functions\n")

    # Focus on branch-narrowing functions first, then scan all
    priority_names = set(BRANCH_NARROWING_FUNCTIONS)
    priority_funcs = [(n, l, b) for n, l, b in all_funcs if n in priority_names]
    other_funcs = [(n, l, b) for n, l, b in all_funcs
                   if n not in priority_names and any(
                       op in b for op in BPF_JMP_OPS.keys()
                   )]

    console.print(f"Priority branch functions found: [bold]{len(priority_funcs)}[/bold]")
    console.print(f"Other functions containing JMP ops: [bold]{len(other_funcs)}[/bold]\n")

    all_conditions: list[BranchCondition] = []
    for name, start, body in priority_funcs + other_funcs[:30]:
        if args.focus and args.focus not in name:
            continue
        conds = analyze_branch_function(name, start, body)
        all_conditions.extend(conds)

    all_conditions.sort(key=lambda c: c.risk_score, reverse=True)
    console.print(f"Extracted [bold]{len(all_conditions)}[/bold] branch conditions\n")

    # ── Rich table ─────────────────────────────────────────────────────────
    table = Table(
        title="Branch Condition Range-Narrowing Analysis",
        box=box.SIMPLE_HEAVY, show_lines=False,
    )
    table.add_column("Risk", width=5, justify="right", style="bold")
    table.add_column("Line", width=7, justify="right")
    table.add_column("JMP Op", width=9, style="cyan")
    table.add_column("Function", width=32, style="bold white")
    table.add_column("True Fields", width=25, style="green")
    table.add_column("False Fields", width=25, style="yellow")
    table.add_column("Asym?", width=6, justify="center")
    table.add_column("Flags", width=55, style="dim red")

    for c in all_conditions[:40]:
        risk_str = f"[red]{c.risk_score}[/red]" if c.risk_score >= 40 else (
            f"[yellow]{c.risk_score}[/yellow]" if c.risk_score >= 20 else str(c.risk_score)
        )
        true_f = ", ".join(c.true_arm.fields_updated)[:25] or "[dim]—[/dim]"
        false_f = ", ".join(c.false_arm.fields_updated)[:25] or "[dim]—[/dim]"
        asym_str = "[red]YES[/red]" if c.asymmetric else "[green]no[/green]"
        flags = "; ".join(c.risk_reasons)[:55] or "—"
        table.add_row(
            risk_str, str(c.start_line), c.jmp_op, c.function_name,
            true_f, false_f, asym_str, flags,
        )

    console.print(table)

    # ── High-risk snippet dump ─────────────────────────────────────────────
    high_risk = [c for c in all_conditions if c.risk_score >= 40][:5]
    if high_risk:
        console.print("\n[bold red]High-Risk Branch Conditions — Code Snippets:[/bold red]")
        for c in high_risk:
            console.print(f"\n[bold]{c.jmp_op}[/bold] at line {c.start_line} in [cyan]{c.function_name}[/cyan] — risk {c.risk_score}")
            console.print(f"  Reasons: [dim]{'; '.join(c.risk_reasons)}[/dim]")
            for line in c.raw_snippet.split("\n")[:8]:
                console.print(f"  [dim]{line}[/dim]")

    # ── Save ──────────────────────────────────────────────────────────────
    DATA_DIR.mkdir(exist_ok=True)
    json_path = DATA_DIR / "branch_conditions.json"
    with open(json_path, "w") as f:
        json.dump([asdict(c) for c in all_conditions], f, indent=2)

    csv_path = DATA_DIR / "branch_conditions.csv"
    with open(csv_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=[
            "function_name", "start_line", "jmp_op", "math_op",
            "src_is_const", "has_both_arms", "asymmetric",
            "true_fields", "false_fields", "risk_score", "risk_reasons",
        ])
        writer.writeheader()
        for c in all_conditions:
            writer.writerow({
                "function_name": c.function_name,
                "start_line": c.start_line,
                "jmp_op": c.jmp_op,
                "math_op": c.math_op,
                "src_is_const": c.src_is_const,
                "has_both_arms": c.has_both_arms,
                "asymmetric": c.asymmetric,
                "true_fields": "|".join(c.true_arm.fields_updated),
                "false_fields": "|".join(c.false_arm.fields_updated),
                "risk_score": c.risk_score,
                "risk_reasons": " | ".join(c.risk_reasons),
            })

    console.print(f"\n[green]JSON:[/green] {json_path}")
    console.print(f"[green]CSV:[/green]  {csv_path}")


if __name__ == "__main__":
    main()
