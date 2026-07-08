using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Threading;
using UIValidator.Win32;

namespace UIValidator.Engines
{
    /// <summary>
    /// OS-level window enumeration and targeted finding with retry logic.
    /// No title matching guesswork — process ID, partial title, class name, all of it.
    /// </summary>
    public static class WindowScanner
    {
        /// <summary>
        /// Enumerate all visible, non-terminal windows currently on the desktop.
        /// </summary>
        public static List<WindowInfo> GetAllVisibleAppWindows()
        {
            var results = new List<WindowInfo>();

            NativeMethods.EnumWindows((hWnd, lParam) =>
            {
                if (ProcessClassFilter.IsValidAppWindow(hWnd))
                {
                    results.Add(WindowInfo.FromHandle(hWnd));
                }
                return true;
            }, IntPtr.Zero);

            return results;
        }

        /// <summary>
        /// Find a window by partial title match, polling until found or timeout.
        /// </summary>
        public static WindowInfo FindByPartialTitle(string substring,
            int timeoutMs = 10000, int pollIntervalMs = 500)
        {
            var sw = Stopwatch.StartNew();

            while (sw.ElapsedMilliseconds < timeoutMs)
            {
                var windows = GetAllVisibleAppWindows();
                var match = windows.FirstOrDefault(w =>
                    w.Title.IndexOf(substring, StringComparison.OrdinalIgnoreCase) >= 0);

                if (match != null)
                    return match;

                Thread.Sleep(pollIntervalMs);
            }

            return null;
        }

        /// <summary>
        /// Find windows belonging to a process by name, with retry.
        /// </summary>
        public static WindowInfo FindByProcessName(string procName,
            int timeoutMs = 10000, int pollIntervalMs = 500)
        {
            var sw = Stopwatch.StartNew();

            while (sw.ElapsedMilliseconds < timeoutMs)
            {
                var windows = GetAllVisibleAppWindows();
                var match = windows.FirstOrDefault(w =>
                    w.ProcessName.Equals(procName, StringComparison.OrdinalIgnoreCase));

                if (match != null)
                    return match;

                Thread.Sleep(pollIntervalMs);
            }

            return null;
        }

        /// <summary>
        /// Find a window by process ID, with retry.
        /// </summary>
        public static WindowInfo FindByPid(int pid,
            int timeoutMs = 10000, int pollIntervalMs = 500)
        {
            var sw = Stopwatch.StartNew();

            while (sw.ElapsedMilliseconds < timeoutMs)
            {
                try
                {
                    var proc = Process.GetProcessById(pid);
                    if (proc.MainWindowHandle != IntPtr.Zero &&
                        ProcessClassFilter.IsValidAppWindow(proc.MainWindowHandle))
                    {
                        return WindowInfo.FromHandle(proc.MainWindowHandle);
                    }
                }
                catch
                {
                    // Process not started yet or already exited
                }

                var windows = GetAllVisibleAppWindows();
                var match = windows.FirstOrDefault(w => w.ProcessId == pid);
                if (match != null)
                    return match;

                Thread.Sleep(pollIntervalMs);
            }

            return null;
        }

        /// <summary>
        /// Try to find a window by any means — title, pid, or process name.
        /// Returns the first match.
        /// </summary>
        public static WindowInfo FindAny(string keyword, int? pidHint = null,
            int timeoutMs = 10000, int pollIntervalMs = 500)
        {
            // If we have a PID hint, try that first with shorter timeout
            if (pidHint.HasValue)
            {
                var byPid = FindByPid(pidHint.Value, timeoutMs / 2, pollIntervalMs);
                if (byPid != null) return byPid;
            }

            // Try title match
            var byTitle = FindByPartialTitle(keyword, timeoutMs, pollIntervalMs);
            if (byTitle != null) return byTitle;

            // Try process name
            return FindByProcessName(keyword, timeoutMs, pollIntervalMs);
        }
    }
}
