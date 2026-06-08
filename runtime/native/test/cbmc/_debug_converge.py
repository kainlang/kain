"""Debug script: trace pipeline WSL CBMC execution for converge harness."""
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent.parent / "scripts"
sys.path.insert(0, str(SCRIPT_DIR))
from _common import RUNTIME_DIR, INCLUDE_DIR

harness_path = RUNTIME_DIR / "test" / "cbmc" / "check_converge.c"
source_path = RUNTIME_DIR / "src" / "core" / "converge.c"
mod_name = "check_converge"
unwind = 5

repo_path = RUNTIME_DIR
wsl_repo = f"/mnt/{repo_path.drive.lower()}{repo_path.as_posix()[2:]}".replace(":", "")
print(f"wsl_repo: {wsl_repo}")

# Concatenate source + harness into combined file
combined = harness_path.parent / f"combined_{mod_name}.c"
combined_code = source_path.read_text(encoding="utf-8") + "\n" + harness_path.read_text(encoding="utf-8")
combined.write_text(combined_code, encoding="utf-8")
print(f"Wrote combined file: {combined} ({len(combined_code)} bytes)")


def to_wsl(p):
    p = str(p)
    if ":" in p:
        drive = p[0].lower()
        rest = p[2:].replace("\\", "/")
        return f"/mnt/{drive}{rest}"
    return p


combined_wsl = to_wsl(combined)
print(f"combined_wsl: {combined_wsl}")

cmd = f"cd {wsl_repo} && cbmc --unwind {unwind} --no-unwinding-assertions --trace {combined_wsl} -I include -I src/core"
print(f"\nWSL command:\n  wsl -d Ubuntu bash -c '{cmd}'")
print("-" * 70)

try:
    proc = subprocess.run(
        ["wsl", "-d", "Ubuntu", "bash", "-c", cmd],
        capture_output=True, timeout=180,
    )
    stdout = proc.stdout.decode("utf-8", errors="replace")
    stderr = proc.stderr.decode("utf-8", errors="replace")
    print(f"Return code: {proc.returncode}")
    print(f"\nSTDOUT ({len(stdout)} bytes):")
    print(stdout[:3000])
    if len(stdout) > 3000:
        print(f"... (truncated, full size: {len(stdout)} bytes)")
    print(f"\nSTDERR ({len(stderr)} bytes):")
    print(stderr[:2000])
    if len(stderr) > 2000:
        print(f"... (truncated)")

    if "VERIFICATION SUCCESSFUL" in stdout:
        print("\n*** VERIFICATION SUCCESSFUL! ***")
        # Count assertions
        import re
        successes = len(re.findall(r'SUCCESS', stdout))
        failures = len(re.findall(r'FAILURE', stdout))
        print(f"  {successes} SUCCESS, {failures} FAILURE")
    elif "VERIFICATION FAILED" in stdout:
        print("\n*** VERIFICATION FAILED ***")
    else:
        print(f"\n*** UNKNOWN STATUS (exit code {proc.returncode}) ***")

except FileNotFoundError:
    print("ERROR: WSL executable not found!")
except subprocess.TimeoutExpired:
    print("ERROR: TIMEOUT after 180s")
except Exception as e:
    print(f"ERROR: {e}")

print("-" * 70)
print("Done.")
