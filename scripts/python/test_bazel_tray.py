import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

from bazel_tray import BazelServerStatus, inspect_bazel_server


class BazelTrayTests(unittest.TestCase):
    def test_inspect_bazel_server_reports_stopped_without_pid_file(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            output_base = Path(raw)
            status = inspect_bazel_server(output_base)
            self.assertEqual(status.state, "stopped")
            self.assertFalse(status.running)
            self.assertFalse(status.stale)

    def test_inspect_bazel_server_reports_stale_pid_file(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            output_base = Path(raw)
            pid_file = output_base / "server" / "server.pid.txt"
            pid_file.parent.mkdir(parents=True)
            pid_file.write_text("12345", encoding="utf-8")

            status = inspect_bazel_server(output_base)
            self.assertEqual(status.state, "stale")
            self.assertTrue(status.stale)
            self.assertIn("12345", status.summary)

    def test_status_dataclass_summary_is_stable(self) -> None:
        status = BazelServerStatus(None, None, None, "stopped", "bazel server is not running")
        self.assertEqual(status.summary, "bazel server is not running")


if __name__ == "__main__":
    unittest.main()
