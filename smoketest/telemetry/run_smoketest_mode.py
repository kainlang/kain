import argparse
import os
import subprocess
import sys
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description="Run the smoketest executable in a specific telemetry mode.")
    parser.add_argument("--mode", required=True, choices=["full", "benchmark", "attrition"])
    parser.add_argument("--executable", required=True)
    parser.add_argument("--output-dir", required=True)
    parser.add_argument("--bench-rounds", type=int, default=0)
    parser.add_argument("--bench-passes", type=int, default=0)
    parser.add_argument("--attrition-ops", type=int, default=0)
    parser.add_argument("--attrition-rounds", type=int, default=0)
    args = parser.parse_args()

    cwd = Path.cwd()
    executable = (cwd / args.executable).resolve()
    output_dir = (cwd / args.output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    env = os.environ.copy()
    env["KAIN_SMOKETEST_MODE"] = args.mode
    env["KAIN_SMOKETEST_OUTPUT_DIR"] = str(output_dir)
    if args.bench_rounds > 0:
        env["KAIN_SMOKETEST_BENCH_ROUNDS"] = str(args.bench_rounds)
    if args.bench_passes > 0:
        env["KAIN_SMOKETEST_BENCH_PASSES"] = str(args.bench_passes)
    if args.attrition_ops > 0:
        env["KAIN_SMOKETEST_ATTRITION_OPS"] = str(args.attrition_ops)
    if args.attrition_rounds > 0:
        env["KAIN_SMOKETEST_ATTRITION_ROUNDS"] = str(args.attrition_rounds)

    result = subprocess.run(
        [str(executable)],
        cwd=str(cwd),
        env=env,
        text=True,
        capture_output=True,
    )
    if result.stdout:
        sys.stdout.write(result.stdout)
    if result.stderr:
        sys.stderr.write(result.stderr)

    summary_path = output_dir / "summary.json"
    if result.returncode != 0:
        if summary_path.exists():
            sys.stderr.write(f"smoketest {args.mode} failed; summary at {summary_path}\n")
        return result.returncode
    if not summary_path.exists():
        sys.stderr.write(f"smoketest {args.mode} succeeded but did not produce {summary_path}\n")
        return 3

    print(f"smoketest {args.mode} ok: {summary_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
