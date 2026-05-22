"""
run_pipeline.py
────────────────
Runs the full Kain native-core formal analysis pipeline in order:
  01 → Catalog range-touching functions
  02 → Find sync gaps
  03 → Arithmetic overflow risk scan
  04 → Auto-generate Z3 proofs from scan outputs
  05 → Branch condition range-narrowing analysis (BPF verifier compat)
  06 → Memory ordering audit (CAS failure-order violations, silent remaps)
  07 → Ownership state machine audit (transition guards, observer invariants)
  08 → Abstract concept prover (invariant-first, independent of C patterns)

Usage:
  python run_pipeline.py [--verifier PATH] [--skip 04] [--only 03]
"""

import subprocess
import sys
import time
import argparse
from pathlib import Path
from rich.console import Console
from rich.panel import Panel
from rich.progress import Progress, SpinnerColumn, TextColumn, TimeElapsedColumn

SCRIPTS_DIR = Path(__file__).parent
DATA_DIR = SCRIPTS_DIR.parent / "data"
VERIFIER_DEFAULT = SCRIPTS_DIR.parent.parent.parent / "verifier.c"

PIPELINE = [
    ("01", "01_catalog_range_functions.py",      "Catalog range-touching functions"),
    ("02", "02_find_sync_gaps.py",               "Find sync gaps in call sites"),
    ("03", "03_arithmetic_scanner.py",            "Arithmetic overflow risk scan"),
    ("04", "04_auto_z3_prover.py",               "Auto-generate and run Z3 proofs"),
    ("05", "05_branch_condition_extractor.py",    "Extract branch condition narrowing"),
    ("06", "06_memory_order_auditor.py",          "Memory ordering audit (CAS/store/failure-order)"),
    ("07", "07_ownership_state_machine_auditor.py","Ownership state machine audit"),
    ("08", "08_abstract_concept_prover.py",       "Abstract concept prover (invariant-first Z3)"),
]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--verifier", default=str(VERIFIER_DEFAULT))
    parser.add_argument("--skip", nargs="*", default=[], help="Script IDs to skip e.g. --skip 04")
    parser.add_argument("--only", nargs="*", default=[], help="Only run these script IDs e.g. --only 01 03")
    args = parser.parse_args()

    console = Console()
    console.print(Panel.fit(
        "[bold cyan]Kain Native Core Formal Analysis Pipeline[/bold cyan]\n"
        "[dim]runtime/native/src/core — Z3-backed security and correctness audit[/dim]",
        border_style="cyan",
    ))

    DATA_DIR.mkdir(exist_ok=True)
    total_start = time.time()
    results = []

    for script_id, script_file, description in PIPELINE:
        if args.only and script_id not in args.only:
            console.print(f"  [dim]⏭  {script_id}: {description} (skipped by --only)[/dim]")
            continue
        if script_id in args.skip:
            console.print(f"  [dim]⏭  {script_id}: {description} (skipped by --skip)[/dim]")
            continue

        console.print(f"\n[bold]>> Step {script_id}:[/bold] {description}")
        script_path = SCRIPTS_DIR / script_file

        if not script_path.exists():
            console.print(f"  [red]Script not found: {script_path}[/red]")
            results.append((script_id, "MISSING", 0))
            continue

        t_start = time.time()
        try:
            proc = subprocess.run(
                [sys.executable, str(script_path), "--verifier", args.verifier],
                capture_output=False,   # let output stream to terminal
                timeout=300,
            )
            elapsed = time.time() - t_start
            if proc.returncode == 0:
                console.print(f"  [green]✓ Done[/green] in {elapsed:.1f}s")
                results.append((script_id, "OK", elapsed))
            else:
                console.print(f"  [red]✗ Failed (exit {proc.returncode})[/red] in {elapsed:.1f}s")
                results.append((script_id, "FAILED", elapsed))
        except subprocess.TimeoutExpired:
            console.print(f"  [red]✗ Timeout (300s)[/red]")
            results.append((script_id, "TIMEOUT", 300))
        except Exception as e:
            console.print(f"  [red]✗ Error: {e}[/red]")
            results.append((script_id, "ERROR", 0))

    total_elapsed = time.time() - total_start
    console.print(f"\n[bold]Pipeline complete in {total_elapsed:.1f}s[/bold]")
    console.print(f"Data outputs: [dim]{DATA_DIR}[/dim]")

    # Summary
    console.print("\n[bold]Step Results:[/bold]")
    for sid, status, elapsed in results:
        style = "green" if status == "OK" else "red"
        console.print(f"  {sid}: [{style}]{status}[/{style}] ({elapsed:.1f}s)")

    # List generated data files
    console.print("\n[bold]Generated Data Files:[/bold]")
    for f in sorted(DATA_DIR.glob("*")):
        size_kb = f.stat().st_size / 1024
        console.print(f"  [dim]{f.name}[/dim]  ({size_kb:.1f} KB)")


if __name__ == "__main__":
    main()
