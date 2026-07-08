using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Text;
using System.Threading;
using UIValidator.Engines;
using UIValidator.Schema;
using UIValidator.Win32;

namespace UIValidator
{
    /// <summary>
    /// THE ORACLE — Multi-command CLI for gaslight-proof Windows UI validation.
    /// 
    /// Every command returns structured JSON. Exit codes:
    ///   0 = success / passed
    ///   1 = failed / not found / expectation not met
    ///   2 = usage error
    ///   3 = crash / exception
    /// </summary>
    class Program
    {
        static string OutputDir => Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "oracle_output");

        static int Main(string[] args)
        {
            // DPI awareness — prevent virtualization from lying about coordinates
            try { NativeMethods.SetProcessDPIAware(); } catch { }

            if (args.Length == 0)
            {
                PrintUsage();
                return 2;
            }

            try
            {
                string cmd = args[0].ToLowerInvariant();
                switch (cmd)
                {
                    case "find":    return CmdFind(args);
                    case "info":    return CmdInfo(args);
                    case "capture": return CmdCapture(args);
                    case "matrix":  return CmdMatrix(args);
                    case "click":   return CmdClick(args);
                    case "type":    return CmdType(args);
                    case "delta":   return CmdDelta(args);
                    case "verify":  return CmdVerify(args);
                    case "list":    return CmdList(args);
                    case "launch":  return CmdLaunch(args);
                    case "scan":    return CmdScan(args);
                    case "kill":    return CmdKill(args);
                    case "debug":   return CmdDebug(args);
                    case "ocr":     return CmdOcr(args);
                    case "uia":     return CmdUia(args);
                    case "analyze": return CmdAnalyze(args);
                    case "describe":return CmdDescribe(args);
                    case "clipboard":return CmdClipboard(args);
                    case "pickfile":return CmdPickFile(args);
                    case "gpu":    return CmdGpu(args);
                    case "version": return CmdVersion();
                    case "help":    PrintUsage(); return 0;
                    default:
                        Console.Error.WriteLine(JsonBuilder.Error($"Unknown command '{cmd}'. Use 'oracle help'."));
                        return 2;
                }
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine(JsonBuilder.Crash(ex.Message));
                return 3;
            }
        }

        // ── find ─────────────────────────────────────────────────────
        static int CmdFind(string[] args)
        {
            string keyword = ArgVal(args, 1) ?? ArgVal(args, "--keyword", "-k");
            int? pid       = ArgInt(args, "--pid", "-p");
            int timeout    = ArgInt(args, "--timeout", "-t") ?? 10000;
            int poll       = ArgInt(args, "--poll") ?? 500;

            if (string.IsNullOrEmpty(keyword) && !pid.HasValue)
            {
                Console.Error.WriteLine(JsonBuilder.Error("find requires --keyword or --pid"));
                return 2;
            }

            WindowInfo win = null;
            if (pid.HasValue)
                win = WindowScanner.FindByPid(pid.Value, timeout, poll);
            else
                win = WindowScanner.FindAny(keyword, pid, timeout, poll);

            if (win == null || !win.IsValidAppWindow)
            {
                string reason = pid.HasValue
                    ? $"No valid app window found for PID {pid.Value} within {timeout}ms"
                    : $"No valid app window found for '{keyword}' within {timeout}ms";
                Console.Error.WriteLine(JsonBuilder.Error(reason));
                return 1;
            }

            Console.Out.WriteLine(JsonBuilder.Passed(
                ("handle",        JsonBuilder.Hex(win.Handle)),
                ("title",         JsonBuilder.Str(win.Title)),
                ("class",         JsonBuilder.Str(win.ClassName)),
                ("dimensions",    JsonBuilder.Str($"{win.Width}x{win.Height}")),
                ("client_size",   JsonBuilder.Str($"{win.ClientWidth}x{win.ClientHeight}")),
                ("position",      JsonBuilder.Str($"{win.X},{win.Y}")),
                ("visible",       JsonBuilder.Bool(win.IsVisible)),
                ("pid",           JsonBuilder.Num((int)win.ProcessId)),
                ("process",       JsonBuilder.Str(win.ProcessName)),
                ("is_terminal",   JsonBuilder.Bool(win.IsTerminal))
            ));
            return 0;
        }

        // ── info ─────────────────────────────────────────────────────
        static int CmdInfo(string[] args)
        {
            var win = ResolveWindow(args);
            if (win == null) return 1;

            Console.Out.WriteLine(JsonBuilder.Passed(
                ("handle",      JsonBuilder.Hex(win.Handle)),
                ("title",       JsonBuilder.Str(win.Title)),
                ("class",       JsonBuilder.Str(win.ClassName)),
                ("dimensions",  JsonBuilder.Str($"{win.Width}x{win.Height}")),
                ("x",           JsonBuilder.Num(win.X)),
                ("y",           JsonBuilder.Num(win.Y)),
                ("client_w",    JsonBuilder.Num(win.ClientWidth)),
                ("client_h",    JsonBuilder.Num(win.ClientHeight)),
                ("visible",     JsonBuilder.Bool(win.IsVisible)),
                ("terminal",    JsonBuilder.Bool(win.IsTerminal)),
                ("pid",         JsonBuilder.Num((int)win.ProcessId)),
                ("process",     JsonBuilder.Str(win.ProcessName))
            ));
            return 0;
        }

        // ── capture ──────────────────────────────────────────────────
        static int CmdCapture(string[] args)
        {
            var win = ResolveWindow(args);
            if (win == null) return 1;

            bool clientOnly = HasFlag(args, "--client", "-c");
            string outDir = ArgVal(args, "--output", "-o") ?? OutputDir;

            try
            {
                string path = clientOnly
                    ? VisualEngine.CaptureClientArea(win.Handle, outDir)
                    : VisualEngine.CaptureWindow(win.Handle, outDir);

                Console.Out.WriteLine(JsonBuilder.Passed(
                    ("screenshot",  JsonBuilder.Str(path.Replace("\\", "\\\\"))),
                    ("dimensions",  JsonBuilder.Str($"{win.Width}x{win.Height}")),
                    ("handle",      JsonBuilder.Hex(win.Handle))
                ));
                return 0;
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine(JsonBuilder.Error($"Capture failed: {ex.Message}"));
                return 1;
            }
        }

        // ── matrix ───────────────────────────────────────────────────
        static int CmdMatrix(string[] args)
        {
            var win = ResolveWindow(args);
            if (win == null) return 1;

            int rows   = ArgInt(args, "--rows", "-r")   ?? 15;
            int cols   = ArgInt(args, "--cols", "-c")   ?? 30;
            string fmt = ArgVal(args, "--format", "-f") ?? "brightness";
            bool text  = HasFlag(args, "--text");

            using (var bmp = VisualEngine.CaptureToBitmap(win.Handle))
            {
                if (fmt == "color" || fmt == "hex")
                {
                    var colorMatrix = SpatialMatrixEngine.GenerateColorMatrix(bmp, rows, cols);
                    if (text)
                    {
                        // Print color matrix as text grid
                        for (int r = 0; r < rows; r++)
                        {
                            for (int c = 0; c < cols; c++)
                            {
                                Console.Out.Write(colorMatrix[r, c] + " ");
                            }
                            Console.Out.WriteLine();
                        }
                    }
                    else
                    {
                        Console.Out.WriteLine(JsonBuilder.Passed(
                            ("type",       JsonBuilder.Str("color")),
                            ("dimensions", JsonBuilder.Str($"{rows}x{cols}")),
                            ("matrix",     JsonBuilder.MatrixStr(colorMatrix)),
                            ("handle",     JsonBuilder.Hex(win.Handle))
                        ));
                    }
                }
                else
                {
                    var brightness = SpatialMatrixEngine.GenerateBrightnessMatrix(bmp, rows, cols);
                    var analysis   = SpatialMatrixEngine.AnalyzeBrightness(brightness);

                    if (text)
                    {
                        Console.Out.WriteLine(SpatialMatrixEngine.FormatAsText(brightness));
                    }
                    else
                    {
                        Console.Out.WriteLine(JsonBuilder.Passed(
                            ("type",            JsonBuilder.Str("brightness")),
                            ("dimensions",      JsonBuilder.Str($"{rows}x{cols}")),
                            ("matrix",          JsonBuilder.Matrix(brightness)),
                            ("coverage_percent", JsonBuilder.Num(analysis.CoveragePercent)),
                            ("is_all_black",    JsonBuilder.Bool(analysis.IsAllBlack)),
                            ("non_zero_cells",  JsonBuilder.Num(analysis.NonZeroCells)),
                            ("handle",          JsonBuilder.Hex(win.Handle))
                        ));
                    }
                }
            }

            return 0;
        }

        // ── click ────────────────────────────────────────────────────
        static int CmdClick(string[] args)
        {
            var win = ResolveWindow(args);
            if (win == null) return 1;

            int? x = ArgInt(args, "--x", "-x");
            int? y = ArgInt(args, "--y", "-y");

            if (!x.HasValue || !y.HasValue)
            {
                Console.Error.WriteLine(JsonBuilder.Error("click requires --x and --y"));
                return 2;
            }

            try
            {
                InputEngine.FocusWindow(win.Handle);
                InputEngine.Click(win.Handle, x.Value, y.Value);

                Console.Out.WriteLine(JsonBuilder.Passed(
                    ("action",   JsonBuilder.Str("click")),
                    ("position", JsonBuilder.Str($"{x},{y}")),
                    ("screen",   JsonBuilder.Str($"{win.X + x},{win.Y + y}")),
                    ("handle",   JsonBuilder.Hex(win.Handle))
                ));
                return 0;
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine(JsonBuilder.Error($"Click failed: {ex.Message}"));
                return 1;
            }
        }

        // ── type ─────────────────────────────────────────────────────
        static int CmdType(string[] args)
        {
            var win = ResolveWindow(args);
            if (win == null) return 1;

            string text = ArgVal(args, "--text", "-t");
            if (string.IsNullOrEmpty(text))
            {
                Console.Error.WriteLine(JsonBuilder.Error("type requires --text"));
                return 2;
            }

            try
            {
                InputEngine.Type(win.Handle, text);

                Console.Out.WriteLine(JsonBuilder.Passed(
                    ("action", JsonBuilder.Str("type")),
                    ("length", JsonBuilder.Num(text.Length)),
                    ("handle", JsonBuilder.Hex(win.Handle))
                ));
                return 0;
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine(JsonBuilder.Error($"Type failed: {ex.Message}"));
                return 1;
            }
        }

        // ── delta ────────────────────────────────────────────────────
        static int CmdDelta(string[] args)
        {
            var win = ResolveWindow(args);
            if (win == null) return 1;

            int interval = ArgInt(args, "--interval", "-i") ?? 200;

            try
            {
                var result = DeltaEngine.CheckFrozen(win.Handle, interval);

                Console.Out.WriteLine(JsonBuilder.Passed(
                    ("total_pixels",    JsonBuilder.Num(result.TotalPixels)),
                    ("changed_pixels",  JsonBuilder.Num(result.ChangedPixels)),
                    ("fraction",        JsonBuilder.Num(result.FractionChanged)),
                    ("is_frozen",       JsonBuilder.Bool(result.IsFrozen)),
                    ("interval_ms",     JsonBuilder.Num(result.IntervalMs)),
                    ("handle",          JsonBuilder.Hex(win.Handle))
                ));

                return result.IsFrozen ? 1 : 0;
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine(JsonBuilder.Error($"Delta failed: {ex.Message}"));
                return 1;
            }
        }

        // ── verify ───────────────────────────────────────────────────
        static int CmdVerify(string[] args)
        {
            var win = ResolveWindow(args);
            if (win == null) return 1;

            string action = ArgVal(args, "--do", "-d");
            string expect = ArgVal(args, "--expect", "-e") ?? "changed";
            int wait      = ArgInt(args, "--wait", "-w") ?? 500;
            string outDir = ArgVal(args, "--output", "-o") ?? OutputDir;

            if (string.IsNullOrEmpty(action))
            {
                Console.Error.WriteLine(JsonBuilder.Error("verify requires --do (e.g. --do click:100,200)"));
                return 2;
            }

            var result = VerifyEngine.Run(win.Handle, action, wait, expect, outDir);

            var fields = new (string, string)[]
            {
                ("passed",             JsonBuilder.Bool(result.Passed)),
                ("reason",             JsonBuilder.Str(result.Reason)),
                ("action",             JsonBuilder.Str(result.Action)),
                ("expect",             JsonBuilder.Str(result.Expect)),
                ("wait_ms",            JsonBuilder.Num(result.WaitMs)),
                ("pixels_changed",     JsonBuilder.Num(result.PixelsChanged)),
                ("cells_changed",      JsonBuilder.Num(result.MatrixCellsChanged)),
                ("fraction_changed",   JsonBuilder.Num(result.FractionChanged)),
                ("coverage_before",    JsonBuilder.Num(result.BeforeCoverage)),
                ("coverage_after",     JsonBuilder.Num(result.AfterCoverage)),
                ("handle",             JsonBuilder.Hex(win.Handle)),
            };

            if (result.AfterScreenshot != null)
            {
                Array.Resize(ref fields, fields.Length + 1);
                fields[fields.Length - 1] = ("after_screenshot",
                    JsonBuilder.Str(result.AfterScreenshot.Replace("\\", "\\\\")));
            }

            if (result.Passed)
                Console.Out.WriteLine(JsonBuilder.Passed(fields));
            else
                Console.Error.WriteLine(JsonBuilder.Obj(fields));

            return result.Passed ? 0 : 1;
        }

        // ── list ─────────────────────────────────────────────────────
        static int CmdList(string[] args)
        {
            var windows = WindowScanner.GetAllVisibleAppWindows();

            var entries = new string[windows.Count];
            for (int i = 0; i < windows.Count; i++)
            {
                var w = windows[i];
                entries[i] = JsonBuilder.Obj(
                    ("handle",   JsonBuilder.Hex(w.Handle)),
                    ("title",    JsonBuilder.Str(w.Title)),
                    ("class",    JsonBuilder.Str(w.ClassName)),
                    ("size",     JsonBuilder.Str($"{w.Width}x{w.Height}")),
                    ("pid",      JsonBuilder.Num((int)w.ProcessId)),
                    ("process",  JsonBuilder.Str(w.ProcessName))
                );
            }

            Console.Out.WriteLine(JsonBuilder.Obj(
                ("count",   JsonBuilder.Num(windows.Count)),
                ("windows", JsonBuilder.Arr(entries))
            ));
            return 0;
        }

        // ── launch ───────────────────────────────────────────────────
        static int CmdLaunch(string[] args)
        {
            string exe = ArgVal(args, 1);
            if (string.IsNullOrEmpty(exe))
                exe = ArgVal(args, "--exe", "-e");

            if (string.IsNullOrEmpty(exe))
            {
                // Try scanning for freshest exe in cwd
                var fresh = ExeScanner.FindFreshest(Directory.GetCurrentDirectory());
                if (fresh != null)
                    exe = fresh.Path;
            }

            if (string.IsNullOrEmpty(exe))
            {
                Console.Error.WriteLine(JsonBuilder.Error("launch requires an exe path, or run from a dir with .exe files."));
                return 2;
            }

            string procArgs   = ArgVal(args, "--args", "-a");
            int waitMs        = ArgInt(args, "--wait", "-w") ?? 0;
            string workingDir = ArgVal(args, "--cwd", "-d");
            bool autoFind     = HasFlag(args, "--find");

            var result = Launcher.Launch(exe, procArgs, waitMs, workingDir);

            if (!result.Success)
            {
                Console.Error.WriteLine(JsonBuilder.Error(result.Reason));
                return 1;
            }

            var fields = new (string, string)[]
            {
                ("status",       JsonBuilder.Str("PASSED")),
                ("pid",          JsonBuilder.Num(result.Pid)),
                ("process",      JsonBuilder.Str(result.ProcessName)),
                ("exe",          JsonBuilder.Str(result.ExePath.Replace("\\", "\\\\"))),
                ("age_seconds",  JsonBuilder.Num(result.AgeSeconds)),
                ("has_exited",   JsonBuilder.Bool(result.HasExited)),
            };

            if (result.HasExited)
            {
                Array.Resize(ref fields, fields.Length + 1);
                fields[fields.Length - 1] = ("exit_code", JsonBuilder.Num(result.ExitCode ?? -1));
            }
            if (result.MainWindowHandle != null)
            {
                Array.Resize(ref fields, fields.Length + 1);
                fields[fields.Length - 1] = ("window_handle", JsonBuilder.Str(result.MainWindowHandle));
            }

            Console.Out.WriteLine(JsonBuilder.Obj(fields));

            // If --find, immediately try to locate the window
            if (autoFind && !result.HasExited)
            {
                Console.Error.Write("# oracle: searching for window... ");
                var win = WindowScanner.FindByPid(result.Pid, 15000, 500);
                if (win != null)
                {
                    Console.Error.WriteLine($"found (0x{win.Handle:X})");
                }
                else
                {
                    Console.Error.WriteLine("not found within 15s");
                }
            }

            return result.HasExited ? 1 : 0;
        }

        // ── scan ────────────────────────────────────────────────────
        static int CmdScan(string[] args)
        {
            string dir     = ArgVal(args, "--dir", "-d") ?? Directory.GetCurrentDirectory();
            string pattern = ArgVal(args, "--pattern", "-p") ?? "*.exe";
            int limit      = ArgInt(args, "--limit", "-n") ?? 10;
            bool noRecurse = HasFlag(args, "--no-recurse");

            try
            {
                var results = ExeScanner.Scan(dir, pattern, limit, !noRecurse);

                var entries = new string[results.Count];
                for (int i = 0; i < results.Count; i++)
                {
                    var e = results[i];
                    entries[i] = JsonBuilder.Obj(
                        ("path",      JsonBuilder.Str(e.Path.Replace("\\", "\\\\"))),
                        ("name",      JsonBuilder.Str(e.Name)),
                        ("dir",       JsonBuilder.Str(e.Directory.Replace("\\", "\\\\"))),
                        ("size",      JsonBuilder.Str(e.SizeHuman)),
                        ("size_bytes", JsonBuilder.Num((long)e.SizeBytes)),
                        ("age",       JsonBuilder.Str(e.AgeHuman)),
                        ("age_seconds", JsonBuilder.Num(e.AgeSeconds))
                    );
                }

                string bestMatch = results.Count > 0
                    ? JsonBuilder.Str(results[0].Path.Replace("\\", "\\\\"))
                    : JsonBuilder.Null();

                Console.Out.WriteLine(JsonBuilder.Obj(
                    ("status",       JsonBuilder.Str("PASSED")),
                    ("count",        JsonBuilder.Num(results.Count)),
                    ("scanned_dir",  JsonBuilder.Str(dir.Replace("\\", "\\\\"))),
                    ("pattern",      JsonBuilder.Str(pattern)),
                    ("best_match",   bestMatch),
                    ("results",      JsonBuilder.Arr(entries))
                ));
                return 0;
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine(JsonBuilder.Error($"Scan failed: {ex.Message}"));
                return 1;
            }
        }

        // ── kill ────────────────────────────────────────────────────
        static int CmdKill(string[] args)
        {
            int? pid = ArgInt(args, "--pid", "-p");
            string keyword = ArgVal(args, "--keyword", "-k");

            if (!pid.HasValue && string.IsNullOrEmpty(keyword))
            {
                Console.Error.WriteLine(JsonBuilder.Error("kill requires --pid or --keyword"));
                return 2;
            }

            if (!pid.HasValue)
            {
                var win = WindowScanner.FindAny(keyword, null, 3000, 200);
                if (win != null)
                    pid = (int)win.ProcessId;
                else
                {
                    Console.Error.WriteLine(JsonBuilder.Error($"No process found for '{keyword}'"));
                    return 1;
                }
            }

            bool ok = Launcher.Kill(pid.Value);

            Console.Out.WriteLine(JsonBuilder.Obj(
                ("status", JsonBuilder.Str(ok ? "PASSED" : "FAILED")),
                ("pid",    JsonBuilder.Num(pid.Value)),
                ("killed", JsonBuilder.Bool(ok))
            ));
            return ok ? 0 : 1;
        }

        // ── debug ───────────────────────────────────────────────────
        static int CmdDebug(string[] args)
        {
            int? pid = ArgInt(args, "--pid", "-p");
            if (!pid.HasValue)
            {
                Console.Error.WriteLine(JsonBuilder.Error("debug requires --pid"));
                return 2;
            }

            try
            {
                var proc = System.Diagnostics.Process.GetProcessById(pid.Value);
                bool isAlive = !proc.HasExited;

                // Enumerate ALL windows (including terminals, invisibles, zero-size)
                // and tag each with rejection reason
                var allWindows = new System.Collections.Generic.List<WindowInfo>();

                NativeMethods.EnumWindows((hWnd, lParam) =>
                {
                    NativeMethods.GetWindowThreadProcessId(hWnd, out uint winPid);
                    if (winPid == pid.Value)
                    {
                        allWindows.Add(WindowInfo.FromHandle(hWnd));
                    }
                    return true;
                }, IntPtr.Zero);

                var entries = new string[allWindows.Count];
                for (int i = 0; i < allWindows.Count; i++)
                {
                    var w = allWindows[i];
                    string rejection = null;

                    if (!w.IsVisible)       rejection = "INVISIBLE";
                    else if (w.IsTerminal)   rejection = "TERMINAL";
                    else if (!w.HasValidSize) rejection = "ZERO_SIZE";

                    var pairs = new System.Collections.Generic.List<(string, string)>
                    {
                        ("handle",    JsonBuilder.Hex(w.Handle)),
                        ("title",     JsonBuilder.Str(w.Title)),
                        ("class",     JsonBuilder.Str(w.ClassName)),
                        ("size",      JsonBuilder.Str($"{w.Width}x{w.Height}")),
                        ("visible",   JsonBuilder.Bool(w.IsVisible)),
                        ("terminal",  JsonBuilder.Bool(w.IsTerminal)),
                        ("rejected",  JsonBuilder.Bool(rejection != null)),
                    };

                    if (rejection != null)
                        pairs.Add(("rejection_reason", JsonBuilder.Str(rejection)));
                    else
                        pairs.Add(("rejection_reason", JsonBuilder.Null()));

                    entries[i] = JsonBuilder.Obj(pairs.ToArray());
                }

                int validCount = allWindows.FindAll(w => w.IsValidAppWindow).Count;
                string verdict = validCount > 0
                    ? $"Found {validCount} valid app window(s)"
                    : allWindows.Count > 0
                        ? $"Created {allWindows.Count} window(s), ALL rejected"
                        : "Created no windows at all — likely a console app or crashed before window creation";

                Console.Out.WriteLine(JsonBuilder.Obj(
                    ("status",         JsonBuilder.Str("PASSED")),
                    ("pid",            JsonBuilder.Num(pid.Value)),
                    ("process",        JsonBuilder.Str(proc.ProcessName)),
                    ("is_alive",       JsonBuilder.Bool(isAlive)),
                    ("has_exited",     JsonBuilder.Bool(!isAlive)),
                    ("total_windows",  JsonBuilder.Num(allWindows.Count)),
                    ("valid_windows",  JsonBuilder.Num(validCount)),
                    ("verdict",        JsonBuilder.Str(verdict)),
                    ("windows",        JsonBuilder.Arr(entries))
                ));

                return validCount > 0 ? 0 : 1;
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine(JsonBuilder.Error($"Debug failed: {ex.Message}"));
                return 1;
            }
        }

        // ── version ──────────────────────────────────────────────────
        static int CmdVersion()
        {
            Console.Out.WriteLine(JsonBuilder.Obj(
                ("name",    JsonBuilder.Str("oracle")),
                ("version", JsonBuilder.Str("1.0.0")),
                ("desc",    JsonBuilder.Str("Gaslight-proof Windows UI validator"))
            ));
            return 0;
        }

        // ── ocr ──────────────────────────────────────────────────────
        static int CmdOcr(string[] args)
        {
            var win = ResolveWindow(args);
            if (win == null) return 1;

            string lang = ArgVal(args, "--lang", "-l");

            // Capture to temp file
            string tempDir = System.IO.Path.Combine(System.IO.Path.GetTempPath(), "oracle_vision");
            System.IO.Directory.CreateDirectory(tempDir);
            string imgPath = System.IO.Path.Combine(tempDir, "oracle_ocr_temp.png");

            using (var bmp = VisualEngine.CaptureToBitmap(win.Handle))
            {
                bmp.Save(imgPath, System.Drawing.Imaging.ImageFormat.Png);
            }

            string result = OcrEngine.OcrImageFile(imgPath, lang);

            // Parse brief summary
            var parsed = DescribeEngine.ParseSimpleJson(result);
            parsed.TryGetValue("word_count", out string wc);
            parsed.TryGetValue("line_count", out string lc);

            Console.Out.WriteLine(JsonBuilder.Obj(
                ("handle",     JsonBuilder.Hex(win.Handle)),
                ("ocr",        JsonBuilder.Str(result)),
                ("word_count", JsonBuilder.Str(wc ?? "?")),
                ("line_count", JsonBuilder.Str(lc ?? "?"))
            ));
            return 0;
        }

        // ── uia ──────────────────────────────────────────────────────
        static int CmdUia(string[] args)
        {
            var win = ResolveWindow(args);
            if (win == null) return 1;

            string result = UiaEngine.GetElementTree(win.Handle);

            var parsed = DescribeEngine.ParseSimpleJson(result);
            parsed.TryGetValue("element_count", out string ec);

            Console.Out.WriteLine(JsonBuilder.Obj(
                ("handle",        JsonBuilder.Hex(win.Handle)),
                ("uia",           JsonBuilder.Str(result)),
                ("element_count", JsonBuilder.Str(ec ?? "?"))
            ));
            return 0;
        }

        // ── analyze ──────────────────────────────────────────────────
        static int CmdAnalyze(string[] args)
        {
            var win = ResolveWindow(args);
            if (win == null) return 1;

            string engines = ArgVal(args, "--engines", "-e") ?? "all";
            bool noOcr  = engines != "all" && !engines.Contains("ocr");
            bool noUia  = engines != "all" && !engines.Contains("uia");

            // Capture bitmap
            Bitmap bmp;
            try
            {
                bmp = VisualEngine.CaptureToBitmap(win.Handle);
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine(JsonBuilder.Error($"Capture failed: {ex.Message}"));
                return 1;
            }

            string tempDir = System.IO.Path.Combine(System.IO.Path.GetTempPath(), "oracle_vision");
            System.IO.Directory.CreateDirectory(tempDir);
            string imgPath = System.IO.Path.Combine(tempDir, "oracle_analyze_temp.png");
            bmp.Save(imgPath, System.Drawing.Imaging.ImageFormat.Png);

            // Run engines
            string ocrResult  = noOcr ? null : OcrEngine.OcrImageFile(imgPath);
            string uiaResult  = noUia ? null : UiaEngine.GetElementTree(win.Handle);
            string colorResult = ColorEngine.AnalyzeColors(bmp);
            string layoutResult = LayoutEngine.AnalyzeLayout(bmp);

            bmp.Dispose();

            var fields = new System.Collections.Generic.List<(string, string)>
            {
                ("handle", JsonBuilder.Hex(win.Handle)),
                ("capture", JsonBuilder.Str(imgPath.Replace("\\", "\\\\"))),
                ("ocr", ocrResult != null ? JsonBuilder.Str(ocrResult) : JsonBuilder.Null()),
                ("uia", uiaResult != null ? JsonBuilder.Str(uiaResult) : JsonBuilder.Null()),
                ("color", JsonBuilder.Str(colorResult)),
                ("layout", JsonBuilder.Str(layoutResult))
            };

            Console.Out.WriteLine(JsonBuilder.Obj(fields.ToArray()));
            return 0;
        }

        // ── describe ─────────────────────────────────────────────────
        static int CmdDescribe(string[] args)
        {
            var win = ResolveWindow(args);
            if (win == null) return 1;

            bool noOcr = HasFlag(args, "--no-ocr");
            bool noUia = HasFlag(args, "--no-uia");

            string markdown = DescribeEngine.DescribeWindow(win.Handle, !noOcr, !noUia);

            Console.Out.WriteLine(JsonBuilder.Obj(
                ("handle",    JsonBuilder.Hex(win.Handle)),
                ("markdown",  JsonBuilder.Str(markdown))
            ));
            return 0;
        }

        // ── clipboard ──────────────────────────────────────────────
        static int CmdClipboard(string[] args)
        {
            // Clipboard access requires STA thread
            Image clipImage = null;
            Exception clipEx = null;
            var staThread = new Thread(() =>
            {
                try
                {
                    clipImage = System.Windows.Forms.Clipboard.GetImage();
                }
                catch (Exception ex)
                {
                    clipEx = ex;
                }
            });
            staThread.SetApartmentState(ApartmentState.STA);
            staThread.Start();
            if (!staThread.Join(5000))
            {
                Console.Error.WriteLine(JsonBuilder.Error("Clipboard access timed out"));
                return 1;
            }

            if (clipEx != null)
            {
                Console.Error.WriteLine(JsonBuilder.Error($"Clipboard access failed: {clipEx.Message}"));
                return 1;
            }

            if (clipImage == null)
            {
                Console.Error.WriteLine(JsonBuilder.Error("Clipboard does not contain an image."));
                return 1;
            }

            // Save to temp file
            string tempDir = System.IO.Path.Combine(System.IO.Path.GetTempPath(), "oracle_vision");
            System.IO.Directory.CreateDirectory(tempDir);
            string imgPath = System.IO.Path.Combine(tempDir, $"oracle_clipboard_{DateTime.Now:yyyyMMdd_HHmmss}.png");

            try
            {
                using (var bmp = new Bitmap(clipImage))
                {
                    bmp.Save(imgPath, System.Drawing.Imaging.ImageFormat.Png);
                }
                clipImage.Dispose();
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine(JsonBuilder.Error($"Failed to save clipboard image: {ex.Message}"));
                return 1;
            }

            // Run engines on the saved image
            string ocrResult   = OcrEngine.OcrImageFile(imgPath);
            string colorResult = null;
            string layoutResult = null;
            string asciiArt = null;
            string brightnessMatrix = null;

            try
            {
                using (var bmp = new Bitmap(imgPath))
                {
                    colorResult  = ColorEngine.AnalyzeColors(bmp);
                    layoutResult = LayoutEngine.AnalyzeLayout(bmp);
                    asciiArt     = AsciiEngine.GenerateAscii(bmp, 80, true);

                    var matrix = SpatialMatrixEngine.GenerateBrightnessMatrix(bmp, 12, 24);
                    brightnessMatrix = SpatialMatrixEngine.FormatAsText(matrix);
                }
            }
            catch { }

            bool noOcrFlag  = HasFlag(args, "--no-ocr");

            Console.Out.WriteLine(JsonBuilder.Obj(
                ("source",            JsonBuilder.Str("clipboard")),
                ("image_path",        JsonBuilder.Str(imgPath.Replace("\\", "\\\\"))),
                ("ocr",               ocrResult   != null && !noOcrFlag ? JsonBuilder.Str(ocrResult)  : JsonBuilder.Null()),
                ("color",             colorResult != null                 ? JsonBuilder.Str(colorResult) : JsonBuilder.Null()),
                ("layout",            layoutResult != null                ? JsonBuilder.Str(layoutResult) : JsonBuilder.Null()),
                ("ascii",             JsonBuilder.Str(asciiArt ?? "")),
                ("brightness_matrix",  JsonBuilder.Str(brightnessMatrix ?? ""))
            ));
            return 0;
        }

        // ── pickfile ───────────────────────────────────────────────
        static int CmdPickFile(string[] args)
        {
            string selectedPath = null;
            Exception pickEx = null;

            var staThread = new Thread(() =>
            {
                try
                {
                    var dialog = new System.Windows.Forms.OpenFileDialog();
                    dialog.Title = "Select an image to analyze";
                    dialog.Filter = "Image Files (*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.webp)|*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.webp|All Files (*.*)|*.*";
                    dialog.CheckFileExists = true;
                    dialog.Multiselect = false;

                    if (dialog.ShowDialog() == System.Windows.Forms.DialogResult.OK)
                    {
                        selectedPath = dialog.FileName;
                    }
                }
                catch (Exception ex)
                {
                    pickEx = ex;
                }
            });
            staThread.SetApartmentState(ApartmentState.STA);
            staThread.Start();

            if (!staThread.Join(30000))
            {
                Console.Error.WriteLine(JsonBuilder.Error("File picker dialog timed out"));
                return 1;
            }

            if (pickEx != null)
            {
                Console.Error.WriteLine(JsonBuilder.Error($"File picker failed: {pickEx.Message}"));
                return 1;
            }

            if (selectedPath == null)
            {
                Console.Error.WriteLine(JsonBuilder.Error("No file selected."));
                return 1;
            }

            // Run OCR + color + layout + ASCII on the selected file
            string ocrResult   = OcrEngine.OcrImageFile(selectedPath);
            string colorResult = null;
            string layoutResult = null;
            string asciiArt = null;
            string brightnessMatrix = null;

            try
            {
                using (var bmp = new Bitmap(selectedPath))
                {
                    colorResult  = ColorEngine.AnalyzeColors(bmp);
                    layoutResult = LayoutEngine.AnalyzeLayout(bmp);
                    asciiArt     = AsciiEngine.GenerateAscii(bmp, 80, true);

                    var matrix = SpatialMatrixEngine.GenerateBrightnessMatrix(bmp, 12, 24);
                    brightnessMatrix = SpatialMatrixEngine.FormatAsText(matrix);
                }
            }
            catch { }

            bool noOcrFlag = HasFlag(args, "--no-ocr");

            Console.Out.WriteLine(JsonBuilder.Obj(
                ("source",            JsonBuilder.Str("filepicker")),
                ("selected_path",     JsonBuilder.Str(selectedPath.Replace("\\", "\\\\"))),
                ("ocr",               ocrResult   != null && !noOcrFlag ? JsonBuilder.Str(ocrResult)  : JsonBuilder.Null()),
                ("color",             colorResult != null                 ? JsonBuilder.Str(colorResult) : JsonBuilder.Null()),
                ("layout",            layoutResult != null                ? JsonBuilder.Str(layoutResult) : JsonBuilder.Null()),
                ("ascii",             JsonBuilder.Str(asciiArt ?? "")),
                ("brightness_matrix",  JsonBuilder.Str(brightnessMatrix ?? ""))
            ));
            return 0;
        }

        // ── gpu ────────────────────────────────────────────────────
        static int CmdGpu(string[] args)
        {
            string imgPath = null;

            // Check if we got a file path directly, or we need to capture a window
            string directPath = ArgVal(args, 1) ?? ArgVal(args, "--image", "-i");
            if (!string.IsNullOrEmpty(directPath) && File.Exists(directPath))
            {
                imgPath = directPath;
            }
            else
            {
                // Try to capture from window
                var win = ResolveWindow(args);
                if (win == null) return 1;

                string tempDir = System.IO.Path.Combine(System.IO.Path.GetTempPath(), "oracle_vision");
                System.IO.Directory.CreateDirectory(tempDir);
                imgPath = System.IO.Path.Combine(tempDir, "oracle_gpu_capture.png");

                using (var bmp = VisualEngine.CaptureToBitmap(win.Handle))
                {
                    bmp.Save(imgPath, System.Drawing.Imaging.ImageFormat.Png);
                }
            }

            // Find Python venv
            string oracleDir = AppDomain.CurrentDomain.BaseDirectory;
            string pythonExe = Path.Combine(oracleDir, "vision_env", "Scripts", "python.exe");
            if (!File.Exists(pythonExe))
            {
                Console.Error.WriteLine(JsonBuilder.Error($"Vision venv not found at {pythonExe}"));
                return 1;
            }

            string scriptPath = Path.Combine(oracleDir, "image_understand.py");
            if (!File.Exists(scriptPath))
            {
                Console.Error.WriteLine(JsonBuilder.Error($"GPU script not found at {scriptPath}"));
                return 1;
            }

            try
            {
                var psi = new ProcessStartInfo
                {
                    FileName = pythonExe,
                    Arguments = $"\"{scriptPath}\" \"{imgPath}\"",
                    UseShellExecute = false,
                    CreateNoWindow = true,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    StandardOutputEncoding = Encoding.UTF8,
                    StandardErrorEncoding = Encoding.UTF8,
                };

                // Set env vars for HuggingFace cache
                string hfCache = Path.Combine(Path.GetTempPath(), "hf-cache");
                psi.EnvironmentVariables["TRANSFORMERS_CACHE"] = hfCache;
                psi.EnvironmentVariables["HF_HOME"] = hfCache;

                using (var proc = Process.Start(psi))
                {
                    if (proc == null)
                    {
                        Console.Error.WriteLine(JsonBuilder.Error("Failed to start Python process"));
                        return 1;
                    }

                    string stdout = proc.StandardOutput.ReadToEnd();
                    string stderr = proc.StandardError.ReadToEnd();

                    if (!proc.WaitForExit(120000))
                    {
                        proc.Kill();
                        Console.Error.WriteLine(JsonBuilder.Error("GPU captioning timed out after 120s"));
                        return 1;
                    }

                    if (proc.ExitCode != 0)
                    {
                        string errMsg = string.IsNullOrEmpty(stderr) ? "Unknown error" : stderr.Trim();
                        Console.Error.WriteLine(JsonBuilder.Error($"GPU captioning failed: {errMsg}"));
                        return 1;
                    }

                    string jsonResult = stdout.Trim();
                    // Parse to extract caption for summary
                    var parsed = DescribeEngine.ParseSimpleJson(jsonResult);
                    parsed.TryGetValue("caption", out string caption);
                    parsed.TryGetValue("device", out string device);
                    parsed.TryGetValue("time_seconds", out string timeSec);

                    Console.Out.WriteLine(JsonBuilder.Obj(
                        ("gpu_result",     JsonBuilder.Str(jsonResult)),
                        ("caption",        JsonBuilder.Str(caption ?? "?")),
                        ("device",         JsonBuilder.Str(device ?? "?")),
                        ("time_seconds",   JsonBuilder.Str(timeSec ?? "?"))
                    ));
                    return 0;
                }
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine(JsonBuilder.Error($"GPU captioning exception: {ex.Message}"));
                return 1;
            }
        }

        // ── helpers ──────────────────────────────────────────────────

        static void PrintUsage()
        {
            Console.Error.WriteLine(@"
╔══════════════════════════════════════════════════════════╗
║                    THE ORACLE v1.0.0                      ║
║         Gaslight-proof Windows UI Validator               ║
╚══════════════════════════════════════════════════════════╝

COMMANDS:
  oracle find    <keyword>   [--timeout 10000] [--poll 500] [--pid N]
  oracle info    --handle 0xN | --keyword str | --pid N
  oracle capture --handle 0xN [--client] [--output dir]
  oracle matrix  --handle 0xN [--rows 15] [--cols 30] [--format brightness|color] [--text]
  oracle click   --handle 0xN --x N --y N
  oracle type    --handle 0xN --text ""hello""
  oracle delta   --handle 0xN [--interval 200]
  oracle verify  --handle 0xN --do ""click:100,200"" [--expect changed] [--wait 500]
  oracle launch  <exe>       [--args ""...""] [--wait 3000] [--cwd dir] [--find]
  oracle scan    [--dir .]   [--pattern *.exe] [--limit 10] [--no-recurse]
  oracle kill    --pid N | --keyword str
  oracle debug   --pid N
  oracle list
  oracle version
  oracle help

  -- VISION COMMANDS (image understanding for text-only LLMs) --
  oracle ocr        --handle 0xN [--lang en]         # extract text with bounding boxes
  oracle uia        --handle 0xN                     # extract UI Automation element tree
  oracle analyze    --handle 0xN [--engines all]     # run all engines, return structured JSON
  oracle describe   --handle 0xN [--no-ocr] [--no-uia]  # LLM-ready rich markdown
  oracle clipboard  [--no-ocr]                       # grab clipboard image + analyze
  oracle pickfile   [--no-ocr]                       # open file picker dialog + analyze
  oracle gpu        --handle 0xN | --image path       # BLIP image captioning on GPU (CUDA)

EXAMPLES:
  oracle scan --dir X:\\blades\\apps           # find freshest exe
  oracle launch X:\\app.exe --wait 3000 --find  # launch + auto-locate window
  oracle debug --pid 23120                      # why didn't this PID create a window?
  oracle find ""MyKainApp"" --timeout 15000
  oracle capture --handle 0x412A
  oracle matrix --handle 0x412A --format color
  oracle verify --handle 0x412A --do ""click:400,300"" --expect ""pixels>100""
  oracle delta --handle 0x412A --interval 300
  oracle describe --handle 0x412A                # full image understanding pipeline
  oracle ocr --handle 0x412A                     # just text extraction
  oracle clipboard                               # analyze clipboard image

TYPICAL KAIN WORKFLOW:
  oracle scan --dir X:\\blades\\apps           # 1. find what you just built
  oracle launch <freshest.exe> --wait 3000      # 2. start it, wait for init
  oracle find --pid <pid> --timeout 10000       # 3. locate the window
  oracle matrix --handle 0xN                    # 4. prove it's rendering (not black)
  oracle verify --handle 0xN --do ""click:100,200"" --expect ""pixels>100""  # 5. prove UI works
");
        }

        /// <summary>
        /// Resolve a window from common args: --handle, --keyword, --pid.
        /// Prints error and returns null on failure.
        /// </summary>
        static WindowInfo ResolveWindow(string[] args)
        {
            string hexHandle = ArgVal(args, "--handle", "-h");
            if (!string.IsNullOrEmpty(hexHandle))
            {
                if (hexHandle.StartsWith("0x", StringComparison.OrdinalIgnoreCase))
                    hexHandle = hexHandle.Substring(2);

                if (IntPtr.Size == 8)
                {
                    long l = Convert.ToInt64(hexHandle, 16);
                    var h = new IntPtr(l);
                    if (ProcessClassFilter.IsValidAppWindow(h))
                        return WindowInfo.FromHandle(h);
                    Console.Error.WriteLine(JsonBuilder.Error($"Handle 0x{hexHandle} is not a valid app window."));
                    return null;
                }
                else
                {
                    int i = Convert.ToInt32(hexHandle, 16);
                    var h = new IntPtr(i);
                    if (ProcessClassFilter.IsValidAppWindow(h))
                        return WindowInfo.FromHandle(h);
                    Console.Error.WriteLine(JsonBuilder.Error($"Handle 0x{hexHandle} is not a valid app window."));
                    return null;
                }
            }

            string keyword = ArgVal(args, "--keyword", "-k");
            if (!string.IsNullOrEmpty(keyword))
            {
                var win = WindowScanner.FindAny(keyword);
                if (win != null) return win;
                Console.Error.WriteLine(JsonBuilder.Error($"No valid window found for '{keyword}'."));
                return null;
            }

            int? pid = ArgInt(args, "--pid", "-p");
            if (pid.HasValue)
            {
                var win = WindowScanner.FindByPid(pid.Value);
                if (win != null) return win;
                Console.Error.WriteLine(JsonBuilder.Error($"No valid window found for PID {pid.Value}."));
                return null;
            }

            Console.Error.WriteLine(JsonBuilder.Error("Specify --handle, --keyword, or --pid."));
            return null;
        }

        /// <summary>
        /// Get a positional argument by index (0-based after command name).
        /// Returns null if index is out of range or the value starts with '-/'.
        /// </summary>
        static string ArgVal(string[] args, int position)
        {
            if (position < args.Length)
            {
                string val = args[position];
                // Don't return flag names as positional values
                if (!val.StartsWith("-") && !val.StartsWith("/"))
                    return val;
            }
            return null;
        }

        static string ArgVal(string[] args, string longName, string shortName = null)
        {
            for (int i = 0; i < args.Length - 1; i++)
            {
                if (args[i].Equals(longName, StringComparison.OrdinalIgnoreCase) ||
                    (shortName != null && args[i].Equals(shortName, StringComparison.OrdinalIgnoreCase)))
                {
                    return args[i + 1];
                }
            }
            // Also check for --key=value format
            string prefix = longName + "=";
            for (int i = 0; i < args.Length; i++)
            {
                if (args[i].StartsWith(prefix, StringComparison.OrdinalIgnoreCase))
                    return args[i].Substring(prefix.Length);
            }
            return null;
        }

        static int? ArgInt(string[] args, string longName, string shortName = null)
        {
            string val = ArgVal(args, longName, shortName);
            if (val != null && int.TryParse(val, out int result))
                return result;
            return null;
        }

        static bool HasFlag(string[] args, string longName, string shortName = null)
        {
            for (int i = 0; i < args.Length; i++)
            {
                if (args[i].Equals(longName, StringComparison.OrdinalIgnoreCase) ||
                    (shortName != null && args[i].Equals(shortName, StringComparison.OrdinalIgnoreCase)))
                    return true;
            }
            return false;
        }
    }
}
