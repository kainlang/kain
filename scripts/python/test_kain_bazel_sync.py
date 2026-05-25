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
        self.assertTrue(sync.path_matches_watch_path("crates/kain-core/src/lib.rs", ("crates",)))
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
                binary_names=("kain", "kn", "blade"),
                repo_kain_home=repo / ".kain",
                repo_kain_config=repo / ".kain" / "config.toml",
                clang_path=None,
                python_path=None,
            )
            payload = sync.merge_stamp_payload(
                existing,
                context=context,
                binary_name="kain",
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

    def test_policy_binary_names_include_blade(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            repo = Path(raw)
            policy_path = repo / "blades" / "kain-mcp" / "config" / "runtime_policy.json"
            policy_path.parent.mkdir(parents=True)
            policy_path.write_text(
                json.dumps({"launcher_sync": {"launcher_binary_names": ["kain", "kn", "blade"]}}),
                encoding="utf-8",
            )
            context = sync.resolve_sync_context(repo)
            self.assertEqual(context.binary_names, ("kain", "kn", "blade"))

    def test_blade_bazel_output_name_is_data_driven(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            context = sync.SyncContext(
                repo_root=root,
                policy={},
                sync_policy={"bazel_binary_output_names": {"blade": "blade_bin"}},
                state_root=root / ".kain" / "state",
                stamp_path=root / ".kain" / "state" / "state" / "kain_sync_stamp.json",
                bazel_config="dev",
                source_watch_paths=(),
                source_filesystem_watch_paths=(),
                runtime_stamp_files=(),
                launcher_dir=root / ".kain" / "bin",
                binary_names=("kain", "kn", "blade"),
                repo_kain_home=root / ".kain",
                repo_kain_config=root / ".kain" / "config.toml",
                clang_path=None,
                python_path=None,
            )
            self.assertEqual(sync.bazel_output_binary_name(context, "blade"), "blade_bin")
            self.assertEqual(sync.bazel_output_binary_name(context, "kain"), "kain")

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
                binary_names=("kain", "kn", "blade"),
                repo_kain_home=root / ".kain",
                repo_kain_config=root / ".kain" / "config.toml",
                clang_path=None,
                python_path=None,
            )
            sync.install_launcher_files(context, shim)
            self.assertTrue((context.launcher_dir / "kain.exe").exists())
            self.assertTrue((context.launcher_dir / "kn.exe").exists())
            self.assertTrue((context.launcher_dir / "blade.exe").exists())
            self.assertTrue((context.launcher_dir / "blade.cmd").exists())

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
                binary_names=("kain", "kn", "blade"),
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
            self.assertEqual(kwargs["cwd"], str(root))
            self.assertEqual(kwargs["env"]["KAIN_ACTIVE_LAUNCHER_NAME"], "kain")


if __name__ == "__main__":
    unittest.main()
