using System;
using System.Diagnostics;
using System.IO;
using System.Threading;
using UIValidator.Schema;

namespace UIValidator.Engines
{
    /// <summary>
    /// Launches executables and returns process info so the oracle chain
    /// is: scan → launch → find → verify — no guesswork for the LLM.
    /// </summary>
    public static class Launcher
    {
        /// <summary>
        /// Start an executable with optional args. Returns structured process info.
        /// Optionally waits for the process to initialize before returning.
        /// </summary>
        public static LaunchResult Launch(string exePath, string args = null,
            int waitMs = 0, string workingDir = null)
        {
            if (!File.Exists(exePath))
            {
                return new LaunchResult
                {
                    Success = false,
                    Reason = $"Executable not found: {exePath}"
                };
            }

            try
            {
                var psi = new ProcessStartInfo
                {
                    FileName = exePath,
                    Arguments = args ?? "",
                    WorkingDirectory = workingDir ?? Path.GetDirectoryName(exePath),
                    UseShellExecute = true, // true = runs as standalone window, not as child
                    CreateNoWindow = false,
                };

                var proc = Process.Start(psi);

                if (proc == null)
                {
                    return new LaunchResult
                    {
                        Success = false,
                        Reason = "Process.Start returned null — launch failed silently."
                    };
                }

                if (waitMs > 0)
                {
                    Thread.Sleep(waitMs);
                    // Refresh process info after wait
                    try { proc.Refresh(); } catch { /* process may have exited */ }
                }

                var result = new LaunchResult
                {
                    Success      = true,
                    Pid          = proc.Id,
                    ProcessName  = proc.ProcessName,
                    ExePath      = exePath,
                    StartedAt    = DateTime.UtcNow,
                    HasExited    = false,
                };

                try
                {
                    result.HasExited    = proc.HasExited;
                    result.ExitCode     = proc.HasExited ? proc.ExitCode : null;
                    result.MainWindowHandle = proc.MainWindowHandle != IntPtr.Zero
                        ? $"0x{proc.MainWindowHandle:X}" : null;
                }
                catch
                {
                    // Process may have exited between launch and query
                    result.HasExited = true;
                }

                return result;
            }
            catch (Exception ex)
            {
                return new LaunchResult
                {
                    Success = false,
                    Reason = $"Launch exception: {ex.Message}"
                };
            }
        }

        /// <summary>
        /// Kill a process by PID. Returns success/failure.
        /// </summary>
        public static bool Kill(int pid)
        {
            try
            {
                var proc = Process.GetProcessById(pid);
                proc.Kill();
                proc.WaitForExit(3000);
                return true;
            }
            catch
            {
                return false;
            }
        }

        /// <summary>
        /// Check if a process is still running by PID.
        /// </summary>
        public static bool IsAlive(int pid)
        {
            try
            {
                var proc = Process.GetProcessById(pid);
                return !proc.HasExited;
            }
            catch
            {
                return false;
            }
        }
    }

    public class LaunchResult
    {
        public bool Success { get; set; }
        public string Reason { get; set; }
        public int Pid { get; set; }
        public string ProcessName { get; set; }
        public string ExePath { get; set; }
        public DateTime StartedAt { get; set; }
        public bool HasExited { get; set; }
        public int? ExitCode { get; set; }
        public string MainWindowHandle { get; set; }
        public double AgeSeconds => Success ? (DateTime.UtcNow - StartedAt).TotalSeconds : 0;
    }
}
