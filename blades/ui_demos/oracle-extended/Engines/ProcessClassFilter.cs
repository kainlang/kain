using System;
using System.Text;

namespace UIValidator.Engines
{
    /// <summary>
    /// The Anti-Gaslight Gate: distinguishes real app windows from phantom
    /// console hosts that LLMs love to mistake for working UIs.
    /// </summary>
    public static class ProcessClassFilter
    {
        /// <summary>
        /// Known terminal/console window class names that are NOT real app windows.
        /// </summary>
        private static readonly string[] TerminalClasses =
        {
            "ConsoleWindowClass",              // legacy cmd.exe
            "CASCADIA_HOSTING_WINDOW_CLASS",   // modern Windows Terminal
            "PuTTY",                            // PuTTY terminal
            "Mintty",                           // Git Bash / Cygwin terminal
            "ConEmu",                           // ConEmu terminal
        };

        /// <summary>
        /// Returns true if the window is a terminal/console, not a real GUI window.
        /// </summary>
        public static bool IsGenericTerminal(IntPtr hWnd)
        {
            var sb = new StringBuilder(256);
            Win32.NativeMethods.GetClassName(hWnd, sb, sb.Capacity);
            string name = sb.ToString();

            foreach (var tc in TerminalClasses)
            {
                if (name.Equals(tc, StringComparison.OrdinalIgnoreCase))
                    return true;
            }
            return false;
        }

        /// <summary>
        /// Get the window class name string.
        /// </summary>
        public static string GetWindowClassName(IntPtr hWnd)
        {
            var sb = new StringBuilder(256);
            Win32.NativeMethods.GetClassName(hWnd, sb, sb.Capacity);
            return sb.ToString();
        }

        /// <summary>
        /// Full validation: visible, not a terminal, has real dimensions.
        /// </summary>
        public static bool IsValidAppWindow(IntPtr hWnd)
        {
            if (hWnd == IntPtr.Zero) return false;
            if (!Win32.NativeMethods.IsWindowVisible(hWnd)) return false;
            if (IsGenericTerminal(hWnd)) return false;

            if (!Win32.NativeMethods.GetWindowRect(hWnd, out Win32.NativeMethods.RECT rect))
                return false;

            return rect.Width > 0 && rect.Height > 0;
        }
    }
}
