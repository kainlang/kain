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
    toolchain_bin: Path
    generated_dir: Path
    cargo_bin_dir: Path
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
    "clang*.exe",
    "clang*.dll",
    "lld*.exe",
    "lld*.dll",
    "llvm*.exe",
    "llvm*.dll",
    "libclang*.dll",
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
            "Install Kain from this repo, bundle clang into toolchain/llvm/bin, "
            "and emit shell activation scripts for the current checkout."
        )
    )
    parser.add_argument("--skip-build", action="store_true", help="Skip `cargo build --release -p cli`.")
    parser.add_argument(
        "--skip-binary-install",
        action="store_true",
        help="Skip copying `kain` and `kn` into the cargo bin directory.",
    )
    parser.add_argument("--dry-run", action="store_true", help="Print planned actions without mutating the repo.")
    parser.add_argument("--clang-path", type=Path, help="Use this clang executable instead of auto-discovery.")
    parser.add_argument(
        "--python-path",
        type=Path,
        help="Optional explicit Python executable for PyO3-backed builds and activation scripts.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    repo_root = Path(__file__).resolve().parent
    system_name = platform.system().lower()
    context = InstallContext(
        repo_root=repo_root,
        toolchain_bin=repo_root / "toolchain" / "llvm" / "bin",
        generated_dir=repo_root / "generated",
        cargo_bin_dir=resolve_cargo_bin_dir(),
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
        install_cli_binaries(context)
        resource_map = build_resource_map(context, bundled_clang_path, python_path)
        write_activation_scripts(context, resource_map)
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
    print(f"Platform       : {context.system_name}")
    print(f"Cargo bin dir  : {context.cargo_bin_dir}")
    print(f"Toolchain bin  : {context.toolchain_bin}")
    print()


def normalize_existing_path(path: Path | None) -> Path | None:
    if path is None:
        return None
    resolved = path.expanduser()
    return resolved if resolved.exists() else None


def resolve_cargo_bin_dir() -> Path:
    cargo_home = os.environ.get("CARGO_HOME")
    if cargo_home:
        return Path(cargo_home).expanduser() / "bin"
    return Path.home() / ".cargo" / "bin"


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

    print("[info] clang not found in repo toolchain or common system locations")
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
    manifest_path = context.repo_root / "toolchain" / "llvm" / "kain_bundle_manifest.json"
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

    current_python = Path(sys.executable).resolve()
    if current_python.exists():
        return current_python

    for name in ("python3.12", "python3", "python"):
        resolved = shutil.which(name)
        if resolved:
            return Path(resolved).resolve()
    return None


def build_cli(context: InstallContext, bundled_clang_path: Path, python_path: Path | None) -> None:
    if context.skip_build:
        print("[skip] cargo build --release -p cli")
        return
    env_overrides = {"KAIN_CLANG_PATH": str(bundled_clang_path)}
    if python_path is not None:
        env_overrides["PYO3_PYTHON"] = str(python_path)
    run_command(["cargo", "build", "--release", "-p", "cli"], context=context, env_overrides=env_overrides)


def install_cli_binaries(context: InstallContext) -> None:
    if context.skip_binary_install:
        print("[skip] install kain and kn binaries")
        return

    install_dir = context.cargo_bin_dir
    if not context.dry_run:
        install_dir.mkdir(parents=True, exist_ok=True)

    extension = ".exe" if context.is_windows else ""
    for name in ("kain", "kn"):
        source = context.repo_root / "target" / "release" / f"{name}{extension}"
        if not source.exists():
            raise RuntimeError(
                f"Expected built binary at {source}. Re-run without --skip-build after cargo succeeds."
            )
        destination = install_dir / f"{name}{extension}"
        print(f"[install] {source} -> {destination}")
        if not context.dry_run:
            shutil.copy2(source, destination)


def build_resource_map(
    context: InstallContext,
    bundled_clang_path: Path,
    python_path: Path | None,
) -> dict[str, str]:
    resource_map = {
        "KAIN_STDLIB_PATH": str(context.repo_root / "stdlib"),
        "KAIN_RUNTIME_C_PATH": str(context.repo_root / "runtime" / "kain_runtime.c"),
        "KAIN_RUNTIME_MANIFEST_PATH": str(context.repo_root / "runtime" / "native_core_runtime.toml"),
        "KAIN_CLANG_PATH": str(bundled_clang_path),
    }
    if python_path is not None:
        resource_map["PYO3_PYTHON"] = str(python_path)
    return resource_map


def write_activation_scripts(context: InstallContext, resource_map: dict[str, str]) -> None:
    if not context.dry_run:
        context.generated_dir.mkdir(parents=True, exist_ok=True)
    shell_path = context.generated_dir / "kain-env.sh"
    powershell_path = context.generated_dir / "kain-env.ps1"

    unix_lines = ["#!/usr/bin/env bash", f'export PATH="{context.cargo_bin_dir}:{context.toolchain_bin}:$PATH"']
    unix_lines.extend(
        f'export {name}="{shell_escape_path(value)}"' for name, value in resource_map.items()
    )

    path_separator = ";" if context.is_windows else os.pathsep
    ps_lines = [f'$env:PATH = "{context.cargo_bin_dir}{path_separator}{context.toolchain_bin}{path_separator}$env:PATH"']
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
    print(f"Bundled clang : {bundled_clang_path}")
    print(f"Python        : {python_path if python_path is not None else 'not set'}")
    print(f"Cargo bin     : {context.cargo_bin_dir}")
    print()
    for name, value in resource_map.items():
        print(f"{name}={value}")
    print()
    print("Next steps:")
    if context.is_windows:
        print(f'  PowerShell: . "{powershell_activation}"')
        print(f"  Then run : kain doctor")
    else:
        print(f"  Bash/Zsh : source {shell_activation}")
        print("  Then run : kain doctor")
    print()


if __name__ == "__main__":
    raise SystemExit(main())
