#!/usr/bin/env python3
"""Generate, verify, promote, and bake semantic error-corpus batches."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import tempfile
import tomllib
from dataclasses import dataclass
from pathlib import Path
from typing import Any


REPO_ROOT = Path(__file__).resolve().parents[3]
SEMANTIC_ROOT = REPO_ROOT / "crates" / "semantic"
TEMPLATES_PATH = SEMANTIC_ROOT / "templates" / "error_case_templates.toml"
DEFAULT_BATCH_DIR = SEMANTIC_ROOT / "batches"
DEFAULT_STAGE_ROOT = SEMANTIC_ROOT / "scratch" / "error_batches"
DEFAULT_PROMOTE_ROOT = SEMANTIC_ROOT / "error_corpus" / "generated"
DEFAULT_REPORT_ROOT = SEMANTIC_ROOT / ".kain" / "reports" / "error_batches"
HEADER_RE = re.compile(r"(?m)^//\s*@(?P<key>expected_code|expected_mode|expected_repair):\s*.*$")
NORMALIZE_RE = re.compile(r"[^a-z0-9]+")
INTERVIEW_FIELDS = (
    "interview_error_family",
    "interview_count",
    "interview_authoring",
    "interview_error_system",
    "interview_examples",
)
INTERVIEW_CHOICES = {"A", "B", "C"}


@dataclass(frozen=True)
class TemplateShape:
    name: str
    family: str
    file_stem: str
    description: str
    expected_mode: str
    expected_repair: str
    repair_required: bool
    allowed_codes: tuple[str, ...]
    donor_hint: str
    body: str
    knobs: dict[str, str]


@dataclass
class PlannedCase:
    shape: TemplateShape
    ordinal: int
    file_name: str
    staged_path: Path
    promoted_path: Path
    knobs: dict[str, str]


@dataclass
class VerificationResult:
    case: PlannedCase
    passed: bool
    actual_code: str
    actual_mode: str
    actual_backend: str
    actual_repair: str
    explanation: str
    rendered_error: str
    duplicate_of: str | None
    report_path: Path
    reasons: list[str]


def normalize_mode(value: str) -> str:
    camel_split = re.sub(r"(?<!^)(?=[A-Z])", "_", value.strip())
    return NORMALIZE_RE.sub("_", camel_split.lower()).strip("_")


def slugify(value: str) -> str:
    return NORMALIZE_RE.sub("_", value.strip().lower()).strip("_")


def temp_root() -> Path:
    for key in ("TMP", "TEMP", "TMPDIR"):
        value = os.environ.get(key)
        if value:
            path = Path(value)
            path.mkdir(parents=True, exist_ok=True)
            return path
    fallback = REPO_ROOT / ".kain" / "tmp"
    fallback.mkdir(parents=True, exist_ok=True)
    return fallback


def load_templates(path: Path) -> dict[str, TemplateShape]:
    data = tomllib.loads(path.read_text(encoding="utf-8"))
    shapes: dict[str, TemplateShape] = {}
    for name, raw in data.get("shapes", {}).items():
        knobs = {key: str(value) for key, value in raw.get("knobs", {}).items()}
        shapes[name] = TemplateShape(
            name=name,
            family=str(raw["family"]),
            file_stem=str(raw["file_stem"]),
            description=str(raw["description"]),
            expected_mode=str(raw["expected_mode"]),
            expected_repair=str(raw["expected_repair"]),
            repair_required=bool(raw.get("repair_required", False)),
            allowed_codes=tuple(str(value) for value in raw.get("allowed_codes", [])),
            donor_hint=str(raw["donor_hint"]),
            body=str(raw["body"]).strip() + "\n",
            knobs=knobs,
        )
    return shapes


def load_batch(path: Path) -> dict[str, Any]:
    return tomllib.loads(path.read_text(encoding="utf-8"))


def validate_batch_meta(batch_path: Path, batch: dict[str, Any]) -> dict[str, str]:
    batch_meta = batch.get("batch", {})
    if not isinstance(batch_meta, dict):
        raise SystemExit(f"batch metadata must be a table in {batch_path}")
    answers: dict[str, str] = {}
    missing = [field for field in INTERVIEW_FIELDS if field not in batch_meta]
    if missing:
        joined = ", ".join(missing)
        raise SystemExit(
            f"batch {batch_path.name} is missing required interview answer(s): {joined}. "
            "Use A/B/C answers so cheap models always leave a trace of the authoring plan."
        )
    for field in INTERVIEW_FIELDS:
        value = str(batch_meta[field]).strip().upper()
        if value not in INTERVIEW_CHOICES:
            raise SystemExit(f"batch {batch_path.name} field {field} must be one of A, B, or C")
        answers[field] = value
    return answers


def merge_knobs(shape: TemplateShape, index: int, overrides: dict[str, Any]) -> dict[str, str]:
    values: dict[str, str] = {"index": str(index)}
    raw: dict[str, Any] = {}
    raw.update(shape.knobs)
    raw.update({key: str(value) for key, value in overrides.items()})
    previous_size = -1
    while previous_size != len(values):
        previous_size = len(values)
        for key, raw_value in raw.items():
            values[key] = str(raw_value).format_map(values)
    return values


def format_allowed_codes(shape: TemplateShape) -> str:
    return ", ".join(shape.allowed_codes)


def render_fixture_text(shape: TemplateShape, knobs: dict[str, str], *, batch_name: str) -> str:
    body = shape.body.format_map(knobs).rstrip()
    header = [
        f"// ERROR: generated {shape.family} fixture from batch {batch_name}",
        f"// @expected_code: {shape.allowed_codes[0] if shape.allowed_codes else 'PENDING'}",
        f"// @expected_mode: {shape.expected_mode}",
        f"// @expected_repair: {shape.expected_repair}",
        f"// @donor_hint: {shape.donor_hint}",
        f"// @allowed_codes: {format_allowed_codes(shape)}",
    ]
    return "\n".join(header + ["", body, ""])


def parse_existing_sources(corpus_root: Path) -> dict[str, str]:
    fingerprints: dict[str, str] = {}
    for path in corpus_root.rglob("*.kn"):
        try:
            text = path.read_text(encoding="utf-8")
        except OSError:
            continue
        digest = hashlib.sha256(strip_generated_header(text).encode("utf-8")).hexdigest()
        fingerprints[digest] = str(path)
    return fingerprints


def strip_generated_header(text: str) -> str:
    lines = []
    for line in text.splitlines():
        if line.startswith("// @") or line.startswith("// ERROR: generated"):
            continue
        lines.append(line)
    return "\n".join(lines).strip()


def make_planned_cases(batch_path: Path, batch: dict[str, Any], shapes: dict[str, TemplateShape]) -> tuple[list[PlannedCase], Path, Path, str]:
    batch_meta = batch.get("batch", {})
    batch_name = str(batch_meta.get("name", batch_path.stem))
    prefix = str(batch_meta.get("prefix", slugify(batch_name)))
    stage_dir = DEFAULT_STAGE_ROOT / batch_name
    promote_dir = DEFAULT_PROMOTE_ROOT / batch_name

    planned: list[PlannedCase] = []
    ordinal = 0
    for case_spec in batch.get("cases", []):
        shape_name = str(case_spec["shape"])
        if shape_name not in shapes:
            raise SystemExit(f"unknown shape '{shape_name}'")
        shape = shapes[shape_name]
        count = int(case_spec.get("count", 1))
        overrides = dict(case_spec.get("knobs", {}))
        case_prefix = str(case_spec.get("prefix", prefix))
        start_index = int(case_spec.get("start_index", 0))
        for local_index in range(count):
            rendered_index = start_index + local_index
            knobs = merge_knobs(shape, rendered_index, overrides)
            file_name = f"{case_prefix}_{shape.file_stem}_{ordinal:03d}.kn"
            planned.append(
                PlannedCase(
                    shape=shape,
                    ordinal=ordinal,
                    file_name=file_name,
                    staged_path=stage_dir / file_name,
                    promoted_path=promote_dir / file_name,
                    knobs=knobs,
                )
            )
            ordinal += 1
    return planned, stage_dir, promote_dir, batch_name


def write_stage_files(planned: list[PlannedCase], batch_name: str, *, overwrite: bool) -> None:
    for case in planned:
        case.staged_path.parent.mkdir(parents=True, exist_ok=True)
        if case.staged_path.exists() and not overwrite:
            raise SystemExit(f"stage file already exists: {case.staged_path}")
        text = render_fixture_text(case.shape, case.knobs, batch_name=batch_name)
        case.staged_path.write_text(text, encoding="utf-8", newline="\n")


def ensure_stage_files(planned: list[PlannedCase], batch_name: str) -> None:
    for case in planned:
        if case.staged_path.exists():
            continue
        case.staged_path.parent.mkdir(parents=True, exist_ok=True)
        text = render_fixture_text(case.shape, case.knobs, batch_name=batch_name)
        case.staged_path.write_text(text, encoding="utf-8", newline="\n")


def run_kain_check(path: Path, target: str, timeout: int) -> tuple[int, dict[str, Any], str]:
    with tempfile.NamedTemporaryFile(delete=False, suffix=".json", dir=temp_root()) as handle:
        json_path = Path(handle.name)
    command = ["kain", "check", str(path), "--target", target, "--json-out", str(json_path)]
    env = os.environ.copy()
    env.setdefault("TMP", "Z:\\_b\\tmp")
    env.setdefault("TEMP", "Z:\\_b\\tmp")
    env.setdefault("TMPDIR", "Z:\\_b\\tmp")
    proc = subprocess.run(
        command,
        cwd=REPO_ROOT,
        text=True,
        capture_output=True,
        timeout=timeout,
        env=env,
        check=False,
    )
    try:
        if not json_path.exists():
            return proc.returncode, {}, proc.stdout + proc.stderr
        payload = json.loads(json_path.read_text(encoding="utf-8"))
    finally:
        if json_path.exists():
            json_path.unlink()
    return proc.returncode, payload, proc.stdout + proc.stderr


def first_diagnostic(payload: dict[str, Any]) -> dict[str, Any]:
    files = payload.get("files", [])
    if not files:
        return {}
    diagnostic = files[0].get("diagnostic", {})
    diagnostics = diagnostic.get("diagnostics", [])
    return diagnostics[0] if diagnostics else {}


def best_repair(diag: dict[str, Any]) -> str:
    repairs = diag.get("semantic", {}).get("repairs", [])
    if not repairs:
        return ""
    repair = repairs[0]
    return str(repair.get("replacement_text") or repair.get("description") or repair.get("repair_id") or "")


def rewrite_metadata(path: Path, *, actual_code: str, actual_mode: str, expected_repair: str) -> None:
    text = path.read_text(encoding="utf-8")
    replacements = {
        "expected_code": f"// @expected_code: {actual_code}",
        "expected_mode": f"// @expected_mode: {actual_mode}",
        "expected_repair": f"// @expected_repair: {expected_repair}",
    }
    used: set[str] = set()

    def replace(match: re.Match[str]) -> str:
        key = match.group("key")
        used.add(key)
        return replacements[key]

    updated = HEADER_RE.sub(replace, text)
    missing = [key for key in replacements if key not in used]
    if missing:
        raise SystemExit(f"missing metadata header(s) in {path}: {', '.join(missing)}")
    path.write_text(updated, encoding="utf-8", newline="\n")


def verify_case(case: PlannedCase, existing: dict[str, str], *, timeout: int, report_dir: Path) -> VerificationResult:
    return_code, payload, rendered = run_kain_check(case.staged_path, "llvm", timeout)
    diag = first_diagnostic(payload)
    actual_code = str(diag.get("code", ""))
    semantic = diag.get("semantic", {})
    actual_mode = str(semantic.get("failure_mode", ""))
    actual_backend = str(semantic.get("backend", ""))
    explanation = str(semantic.get("explanation", ""))
    actual_repair = best_repair(diag)
    normalized_actual = normalize_mode(actual_mode)
    normalized_expected = normalize_mode(case.shape.expected_mode)

    stripped = strip_generated_header(case.staged_path.read_text(encoding="utf-8"))
    digest = hashlib.sha256(stripped.encode("utf-8")).hexdigest()
    duplicate_of = existing.get(digest)
    if duplicate_of and Path(duplicate_of).resolve() == case.promoted_path.resolve():
        duplicate_of = None

    reasons: list[str] = []
    if return_code == 0:
        reasons.append("file compiled successfully instead of failing")
    if actual_code not in case.shape.allowed_codes:
        reasons.append(f"actual code {actual_code or '<missing>'} not in allowed set {case.shape.allowed_codes}")
    if normalized_actual != normalized_expected:
        reasons.append(
            f"semantic failure mode {actual_mode or '<missing>'} did not match expected {case.shape.expected_mode}"
        )
    if not explanation:
        reasons.append("semantic explanation was empty")
    if case.shape.repair_required:
        repair_text = actual_repair or rendered
        if case.shape.expected_repair not in repair_text:
            reasons.append(
                f"expected repair hint '{case.shape.expected_repair}' did not appear in semantic repair output"
            )
    if duplicate_of:
        reasons.append(f"duplicate body matches existing corpus file {duplicate_of}")

    report_dir.mkdir(parents=True, exist_ok=True)
    report_path = report_dir / f"{case.file_name}.json"
    report_path.write_text(json.dumps(payload, indent=2), encoding="utf-8", newline="\n")

    return VerificationResult(
        case=case,
        passed=not reasons,
        actual_code=actual_code,
        actual_mode=actual_mode,
        actual_backend=actual_backend,
        actual_repair=actual_repair,
        explanation=explanation,
        rendered_error=rendered,
        duplicate_of=duplicate_of,
        report_path=report_path,
        reasons=reasons,
    )


def promote_cases(results: list[VerificationResult], promote_dir: Path, *, overwrite: bool) -> list[Path]:
    promote_dir.mkdir(parents=True, exist_ok=True)
    promoted: list[Path] = []
    for result in results:
        if not result.passed:
            continue
        destination = result.case.promoted_path
        if destination.exists() and not overwrite:
            raise SystemExit(f"promoted file already exists: {destination}")
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(result.case.staged_path, destination)
        promoted.append(destination)
    return promoted


def run_bake(batch_name: str, timeout: int) -> list[dict[str, Any]]:
    commands = [
        ["cargo", "test", "-p", "kain-semantic", "test_error_corpus_cases"],
        ["cargo", "test", "-p", "kain-semantic", "sidecar_pack"],
        ["bazel", "test", "//crates/semantic:unit_test", "--config=dev", "--test_output=errors"],
    ]
    outcomes: list[dict[str, Any]] = []
    for command in commands:
        proc = subprocess.run(command, cwd=REPO_ROOT, text=True, capture_output=True, timeout=timeout, check=False)
        outcomes.append(
            {
                "command": command,
                "returncode": proc.returncode,
                "stdout": proc.stdout,
                "stderr": proc.stderr,
            }
        )
    return outcomes


def write_reports(
    batch_name: str,
    batch_path: Path,
    stage_dir: Path,
    promote_dir: Path,
    interview_answers: dict[str, str],
    results: list[VerificationResult],
    bake: list[dict[str, Any]],
) -> tuple[Path, Path]:
    DEFAULT_REPORT_ROOT.mkdir(parents=True, exist_ok=True)
    json_path = DEFAULT_REPORT_ROOT / f"{batch_name}.json"
    md_path = DEFAULT_REPORT_ROOT / f"{batch_name}.md"

    payload = {
        "batch_name": batch_name,
        "batch_path": str(batch_path),
        "stage_dir": str(stage_dir),
        "promote_dir": str(promote_dir),
        "interview_answers": interview_answers,
        "verified_total": len(results),
        "verified_passed": sum(1 for result in results if result.passed),
        "verified_failed": sum(1 for result in results if not result.passed),
        "cases": [
            {
                "file_name": result.case.file_name,
                "shape": result.case.shape.name,
                "family": result.case.shape.family,
                "passed": result.passed,
                "actual_code": result.actual_code,
                "actual_mode": result.actual_mode,
                "actual_backend": result.actual_backend,
                "actual_repair": result.actual_repair,
                "duplicate_of": result.duplicate_of,
                "reasons": result.reasons,
                "report_path": str(result.report_path),
            }
            for result in results
        ],
        "bake": bake,
    }
    json_path.write_text(json.dumps(payload, indent=2), encoding="utf-8", newline="\n")

    lines = [
        f"# Semantic Error Batch Report: {batch_name}",
        "",
        f"- Batch spec: `{batch_path}`",
        f"- Interview answers: `{interview_answers}`",
        f"- Stage dir: `{stage_dir}`",
        f"- Promote dir: `{promote_dir}`",
        f"- Passed: `{payload['verified_passed']}/{payload['verified_total']}`",
        "",
        "## Cases",
        "",
    ]
    for result in results:
        status = "PASS" if result.passed else "FAIL"
        lines.append(
            f"- `{status}` `{result.case.file_name}` shape=`{result.case.shape.name}` code=`{result.actual_code}` mode=`{result.actual_mode}` backend=`{result.actual_backend}`"
        )
        if result.reasons:
            lines.append(f"  reasons: {', '.join(result.reasons)}")
    if bake:
        lines.extend(["", "## Bake", ""])
        for outcome in bake:
            status = "PASS" if outcome["returncode"] == 0 else "FAIL"
            lines.append(f"- `{status}` `{' '.join(outcome['command'])}`")
    md_path.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    return json_path, md_path


def list_shapes(shapes: dict[str, TemplateShape]) -> int:
    for name in sorted(shapes):
        shape = shapes[name]
        print(f"{name}\t{shape.family}\t{shape.expected_mode}\t{','.join(shape.allowed_codes)}\t{shape.description}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--templates", type=Path, default=TEMPLATES_PATH)
    parser.add_argument("--batch", type=Path, help="TOML batch specification")
    parser.add_argument("--list-shapes", action="store_true")
    parser.add_argument("--write-stage", action="store_true", help="generate staged files")
    parser.add_argument("--verify", action="store_true", help="run live kain verification on staged files")
    parser.add_argument("--promote", action="store_true", help="copy verified files into error_corpus/generated/<batch>")
    parser.add_argument("--bake", action="store_true", help="run cargo and bazel semantic bake gates after promotion")
    parser.add_argument("--overwrite", action="store_true", help="allow overwriting staged/promoted files")
    parser.add_argument("--timeout", type=int, default=180)
    args = parser.parse_args()

    shapes = load_templates(args.templates.resolve())
    if args.list_shapes:
        return list_shapes(shapes)
    if not args.batch:
        raise SystemExit("--batch is required unless --list-shapes is used")

    batch_path = args.batch.resolve()
    batch = load_batch(batch_path)
    interview_answers = validate_batch_meta(batch_path, batch)
    planned, stage_dir, promote_dir, batch_name = make_planned_cases(batch_path, batch, shapes)
    if not any([args.write_stage, args.verify, args.promote, args.bake]):
        args.write_stage = True
        args.verify = True

    if args.write_stage:
        write_stage_files(planned, batch_name, overwrite=args.overwrite)
    elif args.verify or args.promote or args.bake:
        ensure_stage_files(planned, batch_name)

    report_dir = DEFAULT_REPORT_ROOT / "cases" / batch_name
    existing = parse_existing_sources(SEMANTIC_ROOT / "error_corpus")
    results: list[VerificationResult] = []
    if args.verify or args.promote or args.bake:
        for case in planned:
            result = verify_case(case, existing, timeout=args.timeout, report_dir=report_dir)
            rewrite_metadata(
                case.staged_path,
                actual_code=result.actual_code or (case.shape.allowed_codes[0] if case.shape.allowed_codes else "PENDING"),
                actual_mode=case.shape.expected_mode,
                expected_repair=case.shape.expected_repair,
            )
            results.append(result)

    if results and not all(result.passed for result in results):
        write_reports(batch_name, batch_path, stage_dir, promote_dir, interview_answers, results, [])
        print(f"verification failed for batch {batch_name}")
        return 1

    if args.promote or args.bake:
        promote_cases(results, promote_dir, overwrite=args.overwrite)

    bake_outcomes: list[dict[str, Any]] = []
    if args.bake:
        bake_outcomes = run_bake(batch_name, timeout=args.timeout * 4)
        if any(outcome["returncode"] != 0 for outcome in bake_outcomes):
            write_reports(batch_name, batch_path, stage_dir, promote_dir, interview_answers, results, bake_outcomes)
            print(f"bake failed for batch {batch_name}")
            return 2

    json_path, md_path = write_reports(
        batch_name,
        batch_path,
        stage_dir,
        promote_dir,
        interview_answers,
        results,
        bake_outcomes,
    )
    print(f"batch={batch_name}")
    print(f"interview={interview_answers}")
    print(f"stage={stage_dir}")
    print(f"promote={promote_dir}")
    print(f"report_json={json_path}")
    print(f"report_md={md_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
