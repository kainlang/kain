#!/usr/bin/env python3
"""
Paired Kain LLVM vs Rust LLVM benchmark runner.

The benchmark cases themselves stay dependency-free. This runner uses only the
Python standard library so the lane remains easy to run on a fresh workstation.
"""

from __future__ import annotations

import argparse
import html
import json
import os
import re
import shutil
import statistics
import subprocess
import sys
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


BENCHMARK_ROOT = Path(__file__).resolve().parent
REPO_ROOT = BENCHMARK_ROOT.parent
OUT_ROOT = BENCHMARK_ROOT / "out"
BUILD_ROOT = OUT_ROOT / "build"
REPORT_ROOT = OUT_ROOT / "reports"
ANSI_RE = re.compile(r"\x1b\[[0-9;]*m")


@dataclass
class CommandResult:
    command: list[str]
    returncode: int
    stdout: str
    stderr: str
    elapsed_ms: float


def strip_ansi(value: str) -> str:
    return ANSI_RE.sub("", value)


def repo_relative(path: Path) -> str:
    try:
        return str(path.resolve().relative_to(REPO_ROOT)).replace("\\", "/")
    except ValueError:
        return str(path)


def display_command(command: list[str]) -> str:
    return " ".join(command)


def run_command(command: list[str], timeout: int, cwd: Path = REPO_ROOT) -> CommandResult:
    start = time.perf_counter_ns()
    completed = subprocess.run(
        command,
        cwd=str(cwd),
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=timeout,
    )
    elapsed_ms = (time.perf_counter_ns() - start) / 1_000_000.0
    return CommandResult(
        command=command,
        returncode=completed.returncode,
        stdout=strip_ansi(completed.stdout),
        stderr=strip_ansi(completed.stderr),
        elapsed_ms=elapsed_ms,
    )


def find_line_that_looks_like_path(output: str) -> str | None:
    for raw_line in reversed(output.splitlines()):
        line = strip_ansi(raw_line).strip()
        if not line:
            continue
        if ":" in line or line.startswith("/") or line.startswith("\\"):
            return line
    return None


def resolve_kain_exe(explicit: str | None, timeout: int) -> Path:
    candidates: list[Path] = []

    if explicit:
        candidates.append(Path(explicit))

    env_kain = os.environ.get("KAIN_EXE")
    if env_kain:
        candidates.append(Path(env_kain))

    bazel = shutil.which("bazel")
    if bazel:
        build = run_command([bazel, "build", "//:kain", "--config=dev"], timeout=timeout)
        info = run_command([bazel, "info", "bazel-bin", "--config=dev"], timeout=timeout)
        info_line = find_line_that_looks_like_path(info.stdout)
        if info_line:
            candidates.append(Path(info_line) / "crates" / "cli" / executable_name("kain"))
        if build.returncode != 0 and not any(candidate.exists() for candidate in candidates):
            combined = (build.stdout + "\n" + build.stderr).strip()
            raise RuntimeError(f"Unable to build //:kain with Bazel.\n{combined}")

    candidates.append(REPO_ROOT / "target" / "debug" / executable_name("kain"))

    path_kain = shutil.which("kain")
    if path_kain:
        candidates.append(Path(path_kain))

    for candidate in candidates:
        if candidate.exists():
            return candidate.resolve()

    raise RuntimeError(
        "Could not find kain compiler. Set KAIN_EXE or pass --kain-exe."
    )


def executable_name(stem: str) -> str:
    if os.name == "nt":
        return f"{stem}.exe"
    return stem


def load_manifest(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        manifest = json.load(handle)
    if "cases" not in manifest or not isinstance(manifest["cases"], list):
        raise ValueError("manifest must contain a cases array")
    return manifest


def selected_cases(manifest: dict[str, Any], only_case: str | None) -> list[dict[str, Any]]:
    cases = manifest["cases"]
    if only_case is None:
        return cases
    selected = [case for case in cases if case["id"] == only_case]
    if not selected:
        raise ValueError(f"unknown case: {only_case}")
    return selected


def validate_case_files(case: dict[str, Any]) -> None:
    kain_path = BENCHMARK_ROOT / case["kain"]
    rust_path = BENCHMARK_ROOT / case["rust"]
    if not kain_path.exists():
        raise FileNotFoundError(f"missing Kain benchmark: {kain_path}")
    if not rust_path.exists():
        raise FileNotFoundError(f"missing Rust benchmark: {rust_path}")


def build_kain_case(
    case: dict[str, Any],
    kain_exe: Path,
    timeout: int,
    no_build: bool,
) -> dict[str, Any]:
    case_id = case["id"]
    build_dir = BUILD_ROOT / case_id / "kain"
    build_dir.mkdir(parents=True, exist_ok=True)
    ll_path = build_dir / f"{case_id}.ll"
    exe_path = build_dir / executable_name(case_id)

    command = [
        str(kain_exe),
        repo_relative(BENCHMARK_ROOT / case["kain"]),
        "-t",
        "llvm",
        "-o",
        repo_relative(ll_path),
    ]

    if no_build:
        return {
            "ok": exe_path.exists(),
            "language": "kain",
            "exe": str(exe_path),
            "command": command,
            "build_ms": 0.0,
            "stdout": "",
            "stderr": "",
            "error": "" if exe_path.exists() else f"missing existing executable {exe_path}",
        }

    result = run_command(command, timeout=timeout)
    produced_exe = ll_path.with_suffix(".exe" if os.name == "nt" else "")
    if produced_exe.exists() and produced_exe != exe_path:
        shutil.copyfile(produced_exe, exe_path)
    elif produced_exe.exists():
        exe_path = produced_exe

    ok = result.returncode == 0 and exe_path.exists()
    return {
        "ok": ok,
        "language": "kain",
        "exe": str(exe_path),
        "command": command,
        "build_ms": result.elapsed_ms,
        "stdout": result.stdout[-4000:],
        "stderr": result.stderr[-4000:],
        "error": "" if ok else "Kain build failed or did not produce executable.",
    }


def build_rust_case(
    case: dict[str, Any],
    rustc: str,
    timeout: int,
    no_build: bool,
) -> dict[str, Any]:
    case_id = case["id"]
    build_dir = BUILD_ROOT / case_id / "rust"
    build_dir.mkdir(parents=True, exist_ok=True)
    exe_path = build_dir / executable_name(case_id)
    command = [
        rustc,
        repo_relative(BENCHMARK_ROOT / case["rust"]),
        "-C",
        "opt-level=3",
        "-C",
        "target-cpu=native",
        "-C",
        "debuginfo=0",
        "-C",
        "panic=abort",
        "-C",
        "overflow-checks=off",
        "-o",
        repo_relative(exe_path),
    ]

    if no_build:
        return {
            "ok": exe_path.exists(),
            "language": "rust",
            "exe": str(exe_path),
            "command": command,
            "build_ms": 0.0,
            "stdout": "",
            "stderr": "",
            "error": "" if exe_path.exists() else f"missing existing executable {exe_path}",
        }

    result = run_command(command, timeout=timeout)
    ok = result.returncode == 0 and exe_path.exists()
    return {
        "ok": ok,
        "language": "rust",
        "exe": str(exe_path),
        "command": command,
        "build_ms": result.elapsed_ms,
        "stdout": result.stdout[-4000:],
        "stderr": result.stderr[-4000:],
        "error": "" if ok else "Rust build failed or did not produce executable.",
    }


def run_executable(exe: str, timeout: int) -> CommandResult:
    return run_command([exe], timeout=timeout)


def measure_executable(
    build: dict[str, Any],
    warmups: int,
    runs: int,
    timeout: int,
) -> dict[str, Any]:
    if not build["ok"]:
        return {
            "ok": False,
            "samples_ms": [],
            "warmups": [],
            "min_ms": None,
            "median_ms": None,
            "mean_ms": None,
            "error": build["error"],
        }

    warmup_results = []
    for _ in range(warmups):
        result = run_executable(build["exe"], timeout=timeout)
        warmup_results.append(result.elapsed_ms)
        if result.returncode != 0:
            return failed_run_result(result, warmup_results)

    samples = []
    for _ in range(runs):
        result = run_executable(build["exe"], timeout=timeout)
        samples.append(result.elapsed_ms)
        if result.returncode != 0:
            return failed_run_result(result, warmup_results, samples)

    return {
        "ok": True,
        "samples_ms": samples,
        "warmups": warmup_results,
        "min_ms": min(samples),
        "median_ms": statistics.median(samples),
        "mean_ms": statistics.fmean(samples),
        "error": "",
    }


def failed_run_result(
    result: CommandResult,
    warmups: list[float],
    samples: list[float] | None = None,
) -> dict[str, Any]:
    return {
        "ok": False,
        "samples_ms": samples or [],
        "warmups": warmups,
        "min_ms": None,
        "median_ms": None,
        "mean_ms": None,
        "error": (
            f"Executable failed with exit code {result.returncode}.\n"
            f"stdout:\n{result.stdout[-2000:]}\n"
            f"stderr:\n{result.stderr[-2000:]}"
        ),
    }


def benchmark_case(
    case: dict[str, Any],
    kain_exe: Path,
    rustc: str,
    warmups: int,
    runs: int,
    timeout: int,
    no_build: bool,
) -> dict[str, Any]:
    validate_case_files(case)
    kain_build = build_kain_case(case, kain_exe, timeout, no_build)
    rust_build = build_rust_case(case, rustc, timeout, no_build)
    kain_run = measure_executable(kain_build, warmups, runs, timeout)
    rust_run = measure_executable(rust_build, warmups, runs, timeout)

    speedup = None
    winner = "n/a"
    if kain_run["ok"] and rust_run["ok"]:
        kain_median = float(kain_run["median_ms"])
        rust_median = float(rust_run["median_ms"])
        if kain_median > 0:
            speedup = rust_median / kain_median
        if kain_median < rust_median:
            winner = "kain"
        elif rust_median < kain_median:
            winner = "rust"
        else:
            winner = "tie"

    return {
        "id": case["id"],
        "title": case.get("title", case["id"]),
        "description": case.get("description", ""),
        "maturity": case.get("maturity", "implemented"),
        "fairness_note": case.get("fairness_note", ""),
        "source": {
            "kain": case["kain"],
            "rust": case["rust"],
        },
        "build": {
            "kain": kain_build,
            "rust": rust_build,
        },
        "run": {
            "kain": kain_run,
            "rust": rust_run,
        },
        "speedup_rust_over_kain": speedup,
        "winner": winner,
    }


def fmt_ms(value: Any) -> str:
    if value is None:
        return "n/a"
    return f"{float(value):.3f}"


def fmt_ratio(value: Any) -> str:
    if value is None:
        return "n/a"
    ratio = float(value)
    if ratio >= 1.0:
        return f"Kain {ratio:.2f}x faster"
    return f"Rust {(1.0 / ratio):.2f}x faster"


def render_samples(samples: list[float]) -> str:
    return ", ".join(f"{sample:.2f}" for sample in samples)


def render_html(report: dict[str, Any]) -> str:
    generated = html.escape(report["generated_at"])
    rows = []
    detail_sections = []
    max_median = 0.0
    for case in report["cases"]:
        for lang in ("kain", "rust"):
            median = case["run"][lang]["median_ms"]
            if median is not None:
                max_median = max(max_median, float(median))

    for case in report["cases"]:
        kain_run = case["run"]["kain"]
        rust_run = case["run"]["rust"]
        ratio = fmt_ratio(case["speedup_rust_over_kain"])
        winner = case["winner"]
        kain_bar = bar_width(kain_run["median_ms"], max_median)
        rust_bar = bar_width(rust_run["median_ms"], max_median)
        rows.append(
            "<tr>"
            f"<td><strong>{html.escape(case['id'])}</strong><br><span>{html.escape(case['title'])}</span><br><em>{html.escape(case.get('maturity', 'implemented'))}</em></td>"
            f"<td class='num'>{fmt_ms(kain_run['median_ms'])}</td>"
            f"<td><div class='bar kain' style='width:{kain_bar}%'></div></td>"
            f"<td class='num'>{fmt_ms(rust_run['median_ms'])}</td>"
            f"<td><div class='bar rust' style='width:{rust_bar}%'></div></td>"
            f"<td>{html.escape(winner)}</td>"
            f"<td>{html.escape(ratio)}</td>"
            "</tr>"
        )
        detail_sections.append(render_case_detail(case))

    status_class = "ok" if report["ok"] else "bad"
    status_text = "PASS" if report["ok"] else "CHECK FAILURES"
    return f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Kain vs Rust LLVM Benchmarks</title>
<style>
:root {{
  --ink: #17130d;
  --muted: #756c5f;
  --paper: #f7efe1;
  --panel: #fffaf0;
  --line: #d8c8ad;
  --kain: #c45a2e;
  --rust: #355f72;
  --good: #2d7d46;
  --bad: #9e2f2f;
}}
* {{ box-sizing: border-box; }}
body {{
  margin: 0;
  color: var(--ink);
  background:
    radial-gradient(circle at 12% 10%, rgba(196, 90, 46, .20), transparent 28rem),
    radial-gradient(circle at 90% 4%, rgba(53, 95, 114, .18), transparent 26rem),
    linear-gradient(135deg, #f7efe1, #ead9bb);
  font: 15px/1.45 Georgia, "Times New Roman", serif;
}}
main {{ max-width: 1160px; margin: 0 auto; padding: 30px 18px 44px; }}
header {{ display: grid; gap: 10px; margin-bottom: 18px; }}
h1 {{ margin: 0; font-size: clamp(32px, 6vw, 72px); letter-spacing: -0.06em; line-height: .92; }}
h2 {{ margin: 28px 0 10px; font-size: 24px; }}
.lede {{ max-width: 860px; color: var(--muted); font-size: 17px; }}
.pill {{
  display: inline-block; width: fit-content; padding: 4px 10px;
  border: 1px solid var(--line); border-radius: 999px; background: rgba(255,255,255,.35);
  font: 12px/1.2 Consolas, monospace; letter-spacing: .06em;
}}
.pill.ok {{ color: var(--good); border-color: rgba(45,125,70,.35); }}
.pill.bad {{ color: var(--bad); border-color: rgba(158,47,47,.35); }}
table {{ width: 100%; border-collapse: collapse; background: rgba(255,250,240,.78); box-shadow: 0 18px 60px rgba(65,45,20,.10); }}
th, td {{ border-bottom: 1px solid var(--line); padding: 10px; text-align: left; vertical-align: middle; }}
th {{ font: 12px/1.2 Consolas, monospace; color: var(--muted); text-transform: uppercase; letter-spacing: .08em; }}
td.num {{ font: 14px/1.2 Consolas, monospace; text-align: right; white-space: nowrap; }}
td span {{ color: var(--muted); }}
.bar {{ height: 12px; min-width: 2px; border-radius: 999px; }}
.bar.kain {{ background: var(--kain); }}
.bar.rust {{ background: var(--rust); }}
section.case {{ margin-top: 16px; padding: 14px; border: 1px solid var(--line); background: rgba(255,250,240,.58); }}
pre {{ overflow: auto; padding: 10px; background: #201b15; color: #ffe9c2; font: 12px/1.4 Consolas, monospace; }}
code {{ font-family: Consolas, monospace; }}
.grid {{ display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 10px; }}
.meta {{ color: var(--muted); }}
@media (max-width: 820px) {{ .grid {{ grid-template-columns: 1fr; }} table {{ font-size: 13px; }} }}
</style>
</head>
<body>
<main>
<header>
<span class="pill {status_class}">{status_text}</span>
<h1>Kain vs Rust LLVM</h1>
<p class="lede">Paired native benchmarks. Programs use no external packages; the runner builds Kain LLVM and Rust LLVM artifacts, times executable samples, and rewrites this report every run.</p>
<p class="meta">Generated {generated}. Warmups: {report['warmups']}. Timed runs: {report['runs']}. Host: {html.escape(report['platform'])}.</p>
</header>
<table>
<thead>
<tr><th>Case</th><th>Kain median ms</th><th>Kain</th><th>Rust median ms</th><th>Rust</th><th>Winner</th><th>Ratio</th></tr>
</thead>
<tbody>
{''.join(rows)}
</tbody>
</table>
<h2>Details</h2>
{''.join(detail_sections)}
</main>
</body>
</html>
"""


def bar_width(value: Any, max_value: float) -> int:
    if value is None or max_value <= 0:
        return 0
    return max(2, int((float(value) / max_value) * 100))


def render_case_detail(case: dict[str, Any]) -> str:
    parts = [
        f"<section class='case'><h3>{html.escape(case['id'])}: {html.escape(case['title'])}</h3>",
        f"<p>{html.escape(case['description'])}</p>",
        f"<p><strong>Maturity:</strong> {html.escape(case.get('maturity', 'implemented'))}</p>",
        f"<p><strong>Fairness note:</strong> {html.escape(case.get('fairness_note', ''))}</p>",
        "<div class='grid'>",
    ]
    for lang in ("kain", "rust"):
        build = case["build"][lang]
        run = case["run"][lang]
        source = html.escape(case["source"][lang])
        parts.append(
            "<div>"
            f"<h4>{lang}</h4>"
            f"<p><code>{source}</code></p>"
            f"<p>build: {fmt_ms(build['build_ms'])} ms | min: {fmt_ms(run['min_ms'])} ms | median: {fmt_ms(run['median_ms'])} ms | mean: {fmt_ms(run['mean_ms'])} ms</p>"
            f"<p>samples: <code>{html.escape(render_samples(run['samples_ms']))}</code></p>"
            f"<pre>{html.escape(display_command(build['command']))}</pre>"
            f"{render_error(build, run)}"
            "</div>"
        )
    parts.append("</div></section>")
    return "".join(parts)


def render_error(build: dict[str, Any], run: dict[str, Any]) -> str:
    errors = []
    if build.get("error"):
        errors.append(build["error"])
    if run.get("error"):
        errors.append(run["error"])
    if not errors:
        return ""
    return f"<pre>{html.escape(chr(10).join(errors))}</pre>"


def write_reports(report: dict[str, Any]) -> Path:
    REPORT_ROOT.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    html_path = REPORT_ROOT / f"{stamp}.html"
    latest_html = REPORT_ROOT / "latest.html"
    latest_json = REPORT_ROOT / "latest.json"
    html_text = render_html(report)
    html_path.write_text(html_text, encoding="utf-8")
    latest_html.write_text(html_text, encoding="utf-8")
    latest_json.write_text(json.dumps(report, indent=2), encoding="utf-8")
    return latest_html


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", default=str(BENCHMARK_ROOT / "benchmarks.json"))
    parser.add_argument("--case", dest="only_case")
    parser.add_argument("--runs", type=int)
    parser.add_argument("--warmups", type=int)
    parser.add_argument("--timeout", type=int, default=180)
    parser.add_argument("--kain-exe")
    parser.add_argument("--rustc", default=os.environ.get("RUSTC", "rustc"))
    parser.add_argument("--no-build", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    manifest = load_manifest(Path(args.manifest))
    warmups = args.warmups if args.warmups is not None else int(manifest.get("default_warmups", 2))
    runs = args.runs if args.runs is not None else int(manifest.get("default_runs", 7))

    report: dict[str, Any] = {
        "suite": manifest.get("suite", "kain-vs-rust-llvm"),
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "platform": sys.platform,
        "warmups": warmups,
        "runs": runs,
        "cases": [],
        "ok": False,
        "toolchain": {},
    }

    try:
        kain_exe = resolve_kain_exe(args.kain_exe, args.timeout)
        rustc_path = shutil.which(args.rustc) or args.rustc
        report["toolchain"] = {
            "kain_exe": str(kain_exe),
            "rustc": rustc_path,
        }
        for case in selected_cases(manifest, args.only_case):
            print(f"[bench] {case['id']}")
            result = benchmark_case(
                case=case,
                kain_exe=kain_exe,
                rustc=rustc_path,
                warmups=warmups,
                runs=runs,
                timeout=args.timeout,
                no_build=args.no_build,
            )
            report["cases"].append(result)
        report["ok"] = all(
            case["run"]["kain"]["ok"] and case["run"]["rust"]["ok"]
            for case in report["cases"]
        )
    except Exception as exc:
        report["fatal_error"] = str(exc)
        report["ok"] = False
        print(f"[bench] fatal: {exc}", file=sys.stderr)
    finally:
        latest = write_reports(report)
        print(f"[bench] report: {latest}")

    return 0 if report["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
