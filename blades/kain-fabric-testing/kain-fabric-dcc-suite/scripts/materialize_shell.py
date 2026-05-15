#!/usr/bin/env python3

from __future__ import annotations

import json
from pathlib import Path


APP_ROOT = Path(__file__).resolve().parent.parent
GENERATED_ROOT = APP_ROOT / "generated"


def load_json(relative_path: str):
    return json.loads((APP_ROOT / relative_path).read_text(encoding="utf-8"))


def text_line(indent: str, role: str, content: str) -> str:
    escaped = (
        content.replace("&", "&amp;")
        .replace("<", "&lt;")
        .replace(">", "&gt;")
        .replace('"', "&quot;")
    )
    return f'{indent}<text role="{role}">{escaped}</text>'


def main() -> None:
    manifest = load_json("config/app_manifest.json")
    modes = load_json("config/workspace_modes.json")["modes"]
    surfaces = load_json("config/surfaces.json")["surfaces"]
    tools = load_json("config/tool_catalog.json")["tools"]
    commands = load_json("config/command_registry.json")["commands"]
    runtime_lanes = load_json("config/runtime_lanes.json")["runtime_lanes"]
    runtime_packs = load_json("config/runtime_packs.json")["runtime_packs"]
    snapshot_path = APP_ROOT / "state" / "runtime_snapshot.json"
    snapshot = json.loads(snapshot_path.read_text(encoding="utf-8")) if snapshot_path.exists() else {}
    dcc_state = snapshot.get("dcc_suite_state", {})
    parity = dcc_state.get("parity_matrix", {})

    center_surface = next(
        (surface for surface in surfaces if surface.get("dock") == "center" and surface.get("kind") == "viewport3d"),
        None,
    )
    viewport_title = center_surface["title"] if center_surface else "Viewport Stage"
    viewport_scene = center_surface.get("scene", "empty_scene") if center_surface else "empty_scene"
    runtime_health = dcc_state.get("runtime_lane_health", "warming")
    runtime_health_detail = dcc_state.get("runtime_lane_health_detail", "bridge warming / fabric warming")
    fabric_status = dcc_state.get("latest_fabric_run", {}).get("status", snapshot.get("recent_sessions", [{}])[0].get("status", "idle"))
    command_count = len(commands)
    pack_count = len(runtime_packs)
    parity_count = parity.get("capability_count", 0)
    parity_status_summary = parity.get("status_summary", "reference_only=0")

    lines = [
        "component App():",
        f'    render <panel title="{manifest["window_title"]}" layout="dock" persistent_layout_id="{manifest["layout_id"]}" gap={{12}} padding={{12}}>',
        '        <panel title="Workspace Modes" dock="left" split_ratio={0.22} min_width={250} max_width={360} resizable={true} layout="column" gap={10}>',
        '            <tree title="Mode Rail">',
    ]
    for mode in modes:
        lines.append(text_line("                ", "body", f'{mode["label"]} [{mode["id"]}]'))
        lines.append(text_line("                ", "caption", mode["summary"]))
    lines.extend(
        [
            "            </tree>",
            '            <inspector title="Bridge Status">',
            text_line("                ", "metric", runtime_health),
            text_line("                ", "caption", runtime_health_detail),
            text_line("                ", "caption", f"fabric status: {fabric_status}"),
            text_line("                ", "caption", f"commands: {command_count} | runtime packs: {pack_count}"),
            "            </inspector>",
            "        </panel>",
            '        <panel title="Viewport Workbench" dock="center" layout="column" gap={10} flex_grow={1} overflow="hidden">',
            '            <panel title="Session Deck" layout="row" gap={10}>',
            '                <inspector title="Project">',
            text_line("                    ", "eyebrow", manifest["name"]),
            text_line("                    ", "caption", "Linux materialized shell"),
            text_line("                    ", "caption", "Fabric-first DCC workstation bootstrap"),
            "                </inspector>",
            '                <inspector title="Mode Summary">',
            text_line("                    ", "caption", "layout / model / sculpt / paint / lookdev / render"),
            text_line("                    ", "caption", "Bridge-backed session + runtime snapshot"),
            text_line("                    ", "caption", f"parity capabilities: {parity_count}"),
            text_line("                    ", "caption", f"parity status: {parity_status_summary}"),
            text_line("                    ", "caption", "Static shell path until Fabric graph is healthy"),
            "                </inspector>",
            "            </panel>",
            f'            <viewport3d title="{viewport_title}" scene="{viewport_scene}" />',
            '            <panel title="Workbench Telemetry" layout="row" gap={10}>',
            '                <graph title="Runtime Lanes" />',
            '                <timeline title="Jobs Monitor" />',
            "            </panel>",
            "        </panel>",
            '        <panel title="Registry Inspectors" dock="right" split_ratio={0.24} min_width={280} max_width={420} resizable={true} layout="column" gap={10}>',
            '            <inspector title="Active Tools">',
        ]
    )
    for tool in tools[:8]:
        lines.append(text_line("                ", "body", f'{tool["label"]} [{tool["lane"]}]'))
        lines.append(text_line("                ", "caption", tool["summary"]))
    lines.extend(
        [
            "            </inspector>",
            '            <inspector title="Runtime Ownership">',
        ]
    )
    for lane in runtime_lanes:
        lines.append(text_line("                ", "body", f'{lane["label"]} [{lane["runtime"]}]'))
        lines.append(text_line("                ", "caption", lane["summary"]))
    lines.extend(
        [
            "            </inspector>",
            "        </panel>",
            '        <panel title="Command Deck" dock="bottom" split_ratio={0.24} min_height={180} max_height={320} resizable={true} layout="row" gap={10}>',
            '            <timeline title="Command Registry">',
        ]
    )
    for command in commands[:10]:
        lines.append(text_line("                ", "body", f'{command["label"]} [{command["id"]}]'))
        lines.append(text_line("                ", "caption", f'surface={command["surface"]} | intent={command["intent"]}'))
    lines.extend(
        [
            "            </timeline>",
            '            <inspector title="Surface Registry">',
        ]
    )
    for surface in surfaces[:8]:
        lines.append(text_line("                ", "body", f'{surface["title"]} [{surface["kind"]}]'))
        lines.append(text_line("                ", "caption", f'dock={surface["dock"]} | id={surface["id"]}'))
    lines.extend(
        [
            "            </inspector>",
            "        </panel>",
            "    </panel>",
        ]
    )

    GENERATED_ROOT.mkdir(parents=True, exist_ok=True)
    output_path = GENERATED_ROOT / "main.generated.kn"
    output_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Materialized {output_path}")


if __name__ == "__main__":
    main()
