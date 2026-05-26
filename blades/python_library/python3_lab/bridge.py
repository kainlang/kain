"""
python3_lab/bridge.py — Flet bridge for Kain interop proving ground.

Kain owns the architecture (worlds, actors, shatter, teleport, laws, patches).
Flet owns the widget tree and pixel rendering.
This bridge translates Kain's plan into a live Flet desktop application.
"""
import json
import sys
import os

try:
    import flet as ft
    FLET_AVAILABLE = True
except ImportError:
    FLET_AVAILABLE = False
    ft = None


def module_digest(plan_text: str) -> int:
    """Verify flet is importable and return a stable version-based score.
    Returns 0 if flet is not available."""
    if not FLET_AVAILABLE:
        return 0
    try:
        ver = ft.__version__
        return sum(ord(c) for c in ver) % 1000003
    except Exception:
        return 0


def flet_version() -> str:
    """Return the installed flet version string."""
    if not FLET_AVAILABLE:
        return ""
    try:
        return ft.__version__
    except Exception:
        return ""


def run_flet_app(plan_text: str) -> str:
    """Execute a Flet desktop app driven by Kain's plan JSON.

    Kain writes the plan. Flet renders the widget tree.
    When the user closes the window, we return a report JSON back to Kain
    so the architecture can validate and score the session.
    """
    if not FLET_AVAILABLE:
        return json.dumps({"status": "flet_not_available", "frames": 0, "score": 0})
    try:
        plan = json.loads(plan_text)
    except json.JSONDecodeError:
        return json.dumps({"status": "bad_plan_json", "frames": 0, "score": 0})

    result = {"status": "running", "frames": 0, "score": 0, "errors": []}

    def main(page: ft.Page):
        nonlocal result

        # --- Page setup from plan ---
        page.title = plan.get("title", "Kain Flet Proving Ground")
        page.window.width = plan.get("window_width", 900)
        page.window.height = plan.get("window_height", 640)
        page.theme_mode = (
            ft.ThemeMode.DARK
            if plan.get("theme_mode", "dark") == "dark"
            else ft.ThemeMode.LIGHT
        )
        page.padding = 24
        page.scroll = ft.ScrollMode.AUTO

        # --- Live state mirrors Kain's world slots ---
        frame_count = [0]
        signal_history = []

        # ====================================================================
        #                        COUNTER HUB
        # ====================================================================
        counter_value = [0]

        def increment(e):
            frame_count[0] += 1
            counter_value[0] += 1
            counter_display.value = str(counter_value[0])
            _update_signal_log(page, signal_history, frame_count[0], counter_value[0])
            page.update()

        def decrement(e):
            frame_count[0] += 1
            counter_value[0] -= 1
            counter_display.value = str(counter_value[0])
            _update_signal_log(page, signal_history, frame_count[0], counter_value[0])
            page.update()

        def reset_counter(e):
            frame_count[0] += 1
            counter_value[0] = 0
            counter_display.value = "0"
            _update_signal_log(page, signal_history, frame_count[0], counter_value[0])
            page.update()

        counter_display = ft.Text(value="0", size=48, weight=ft.FontWeight.BOLD)
        counter_hub = ft.Column([
            ft.Text("Counter Hub", size=18, weight=ft.FontWeight.BOLD),
            ft.Row([
                ft.IconButton(ft.Icons.REMOVE_CIRCLE, on_click=decrement, icon_size=32),
                ft.Container(content=counter_display, width=120, alignment=ft.alignment.center),
                ft.IconButton(ft.Icons.ADD_CIRCLE, on_click=increment, icon_size=32),
                ft.IconButton(ft.Icons.REFRESH, on_click=reset_counter, icon_size=28),
            ], alignment=ft.MainAxisAlignment.CENTER),
        ])

        # ====================================================================
        #                        ACTOR STATUS PANEL
        # ====================================================================
        relay_bias = plan.get("relay_bias", 31)
        authority_seed = plan.get("authority_seed", 17)
        actor_status = ft.Column([
            ft.Text("Actor Status", size=18, weight=ft.FontWeight.BOLD),
            ft.Row([
                ft.Text(f"Relay Bias: {relay_bias}", italic=True),
                ft.Text(f" |  Seed: {authority_seed}", italic=True),
            ]),
            ft.Text(f"Turns simulated: {plan.get('rounds', 0)}", size=14),
            ft.Text(f"Teleport vector: [{plan.get('teleport_bias',0)}, {plan.get('teleport_phase',0)}, {plan.get('teleport_salt',0)}]", size=14),
            ft.Text(f"Flet bridge: {flet_version()}", size=12, color=ft.Colors.GREY_400),
        ])

        # ====================================================================
        #                        SIGNAL HISTORY TABLE
        # ====================================================================
        signal_table = ft.DataTable(
            columns=[
                ft.DataColumn(ft.Text("Frame")),
                ft.DataColumn(ft.Text("Counter")),
                ft.DataColumn(ft.Text("Signal")),
                ft.DataColumn(ft.Text("Checksum")),
            ],
            rows=[],
            border=ft.border.all(1, ft.Colors.GREY_800),
        )

        def _update_signal_log(page, history, frame, counter):
            history.append((frame, counter))
            if len(history) > 16:
                history.pop(0)
            rows = []
            modulus = 1000000007
            for i, (f, c) in enumerate(reversed(history)):
                signal = ((f * 13) + (c * 7) + 37) % modulus
                checksum = (signal + f + c) % modulus
                rows.append(ft.DataRow(cells=[
                    ft.DataCell(ft.Text(str(f))),
                    ft.DataCell(ft.Text(str(c))),
                    ft.DataCell(ft.Text(str(signal % 10000))),
                    ft.DataCell(ft.Text(str(checksum % 10000))),
                ]))
            signal_table.rows = rows

        data_panel = ft.Column([
            ft.Text("Signal History", size=18, weight=ft.FontWeight.BOLD),
            ft.Container(content=signal_table, height=280),
        ])

        # ====================================================================
        #                        TELEPORT VIEW
        # ====================================================================
        teleport_log = ft.Column([
            ft.Text("Teleport Log", size=18, weight=ft.FontWeight.BOLD),
            ft.Text("shard struct teleport between Authority ↔ Mirror", size=13, color=ft.Colors.GREY_400),
            ft.Text(f"  bias={plan.get('teleport_bias',0)}  phase={plan.get('teleport_phase',0)}  salt={plan.get('teleport_salt',0)}", size=13),
            ft.Text("  via flet_pulse_bus  |  single_writer entangle", size=13, color=ft.Colors.GREY_400),
        ])

        # ====================================================================
        #                        MAIN LAYOUT
        # ====================================================================
        header = ft.Row([
            ft.Icon(ft.Icons.DASHBOARD, size=36, color=ft.Colors.BLUE_400),
            ft.Column([
                ft.Text("Kain // Flet Interop", size=28, weight=ft.FontWeight.BOLD),
                ft.Text("Kain owns state. Flet owns pixels. Architecture flows through worlds, actors, shatter, teleport, and entangle.", size=13, color=ft.Colors.GREY_400),
            ]),
        ])

        page.add(
            header,
            ft.Divider(height=24),
            ft.Row([
                ft.Container(content=actor_status, expand=1, padding=12),
                ft.Container(content=counter_hub, expand=1, padding=12),
            ]),
            ft.Divider(height=24),
            ft.Row([
                ft.Container(content=data_panel, expand=2, padding=12),
                ft.Container(content=teleport_log, expand=1, padding=12),
            ]),
        )

        # Seed initial rows
        _update_signal_log(page, signal_history, 0, 0)
        page.update()

        # --- Window closed: finalize report ---
        result["status"] = "ok"
        result["frames"] = frame_count[0]
        result["score"] = (
            counter_value[0] * 17 + frame_count[0] * 31 + len(signal_history) * 7
        ) % 1000000007
        result["final_counter"] = counter_value[0]
        result["flet_version"] = flet_version()

    # --- Run the Flet desktop app (blocks until window closes) ---
    try:
        ft.app(target=main)
    except Exception as exc:
        result["status"] = "flet_crash"
        result["errors"].append(str(exc))

    return json.dumps(result)
