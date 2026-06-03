#!/usr/bin/env python3
"""Scaffold annotated Kain error-corpus fixtures.

This is intentionally a generator scaffold, not a closed taxonomy. Add template
builders here when a new Kain diagnostic family becomes repeatable.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Iterable


REPO_ROOT = Path(__file__).resolve().parents[3]
DEFAULT_OUT = REPO_ROOT / "crates" / "semantic" / "error_corpus"


@dataclass(frozen=True)
class Template:
    family: str
    stem: str
    code: str
    mode: str
    repair: str
    body: Callable[[int], str]

    def render(self, index: int, prefix: str) -> tuple[str, str]:
        name = f"{prefix}_{self.stem}_{index:03d}.kn"
        text = "\n".join(
            [
                f"// ERROR: generated {self.family} corpus fixture",
                f"// @expected_code: {self.code}",
                f"// @expected_mode: {self.mode}",
                f"// @expected_repair: {self.repair}",
                self.body(index).rstrip(),
                "",
            ]
        )
        return name, text


def type_typo(index: int) -> str:
    return f"""fn main() -> Int:
    let signal = prntln("semantic typo {index}")
    return 0"""


def wrong_arg_count(index: int) -> str:
    return f"""fn mix_{index}(a: Int, b: Int) -> Int:
    return a + b

fn main() -> Int:
    return mix_{index}(17)"""


def effect_violation(index: int) -> str:
    return f"""fn read_side_{index}() -> String with IO:
    return "semantic side effect"

fn pure_lane_{index}() -> Int with Pure:
    let text = read_side_{index}()
    return len(text)

fn main() -> Int:
    return pure_lane_{index}()"""


def ownership_decay(index: int) -> str:
    return f"""fn main() -> Int with Unsafe:
    let cells: ptr<Int> = alloc_zeroed(4, "Int")
    decay cells
    collapse cells:
        mem_store(cells, {index}, "Int")
    return 0"""


def world_missing_surface(index: int) -> str:
    return f"""world GeneratedWorld{index}:
    state value: Int = {index}

fn main() -> Int:
    return 0"""


def entangle_mismatch(index: int) -> str:
    return f"""world GeneratedMaster{index}:
    state value: Int = {index}

world GeneratedMirror{index}:
    state value_copy: String = "bad"

entangle GeneratedMaster{index}.value <-> GeneratedMirror{index}.value_copy with single_writer

fn main() -> Int:
    return 0"""


def converge_mismatch(index: int) -> str:
    return f"""converge generated_lane_{index}(value: Int) -> Int:
    spec reference:
        return value + 1
    fast wrong_lane when target("llvm"):
        return "not an int"
    verify random(4)

fn main() -> Int:
    return generated_lane_{index}(3)"""


def shader_host_call(index: int) -> str:
    return f"""shader compute GeneratedHostCall{index}(id: UVec3) -> Vec4:
    println("host call from shader")
    return vec4(id.x as Float, 0.0, 0.0, 1.0)"""


def c_abi_boundary(index: int) -> str:
    return f"""include <math.h> as cmath

fn main() -> Int:
    let raw = cmath_sqrt("bad abi value {index}")
    return raw as Int"""


def python_boundary(index: int) -> str:
    return f"""import math as py_math

fn main() -> Int:
    let sqrt_fn = python_getattr_raw(py_math, "sqrt")
    return to_int(python_call_raw(sqrt_fn, ["bad python value {index}"]))"""


TEMPLATES: tuple[Template, ...] = (
    Template("type typo", "type_typo", "KAIN-TYPE-0002", "Typo", "println", type_typo),
    Template("type arity", "wrong_arg_count", "KAIN-TYPE-0025", "GenericUnknown", "add_argument", wrong_arg_count),
    Template("effect boundary", "effect_pure_io", "KAIN-EFFECT-0001", "GenericUnknown", "mark_io", effect_violation),
    Template("ownership", "ownership_decay", "KAIN-BORROW-0004", "OwnershipViolation", "remove_decay", ownership_decay),
    Template("world", "world_missing_surface", "KAIN-TYPE-0003", "MissingSurface", "add_surface", world_missing_surface),
    Template("entangle", "entangle_type_mismatch", "KAIN-TYPE-0003", "EntangleViolation", "match_state_types", entangle_mismatch),
    Template("converge", "converge_mismatch", "KAIN-TYPE-0003", "ConvergeMismatch", "align_fast_lane", converge_mismatch),
    Template("shader", "shader_host_call", "KAIN-SHADER-0001", "ShaderHostBoundary", "remove_host_call", shader_host_call),
    Template("c abi", "c_abi_boundary", "KAIN-CODEGEN-0008", "CAbiBoundary", "cmath_sqrt", c_abi_boundary),
    Template("python", "python_boundary", "KAIN-TYPE-0002", "PythonInteropBoundary", "py_math.sqrt", python_boundary),
)


def select_templates(count: int, families: set[str] | None) -> Iterable[tuple[int, Template]]:
    pool = [template for template in TEMPLATES if not families or template.family in families]
    if not pool:
        raise SystemExit("no templates selected")
    for index in range(count):
        yield index, pool[index % len(pool)]


def write_fixture(path: Path, text: str, overwrite: bool) -> bool:
    if path.exists() and not overwrite:
        return False
    path.write_text(text, encoding="utf-8", newline="\n")
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT, help="fixture output directory")
    parser.add_argument("--count", type=int, default=20, help="number of fixtures to scaffold")
    parser.add_argument("--prefix", default="agent_generated", help="filename prefix")
    parser.add_argument("--family", action="append", help="restrict to a template family; repeatable")
    parser.add_argument("--overwrite", action="store_true", help="allow replacing existing generated files")
    parser.add_argument("--dry-run", action="store_true", help="print planned files without writing")
    args = parser.parse_args()

    if args.count < 1:
        raise SystemExit("--count must be positive")

    out_dir = args.out.resolve()
    families = set(args.family or [])
    planned: list[tuple[Path, str]] = []
    for index, template in select_templates(args.count, families):
        name, text = template.render(index, args.prefix)
        planned.append((out_dir / name, text))

    if args.dry_run:
        for path, _ in planned:
            print(path)
        return 0

    out_dir.mkdir(parents=True, exist_ok=True)
    written = 0
    skipped = 0
    for path, text in planned:
        if write_fixture(path, text, args.overwrite):
            print(f"wrote {path}")
            written += 1
        else:
            print(f"skip existing {path}")
            skipped += 1

    print(f"generated={written} skipped={skipped} out={out_dir}")
    print("next: edit fixtures, then run verify_error_corpus.py --changed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
