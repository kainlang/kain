#!/usr/bin/env python3
"""
Kain Setup — add Kain to your environment.

Ships inside the Kain distribution archive. Cross-platform:
  - Windows: sets user env vars via PowerShell
  - Linux/macOS: adds to shell config (.profile, .zshrc, .bashrc)

Usage:
    python setup.py                       # auto-detect, add to PATH
    python setup.py --path D:\tools\kain  # custom install path
    python setup.py --uninstall           # remove from environment
    python setup.py -f                    # force re-add even if already installed
    python setup.py --dry-run             # preview without changing anything
    python setup.py --info                # show distribution info
"""

import argparse
import json
import os
import platform
import re
import subprocess
import sys
from pathlib import Path

SYSTEM = platform.system()


# ── Detect distribution root ───────────────────────────────────────────────
def get_kain_home():
    """Find Kain distribution root (parent of this script)."""
    script = Path(__file__).resolve()
    if script.name == "setup.py":
        return script.parent
    for parent in script.parents:
        if (parent / "bin" / "kain.exe").exists() or (parent / "install_manifest.json").exists():
            return parent
    return script.parent


# ── Shell config (Linux/macOS) ─────────────────────────────────────────────
def shell_rc_files():
    """Return shell config files to modify, in priority order."""
    home = Path.home()
    files = []
    for rc in [".profile", ".zshrc", ".bashrc", ".bash_profile", ".config/fish/config.fish"]:
        p = home / rc
        if p.exists():
            files.append(p)
    if not files:
        files.append(home / ".profile")
    return files


def shell_config_lines(kain_home: str):
    """Lines to add to shell config."""
    return [
        "",
        "# Kain",
        f'export KAIN_HOME="{kain_home}"',
        f'export PATH="{kain_home}/bin:$PATH"',
        "",
    ]


def kain_already_in_shell_rc() -> bool:
    """Check if Kain block already exists in any shell config."""
    for rc in shell_rc_files():
        content = rc.read_text()
        if "# Kain" in content and "KAIN_HOME=" in content:
            return True
    return False


def add_to_shell_rc(kain_home: str, force: bool, dry_run: bool):
    """Add Kain exports to shell config, skipping if already present."""
    if not force and kain_already_in_shell_rc():
        print("  ℹ Kain already in shell config (use --force to re-add)")
        return

    lines = shell_config_lines(kain_home)
    for rc_file in shell_rc_files():
        content = rc_file.read_text()
        if "# Kain" in content and "KAIN_HOME=" in content:
            if not dry_run:
                print(f"  ℹ Already in {rc_file.name}, skipping")
            continue
        if dry_run:
            print(f"  Would add to: {rc_file}")
            for line in lines:
                if line:
                    print(f"    + {line}")
            continue
        with open(rc_file, "a") as f:
            f.write("\n".join(lines))
            f.write("\n")
        print(f"  ✓ Added to {rc_file.name}")


def remove_from_shell_rc(dry_run: bool = False):
    """Remove Kain blocks from shell config files."""
    pattern = re.compile(r"\n?# Kain\n.*export KAIN_HOME=.*\n.*export PATH=.*\n?", re.MULTILINE)
    for rc_file in shell_rc_files():
        original = rc_file.read_text()
        cleaned = pattern.sub("", original)
        if cleaned != original:
            if dry_run:
                print(f"  Would remove from: {rc_file}")
                continue
            rc_file.write_text(cleaned)
            print(f"  ✓ Removed from {rc_file.name}")


# ── Windows environment (PowerShell) ───────────────────────────────────────
def windows_env_commands(kain_home: str, system_wide: bool):
    """PowerShell commands to add Kain to PATH."""
    scope = "Machine" if system_wide else "User"
    bin_path = os.path.join(kain_home, "bin")
    return [
        f'[Environment]::SetEnvironmentVariable("KAIN_HOME", r"{kain_home}", "{scope}")',
        f'$path = [Environment]::GetEnvironmentVariable("PATH", "{scope}")',
        f'if ($path -notlike "*{bin_path}*") {{',
        f'    [Environment]::SetEnvironmentVariable("PATH", "$path;{bin_path}", "{scope}")',
        f'    Write-Host "✓ Added {bin_path} to {scope} PATH"',
        f'}} else {{',
        f'    Write-Host "ℹ {bin_path} already in {scope} PATH"',
        f'}}',
    ]


def windows_uninstall_commands(system_wide: bool):
    """PowerShell commands to remove Kain from PATH."""
    scope = "Machine" if system_wide else "User"
    return [
        f'$kainHome = [Environment]::GetEnvironmentVariable("KAIN_HOME", "{scope}")',
        f'if ($kainHome) {{',
        f'    $binPath = "$kainHome\\bin"',
        f'    $path = [Environment]::GetEnvironmentVariable("PATH", "{scope}")',
        f'    $newPath = ($path -split ";" | Where-Object {{ $_ -ne $binPath }}) -join ";"',
        f'    [Environment]::SetEnvironmentVariable("PATH", $newPath, "{scope}")',
        f'    [Environment]::SetEnvironmentVariable("KAIN_HOME", $null, "{scope}")',
        f'    Write-Host "✓ Removed Kain from {scope} PATH"',
        f'}} else {{',
        f'    Write-Host "Kain not configured for {scope} scope"',
        f'}}',
    ]


def run_powershell(commands, dry_run: bool = False):
    """Execute PowerShell script and return success."""
    script = "\n".join(commands)
    if dry_run:
        print("  Would run PowerShell:")
        for cmd in commands:
            print(f"    {cmd}")
        return True
    try:
        result = subprocess.run(
            ["powershell", "-NoProfile", "-Command", script],
            capture_output=True, text=True
        )
        if result.stdout:
            print(result.stdout.strip())
        if result.returncode != 0:
            print(f"  ⚠ PowerShell exited with code {result.returncode}")
            if result.stderr:
                print(f"    {result.stderr.strip()}")
            return False
        return True
    except FileNotFoundError:
        print("  ⚠ PowerShell not found on PATH")
        return False


# ── Commands ────────────────────────────────────────────────────────────────
def cmd_install(kain_home: Path, system_wide: bool, force: bool, dry_run: bool):
    """Add Kain to the user environment."""
    print(f"\n  Kain Setup")
    print(f"  Home: {kain_home}")
    print(f"  Scope: {'System (admin)' if system_wide else 'User'}")
    print(f"  Mode: {'Dry run' if dry_run else 'Apply'}\n")

    if SYSTEM == "Windows":
        commands = windows_env_commands(str(kain_home), system_wide)
        ok = run_powershell(commands, dry_run)
        if ok and not dry_run:
            print("\n  ✅ Kain added to environment")
            print("  ℹ Restart your terminal or log out/in for changes to take effect")
    else:
        add_to_shell_rc(str(kain_home), force, dry_run)
        if not dry_run:
            print("\n  ✅ Kain added to shell config")
            print("  ℹ Restart your shell or: source ~/.profile")


def cmd_uninstall(system_wide: bool, dry_run: bool):
    """Remove Kain from the environment."""
    print(f"\n  Kain Uninstall\n")
    if SYSTEM == "Windows":
        commands = windows_uninstall_commands(system_wide)
        ok = run_powershell(commands, dry_run)
        if ok and not dry_run:
            print("\n  ✅ Kain removed from environment")
    else:
        remove_from_shell_rc(dry_run)
        if not dry_run:
            print("\n  ✅ Kain removed from shell config")


def cmd_info(kain_home: Path):
    """Show distribution info."""
    manifest = kain_home / "install_manifest.json"
    print(f"\n  Kain Distribution Info")
    print(f"  Location: {kain_home}")
    if manifest.exists():
        data = json.loads(manifest.read_text())
        print(f"  Version: {data.get('version', '?')}")
        print(f"  Platform: {data.get('platform', '?')}")
        print(f"  Git: {data.get('git_commit', '?')[:12]}")
    for dir_name in ["bin", "lib", "stdlib"]:
        d = kain_home / dir_name
        if d.exists():
            files = [f for f in d.iterdir() if f.is_file()]
            if files:
                total = sum(f.stat().st_size for f in files)
                print(f"  {dir_name}/: {len(files)} files, {total / 1024 / 1024:.1f} MB")


# ── CLI ────────────────────────────────────────────────────────────────────
def main():
    kain_home = get_kain_home()
    parser = argparse.ArgumentParser(
        description="Setup Kain compiler in your environment",
        epilog="Without arguments, installs with auto-detected settings."
    )
    parser.add_argument("command", nargs="?", default="install",
                        choices=["install", "uninstall", "info"],
                        help="Action (default: install)")
    parser.add_argument("--path", default=str(kain_home),
                        help=f"Kain path (default: {kain_home})")
    parser.add_argument("--system", action="store_true",
                        help="System-wide (admin req. on Windows)")
    parser.add_argument("--dry-run", "-n", action="store_true",
                        help="Preview without changes")
    parser.add_argument("--force", "-f", action="store_true",
                        help="Re-add even if already configured")
    args = parser.parse_args()

    kain_home = Path(args.path).resolve()

    if args.command == "install":
        cmd_install(kain_home, args.system, args.force, args.dry_run)
    elif args.command == "uninstall":
        cmd_uninstall(args.system, args.dry_run)
    elif args.command == "info":
        cmd_info(kain_home)


if __name__ == "__main__":
    main()
