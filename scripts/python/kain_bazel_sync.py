#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import shutil
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Sequence

PYTHON_POLLUTION_ENV_KEYS = (
    "PYO3_PYTHON",
    "PYTHONHOME",
    "PYTHONPATH",
    "VIRTUAL_ENV",
    "CONDA_PREFIX",
    "CONDA_DEFAULT_ENV",
    "PYTHONEXECUTABLE",
    "__PYVENV_LAUNCHER__",
)

DEFAULT_BINARY_NAMES = ("kain",)
SUPPORTED_BINARY_NAMES = frozenset(DEFAULT_BINARY_NAMES)
MAX_SYNC_STAMP_ATTEMPTS = 3
DEFAULT_SOURCE_WATCH_PATHS = (
    "crates",
    "runtime",
    "src",
    "tools/bazel",
    "toolchain/rules_rust",
    "Cargo.toml",
    "Cargo.lock",
    "Cargo.Bazel.lock",
    "BUILD.bazel",
    "MODULE.bazel",
    "MODULE.bazel.lock",
    ".bazelrc",
    ".bazelversion",
    ".bazeliskrc",
)
DEFAULT_RUNTIME_STAMP_FILES = (
    "runtime/runtime.c",
    "runtime/native_core_runtime.toml",
    "blades/kain-mcp/config/runtime_policy.json",
)
DEFAULT_BAZEL_STORAGE_LIMIT_GIB = 150
DEFAULT_BAZEL_STORAGE_LIMIT_BYTES = DEFAULT_BAZEL_STORAGE_LIMIT_GIB * 1024 * 1024 * 1024


class SyncError(RuntimeError):
    pass


@dataclass(frozen=True)
class CommandResult:
    exit_code: int
    output_lines: tuple[str, ...]

    @property
    def output_text(self) -> str:
        return "\n".join(self.output_lines)


@dataclass(frozen=True)
class BuildDecision:
    should_build: bool
    reason: str


@dataclass(frozen=True)
class SyncContext:
    repo_root: Path
    policy: dict[str, object]
    sync_policy: dict[str, object]
    state_root: Path
    stamp_path: Path
    bazel_config: str
    source_watch_paths: tuple[str, ...]
    source_filesystem_watch_paths: tuple[str, ...]
    runtime_stamp_files: tuple[str, ...]
    launcher_dir: Path
    binary_names: tuple[str, ...]
    repo_kain_home: Path
    repo_kain_config: Path
    clang_path: Path | None
    python_path: Path | None


@dataclass(frozen=True)
class BazelStorageEntry:
    path: Path
    size_bytes: int
    mtime_unix: float
    root_name: str


@dataclass(frozen=True)
class BazelStorageRoots:
    output_user_root: Path | None
    disk_cache: Path | None
    repository_cache: Path | None


def short_sha256(text: str, width: int = 20) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()[:width]


def normalize_relative_path(path_text: str) -> str:
    normalized = path_text.strip().replace("\\", "/")
    while normalized.startswith("./"):
        normalized = normalized[2:]
    while normalized.startswith("/"):
        normalized = normalized[1:]
    return normalized


def path_matches_watch_path(candidate_path: str, watch_paths: Sequence[str]) -> bool:
    candidate = normalize_relative_path(candidate_path).lower()
    for watch_path in watch_paths:
        watch = normalize_relative_path(watch_path).lower()
        if candidate == watch or candidate.startswith(watch + "/"):
            return True
    return False


def as_string_list(value: object, default: Sequence[str] = ()) -> tuple[str, ...]:
    if value is None:
        return tuple(default)
    if isinstance(value, str):
        return (value,) if value.strip() else tuple(default)
    if isinstance(value, Sequence):
        result = tuple(str(item) for item in value if str(item).strip())
        return result if result else tuple(default)
    return tuple(default)


def sanitize_binary_names(values: Sequence[str]) -> tuple[str, ...]:
    filtered = tuple(name for name in values if name in SUPPORTED_BINARY_NAMES)
    return filtered if filtered else DEFAULT_BINARY_NAMES


def get_host_triple() -> str:
    """Detect the host platform target triple string.

    Mirrors `TargetTriple::host()` from ``crates/target-triple/src/triple.rs``.
    Returns e.g. ``"x86_64-pc-windows-msvc"``, ``"x86_64-unknown-linux-gnu"``,
    or ``"arm64-apple-darwin"``.
    """
    machine = platform.machine().lower()
    system = platform.system().lower()

    if machine in ("x86_64", "amd64", "x64"):
        arch = "x86_64"
    elif machine in ("aarch64", "arm64"):
        arch = "aarch64"
    else:
        arch = "unknown"

    if system == "windows":
        return f"{arch}-pc-windows-msvc"
    elif system == "linux":
        return f"{arch}-unknown-linux-gnu"
    elif system == "darwin":
        return f"{arch}-apple-darwin"
    else:
        return f"{arch}-unknown-unknown"


def get_runtime_lib_name(target_triple: str) -> str:
    """Return the native runtime library filename for a target triple.

    Mirrors ``TargetTriple::runtime_lib_name()`` from
    ``crates/target-triple/src/triple.rs``.
    Windows targets produce ``"kain_runtime.lib"`` (COFF static library);
    all other targets produce ``"libkain_runtime.a"``.
    """
    if "windows" in target_triple.lower():
        return "kain_runtime.lib"
    return "libkain_runtime.a"


def bool_from_policy(value: object, default: bool = False) -> bool:
    if isinstance(value, bool):
        return value
    if isinstance(value, str):
        lowered = value.strip().lower()
        if lowered in {"1", "true", "yes", "on"}:
            return True
        if lowered in {"0", "false", "no", "off"}:
            return False
    return default


def read_json(path: Path) -> dict[str, object]:
    if not path.exists():
        return {}
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return {}
    return payload if isinstance(payload, dict) else {}


def write_json_atomic(path: Path, payload: dict[str, object]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_name(f"{path.name}.tmp.{os.getpid()}")
    tmp.write_text(json.dumps(payload, indent=2, sort_keys=True), encoding="utf-8")
    os.replace(tmp, path)


def write_text_atomic(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_name(f"{path.name}.tmp.{os.getpid()}")
    tmp.write_text(text, encoding="utf-8", newline="\n")
    os.replace(tmp, path)


def copy_file_atomic(source: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    tmp = destination.with_name(f"{destination.name}.tmp.{os.getpid()}")
    shutil.copy2(source, tmp)
    os.replace(tmp, destination)


def files_equal(left: Path, right: Path) -> bool:
    if not left.exists() or not right.exists():
        return False
    left_stat = left.stat()
    right_stat = right.stat()
    if left_stat.st_size != right_stat.st_size:
        return False
    return file_content_hash(left) == file_content_hash(right)


def copy_file_atomic_if_unlocked(source: Path, destination: Path) -> str | None:
    tmp = destination.with_name(f"{destination.name}.tmp.{os.getpid()}")
    try:
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, tmp)
        os.replace(tmp, destination)
        cleanup_launcher_replacements(destination)
        return None
    except PermissionError as error:
        try:
            tmp.unlink(missing_ok=True)
        except OSError:
            pass
        if files_equal(source, destination):
            return None
        cleanup_launcher_replacements(destination)
        pending = destination.with_name(f"{destination.name}.pending.{os.getpid()}")
        shutil.copy2(source, pending)
        return f"{destination} is locked; staged replacement at {pending}: {error}"


def cleanup_launcher_replacements(destination: Path) -> None:
    for pattern in (f"{destination.name}.pending.*", f"{destination.name}.tmp.*"):
        for candidate in destination.parent.glob(pattern):
            try:
                candidate.unlink()
            except OSError:
                pass


def resolve_repo_root(explicit: str | None = None) -> Path:
    candidates: list[Path] = []
    if explicit:
        candidates.append(Path(explicit))
    for key in ("KAIN_REPO_ROOT", "KAIN_MCP_REPO_ROOT"):
        value = os.environ.get(key)
        if value:
            candidates.append(Path(value))
    candidates.append(Path(__file__).resolve().parents[2])

    for candidate in candidates:
        root = candidate.expanduser().resolve()
        if (root / "MODULE.bazel").exists() or (root / ".git").exists():
            return root

    try:
        output = subprocess.check_output(
            ["git", "rev-parse", "--show-toplevel"],
            text=True,
            stderr=subprocess.DEVNULL,
        ).strip()
        if output:
            return Path(output).resolve()
    except (OSError, subprocess.CalledProcessError):
        pass
    raise SyncError("unable to resolve repository root")


def resolve_configured_path(repo_root: Path, value: str) -> Path:
    expanded = os.path.expandvars(value)
    if expanded.startswith("~"):
        expanded = str(Path(expanded).expanduser())
    path = Path(expanded)
    if not path.is_absolute():
        path = repo_root / path
    return path.resolve()


def resolve_policy(repo_root: Path) -> dict[str, object]:
    path = repo_root / "blades" / "kain-mcp" / "config" / "runtime_policy.json"
    return read_json(path)


def resolve_command_path(name: str) -> Path | None:
    value = shutil.which(name)
    return Path(value).resolve() if value else None


def command_prints_path(command: Sequence[str]) -> Path | None:
    try:
        output = subprocess.check_output(
            list(command) + ["-c", "import sys; print(sys.executable)"],
            text=True,
            stderr=subprocess.DEVNULL,
            env=sanitized_python_env(os.environ.copy()),
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return None
    candidate = Path(output)
    return candidate.resolve() if candidate.exists() else None


def resolve_python_path() -> Path | None:
    override = os.environ.get("KAIN_BAZEL_PYTHON")
    if override:
        candidate = Path(override).expanduser()
        if candidate.exists():
            return candidate.resolve()

    if platform.system().lower() == "windows":
        py = resolve_command_path("py")
        if py:
            for minor in ("12", "11", "10"):
                resolved = command_prints_path([str(py), f"-3.{minor}"])
                if resolved:
                    return resolved
            resolved = command_prints_path([str(py), "-3"])
            if resolved:
                return resolved

    for name in ("python3.12", "python3.11", "python3.10", "python3", "python"):
        command = resolve_command_path(name)
        if command:
            resolved = command_prints_path([str(command)])
            if resolved:
                return resolved
    return None


def resolve_clang_path(repo_root: Path) -> Path | None:
    candidates: list[Path] = []
    override = os.environ.get("KAIN_CLANG_PATH")
    if override:
        candidates.append(Path(override))
    candidates.extend(
        (
            repo_root / "toolchain" / "llvm" / "bin" / "clang.exe",
            repo_root / "toolchain" / "llvm" / "bin" / "clang",
        )
    )
    for name in ("clang", "clang.exe"):
        command = resolve_command_path(name)
        if command:
            candidates.append(command)
    if platform.system().lower() == "windows":
        candidates.append(Path("C:/Program Files/LLVM/bin/clang.exe"))
    for candidate in candidates:
        expanded = candidate.expanduser()
        if expanded.exists():
            return expanded.resolve()
    return None


def resolve_bash_path() -> Path | None:
    override = os.environ.get("BAZEL_SH")
    if override:
        candidate = Path(override).expanduser()
        if candidate.exists():
            return candidate.resolve()
    command = resolve_command_path("bash")
    if command:
        return command
    if platform.system().lower() == "windows":
        for candidate in (
            Path("F:/Scoop/apps/git/current/bin/bash.exe"),
            Path("C:/Program Files/Git/bin/bash.exe"),
            Path("C:/msys64/usr/bin/bash.exe"),
        ):
            if candidate.exists():
                return candidate.resolve()
    return None


def resolve_sync_context(
    repo_root: Path,
    bazel_config: str | None = None,
    launcher_dir: str | None = None,
) -> SyncContext:
    policy = resolve_policy(repo_root)
    sync_policy_raw = policy.get("launcher_sync", {})
    sync_policy = sync_policy_raw if isinstance(sync_policy_raw, dict) else {}
    is_windows = platform.system().lower() == "windows"
    state_root_key = str(sync_policy.get("state_root_env_key", "KAIN_SYNC_ROOT"))
    state_root_override = os.environ.get(state_root_key)
    default_state_key = (
        "default_state_root_windows" if is_windows else "default_state_root_unix"
    )
    state_root = resolve_configured_path(
        repo_root,
        state_root_override or str(sync_policy.get(default_state_key, ".kain/state")),
    )
    stamp_override = os.environ.get("KAIN_SYNC_STAMP_PATH")
    stamp_path = (
        Path(stamp_override).expanduser().resolve()
        if stamp_override
        else (
            state_root
            / str(sync_policy.get("stamp_relative_path", "state/kain_sync_stamp.json"))
        ).resolve()
    )
    config_key = (
        "bazel_default_config_windows" if is_windows else "bazel_default_config_unix"
    )
    resolved_config = (
        bazel_config
        or os.environ.get("KAIN_BAZEL_CONFIG")
        or str(sync_policy.get(config_key, "dev"))
    )
    launcher_key = (
        "shared_launcher_dir_windows" if is_windows else "shared_launcher_dir_unix"
    )
    launcher_value = (
        launcher_dir
        or os.environ.get("KAIN_BAZEL_LAUNCHER_DIR")
        or str(sync_policy.get(launcher_key, ".kain/bin"))
    )
    binary_names = sanitize_binary_names(
        as_string_list(sync_policy.get("launcher_binary_names"), DEFAULT_BINARY_NAMES)
    )
    repo_kain_home = (repo_root / ".kain").resolve()
    return SyncContext(
        repo_root=repo_root,
        policy=policy,
        sync_policy=sync_policy,
        state_root=state_root,
        stamp_path=stamp_path,
        bazel_config=resolved_config,
        source_watch_paths=as_string_list(
            sync_policy.get("source_watch_paths"), DEFAULT_SOURCE_WATCH_PATHS
        ),
        source_filesystem_watch_paths=as_string_list(
            sync_policy.get("source_filesystem_watch_paths"), ("toolchain/rules_rust",)
        ),
        runtime_stamp_files=as_string_list(
            sync_policy.get("runtime_stamp_files"), DEFAULT_RUNTIME_STAMP_FILES
        ),
        launcher_dir=resolve_configured_path(repo_root, launcher_value),
        binary_names=binary_names,
        repo_kain_home=repo_kain_home,
        repo_kain_config=repo_kain_home / "config.toml",
        clang_path=resolve_clang_path(repo_root),
        python_path=resolve_python_path(),
    )


def read_bazel_rc_setting(repo_root: Path, prefixes: Sequence[str]) -> str | None:
    for rc_name in (".bazelrc.local", ".bazelrc"):
        rc_path = repo_root / rc_name
        if not rc_path.exists():
            continue
        try:
            lines = rc_path.read_text(encoding="utf-8").splitlines()
        except OSError:
            continue
        for raw_line in lines:
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue
            for prefix in prefixes:
                if line.startswith(prefix):
                    value = line[len(prefix) :].strip()
                    if value:
                        return value
    return None


def resolve_bazel_storage_roots(repo_root: Path) -> BazelStorageRoots:
    output_user_root = os.environ.get("KAIN_BAZEL_OUTPUT_USER_ROOT")
    if not output_user_root:
        output_user_root = read_bazel_rc_setting(
            repo_root, ("startup --output_user_root=",)
        )
    repository_cache = os.environ.get("KAIN_BAZEL_REPOSITORY_CACHE")
    if not repository_cache:
        repository_cache = read_bazel_rc_setting(
            repo_root, ("common --repository_cache=",)
        )
    disk_cache = os.environ.get("KAIN_BAZEL_DISK_CACHE")
    if not disk_cache:
        disk_cache = read_bazel_rc_setting(repo_root, ("build --disk_cache=",))
    return BazelStorageRoots(
        output_user_root=(
            resolve_configured_path(repo_root, output_user_root)
            if output_user_root
            else None
        ),
        disk_cache=(
            resolve_configured_path(repo_root, disk_cache) if disk_cache else None
        ),
        repository_cache=(
            resolve_configured_path(repo_root, repository_cache)
            if repository_cache
            else None
        ),
    )


def resolve_bazel_storage_limit_bytes(default_gib: int = DEFAULT_BAZEL_STORAGE_LIMIT_GIB) -> int:
    raw_bytes = os.environ.get("KAIN_BAZEL_STORAGE_LIMIT_BYTES", "").strip()
    if raw_bytes:
        try:
            return max(0, int(raw_bytes))
        except ValueError:
            pass
    raw_gib = os.environ.get("KAIN_BAZEL_STORAGE_LIMIT_GIB", "").strip()
    if raw_gib:
        try:
            return max(0, int(float(raw_gib) * (1024**3)))
        except ValueError:
            pass
    return default_gib * 1024 * 1024 * 1024


def format_gibibytes(value_bytes: int) -> str:
    return f"{value_bytes / (1024 ** 3):.2f} GiB"


def format_optional_path(value: Path | None) -> str:
    return str(value) if value else "(unset)"


def path_tree_size_bytes(path: Path) -> int:
    if not path.exists():
        return 0
    try:
        if path.is_file() or path.is_symlink():
            return int(path.stat().st_size)
    except OSError:
        return 0

    total = 0
    stack = [path]
    while stack:
        current = stack.pop()
        try:
            children = list(current.iterdir())
        except OSError:
            continue
        for child in children:
            try:
                if child.is_symlink() or child.is_file():
                    total += int(child.stat().st_size)
                elif child.is_dir():
                    stack.append(child)
                else:
                    total += int(child.stat().st_size)
            except OSError:
                continue
    return total


def path_mtime_unix(path: Path) -> float:
    try:
        return float(path.stat().st_mtime)
    except OSError:
        return 0.0


def delete_path_best_effort(path: Path) -> tuple[bool, str | None]:
    def onerror(func, failed_path, exc_info):
        error = exc_info[1]
        if isinstance(error, FileNotFoundError):
            return
        try:
            os.chmod(failed_path, 0o700)
        except OSError:
            pass
        try:
            func(failed_path)
        except FileNotFoundError:
            return

    try:
        if not path.exists():
            return True, None
        if path.is_dir() and not path.is_symlink():
            shutil.rmtree(path, onerror=onerror)
        else:
            path.unlink(missing_ok=True)
        return True, None
    except OSError as error:
        return False, f"failed to remove {path}: {error}"


def collect_storage_entries(root_name: str, root: Path) -> list[BazelStorageEntry]:
    if not root.exists():
        return []
    entries: list[BazelStorageEntry] = []
    try:
        children = list(root.iterdir())
    except OSError:
        return []
    for child in children:
        try:
            entries.append(
                BazelStorageEntry(
                    path=child,
                    size_bytes=path_tree_size_bytes(child),
                    mtime_unix=path_mtime_unix(child),
                    root_name=root_name,
                )
            )
        except OSError:
            continue
    return entries


def prune_bazel_storage(
    repo_root: Path,
    *,
    max_bytes: int | None = None,
    dry_run: bool = False,
) -> tuple[int, int, tuple[str, ...], tuple[str, ...]]:
    roots = resolve_bazel_storage_roots(repo_root)
    budget = resolve_bazel_storage_limit_bytes() if max_bytes is None else max_bytes
    if budget <= 0:
        budget = DEFAULT_BAZEL_STORAGE_LIMIT_BYTES

    named_roots: list[tuple[str, Path]] = []
    if roots.output_user_root:
        named_roots.append(("output_user_root", roots.output_user_root))
    if roots.disk_cache:
        named_roots.append(("disk_cache", roots.disk_cache))
    if roots.repository_cache:
        named_roots.append(("repository_cache", roots.repository_cache))

    entries: list[BazelStorageEntry] = []
    total_before = 0
    for root_name, root in named_roots:
        root_size = path_tree_size_bytes(root)
        total_before += root_size
        entries.extend(collect_storage_entries(root_name, root))

    removed: list[str] = []
    warnings: list[str] = []
    current_total = total_before
    for entry in sorted(entries, key=lambda item: (item.mtime_unix, item.size_bytes)):
        if current_total <= budget:
            break
        if dry_run:
            removed.append(f"{entry.root_name}:{entry.path}")
            current_total -= entry.size_bytes
            continue
        ok, warning = delete_path_best_effort(entry.path)
        if ok:
            removed.append(f"{entry.root_name}:{entry.path}")
            current_total -= entry.size_bytes
        elif warning:
            warnings.append(warning)

    if current_total > budget:
        warnings.append(
            "storage budget still exceeded after pruning; remaining bytes belong to currently retained roots"
        )
    return total_before, current_total, tuple(removed), tuple(warnings)


def report_bazel_storage_prune(repo_root: Path, *, dry_run: bool = False) -> int:
    budget = resolve_bazel_storage_limit_bytes()
    roots = resolve_bazel_storage_roots(repo_root)
    total_before, total_after, removed, warnings = prune_bazel_storage(
        repo_root, max_bytes=budget, dry_run=dry_run
    )

    print(f"Repo Root : {repo_root}")
    print(f"Budget    : {budget} bytes ({format_gibibytes(budget)})")
    print(f"Mode      : {'dry-run' if dry_run else 'prune'}")
    print(f"Output Root : {format_optional_path(roots.output_user_root)}")
    print(f"Disk Cache  : {format_optional_path(roots.disk_cache)}")
    print(f"Repo Cache  : {format_optional_path(roots.repository_cache)}")
    print(f"Before    : {total_before} bytes ({format_gibibytes(total_before)})")
    print(f"After     : {total_after} bytes ({format_gibibytes(total_after)})")
    print(f"Removed   : {len(removed)} entries")
    for item in removed:
        print(f"  {item}")
    if warnings:
        print("Warnings  :")
        for warning in warnings:
            print(f"  {warning}")
    return 0


def sanitized_python_env(base: dict[str, str]) -> dict[str, str]:
    env = dict(base)
    for key in PYTHON_POLLUTION_ENV_KEYS:
        env.pop(key, None)
    return env


def runtime_env(context: SyncContext) -> dict[str, str]:
    env = sanitized_python_env(os.environ.copy())
    temp_root = sync_temp_root(context)
    host_triple = get_host_triple()
    kain_lib_dir = context.repo_kain_home / "lib" / host_triple
    runtime_lib_name = get_runtime_lib_name(host_triple)
    runtime_lib_path = kain_lib_dir / runtime_lib_name
    env.update(
        {
            "KAIN_REPO_ROOT": str(context.repo_root),
            "KAIN_HOME": str(context.repo_kain_home),
            "KAIN_CONFIG": str(context.repo_kain_config),
            "KAIN_STDLIB_PATH": str(context.repo_root / "stdlib"),
            "KAIN_RUNTIME_LIB_PATH": str(runtime_lib_path) if runtime_lib_path.exists() else "",
            "KAIN_RUNTIME_C_PATH": str(context.repo_root / "runtime" / "runtime.c"),
            "KAIN_RUNTIME_MANIFEST_PATH": str(
                context.repo_root / "runtime" / "native_core_runtime.toml"
            ),
            "KAIN_SYNC_ROOT": str(context.state_root),
            "KAIN_SYNC_STAMP_PATH": str(context.stamp_path),
            "KAIN_BAZEL_CONFIG": context.bazel_config,
            "KAIN_BAZEL_LAUNCHER_DIR": str(context.launcher_dir),
            "TMP": str(temp_root),
            "TEMP": str(temp_root),
            "TMPDIR": str(temp_root),
        }
    )
    if context.clang_path:
        env["KAIN_CLANG_PATH"] = str(context.clang_path)
        env["PATH"] = prepend_path(env.get("PATH", ""), str(context.clang_path.parent))
    bash_path = resolve_bash_path()
    if bash_path:
        env["BAZEL_SH"] = str(bash_path)
    return env


def sync_temp_root(context: SyncContext) -> Path:
    temp_root = (context.state_root / "tmp").resolve()
    temp_root.mkdir(parents=True, exist_ok=True)
    return temp_root


def prepend_path(path_value: str, prefix: str) -> str:
    separator = os.pathsep
    parts = [
        part
        for part in path_value.split(separator)
        if part and os.path.normcase(part) != os.path.normcase(prefix)
    ]
    return separator.join([prefix] + parts)


def bazel_python_args(context: SyncContext) -> tuple[str, ...]:
    if not context.python_path:
        return ()
    return (
        f"--repo_env=PYO3_PYTHON={context.python_path}",
        f"--action_env=PYO3_PYTHON={context.python_path}",
    )


def bazel_output_binary_name(context: SyncContext, binary_name: str) -> str:
    configured = context.sync_policy.get("bazel_binary_output_names", {})
    if isinstance(configured, dict):
        value = configured.get(binary_name)
        if value:
            return str(value)
    return binary_name


def sibling_bazel_build_binary(binary_name: str) -> str | None:
    # kn is now an alias for kain via argv[0] detection — no separate binary needed.
    return None


def run_capture(
    args: Sequence[str], cwd: Path, env: dict[str, str] | None = None
) -> CommandResult:
    try:
        proc = subprocess.run(
            list(args),
            cwd=str(cwd),
            env=env,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
    except OSError as error:
        raise SyncError(f"failed to start {args[0]}: {error}") from error
    lines = tuple(strip_ansi(line.rstrip("\n")) for line in proc.stdout.splitlines())
    return CommandResult(proc.returncode, lines)


def run_live(
    args: Sequence[str], cwd: Path, env: dict[str, str] | None = None
) -> CommandResult:
    try:
        proc = subprocess.Popen(
            list(args),
            cwd=str(cwd),
            env=env,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
    except OSError as error:
        raise SyncError(f"failed to start {args[0]}: {error}") from error

    output: list[str] = []
    assert proc.stdout is not None
    for line in proc.stdout:
        text = line.rstrip("\n")
        output.append(strip_ansi(text))
        print(text, flush=True)
    return CommandResult(proc.wait(), tuple(output))


def strip_ansi(text: str) -> str:
    # The launcher only needs enough scrubbing for Bazel status output and paths.
    import re

    return re.sub(r"\x1B\[[0-9;]*[A-Za-z]", "", text)


def split_package_id(package_id: str) -> tuple[str, str]:
    if " " not in package_id:
        return package_id, ""
    name, version = package_id.rsplit(" ", 1)
    return name, version


def git_lines(repo_root: Path, args: Sequence[str]) -> tuple[str, ...]:
    result = run_capture(["git", "-C", str(repo_root), *args], cwd=repo_root)
    if result.exit_code != 0:
        return ()
    return tuple(line.strip() for line in result.output_lines if line.strip())


def cargo_metadata(repo_root: Path, env: dict[str, str]) -> dict[str, object]:
    result = run_capture(
        ["cargo", "metadata", "--format-version", "1", "--no-deps"],
        repo_root,
        env,
    )
    if result.exit_code != 0:
        raise SyncError(f"cargo metadata failed with exit code {result.exit_code}")
    try:
        payload = json.loads(result.output_text)
    except json.JSONDecodeError as error:
        raise SyncError(f"cargo metadata returned invalid JSON: {error}") from error
    if not isinstance(payload, dict):
        raise SyncError("cargo metadata returned an unexpected payload shape")
    return payload


def cargo_bazel_lock_path(repo_root: Path) -> Path:
    return (repo_root / "Cargo.Bazel.lock").resolve()


def cargo_bazel_lock_data(repo_root: Path) -> dict[str, object]:
    path = cargo_bazel_lock_path(repo_root)
    if not path.exists():
        raise SyncError(f"Cargo.Bazel.lock not found at {path}")
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        raise SyncError(f"Cargo.Bazel.lock is invalid JSON: {error}") from error
    if not isinstance(payload, dict):
        raise SyncError("Cargo.Bazel.lock has an unexpected top-level shape")
    return payload


def cargo_bazel_lock_entry_dep_names(entry: dict[str, object]) -> set[str]:
    dep_names: set[str] = set()
    common_attrs = entry.get("common_attrs", {})
    if not isinstance(common_attrs, dict):
        return dep_names
    deps = common_attrs.get("deps", {})
    if not isinstance(deps, dict):
        return dep_names

    def add_dep_list(items: object) -> None:
        if not isinstance(items, list):
            return
        for item in items:
            if not isinstance(item, dict):
                continue
            raw_id = str(item.get("id", "")).strip()
            if not raw_id:
                continue
            dep_name, _dep_version = split_package_id(raw_id)
            dep_names.add(dep_name)

    add_dep_list(deps.get("common"))
    selects = deps.get("selects", {})
    if isinstance(selects, dict):
        for values in selects.values():
            add_dep_list(values)

    # Proc-macro deps (e.g. enum_dispatch) live under proc_macro_deps, not deps
    proc_macro_deps = common_attrs.get("proc_macro_deps", {})
    if isinstance(proc_macro_deps, dict):
        add_dep_list(proc_macro_deps.get("common"))
        proc_selects = proc_macro_deps.get("selects", {})
        if isinstance(proc_selects, dict):
            for values in proc_selects.values():
                add_dep_list(values)

    return dep_names


def cargo_manifest_required_external_deps(package: dict[str, object]) -> set[str]:
    required: set[str] = set()
    dependencies = package.get("dependencies", [])
    if not isinstance(dependencies, list):
        return required

    for dependency in dependencies:
        if not isinstance(dependency, dict):
            continue
        if dependency.get("kind") not in (None, "normal"):
            continue
        if bool(dependency.get("optional")):
            continue
        if dependency.get("path"):
            continue
        name = str(dependency.get("name", "")).strip()
        if not name:
            continue
        required.add(name)

    return required


def cargo_bazel_manifest_drift(context: SyncContext, env: dict[str, str]) -> list[str]:
    metadata = cargo_metadata(context.repo_root, env)
    packages = metadata.get("packages", [])
    if not isinstance(packages, list):
        return []

    lock = cargo_bazel_lock_data(context.repo_root)
    workspace_members = lock.get("workspace_members", {})
    crates = lock.get("crates", {})
    if not isinstance(workspace_members, dict) or not isinstance(crates, dict):
        return []

    drift_messages: list[str] = []
    for package in packages:
        if not isinstance(package, dict):
            continue
        name = str(package.get("name", "")).strip()
        version = str(package.get("version", "")).strip()
        if not name or not version:
            continue
        package_id = f"{name} {version}"
        if package_id not in workspace_members:
            continue
        lock_entry = crates.get(package_id)
        if not isinstance(lock_entry, dict):
            drift_messages.append(f"{package_id}: missing crate_universe package entry")
            continue
        manifest_deps = cargo_manifest_required_external_deps(package)
        lock_deps = cargo_bazel_lock_entry_dep_names(lock_entry)
        missing = sorted(dep for dep in manifest_deps if dep not in lock_deps)
        if missing:
            drift_messages.append(f"{package_id}: missing {', '.join(missing)}")

    return drift_messages


def repo_head_sha(repo_root: Path) -> str:
    override = os.environ.get("KAIN_SYNC_REPO_SHA")
    if override:
        return override
    lines = git_lines(repo_root, ("rev-parse", "HEAD"))
    return lines[0] if lines else "unknown"


def git_head_object(repo_root: Path, relative: str) -> str:
    lines = git_lines(repo_root, ("rev-parse", "--verify", f"HEAD:{relative}"))
    return lines[0] if lines else "missing"


def filesystem_descriptor(repo_root: Path, relative: str) -> str:
    candidate = repo_root / relative
    if not candidate.exists():
        return f"fs|{relative}|missing"
    if candidate.is_file():
        stat = candidate.stat()
        return f"fs|{relative}|file|{stat.st_size}|{stat.st_mtime_ns}"
    entries = [f"root|{relative}|dir"]
    for path in sorted(candidate.rglob("*"), key=lambda value: str(value).lower()):
        rel = normalize_relative_path(str(path.relative_to(repo_root)))
        if path.is_dir():
            entries.append(f"dir|{rel}")
        else:
            stat = path.stat()
            entries.append(f"file|{rel}|{stat.st_size}|{stat.st_mtime_ns}")
    return f"fs|{relative}|{short_sha256(chr(10).join(entries))}"


def file_content_hash(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def source_stamp_data(
    repo_root: Path,
    watch_paths: Sequence[str],
    filesystem_watch_paths: Sequence[str] = (),
) -> dict[str, object]:
    override = os.environ.get("KAIN_SYNC_SOURCE_STAMP")
    normalized_watch_paths = tuple(
        sorted(
            {normalize_relative_path(path) for path in watch_paths if path.strip()},
            key=str.lower,
        )
    )
    if override:
        return {
            "stamp": override,
            "dirty_count": 0,
            "watch_paths": normalized_watch_paths,
            "filesystem_watch_paths": tuple(filesystem_watch_paths),
        }

    head_descriptors = [
        f"head|{relative}|{git_head_object(repo_root, relative)}"
        for relative in normalized_watch_paths
    ]
    dirty_paths: set[str] = set()
    for args in (
        ("diff", "--name-only"),
        ("diff", "--cached", "--name-only"),
        ("ls-files", "--others", "--exclude-standard"),
    ):
        for line in git_lines(repo_root, args):
            normalized = normalize_relative_path(line)
            if normalized and path_matches_watch_path(
                normalized, normalized_watch_paths
            ):
                dirty_paths.add(normalized)

    dirty_descriptors: list[str] = []
    for relative in sorted(dirty_paths, key=str.lower):
        candidate = repo_root / relative
        if candidate.is_file():
            dirty_descriptors.append(
                f"dirty|{relative}|file|{file_content_hash(candidate)}"
            )
        elif candidate.is_dir():
            dirty_descriptors.append(f"dirty|{relative}|dir|present")
        else:
            dirty_descriptors.append(f"dirty|{relative}|missing")

    stamp_lines: list[str] = [
        f"watch|{relative}" for relative in normalized_watch_paths
    ]
    stamp_lines.extend(head_descriptors)
    stamp_lines.extend(dirty_descriptors)
    normalized_fs_watch_paths: list[str] = []
    for relative in filesystem_watch_paths:
        normalized = normalize_relative_path(relative)
        if not normalized:
            continue
        normalized_fs_watch_paths.append(normalized)
        stamp_lines.append(f"watch-fs|{normalized}")
        stamp_lines.append(filesystem_descriptor(repo_root, normalized))

    return {
        "stamp": short_sha256("\n".join(stamp_lines)),
        "dirty_count": len(dirty_descriptors),
        "watch_paths": normalized_watch_paths,
        "filesystem_watch_paths": tuple(normalized_fs_watch_paths),
    }


def runtime_stamp(repo_root: Path, runtime_stamp_files: Sequence[str]) -> str:
    override = os.environ.get("KAIN_SYNC_RUNTIME_STAMP")
    if override:
        return override
    lines: list[str] = []
    for relative in runtime_stamp_files:
        normalized = normalize_relative_path(relative)
        candidate = repo_root / relative
        if candidate.exists():
            stat = candidate.stat()
            lines.append(f"{normalized}|1|{stat.st_size}|{int(stat.st_mtime)}")
        else:
            lines.append(f"{normalized}|0||")
    return short_sha256("\n".join(lines))


def binary_entry(
    stamp_payload: dict[str, object], binary_name: str
) -> dict[str, object] | None:
    raw = stamp_payload.get("binary_by_name", {})
    if isinstance(raw, dict):
        entry = raw.get(binary_name)
        if isinstance(entry, dict):
            return entry
    if binary_name == "kain":
        legacy = stamp_payload.get("binary")
        if isinstance(legacy, dict):
            return legacy
    return None


def stamped_binary_path(
    stamp_payload: dict[str, object], binary_name: str
) -> Path | None:
    entry = binary_entry(stamp_payload, binary_name)
    if not entry:
        return None
    path = entry.get("path")
    return Path(str(path)).expanduser().resolve() if path else None


def choose_build_action(
    stamp_payload: dict[str, object],
    binary_name: str,
    current_source_stamp: str,
    bazel_config: str,
    skip_build: bool,
) -> BuildDecision:
    if skip_build:
        return BuildDecision(False, "skip-build flag set")
    if not current_source_stamp:
        return BuildDecision(True, "source stamp unavailable")
    entry = binary_entry(stamp_payload, binary_name)
    if entry is None:
        return BuildDecision(True, "missing stamped binary entry")
    entry_source_stamp = str(entry.get("source_stamp", ""))
    if not entry_source_stamp:
        return BuildDecision(True, "missing per-binary source stamp")
    if entry_source_stamp != current_source_stamp:
        return BuildDecision(True, "binary source stamp changed")
    entry_config = str(entry.get("bazel_config", ""))
    if not entry_config:
        return BuildDecision(True, "missing per-binary bazel config")
    if entry_config != bazel_config:
        return BuildDecision(True, "binary bazel config changed")
    path = stamped_binary_path(stamp_payload, binary_name)
    if path is None:
        return BuildDecision(True, "missing stamped binary path")
    if not path.exists():
        return BuildDecision(True, "staged binary missing")
    return BuildDecision(False, "source unchanged")


def binary_fingerprint(path: Path) -> dict[str, object]:
    resolved = path.resolve()
    if not resolved.exists():
        return {
            "path": str(resolved),
            "exists": False,
        }
    stat = resolved.stat()
    return {
        "path": str(resolved),
        "exists": True,
        "size_bytes": stat.st_size,
        "mtime_unix": int(stat.st_mtime),
    }


def staged_binary_path(
    context: SyncContext,
    binary_name: str,
    source_stamp: str,
    bazel_fingerprint: dict[str, object],
) -> Path:
    suffix = ".exe" if platform.system().lower() == "windows" else ""
    relative_dir = str(context.sync_policy.get("staged_binary_relative_dir", "bin"))
    version_token = f"{bazel_fingerprint.get('mtime_unix', 0)}-{bazel_fingerprint.get('size_bytes', 0)}"
    return (
        context.state_root
        / relative_dir
        / f"{binary_name}-{context.bazel_config}-{source_stamp}-{version_token}{suffix}"
    ).resolve()


def test_cargo_bazel_repin_mismatch(result: CommandResult) -> bool:
    text = result.output_text
    return (
        "out of date for 'crates'" in text
        and "CARGO_BAZEL_REPIN=true" in text
        and ("Digests do not match:" in text or "crate_universe" in text)
    )


def test_bazel_output_lock_mismatch(result: CommandResult) -> bool:
    text = result.output_text
    return (
        "failed to delete output files before executing action:" in text
        and "Permission denied" in text
    )


def acquire_lock(lock_path: Path, timeout_seconds: int = 300) -> object:
    deadline = time.monotonic() + timeout_seconds
    lock_path.parent.mkdir(parents=True, exist_ok=True)
    while True:
        try:
            if platform.system().lower() == "windows":
                import msvcrt

                handle = lock_path.open("a+b")
                msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)
                return handle
            import fcntl

            handle = lock_path.open("a+b")
            fcntl.flock(handle.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
            return handle
        except OSError:
            if time.monotonic() >= deadline:
                raise SyncError(f"timed out waiting for lock {lock_path}")
            time.sleep(0.25)


def release_lock(handle: object) -> None:
    try:
        if platform.system().lower() == "windows":
            import msvcrt

            msvcrt.locking(handle.fileno(), msvcrt.LK_UNLCK, 1)
        else:
            import fcntl

            fcntl.flock(handle.fileno(), fcntl.LOCK_UN)
    finally:
        handle.close()


def bazel_env(context: SyncContext) -> dict[str, str]:
    env = runtime_env(context)
    return env


def cargo_bazel_repin(
    context: SyncContext,
    binary_name: str,
    env: dict[str, str],
    extra_args: Sequence[str],
    *,
    reason: str,
) -> None:
    lock_relative = str(
        context.sync_policy.get(
            "cargo_bazel_repin_lock_relative_path", "locks/cargo-bazel-repin.lock"
        )
    )
    lock_timeout = int(
        context.sync_policy.get("cargo_bazel_repin_lock_timeout_seconds", 300)
    )
    lock_path = context.state_root / lock_relative
    print(f"[kain] {reason}; repinning crate_universe and retrying once...", flush=True)
    lock_handle = acquire_lock(lock_path, lock_timeout)
    try:
        repin_env = dict(env)
        repin_env["CARGO_BAZEL_REPIN"] = "true"
        repin_args = [
            "bazel",
            "fetch",
            f"//:{binary_name}",
            f"--config={context.bazel_config}",
            *extra_args,
        ]
        repin_result = run_live(repin_args, context.repo_root, repin_env)
        if repin_result.exit_code != 0:
            raise SyncError(f"auto-repin failed while running {' '.join(repin_args)}")
    finally:
        release_lock(lock_handle)


def build_counter_path(repo_root: Path) -> Path:
    """Path to the persisted build counter file (outside Bazel sandbox)."""
    return (repo_root / ".kain" / "build_counter").resolve()


def read_build_counter(file_path: Path) -> int:
    """Read the current build counter value. Initializes to 8000 if absent."""
    try:
        val = int(file_path.read_text().strip())
        return val
    except (FileNotFoundError, ValueError, OSError):
        return 8000


def write_build_info_file(
    context: SyncContext,
    counter_value: int,
    short_sha: str,
    commit_count: str,
    tracking_mode: str,
) -> None:
    """Write data/build_info.env for the CLI crate so build.rs can read it.

    This is the primary delivery mechanism for build tracking metadata into
    the Rust build script, since Bazel's --action_env does not reliably
    propagate to cargo_build_script actions in all rules_rust versions.
    """
    info_path = context.repo_root / "crates" / "cli" / "data" / "build_info.env"
    build_number = f"{tracking_mode}-{counter_value}-{short_sha}"
    source_date_epoch = str(int(time.time()))
    lines = [
        f"KAIN_BUILD_NUMBER={build_number}",
        f"KAIN_BUILD_TRACKING_MODE={tracking_mode}",
        f"KAIN_BUILD_GIT_SHA={short_sha}",
        f"KAIN_BUILD_GIT_COMMIT_COUNT={commit_count}",
        f"KAIN_BUILD_GIT_DIRTY=clean",
        f"SOURCE_DATE_EPOCH={source_date_epoch}",
    ]
    write_text_atomic(info_path, "\n".join(lines) + "\n")


def bazel_build_tracking_args(context: SyncContext) -> tuple[str, ...]:
    """Return --action_env flags that embed build tracking into the binary.

    Note: --action_env does not reliably propagate to cargo_build_script
    actions in all rules_rust versions, so the primary delivery mechanism
    is write_build_info_file() which produces data/build_info.env read by
    build.rs.  These --action_env flags are retained as a secondary channel
    for any actions that do receive them.
    """
    tracking_mode = context.sync_policy.get(
        "build_tracking_mode", f"bazel-{context.bazel_config}"
    )
    if not isinstance(tracking_mode, str) or not tracking_mode.strip():
        tracking_mode = f"bazel-{context.bazel_config}"
    repo_sha = repo_head_sha(context.repo_root)
    short_sha = repo_sha[:12] if len(repo_sha) >= 12 else repo_sha

    # Read and increment build counter
    counter_path = build_counter_path(context.repo_root)
    counter_value = read_build_counter(counter_path)
    next_counter = counter_value + 1
    write_text_atomic(counter_path, str(next_counter))

    # Get git commit count
    commit_lines = git_lines(context.repo_root, ("rev-list", "--count", "HEAD"))
    commit_count = commit_lines[0] if commit_lines else "0"

    # Write build info file for build.rs to consume
    write_build_info_file(context, counter_value, short_sha, commit_count, tracking_mode)

    build_number = f"{tracking_mode}-{counter_value}-{short_sha}"
    source_date_epoch = str(int(time.time()))
    return (
        f"--action_env=SOURCE_DATE_EPOCH={source_date_epoch}",
        f"--action_env=KAIN_BUILD_NUMBER={build_number}",
        f"--action_env=KAIN_BUILD_TRACKING_MODE={tracking_mode}",
        f"--action_env=KAIN_BUILD_GIT_SHA={short_sha}",
        f"--action_env=KAIN_BUILD_GIT_COMMIT_COUNT={commit_count}",
    )


def run_bazel_build_target(context: SyncContext, binary_name: str) -> CommandResult:
    extra_args = (
        *bazel_python_args(context),
        *bazel_build_tracking_args(context),
    )
    env = bazel_env(context)
    auto_repin = bool_from_policy(
        context.sync_policy.get("cargo_bazel_auto_repin_enabled"), True
    )
    if auto_repin:
        drift = cargo_bazel_manifest_drift(context, env)
        if drift:
            drift_preview = "; ".join(drift[:5])
            if len(drift) > 5:
                drift_preview += f"; ... (+{len(drift) - 5} more)"
            cargo_bazel_repin(
                context,
                binary_name,
                env,
                extra_args,
                reason=f"Cargo.Bazel.lock manifest drift detected ({drift_preview})",
            )

    build_args = [
        "bazel",
        "build",
        f"//:{binary_name}",
        f"--config={context.bazel_config}",
        *extra_args,
    ]
    result = run_live(build_args, context.repo_root, env)
    if result.exit_code == 0:
        return result

    if not auto_repin or not test_cargo_bazel_repin_mismatch(result):
        return result

    cargo_bazel_repin(
        context,
        binary_name,
        env,
        extra_args,
        reason="Cargo.Bazel.lock drift detected",
    )
    print(
        f"[kain] Cargo.Bazel.lock refreshed; retrying bazel build //:{binary_name} --config={context.bazel_config}...",
        flush=True,
    )
    return run_live(build_args, context.repo_root, env)


def invoke_bazel_build(context: SyncContext, binary_name: str) -> str:
    result = run_bazel_build_target(context, binary_name)
    if result.exit_code == 0:
        return binary_name

    fallback_binary = sibling_bazel_build_binary(binary_name)
    if fallback_binary and test_bazel_output_lock_mismatch(result):
        # `kain` and `kn` both enter the same launcher and differ only by argv0
        # identity. If one Bazel output file is locked, build the sibling target
        # and stage it under the requested launcher identity instead of staying stale.
        print(
            f"[kain] bazel output for //:{binary_name} is locked; retrying via //:{fallback_binary} and staging it as {binary_name}...",
            flush=True,
        )
        fallback_result = run_bazel_build_target(context, fallback_binary)
        if fallback_result.exit_code == 0:
            return fallback_binary
        raise SyncError(
            f"bazel build //:{binary_name} --config={context.bazel_config} failed with exit code {result.exit_code}; "
            f"fallback //:{fallback_binary} also failed with exit code {fallback_result.exit_code}"
        )

    raise SyncError(
        f"bazel build //:{binary_name} --config={context.bazel_config} failed with exit code {result.exit_code}"
    )


def resolve_bazel_binary_path(context: SyncContext, binary_name: str) -> Path:
    result = run_capture(
        [
            "bazel",
            "info",
            "bazel-bin",
            f"--config={context.bazel_config}",
            *bazel_python_args(context),
        ],
        context.repo_root,
        bazel_env(context),
    )
    if result.exit_code != 0:
        raise SyncError(
            f"bazel info bazel-bin failed with exit code {result.exit_code}"
        )
    lines = [line.strip() for line in result.output_lines if line.strip()]
    if not lines:
        raise SyncError("bazel info bazel-bin returned no output")
    suffix = ".exe" if platform.system().lower() == "windows" else ""
    output_name = bazel_output_binary_name(context, binary_name)
    return (Path(lines[-1]) / "crates" / "cli" / f"{output_name}{suffix}").resolve()


def merge_stamp_payload(
    existing: dict[str, object],
    *,
    context: SyncContext,
    binary_name: str,
    bazel_binary_name: str,
    current_source_stamp: str,
    source_data: dict[str, object],
    runtime_hash: str,
    build_performed: bool,
    build_reason: str,
    active_binary_fingerprint: dict[str, object] | None,
    bazel_binary_path: Path | None,
    launcher_path: Path | None,
) -> dict[str, object]:
    now = int(time.time())
    binary_by_name_raw = existing.get("binary_by_name", {})
    binary_by_name: dict[str, object] = (
        dict(binary_by_name_raw) if isinstance(binary_by_name_raw, dict) else {}
    )
    if active_binary_fingerprint is not None and bazel_binary_path is not None:
        binary_by_name[binary_name] = {
            **active_binary_fingerprint,
            "bazel_path": str(bazel_binary_path),
            "bazel_binary_name": bazel_binary_name,
            "source_stamp": current_source_stamp,
            "bazel_config": context.bazel_config,
            "runtime_stamp": runtime_hash,
            "repo_sha": repo_head_sha(context.repo_root),
            "synced_at_unix": now,
        }

    payload: dict[str, object] = {
        "schema_version": 1,
        "repo_root": str(context.repo_root),
        "repo_sha": repo_head_sha(context.repo_root),
        "runtime_stamp": runtime_hash,
        "runtime_stamp_files": list(context.runtime_stamp_files),
        "binary_by_name": binary_by_name,
        "build_number": f"bazel-{context.bazel_config}",
        "synced_at_unix": now,
        "last_attempt_unix": now,
        "managed_sync": False,
        "source_of_truth": "bazel-wrapper",
        "bazel_config": context.bazel_config,
        "source_stamp": current_source_stamp,
        "source_watch_paths": list(
            source_data.get("watch_paths", context.source_watch_paths)
        ),
        "source_filesystem_watch_paths": list(
            source_data.get(
                "filesystem_watch_paths", context.source_filesystem_watch_paths
            )
        ),
        "source_dirty_count": int(source_data.get("dirty_count", 0)),
        "build_performed": build_performed,
        "build_reason": build_reason,
    }
    if "kain" in binary_by_name:
        payload["binary"] = binary_by_name["kain"]
    elif isinstance(existing.get("binary"), dict):
        payload["binary"] = existing["binary"]
    if launcher_path:
        payload["launcher_path"] = str(launcher_path.resolve())
    return payload


def launch_binary(
    context: SyncContext,
    binary_name: str,
    forward_args: Sequence[str],
    *,
    skip_build: bool = False,
    update_stamp_only: bool = False,
    launcher_path: Path | None = None,
    source_data_override: dict[str, object] | None = None,
) -> int:
    if binary_name not in context.binary_names:
        raise SyncError(
            f"unsupported launcher binary {binary_name!r}; configured binaries are {', '.join(context.binary_names)}"
        )

    context.repo_kain_home.mkdir(parents=True, exist_ok=True)
    env = runtime_env(context)
    env["KAIN_ACTIVE_LAUNCHER_NAME"] = binary_name
    env["KAIN_ACTIVE_LAUNCHER_MODE"] = "bazel-wrapper"
    if launcher_path:
        env["KAIN_ACTIVE_LAUNCHER_PATH"] = str(launcher_path.resolve())

    # Ensure the native C runtime library is fresh before any build decision.
    # The runtime stamp is tracked in the payload, but the compiled .lib was
    # historically neglected — sync it unconditionally so the linker always
    # gets a library that matches the current runtime/native source tree.
    sync_runtime_library(context)

    existing = read_json(context.stamp_path)
    source_data = source_data_override or source_stamp_data(
        context.repo_root,
        context.source_watch_paths,
        context.source_filesystem_watch_paths,
    )
    current_source_stamp = str(source_data.get("stamp", ""))
    decision = choose_build_action(
        existing, binary_name, current_source_stamp, context.bazel_config, skip_build
    )

    bazel_binary_path: Path | None = None
    active_binary_path: Path | None = None
    active_fingerprint: dict[str, object] | None = None
    built_binary_name = binary_name
    if decision.should_build:
        built_binary_name = invoke_bazel_build(context, binary_name)
        bazel_binary_path = resolve_bazel_binary_path(context, built_binary_name)
        if not bazel_binary_path.exists():
            raise SyncError(f"Bazel binary not found at {bazel_binary_path}")
        bazel_fingerprint = binary_fingerprint(bazel_binary_path)
        active_binary_path = staged_binary_path(
            context, binary_name, current_source_stamp, bazel_fingerprint
        )
        if not active_binary_path.exists():
            copy_file_atomic(bazel_binary_path, active_binary_path)
        active_fingerprint = binary_fingerprint(active_binary_path)
    else:
        stamped_path = stamped_binary_path(existing, binary_name)
        if not skip_build and stamped_path is not None:
            active_binary_path = stamped_path
            active_fingerprint = binary_fingerprint(active_binary_path)
            entry = binary_entry(existing, binary_name)
            if entry and entry.get("bazel_path"):
                bazel_binary_path = (
                    Path(str(entry["bazel_path"])).expanduser().resolve()
                )

    runtime_hash = runtime_stamp(context.repo_root, context.runtime_stamp_files)
    payload = merge_stamp_payload(
        existing,
        context=context,
        binary_name=binary_name,
        bazel_binary_name=built_binary_name,
        current_source_stamp=current_source_stamp,
        source_data=source_data,
        runtime_hash=runtime_hash,
        build_performed=decision.should_build,
        build_reason=decision.reason,
        active_binary_fingerprint=active_fingerprint,
        bazel_binary_path=bazel_binary_path,
        launcher_path=launcher_path,
    )
    write_json_atomic(context.stamp_path, payload)

    if update_stamp_only:
        return 0
    if active_binary_path is None or not active_binary_path.exists():
        raise SyncError(
            f"no runnable staged binary is available for {binary_name}; run without --skip-build"
        )

    args = strip_forward_separator(forward_args)
    # Preserve the user's invocation directory for the actual binary so Kain
    # can resolve the nearest project/workspace from where the command was run.
    result = subprocess.run([str(active_binary_path), *args], env=env, check=False)
    return int(result.returncode)


def strip_forward_separator(forward_args: Sequence[str]) -> list[str]:
    args = list(forward_args)
    if args and args[0] == "--":
        return args[1:]
    return args


def rustc_command() -> list[str]:
    override = os.environ.get("RUSTC")
    if override:
        return [override]
    toolchain = os.environ.get("KAIN_RUST_TOOLCHAIN")
    if toolchain and resolve_command_path("rustup"):
        return ["rustup", "run", toolchain, "rustc"]
    if resolve_command_path("rustc"):
        # Invoke by tool name instead of resolved rustup proxy path; rustup keys
        # off argv[0] on Windows, and the full proxy path can look like rustup.
        return ["rustc"]
    raise SyncError("rustc was not found in PATH or RUSTC; cannot build launcher shim")


def build_launcher_shim(context: SyncContext) -> Path:
    source_path = (
        context.repo_root / "scripts" / "windows" / "kain_bazel_cli_launcher.rs"
    )
    if not source_path.exists():
        raise SyncError(f"Bazel launcher source not found at {source_path}")
    suffix = ".exe" if platform.system().lower() == "windows" else ""
    output_path = context.state_root / "artifacts" / f"kain_bazel_cli_launcher{suffix}"
    output_path.parent.mkdir(parents=True, exist_ok=True)
    temp_path = output_path.with_name(f"{output_path.name}.tmp.{os.getpid()}")
    temp_root = sync_temp_root(context)
    env = os.environ.copy()
    env["KAIN_DEFAULT_REPO_ROOT"] = str(context.repo_root)
    env["KAIN_DEFAULT_BAZEL_CONFIG"] = context.bazel_config
    env["KAIN_DEFAULT_LAUNCHER_DIR"] = str(context.launcher_dir)
    env["TMP"] = str(temp_root)
    env["TEMP"] = str(temp_root)
    env["TMPDIR"] = str(temp_root)
    result = run_live(
        [
            *rustc_command(),
            str(source_path),
            "--crate-name",
            "kain_bazel_cli_launcher",
            "-C",
            "opt-level=2",
            "-C",
            "debuginfo=0",
            "-o",
            str(temp_path),
        ],
        context.repo_root,
        env,
    )
    if result.exit_code != 0:
        raise SyncError(
            f"rustc failed to build Bazel launcher shim with exit code {result.exit_code}"
        )
    os.replace(temp_path, output_path)
    return output_path


def install_launcher_files(context: SyncContext, shim_path: Path) -> list[str]:
    context.launcher_dir.mkdir(parents=True, exist_ok=True)
    is_windows = platform.system().lower() == "windows"
    exe_suffix = ".exe" if is_windows else ""
    pending: list[str] = []
    for name in context.binary_names:
        launcher_path = context.launcher_dir / f"{name}{exe_suffix}"
        pending_message = copy_file_atomic_if_unlocked(shim_path, launcher_path)
        if pending_message:
            pending.append(pending_message)
        if not is_windows:
            launcher_path.chmod(0o755)
            continue
        # WSL shells can see the Windows PATH entry, but they need a bare
        # POSIX-shim name to resolve `kain` instead of only `kain.exe`.
        write_text_atomic(
            context.launcher_dir / name,
            _wsl_launcher_shim_text(name),
        )
        wrapper = context.launcher_dir / f"{name}.cmd"
        write_text_atomic(
            wrapper,
            f'@echo off\n"%~dp0{name}.exe" %*\nexit /b %ERRORLEVEL%\n',
        )
    return pending


def _wsl_launcher_shim_text(binary_name: str) -> str:
    return (
        "#!/usr/bin/env sh\n"
        "set -eu\n"
        'launcher_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"\n'
        f'exec "$launcher_dir/{binary_name}.exe" "$@"\n'
    )


def persist_windows_user_env(context: SyncContext) -> None:
    if platform.system().lower() != "windows":
        print(
            "[kain] --persist-user-env is only implemented for Windows user environment variables.",
            flush=True,
        )
        return
    try:
        import winreg
    except ImportError as error:
        raise SyncError(
            "winreg is unavailable; cannot persist Windows user env"
        ) from error

    host_triple = get_host_triple()
    runtime_lib_name = get_runtime_lib_name(host_triple)
    runtime_lib_path = context.repo_kain_home / "lib" / host_triple / runtime_lib_name
    values = {
        "KAIN_REPO_ROOT": str(context.repo_root),
        "KAIN_HOME": str(context.repo_kain_home),
        "KAIN_CONFIG": str(context.repo_kain_config),
        "KAIN_STDLIB_PATH": str(context.repo_root / "stdlib"),
        "KAIN_RUNTIME_LIB_PATH": str(runtime_lib_path) if runtime_lib_path.exists() else "",
        "KAIN_RUNTIME_C_PATH": str(context.repo_root / "runtime" / "runtime.c"),
        "KAIN_RUNTIME_MANIFEST_PATH": str(
            context.repo_root / "runtime" / "native_core_runtime.toml"
        ),
        "KAIN_SYNC_ROOT": str(context.state_root),
        "KAIN_SYNC_STAMP_PATH": str(context.stamp_path),
        "KAIN_BAZEL_CONFIG": context.bazel_config,
        "KAIN_BAZEL_LAUNCHER_DIR": str(context.launcher_dir),
    }
    if context.clang_path:
        values["KAIN_CLANG_PATH"] = str(context.clang_path)
    bash_path = resolve_bash_path()
    if bash_path:
        values["BAZEL_SH"] = str(bash_path)

    with winreg.OpenKey(
        winreg.HKEY_CURRENT_USER, "Environment", 0, winreg.KEY_READ | winreg.KEY_WRITE
    ) as key:
        for name, value in values.items():
            winreg.SetValueEx(key, name, 0, winreg.REG_EXPAND_SZ, value)
        try:
            current_path, _ = winreg.QueryValueEx(key, "Path")
        except FileNotFoundError:
            current_path = ""
        next_path = prepend_path(str(current_path), str(context.launcher_dir))
        if context.clang_path:
            next_path = prepend_path(next_path, str(context.clang_path.parent))
        winreg.SetValueEx(key, "Path", 0, winreg.REG_EXPAND_SZ, next_path)
        try:
            winreg.DeleteValue(key, "PYO3_PYTHON")
        except FileNotFoundError:
            pass


def _resolve_bazel_output_dir(context: SyncContext) -> Path | None:
    """Run `bazel info bazel-bin` and return the output directory."""
    result = run_capture(
        ["bazel", "info", "bazel-bin", f"--config={context.bazel_config}"],
        context.repo_root,
        bazel_env(context),
    )
    if result.exit_code != 0:
        return None
    lines = [line.strip() for line in result.output_lines if line.strip()]
    return Path(lines[-1]).resolve() if lines else None


def _find_lib_exe() -> str | None:
    """Find a librarian (archiver) tool for Windows .obj → .lib bundling.

    Tries llvm-lib (LLVM archiver, used with clang-cl) first via PATH,
    then falls back to MSVC lib.exe (Microsoft Librarian) via PATH and
    common Visual Studio installation paths.

    Returns the full path to the tool, or None if neither is available.
    """
    # llvm-lib uses the same /OUT: syntax as MSVC's lib.exe
    exe = shutil.which("llvm-lib")
    if exe:
        return exe
    # Fall back to MSVC lib.exe
    exe = shutil.which("lib.exe")
    if exe:
        return exe
    # Common MSVC installation paths
    program_files = os.environ.get("ProgramFiles", "C:\\Program Files")
    program_files_x86 = os.environ.get(
        "ProgramFiles(x86)", "C:\\Program Files (x86)"
    )
    candidates = [
        f"{program_files_x86}\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Tools\\MSVC",
        f"{program_files_x86}\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\MSVC",
        f"{program_files}\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Tools\\MSVC",
        f"{program_files}\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\MSVC",
        f"{program_files_x86}\\Microsoft Visual Studio\\2019\\BuildTools\\VC\\Tools\\MSVC",
        f"{program_files_x86}\\Microsoft Visual Studio\\2019\\Community\\VC\\Tools\\MSVC",
    ]
    for base in candidates:
        if os.path.isdir(base):
            try:
                for entry in os.listdir(base):
                    lib_path = os.path.join(
                        base, entry, "bin", "Hostx64", "x64", "lib.exe"
                    )
                    if os.path.isfile(lib_path):
                        return lib_path
            except OSError:
                continue
    return None


def sync_runtime_library(
    context: SyncContext,
    *,
    target_triple: str | None = None,
    skip_build: bool = False,
) -> None:
    """Build the native C runtime as a static library and install it per-target.

    The library is compiled with -ffunction-sections -fdata-sections so the
    linker can dead-strip unused functions when --gc-sections is enabled.

    On POSIX, Bazel cc_library produces a .a file directly at a known path.
    On Windows, cc_library produces .obj files in _objs/; we
    archive them into a .lib using llvm-lib (clang-cl) or lib.exe (MSVC).
    The .bazelrc.local file on this machine sets --compiler=clang-cl.

    Args:
        context: Sync context with repo paths and config.
        target_triple: Target triple for the runtime library
            (e.g. ``"x86_64-unknown-linux-gnu"``).
            Defaults to the host triple if not provided.
        skip_build: If True, skip the Bazel build step (use existing artifacts).
    """
    effective_triple = target_triple if target_triple else get_host_triple()
    runtime_lib_name = get_runtime_lib_name(effective_triple)

    if not skip_build:
        env = bazel_env(context)
        build_args = [
            "bazel",
            "build",
            "//runtime:native_core_runtime",
            f"--config={context.bazel_config}",
        ]
        result = run_live(build_args, context.repo_root, env)
        if result.exit_code != 0:
            raise SyncError(
                f"bazel build //runtime:native_core_runtime failed with exit code {result.exit_code}. "
                f"The runtime library is required for Kain executables to link."
            )

    bazel_bin = _resolve_bazel_output_dir(context)
    if not bazel_bin:
        raise SyncError(
            "unable to resolve bazel-bin; the runtime library cannot be located. "
            "Ensure Bazel has completed a build of //runtime:native_core_runtime."
        )

    # Install to ~/.kain/lib/<target-triple>/<runtime_lib_name>
    install_dir = context.repo_kain_home / "lib" / effective_triple
    install_dir.mkdir(parents=True, exist_ok=True)

    is_windows = platform.system().lower() == "windows"
    if is_windows:
        # Windows: cc_library produces .obj files in _objs/ — archive into .lib
        # Works with both clang-cl (llvm-lib) and MSVC (lib.exe).
        obj_dir = bazel_bin / "runtime" / "_objs" / "native_core_runtime_c"
        obj_files = sorted(obj_dir.glob("*.obj")) if obj_dir.exists() else []
        if not obj_files:
            raise SyncError(
                f"no .obj files found at {obj_dir}. "
                f"Ensure bazel build //runtime:native_core_runtime completed successfully."
            )
        dst_path = install_dir / runtime_lib_name
        temp_path = dst_path.with_name(f"{dst_path.name}.tmp.{os.getpid()}")
        lib_exe = _find_lib_exe()
        if lib_exe is None:
            raise SyncError(
                "llvm-lib or lib.exe not found on PATH or in standard installation paths. "
                "Install LLVM (for llvm-lib) or Visual Studio Build Tools (for lib.exe), "
                "or run from a Developer Command Prompt."
            )
        cmd = [lib_exe, "/OUT:" + str(temp_path)] + [str(f) for f in obj_files]
        result = run_capture(cmd, context.repo_root)
        if result.exit_code != 0:
            raise SyncError(
                f"{os.path.basename(lib_exe)} failed with exit code {result.exit_code}: {result.output_text[:200]}"
            )
        os.replace(temp_path, dst_path)
        size_str = f"{sum(f.stat().st_size for f in obj_files)} bytes (archived)"
    else:
        # POSIX: cc_library produces libnative_core_runtime.a directly
        src_path = bazel_bin / "runtime" / "libnative_core_runtime.a"
        if not src_path.exists():
            raise SyncError(
                f"static library not found at {src_path}. "
                f"Ensure bazel build //runtime:native_core_runtime completed successfully."
            )
        dst_path = install_dir / runtime_lib_name
        copy_file_atomic(src_path, dst_path)
        size_str = f"{src_path.stat().st_size} bytes"

    print(f"  [runtime] {dst_path} ({size_str})")

    return effective_triple


def sync_launchers(
    context: SyncContext,
    *,
    skip_build: bool = False,
    managed_sync: bool = False,
    persist_user_env: bool = False,
    target_triple: str | None = None,
) -> int:
    if not managed_sync:
        print("=" * 76)
        print("Syncing KAIN Bazel launchers")
        print("=" * 76)
        print(f"Repo Root : {context.repo_root}")
        print(f"Launcher  : {context.launcher_dir}")
        print(f"Config    : {context.bazel_config}")
        print(f"State Root: {context.state_root}")
        print(f"Binaries  : {', '.join(context.binary_names)}")
        if context.clang_path:
            print(f"clang     : {context.clang_path}")
        if context.python_path:
            print(f"python    : {context.python_path}")
        bash_path = resolve_bash_path()
        if bash_path:
            print(f"BAZEL_SH  : {bash_path}")
        print()

    installed_triple = sync_runtime_library(
        context, target_triple=target_triple, skip_build=skip_build
    )

    for attempt in range(1, MAX_SYNC_STAMP_ATTEMPTS + 1):
        source_data = source_stamp_data(
            context.repo_root,
            context.source_watch_paths,
            context.source_filesystem_watch_paths,
        )
        current_source_stamp = str(source_data.get("stamp", ""))
        for binary_name in context.binary_names:
            launch_path = context.launcher_dir / (
                binary_name + (".exe" if platform.system().lower() == "windows" else "")
            )
            launch_binary(
                context,
                binary_name,
                (),
                skip_build=skip_build,
                update_stamp_only=True,
                launcher_path=launch_path,
                source_data_override=source_data,
            )
            if not managed_sync:
                print(f"  [stamp] {binary_name}")

        final_source_data = source_stamp_data(
            context.repo_root,
            context.source_watch_paths,
            context.source_filesystem_watch_paths,
        )
        final_source_stamp = str(final_source_data.get("stamp", ""))
        if final_source_stamp == current_source_stamp:
            break
        if attempt >= MAX_SYNC_STAMP_ATTEMPTS:
            raise SyncError(
                "launcher source stamp kept changing during sync; rerun after the watched source settles"
            )
        if not managed_sync:
            print(
                "  [resync] source stamp changed during sync "
                f"({current_source_stamp} -> {final_source_stamp}); rerunning stamp pass..."
            )

    shim_path = build_launcher_shim(context)
    pending_replacements = install_launcher_files(context, shim_path)
    if persist_user_env:
        persist_windows_user_env(context)

    # ── Update install_manifest.json ───────────────────────────────────────
    _update_install_manifest(context, installed_triple, managed=managed_sync)

    if not managed_sync:
        print(f"  [shim] {shim_path}")
        print(f"  [installed] {context.launcher_dir}")
        print(f"  [stamp] {context.stamp_path}")
        for pending in pending_replacements:
            print(f"  [pending] {pending}")
    return 0


def _update_install_manifest(
    context: SyncContext,
    installed_triple: str,
    managed: bool = False,
) -> None:
    """Update ``install_manifest.json`` with host triple and installed targets.

    Creates or upgrades the manifest to schema_version 2, recording which
    triples have runtime libraries installed and preserving all existing
    environment metadata.
    """
    manifest_path = context.repo_kain_home / "install_manifest.json"
    existing = read_json(manifest_path)

    host_triple = get_host_triple()
    installed_targets: list[str] = list(
        existing.get("installed_targets", [])
    )
    if installed_triple not in installed_targets:
        installed_targets.append(installed_triple)
        installed_targets.sort()

    binaries: list[str] = list(
        existing.get("binaries", [])
    )
    # Auto-discover binaries from the launcher dir
    for binary_name in context.binary_names:
        exe_name = binary_name + (".exe" if platform.system().lower() == "windows" else "")
        binary_path = context.launcher_dir / exe_name
        if binary_path.exists() and str(binary_path) not in binaries:
            binaries.append(str(binary_path))

    manifest: dict[str, object] = {
        "schema_version": 2,
        "host_platform": platform.system().lower(),
        "host_triple": host_triple,
        "installed_targets": installed_targets,
        "kain_home": str(context.repo_kain_home),
        "bin_dir": str(context.launcher_dir),
        "stdlib_dir": str(context.repo_kain_home / "stdlib"),
        "runtime_dir": str(context.repo_kain_home / "lib"),
        "toolchain_bin": str(
            context.repo_kain_home / "toolchain" / "llvm" / "bin"
        ),
        "bundled_clang": str(context.clang_path) if context.clang_path else "",
        "binaries": sorted(set(binaries)),
        "resource_env": {
            "KAIN_HOME": str(context.repo_kain_home),
            "KAIN_STDLIB_PATH": str(context.repo_kain_home / "stdlib"),
            "KAIN_RUNTIME_MANIFEST_PATH": str(
                context.repo_kain_home / "runtime" / "native_core_runtime.toml"
            ),
            "KAIN_CLANG_PATH": str(context.clang_path) if context.clang_path else "",
        },
    }

    write_json_atomic(manifest_path, manifest)
    if not managed:
        print(f"  [manifest] {manifest_path} (schema v2, {len(installed_targets)} target(s))")


def launcher_status(context: SyncContext, *, json_output: bool = False) -> int:
    existing = read_json(context.stamp_path)
    source_data = source_stamp_data(
        context.repo_root,
        context.source_watch_paths,
        context.source_filesystem_watch_paths,
    )
    current_source_stamp = str(source_data.get("stamp", ""))
    binaries: dict[str, object] = {}
    for binary_name in context.binary_names:
        entry = binary_entry(existing, binary_name)
        decision = choose_build_action(
            existing,
            binary_name,
            current_source_stamp,
            context.bazel_config,
            skip_build=False,
        )
        path = stamped_binary_path(existing, binary_name)
        binaries[binary_name] = {
            "build_required": decision.should_build,
            "build_reason": decision.reason,
            "bazel_binary_name": str(entry.get("bazel_binary_name", binary_name))
            if entry
            else binary_name,
            "stamped_source": str(entry.get("source_stamp", "")) if entry else "",
            "path": str(path) if path else "",
            "path_exists": bool(path and path.exists()),
        }

    payload = {
        "repo_root": str(context.repo_root),
        "bazel_config": context.bazel_config,
        "stamp_path": str(context.stamp_path),
        "launcher_dir": str(context.launcher_dir),
        "source_stamp": current_source_stamp,
        "source_dirty_count": int(source_data.get("dirty_count", 0)),
        "binaries": binaries,
    }
    if json_output:
        print(json.dumps(payload, indent=2, sort_keys=True))
        return 0

    print(f"Repo Root : {payload['repo_root']}")
    print(f"Launcher  : {payload['launcher_dir']}")
    print(f"Config    : {payload['bazel_config']}")
    print(f"Stamp     : {payload['stamp_path']}")
    print(
        f"Source    : {payload['source_stamp']} dirty={payload['source_dirty_count']}"
    )
    for binary_name, raw in binaries.items():
        entry = raw if isinstance(raw, dict) else {}
        required = "yes" if entry.get("build_required") else "no"
        print(
            f"{binary_name:8} rebuild={required:3} reason={entry.get('build_reason')} path={entry.get('path')}"
        )
    return 0


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Boring, testable Bazel-backed Kain launcher control plane."
    )
    parser.add_argument("--repo-root", help="Repository root override.")
    subparsers = parser.add_subparsers(dest="command", required=True)

    launch = subparsers.add_parser(
        "launch", help="Build/stage/run one Bazel CLI binary."
    )
    launch.add_argument("--binary", required=True, choices=DEFAULT_BINARY_NAMES)
    launch.add_argument("--bazel-config", default=None)
    launch.add_argument("--launcher-path", default=None)
    launch.add_argument("--launcher-dir", default=None)
    launch.add_argument("--skip-build", action="store_true")
    launch.add_argument("--update-stamp-only", action="store_true")
    launch.add_argument("forward_args", nargs=argparse.REMAINDER)

    sync = subparsers.add_parser(
        "sync", help="Build/stage/install all managed launcher shims."
    )
    sync.add_argument("--bazel-config", default=None)
    sync.add_argument("--launcher-dir", default=None)
    sync.add_argument("--skip-build", action="store_true")
    sync.add_argument("--managed-sync", action="store_true")
    sync.add_argument("--persist-user-env", action="store_true")
    sync.add_argument(
        "--target-triple",
        default=None,
        help="Target triple for runtime library installation "
        "(e.g. x86_64-unknown-linux-gnu). Defaults to host triple.",
    )

    status = subparsers.add_parser(
        "status", help="Explain launcher freshness without building."
    )
    status.add_argument("--bazel-config", default=None)
    status.add_argument("--launcher-dir", default=None)
    status.add_argument("--json", action="store_true")

    prune = subparsers.add_parser(
        "prune-storage",
        help="Trim Bazel storage back under the configured cap.",
    )
    prune.add_argument(
        "--dry-run",
        action="store_true",
        help="Report what would be removed without deleting anything.",
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        repo_root = resolve_repo_root(args.repo_root)
        if args.command == "prune-storage":
            return report_bazel_storage_prune(repo_root, dry_run=args.dry_run)
        context = resolve_sync_context(
            repo_root,
            bazel_config=getattr(args, "bazel_config", None),
            launcher_dir=getattr(args, "launcher_dir", None),
        )
        if args.command == "launch":
            return launch_binary(
                context,
                args.binary,
                args.forward_args,
                skip_build=args.skip_build,
                update_stamp_only=args.update_stamp_only,
                launcher_path=Path(args.launcher_path).resolve()
                if args.launcher_path
                else None,
            )
        if args.command == "sync":
            return sync_launchers(
                context,
                skip_build=args.skip_build,
                managed_sync=args.managed_sync,
                persist_user_env=args.persist_user_env,
                target_triple=args.target_triple,
            )
        if args.command == "status":
            return launcher_status(context, json_output=args.json)
        raise SyncError(f"unknown command {args.command}")
    except SyncError as error:
        print(f"[kain] {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
