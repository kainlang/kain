import unittest
from pathlib import Path
import sys


BENCHMARK_ROOT = Path(__file__).resolve().parents[1]
if str(BENCHMARK_ROOT) not in sys.path:
    sys.path.insert(0, str(BENCHMARK_ROOT))

import bench as benchmark_cli


class BenchCatalogTests(unittest.TestCase):
    def setUp(self) -> None:
        manifest_path = BENCHMARK_ROOT / "benchmarks.json"
        self.manifest = benchmark_cli.load_manifest(manifest_path)

    def test_manifest_cases_include_tags_and_suites(self) -> None:
        cases = self.manifest.get("cases", [])
        self.assertGreater(len(cases), 0)
        for case in cases:
            self.assertIn("tags", case)
            self.assertIsInstance(case["tags"], list)
            self.assertIn("suites", case)
            self.assertIsInstance(case["suites"], list)

    def test_select_case_ids_by_tag(self) -> None:
        selected = benchmark_cli.select_case_ids(
            self.manifest,
            case_ids=[],
            tags=["semantic"],
            suites=[],
        )
        self.assertIsNotNone(selected)
        self.assertGreater(len(selected), 0)
        by_id = benchmark_cli.indexed_cases(self.manifest)
        for case_id in selected:
            self.assertIn("semantic", by_id[case_id].get("tags", []))

    def test_select_case_ids_by_suite(self) -> None:
        selected = benchmark_cli.select_case_ids(
            self.manifest,
            case_ids=[],
            tags=[],
            suites=["smoke"],
        )
        self.assertEqual(
            selected,
            [
                "branch_dispatch",
                "call_chain",
                "memory_stream",
                "scalar_mix",
                "native_map_lookup",
                "actor_mailbox_erlang",
            ],
        )


if __name__ == "__main__":
    unittest.main()
