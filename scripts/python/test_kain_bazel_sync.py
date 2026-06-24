#!/usr/bin/env python3
from __future__ import annotations

import json
import os
import subprocess
import tempfile
import unittest
from unittest import mock
from pathlib import Path

import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))

import kain_bazel_sync as sync


def run(args: list[str], cwd: Path) -> None:
    subprocess.run(args, cwd=str(cwd), check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)


class KainBazelSyncTests(unittest.TestCase):
    def test_path_matching_is_directory_boundary_safe(self) -> None:
        self.assertTrue(sync.path_matches_watch_path("crates/core/src/lib.rs", ("crates",)))
        self.assertTrue(sync.path_matches_watch_path("MODULE.bazel", ("MODULE.bazel",)))
        self.assertFalse(sync.path_matches_watch_path("crates2/not-watched.rs", ("crates",)))
        self.assertFalse(sync.path_matches_watch_path("docs/crates/readme.md", ("crates",)))

    def test_source_stamp_changes_for_dirty_watched_file(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            repo = Path(raw)
            run(["git", "init"], repo)
            run(["git", "config", "user.email", "kain@example.invalid"], repo)
            run(["git", "config", "user.name", "Kain Test"], repo)
            watched = repo / "crates" / "demo"
            watched.mkdir(parents=True)
            source = watched / "lib.rs"
            source.write_text("pub fn v() -> i32 { 1 }\n", encoding="utf-8")
            run(["git", "add", "."], repo)
            run(["git", "commit", "-m", "seed"], repo)

            clean = sync.source_stamp_data(repo, ("crates",))
            source.write_text("pub fn v() -> i32 { 2 }\n", encoding="utf-8")
            dirty = sync.source_stamp_data(repo, ("crates",))

            self.assertNotEqual(clean["stamp"], dirty["stamp"])
            self.assertEqual(dirty["dirty_count"], 1)

    def test_skip_build_never_certifies_current_source_on_old_binary(self) -> None:
        previous_stamp = "old-source"
        current_stamp = "new-source"
        existing = {
            "binary_by_name": {
                "kain": {
                    "path": str(Path("old-kain.exe").resolve()),
                    "source_stamp": previous_stamp,
                    "bazel_config": "dev",
                    "exists": True,
                }
            }
        }
        decision = sync.choose_build_action(existing, "kain", current_stamp, "dev", skip_build=True)
        self.assertFalse(decision.should_build)
        self.assertEqual(decision.reason, "skip-build flag set")

        with tempfile.TemporaryDirectory() as raw:
            repo = Path(raw)
            context = sync.SyncContext(
                repo_root=repo,
                policy={},
                sync_policy={},
                state_root=repo / ".kain" / "state",
                stamp_path=repo / ".kain" / "state" / "state" / "kain_sync_stamp.json",
                bazel_config="dev",
                source_watch_paths=("crates",),
                source_filesystem_watch_paths=(),
                runtime_stamp_files=(),
                launcher_dir=repo / ".kain" / "bin",
                binary_names=("kain",),
                repo_kain_home=repo / ".kain",
                repo_kain_config=repo / ".kain" / "config.toml",
                clang_path=None,
                python_path=None,
            )
            payload = sync.merge_stamp_payload(
                existing,
                context=context,
                binary_name="kain",
                bazel_binary_name="kain",
                current_source_stamp=current_stamp,
                source_data={"stamp": current_stamp, "dirty_count": 1, "watch_paths": ("crates",), "filesystem_watch_paths": ()},
                runtime_hash="runtime",
                build_performed=False,
                build_reason=decision.reason,
                active_binary_fingerprint=None,
                bazel_binary_path=None,
                launcher_path=None,
            )
        self.assertEqual(payload["binary_by_name"]["kain"]["source_stamp"], previous_stamp)
        self.assertEqual(payload["source_stamp"], current_stamp)
        self.assertEqual(payload["build_reason"], "skip-build flag set")

    def test_build_required_after_poisoned_skip_build_stamp(self) -> None:
        current_stamp = "new-source"
        poisoned_global_stamp = {
            "source_stamp": current_stamp,
            "binary_by_name": {
                "kain": {
                    "path": str(Path("old-kain.exe").resolve()),
                    "source_stamp": "old-source",
                    "bazel_config": "dev",
                }
            },
        }
        decision = sync.choose_build_action(poisoned_global_stamp, "kain", current_stamp, "dev", skip_build=False)
        self.assertTrue(decision.should_build)
        self.assertEqual(decision.reason, "binary source stamp changed")

    def test_policy_binary_names_ignore_removed_blade(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            repo = Path(raw)
            policy_path = repo / "blades" / "kain-mcp" / "config" / "runtime_policy.json"
            policy_path.parent.mkdir(parents=True)
            policy_path.write_text(
                json.dumps({"launcher_sync": {"launcher_binary_names": ["kain", "kn", "blade"]}}),
                encoding="utf-8",
            )
            context = sync.resolve_sync_context(repo)
            # kn is no longer a separate binary; it is an alias for kain
            self.assertEqual(context.binary_names, ("kain",))

    def test_bazel_output_binary_name_uses_configured_value(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            context = sync.SyncContext(
                repo_root=root,
                policy={},
                sync_policy={},
                state_root=root / ".kain" / "state",
                stamp_path=root / ".kain" / "state" / "state" / "kain_sync_stamp.json",
                bazel_config="dev",
                source_watch_paths=(),
                source_filesystem_watch_paths=(),
                runtime_stamp_files=(),
                launcher_dir=root / ".kain" / "bin",
                binary_names=("kain",),
                repo_kain_home=root / ".kain",
                repo_kain_config=root / ".kain" / "config.toml",
                clang_path=None,
                python_path=None,
            )
            self.assertEqual(sync.bazel_output_binary_name(context, "kain"), "kain")

    def test_runtime_env_forces_repo_state_temp_root(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            context = sync.SyncContext(
                repo_root=root,
                policy={},
                sync_policy={},
                state_root=root / ".kain" / "state",
                stamp_path=root / ".kain" / "state" / "state" / "kain_sync_stamp.json",
                bazel_config="dev",
                source_watch_paths=(),
                source_filesystem_watch_paths=(),
                runtime_stamp_files=(),
                launcher_dir=root / ".kain" / "bin",
                binary_names=("kain",),
                repo_kain_home=root / ".kain",
                repo_kain_config=root / ".kain" / "config.toml",
                clang_path=None,
                python_path=None,
            )
            with mock.patch.dict(os.environ, {"TMP": r"F:\DevTemp", "TEMP": r"F:\DevTemp", "TMPDIR": r"F:\DevTemp"}, clear=False):
                env = sync.runtime_env(context)

            expected_temp = str((context.state_root / "tmp").resolve())
            self.assertEqual(env["TMP"], expected_temp)
            self.assertEqual(env["TEMP"], expected_temp)
            self.assertEqual(env["TMPDIR"], expected_temp)
            self.assertTrue(Path(expected_temp).exists())

    def test_resolve_bazel_storage_limit_bytes_uses_env_overrides(self) -> None:
        with mock.patch.dict(
            os.environ,
            {"KAIN_BAZEL_STORAGE_LIMIT_BYTES": "1234"},
            clear=True,
        ):
            self.assertEqual(sync.resolve_bazel_storage_limit_bytes(), 1234)

        with mock.patch.dict(
            os.environ,
            {"KAIN_BAZEL_STORAGE_LIMIT_GIB": "1.5"},
            clear=True,
        ):
            self.assertEqual(
                sync.resolve_bazel_storage_limit_bytes(),
                int(1.5 * (1024**3)),
            )

    def test_prune_bazel_storage_prunes_oldest_entries_first(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            repo = Path(raw)
            output_root = repo / "cache" / "output"
            disk_root = repo / "cache" / "disk"
            repo_cache = repo / "cache" / "repo"

            with mock.patch.dict(
                os.environ,
                {
                    "KAIN_BAZEL_OUTPUT_USER_ROOT": str(output_root),
                    "KAIN_BAZEL_DISK_CACHE": str(disk_root),
                    "KAIN_BAZEL_REPOSITORY_CACHE": str(repo_cache),
                },
                clear=True,
            ):
                for root, child_name, payload, mtime in (
                    (output_root, "old", b"aaaa", 1000),
                    (disk_root, "mid", b"bbbbb", 2000),
                    (repo_cache, "new", b"cccccc", 3000),
                ):
                    child = root / child_name
                    child.mkdir(parents=True)
                    (child / "payload.txt").write_bytes(payload)
                    os.utime(child, (mtime, mtime))

                total_before, total_after, removed, warnings = sync.prune_bazel_storage(
                    repo,
                    max_bytes=10,
                )

            self.assertEqual(total_before, 15)
            self.assertEqual(total_after, 6)
            self.assertEqual(
                removed,
                (
                    f"output_user_root:{output_root / 'old'}",
                    f"disk_cache:{disk_root / 'mid'}",
                ),
            )
            self.assertFalse((output_root / "old").exists())
            self.assertFalse((disk_root / "mid").exists())
            self.assertTrue((repo_cache / "new").exists())
            self.assertEqual(warnings, ())

    def test_sibling_bazel_build_binary_returns_none(self) -> None:
        # kn is now an alias for kain — no separate sibling binary
        self.assertIsNone(sync.sibling_bazel_build_binary("kain"))
        self.assertIsNone(sync.sibling_bazel_build_binary("blade"))

    def test_invoke_bazel_build_raises_on_locked_output_without_sibling(self) -> None:
        # kn is now an alias — no sibling fallback. A locked output should raise.
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            context = sync.SyncContext(
                repo_root=root,
                policy={},
                sync_policy={},
                state_root=root / ".kain" / "state",
                stamp_path=root / ".kain" / "state" / "state" / "kain_sync_stamp.json",
                bazel_config="dev",
                source_watch_paths=(),
                source_filesystem_watch_paths=(),
                runtime_stamp_files=(),
                launcher_dir=root / ".kain" / "bin",
                binary_names=("kain",),
                repo_kain_home=root / ".kain",
                repo_kain_config=root / ".kain" / "config.toml",
                clang_path=None,
                python_path=None,
            )
            locked = sync.CommandResult(
                1,
                (
                    "Compiling Rust bin kain (51 files)",
                    "ERROR: failed to delete output files before executing action: bazel-out/x64_windows-dbg/bin/crates/cli/kain.exe (Permission denied)",
                ),
            )
            with mock.patch.object(
                sync, "run_bazel_build_target", return_value=locked
            ):
                with self.assertRaises(sync.SyncError):
                    sync.invoke_bazel_build(context, "kain")

    def test_sync_launchers_retries_when_source_stamp_changes_mid_run(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            context = sync.SyncContext(
                repo_root=root,
                policy={},
                sync_policy={},
                state_root=root / ".kain" / "state",
                stamp_path=root / ".kain" / "state" / "state" / "kain_sync_stamp.json",
                bazel_config="dev",
                source_watch_paths=(),
                source_filesystem_watch_paths=(),
                runtime_stamp_files=(),
                launcher_dir=root / ".kain" / "bin",
                binary_names=("kain",),
                repo_kain_home=root / ".kain",
                repo_kain_config=root / ".kain" / "config.toml",
                clang_path=None,
                python_path=None,
            )
            stamps = [
                {"stamp": "stamp-a", "dirty_count": 1, "watch_paths": (), "filesystem_watch_paths": ()},
                {"stamp": "stamp-b", "dirty_count": 1, "watch_paths": (), "filesystem_watch_paths": ()},
                {"stamp": "stamp-b", "dirty_count": 1, "watch_paths": (), "filesystem_watch_paths": ()},
                {"stamp": "stamp-b", "dirty_count": 1, "watch_paths": (), "filesystem_watch_paths": ()},
            ]
            seen: list[tuple[str, str]] = []

            def fake_launch_binary(
                context_arg: sync.SyncContext,
                binary_name: str,
                forward_args: list[str] | tuple[str, ...],
                *,
                skip_build: bool = False,
                update_stamp_only: bool = False,
                launcher_path: Path | None = None,
                source_data_override: dict[str, object] | None = None,
            ) -> int:
                self.assertIs(context_arg, context)
                self.assertEqual(forward_args, ())
                self.assertTrue(update_stamp_only)
                self.assertIsNotNone(launcher_path)
                assert source_data_override is not None
                seen.append((binary_name, str(source_data_override["stamp"])))
                return 0

            with mock.patch.object(sync, "source_stamp_data", side_effect=stamps), mock.patch.object(
                sync, "launch_binary", side_effect=fake_launch_binary
            ), mock.patch.object(
                sync, "build_launcher_shim", return_value=root / "shim.exe"
            ), mock.patch.object(
                sync, "install_launcher_files", return_value=[]
            ):
                exit_code = sync.sync_launchers(context, managed_sync=True)

            self.assertEqual(exit_code, 0)
            self.assertEqual(
                seen,
                [
                    ("kain", "stamp-a"),
                    ("kain", "stamp-b"),
                ],
            )

    def test_sync_launchers_raises_when_source_stamp_never_settles(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            context = sync.SyncContext(
                repo_root=root,
                policy={},
                sync_policy={},
                state_root=root / ".kain" / "state",
                stamp_path=root / ".kain" / "state" / "state" / "kain_sync_stamp.json",
                bazel_config="dev",
                source_watch_paths=(),
                source_filesystem_watch_paths=(),
                runtime_stamp_files=(),
                launcher_dir=root / ".kain" / "bin",
                binary_names=("kain",),
                repo_kain_home=root / ".kain",
                repo_kain_config=root / ".kain" / "config.toml",
                clang_path=None,
                python_path=None,
            )
            stamps = [
                {"stamp": "stamp-a", "dirty_count": 1, "watch_paths": (), "filesystem_watch_paths": ()},
                {"stamp": "stamp-b", "dirty_count": 1, "watch_paths": (), "filesystem_watch_paths": ()},
                {"stamp": "stamp-b", "dirty_count": 1, "watch_paths": (), "filesystem_watch_paths": ()},
                {"stamp": "stamp-c", "dirty_count": 1, "watch_paths": (), "filesystem_watch_paths": ()},
                {"stamp": "stamp-c", "dirty_count": 1, "watch_paths": (), "filesystem_watch_paths": ()},
                {"stamp": "stamp-d", "dirty_count": 1, "watch_paths": (), "filesystem_watch_paths": ()},
            ]

            with mock.patch.object(sync, "source_stamp_data", side_effect=stamps), mock.patch.object(
                sync, "launch_binary", return_value=0
            ), mock.patch.object(
                sync, "build_launcher_shim", return_value=root / "shim.exe"
            ), mock.patch.object(
                sync, "install_launcher_files", return_value=[]
            ):
                with self.assertRaises(sync.SyncError):
                    sync.sync_launchers(context, managed_sync=True)

    def test_build_launcher_shim_uses_state_root_temp(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            source = root / "scripts" / "windows" / "kain_bazel_cli_launcher.rs"
            source.parent.mkdir(parents=True)
            source.write_text("fn main() {}\n", encoding="utf-8")
            context = sync.SyncContext(
                repo_root=root,
                policy={},
                sync_policy={},
                state_root=root / ".kain" / "state",
                stamp_path=root / ".kain" / "state" / "state" / "kain_sync_stamp.json",
                bazel_config="dev",
                source_watch_paths=(),
                source_filesystem_watch_paths=(),
                runtime_stamp_files=(),
                launcher_dir=root / ".kain" / "bin",
                binary_names=("kain",),
                repo_kain_home=root / ".kain",
                repo_kain_config=root / ".kain" / "config.toml",
                clang_path=None,
                python_path=None,
            )
            seen_env: dict[str, str] = {}

            def fake_run_live(
                args: list[str] | tuple[str, ...],
                cwd: Path,
                env: dict[str, str] | None = None,
            ) -> sync.CommandResult:
                self.assertEqual(cwd, root)
                self.assertIsNotNone(env)
                assert env is not None
                seen_env.update(
                    {key: env[key] for key in ("TMP", "TEMP", "TMPDIR")}
                )
                output_path = Path(args[-1])
                output_path.write_bytes(b"shim")
                return sync.CommandResult(0, ())

            with mock.patch.object(sync, "rustc_command", return_value=["rustc"]), mock.patch.object(
                sync, "run_live", side_effect=fake_run_live
            ):
                shim_path = sync.build_launcher_shim(context)

            expected_temp = str((context.state_root / "tmp").resolve())
            self.assertEqual(seen_env["TMP"], expected_temp)
            self.assertEqual(seen_env["TEMP"], expected_temp)
            self.assertEqual(seen_env["TMPDIR"], expected_temp)
            self.assertTrue(shim_path.exists())

    def test_copy_file_atomic_if_unlocked_replaces_stale_pending_copy(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            source = root / "source.exe"
            destination = root / "dest.exe"
            source.write_bytes(b"fresh")
            destination.write_bytes(b"old")
            stale_pending = root / "dest.exe.pending.stale"
            stale_pending.write_bytes(b"stale")

            with mock.patch.object(sync.os, "replace", side_effect=PermissionError("locked")):
                message = sync.copy_file_atomic_if_unlocked(source, destination)

            self.assertIsNotNone(message)
            self.assertFalse(stale_pending.exists())
            pending_files = list(root.glob("dest.exe.pending.*"))
            self.assertEqual(len(pending_files), 1)
            self.assertEqual(pending_files[0].read_bytes(), b"fresh")

    def test_install_launcher_files_writes_all_windows_wrappers_when_windows(self) -> None:
        if os.name != "nt":
            self.skipTest("Windows .cmd wrapper behavior is platform-specific")
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            shim = root / "shim.exe"
            shim.write_bytes(b"shim")
            context = sync.SyncContext(
                repo_root=root,
                policy={},
                sync_policy={},
                state_root=root / ".kain" / "state",
                stamp_path=root / ".kain" / "state" / "state" / "kain_sync_stamp.json",
                bazel_config="dev",
                source_watch_paths=(),
                source_filesystem_watch_paths=(),
                runtime_stamp_files=(),
                launcher_dir=root / ".kain" / "bin",
                binary_names=("kain",),
                repo_kain_home=root / ".kain",
                repo_kain_config=root / ".kain" / "config.toml",
                clang_path=None,
                python_path=None,
            )
            sync.install_launcher_files(context, shim)
            self.assertTrue((context.launcher_dir / "kain.exe").exists())
            self.assertEqual(
                (context.launcher_dir / "kain").read_text(encoding="utf-8"),
                sync._wsl_launcher_shim_text("kain"),
            )
            self.assertFalse((context.launcher_dir / "kn.exe").exists())
            self.assertFalse((context.launcher_dir / "blade.exe").exists())
            self.assertFalse((context.launcher_dir / "blade.cmd").exists())

    def test_launch_uses_child_process_and_returns_child_status(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            staged = root / ".kain" / "state" / "bin" / ("kain.exe" if os.name == "nt" else "kain")
            staged.parent.mkdir(parents=True)
            staged.write_bytes(b"fake")
            context = sync.SyncContext(
                repo_root=root,
                policy={},
                sync_policy={},
                state_root=root / ".kain" / "state",
                stamp_path=root / ".kain" / "state" / "state" / "kain_sync_stamp.json",
                bazel_config="dev",
                source_watch_paths=(),
                source_filesystem_watch_paths=(),
                runtime_stamp_files=(),
                launcher_dir=root / ".kain" / "bin",
                binary_names=("kain",),
                repo_kain_home=root / ".kain",
                repo_kain_config=root / ".kain" / "config.toml",
                clang_path=None,
                python_path=None,
            )
            current_stamp = "current"
            sync.write_json_atomic(
                context.stamp_path,
                {
                    "binary_by_name": {
                        "kain": {
                            "path": str(staged),
                            "bazel_path": str(staged),
                            "source_stamp": current_stamp,
                            "bazel_config": "dev",
                        }
                    }
                },
            )
            calls: dict[str, object] = {}

            class FakeRun:
                returncode = 23

            def fake_run(args: list[str], **kwargs: object) -> FakeRun:
                calls["args"] = args
                calls["kwargs"] = kwargs
                return FakeRun()

            with mock.patch.object(
                sync,
                "source_stamp_data",
                return_value={"stamp": current_stamp, "dirty_count": 0, "watch_paths": (), "filesystem_watch_paths": ()},
            ), mock.patch.object(sync, "runtime_stamp", return_value="runtime"), mock.patch.object(
                sync, "repo_head_sha", return_value="test-sha"
            ), mock.patch.object(
                sync.subprocess, "run", side_effect=fake_run
            ):
                exit_code = sync.launch_binary(context, "kain", ["--", "--help"])

            self.assertEqual(exit_code, 23)
            self.assertEqual(calls["args"], [str(staged.resolve()), "--help"])
            kwargs = calls["kwargs"]
            self.assertIsInstance(kwargs, dict)
            self.assertNotIn("cwd", kwargs)
            self.assertEqual(kwargs["env"]["KAIN_ACTIVE_LAUNCHER_NAME"], "kain")


if __name__ == "__main__":
    unittest.main()
