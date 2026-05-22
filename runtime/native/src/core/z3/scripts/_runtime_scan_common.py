"""Shared helpers for the native-core Z3 bounty-hunter scripts."""

from __future__ import annotations

import csv
import json
import re
from pathlib import Path
from typing import Iterable

SCRIPT_DIR = Path(__file__).resolve().parent
Z3_ROOT = SCRIPT_DIR.parent
CORE_ROOT = Z3_ROOT.parent
DATA_DIR = Z3_ROOT / "data"
GENERATED_DIR = Z3_ROOT / "generated" / "auto_z3"

SOURCE_FILE_GLOB = "*.c"
CONTROL_KEYWORDS = {
    "if",
    "for",
    "while",
    "switch",
    "return",
    "sizeof",
}
SIZE_TERMS = (
    "size",
    "length",
    "capacity",
    "count",
    "offset",
    "stride",
    "index",
    "slot",
    "probe",
    "cursor",
    "generation",
    "header",
    "payload",
    "bytes",
    "body",
    "path",
    "mask",
)
GUARD_TOKENS = (
    "SIZE_MAX",
    "INT64_MAX",
    "INT64_MIN",
    "UINTPTR_MAX",
    "ULONG_MAX",
    "U64_MAX",
    "U32_MAX",
    "S64_MAX",
    "S64_MIN",
    "S32_MAX",
    "S32_MIN",
    "overflow",
    "underflow",
    "kain_add_overflow",
    "kain_mul_overflow",
    "kain_sub_underflow",
    "abi_net_size_add_overflow",
)
LOCK_TOKENS = (
    "pthread_mutex_lock(",
    "pthread_mutex_unlock(",
    "EnterCriticalSection(",
    "LeaveCriticalSection(",
    "kain_async_mutex_lock(",
    "kain_async_mutex_unlock(",
    "kain_ownership_lock(",
    "kain_ownership_unlock(",
)
ONCE_TOKENS = (
    "pthread_once(",
    "InitOnceExecuteOnce(",
    "call_once(",
    "std::call_once(",
)
ATOMIC_TOKENS = (
    "atomic_",
    "__atomic_",
    "Interlocked",
)
DESTROY_TOKENS = (
    "pthread_mutex_destroy(",
    "pthread_cond_destroy(",
    "DeleteCriticalSection(",
    "CloseHandle(",
    "free(",
)
ARITHMETIC_OPERATORS = ("<<", ">>", "*", "+", "-")
ALLOC_SINK_TOKENS = (
    "malloc(",
    "calloc(",
    "realloc(",
    "memcpy(",
    "memmove(",
    "memset(",
    "snprintf(",
)


def ensure_output_dirs() -> None:
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    GENERATED_DIR.mkdir(parents=True, exist_ok=True)


def iter_core_sources() -> list[Path]:
    return sorted(CORE_ROOT.glob(SOURCE_FILE_GLOB))


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def split_lines(text: str) -> list[str]:
    return text.splitlines()


def compact_spaces(value: str) -> str:
    return re.sub(r"\s+", " ", value).strip()


def is_function_signature(signature: str) -> bool:
    compact = compact_spaces(signature)
    if not compact or "(" not in compact or ")" not in compact:
        return False
    if compact.endswith(";"):
        return False
    if "=" in compact and "{" not in compact:
        return False
    head = compact.split("(", 1)[0].strip()
    if not head:
        return False
    name = head.split()[-1].lstrip("*")
    if name in CONTROL_KEYWORDS:
        return False
    return bool(re.match(r"^[A-Za-z_]\w*$", name))


def extract_function_name(signature: str) -> str:
    compact = compact_spaces(signature)
    matches = re.findall(r"([A-Za-z_]\w*)\s*\(", compact)
    if not matches:
        return "<unknown>"
    for name in reversed(matches):
        if name not in CONTROL_KEYWORDS:
            return name
    return matches[-1]


def extract_functions(path: Path) -> list[dict]:
    lines = split_lines(read_text(path))
    functions: list[dict] = []
    index = 0
    while index < len(lines):
        start = index
        signature_lines: list[str] = []
        cursor = index
        found_function = False
        while cursor < len(lines) and cursor - start <= 12:
            raw = lines[cursor]
            stripped = raw.strip()
            if stripped.startswith("#"):
                break
            signature_lines.append(raw)
            joined = compact_spaces(" ".join(signature_lines))
            if ";" in stripped and "{" not in stripped:
                break
            if "{" in raw:
                if is_function_signature(joined):
                    name = extract_function_name(joined)
                    brace_depth = raw.count("{") - raw.count("}")
                    end = cursor
                    while end + 1 < len(lines) and brace_depth > 0:
                        end += 1
                        brace_depth += lines[end].count("{") - lines[end].count("}")
                    functions.append(
                        {
                            "file": str(path),
                            "name": name,
                            "start_line": start + 1,
                            "end_line": end + 1,
                            "signature": joined,
                            "body": "\n".join(lines[start : end + 1]),
                            "lines": lines[start : end + 1],
                        }
                    )
                    index = end + 1
                    found_function = True
                break
            cursor += 1
        if not found_function:
            index += 1
    return functions


def count_token_hits(text: str, tokens: Iterable[str]) -> int:
    return sum(text.count(token) for token in tokens)


def find_token_hits(text: str, tokens: Iterable[str]) -> list[str]:
    return sorted({token for token in tokens if token in text})


def nearby_guard(lines: list[str], line_index: int, window: int = 6) -> list[str]:
    start = max(0, line_index - window)
    end = min(len(lines), line_index + window + 1)
    window_text = "\n".join(lines[start:end])
    return find_token_hits(window_text, GUARD_TOKENS)


def arithmetic_ops_in_line(line: str) -> list[str]:
    hits: list[str] = []
    if "<<" in line:
        hits.append("<<")
    if ">>" in line:
        hits.append(">>")
    if "*" in line and "/*" not in line:
        hits.append("*")
    plus_count = line.count("+")
    if plus_count > line.count("++"):
        hits.append("+")
    minus_count = line.count("-")
    if minus_count > (line.count("--") + line.count("->")):
        hits.append("-")
    return hits


def stable_relpath(path: str | Path) -> str:
    return str(Path(path).resolve().relative_to(CORE_ROOT))


def write_json_and_csv(stem: str, rows: list[dict]) -> tuple[Path, Path]:
    ensure_output_dirs()
    json_path = DATA_DIR / f"{stem}.json"
    csv_path = DATA_DIR / f"{stem}.csv"
    json_path.write_text(json.dumps(rows, indent=2), encoding="utf-8")
    fieldnames: list[str] = []
    for row in rows:
        for key in row:
            if key not in fieldnames:
                fieldnames.append(key)
    with csv_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        for row in rows:
            writer.writerow(row)
    return json_path, csv_path


def print_top_table(title: str, rows: list[dict], columns: list[tuple[str, int]], limit: int = 20) -> None:
    print(title)
    if not rows:
        print("  <no rows>")
        return
    header = " | ".join(name.ljust(width) for name, width in columns)
    print(header)
    print("-" * len(header))
    for row in rows[:limit]:
        pieces = []
        for name, width in columns:
            value = str(row.get(name, ""))[:width]
            pieces.append(value.ljust(width))
        print(" | ".join(pieces))


def function_line_excerpt(lines: list[str], limit: int = 3) -> str:
    payload = []
    for raw in lines:
        stripped = raw.strip()
        if stripped:
            payload.append(compact_spaces(stripped))
        if len(payload) >= limit:
            break
    return " || ".join(payload)
