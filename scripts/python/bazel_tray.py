#!/usr/bin/env python3
from __future__ import annotations

import argparse
import ctypes
import os
import shutil
import subprocess
import sys
import threading
from dataclasses import dataclass
from pathlib import Path

from kain_bazel_sync import resolve_repo_root


IS_WINDOWS = os.name == "nt"

WM_USER = 0x0400
WM_APP = 0x8000
WM_TIMER = 0x0113
WM_DESTROY = 0x0002
WM_COMMAND = 0x0111
WM_LBUTTONUP = 0x0202
WM_RBUTTONUP = 0x0205

WM_TRAYICON = WM_USER + 1
WM_REFRESH_STATUS = WM_APP + 1

NIM_ADD = 0x00000000
NIM_MODIFY = 0x00000001
NIM_DELETE = 0x00000002

NIF_MESSAGE = 0x00000001
NIF_ICON = 0x00000002
NIF_TIP = 0x00000004

MF_STRING = 0x00000000
MF_GRAYED = 0x00000001
MF_DISABLED = 0x00000002
MF_SEPARATOR = 0x00000800

TPM_LEFTALIGN = 0x0000
TPM_BOTTOMALIGN = 0x0020
TPM_RIGHTBUTTON = 0x0002

ID_REFRESH = 1001
ID_SHUTDOWN = 1002
ID_OPEN_OUTPUT_BASE = 1003
ID_EXIT = 1004

IDI_APPLICATION = 32512
IDI_ERROR = 32513
IDI_WARNING = 32515
IDI_INFORMATION = 32516

PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
STILL_ACTIVE = 259


class BazelTrayError(RuntimeError):
    pass


@dataclass(frozen=True)
class BazelServerStatus:
    output_base: Path | None
    pid_file: Path | None
    pid: int | None
    state: str
    summary: str

    @property
    def running(self) -> bool:
        return self.state == "running"

    @property
    def stale(self) -> bool:
        return self.state == "stale"


def resolve_bazel_command() -> str:
    override = os.environ.get("KAIN_BAZEL_COMMAND", "").strip()
    if override:
        resolved = shutil.which(override)
        return resolved or override
    for candidate in ("bazel", "bazelisk"):
        resolved = shutil.which(candidate)
        if resolved:
            return resolved
    raise BazelTrayError("unable to find bazel or bazelisk on PATH")


def resolve_output_base(repo_root: Path, bazel_command: str) -> Path:
    override = os.environ.get("KAIN_BAZEL_OUTPUT_BASE", "").strip()
    if override:
        return Path(override).expanduser().resolve()
    result = subprocess.run(
        [bazel_command, "--batch", "info", "output_base", "--config=dev"],
        cwd=str(repo_root),
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
        check=True,
    )
    for line in result.stdout.splitlines():
        candidate = line.strip()
        if candidate:
            return Path(candidate).resolve()
    raise BazelTrayError("bazel did not return an output base")


def read_pid_file(output_base: Path) -> tuple[Path | None, int | None]:
    pid_file = output_base / "server" / "server.pid.txt"
    if not pid_file.exists():
        return pid_file, None
    try:
        raw = pid_file.read_text(encoding="utf-8").strip()
        return pid_file, int(raw)
    except (OSError, ValueError):
        return pid_file, None


def is_pid_running(pid: int) -> bool:
    if not IS_WINDOWS:
        return False
    kernel32 = ctypes.windll.kernel32
    handle = kernel32.OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, False, pid)
    if not handle:
        return False
    try:
        exit_code = ctypes.c_ulong()
        if not kernel32.GetExitCodeProcess(handle, ctypes.byref(exit_code)):
            return False
        return exit_code.value == STILL_ACTIVE
    finally:
        kernel32.CloseHandle(handle)


def inspect_bazel_server(output_base: Path) -> BazelServerStatus:
    pid_file, pid = read_pid_file(output_base)
    if pid is None:
        if pid_file.exists():
            return BazelServerStatus(output_base, pid_file, None, "stale", "server pid file is unreadable")
        return BazelServerStatus(output_base, None, None, "stopped", "bazel server is not running")
    if is_pid_running(pid):
        return BazelServerStatus(output_base, pid_file, pid, "running", f"bazel server is running (pid {pid})")
    return BazelServerStatus(output_base, pid_file, pid, "stale", f"bazel pid file is stale (pid {pid})")


def shutdown_bazel_server(repo_root: Path, bazel_command: str, output_base: Path) -> None:
    subprocess.run(
        [bazel_command, "shutdown"],
        cwd=str(repo_root),
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        timeout=30,
        check=False,
    )
    status = inspect_bazel_server(output_base)
    if status.running and status.pid is not None:
        subprocess.run(
            ["taskkill", "/PID", str(status.pid), "/T", "/F"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=False,
        )


def open_output_base(output_base: Path) -> None:
    if not IS_WINDOWS:
        raise BazelTrayError("output base explorer is only supported on Windows")
    os.startfile(str(output_base))


def load_stock_icon(icon_id: int) -> int:
    if not IS_WINDOWS:
        raise BazelTrayError("tray icons are only supported on Windows")
    user32 = ctypes.windll.user32
    icon = user32.LoadIconW(None, ctypes.c_void_p(icon_id))
    if not icon:
        raise ctypes.WinError()
    return icon


def make_menu_text(status: BazelServerStatus) -> str:
    if status.running:
        return status.summary
    if status.stale:
        return status.summary
    return status.summary


class TrayIconData(ctypes.Structure):
    _fields_ = [
        ("cbSize", ctypes.c_ulong),
        ("hWnd", ctypes.c_void_p),
        ("uID", ctypes.c_uint),
        ("uFlags", ctypes.c_uint),
        ("uCallbackMessage", ctypes.c_uint),
        ("hIcon", ctypes.c_void_p),
        ("szTip", ctypes.c_wchar * 128),
    ]


class POINT(ctypes.Structure):
    _fields_ = [("x", ctypes.c_long), ("y", ctypes.c_long)]


def _make_window_class_name() -> str:
    return "KainBazelTrayWindow"


class BazelTrayApp:
    def __init__(self, repo_root: Path, bazel_command: str, output_base: Path) -> None:
        if not IS_WINDOWS:
            raise BazelTrayError("the tray app only runs on Windows")
        self.repo_root = repo_root
        self.bazel_command = bazel_command
        self.output_base = output_base
        self.user32 = ctypes.windll.user32
        self.shell32 = ctypes.windll.shell32
        self.kernel32 = ctypes.windll.kernel32
        self.user32.LoadIconW.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        self.user32.LoadIconW.restype = ctypes.c_void_p
        self.shell32.Shell_NotifyIconW.argtypes = [ctypes.c_uint, ctypes.c_void_p]
        self.shell32.Shell_NotifyIconW.restype = ctypes.c_int
        self.hinstance = self.kernel32.GetModuleHandleW(None)
        self.hwnd = None
        self._class_atom = None
        self._wndproc = None
        self._status = BazelServerStatus(None, None, None, "stopped", "bazel server is not running")
        self._status_lock = threading.Lock()
        self._running_icon = load_stock_icon(IDI_INFORMATION)
        self._stopped_icon = load_stock_icon(IDI_ERROR)
        self._stale_icon = load_stock_icon(IDI_WARNING)

    def run(self) -> int:
        self._create_window()
        self._set_status(self._status)
        self.user32.SetTimer(self.hwnd, 1, 5000, None)
        self._refresh_status()
        msg = ctypes.wintypes.MSG()
        while self.user32.GetMessageW(ctypes.byref(msg), None, 0, 0) != 0:
            self.user32.TranslateMessage(ctypes.byref(msg))
            self.user32.DispatchMessageW(ctypes.byref(msg))
        return int(msg.wParam)

    def _create_window(self) -> None:
        WNDPROC = ctypes.WINFUNCTYPE(
            ctypes.c_ssize_t,
            ctypes.c_void_p,
            ctypes.c_uint,
            ctypes.c_void_p,
            ctypes.c_void_p,
        )

        class WNDCLASSW(ctypes.Structure):
            _fields_ = [
                ("style", ctypes.c_uint),
                ("lpfnWndProc", WNDPROC),
                ("cbClsExtra", ctypes.c_int),
                ("cbWndExtra", ctypes.c_int),
                ("hInstance", ctypes.c_void_p),
                ("hIcon", ctypes.c_void_p),
                ("hCursor", ctypes.c_void_p),
                ("hbrBackground", ctypes.c_void_p),
                ("lpszMenuName", ctypes.c_wchar_p),
                ("lpszClassName", ctypes.c_wchar_p),
            ]

        def wndproc(hwnd, msg, wparam, lparam):
            return self._window_proc(hwnd, msg, wparam, lparam)

        self._wndproc = WNDPROC(wndproc)
        wnd_class = WNDCLASSW()
        wnd_class.style = 0
        wnd_class.lpfnWndProc = self._wndproc
        wnd_class.cbClsExtra = 0
        wnd_class.cbWndExtra = 0
        wnd_class.hInstance = self.hinstance
        wnd_class.hIcon = self._running_icon
        wnd_class.hCursor = None
        wnd_class.hbrBackground = None
        wnd_class.lpszMenuName = None
        wnd_class.lpszClassName = _make_window_class_name()

        self._class_atom = self.user32.RegisterClassW(ctypes.byref(wnd_class))
        if not self._class_atom and ctypes.get_last_error():
            raise ctypes.WinError()

        self.hwnd = self.user32.CreateWindowExW(
            0,
            wnd_class.lpszClassName,
            "Kain Bazel Tray",
            0,
            0,
            0,
            0,
            0,
            None,
            None,
            self.hinstance,
            None,
        )
        if not self.hwnd:
            raise ctypes.WinError()

    def _window_proc(self, hwnd, msg, wparam, lparam):
        if msg == WM_TRAYICON:
            if lparam in (WM_RBUTTONUP, WM_LBUTTONUP):
                self._show_menu()
            return 0
        if msg == WM_REFRESH_STATUS:
            self._refresh_status()
            return 0
        if msg == WM_TIMER:
            self._refresh_status()
            return 0
        if msg == WM_COMMAND:
            self._handle_command(int(wparam) & 0xFFFF)
            return 0
        if msg == WM_DESTROY:
            self._delete_icon()
            self.user32.PostQuitMessage(0)
            return 0
        return self.user32.DefWindowProcW(hwnd, msg, wparam, lparam)

    def _current_icon(self) -> int:
        with self._status_lock:
            if self._status.running:
                return self._running_icon
            if self._status.stale:
                return self._stale_icon
            return self._stopped_icon

    def _current_tip(self) -> str:
        with self._status_lock:
            summary = self._status.summary
        tip = f"Kain Bazel: {summary}"
        return tip[:127]

    def _set_status(self, status: BazelServerStatus) -> None:
        with self._status_lock:
            self._status = status
        if self.hwnd:
            self._update_icon()

    def _refresh_status(self) -> None:
        try:
            status = inspect_bazel_server(self.output_base)
        except OSError as error:
            status = BazelServerStatus(self.output_base, None, None, "stopped", f"status unavailable: {error}")
        self._set_status(status)

    def _update_icon(self) -> None:
        data = TrayIconData()
        data.cbSize = ctypes.sizeof(TrayIconData)
        data.hWnd = self.hwnd
        data.uID = 1
        data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP
        data.uCallbackMessage = WM_TRAYICON
        data.hIcon = self._current_icon()
        data.szTip = self._current_tip()
        action = NIM_ADD if not getattr(self, "_tray_added", False) else NIM_MODIFY
        if not self.shell32.Shell_NotifyIconW(action, ctypes.byref(data)):
            raise ctypes.WinError()
        self._tray_added = True

    def _delete_icon(self) -> None:
        if not getattr(self, "_tray_added", False):
            return
        data = TrayIconData()
        data.cbSize = ctypes.sizeof(TrayIconData)
        data.hWnd = self.hwnd
        data.uID = 1
        self.shell32.Shell_NotifyIconW(NIM_DELETE, ctypes.byref(data))
        self._tray_added = False

    def _show_menu(self) -> None:
        menu = self.user32.CreatePopupMenu()
        try:
            status = self._status
            status_label = f"Status: {make_menu_text(status)}"
            self.user32.AppendMenuW(menu, MF_STRING | MF_DISABLED | MF_GRAYED, 0, status_label)
            self.user32.AppendMenuW(menu, MF_SEPARATOR, 0, None)
            self.user32.AppendMenuW(menu, MF_STRING, ID_REFRESH, "Refresh")
            shutdown_label = "Shutdown Bazel"
            self.user32.AppendMenuW(menu, MF_STRING, ID_SHUTDOWN, shutdown_label)
            self.user32.AppendMenuW(menu, MF_STRING, ID_OPEN_OUTPUT_BASE, "Open output base")
            self.user32.AppendMenuW(menu, MF_SEPARATOR, 0, None)
            self.user32.AppendMenuW(menu, MF_STRING, ID_EXIT, "Exit")

            point = POINT()
            self.user32.GetCursorPos(ctypes.byref(point))
            self.user32.SetForegroundWindow(self.hwnd)
            self.user32.TrackPopupMenu(
                menu,
                TPM_LEFTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON,
                point.x,
                point.y,
                0,
                self.hwnd,
                None,
            )
        finally:
            self.user32.DestroyMenu(menu)

    def _handle_command(self, command_id: int) -> None:
        if command_id == ID_REFRESH:
            self._refresh_status()
            return
        if command_id == ID_SHUTDOWN:
            threading.Thread(target=self._shutdown_worker, daemon=True).start()
            return
        if command_id == ID_OPEN_OUTPUT_BASE:
            try:
                open_output_base(self.output_base)
            except OSError as error:
                self._set_status(BazelServerStatus(self.output_base, None, None, "stopped", f"failed to open output base: {error}"))
            return
        if command_id == ID_EXIT:
            self.user32.DestroyWindow(self.hwnd)

    def _shutdown_worker(self) -> None:
        try:
            shutdown_bazel_server(self.repo_root, self.bazel_command, self.output_base)
        finally:
            self.user32.PostMessageW(self.hwnd, WM_REFRESH_STATUS, 0, 0)


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Tiny Bazel tray monitor for Windows.")
    parser.add_argument("--repo-root", default=None, help="Repository root override.")
    parser.add_argument("--once", action="store_true", help="Print the current status and exit.")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    if not IS_WINDOWS:
        raise BazelTrayError("the Bazel tray helper only runs on Windows")
    repo_root = resolve_repo_root(args.repo_root)
    bazel_command = resolve_bazel_command()
    output_base = resolve_output_base(repo_root, bazel_command)
    if args.once:
        status = inspect_bazel_server(output_base)
        print(f"repo_root={repo_root}")
        print(f"bazel_command={bazel_command}")
        print(f"output_base={output_base}")
        print(f"status={status.state}")
        print(f"summary={status.summary}")
        return 0
    app = BazelTrayApp(repo_root, bazel_command, output_base)
    return app.run()


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        if IS_WINDOWS:
            try:
                ctypes.windll.user32.MessageBoxW(None, str(error), "Kain Bazel Tray", 0x10)
            except Exception:
                pass
        raise
