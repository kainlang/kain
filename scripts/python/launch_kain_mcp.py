#!/usr/bin/env python3

from __future__ import annotations

import hashlib
import json
import os
import shutil
import subprocess
import sys
import threading
import time
from datetime import datetime, timezone
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
BANNER_PREFIX = b" KAIN Compiler v"


def read_json_file(path: Path) -> dict:
    try:
        raw = path.read_text(encoding="utf-8")
    except OSError:
        return {}
    try:
        value = json.loads(raw)
    except json.JSONDecodeError:
        return {}
    if isinstance(value, dict):
        return value
    return {}


def write_json_atomic(path: Path, payload: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temp_path = path.with_suffix(path.suffix + f".tmp-{os.getpid()}")
    temp_path.write_text(json.dumps(payload, indent=2, sort_keys=True), encoding="utf-8")
    os.replace(temp_path, path)


def resolve_repo_root() -> Path:
    configured = os.environ.get("KAIN_REPO_ROOT", "").strip()
    if configured:
        return Path(configured).resolve()
    return REPO_ROOT


def resolve_blade_root(repo_root: Path) -> Path:
    return repo_root / "blades" / "kain-mcp"


def load_runtime_policy(blade_root: Path) -> dict:
    return read_json_file(blade_root / "config" / "runtime_policy.json")


def policy_env(policy: dict) -> dict:
    value = policy.get("environment", {})
    if isinstance(value, dict):
        return value
    return {}


def policy_sync(policy: dict) -> dict:
    value = policy.get("launcher_sync", {})
    if isinstance(value, dict):
        return value
    return {}


def resolve_kain_binary_candidates(policy: dict) -> list[str]:
    raw = policy.get("kain_binary_candidates", [])
    if isinstance(raw, list):
        candidates = [str(item) for item in raw if isinstance(item, str)]
        if candidates:
            return candidates
    return [
        "target/debug/kain.exe",
        "target/debug/kain",
        "target/release/kain.exe",
        "target/release/kain",
        "PATH:kain",
    ]


def resolve_sync_state_root(sync_policy: dict) -> Path:
    env_key = str(sync_policy.get("state_root_env_key", "KAIN_SYNC_ROOT"))
    override = os.environ.get(env_key, "").strip()
    if override:
        return Path(os.path.expandvars(os.path.expanduser(override))).resolve()

    default_value = "%USERPROFILE%/.kain" if os.name == "nt" else "~/.kain"
    key = "default_state_root_windows" if os.name == "nt" else "default_state_root_unix"
    configured = str(sync_policy.get(key, default_value))
    expanded = os.path.expandvars(os.path.expanduser(configured))
    return Path(expanded).resolve()


def resolve_sync_paths(sync_policy: dict) -> tuple[Path, Path, Path]:
    state_root = resolve_sync_state_root(sync_policy)
    lock_relative = str(sync_policy.get("lock_relative_path", "locks/sync.lock"))
    stamp_relative = str(sync_policy.get("stamp_relative_path", "state/kain_sync_stamp.json"))
    counter_relative = str(sync_policy.get("build_counter_relative_path", "state/build_counter.json"))
    lock_path = (state_root / lock_relative).resolve()
    stamp_path = (state_root / stamp_relative).resolve()
    counter_path = (state_root / counter_relative).resolve()
    return lock_path, stamp_path, counter_path


def resolve_launch_trace_path(sync_policy: dict) -> Path:
    state_root = resolve_sync_state_root(sync_policy)
    relative = str(sync_policy.get("launch_trace_relative_path", "state/kain_mcp_launcher_trace.jsonl"))
    return (state_root / relative).resolve()


def append_launch_trace(sync_policy: dict, event: str, **fields) -> None:
    if not bool(sync_policy.get("launch_trace_enabled", False)):
        return
    trace_path = resolve_launch_trace_path(sync_policy)
    trace_path.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "schema_version": 1,
        "event": event,
        "pid": os.getpid(),
        "unix": int(time.time()),
        "utc": datetime.now(timezone.utc).isoformat(),
    }
    payload.update(fields)
    line = json.dumps(payload, sort_keys=True)
    try:
        with trace_path.open("a", encoding="utf-8") as handle:
            handle.write(line)
            handle.write("\n")
    except OSError:
        return


def resolve_path_candidate(repo_root: Path, candidate: str) -> str:
    normalized = candidate.strip()
    if not normalized:
        return ""
    if normalized.startswith("PATH:"):
        command_name = normalized[len("PATH:") :].strip()
        if not command_name:
            return ""
        resolved = shutil.which(command_name)
        return resolved or command_name
    candidate_path = (repo_root / normalized).resolve()
    if candidate_path.exists():
        return str(candidate_path)
    return ""


def resolve_synced_binary_from_stamp(sync_stamp: dict) -> str:
    binary = sync_stamp.get("binary", {})
    if not isinstance(binary, dict):
        return ""
    raw_path = binary.get("path")
    if not isinstance(raw_path, str) or not raw_path.strip():
        return ""
    candidate = Path(raw_path.strip())
    if candidate.exists():
        return str(candidate)
    return ""


def normalize_candidate_for_compare(candidate: str) -> str:
    try:
        path = Path(candidate)
    except OSError:
        return candidate.strip().lower()
    if path.exists():
        return str(path.resolve()).lower()
    return candidate.strip().lower()


def resolve_kain_binary(
    repo_root: Path,
    policy: dict,
    sync_stamp: dict | None = None,
    *,
    allow_synced_binary: bool = True,
    excluded_candidates: set[str] | None = None,
) -> str:
    environment = policy_env(policy)
    override_key = str(environment.get("kain_bin_override_key", "KAIN_MCP_KAIN_BIN"))
    override = os.environ.get(override_key, "").strip()
    if override:
        return override

    sync_policy = policy_sync(policy)
    excluded = excluded_candidates or set()
    if allow_synced_binary and bool(sync_policy.get("prefer_synced_binary", False)) and sync_stamp:
        synced_binary = resolve_synced_binary_from_stamp(sync_stamp)
        if synced_binary and normalize_candidate_for_compare(synced_binary) not in excluded:
            return synced_binary

    for candidate in resolve_kain_binary_candidates(policy):
        resolved = resolve_path_candidate(repo_root, candidate)
        if resolved and normalize_candidate_for_compare(resolved) not in excluded:
            return resolved

    fallback = shutil.which("kain")
    if fallback and normalize_candidate_for_compare(fallback) not in excluded:
        return fallback

    raise SystemExit(
        "Could not resolve a Kain binary. Set KAIN_MCP_KAIN_BIN or build the repo-local cli first."
    )


def resolve_relative_command_tokens(repo_root: Path, tokens: list[str]) -> list[str]:
    resolved: list[str] = []
    for token in tokens:
        if not token:
            continue
        candidate = Path(token)
        if candidate.is_absolute():
            resolved.append(token)
            continue
        if "/" in token or "\\" in token or token.startswith("."):
            repo_candidate = (repo_root / token).resolve()
            if repo_candidate.exists():
                resolved.append(str(repo_candidate))
                continue
        resolved.append(token)
    return resolved


def resolve_sync_command(repo_root: Path, sync_policy: dict) -> list[str]:
    key = "sync_command_windows" if os.name == "nt" else "sync_command_unix"
    raw = sync_policy.get(key, [])
    if isinstance(raw, list):
        tokens = [str(item) for item in raw if isinstance(item, str) and item.strip()]
        if tokens:
            return resolve_relative_command_tokens(repo_root, tokens)

    if os.name == "nt":
        return [
            "powershell",
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            str((repo_root / "scripts/windows/sync-kain-source-of-truth.ps1").resolve()),
            "-ManagedSync",
        ]
    return ["python3", str((repo_root / "install_kain.py").resolve())]


def compute_repo_sha(repo_root: Path) -> str:
    try:
        result = subprocess.run(
            ["git", "rev-parse", "HEAD"],
            cwd=str(repo_root),
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
        )
    except (OSError, subprocess.CalledProcessError):
        return "unknown"
    return result.stdout.strip() or "unknown"


def compute_runtime_stamp(repo_root: Path, sync_policy: dict) -> str:
    runtime_files = sync_policy.get("runtime_stamp_files", [])
    if not isinstance(runtime_files, list):
        runtime_files = []
    normalized_files = [str(item) for item in runtime_files if isinstance(item, str) and item.strip()]
    if not normalized_files:
        normalized_files = [
            "runtime/kain_runtime.c",
            "runtime/native_runtime.toml",
            "blades/kain-mcp/config/runtime_policy.json",
        ]

    lines: list[str] = []
    for relative in normalized_files:
        rel = relative.replace("\\", "/")
        candidate = (repo_root / relative).resolve()
        if candidate.exists():
            stat = candidate.stat()
            mtime_unix = int(stat.st_mtime)
            lines.append(f"{rel}|1|{stat.st_size}|{mtime_unix}")
        else:
            lines.append(f"{rel}|0||")
    digest = hashlib.sha256("\n".join(lines).encode("utf-8")).hexdigest()
    return digest[:20]


def binary_fingerprint(path_text: str) -> dict:
    if not path_text:
        return {}
    path = Path(path_text)
    if not path.exists():
        return {"path": path_text, "exists": False}
    stat = path.stat()
    return {
        "path": str(path.resolve()),
        "exists": True,
        "size_bytes": int(stat.st_size),
        "mtime_unix": int(stat.st_mtime),
    }


def sync_state_from_local(repo_root: Path, sync_policy: dict, binary_path: str) -> dict:
    return {
        "repo_sha": compute_repo_sha(repo_root),
        "runtime_stamp": compute_runtime_stamp(repo_root, sync_policy),
        "binary": binary_fingerprint(binary_path),
        "checked_at_unix": int(time.time()),
    }


def sync_state_stale_reasons(local_state: dict, sync_stamp: dict) -> list[str]:
    reasons: list[str] = []
    if not sync_stamp:
        return ["missing sync stamp"]

    stamp_repo_sha = str(sync_stamp.get("repo_sha", ""))
    if stamp_repo_sha != str(local_state.get("repo_sha", "")):
        reasons.append("repo sha changed")

    stamp_runtime_stamp = str(sync_stamp.get("runtime_stamp", ""))
    if stamp_runtime_stamp != str(local_state.get("runtime_stamp", "")):
        reasons.append("runtime stamp changed")

    local_binary = local_state.get("binary", {})
    if not isinstance(local_binary, dict):
        local_binary = {}
    stamp_binary = sync_stamp.get("binary", {})
    if not isinstance(stamp_binary, dict):
        stamp_binary = {}

    if str(stamp_binary.get("path", "")) != str(local_binary.get("path", "")):
        reasons.append("binary path changed")
    if int(stamp_binary.get("size_bytes", -1)) != int(local_binary.get("size_bytes", -1)):
        reasons.append("binary size changed")
    if int(stamp_binary.get("mtime_unix", -1)) != int(local_binary.get("mtime_unix", -1)):
        reasons.append("binary mtime changed")

    return reasons


def read_sync_stamp(stamp_path: Path) -> dict:
    return read_json_file(stamp_path)


def upsert_sync_attempt(stamp_path: Path, existing: dict) -> dict:
    next_stamp = dict(existing) if isinstance(existing, dict) else {}
    next_stamp["schema_version"] = 1
    next_stamp["last_attempt_unix"] = int(time.time())
    write_json_atomic(stamp_path, next_stamp)
    return next_stamp


def try_acquire_sync_lock(lock_path: Path, stale_lock_seconds: int) -> bool:
    lock_path.parent.mkdir(parents=True, exist_ok=True)
    now = int(time.time())
    if lock_path.exists():
        try:
            age = now - int(lock_path.stat().st_mtime)
        except OSError:
            age = 0
        if stale_lock_seconds > 0 and age > stale_lock_seconds:
            try:
                lock_path.unlink()
            except OSError:
                return False
        else:
            return False

    payload = {
        "pid": os.getpid(),
        "created_unix": now,
    }
    try:
        descriptor = os.open(str(lock_path), os.O_CREAT | os.O_EXCL | os.O_WRONLY)
        try:
            os.write(descriptor, json.dumps(payload).encode("utf-8"))
        finally:
            os.close(descriptor)
        return True
    except FileExistsError:
        return False
    except OSError:
        return False


def release_sync_lock(lock_path: Path) -> None:
    try:
        lock_path.unlink()
    except OSError:
        pass


def preflight_binary_for_blade(repo_root: Path, blade_root: Path, policy: dict, kain_bin: str) -> bool:
    sync_policy = policy_sync(policy)
    if not bool(sync_policy.get("preflight_enabled", True)):
        return True

    timeout_seconds = int(sync_policy.get("preflight_timeout_seconds", 20))
    command = [kain_bin, "run", "plan", "--json", str(blade_root)]
    try:
        result = subprocess.run(
            command,
            cwd=str(repo_root),
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=max(timeout_seconds, 1),
        )
    except (OSError, subprocess.TimeoutExpired):
        return False
    return result.returncode == 0


def maybe_run_managed_sync(repo_root: Path, policy: dict, initial_sync_stamp: dict) -> dict:
    sync_policy = policy_sync(policy)
    if not bool(sync_policy.get("enabled", False)):
        return initial_sync_stamp

    lock_path, stamp_path, _counter_path = resolve_sync_paths(sync_policy)
    sync_stamp = initial_sync_stamp if isinstance(initial_sync_stamp, dict) else {}

    selected_binary = resolve_kain_binary(repo_root, policy, sync_stamp)
    local_state = sync_state_from_local(repo_root, sync_policy, selected_binary)
    stale_reasons = sync_state_stale_reasons(local_state, sync_stamp)
    if not stale_reasons:
        append_launch_trace(
            sync_policy,
            "managed_sync_not_needed",
            binary=selected_binary,
            repo_sha=str(local_state.get("repo_sha", "")),
        )
        return sync_stamp

    print(
        "[kain-mcp] auto-sync required before startup: " + ", ".join(stale_reasons) + ".",
        file=sys.stderr,
    )
    append_launch_trace(
        sync_policy,
        "managed_sync_required",
        binary=selected_binary,
        repo_sha=str(local_state.get("repo_sha", "")),
        stale_reasons=stale_reasons,
    )

    cooldown_seconds = int(sync_policy.get("cooldown_seconds", 45))
    last_attempt = int(sync_stamp.get("last_attempt_unix", 0)) if sync_stamp else 0
    now = int(time.time())
    if cooldown_seconds > 0 and last_attempt > 0 and (now - last_attempt) < cooldown_seconds:
        print(
            "[kain-mcp] auto-sync skipped (cooldown active); using current binary.",
            file=sys.stderr,
        )
        append_launch_trace(
            sync_policy,
            "managed_sync_skipped_cooldown",
            cooldown_seconds=cooldown_seconds,
            last_attempt_unix=last_attempt,
        )
        return sync_stamp

    stale_lock_seconds = int(sync_policy.get("stale_lock_seconds", 900))
    if not try_acquire_sync_lock(lock_path, stale_lock_seconds):
        print(
            "[kain-mcp] auto-sync skipped (sync lock is active); using current binary.",
            file=sys.stderr,
        )
        append_launch_trace(
            sync_policy,
            "managed_sync_skipped_lock",
            lock_path=str(lock_path),
            stale_lock_seconds=stale_lock_seconds,
        )
        return sync_stamp

    try:
        sync_stamp = upsert_sync_attempt(stamp_path, sync_stamp)
        command = resolve_sync_command(repo_root, sync_policy)
        env = os.environ.copy()
        env["KAIN_REPO_ROOT"] = str(repo_root)
        env["KAIN_SYNC_ROOT"] = str(resolve_sync_state_root(sync_policy))
        env["KAIN_SYNC_STAMP_PATH"] = str(stamp_path)
        env["KAIN_SYNC_LOCK_PATH"] = str(lock_path)
        env["KAIN_SYNC_REPO_SHA"] = str(local_state.get("repo_sha", "unknown"))
        env["KAIN_SYNC_RUNTIME_STAMP"] = str(local_state.get("runtime_stamp", "unknown"))
        print("[kain-mcp] running managed sync before MCP startup.", file=sys.stderr)
        append_launch_trace(
            sync_policy,
            "managed_sync_start",
            command=command,
            repo_sha=str(local_state.get("repo_sha", "")),
        )
        result = subprocess.run(
            command,
            cwd=str(repo_root),
            env=env,
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        if result.stdout:
            print(result.stdout, end="", file=sys.stderr)
        if result.stderr:
            print(result.stderr, end="", file=sys.stderr)
        if result.returncode != 0:
            print(
                f"[kain-mcp] auto-sync command failed (exit {result.returncode}); using current binary.",
                file=sys.stderr,
            )
            append_launch_trace(
                sync_policy,
                "managed_sync_failed",
                returncode=result.returncode,
            )
            return sync_stamp

        refreshed = read_sync_stamp(stamp_path)
        append_launch_trace(
            sync_policy,
            "managed_sync_completed",
            returncode=result.returncode,
            synced_repo_sha=str(refreshed.get("repo_sha", "")) if isinstance(refreshed, dict) else "",
        )
        if refreshed:
            return refreshed
        return sync_stamp
    finally:
        release_sync_lock(lock_path)


def iter_python_runtime_dirs() -> list[Path]:
    discovered: list[Path] = []
    configured = os.environ.get("KAIN_MCP_PYTHON_DIRS", "").strip()
    if configured:
        for item in configured.split(os.pathsep):
            trimmed = item.strip()
            if trimmed:
                discovered.append(Path(trimmed))

    discovered.append(Path(sys.executable).resolve().parent)

    local_app_data = os.environ.get("LOCALAPPDATA", "").strip()
    if local_app_data:
        python_root = Path(local_app_data) / "Programs" / "Python"
        for child in python_root.glob("Python3*"):
            discovered.append(child)

    ordered: list[Path] = []
    seen: set[str] = set()
    for path in discovered:
        candidate = path.resolve()
        key = str(candidate).lower()
        if key in seen:
            continue
        if not candidate.exists():
            continue
        seen.add(key)
        ordered.append(candidate)
    return ordered


def build_launch_path() -> str:
    existing = os.environ.get("PATH", "")
    prefixes = [str(path) for path in iter_python_runtime_dirs()]
    if not prefixes:
        return existing
    if existing:
        prefixes.append(existing)
    return os.pathsep.join(prefixes)


def forward_stdin(child_stdin) -> None:
    stdin_fd = sys.stdin.fileno()
    try:
        while True:
            chunk = os.read(stdin_fd, 65536)
            if not chunk:
                break
            child_stdin.write(chunk)
            child_stdin.flush()
    finally:
        try:
            child_stdin.close()
        except OSError:
            pass


def forward_stderr(child_stderr) -> None:
    child_fd = child_stderr.fileno()
    stderr_fd = sys.stderr.fileno()
    try:
        while True:
            chunk = os.read(child_fd, 65536)
            if not chunk:
                break
            os.write(stderr_fd, chunk)
    finally:
        try:
            child_stderr.close()
        except OSError:
            pass


def forward_stdout(child_stdout) -> None:
    child_fd = child_stdout.fileno()
    stdout_fd = sys.stdout.fileno()
    first_line_buffer = bytearray()
    first_line_pending = True

    try:
        while True:
            chunk = os.read(child_fd, 65536)
            if not chunk:
                break

            if first_line_pending:
                first_line_buffer.extend(chunk)

                if first_line_buffer.startswith(b"Content-Length:"):
                    os.write(stdout_fd, bytes(first_line_buffer))
                    first_line_buffer.clear()
                    first_line_pending = False
                    continue

                newline_index = first_line_buffer.find(b"\n")
                if newline_index != -1:
                    first_line = bytes(first_line_buffer[: newline_index + 1])
                    remainder = bytes(first_line_buffer[newline_index + 1 :])
                    if not first_line.startswith(BANNER_PREFIX):
                        os.write(stdout_fd, first_line)
                    if remainder:
                        os.write(stdout_fd, remainder)
                    first_line_buffer.clear()
                    first_line_pending = False
                    continue

                if len(first_line_buffer) > 2048:
                    os.write(stdout_fd, bytes(first_line_buffer))
                    first_line_buffer.clear()
                    first_line_pending = False
                    continue

                continue

            os.write(stdout_fd, chunk)

        if first_line_pending and first_line_buffer:
            os.write(stdout_fd, bytes(first_line_buffer))
    finally:
        try:
            child_stdout.close()
        except OSError:
            pass


def main() -> int:
    repo_root = resolve_repo_root()
    blade_root = resolve_blade_root(repo_root)
    policy = load_runtime_policy(blade_root)

    sync_policy = policy_sync(policy)
    append_launch_trace(
        sync_policy,
        "launcher_start",
        argv=sys.argv,
        cwd=os.getcwd(),
        repo_root=str(repo_root),
    )
    lock_path, stamp_path, _counter_path = resolve_sync_paths(sync_policy)
    sync_stamp = read_sync_stamp(stamp_path)
    if bool(sync_policy.get("enabled", False)):
        sync_stamp = maybe_run_managed_sync(repo_root, policy, sync_stamp)

    kain_bin = resolve_kain_binary(repo_root, policy, sync_stamp)
    synced_binary = resolve_synced_binary_from_stamp(sync_stamp) if sync_stamp else ""
    if synced_binary and normalize_candidate_for_compare(synced_binary) == normalize_candidate_for_compare(kain_bin):
        if not preflight_binary_for_blade(repo_root, blade_root, policy, kain_bin):
            print(
                "[kain-mcp] managed synced binary failed preflight; falling back to repo/PATH resolution.",
                file=sys.stderr,
            )
            append_launch_trace(
                sync_policy,
                "preflight_failed",
                binary=kain_bin,
            )
            excluded = {normalize_candidate_for_compare(synced_binary)}
            kain_bin = resolve_kain_binary(
                repo_root,
                policy,
                sync_stamp,
                allow_synced_binary=False,
                excluded_candidates=excluded,
            )

    env = os.environ.copy()
    env["KAIN_REPO_ROOT"] = str(repo_root)
    env["KAIN_MCP_BLADE_ROOT"] = str(blade_root)
    env["KAIN_SYNC_LOCK_PATH"] = str(lock_path)
    env["KAIN_SYNC_STAMP_PATH"] = str(stamp_path)
    env["PATH"] = build_launch_path()

    command = [kain_bin, "run", str(blade_root)]
    if len(sys.argv) > 1:
        command.extend(["--", *sys.argv[1:]])
    append_launch_trace(
        sync_policy,
        "spawn_child",
        command=command,
        kain_bin=kain_bin,
        blade_root=str(blade_root),
    )

    child = subprocess.Popen(
        command,
        cwd=str(repo_root),
        env=env,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )

    stdin_thread = threading.Thread(target=forward_stdin, args=(child.stdin,), daemon=True)
    stdout_thread = threading.Thread(target=forward_stdout, args=(child.stdout,), daemon=True)
    stderr_thread = threading.Thread(target=forward_stderr, args=(child.stderr,), daemon=True)

    stdin_thread.start()
    stdout_thread.start()
    stderr_thread.start()

    exit_code = child.wait()
    append_launch_trace(
        sync_policy,
        "child_exit",
        exit_code=exit_code,
    )
    stdout_thread.join()
    stderr_thread.join()
    stdin_thread.join(timeout=0.1)
    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
