import tempfile
import unittest
from pathlib import Path
import sys


BENCHMARK_ROOT = Path(__file__).resolve().parents[1]
if str(BENCHMARK_ROOT) not in sys.path:
    sys.path.insert(0, str(BENCHMARK_ROOT))

import run as benchmark_run


def make_outputs(root: Path, stamp: str) -> dict[str, Path]:
    reports = root / "reports"
    reports.mkdir(parents=True, exist_ok=True)
    outputs = {
        "latest_json": reports / f"{stamp}.latest.json",
        "latest_llm": reports / f"{stamp}.latest.llm.md",
        "timestamped_json": reports / f"{stamp}.json",
        "timestamped_llm": reports / f"{stamp}.llm.md",
        "latest_minimal": root / f"{stamp}.md",
    }
    for path in outputs.values():
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("", encoding="utf-8")
    return outputs


def make_report(generated_at: str, median_ms: float, *, case_id: str = "scalar_mix") -> dict:
    throughput = (1000.0 * 1000.0) / median_ms
    return {
        "suite": "kain-multi-language-benchmarks",
        "generated_at": generated_at,
        "platform": "win32",
        "warmups": 0,
        "runs": 3,
        "languages": ["kain"],
        "latest_stem": "latest",
        "baseline_mode": "off",
        "language_labels": {"kain": "Kain LLVM"},
        "cases": [
            {
                "id": case_id,
                "title": "Scalar Mix",
                "description": "history test fixture",
                "maturity": "implemented",
                "fairness_note": "",
                "language_notes": {},
                "languages": ["kain"],
                "source": {"kain": f"cases/{case_id}/main.kn"},
                "build": {
                    "kain": {
                        "ok": True,
                        "build_ms": 1.0,
                        "command": ["kain", "build"],
                        "run_command": ["benchmark", case_id],
                        "env": {},
                        "error": "",
                    }
                },
                "run": {
                    "kain": {
                        "ok": True,
                        "samples_ms": [median_ms, median_ms, median_ms],
                        "warmups": [],
                        "min_ms": median_ms,
                        "max_ms": median_ms,
                        "median_ms": median_ms,
                        "mean_ms": median_ms,
                        "stdev_ms": 0.0,
                        "coefficient_of_variation": 0.0,
                        "max_to_median_ratio": 1.0,
                        "unstable": False,
                        "stability_note": "",
                        "error": "",
                    }
                },
                "winner": "kain",
                "fastest_median_ms": median_ms,
                "relative_to_fastest": {"kain": 1.0},
                "telemetry": {
                    "primary_metric_id": "items_per_s",
                    "metrics": [
                        {
                            "id": "items_per_s",
                            "label": "items/s",
                            "unit": "items/s",
                            "work_items": 1000,
                            "values": {"kain": throughput},
                        }
                    ],
                },
            }
        ],
        "ok": True,
        "toolchain": {
            "kain_native_tuning": {
                "profile": "benchmark-release",
                "opt_level": "3",
                "target_cpu": "native",
                "debug_info": "0",
            }
        },
        "git": {
            "available": True,
            "branch": "main",
            "commit": "deadbeef",
            "dirty": False,
            "dirty_entries": 0,
            "status_sample": [],
        },
        "baseline_cache": {
            "mode": "off",
            "root": "",
            "hits": 0,
            "refreshed": 0,
            "misses": 0,
            "disabled": 0,
            "eligible_languages": 0,
        },
    }


class BenchmarkHistoryTests(unittest.TestCase):
    def test_history_tracks_improvement_against_previous_run(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            db_path = root / "history.sqlite3"

            first_report = make_report("2026-05-20T00:00:00+00:00", 10.0)
            benchmark_run.persist_report_history(
                first_report,
                db_path,
                make_outputs(root, "first"),
                manifest_path=str(BENCHMARK_ROOT / "benchmarks.json"),
                only_case="",
                timeout=60,
                minimal_name="latest.md",
            )

            second_report = make_report("2026-05-20T00:10:00+00:00", 8.0)
            history = benchmark_run.summarize_report_history(second_report, db_path)

            self.assertTrue(history["enabled"])
            self.assertEqual(history["kain_summary"]["compared_cases"], 1)
            self.assertEqual(history["kain_summary"]["improved_cases"], 1)
            self.assertEqual(history["kain_summary"]["regressed_cases"], 0)
            case_history = second_report["cases"][0]["history"]["kain"]
            self.assertTrue(case_history["available"])
            self.assertAlmostEqual(case_history["previous_median_ms"], 10.0)
            self.assertAlmostEqual(case_history["current_median_ms"], 8.0)
            self.assertAlmostEqual(case_history["delta_ms"], -2.0)
            self.assertEqual(case_history["direction"], "faster")
            self.assertGreater(case_history["primary_metric"]["delta_value"], 0.0)
            self.assertEqual(case_history["primary_metric"]["direction"], "higher")

            persisted = benchmark_run.persist_report_history(
                second_report,
                db_path,
                make_outputs(root, "second"),
                manifest_path=str(BENCHMARK_ROOT / "benchmarks.json"),
                only_case="",
                timeout=60,
                minimal_name="latest.md",
            )
            self.assertEqual(persisted["database"]["total_runs"], 2)

    def test_history_flags_large_regression_alert(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            db_path = root / "history.sqlite3"

            benchmark_run.persist_report_history(
                make_report("2026-05-20T00:00:00+00:00", 10.0),
                db_path,
                make_outputs(root, "base"),
                manifest_path=str(BENCHMARK_ROOT / "benchmarks.json"),
                only_case="",
                timeout=60,
                minimal_name="latest.md",
            )

            regressed = make_report("2026-05-20T00:20:00+00:00", 25.5)
            history = benchmark_run.summarize_report_history(regressed, db_path)
            case_history = regressed["cases"][0]["history"]["kain"]

            self.assertEqual(history["kain_summary"]["regressed_cases"], 1)
            self.assertEqual(history["kain_summary"]["alert_regressions"], 1)
            self.assertTrue(case_history["alert"])
            self.assertEqual(case_history["direction"], "slower")


if __name__ == "__main__":
    unittest.main()
