from __future__ import annotations

import json
from pathlib import Path
from typing import Any


def load_probe_targets(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def generate_probe_corpus(probe_targets: dict[str, Any], probes_root: Path) -> dict[str, Any]:
    probes_root.mkdir(parents=True, exist_ok=True)
    generated_files: list[str] = []
    category_summaries: list[dict[str, Any]] = []

    for category in probe_targets.get("categories", []):
        output_dir = probes_root / category["output_dir"]
        output_dir.mkdir(parents=True, exist_ok=True)
        count = int(category.get("count", 0))
        generated = []
        for index in range(1, count + 1):
            name = f"{category['prefix']}_{index:03d}.kn"
            target = output_dir / name
            target.write_text(render_probe(category["id"], index), encoding="utf-8")
            generated.append(target.relative_to(probes_root).as_posix())
        generated_files.extend(generated)
        category_summaries.append(
            {
                "category": category["id"],
                "output_dir": category["output_dir"],
                "count": count,
                "themes": list(category.get("themes", [])),
                "generated_files": generated[:5],
            }
        )

    god_files = []
    for entry in probe_targets.get("god_files", []):
        category = entry["category"]
        target = probes_root / entry["name"]
        target.write_text(render_god_probe(entry["name"], category), encoding="utf-8")
        god_files.append(target.relative_to(probes_root).as_posix())
        generated_files.append(target.relative_to(probes_root).as_posix())

    index_payload = {
        "generated_file_count": len(generated_files),
        "categories": category_summaries,
        "god_files": god_files,
    }
    (probes_root / "index.json").write_text(json.dumps(index_payload, indent=2), encoding="utf-8")
    return index_payload


def render_probe(category: str, index: int) -> str:
    if category == "selfhost_core":
        return render_core_probe(index)
    if category == "selfhost_ui":
        return render_ui_probe(index)
    if category == "selfhost_memory":
        return render_memory_probe(index)
    if category == "selfhost_traits":
        return render_traits_probe(index)
    if category == "selfhost_paths":
        return render_paths_probe(index)
    return f"fn probe_{index}() -> Int:\n    {index}\n"


def render_core_probe(index: int) -> str:
    return f"""struct CoreSpanProbe{index:03d}:
    label: String
    span: Span
    nested_span: Span

fn wrap_return_in_poll_ready_{index:03d}(block: Block, span: Span) -> Expr:
    match block:
        Block {{ stmts: stmts, span: block_span }} =>
            if ({index} % 2) == 0:
                Expr::Return(Some(Expr::None(span)), span)
            else:
                Expr::Match {{ scrutinee: Expr::Ident(\"segment\", span), arms: [], span: block_span }}

fn rewrite_access_to_self_{index:03d}(stmt: Stmt, span: Span) -> Stmt:
    match stmt:
        Stmt::Let {{ pattern: Pattern::Binding {{ name: name, mutable: false, span: bind_span }}, value: Some(value), span: stmt_span }} =>
            Stmt::Expr(Expr::Assign {{
                target: Expr::Field {{ object: Expr::Ident(\"self\", span), field: name, span: stmt_span }},
                value: value,
                span: bind_span
            }})
        _ => stmt

fn typed_branch_probe_{index:03d}(segment: String) -> Option<Expr>:
    if segment.ends_with_return:
        none
    else:
        Some(wrap_return_in_poll_ready_{index:03d}(Block {{ stmts: [], span: Span::default() }}, Span::default()))
"""


def render_ui_probe(index: int) -> str:
    return f"""component UiProbe{index:03d}:
    state counter: Int = {index}

fn eval_jsx_probe_{index:03d}(node: JSXNode, span: Span) -> VNode:
    match node:
        JSXNode::Element {{ tag: tag, attrs: attrs, children: children }} =>
            VNode::Element {{
                tag: tag,
                attrs: attrs,
                children: children.map(none),
                key: none
            }}
        JSXNode::Fragment(children) => VNode::Fragment(children.map(none))
        _ => VNode::Text(f\"ui probe {index}\")

fn reconcile_probe_{index:03d}(prev: VNode, next: VNode, span: Span) -> VNode:
    match next:
        VNode::Component {{ instance: instance, rendered: rendered }} =>
            if prev == next:
                none
            else:
                reconcile(prev, rendered, span)
        _ => next
"""


def render_memory_probe(index: int) -> str:
    return f"""struct MemoryLayoutProbe{index:03d}:
    bits: Int
    align: Int
    span: Span

fn lower_type_memory_probe_{index:03d}(ty: Type, span: Span) -> Type:
    match ty:
        Type::Array(inner, size, type_span) => Type::Array(lower_type_memory(inner), size, type_span)
        Type::Slice(inner, type_span) => Type::Slice(lower_type_memory(inner), type_span)
        Type::Ref {{ mutable_: mutable_, inner: inner, lifetime: lifetime, span: type_span }} =>
            Type::Ref {{ mutable_: mutable_, inner: lower_type_memory(inner), lifetime: lifetime, span: type_span }}
        Type::Result(ok, err, type_span) =>
            Type::Result(lower_type_memory(ok), lower_type_memory(err), type_span)
        _ => ty

fn pointer_probe_{index:03d}(ptr: Ptr<Int>, span: Span) -> Ptr<Int>:
    mem_store(ptr, {index})
    ptr_offset(ptr, {index})
"""


def render_traits_probe(index: int) -> str:
    return f"""enum TraitProbeValue{index:03d}:
    Unit
    Text(String)

impl Display for TraitProbeValue{index:03d}:
    fn fmt(_self: &Self_, f: &mut Formatter) -> Result<(), Error>:
        match _self:
            TraitProbeValue{index:03d}::Unit => write(f, \"unit-{index}\")
            TraitProbeValue{index:03d}::Text(value) => write(f, value)

impl Default for TraitProbeValue{index:03d}:
    fn default_() -> Self_:
        TraitProbeValue{index:03d}__Unit

fn trait_helper_probe_{index:03d}(value: TraitProbeValue{index:03d}) -> String:
    format(\"{{}}\", value)
"""


def render_paths_probe(index: int) -> str:
    return f"""enum crate__foo__bar__PathProbe{index:03d}:
    Leaf
    Node(String)
    Rich {{ path: String, span: Span }}

fn flatten_probe_{index:03d}(value: crate__foo__bar__PathProbe{index:03d}, span: Span) -> Option<String>:
    match value:
        crate__foo__bar__PathProbe{index:03d}::Leaf => none
        crate__foo__bar__PathProbe{index:03d}::Node(path) => Some(path)
        crate__foo__bar__PathProbe{index:03d}::Rich {{ path: path, span: rich_span }} =>
            Some(path.trim_start_matches(crate__prefix__value))

impl PathOwner{index:03d}:
    fn Self___normalize(_self: &Self_, value: String) -> String:
        value.strip_prefix(needle_{index})
"""


def render_god_probe(name: str, category: str) -> str:
    body = [
        f"# god probe: {name}",
        render_core_probe(900),
        render_ui_probe(901),
        render_memory_probe(902),
        render_traits_probe(903),
        render_paths_probe(904),
    ]
    if category == "selfhost_ui":
        body.append(render_ui_probe(905))
    return "\n\n".join(body) + "\n"
