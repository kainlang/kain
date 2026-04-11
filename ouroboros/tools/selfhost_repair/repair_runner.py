from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import time
from collections import Counter, defaultdict
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

TOOLS_ROOT = Path(__file__).resolve().parents[1]
if str(TOOLS_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOLS_ROOT))

from repair_rules import (
    enabled_rules_for_target,
    infer_crate_name,
    load_bootstrap_feature_policy,
    load_probe_targets,
    load_repair_rules,
    load_taxonomy,
)
from reporting import render_markdown, write_json, write_markdown
from probes import generate_probe_corpus
from ouroboros_pathing import discover_workspace_context


CONTEXT = discover_workspace_context(__file__)
OUROBOROS_ROOT = CONTEXT.ouroboros_root
DEFAULT_PHASE2_ROOT = OUROBOROS_ROOT / "out" / "selfhost" / "phase2"
DEFAULT_REPAIRED_ROOT = OUROBOROS_ROOT / "out" / "selfhost" / "phase2_repaired"
DEFAULT_REPAIR_DOCS = OUROBOROS_ROOT / "docs" / "selfhost" / "repairs"
DEFAULT_BOOTSTRAP_POLICY = DEFAULT_REPAIR_DOCS / "bootstrap_feature_policy.json"
DEFAULT_PROBES_ROOT = OUROBOROS_ROOT / "probes"
DEFAULT_BUILD_LOG = DEFAULT_PHASE2_ROOT / "stage2_workspace" / "stage2_build.log"
DEFAULT_REPORT_JSON = DEFAULT_REPAIRED_ROOT / "phase2_repair_report.json"
DEFAULT_REPORT_MD = DEFAULT_REPAIRED_ROOT / "phase2_repair_report.md"
DEFAULT_RULE_PROMOTION_LEDGER = OUROBOROS_ROOT / "docs" / "selfhost" / "rule_promotion_ledger.json"
DEFAULT_BOOTSTRAP_EXCEPTIONS = OUROBOROS_ROOT / "docs" / "selfhost" / "bootstrap_exceptions.json"
PARSER_IMPL_FRAGMENT_TARGET = "stage2_workspace/crates/kain-core/src/lib.rs"
PARSER_IMPL_FRAGMENT_RULE_ID = "parser_impl_fragment_closure_balance"
PARSER_HELPER_SURFACE_RULE_ID = "parser_helper_surface_injection"
PARSER_HELPER_SURFACE_ANCHOR = "impl Parser {\n    fn parse_mod(&mut self, vis: Visibility) -> Result<Item, KainError> {"


@dataclass
class ErrorRecord:
    code: str | None
    text: str
    file: str | None
    line: int | None
    col: int | None
    bucket_id: str
    candidate_rule_ids: list[str]


@dataclass
class ValidationSummary:
    ran: bool
    mode: str
    returncode: int | None
    success: bool
    log_path: str | None


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def parse_error_blocks(log_text: str) -> list[str]:
    blocks: list[str] = []
    current: list[str] = []
    for line in log_text.splitlines():
        if line.startswith("error"):
            if current:
                blocks.append("\n".join(current).strip())
            current = [line]
        elif current:
            if line.startswith("warning") and current:
                blocks.append("\n".join(current).strip())
                current = []
            else:
                current.append(line)
    if current:
        blocks.append("\n".join(current).strip())
    return [block for block in blocks if block]


def classify_log(log_path: Path, taxonomy) -> dict[str, Any]:
    if not log_path.exists():
        return {
            "exists": False,
            "bucket_counts": {},
            "records": [],
            "hotspots": [],
            "unknown_samples": [],
        }

    log_text = log_path.read_text(encoding="utf-8", errors="ignore")
    blocks = parse_error_blocks(log_text)
    records: list[ErrorRecord] = []
    hotspots = Counter()
    for block in blocks:
        first_line = block.splitlines()[0] if block else ""
        code = None
        if first_line.startswith("error[") and "]" in first_line:
            code = first_line.split("[", 1)[1].split("]", 1)[0]
        file = None
        line = None
        col = None
        for block_line in block.splitlines():
            stripped = block_line.strip()
            if stripped.startswith("-->"):
                location = stripped.removeprefix("-->").strip()
                if ":" in location:
                    parts = location.rsplit(":", 2)
                    if len(parts) == 3:
                        file = parts[0]
                        try:
                            line = int(parts[1])
                            col = int(parts[2])
                        except ValueError:
                            line = None
                            col = None
                break
        bucket = taxonomy.classify(block)
        records.append(
            ErrorRecord(
                code=code,
                text=block,
                file=file,
                line=line,
                col=col,
                bucket_id=bucket.id,
                candidate_rule_ids=bucket.candidate_rule_ids,
            )
        )
        if file:
            hotspots[file] += 1

    bucket_counts = Counter(record.bucket_id for record in records)
    unknown_samples = [record.text.splitlines()[0] for record in records if record.bucket_id == "unknown"][:12]
    return {
        "exists": True,
        "bucket_counts": dict(bucket_counts),
        "records": [record.__dict__ for record in records],
        "hotspots": [
            {"file": file, "error_count": count}
            for file, count in hotspots.most_common(20)
        ],
        "unknown_samples": unknown_samples,
    }


def build_report(
    phase2_root: Path,
    repaired_root: Path,
    taxonomy,
    rules,
    phase_name: str,
    validation_mode: str,
) -> dict[str, Any]:
    before = classify_log(phase2_root / "stage2_workspace" / "stage2_build.log", taxonomy)
    repair_summary = apply_repairs(phase2_root, repaired_root, rules, phase_name)
    preflight = run_structural_preflight(repaired_root)
    validation = run_validation(repaired_root, validation_mode)
    after_log = Path(validation.log_path) if validation.log_path else repaired_root / "stage2_workspace" / "stage2_build.log"
    after = classify_log(after_log, taxonomy)
    rule_promotions = load_json_payload(DEFAULT_RULE_PROMOTION_LEDGER).get("entries", [])
    bootstrap_exceptions = load_json_payload(DEFAULT_BOOTSTRAP_EXCEPTIONS).get("exceptions", [])
    return {
        "generated_at": utc_now(),
        "phase": phase_name,
        "input_artifacts": collect_input_artifacts(phase2_root),
        "bootstrap_policy": load_policy_summary(DEFAULT_BOOTSTRAP_POLICY),
        "before": before,
        "repairs": repair_summary,
        "preflight": preflight,
        "validation": validation.__dict__,
        "after": after,
        "rule_promotions": rule_promotions,
        "bounded_bootstrap_exceptions": bootstrap_exceptions,
    }


def collect_input_artifacts(phase2_root: Path) -> list[str]:
    artifacts: list[str] = []
    if not phase2_root.exists():
        return artifacts
    for path in sorted(phase2_root.rglob("*")):
        if path.is_file() and path.suffix in {".kn", ".rs", ".log", ".json", ".md", ".txt"}:
            artifacts.append(path.relative_to(phase2_root).as_posix())
    return artifacts


def load_policy_summary(policy_path: Path) -> dict[str, Any]:
    if not policy_path.exists():
        return {"exists": False, "policy_count": 0, "policies": []}
    payload = load_bootstrap_feature_policy(policy_path)
    policies = payload.get("policies", [])
    return {
        "exists": True,
        "policy_count": len(policies),
        "policies": [
            {
                "id": policy.get("id"),
                "crate": policy.get("crate"),
                "kind": policy.get("kind"),
                "mode": policy.get("mode"),
                "member_count": len(policy.get("members", [])),
            }
            for policy in policies
        ],
    }


def load_json_payload(path: Path) -> dict[str, Any]:
    if not path.exists():
        return {}
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return {}


def copy_phase2_root(src: Path, dst: Path) -> None:
    if dst.exists():
        for _ in range(5):
            try:
                shutil.rmtree(dst)
                break
            except OSError:
                time.sleep(0.25)
        if dst.exists():
            for path in sorted(dst.rglob("*"), reverse=True):
                try:
                    if path.is_file() or path.is_symlink():
                        os.chmod(path, 0o666)
                        path.unlink(missing_ok=True)
                    elif path.is_dir():
                        path.rmdir()
                except OSError:
                    continue
            if dst.exists():
                shutil.rmtree(dst, ignore_errors=True)
    shutil.copytree(src, dst)


def target_files_for_kind(repaired_root: Path, target_kind: str) -> list[Path]:
    if target_kind == "generated_rust":
        return [path for path in repaired_root.rglob("*.rs") if path.is_file()]
    if target_kind == "kn_bundle":
        return [path for path in repaired_root.rglob("*.kn") if path.is_file()]
    if target_kind == "build_log":
        build_log = repaired_root / "stage2_workspace" / "stage2_build.log"
        return [build_log] if build_log.exists() else []
    return []


def brace_delta_for_line(line: str) -> int:
    depth_delta = 0
    in_string = False
    in_char = False
    escaped = False
    idx = 0
    while idx < len(line):
        ch = line[idx]
        nxt = line[idx + 1] if idx + 1 < len(line) else ""
        if not in_string and not in_char and ch == "/" and nxt == "/":
            break
        if in_string:
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == "\"":
                in_string = False
            idx += 1
            continue
        if in_char:
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == "'":
                in_char = False
            idx += 1
            continue
        if ch == "\"":
            in_string = True
        elif ch == "'":
            in_char = True
        elif ch == "{":
            depth_delta += 1
        elif ch == "}":
            depth_delta -= 1
        idx += 1
    return depth_delta


def repair_parser_impl_fragmentation(text: str) -> tuple[str, int]:
    lines = text.splitlines(keepends=True)
    if not lines:
        return text, 0
    depth = 0
    baseline_depth: int | None = None
    insertions = 0
    updated_lines: list[str] = []
    for line in lines:
        stripped = line.strip()
        if stripped.startswith("impl Parser {"):
            if baseline_depth is None:
                baseline_depth = depth
            elif depth > baseline_depth:
                missing = depth - baseline_depth
                updated_lines.extend(["}\n"] * missing)
                insertions += missing
                depth -= missing
        updated_lines.append(line)
        depth += brace_delta_for_line(line)
    if insertions == 0:
        return text, 0
    return "".join(updated_lines), insertions


def repair_parser_helper_surface(text: str) -> tuple[str, int]:
    required_helpers = ("check", "advance", "current_span", "skip_newlines", "at_end", "parse_item")
    missing = [name for name in required_helpers if f"fn {name}(" not in text]
    if not missing:
        return text, 0
    if PARSER_HELPER_SURFACE_ANCHOR not in text:
        return text, 0

    helper_block = """impl Parser {
    fn current_span(&self) -> Span {
        if !self.injected_tokens.is_empty() {
            return self.injected_tokens[0].span;
        }
        self.tokens
            .get(self.pos)
            .map(|token| token.span)
            .unwrap_or(Span { start: 0, end: 0 })
    }

    fn at_end(&self) -> bool {
        matches!(self.peek_kind(), TokenKind::Eof)
    }

    fn check(&self, expected: TokenKind) -> bool {
        self.peek_kind() == expected
    }

    fn advance(&mut self) -> TokenKind {
        if !self.injected_tokens.is_empty() {
            let token = self.injected_tokens.remove(0);
            return token.kind;
        }
        let kind = self
            .tokens
            .get(self.pos)
            .map(|token| token.kind.clone())
            .unwrap_or(TokenKind::Eof);
        if self.pos < self.tokens.len() {
            self.pos += 1;
        }
        kind
    }

    fn skip_newlines(&mut self) {
        while matches!(self.peek_kind(), TokenKind::Newline(_)) {
            self.advance();
        }
    }

    fn parse_item(&mut self) -> Result<Item, KainError> {
        let attrs = self.parse_attributes()?;
        let vis = self.parse_visibility();
        if self.check(TokenKind::Struct) {
            self.parse_struct_with_attrs(vis, attrs)
        } else if self.check(TokenKind::Fn) {
            self.parse_function_with_attrs(vis, attrs)
        } else if self.check(TokenKind::Enum) {
            self.parse_enum(vis)
        } else if self.check(TokenKind::Trait) {
            self.parse_trait(vis)
        } else if self.check(TokenKind::Impl) {
            self.parse_impl()
        } else if self.check(TokenKind::Mod) {
            self.parse_mod(vis)
        } else if self.check(TokenKind::Shader) {
            self.parse_shader()
        } else if self.check(TokenKind::Actor) {
            self.parse_actor_with_attrs(attrs)
        } else if self.check(TokenKind::Const) {
            self.parse_const(vis)
        } else {
            Err(self.parser_error(
                format!("Expected top-level item, found {}", self.token_to_user_string(&self.peek_kind())),
                self.current_span(),
            ))
        }
    }
}

"""
    updated = text.replace(PARSER_HELPER_SURFACE_ANCHOR, f"{helper_block}{PARSER_HELPER_SURFACE_ANCHOR}", 1)
    if updated == text:
        return text, 0
    return updated, len(missing)


def apply_repairs(phase2_root: Path, repaired_root: Path, rules, phase_name: str) -> dict[str, Any]:
    copy_phase2_root(phase2_root, repaired_root)
    replacements_by_rule: dict[str, dict[str, Any]] = {}
    replacements_by_file: defaultdict[str, dict[str, Any]] = defaultdict(lambda: {"replacements": 0, "rules": set()})

    def record_replacement(rule_id: str, hits: int, relative_path: str, confidence: float) -> None:
        if hits <= 0:
            return
        entry = replacements_by_rule.setdefault(
            rule_id,
            {
                "rule_id": rule_id,
                "replacement_count": 0,
                "file_count": 0,
                "files": {},
                "confidence": confidence,
            },
        )
        entry["replacement_count"] += hits
        entry["files"].setdefault(relative_path, 0)
        if entry["files"][relative_path] == 0:
            entry["file_count"] += 1
        entry["files"][relative_path] += hits
        replacements_by_file[relative_path]["replacements"] += hits
        replacements_by_file[relative_path]["rules"].add(rule_id)

    for target_kind in sorted({rule.target_kind for rule in rules if rule.enabled}):
        for file_path in target_files_for_kind(repaired_root, target_kind):
            relative_path = file_path.relative_to(repaired_root).as_posix()
            crate_name = infer_crate_name(relative_path)
            applicable = enabled_rules_for_target(rules, relative_path, crate_name, phase_name, target_kind)
            if not applicable and relative_path != PARSER_IMPL_FRAGMENT_TARGET:
                continue
            original = file_path.read_text(encoding="utf-8", errors="ignore")
            updated = original
            for rule in applicable:
                updated, hits = rule.apply(updated)
                record_replacement(rule.id, hits, relative_path, rule.confidence)
            if relative_path == PARSER_IMPL_FRAGMENT_TARGET:
                updated, inserted_braces = repair_parser_impl_fragmentation(updated)
                record_replacement(PARSER_IMPL_FRAGMENT_RULE_ID, inserted_braces, relative_path, 0.72)
                updated, helper_injections = repair_parser_helper_surface(updated)
                record_replacement(PARSER_HELPER_SURFACE_RULE_ID, helper_injections, relative_path, 0.75)
            if updated != original:
                file_path.write_text(updated, encoding="utf-8")

    file_summaries = [
        {
            "file": file_name,
            "replacements": payload["replacements"],
            "rules_triggered": len(payload["rules"]),
            "rule_ids": sorted(payload["rules"]),
        }
        for file_name, payload in replacements_by_file.items()
    ]
    file_summaries.sort(key=lambda item: (-item["replacements"], item["file"]))

    rule_summaries = list(replacements_by_rule.values())
    rule_summaries.sort(key=lambda item: (-item["replacement_count"], item["rule_id"]))
    total_replacements = sum(item["replacement_count"] for item in rule_summaries)

    return {
        "repaired_root": repaired_root.as_posix(),
        "rule_hits": rule_summaries,
        "files_most_improved": file_summaries,
        "rules_applied": len([item for item in rule_summaries if item["replacement_count"] > 0]),
        "total_replacements": total_replacements,
    }


def run_structural_preflight(repaired_root: Path) -> dict[str, Any]:
    checks = [
        {
            "id": "no_any_none",
            "pattern": ".any(None)",
            "glob": "stage2_workspace/**/*.rs",
        },
        {
            "id": "no_ok_or_else_none",
            "pattern": ".ok_or_else(None)",
            "glob": "stage2_workspace/**/*.rs",
        },
        {
            "id": "no_with_gil_none",
            "pattern": "with_gil(None)",
            "glob": "stage2_workspace/**/*.rs",
        },
        {
            "id": "no_define_native_none",
            "pattern": "define_native(\"",
            "glob": "stage2_workspace/**/*.rs",
        },
        {
            "id": "no_unresolved_memory_helpers",
            "pattern": "bit_width",
            "glob": "stage2_workspace/crates/kain-core/src/lib.rs",
        },
        {
            "id": "no_unresolved_spawn_tx",
            "pattern": " tx ",
            "glob": "stage2_workspace/crates/kain-core/src/lib.rs",
        },
    ]
    results: list[dict[str, Any]] = []
    for check in checks:
        matches: list[str] = []
        for path in repaired_root.glob(check["glob"]):
            text = path.read_text(encoding="utf-8", errors="ignore")
            if check["id"] == "no_define_native_none":
                if "define_native(" in text and ", None);" in text:
                    matches.append(path.relative_to(repaired_root).as_posix())
            elif check["id"] == "no_unresolved_memory_helpers":
                if any(token in text for token in ["bit_width", "field_offset", "field_bit_offset", "bit_signed"]):
                    matches.append(path.relative_to(repaired_root).as_posix())
            elif check["pattern"] in text:
                matches.append(path.relative_to(repaired_root).as_posix())
        results.append(
            {
                "id": check["id"],
                "failed": bool(matches),
                "match_count": len(matches),
                "files": matches[:20],
            }
        )
    return {
        "checks": results,
        "failed": [item["id"] for item in results if item["failed"]],
    }


def run_validation(repaired_root: Path, mode: str) -> ValidationSummary:
    workspace = repaired_root / "stage2_workspace"
    if not workspace.exists():
        return ValidationSummary(ran=False, mode=mode, returncode=None, success=False, log_path=None)

    if mode == "skip":
        return ValidationSummary(ran=False, mode=mode, returncode=None, success=False, log_path=None)

    if mode not in {"check", "build"}:
        raise ValueError(f"Unsupported validation mode: {mode}")

    log_path = workspace / ("stage2_repair_check.log" if mode == "check" else "stage2_repair_build.log")
    command = ["cargo", mode, "-p", "cli", "--bin", "kain"]
    result = subprocess.run(
        command,
        cwd=workspace,
        capture_output=True,
        text=True,
        errors="ignore",
        env=dict(os.environ, PYTHON_EXECUTABLE=str(Path(sys.executable).resolve())),
    )
    log_path.write_text((result.stdout or "") + (result.stderr or ""), encoding="utf-8")
    return ValidationSummary(
        ran=True,
        mode=mode,
        returncode=result.returncode,
        success=result.returncode == 0,
        log_path=log_path.as_posix(),
    )


def suggest_manual_fixes(before: dict[str, Any], after: dict[str, Any]) -> list[str]:
    suggestions: list[str] = []
    after_counts = after.get("bucket_counts", {}) or before.get("bucket_counts", {})
    ranked = sorted(after_counts.items(), key=lambda item: (-item[1], item[0]))
    for bucket_id, count in ranked[:5]:
        if bucket_id == "unknown":
            suggestions.append(f"Investigate unknown bucket with {count} remaining classified errors and add new taxonomy/rules.")
        elif bucket_id == "trait_impl_fidelity":
            suggestions.append("Promote receiver/impl reconstruction from repaired-output rules into upstream emitter logic once patterns stabilize.")
        elif bucket_id in {"span_move", "span_ownership"}:
            suggestions.append("Audit helper-generated span propagation sites and prefer clone-based reuse patterns upstream.")
        elif bucket_id == "memory_lowering_leakage":
            suggestions.append("Treat low-level memory lowering as a single bootstrap-safe family and replace the malformed builder/helper cluster together.")
        elif bucket_id == "spawn_runtime_leakage":
            suggestions.append("Replace the generated spawn/actor bootstrap path as a single family instead of chasing leaked `tx`-style locals.")
        elif bucket_id == "parser_helper_leakage":
            suggestions.append("Normalize the remaining `slice(...)` and parser helper residues with narrow rule-driven replacements or emitter fixes.")
        elif bucket_id == "result_option_unit_coercion":
            suggestions.append("Reduce result/option/unit coercion mismatches by targeting the top generated helper families rather than individual E0308 sites.")
        elif bucket_id == "path_flattening":
            suggestions.append("Expand flattened-path repair rules or upstream normalization for surviving crate/module prefixes.")
        elif bucket_id == "placeholder_none":
            suggestions.append("Replace placeholder combinator fallbacks with typed closures or upstream typed placeholders.")
    return suggestions


def recommended_next_lane(validation: ValidationSummary, after: dict[str, Any], preflight: dict[str, Any]) -> str:
    if preflight.get("failed"):
        return "phase2-core"
    if not validation.ran:
        return "phase2-core"
    if validation.success:
        return "phase2-full"
    after_counts = after.get("bucket_counts", {})
    if after_counts:
        return "phase2-core"
    return "analyze"


def build_report(
    phase_name: str,
    input_root: Path,
    repaired_root: Path,
    build_log_path: Path,
    artifacts: list[str],
    before: dict[str, Any],
    repair_summary: dict[str, Any],
    validation: ValidationSummary,
    after: dict[str, Any],
    enabled_rules: int,
    bootstrap_policy: dict[str, Any],
    preflight: dict[str, Any],
) -> dict[str, Any]:
    after_counts = after.get("bucket_counts", {}) if after else {}
    return {
        "generated_at_utc": utc_now(),
        "phase_name": phase_name,
        "input_root": input_root.as_posix(),
        "repaired_root": repaired_root.as_posix(),
        "build_log_path": build_log_path.as_posix(),
        "input_artifacts": artifacts,
        "bootstrap_policy": bootstrap_policy,
        "summary": {
            "artifacts_processed": len(artifacts),
            "enabled_rules": enabled_rules,
            "rules_applied": repair_summary.get("rules_applied", 0),
            "total_replacements": repair_summary.get("total_replacements", 0),
            "preflight_failures": len(preflight.get("failed", [])),
            "blocker_bucket_count": len(after_counts) if after_counts else len(before.get("bucket_counts", {})),
        },
        "before": before,
        "after": after,
        "rule_hits": repair_summary.get("rule_hits", []),
        "files_most_improved": repair_summary.get("files_most_improved", []),
        "structural_preflight": preflight,
        "files_still_failing_hardest": after.get("hotspots", []) if after else before.get("hotspots", []),
        "unknown_failures": after.get("unknown_samples", []) if after else before.get("unknown_samples", []),
        "validation": validation.__dict__,
        "front_errors": after.get("records", [])[:25] if after else before.get("records", [])[:25],
        "rule_promotions": load_json_payload(DEFAULT_RULE_PROMOTION_LEDGER).get("entries", []),
        "bounded_bootstrap_exceptions": load_json_payload(DEFAULT_BOOTSTRAP_EXCEPTIONS).get("exceptions", []),
        "recommended_next_lane": recommended_next_lane(validation, after, preflight),
        "suggested_next_manual_fixes": suggest_manual_fixes(before, after),
    }


def run_probe_generation(probe_targets_path: Path, probes_root: Path) -> dict[str, Any]:
    probe_targets = load_probe_targets(probe_targets_path)
    return generate_probe_corpus(probe_targets, probes_root)


def main() -> int:
    parser = argparse.ArgumentParser(description="Ouroboros V2 selfhost repair engine")
    parser.add_argument("command", nargs="?", default="run-all", choices=["analyze", "repair", "generate-probes", "run-all"])
    parser.add_argument("--phase-name", default="phase2")
    parser.add_argument("--input-root", type=Path, default=DEFAULT_PHASE2_ROOT)
    parser.add_argument("--repaired-root", type=Path, default=DEFAULT_REPAIRED_ROOT)
    parser.add_argument("--repair-docs", type=Path, default=DEFAULT_REPAIR_DOCS)
    parser.add_argument("--probes-root", type=Path, default=DEFAULT_PROBES_ROOT)
    parser.add_argument("--validation", choices=["skip", "check", "build"], default="check")
    parser.add_argument("--report-json", type=Path, default=DEFAULT_REPORT_JSON)
    parser.add_argument("--report-md", type=Path, default=DEFAULT_REPORT_MD)
    args = parser.parse_args()

    taxonomy = load_taxonomy(args.repair_docs / "error_taxonomy.json")
    rules = load_repair_rules(args.repair_docs / "repair_rules.json")
    bootstrap_policy = load_policy_summary(args.repair_docs / "bootstrap_feature_policy.json")
    artifacts = collect_input_artifacts(args.input_root)
    build_log_path = args.input_root / "stage2_workspace" / "stage2_build.log"
    before = classify_log(build_log_path, taxonomy)

    if args.command == "analyze":
        report = build_report(
            args.phase_name,
            args.input_root,
            args.repaired_root,
            build_log_path,
            artifacts,
            before,
            {"rule_hits": [], "files_most_improved": [], "rules_applied": 0, "total_replacements": 0},
            ValidationSummary(ran=False, mode="skip", returncode=None, success=False, log_path=None),
            {},
            len([rule for rule in rules if rule.enabled]),
            bootstrap_policy,
            {"checks": [], "failed": []},
        )
        write_json(args.report_json, report)
        write_markdown(args.report_md, render_markdown(report))
        print(json.dumps({"report_json": args.report_json.as_posix(), "report_md": args.report_md.as_posix()}, indent=2))
        return 0

    if args.command == "generate-probes":
        summary = run_probe_generation(args.repair_docs / "probe_targets.json", args.probes_root)
        print(json.dumps(summary, indent=2))
        return 0

    repair_summary = apply_repairs(args.input_root, args.repaired_root, rules, args.phase_name)
    preflight = run_structural_preflight(args.repaired_root)

    validation = ValidationSummary(ran=False, mode="skip", returncode=None, success=False, log_path=None)
    after: dict[str, Any] = {}
    if args.command == "run-all" or args.command == "repair":
        validation = run_validation(args.repaired_root, args.validation)
        if validation.log_path:
            after = classify_log(Path(validation.log_path), taxonomy)

    if args.command == "run-all":
        probe_summary = run_probe_generation(args.repair_docs / "probe_targets.json", args.probes_root)
    else:
        probe_summary = None

    report = build_report(
        args.phase_name,
        args.input_root,
        args.repaired_root,
        build_log_path,
        artifacts,
        before,
        repair_summary,
        validation,
        after,
        len([rule for rule in rules if rule.enabled]),
        bootstrap_policy,
        preflight,
    )
    if probe_summary is not None:
        report["probe_corpus"] = probe_summary

    write_json(args.report_json, report)
    write_markdown(args.report_md, render_markdown(report))
    print(json.dumps({
        "report_json": args.report_json.as_posix(),
        "report_md": args.report_md.as_posix(),
        "repaired_root": args.repaired_root.as_posix(),
        "validation_success": validation.success,
    }, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
