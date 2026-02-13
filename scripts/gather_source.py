r"""
Source Folder Gatherer
Walks through a directory tree and copies all Source/ folders into a single output folder.
Useful for extracting C++ source from UE5 projects, plugins, etc.

Usage:
    python gather_source.py <input_dir> <output_dir>
    python gather_source.py "D:\UE5Projects" "M:\Kain-Lang\kain-private\plugin-corpus"
    python gather_source.py <input_dir> <output_dir> --dry-run
"""

import os
import sys
import shutil
import argparse
from pathlib import Path


def find_source_folders(root_dir):
    """Find all folders named 'Source' under root_dir."""
    source_folders = []
    root = Path(root_dir)

    for dirpath, dirnames, filenames in os.walk(root):
        # Skip common junk directories to speed up traversal
        dirnames[:] = [
            d for d in dirnames
            if d not in (
                'Intermediate', 'Saved', 'DerivedDataCache', 'Binaries',
                '.git', '.svn', 'node_modules', '__pycache__',
                '.vs', '.idea', 'Build',
            )
        ]

        for d in dirnames:
            if d == 'Source':
                full_path = Path(dirpath) / d
                # Verify it actually contains .h or .cpp files somewhere
                has_cpp = any(
                    f.suffix in ('.h', '.cpp', '.cs')
                    for f in full_path.rglob('*')
                    if f.is_file()
                )
                if has_cpp:
                    source_folders.append(full_path)

    return source_folders


def derive_unique_name(source_path, root_dir):
    """Derive a unique folder name from the source path's parent context."""
    # Use the parent folder name (usually the plugin/project name)
    parent = source_path.parent.name

    # If parent is generic, go up one more level
    if parent.lower() in ('source', 'src', 'code'):
        parent = source_path.parent.parent.name

    return parent


def gather_sources(input_dir, output_dir, dry_run=False, copy_mode='copy'):
    """Find all Source/ folders and copy them to output_dir."""
    input_path = Path(input_dir)
    output_path = Path(output_dir)

    if not input_path.exists():
        print(f"❌ Input directory not found: {input_dir}")
        sys.exit(1)

    print(f"🔍 Scanning: {input_dir}")
    print(f"📁 Output:   {output_dir}")
    print()

    source_folders = find_source_folders(input_dir)
    print(f"📋 Found {len(source_folders)} Source folders\n")

    if not source_folders:
        print("Nothing to do.")
        return

    # Track names to handle collisions
    used_names = {}
    copied = 0
    skipped = 0

    for src_folder in source_folders:
        name = derive_unique_name(src_folder, input_dir)

        # Handle name collisions by appending a counter
        if name in used_names:
            used_names[name] += 1
            unique_name = f"{name}_{used_names[name]}"
        else:
            used_names[name] = 0
            unique_name = name

        dest = output_path / unique_name / "Source"

        # Count files for display
        file_count = sum(1 for f in src_folder.rglob('*') if f.is_file() and f.suffix in ('.h', '.cpp', '.cs'))

        if dry_run:
            print(f"  [DRY RUN] {src_folder}")
            print(f"         → {dest}  ({file_count} C++ files)")
        else:
            try:
                if dest.exists():
                    print(f"  ⏭️  Skipping (exists): {dest}")
                    skipped += 1
                    continue

                dest.parent.mkdir(parents=True, exist_ok=True)
                shutil.copytree(str(src_folder), str(dest))
                print(f"  ✅ {unique_name} ({file_count} files)")
                copied += 1
            except Exception as e:
                print(f"  ⚠️  Failed: {src_folder} → {e}")
                skipped += 1

    print(f"\n{'=' * 50}")
    if dry_run:
        print(f"📊 Would copy {len(source_folders)} Source folders")
    else:
        print(f"📊 Copied: {copied}  |  Skipped: {skipped}  |  Total found: {len(source_folders)}")
    print(f"{'=' * 50}")


def main():
    parser = argparse.ArgumentParser(
        description='Gather all Source/ folders from a directory tree into one place'
    )
    parser.add_argument('input_dir', help='Root directory to scan for Source/ folders')
    parser.add_argument('output_dir', help='Directory to copy Source folders into')
    parser.add_argument('--dry-run', action='store_true', help='Show what would be copied without copying')
    args = parser.parse_args()

    gather_sources(args.input_dir, args.output_dir, dry_run=args.dry_run)


if __name__ == '__main__':
    main()
