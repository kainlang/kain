#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import platform
import shlex
import shutil
import subprocess
import sys
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Iterable


@dataclass(frozen=True)
class PackageManagerPlan:
    command: str
    install_args: tuple[str, ...]
    update_args: tuple[str, ...] = ()
    requires_sudo: bool = True


@dataclass(frozen=True)
class InstallContext:
    repo_root: Path
    kain_home: Path
    kain_bin_dir: Path
    stdlib_dir: Path
    runtime_dir: Path
    toolchain_bin: Path
    generated_dir: Path
    packages_dir: Path
    tooling_dir: Path
    cache_dir: Path
    install_manifest_path: Path
    system_name: str
    is_windows: bool
    is_macos: bool
    is_linux: bool
    dry_run: bool
    skip_build: bool
    skip_binary_install: bool
    explicit_clang_path: Path | None
    explicit_python_path: Path | None


WINDOWS_BUNDLE_PATTERNS = (
    "clang.exe",
    "clang++.exe",
    "lld.exe",
    "lld-link.exe",
    "llvm-ar.exe",
    "llvm-lib.exe",
    "llvm-ranlib.exe",
    "llvm-mt.exe",
    "llvm-rc.exe",
    "LLVM-C.dll",
    "libclang.dll",
    "libomp*.dll",
    "zlib1.dll",
)

UNIX_COMPANION_TOOLS = (
    "clang",
    "clang++",
    "llvm-ar",
    "llvm-ranlib",
    "lld",
    "ld.lld",
)

LINUX_PACKAGE_MANAGERS = (
    PackageManagerPlan("apt-get", ("install", "-y", "clang", "lld", "llvm"), ("update",)),
    PackageManagerPlan("dnf", ("install", "-y", "clang", "lld", "llvm")),
    PackageManagerPlan("yum", ("install", "-y", "clang", "lld", "llvm")),
    PackageManagerPlan("pacman", ("-Sy", "--noconfirm", "clang", "lld", "llvm"), requires_sudo=True),
    PackageManagerPlan("zypper", ("install", "-y", "clang", "lld", "llvm")),
    PackageManagerPlan(
        "apk",
        ("add", "clang", "llvm", "lld", "build-base", "musl-dev"),
        requires_sudo=False,
    ),
)

WINDOWS_PACKAGE_MANAGERS = (
    PackageManagerPlan(
        "winget",
        (
            "install",
            "--id",
            "LLVM.LLVM",
            "-e",
            "--accept-package-agreements",
            "--accept-source-agreements",
        ),
        requires_sudo=False,
    ),
    PackageManagerPlan("choco", ("install", "llvm", "-y"), requires_sudo=False),
    PackageManagerPlan("scoop", ("install", "llvm"), requires_sudo=False),
)

MACOS_PACKAGE_MANAGERS = (
    PackageManagerPlan("brew", ("install", "llvm"), requires_sudo=False),
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Install Kain from this repo into a self-contained Kain home bundle "
            "with stdlib, runtime assets, and a bundled LLVM toolchain."
        )
    )
    parser.add_argument("--skip-build", action="store_true", help="Skip `cargo build --release -p cli`.")
    parser.add_argument(
        "--skip-binary-install",
        action="store_true",
        help="Skip copying CLI binaries into the managed Kain user bin.",
    )
    parser.add_argument("--dry-run", action="store_true", help="Print planned actions without mutating the repo.")
    parser.add_argument("--clang-path", type=Path, help="Use this clang executable instead of auto-discovery.")
    parser.add_argument(
        "--kain-home",
        type=Path,
        help="Install into this Kain home directory instead of the default ~/.kain.",
    )
    parser.add_argument(
        "--python-path",
        type=Path,
        help="Optional explicit Python executable for PyO3-backed builds and activation scripts.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    repo_root = Path(__file__).resolve().parent
    kain_home = resolve_kain_home_dir(args.kain_home)
    system_name = platform.system().lower()
    context = InstallContext(
        repo_root=repo_root,
        kain_home=kain_home,
        kain_bin_dir=kain_home / "bin",
        stdlib_dir=kain_home / "stdlib",
        runtime_dir=kain_home / "runtime",
        toolchain_bin=kain_home / "toolchain" / "llvm" / "bin",
        generated_dir=kain_home / "generated",
        packages_dir=kain_home / "packages",
        tooling_dir=kain_home / "tooling",
        cache_dir=kain_home / "cache",
        install_manifest_path=kain_home / "install_manifest.json",
        system_name=system_name,
        is_windows=system_name == "windows",
        is_macos=system_name == "darwin",
        is_linux=system_name == "linux",
        dry_run=args.dry_run,
        skip_build=args.skip_build,
        skip_binary_install=args.skip_binary_install,
        explicit_clang_path=normalize_existing_path(args.clang_path),
        explicit_python_path=normalize_existing_path(args.python_path),
    )

    print_banner(context)

    try:
        clang_path = resolve_or_install_clang(context)
        bundled_clang_path = bundle_clang_into_repo(context, clang_path)
        python_path = resolve_python_path(context)
        build_cli(context, bundled_clang_path, python_path)
        install_bundle_resources(context)
        installed_binaries = install_cli_binaries(context)
        resource_map = build_resource_map(context, bundled_clang_path, python_path)
        ensure_user_bin_on_path(context)
        write_activation_scripts(context, resource_map)
        write_install_manifest(context, bundled_clang_path, python_path, resource_map, installed_binaries)
        print_summary(context, bundled_clang_path, python_path, resource_map)
        return 0
    except subprocess.CalledProcessError as err:
        print(
            f"[error] command failed with exit code {err.returncode}: {format_command(err.cmd)}",
            file=sys.stderr,
        )
        return err.returncode or 1
    except Exception as err:
        print(f"[error] {err}", file=sys.stderr)
        return 1


def print_banner(context: InstallContext) -> None:
    print("=" * 76)
    print("Kain Universal Installer")
    print("=" * 76)
    print(f"Repo root      : {context.repo_root}")
    print(f"Kain home      : {context.kain_home}")
    print(f"Kain user bin  : {context.kain_bin_dir}")
    print(f"Stdlib dir     : {context.stdlib_dir}")
    print(f"Runtime dir    : {context.runtime_dir}")
    print(f"Platform       : {context.system_name}")
    print(f"Toolchain bin  : {context.toolchain_bin}")
    print()


def normalize_existing_path(path: Path | None) -> Path | None:
    if path is None:
        return None
    resolved = path.expanduser()
    return resolved if resolved.exists() else None


def resolve_kain_home_dir(explicit_home: Path | None) -> Path:
    if explicit_home is not None:
        return explicit_home.expanduser()
    env_home = os.environ.get("KAIN_HOME")
    if env_home:
        return Path(env_home).expanduser()
    return Path.home() / ".kain"


def command_exists(name: str) -> bool:
    return shutil.which(name) is not None


def run_command(
    command: list[str],
    *,
    context: InstallContext,
    cwd: Path | None = None,
    env_overrides: dict[str, str] | None = None,
) -> None:
    print(f"[run] {format_command(command)}")
    if context.dry_run:
        return
    env = os.environ.copy()
    if env_overrides:
        env.update(env_overrides)
    subprocess.run(command, cwd=cwd or context.repo_root, env=env, check=True)


def format_command(command: Iterable[str] | object) -> str:
    if isinstance(command, (list, tuple)):
        return " ".join(shlex.quote(str(part)) for part in command)
    return str(command)


def resolve_or_install_clang(context: InstallContext) -> Path:
    clang_path = resolve_clang_path(context)
    if clang_path is not None:
        print(f"[ok] clang -> {clang_path}")
        return clang_path

    print("[info] clang not found in the managed toolchain or common system locations")
    install_llvm_with_package_manager(context)
    clang_path = resolve_clang_path(context)
    if clang_path is None:
        raise RuntimeError(
            "Unable to resolve clang after package-manager install attempt. "
            "Install LLVM manually, then rerun with --clang-path."
        )
    print(f"[ok] clang -> {clang_path}")
    return clang_path


def resolve_clang_path(context: InstallContext) -> Path | None:
    candidates: list[Path] = []
    if context.explicit_clang_path is not None:
        candidates.append(context.explicit_clang_path)

    env_clang = normalize_existing_path(Path(os.environ["KAIN_CLANG_PATH"])) if os.environ.get("KAIN_CLANG_PATH") else None
    if env_clang is not None:
        candidates.append(env_clang)

    candidates.extend(
        path for path in (
            context.repo_root / "toolchain" / "llvm" / "bin" / ("clang.exe" if context.is_windows else "clang"),
            context.repo_root / "toolchain" / "llvm" / "bin" / "clang",
            context.repo_root / "toolchain" / "llvm" / "bin" / "clang.exe",
        )
        if path.exists()
    )

    which_clang = shutil.which("clang")
    if which_clang:
        candidates.append(Path(which_clang))

    if context.is_windows:
        candidates.extend(
            path
            for path in (
                Path(r"C:\Program Files\LLVM\bin\clang.exe"),
                Path(r"C:\LLVM\bin\clang.exe"),
            )
            if path.exists()
        )
    elif context.is_macos:
        candidates.extend(path for path in macos_clang_candidates() if path.exists())
        xcrun_clang = resolve_xcrun_clang()
        if xcrun_clang is not None:
            candidates.append(xcrun_clang)
    elif context.is_linux:
        candidates.extend(
            path
            for path in (
                Path("/usr/bin/clang"),
                Path("/usr/local/bin/clang"),
                Path("/snap/bin/clang"),
            )
            if path.exists()
        )

    seen: set[Path] = set()
    for candidate in candidates:
        resolved = candidate.expanduser()
        try:
            resolved = resolved.resolve()
        except OSError:
            resolved = resolved
        if resolved in seen:
            continue
        seen.add(resolved)
        if resolved.exists():
            return resolved
    return None


def macos_clang_candidates() -> list[Path]:
    candidates = [
        Path("/opt/homebrew/opt/llvm/bin/clang"),
        Path("/usr/local/opt/llvm/bin/clang"),
    ]
    if command_exists("brew"):
        try:
            prefix = subprocess.check_output(["brew", "--prefix", "llvm"], text=True).strip()
            if prefix:
                candidates.append(Path(prefix) / "bin" / "clang")
        except subprocess.SubprocessError:
            pass
    return candidates


def resolve_xcrun_clang() -> Path | None:
    if not command_exists("xcrun"):
        return None
    try:
        resolved = subprocess.check_output(["xcrun", "--find", "clang"], text=True).strip()
    except subprocess.SubprocessError:
        return None
    if not resolved:
        return None
    path = Path(resolved)
    return path if path.exists() else None


def install_llvm_with_package_manager(context: InstallContext) -> None:
    plans = (
        WINDOWS_PACKAGE_MANAGERS
        if context.is_windows
        else MACOS_PACKAGE_MANAGERS
        if context.is_macos
        else LINUX_PACKAGE_MANAGERS
    )

    for plan in plans:
        if not command_exists(plan.command):
            continue
        prefix = sudo_prefix(context, plan)
        if plan.update_args:
            run_command(prefix + [plan.command, *plan.update_args], context=context)
        run_command(prefix + [plan.command, *plan.install_args], context=context)
        return

    raise RuntimeError(
        f"No supported package manager was found for platform '{context.system_name}'. "
        "Install LLVM manually, then rerun with --clang-path."
    )


def sudo_prefix(context: InstallContext, plan: PackageManagerPlan) -> list[str]:
    if context.is_windows or not plan.requires_sudo:
        return []
    geteuid = getattr(os, "geteuid", None)
    if callable(geteuid) and geteuid() == 0:
        return []
    if command_exists("sudo"):
        return ["sudo"]
    return []


def bundle_clang_into_repo(context: InstallContext, clang_path: Path) -> Path:
    target_bin = context.toolchain_bin
    if not context.dry_run:
        target_bin.mkdir(parents=True, exist_ok=True)

    if context.is_windows:
        bundled_clang = target_bin / "clang.exe"
        if clang_path.resolve() == bundled_clang.resolve() if bundled_clang.exists() else False:
            return bundled_clang
        copied_files = mirror_windows_llvm_bin(context, clang_path.parent, target_bin)
        if bundled_clang not in copied_files:
            raise RuntimeError(
                f"Bundling clang into {target_bin} did not materialize {bundled_clang.name}"
            )
        write_bundle_manifest(context, clang_path, bundled_clang, copied_files, "copy")
        return bundled_clang

    bundled_tools = link_unix_toolchain(context, clang_path.parent, target_bin, clang_path)
    bundled_clang = target_bin / "clang"
    if bundled_clang not in bundled_tools:
        raise RuntimeError(f"Bundling clang into {target_bin} did not materialize clang")
    write_bundle_manifest(context, clang_path, bundled_clang, bundled_tools, "symlink")
    return bundled_clang


def mirror_windows_llvm_bin(context: InstallContext, source_bin: Path, target_bin: Path) -> list[Path]:
    copied_files: list[Path] = []
    seen: set[Path] = set()
    for pattern in WINDOWS_BUNDLE_PATTERNS:
        for source in source_bin.glob(pattern):
            if not source.is_file():
                continue
            resolved = source.resolve()
            if resolved in seen:
                continue
            seen.add(resolved)
            destination = target_bin / source.name
            print(f"[bundle] copy {source} -> {destination}")
            if not context.dry_run:
                shutil.copy2(source, destination)
            copied_files.append(destination)
    return copied_files


def link_unix_toolchain(
    context: InstallContext,
    source_bin: Path,
    target_bin: Path,
    clang_path: Path,
) -> list[Path]:
    bundled_tools: list[Path] = []
    source_map = {"clang": clang_path}
    for tool_name in UNIX_COMPANION_TOOLS:
        if tool_name == "clang":
            continue
        candidate = source_bin / tool_name
        if candidate.exists() and candidate.is_file():
            source_map[tool_name] = candidate

    for tool_name, source in source_map.items():
        destination = target_bin / tool_name
        link_or_copy(context, source, destination)
        bundled_tools.append(destination)
    return bundled_tools


def link_or_copy(context: InstallContext, source: Path, destination: Path) -> None:
    print(f"[bundle] link {source} -> {destination}")
    if context.dry_run:
        return
    if destination.exists() or destination.is_symlink():
        destination.unlink()
    try:
        destination.symlink_to(source)
    except OSError:
        shutil.copy2(source, destination)


def write_bundle_manifest(
    context: InstallContext,
    source_clang: Path,
    bundled_clang: Path,
    bundled_files: list[Path],
    strategy: str,
) -> None:
    manifest_path = context.toolchain_bin.parent / "kain_bundle_manifest.json"
    payload = {
        "schema_version": 1,
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "platform": context.system_name,
        "source_clang": str(source_clang),
        "bundled_clang": str(bundled_clang),
        "strategy": strategy,
        "files": [str(path) for path in bundled_files],
    }
    print(f"[write] {manifest_path}")
    if context.dry_run:
        return
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def resolve_python_path(context: InstallContext) -> Path | None:
    if context.explicit_python_path is not None:
        return context.explicit_python_path

    env_python = os.environ.get("PYO3_PYTHON")
    if env_python:
        candidate = normalize_existing_path(Path(env_python))
        if candidate is not None:
            return candidate

    def probe(candidate: Path | None) -> Path | None:
        if candidate is None:
            return None
        resolved = normalize_existing_path(candidate)
        if resolved is None:
            return None
        if "windowsapps" in str(resolved).lower() and resolved.name.lower() == "python.exe":
            return None
        try:
            completed = subprocess.run(
                [str(resolved), "-c", "import sys; print(f'{sys.version_info[0]}.{sys.version_info[1]}'); print(sys.executable)"],
                check=True,
                capture_output=True,
                text=True,
            )
        except Exception:
            return None
        lines = [line.strip() for line in completed.stdout.splitlines() if line.strip()]
        if len(lines) < 2:
            return None
        major_minor = lines[0].split(".", 1)
        if len(major_minor) != 2:
            return None
        try:
            major = int(major_minor[0])
            minor = int(major_minor[1])
        except ValueError:
            return None
        if major != 3 or minor < 10:
            return None
        return normalize_existing_path(Path(lines[1]))

    for env_key in ("VIRTUAL_ENV", "CONDA_PREFIX"):
        root = os.environ.get(env_key)
        if not root:
            continue
        for suffix in ("Scripts/python.exe", "bin/python", "python.exe"):
            resolved = probe(Path(root) / suffix)
            if resolved is not None:
                return resolved

    current_python = probe(Path(sys.executable).resolve())
    if current_python is not None:
        return current_python

    for name in ("python", "python3"):
        resolved_name = shutil.which(name)
        if resolved_name:
            resolved = probe(Path(resolved_name))
            if resolved is not None:
                return resolved

    py_launcher = shutil.which("py")
    if py_launcher:
        try:
            completed = subprocess.run(
                [py_launcher, "-c", "import sys; print(sys.executable)"],
                check=True,
                capture_output=True,
                text=True,
            )
            resolved = probe(Path(completed.stdout.strip()))
            if resolved is not None:
                return resolved
        except Exception:
            pass

        for minor in (14, 13, 12, 11, 10):
            try:
                completed = subprocess.run(
                    [py_launcher, f"-3.{minor}", "-c", "import sys; print(sys.executable)"],
                    check=True,
                    capture_output=True,
                    text=True,
                )
                resolved = probe(Path(completed.stdout.strip()))
                if resolved is not None:
                    return resolved
            except Exception:
                pass

    if context.is_windows:
        for minor in (14, 13, 12, 11, 10):
            compact = f"3{minor}"
            for raw in (
                Path(os.environ.get("LOCALAPPDATA", "")) / "Programs" / "Python" / f"Python{compact}" / "python.exe",
                Path.home() / "AppData" / "Local" / "Programs" / "Python" / f"Python{compact}" / "python.exe",
                Path(f"C:/Python{compact}/python.exe"),
            ):
                resolved = probe(raw)
                if resolved is not None:
                    return resolved

    return None


def build_cli(context: InstallContext, bundled_clang_path: Path, python_path: Path | None) -> None:
    if context.skip_build:
        print("[skip] cargo build --release -p cli")
        return
    env_overrides = {"KAIN_CLANG_PATH": str(bundled_clang_path)}
    if python_path is not None:
        env_overrides["PYO3_PYTHON"] = str(python_path)
    run_command(["cargo", "build", "--release", "-p", "cli"], context=context, env_overrides=env_overrides)


def install_bundle_resources(context: InstallContext) -> None:
    ensure_standard_home_directories(context)
    sync_directory(context, context.repo_root / "stdlib", context.stdlib_dir)
    sync_directory(context, context.repo_root / "runtime", context.runtime_dir)


def ensure_standard_home_directories(context: InstallContext) -> None:
    for path in (
        context.kain_home,
        context.kain_bin_dir,
        context.generated_dir,
        context.packages_dir,
        context.tooling_dir,
        context.cache_dir,
        context.toolchain_bin,
    ):
        print(f"[mkdir] {path}")
        if not context.dry_run:
            path.mkdir(parents=True, exist_ok=True)


def sync_directory(context: InstallContext, source: Path, destination: Path) -> None:
    print(f"[sync] {source} -> {destination}")
    if context.dry_run:
        return
    if destination.exists():
        shutil.rmtree(destination)
    shutil.copytree(source, destination)


def install_cli_binaries(context: InstallContext) -> list[Path]:
    if context.skip_binary_install:
        print("[skip] install kain, kn, and blade binaries")
        return []

    install_dir = context.kain_bin_dir
    if not context.dry_run:
        install_dir.mkdir(parents=True, exist_ok=True)

    target_dir = context.repo_root / "target"
    try:
        res = subprocess.run(
            ["cargo", "metadata", "--no-deps", "--format-version", "1"],
            cwd=context.repo_root,
            capture_output=True,
            text=True,
            check=True,
        )
        metadata = json.loads(res.stdout)
        if "target_directory" in metadata:
            target_dir = Path(metadata["target_directory"])
    except Exception:
        pass

    installed_binaries: list[Path] = []
    extension = ".exe" if context.is_windows else ""
    for name in ("kain", "kn", "blade"):
        source = target_dir / "release" / f"{name}{extension}"
        if not source.exists():
            raise RuntimeError(
                f"Expected built binary at {source}. Re-run without --skip-build after cargo succeeds."
            )
        destination = install_dir / f"{name}{extension}"
        print(f"[install] {source} -> {destination}")
        if not context.dry_run:
            shutil.copy2(source, destination)
        installed_binaries.append(destination)
    return installed_binaries


def build_resource_map(
    context: InstallContext,
    bundled_clang_path: Path,
    python_path: Path | None,
) -> dict[str, str]:
    resource_map = {
        "KAIN_HOME": str(context.kain_home),
        "KAIN_STDLIB_PATH": str(context.stdlib_dir),
        "KAIN_RUNTIME_C_PATH": str(context.runtime_dir / "runtime.c"),
        "KAIN_RUNTIME_MANIFEST_PATH": str(context.runtime_dir / "native_core_runtime.toml"),
        "KAIN_CLANG_PATH": str(bundled_clang_path),
    }
    if python_path is not None:
        resource_map["PYO3_PYTHON"] = str(python_path)
    return resource_map


def ensure_user_bin_on_path(context: InstallContext) -> None:
    if not context.is_windows:
        return
    update_windows_user_path(context, context.kain_bin_dir)


def update_windows_user_path(context: InstallContext, entry: Path) -> None:
    try:
        import ctypes
        import winreg
    except ImportError:
        print(f"[warn] unable to import winreg; add {entry} to PATH manually")
        return

    entry_text = str(entry)
    if context.dry_run:
        print(f"[path] add {entry} to HKCU\\Environment\\Path")
        return

    with winreg.OpenKey(
        winreg.HKEY_CURRENT_USER,
        "Environment",
        0,
        winreg.KEY_READ | winreg.KEY_WRITE,
    ) as key:
        try:
            current_value, value_type = winreg.QueryValueEx(key, "Path")
        except FileNotFoundError:
            current_value, value_type = "", winreg.REG_EXPAND_SZ

        path_entries = [part.strip() for part in str(current_value).split(";") if part.strip()]
        normalized_entry = os.path.normcase(os.path.normpath(entry_text))
        if any(os.path.normcase(os.path.normpath(part)) == normalized_entry for part in path_entries):
            print(f"[path] {entry} already present in HKCU\\Environment\\Path")
            return

        updated_entries = path_entries + [entry_text]
        updated_value = ";".join(updated_entries)
        print(f"[path] add {entry} to HKCU\\Environment\\Path")
        winreg.SetValueEx(
            key,
            "Path",
            0,
            value_type if value_type in (winreg.REG_SZ, winreg.REG_EXPAND_SZ) else winreg.REG_EXPAND_SZ,
            updated_value,
        )

    broadcast_environment_refresh(ctypes)


def broadcast_environment_refresh(ctypes_module: object) -> None:
    try:
        hwnd_broadcast = 0xFFFF
        wm_settingchange = 0x001A
        send_timeout_abort = 0x0002
        ctypes_module.windll.user32.SendMessageTimeoutW(
            hwnd_broadcast,
            wm_settingchange,
            0,
            "Environment",
            send_timeout_abort,
            5000,
            None,
        )
    except Exception:
        return


def write_activation_scripts(context: InstallContext, resource_map: dict[str, str]) -> None:
    if not context.dry_run:
        context.generated_dir.mkdir(parents=True, exist_ok=True)
    shell_path = context.generated_dir / "kain-env.sh"
    powershell_path = context.generated_dir / "kain-env.ps1"

    unix_lines = ["#!/usr/bin/env bash", f'export PATH="{context.kain_bin_dir}:$PATH"']
    unix_lines.extend(
        f'export {name}="{shell_escape_path(value)}"' for name, value in resource_map.items()
    )

    path_separator = ";" if context.is_windows else os.pathsep
    ps_lines = [f'$env:PATH = "{context.kain_bin_dir}{path_separator}$env:PATH"']
    ps_lines.extend(f'$env:{name} = "{value}"' for name, value in resource_map.items())

    write_text_file(context, shell_path, "\n".join(unix_lines) + "\n", make_executable=True)
    write_text_file(context, powershell_path, "\n".join(ps_lines) + "\n")


def shell_escape_path(value: str) -> str:
    return value.replace("\\", "\\\\").replace('"', '\\"')


def write_text_file(
    context: InstallContext,
    path: Path,
    content: str,
    *,
    make_executable: bool = False,
) -> None:
    print(f"[write] {path}")
    if context.dry_run:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")
    if make_executable and not context.is_windows:
        path.chmod(path.stat().st_mode | 0o111)


def write_install_manifest(
    context: InstallContext,
    bundled_clang_path: Path,
    python_path: Path | None,
    resource_map: dict[str, str],
    installed_binaries: list[Path],
) -> None:
    payload = {
        "schema_version": 1,
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "platform": context.system_name,
        "repo_root": str(context.repo_root),
        "kain_home": str(context.kain_home),
        "bin_dir": str(context.kain_bin_dir),
        "stdlib_dir": str(context.stdlib_dir),
        "runtime_dir": str(context.runtime_dir),
        "toolchain_bin": str(context.toolchain_bin),
        "packages_dir": str(context.packages_dir),
        "tooling_dir": str(context.tooling_dir),
        "cache_dir": str(context.cache_dir),
        "bundled_clang": str(bundled_clang_path),
        "python": str(python_path) if python_path is not None else None,
        "binaries": [str(path) for path in installed_binaries],
        "resource_env": resource_map,
    }
    print(f"[write] {context.install_manifest_path}")
    if context.dry_run:
        return
    context.install_manifest_path.parent.mkdir(parents=True, exist_ok=True)
    context.install_manifest_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def print_summary(
    context: InstallContext,
    bundled_clang_path: Path,
    python_path: Path | None,
    resource_map: dict[str, str],
) -> None:
    shell_activation = context.generated_dir / "kain-env.sh"
    powershell_activation = context.generated_dir / "kain-env.ps1"
    print()
    print("=" * 76)
    print("Kain installer completed")
    print("=" * 76)
    print(f"Kain home     : {context.kain_home}")
    print(f"Kain user bin : {context.kain_bin_dir}")
    print(f"Bundled clang : {bundled_clang_path}")
    print(f"Python        : {python_path if python_path is not None else 'not set'}")
    print(f"Stdlib        : {context.stdlib_dir}")
    print(f"Runtime       : {context.runtime_dir}")
    print(f"Manifest      : {context.install_manifest_path}")
    print()
    for name, value in resource_map.items():
        print(f"{name}={value}")
    print()
    print("Next steps:")
    if context.is_windows:
        print(f'  Current shell: . "{powershell_activation}"')
        print("  New shell    : open a fresh PowerShell or cmd session")
        print("  Then run     : kain doctor")
    else:
        print(f"  Bash/Zsh: source {shell_activation}")
        print("  Then run: kain doctor")
    print()


if __name__ == "__main__":
    raise SystemExit(main())
