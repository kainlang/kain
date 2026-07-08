using System;
using System.Runtime.InteropServices;
using System.Text;

namespace UIValidator.Win32
{
    public static class NativeMethods
    {
        // ── user32.dll ──────────────────────────────────────────────

        [DllImport("user32.dll", SetLastError = true)]
        public static extern IntPtr FindWindow(string lpClassName, string lpWindowName);

        [DllImport("user32.dll", SetLastError = true)]
        public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);

        [DllImport("user32.dll")]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool IsWindowVisible(IntPtr hWnd);

        [DllImport("user32.dll")]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool IsWindowEnabled(IntPtr hWnd);

        [DllImport("user32.dll", SetLastError = true)]
        public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);

        [DllImport("user32.dll", SetLastError = true)]
        public static extern bool GetClientRect(IntPtr hWnd, out RECT lpRect);

        [DllImport("user32.dll", SetLastError = true)]
        public static extern bool ClientToScreen(IntPtr hWnd, out POINT lpPoint);

        [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        public static extern int GetClassName(IntPtr hWnd, StringBuilder lpClassName, int nMaxCount);

        [DllImport("user32.dll", CharSet = CharSet.Auto, SetLastError = true)]
        public static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);

        [DllImport("user32.dll", SetLastError = true)]
        public static extern int GetWindowTextLength(IntPtr hWnd);

        [DllImport("user32.dll", SetLastError = true)]
        public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);

        [DllImport("user32.dll", SetLastError = true)]
        public static extern bool SetForegroundWindow(IntPtr hWnd);

        [DllImport("user32.dll", SetLastError = true)]
        public static extern bool BringWindowToTop(IntPtr hWnd);

        [DllImport("user32.dll")]
        public static extern IntPtr GetForegroundWindow();

        [DllImport("user32.dll")]
        public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

        [DllImport("user32.dll", SetLastError = true)]
        public static extern uint SendInput(uint nInputs, INPUT[] pInputs, int cbSize);

        [DllImport("user32.dll")]
        public static extern int GetSystemMetrics(int nIndex);

        [DllImport("user32.dll")]
        public static extern bool SetProcessDPIAware();

        // ── gdi32.dll ───────────────────────────────────────────────

        [DllImport("gdi32.dll", SetLastError = true)]
        public static extern bool BitBlt(IntPtr hdcDest, int xDest, int yDest,
            int width, int height, IntPtr hdcSrc, int xSrc, int ySrc, uint rop);

        [DllImport("gdi32.dll")]
        public static extern IntPtr CreateCompatibleDC(IntPtr hdc);

        [DllImport("gdi32.dll")]
        public static extern IntPtr CreateCompatibleBitmap(IntPtr hdc, int width, int height);

        [DllImport("gdi32.dll")]
        public static extern IntPtr SelectObject(IntPtr hdc, IntPtr hgdiobj);

        [DllImport("gdi32.dll")]
        public static extern bool DeleteDC(IntPtr hdc);

        [DllImport("gdi32.dll")]
        public static extern bool DeleteObject(IntPtr hObject);

        [DllImport("user32.dll")]
        public static extern IntPtr GetDC(IntPtr hWnd);

        [DllImport("user32.dll")]
        public static extern bool ReleaseDC(IntPtr hWnd, IntPtr hDC);

        // ── kernel32.dll ────────────────────────────────────────────

        [DllImport("kernel32.dll")]
        public static extern uint GetCurrentThreadId();

        // ── Delegates ───────────────────────────────────────────────

        public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

        // ── Structs ─────────────────────────────────────────────────

        [StructLayout(LayoutKind.Sequential)]
        public struct RECT
        {
            public int Left;
            public int Top;
            public int Right;
            public int Bottom;
            public int Width  => Right - Left;
            public int Height => Bottom - Top;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct POINT
        {
            public int X;
            public int Y;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct INPUT
        {
            public uint type;
            public INPUTUNION u;
            public static int Size => Marshal.SizeOf(typeof(INPUT));
        }

        [StructLayout(LayoutKind.Explicit)]
        public struct INPUTUNION
        {
            [FieldOffset(0)] public MOUSEINPUT mi;
            [FieldOffset(0)] public KEYBDINPUT ki;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct MOUSEINPUT
        {
            public int dx;
            public int dy;
            public uint mouseData;
            public uint dwFlags;
            public uint time;
            public IntPtr dwExtraInfo;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct KEYBDINPUT
        {
            public ushort wVk;
            public ushort wScan;
            public uint dwFlags;
            public uint time;
            public IntPtr dwExtraInfo;
        }

        // ── Constants ───────────────────────────────────────────────

        // BitBlt raster ops
        public const uint SRCCOPY    = 0x00CC0020;
        public const uint CAPTUREBLT = 0x40000000;

        // GetSystemMetrics
        public const int SM_CXSCREEN = 0;
        public const int SM_CYSCREEN = 1;

        // SendInput types
        public const uint INPUT_MOUSE    = 0;
        public const uint INPUT_KEYBOARD = 1;

        // Mouse event flags
        public const uint MOUSEEVENTF_MOVE       = 0x0001;
        public const uint MOUSEEVENTF_LEFTDOWN   = 0x0002;
        public const uint MOUSEEVENTF_LEFTUP     = 0x0004;
        public const uint MOUSEEVENTF_ABSOLUTE   = 0x8000;
        public const uint MOUSEEVENTF_VIRTUALDESK = 0x4000;

        // Keyboard event flags
        public const uint KEYEVENTF_KEYDOWN  = 0x0000;
        public const uint KEYEVENTF_KEYUP    = 0x0002;
        public const uint KEYEVENTF_UNICODE  = 0x0004;
        public const uint KEYEVENTF_SCANCODE = 0x0008;

        // ShowWindow
        public const int SW_RESTORE = 9;
        public const int SW_SHOW    = 5;
    }
}
