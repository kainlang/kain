from __future__ import annotations

import fnmatch
import json
import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Iterable


@dataclass
class Scope:
    globs: list[str] = field(default_factory=list)
    crate_names: list[str] = field(default_factory=list)


@dataclass
class RepairRule:
    id: str
    description: str
    target_kind: str
    match_type: str
    pattern: str
    replacement: str
    scope: Scope
    phase: list[str]
    severity: str
    enabled: bool
    notes: str = ""
    confidence: float | None = None

    def applies_to(self, relative_path: str, crate_name: str | None, phase_name: str, target_kind: str) -> bool:
        if not self.enabled:
            return False
        if self.target_kind != target_kind:
            return False
        if self.phase:
            normalized = {phase_name}
            if "-" in phase_name:
                normalized.add(phase_name.split("-", 1)[0])
            if not any(phase in normalized for phase in self.phase):
                return False
        if self.scope.globs and not any(fnmatch.fnmatch(relative_path, pattern) for pattern in self.scope.globs):
            return False
        if self.scope.crate_names and crate_name not in self.scope.crate_names:
            return False
        return True

    def apply(self, text: str) -> tuple[str, int]:
        if self.match_type == "literal":
            return text.replace(self.pattern, self.replacement), text.count(self.pattern)
        if self.match_type == "regex":
            return re.subn(self.pattern, self.replacement, text, flags=re.MULTILINE | re.DOTALL)
        raise ValueError(f"Unsupported match_type for rule {self.id}: {self.match_type}")


@dataclass
class TaxonomyBucket:
    id: str
    description: str
    severity: str
    regexes: list[str]
    candidate_rule_ids: list[str]

    def matches(self, text: str) -> bool:
        return any(re.search(pattern, text, flags=re.IGNORECASE | re.MULTILINE) for pattern in self.regexes)


@dataclass
class Taxonomy:
    buckets: list[TaxonomyBucket]

    def classify(self, text: str) -> TaxonomyBucket:
        for bucket in self.buckets:
            if bucket.id == "unknown":
                continue
            if bucket.matches(text):
                return bucket
        return self.unknown_bucket()

    def unknown_bucket(self) -> TaxonomyBucket:
        for bucket in self.buckets:
            if bucket.id == "unknown":
                return bucket
        raise ValueError("Taxonomy must include an 'unknown' bucket")


def _read_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def load_repair_rules(path: Path) -> list[RepairRule]:
    payload = _read_json(path)
    rules: list[RepairRule] = []
    for entry in payload.get("rules", []):
        scope_payload = entry.get("scope", {})
        rules.append(
            RepairRule(
                id=entry["id"],
                description=entry["description"],
                target_kind=entry["target_kind"],
                match_type=entry["match_type"],
                pattern=entry["pattern"],
                replacement=entry.get("replacement", ""),
                scope=Scope(
                    globs=list(scope_payload.get("globs", [])),
                    crate_names=list(scope_payload.get("crate_names", [])),
                ),
                phase=list(entry.get("phase", [])),
                severity=entry.get("severity", "medium"),
                enabled=bool(entry.get("enabled", True)),
                notes=entry.get("notes", ""),
                confidence=entry.get("confidence"),
            )
        )
    return rules


def load_taxonomy(path: Path) -> Taxonomy:
    payload = _read_json(path)
    buckets = [
        TaxonomyBucket(
            id=entry["id"],
            description=entry["description"],
            severity=entry.get("severity", "review"),
            regexes=list(entry.get("regexes", [])),
            candidate_rule_ids=list(entry.get("candidate_rule_ids", [])),
        )
        for entry in payload.get("buckets", [])
    ]
    return Taxonomy(buckets=buckets)


def infer_crate_name(relative_path: str) -> str | None:
    normalized = relative_path.replace("\\", "/")
    parts = [part for part in normalized.split("/") if part]
    for index, part in enumerate(parts):
        if part == "crates" and index + 1 < len(parts):
            return parts[index + 1]
    if parts:
        stem = Path(parts[-1]).stem
        if stem in {"kain-core", "kain-import", "kain-sys-codegen", "cli"}:
            return stem
    return None


def load_probe_targets(path: Path) -> dict[str, Any]:
    return _read_json(path)


def load_bootstrap_feature_policy(path: Path) -> dict[str, Any]:
    return _read_json(path)


def enabled_rules_for_target(
    rules: Iterable[RepairRule],
    relative_path: str,
    crate_name: str | None,
    phase_name: str,
    target_kind: str,
) -> list[RepairRule]:
    return [rule for rule in rules if rule.applies_to(relative_path, crate_name, phase_name, target_kind)]
