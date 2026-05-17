#!/usr/bin/env python3
"""
Data-driven release-readiness gate for Kain.
"""

from __future__ import annotations

import argparse
import json
import shlex
import subprocess
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any


DEFAULT_POLICY = Path("release/readiness_policy.json")
POLICY_SCHEMA_VERSION = 1
CHECK_KINDS = {
    "attrition_report",
    "benchmark_report",
    "coverage_matrix",
    "source_imports",
}


@dataclass
class HookResult:
    identifier: str
    passed: bool
    status: str
    command: list[str]
    cwd: str
    artifacts: list[str] = field(default_factory=list)
    missing_artifacts: list[str] = field(default_factory=list)
    returncode: int | None = None
    elapsed_ms: float = 0.0
    stdout: str = ""
    stderr: str = ""
    message: str = ""

    def to_dict(self) -> dict[str, Any]:
        return {
            "id": self.identifier,
            "passed": self.passed,
            "status": self.status,
            "command": self.command,
            "cwd": self.cwd,
            "artifacts": self.artifacts,
            "missing_artifacts": self.missing_artifacts,
            "returncode": self.returncode,
            "elapsed_ms": self.elapsed_ms,
            "stdout": self.stdout,
            "stderr": self.stderr,
            "message": self.message,
        }


@dataclass
class CheckResult:
    identifier: str
    kind: str
    passed: bool
    issues: list[str] = field(default_factory=list)
    evidence: dict[str, bool] = field(default_factory=dict)
    report_path: str = ""

    def to_dict(self) -> dict[str, Any]:
        return {
            "id": self.identifier,
            "kind": self.kind,
            "passed": self.passed,
            "issues": self.issues,
            "evidence": self.evidence,
            "report_path": self.report_path,
        }


def load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def resolve_path(repo_root: Path, path_text: str) -> Path:
    path = Path(path_text)
    if path.is_absolute():
        return path.resolve()
    return (repo_root / path).resolve()


def display_path(repo_root: Path, path: Path) -> str:
    try:
        return str(path.resolve().relative_to(repo_root.resolve())).replace("\\", "/")
    except ValueError:
        return str(path.resolve())


def display_command(command: list[str]) -> str:
    return " ".join(shlex.quote(part) for part in command)


def interpolate_command(command: list[str], repo_root: Path) -> list[str]:
    values = {
        "python": sys.executable,
        "repo": str(repo_root.resolve()),
    }
    expanded: list[str] = []
    for token in command:
        if token.startswith("{") and token.endswith("}"):
            key = token[1:-1]
            expanded.append(values.get(key, token))
        else:
            expanded.append(token)
    return expanded


def benchmark_case_evidence_id(case_id: str) -> str:
    return f"benchmark.case.{case_id}"


def attrition_case_evidence_id(case_id: str) -> str:
    return f"attrition.case.{case_id}"


def validate_policy_structure(
    repo_root: Path,
    policy: dict[str, Any],
) -> tuple[list[str], dict[str, dict[str, Any]], dict[str, dict[str, Any]]]:
    errors: list[str] = []
    hooks_by_id: dict[str, dict[str, Any]] = {}
    checks_by_id: dict[str, dict[str, Any]] = {}

    if policy.get("schema_version") != POLICY_SCHEMA_VERSION:
        errors.append(
            f"unsupported policy schema_version {policy.get('schema_version')!r}; expected {POLICY_SCHEMA_VERSION}"
        )

    raw_hooks = policy.get("hooks")
    if not isinstance(raw_hooks, list) or not raw_hooks:
        errors.append("policy must declare a non-empty hooks list")
        raw_hooks = []

    for hook in raw_hooks:
        if not isinstance(hook, dict):
            errors.append("hooks[] entries must be objects")
            continue
        hook_id = hook.get("id")
        if not isinstance(hook_id, str) or not hook_id.strip():
            errors.append("hooks[] entry is missing a non-empty string id")
            continue
        if hook_id in hooks_by_id:
            errors.append(f"duplicate hook id '{hook_id}'")
            continue
        command = hook.get("command")
        if not isinstance(command, list) or not command or not all(isinstance(part, str) and part for part in command):
            errors.append(f"hook '{hook_id}' must declare a non-empty string command array")
        cwd_text = hook.get("cwd", ".")
        if not isinstance(cwd_text, str) or not cwd_text.strip():
            errors.append(f"hook '{hook_id}' has invalid cwd")
        else:
            cwd_path = resolve_path(repo_root, cwd_text)
            if not cwd_path.exists():
                errors.append(f"hook '{hook_id}' cwd does not exist: {cwd_text}")
        artifacts = hook.get("artifacts", [])
        if not isinstance(artifacts, list) or not all(isinstance(item, str) and item for item in artifacts):
            errors.append(f"hook '{hook_id}' artifacts must be a string array")
        hooks_by_id[hook_id] = hook

    raw_checks = policy.get("checks")
    if not isinstance(raw_checks, list) or not raw_checks:
        errors.append("policy must declare a non-empty checks list")
        raw_checks = []

    for check in raw_checks:
        if not isinstance(check, dict):
            errors.append("checks[] entries must be objects")
            continue
        check_id = check.get("id")
        if not isinstance(check_id, str) or not check_id.strip():
            errors.append("checks[] entry is missing a non-empty string id")
            continue
        if check_id in checks_by_id:
            errors.append(f"duplicate check id '{check_id}'")
            continue
        kind = check.get("kind")
        if kind not in CHECK_KINDS:
            errors.append(f"check '{check_id}' has unsupported kind {kind!r}")
            continue

        if kind == "benchmark_report":
            manifest_text = check.get("manifest_path")
            if not isinstance(manifest_text, str) or not manifest_text.strip():
                errors.append(f"benchmark check '{check_id}' is missing manifest_path")
            else:
                manifest_path = resolve_path(repo_root, manifest_text)
                if not manifest_path.exists():
                    errors.append(f"benchmark check '{check_id}' manifest does not exist: {manifest_text}")
            required_cases = check.get("required_cases")
            if not isinstance(required_cases, list) or not required_cases:
                errors.append(f"benchmark check '{check_id}' must declare required_cases")

        if kind == "attrition_report":
            required_cases = check.get("required_cases")
            if not isinstance(required_cases, list) or not required_cases:
                errors.append(f"attrition check '{check_id}' must declare required_cases")

        if kind == "source_imports":
            rules = check.get("rules")
            if not isinstance(rules, list) or not rules:
                errors.append(f"source import check '{check_id}' must declare rules")
            else:
                rule_ids: set[str] = set()
                for rule in rules:
                    if not isinstance(rule, dict):
                        errors.append(f"source import check '{check_id}' has a non-object rule")
                        continue
                    rule_id = rule.get("id")
                    if not isinstance(rule_id, str) or not rule_id.strip():
                        errors.append(f"source import check '{check_id}' has a rule without a valid id")
                        continue
                    if rule_id in rule_ids:
                        errors.append(f"source import check '{check_id}' has duplicate rule id '{rule_id}'")
                    rule_ids.add(rule_id)
                    file_text = rule.get("path")
                    if not isinstance(file_text, str) or not file_text.strip():
                        errors.append(f"source import rule '{rule_id}' is missing path")
                    else:
                        file_path = resolve_path(repo_root, file_text)
                        if not file_path.exists():
                            errors.append(f"source import rule '{rule_id}' path does not exist: {file_text}")
                    required_imports = rule.get("required_imports")
                    if not isinstance(required_imports, list) or not required_imports:
                        errors.append(f"source import rule '{rule_id}' must declare required_imports")

        if kind == "coverage_matrix":
            features = check.get("features")
            if not isinstance(features, list) or not features:
                errors.append(f"coverage check '{check_id}' must declare features")
            else:
                feature_ids: set[str] = set()
                for feature in features:
                    if not isinstance(feature, dict):
                        errors.append(f"coverage check '{check_id}' has a non-object feature")
                        continue
                    feature_id = feature.get("id")
                    if not isinstance(feature_id, str) or not feature_id.strip():
                        errors.append(f"coverage check '{check_id}' has a feature without a valid id")
                        continue
                    if feature_id in feature_ids:
                        errors.append(f"coverage check '{check_id}' has duplicate feature id '{feature_id}'")
                    feature_ids.add(feature_id)
                    evidence_ids = feature.get("required_evidence")
                    if not isinstance(evidence_ids, list) or not evidence_ids:
                        errors.append(f"coverage feature '{feature_id}' must declare required_evidence")
                    current_surfaces = feature.get("current_surfaces", [])
                    if current_surfaces and not isinstance(current_surfaces, list):
                        errors.append(f"coverage feature '{feature_id}' current_surfaces must be an array")
                    else:
                        for path_text in current_surfaces:
                            if not isinstance(path_text, str) or not path_text.strip():
                                errors.append(f"coverage feature '{feature_id}' has invalid current_surfaces entry")
                                continue
                            if not resolve_path(repo_root, path_text).exists():
                                errors.append(f"coverage feature '{feature_id}' current surface does not exist: {path_text}")
                    owners = feature.get("owners", [])
                    if owners and not isinstance(owners, list):
                        errors.append(f"coverage feature '{feature_id}' owners must be an array")
                    else:
                        for owner in owners:
                            if not isinstance(owner, dict):
                                errors.append(f"coverage feature '{feature_id}' has a non-object owner")
                                continue
                            owned_paths = owner.get("owned_paths", [])
                            if not isinstance(owned_paths, list) or not owned_paths:
                                errors.append(f"coverage feature '{feature_id}' owner must declare owned_paths")
                                continue
                            for path_text in owned_paths:
                                if not isinstance(path_text, str) or not path_text.strip():
                                    errors.append(f"coverage feature '{feature_id}' has invalid owned_paths entry")
                                    continue
                                if not resolve_path(repo_root, path_text).exists():
                                    errors.append(f"coverage feature '{feature_id}' owned path does not exist: {path_text}")

        checks_by_id[check_id] = check

    profiles = policy.get("profiles")
    if not isinstance(profiles, dict) or not profiles:
        errors.append("policy must declare a non-empty profiles object")
        profiles = {}

    for profile_name, profile in profiles.items():
        if not isinstance(profile, dict):
            errors.append(f"profile '{profile_name}' must be an object")
            continue
        run_hooks = profile.get("run_hooks", [])
        evaluate_checks = profile.get("evaluate_checks", [])
        if not isinstance(run_hooks, list) or not all(isinstance(item, str) and item for item in run_hooks):
            errors.append(f"profile '{profile_name}' run_hooks must be a string array")
            continue
        if not isinstance(evaluate_checks, list) or not all(isinstance(item, str) and item for item in evaluate_checks):
            errors.append(f"profile '{profile_name}' evaluate_checks must be a string array")
            continue
        for hook_id in run_hooks:
            if hook_id not in hooks_by_id:
                errors.append(f"profile '{profile_name}' references unknown hook '{hook_id}'")
        for check_id in evaluate_checks:
            if check_id not in checks_by_id:
                errors.append(f"profile '{profile_name}' references unknown check '{check_id}'")

    return errors, hooks_by_id, checks_by_id


def run_hook(repo_root: Path, hook: dict[str, Any], execute: bool) -> HookResult:
    hook_id = str(hook["id"])
    command = interpolate_command(list(hook.get("command", [])), repo_root)
    cwd = resolve_path(repo_root, str(hook.get("cwd", ".")))
    artifact_paths = [resolve_path(repo_root, artifact) for artifact in hook.get("artifacts", [])]
    artifact_labels = [display_path(repo_root, artifact) for artifact in artifact_paths]
    expected_returncode = int(hook.get("expected_returncode", 0))

    if not execute:
        if artifact_paths and all(path.exists() for path in artifact_paths):
            return HookResult(
                identifier=hook_id,
                passed=True,
                status="reused",
                command=command,
                cwd=display_path(repo_root, cwd),
                artifacts=artifact_labels,
                missing_artifacts=[],
                message="reused existing artifacts",
            )
        return HookResult(
            identifier=hook_id,
            passed=False,
            status="not-run",
            command=command,
            cwd=display_path(repo_root, cwd),
            artifacts=artifact_labels,
            missing_artifacts=artifact_labels if artifact_labels else [],
            message="hook was not executed and no reusable artifacts were found",
        )

    started = time.perf_counter()
    completed = subprocess.run(
        command,
        cwd=str(cwd),
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    elapsed_ms = (time.perf_counter() - started) * 1000.0
    missing_artifacts = [display_path(repo_root, path) for path in artifact_paths if not path.exists()]
    passed = completed.returncode == expected_returncode and not missing_artifacts
    if completed.returncode != expected_returncode:
        message = f"expected exit code {expected_returncode}, got {completed.returncode}"
    elif missing_artifacts:
        message = "missing declared artifacts"
    else:
        message = "hook completed successfully"
    return HookResult(
        identifier=hook_id,
        passed=passed,
        status="passed" if passed else "failed",
        command=command,
        cwd=display_path(repo_root, cwd),
        artifacts=artifact_labels,
        missing_artifacts=missing_artifacts,
        returncode=completed.returncode,
        elapsed_ms=elapsed_ms,
        stdout=completed.stdout,
        stderr=completed.stderr,
        message=message,
    )


def evaluate_benchmark_report(
    repo_root: Path,
    check: dict[str, Any],
) -> CheckResult:
    check_id = str(check["id"])
    report_path = resolve_path(repo_root, str(check["report_path"]))
    evidence: dict[str, bool] = {}
    issues: list[str] = []
    if not report_path.exists():
        issues.append(f"benchmark report does not exist: {display_path(repo_root, report_path)}")
        return CheckResult(
            identifier=check_id,
            kind="benchmark_report",
            passed=False,
            issues=issues,
            evidence=evidence,
            report_path=display_path(repo_root, report_path),
        )

    report = load_json(report_path)
    case_map = {
        str(case.get("id")): case
        for case in report.get("cases", [])
        if isinstance(case, dict) and isinstance(case.get("id"), str)
    }
    default_forbidden_fragments = [str(fragment).lower() for fragment in check.get("forbidden_fragments", [])]

    for required_case in check.get("required_cases", []):
        case_id = str(required_case.get("id"))
        case_issues: list[str] = []
        case = case_map.get(case_id)
        if case is None:
            case_issues.append(f"missing benchmark case '{case_id}' in report")
        else:
            allowed_maturity = required_case.get("allowed_maturity", ["implemented"])
            maturity = str(case.get("maturity", ""))
            if maturity not in allowed_maturity:
                case_issues.append(
                    f"benchmark case '{case_id}' has maturity '{maturity}', expected one of {allowed_maturity}"
                )

            required_languages = required_case.get("required_languages", ["kain"])
            language_notes = case.get("language_notes", {})
            note_texts = [str(case.get("fairness_note", ""))]
            for language in required_languages:
                note_texts.append(str(language_notes.get(language, "")))
            fragments = default_forbidden_fragments + [str(fragment).lower() for fragment in required_case.get("forbidden_fragments", [])]
            for fragment in fragments:
                for note_text in note_texts:
                    if fragment and fragment in note_text.lower():
                        case_issues.append(
                            f"benchmark case '{case_id}' contains forbidden fragment '{fragment}'"
                        )
                        break

            build_map = case.get("build", {})
            run_map = case.get("run", {})
            for language in required_languages:
                build_result = build_map.get(language)
                run_result = run_map.get(language)
                if not isinstance(build_result, dict):
                    case_issues.append(f"benchmark case '{case_id}' is missing build results for '{language}'")
                    continue
                if not build_result.get("ok", False):
                    error_text = str(build_result.get("error", "")).strip() or "build failed"
                    case_issues.append(f"benchmark case '{case_id}' build failed for '{language}': {error_text}")
                if not isinstance(run_result, dict):
                    case_issues.append(f"benchmark case '{case_id}' is missing run results for '{language}'")
                    continue
                if not run_result.get("ok", False):
                    error_text = str(run_result.get("error", "")).strip() or "run failed"
                    case_issues.append(f"benchmark case '{case_id}' run failed for '{language}': {error_text}")
                if run_result.get("median_ms") is None and not run_result.get("samples_ms"):
                    case_issues.append(f"benchmark case '{case_id}' did not record a timing sample for '{language}'")

        evidence[benchmark_case_evidence_id(case_id)] = not case_issues
        issues.extend(case_issues)

    return CheckResult(
        identifier=check_id,
        kind="benchmark_report",
        passed=not issues,
        issues=issues,
        evidence=evidence,
        report_path=display_path(repo_root, report_path),
    )


def evaluate_attrition_report(
    repo_root: Path,
    check: dict[str, Any],
) -> CheckResult:
    check_id = str(check["id"])
    report_path = resolve_path(repo_root, str(check["report_path"]))
    evidence: dict[str, bool] = {}
    issues: list[str] = []
    if not report_path.exists():
        issues.append(f"attrition report does not exist: {display_path(repo_root, report_path)}")
        return CheckResult(
            identifier=check_id,
            kind="attrition_report",
            passed=False,
            issues=issues,
            evidence=evidence,
            report_path=display_path(repo_root, report_path),
        )

    report = load_json(report_path)
    if bool(check.get("require_suite_passed", True)) and not bool(report.get("suite_passed", False)):
        issues.append("attrition suite did not pass")

    case_map = {
        str(case.get("case_id")): case
        for case in report.get("cases", [])
        if isinstance(case, dict) and isinstance(case.get("case_id"), str)
    }
    for case_id in check.get("required_cases", []):
        case_issues: list[str] = []
        case = case_map.get(case_id)
        if case is None:
            case_issues.append(f"missing attrition case '{case_id}' in report")
        else:
            parsed = case.get("run", {}).get("parsed", {})
            passed = bool(parsed.get("passed", False)) and int(parsed.get("overall_status", 1)) == 0
            if not passed:
                failure_text = str(parsed.get("validate_failure", "")).strip() or str(parsed.get("run_failure", "")).strip()
                if not failure_text:
                    failure_text = "case did not report a passing attrition result"
                case_issues.append(f"attrition case '{case_id}' failed: {failure_text}")
        evidence[attrition_case_evidence_id(case_id)] = not case_issues
        issues.extend(case_issues)

    return CheckResult(
        identifier=check_id,
        kind="attrition_report",
        passed=not issues,
        issues=issues,
        evidence=evidence,
        report_path=display_path(repo_root, report_path),
    )


def evaluate_source_imports(
    repo_root: Path,
    check: dict[str, Any],
) -> CheckResult:
    check_id = str(check["id"])
    evidence: dict[str, bool] = {}
    issues: list[str] = []

    for rule in check.get("rules", []):
        rule_id = str(rule.get("id"))
        file_path = resolve_path(repo_root, str(rule.get("path")))
        rule_issues: list[str] = []
        if not file_path.exists():
            rule_issues.append(f"source import rule '{rule_id}' path does not exist")
        else:
            source_text = file_path.read_text(encoding="utf-8")
            missing_imports = [
                required_import
                for required_import in rule.get("required_imports", [])
                if required_import not in source_text
            ]
            if missing_imports:
                rule_issues.append(
                    f"source import rule '{rule_id}' is missing imports {missing_imports} in {display_path(repo_root, file_path)}"
                )
        evidence[rule_id] = not rule_issues
        issues.extend(rule_issues)

    return CheckResult(
        identifier=check_id,
        kind="source_imports",
        passed=not issues,
        issues=issues,
        evidence=evidence,
    )


def evaluate_coverage_matrix(
    repo_root: Path,
    check: dict[str, Any],
    evidence: dict[str, bool],
) -> CheckResult:
    check_id = str(check["id"])
    feature_evidence: dict[str, bool] = {}
    issues: list[str] = []

    for feature in check.get("features", []):
        feature_id = str(feature.get("id"))
        label = str(feature.get("label", feature_id))
        feature_issues: list[str] = []

        for surface_path_text in feature.get("current_surfaces", []):
            surface_path = resolve_path(repo_root, str(surface_path_text))
            if not surface_path.exists():
                feature_issues.append(f"feature '{feature_id}' current surface is missing: {surface_path_text}")

        for owner in feature.get("owners", []):
            for owned_path_text in owner.get("owned_paths", []):
                owned_path = resolve_path(repo_root, str(owned_path_text))
                if not owned_path.exists():
                    feature_issues.append(f"feature '{feature_id}' owned path is missing: {owned_path_text}")

        for evidence_id in feature.get("required_evidence", []):
            if evidence.get(evidence_id) is not True:
                feature_issues.append(
                    f"feature '{feature_id}' ({label}) is missing passing evidence '{evidence_id}'"
                )

        feature_evidence[f"coverage.feature.{feature_id}"] = not feature_issues
        issues.extend(feature_issues)

    return CheckResult(
        identifier=check_id,
        kind="coverage_matrix",
        passed=not issues,
        issues=issues,
        evidence=feature_evidence,
    )


def evaluate_check(
    repo_root: Path,
    check: dict[str, Any],
    evidence: dict[str, bool],
) -> CheckResult:
    kind = str(check["kind"])
    if kind == "benchmark_report":
        return evaluate_benchmark_report(repo_root, check)
    if kind == "attrition_report":
        return evaluate_attrition_report(repo_root, check)
    if kind == "source_imports":
        return evaluate_source_imports(repo_root, check)
    if kind == "coverage_matrix":
        return evaluate_coverage_matrix(repo_root, check, evidence)
    return CheckResult(
        identifier=str(check["id"]),
        kind=kind,
        passed=False,
        issues=[f"unsupported check kind '{kind}'"],
    )


def evaluate_policy(
    repo_root: Path,
    policy: dict[str, Any],
    profile_name: str,
    execute_hooks: bool,
) -> dict[str, Any]:
    policy_errors, hooks_by_id, checks_by_id = validate_policy_structure(repo_root, policy)
    profiles = policy.get("profiles", {})
    profile = profiles.get(profile_name)
    if profile is None:
        policy_errors.append(f"unknown profile '{profile_name}'")
        profile = {"run_hooks": [], "evaluate_checks": []}

    hook_results: list[HookResult] = []
    check_results: list[CheckResult] = []
    evidence: dict[str, bool] = {}
    errors = list(policy_errors)

    for hook_id in profile.get("run_hooks", []):
        hook = hooks_by_id[hook_id]
        result = run_hook(repo_root, hook, execute=execute_hooks)
        hook_results.append(result)
        evidence[result.identifier] = result.passed
        if not result.passed:
            errors.append(f"{result.identifier}: {result.message}")

    for check_id in profile.get("evaluate_checks", []):
        check = checks_by_id[check_id]
        result = evaluate_check(repo_root, check, evidence)
        check_results.append(result)
        evidence[result.identifier] = result.passed
        evidence.update(result.evidence)
        if not result.passed:
            for issue in result.issues:
                errors.append(f"{result.identifier}: {issue}")

    ok = not errors and all(result.passed for result in hook_results) and all(result.passed for result in check_results)
    return {
        "ok": ok,
        "profile": profile_name,
        "execute_hooks": execute_hooks,
        "hook_results": [result.to_dict() for result in hook_results],
        "check_results": [result.to_dict() for result in check_results],
        "errors": errors,
    }


def render_text_result(payload: dict[str, Any]) -> str:
    lines = [
        f"Release readiness profile: {payload['profile']}",
        f"Hook execution: {'enabled' if payload['execute_hooks'] else 'reuse-only'}",
        "",
        "Hooks:",
    ]
    for hook in payload["hook_results"]:
        status = "PASS" if hook["passed"] else "FAIL"
        lines.append(f"  - [{status}] {hook['id']} ({hook['status']})")
        lines.append(f"    command: {display_command(hook['command'])}")
        if hook.get("message"):
            lines.append(f"    note: {hook['message']}")
        if hook.get("missing_artifacts"):
            lines.append(f"    missing_artifacts: {hook['missing_artifacts']}")

    lines.append("")
    lines.append("Checks:")
    for check in payload["check_results"]:
        status = "PASS" if check["passed"] else "FAIL"
        lines.append(f"  - [{status}] {check['id']} ({check['kind']})")
        if check.get("report_path"):
            lines.append(f"    report: {check['report_path']}")
        for issue in check.get("issues", []):
            lines.append(f"    issue: {issue}")

    lines.append("")
    lines.append(f"Overall status: {'PASS' if payload['ok'] else 'FAIL'}")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description="Run the Kain release-readiness gate.")
    parser.add_argument("--policy", default=str(DEFAULT_POLICY), help="Path to the release-readiness policy JSON.")
    parser.add_argument("--profile", default="quick", help="Policy profile to evaluate.")
    parser.add_argument("--run", action="store_true", help="Execute hook commands instead of only reusing existing artifacts.")
    parser.add_argument("--json", action="store_true", help="Print the result payload as JSON.")
    parser.add_argument("--list-profiles", action="store_true", help="List available profiles and exit.")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[2]
    policy_path = resolve_path(repo_root, args.policy)
    if not policy_path.exists():
        raise SystemExit(f"release-readiness policy not found: {policy_path}")

    policy = load_json(policy_path)
    if args.list_profiles:
        profiles = policy.get("profiles", {})
        for profile_name in sorted(profiles):
            print(profile_name)
        return 0

    payload = evaluate_policy(repo_root, policy, args.profile, execute_hooks=args.run)
    payload["policy_path"] = display_path(repo_root, policy_path)

    if args.json:
        print(json.dumps(payload, indent=2))
    else:
        print(render_text_result(payload))

    return 0 if payload["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
