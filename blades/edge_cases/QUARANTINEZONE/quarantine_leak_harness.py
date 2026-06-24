"""
quarantine_leak_harness.py — Launch a Kain executable and monitor memory telemetry.

Uses psutil for cross-platform process memory tracking.
Captures per-second RSS, VMS, and USS with subprocess lifecycle management.
"""
import subprocess
import sys
import time
import json
import os
from pathlib import Path

try:
    import psutil
except ImportError:
    print("ERROR: psutil not installed. Run: pip install psutil")
    sys.exit(1)

EXE_PATH = Path(os.environ.get(
    "KAIN_LEAK_EXE",
    r"X:\.kain\out\x86_64-windows\dev\ll\quarantine_leak_test\compile\quarantine_leak_test.exe"
))
TIMEOUT_SECONDS = 60
POLL_INTERVAL_MS = 500
OUTPUT_LOG = Path(__file__).parent / "whatthefuckiswrongwiththisshit" / "memory_telemetry.json"

def monitor_process(proc: subprocess.Popen, timeout: float) -> list[dict]:
    """Poll process memory every POLL_INTERVAL_MS until timeout or exit."""
    samples = []
    psproc = psutil.Process(proc.pid)
    start = time.time()

    while time.time() - start < timeout:
        try:
            mem = psproc.memory_info()
            sample = {
                "elapsed_s": round(time.time() - start, 2),
                "rss_mb": round(mem.rss / (1024 * 1024), 2),
                "vms_mb": round(mem.vms / (1024 * 1024), 2),
                "pid": proc.pid,
            }
            # USS is Windows-only via memory_full_info
            try:
                full = psproc.memory_full_info()
                sample["uss_mb"] = round(full.uss / (1024 * 1024), 2)
            except Exception:
                sample["uss_mb"] = -1
            samples.append(sample)

            # Check if still alive
            if proc.poll() is not None:
                samples.append({"elapsed_s": round(time.time() - start, 2), "exited": True, "exit_code": proc.returncode})
                break

        except (psutil.NoSuchProcess, psutil.AccessDenied):
            samples.append({"elapsed_s": round(time.time() - start, 2), "exited": True, "exit_code": proc.returncode if proc.poll() is not None else -1})
            break

        time.sleep(POLL_INTERVAL_MS / 1000.0)

    # Kill if still running
    if proc.poll() is None:
        try:
            psproc.kill()
        except Exception:
            pass
        proc.wait(timeout=2)

    return samples


def main():
    if not EXE_PATH.exists():
        print(f"ERROR: exe not found at {EXE_PATH}")
        print("Build first: kain build X:/blades/edge_cases/QUARANTINEZONE/quarantine_leak_test.kn --target llvm")
        sys.exit(1)

    print(f"Launching: {EXE_PATH}")
    print(f"Timeout: {TIMEOUT_SECONDS}s | Poll: {POLL_INTERVAL_MS}ms")
    print()

    proc = subprocess.Popen(
        str(EXE_PATH),
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

    samples = monitor_process(proc, TIMEOUT_SECONDS)

    # Analysis
    if not samples:
        print("NO SAMPLES — process exited immediately")
        sys.exit(1)

    rss_values = [s["rss_mb"] for s in samples if "rss_mb" in s]
    if len(rss_values) < 2:
        print(f"Only {len(rss_values)} samples — process too short-lived ({samples[-1].get('exit_code', '?')})")
        print(json.dumps(samples, indent=2))
        sys.exit(1)

    first_rss = rss_values[0]
    last_rss = rss_values[-1]
    growth = last_rss - first_rss
    elapsed = samples[-1]["elapsed_s"]
    rate_mb_per_s = growth / elapsed if elapsed > 0 else 0

    print(f"  Start RSS:  {first_rss:>10.1f} MB")
    print(f"  End RSS:    {last_rss:>10.1f} MB")
    print(f"  Growth:     {growth:>10.1f} MB")
    print(f"  Elapsed:    {elapsed:>10.1f} s")
    print(f"  Leak rate:  {rate_mb_per_s:>10.1f} MB/s")
    print(f"  Samples:    {len(samples):>10}")
    print()

    # Leak verdict
    if growth > 500 and rate_mb_per_s > 5:
        print("*** VERDICT: MEMORY LEAK CONFIRMED ***")
        print(f"   {growth:.0f} MB leaked in {elapsed:.0f}s ({rate_mb_per_s:.0f} MB/s)")
        if rate_mb_per_s > 100:
            print(f"   Projected 60s total: ~{rate_mb_per_s * 60:.0f} MB ({rate_mb_per_s * 60 / 1024:.1f} GB)")
    elif growth < 50:
        print("PASS: Memory stable — leak fixed or absent")
    else:
        print("INCONCLUSIVE: Moderate growth, needs longer run")

    # Save raw data
    OUTPUT_LOG.parent.mkdir(parents=True, exist_ok=True)
    report = {
        "exe": str(EXE_PATH),
        "first_rss_mb": first_rss,
        "last_rss_mb": last_rss,
        "growth_mb": growth,
        "elapsed_s": elapsed,
        "leak_rate_mb_per_s": round(rate_mb_per_s, 2),
        "sample_count": len(samples),
        "verdict": "LEAK" if growth > 500 else "STABLE" if growth < 50 else "INCONCLUSIVE",
        "samples": samples,
    }
    with open(OUTPUT_LOG, "w") as f:
        json.dump(report, f, indent=2)
    print(f"\nRaw data: {OUTPUT_LOG}")


if __name__ == "__main__":
    main()
