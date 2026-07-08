using System;
using System.Text;
using UIValidator.Win32;

namespace UIValidator
{
    /// <summary>
    /// Structured OS-level truth about a window — no guesswork, no hallucination.
    /// </summary>
    public class WindowInfo
    {
        public IntPtr Handle { get; set; }
        public string Title { get; set; }
        public string ClassName { get; set; }
        public int X { get; set; }
        public int Y { get; set; }
        public int Width { get; set; }
        public int Height { get; set; }
        public int ClientWidth { get; set; }
        public int ClientHeight { get; set; }
        public bool IsVisible { get; set; }
        public bool IsTerminal { get; set; }
        public uint ProcessId { get; set; }
        public string ProcessName { get; set; }

        /// <summary>
        /// Extract all available OS truth about a window handle.
        /// </summary>
        public static WindowInfo FromHandle(IntPtr hWnd)
        {
            var info = new WindowInfo { Handle = hWnd };

            // Visibility
            info.IsVisible = NativeMethods.IsWindowVisible(hWnd);

            // Rectangle
            if (NativeMethods.GetWindowRect(hWnd, out NativeMethods.RECT rect))
            {
                info.X = rect.Left;
                info.Y = rect.Top;
                info.Width  = rect.Width;
                info.Height = rect.Height;
            }

            // Client rectangle
            if (NativeMethods.GetClientRect(hWnd, out NativeMethods.RECT clientRect))
            {
                info.ClientWidth  = clientRect.Width;
                info.ClientHeight = clientRect.Height;
            }

            // Title
            int titleLen = NativeMethods.GetWindowTextLength(hWnd);
            if (titleLen > 0)
            {
                var sb = new StringBuilder(titleLen + 1);
                NativeMethods.GetWindowText(hWnd, sb, sb.Capacity);
                info.Title = sb.ToString();
            }
            else
            {
                info.Title = "";
            }

            // Class name
            var cnb = new StringBuilder(256);
            NativeMethods.GetClassName(hWnd, cnb, cnb.Capacity);
            info.ClassName = cnb.ToString();

            // Terminal detection
            info.IsTerminal = info.ClassName.Equals("ConsoleWindowClass",
                StringComparison.OrdinalIgnoreCase) ||
                info.ClassName.Equals("CASCADIA_HOSTING_WINDOW_CLASS",
                StringComparison.OrdinalIgnoreCase);

            // Process
            NativeMethods.GetWindowThreadProcessId(hWnd, out uint pid);
            info.ProcessId = pid;

            try
            {
                var proc = System.Diagnostics.Process.GetProcessById((int)pid);
                info.ProcessName = proc.ProcessName;
            }
            catch
            {
                info.ProcessName = $"pid:{pid}";
            }

            return info;
        }

        public bool HasValidSize => Width > 0 && Height > 0;
        public bool IsValidAppWindow => IsVisible && !IsTerminal && HasValidSize;
    }
}
