using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Text;

namespace UIValidator.Engines
{
    /// <summary>
    /// OCR engine — uses Tesseract OCR via process call.
    /// Tesseract is installed via scoop: C:\Users\zenta\scoop\apps\tesseract\current
    ///
    /// Output includes word-level bounding boxes via Tesseract TSV output mode.
    /// Accuracy: ~90-95% on clean UI text, ~80% on challenging content.
    /// For 100% accuracy on native app text, use UiaEngine instead.
    /// </summary>
    public static class OcrEngine
    {
        // ── cached paths (resolve once, avoid scanning every call) ──
        private static string _tesseractPath;
        private static string _tessdataDir;
        private static readonly object _initLock = new object();

        /// <summary>
        /// Path to tesseract.exe (via scoop install).
        /// Resolved once and cached.
        /// </summary>
        private static string TesseractPath
        {
            get
            {
                if (_tesseractPath != null) return _tesseractPath;
                lock (_initLock)
                {
                    if (_tesseractPath != null) return _tesseractPath;

                    // Check PATH first
                    string fromPath = FindExePath("tesseract.exe");
                    if (fromPath != null)
                    {
                        _tesseractPath = fromPath;
                        return _tesseractPath;
                    }

                    // Scoop fallback (check common scoop root locations)
                    string[] scoopRoots = ScoopRoots();
                    foreach (string root in scoopRoots)
                    {
                        string scoopPath = Path.Combine(root, "apps", "tesseract", "current", "tesseract.exe");
                        if (File.Exists(scoopPath))
                        {
                            _tesseractPath = scoopPath;
                            return _tesseractPath;
                        }
                    }

                    _tesseractPath = "tesseract.exe"; // hope it's on PATH
                    return _tesseractPath;
                }
            }
        }

        /// <summary>
        /// Known scoop installation roots.
        /// </summary>
        private static string[] ScoopRoots()
        {
            return new[]
            {
                Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), "scoop"),
                "F:\\Scoop",
                "C:\\Scoop",
            };
        }

        /// <summary>
        /// Find the tessdata directory.  Resolved once and cached.
        /// Tesseract needs TESSDATA_PREFIX set to a dir with:
        ///   - eng.traineddata (language data)
        ///   - configs/tsv (TSV output config)
        /// </summary>
        private static string TessdataDir
        {
            get
            {
                if (_tessdataDir != null) return _tessdataDir;
                lock (_initLock)
                {
                    if (_tessdataDir != null) return _tessdataDir;

                    // 1. Prefer explicit env var
                    string envDir = Environment.GetEnvironmentVariable("TESSDATA_PREFIX");
                    if (!string.IsNullOrEmpty(envDir) && Directory.Exists(envDir))
                    {
                        _tessdataDir = envDir;
                        return _tessdataDir;
                    }

                    // 2. Search scoop roots for a dir that has eng.traineddata
                    string[] scoopRoots = ScoopRoots();
                    foreach (string root in scoopRoots)
                    {
                        // tesseract-languages package has the actual language data
                        string langDir = Path.Combine(root, "apps", "tesseract-languages", "current");
                        if (Directory.Exists(langDir) && File.Exists(Path.Combine(langDir, "eng.traineddata")))
                        {
                            _tessdataDir = langDir;
                            return _tessdataDir;
                        }

                        // tesseract package's tessdata dir (traineddata may or may not be here)
                        string tessDir = Path.Combine(root, "apps", "tesseract", "current", "tessdata");
                        if (Directory.Exists(tessDir) && File.Exists(Path.Combine(tessDir, "eng.traineddata")))
                        {
                            _tessdataDir = tessDir;
                            return _tessdataDir;
                        }
                    }

                    return null;
                }
            }
        }

        /// <summary>
        /// Run Tesseract OCR on an image file. Returns JSON with lines, words, positions.
        /// </summary>
        public static string OcrImageFile(string imagePath, string language = null)
        {
            if (!File.Exists(imagePath))
                return JsonError($"Image not found: {imagePath}");

            try
            {
                // ── Step 1: Get word-level data via TSV output ──
                string tsvContent = RunTesseractTsv(imagePath, language ?? "eng", out string errorDetail);

                if (tsvContent == null)
                    return JsonError($"Tesseract returned no output. {errorDetail ?? ""}");

                // ── Step 2: Parse TSV into structured data ──
                var (lines, words) = ParseTesseractTsv(tsvContent);

                // ── Step 3: Build JSON ──
                var sb = new StringBuilder();
                sb.Append("{");
                sb.Append("\"success\":true,");
                sb.Append("\"error\":null,");
                sb.Append("\"line_count\":").Append(lines.Count).Append(",");
                sb.Append("\"word_count\":").Append(words.Count).Append(",");
                sb.Append("\"language\":\"tesseract\",");

                sb.Append("\"lines\":[");
                for (int i = 0; i < lines.Count; i++)
                {
                    if (i > 0) sb.Append(",");
                    var l = lines[i];
                    sb.Append("{");
                    sb.Append("\"text\":").Append(JsonEscape(l.Text)).Append(",");
                    sb.Append("\"x\":").Append(l.X).Append(",");
                    sb.Append("\"y\":").Append(l.Y).Append(",");
                    sb.Append("\"w\":").Append(l.W).Append(",");
                    sb.Append("\"h\":").Append(l.H);
                    sb.Append("}");
                }
                sb.Append("],");

                sb.Append("\"words\":[");
                for (int i = 0; i < words.Count; i++)
                {
                    if (i > 0) sb.Append(",");
                    var w = words[i];
                    sb.Append("{");
                    sb.Append("\"text\":").Append(JsonEscape(w.Text)).Append(",");
                    sb.Append("\"x\":").Append(w.X).Append(",");
                    sb.Append("\"y\":").Append(w.Y).Append(",");
                    sb.Append("\"w\":").Append(w.W).Append(",");
                    sb.Append("\"h\":").Append(w.H).Append(",");
                    sb.Append("\"confidence\":").Append(w.Confidence.ToString("F2"));
                    sb.Append("}");
                }
                sb.Append("]");

                sb.Append("}");
                return sb.ToString();
            }
            catch (Exception ex)
            {
                return JsonError($"OCR exception: {ex.Message}");
            }
        }

        /// <summary>
        /// Run tesseract with TSV output for word-level bounding boxes.
        /// Returns TSV content on success (null on failure).
        /// On failure, errorOut contains diagnostic detail (stderr, exit code, etc.)
        /// </summary>
        private static string RunTesseractTsv(string imagePath, string language, out string errorOut)
        {
            errorOut = null;

            // Create temp output file (tesseract adds .tsv extension)
            string tempDir = Path.Combine(Path.GetTempPath(), "oracle_ocr");
            Directory.CreateDirectory(tempDir);

            // Build a safe temp base filename — strip chars that confuse tesseract or
            // blow past MAX_PATH on Windows (260 chars).
            string safeBase = SanitizeForFilename(Path.GetFileNameWithoutExtension(imagePath));
            // Keep the total tempBase path well under MAX_PATH … 200 chars is safe.
            string timestamp = DateTime.Now.ToString("yyyyMMddHHmmss");
            string tempBase = Path.Combine(tempDir, $"ocr_{safeBase}_{timestamp}");
            if (tempBase.Length > 200)
            {
                // Truncate the safeBase portion if the full path is too long
                int maxBaseLen = 200 - (tempDir.Length + 20 + timestamp.Length); // 20 for "\ocr_" + "_" overhead
                if (maxBaseLen < 8) maxBaseLen = 8; // absolute minimum
                safeBase = safeBase.Substring(0, Math.Min(safeBase.Length, maxBaseLen));
                tempBase = Path.Combine(tempDir, $"ocr_{safeBase}_{timestamp}");
            }

            string tessdata = TessdataDir;

            var psi = new ProcessStartInfo
            {
                FileName = TesseractPath,
                Arguments = $"\"{imagePath}\" \"{tempBase}\" -l {language} --psm 11 tsv",
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                StandardOutputEncoding = Encoding.UTF8,
                StandardErrorEncoding = Encoding.UTF8,
            };

            // Set TESSDATA_PREFIX in the child process environment
            if (tessdata != null)
                psi.EnvironmentVariables["TESSDATA_PREFIX"] = tessdata;

            using (var proc = Process.Start(psi))
            {
                if (proc == null)
                {
                    errorOut = "Process.Start returned null — could not launch tesseract.";
                    return null;
                }

                string stderr = proc.StandardError.ReadToEnd();
                if (!proc.WaitForExit(30000))
                {
                    proc.Kill();
                    errorOut = $"Tesseract timed out after 30s.\nStderr: {stderr}";
                    return null;
                }

                if (proc.ExitCode != 0)
                {
                    string tessHint = tessdata != null
                        ? $"TESSDATA_PREFIX={tessdata}"
                        : "TESSDATA_PREFIX is NOT set";
                    errorOut = $"Tesseract exited with code {proc.ExitCode}. {tessHint}\nStderr: {stderr}";
                    return null;
                }

                // Read the TSV output file
                string tsvPath = tempBase + ".tsv";
                if (!File.Exists(tsvPath))
                {
                    errorOut = $"Tesseract exited 0 but no TSV file was created at: {tsvPath}\nStdout: {proc.StandardOutput.ReadToEnd()}\nStderr: {stderr}";
                    return null;
                }

                string tsv = File.ReadAllText(tsvPath, Encoding.UTF8);

                // Sanity check: TSV should have a header row
                if (string.IsNullOrWhiteSpace(tsv) || !tsv.StartsWith("level"))
                {
                    try { File.Delete(tsvPath); } catch { }
                    errorOut = $"TSV file is empty or has no header row. Content: {tsv?.Substring(0, Math.Min(tsv.Length, 200))}";
                    return null;
                }

                // Clean up temp file
                try { File.Delete(tsvPath); }
                catch { }

                return tsv;
            }
        }

        /// <summary>
        /// Remove characters from a string that could cause filesystem or shell issues.
        /// </summary>
        private static string SanitizeForFilename(string input)
        {
            if (string.IsNullOrEmpty(input)) return "img";
            // Replace common problematic characters with underscores
            var sb = new StringBuilder(input.Length);
            foreach (char c in input)
            {
                if (char.IsLetterOrDigit(c) || c == '_' || c == '-')
                    sb.Append(c);
                else if (c == ' ' || c == '.' || c == ',' || c == '(' || c == ')' || c == '&' || c == '#' || c == '%')
                    sb.Append('_');
                // else skip the character entirely
            }
            string result = sb.ToString().Trim('_', '-', ' ');
            return string.IsNullOrEmpty(result) ? "img" : result;
        }

        /// <summary>
        /// Parse Tesseract TSV output into line and word structures.
        /// TSV format: level, page_num, block_num, par_num, line_num, word_num,
        ///              left, top, width, height, conf, text
        /// Level 5 = word, Level 4 = line-with-text (for line grouping)
        /// </summary>
        private static (List<OcrLineInfo> lines, List<OcrWordInfo> words) ParseTesseractTsv(string tsv)
        {
            var lines = new List<OcrLineInfo>();
            var words = new List<OcrWordInfo>();

            var lineEntries = tsv.Split(new[] { '\n', '\r' }, StringSplitOptions.RemoveEmptyEntries);

            // Track line-level bounding boxes (aggregated from words)
            var lineBounds = new Dictionary<int, (int x1, int y1, int x2, int y2, List<string> texts)>();

            foreach (string entry in lineEntries)
            {
                // Skip header
                if (entry.StartsWith("level")) continue;

                var parts = entry.Split('\t');
                if (parts.Length < 12) continue;

                if (!int.TryParse(parts[0], out int level)) continue;
                // level 5 = word
                if (level != 5) continue;

                int.TryParse(parts[4], out int lineNum);   // line_num
                int.TryParse(parts[6], out int left);      // left
                int.TryParse(parts[7], out int top);       // top
                int.TryParse(parts[8], out int width);     // width
                int.TryParse(parts[9], out int height);    // height
                float.TryParse(parts[10], out float conf);  // confidence
                string text = parts[11]?.Trim();

                if (string.IsNullOrEmpty(text)) continue;
                if (conf < 0) conf = 0; // -1 means no confidence

                // Add word
                words.Add(new OcrWordInfo
                {
                    Text = text,
                    X = left,
                    Y = top,
                    W = width,
                    H = height,
                    Confidence = conf
                });

                // Track line bounds
                if (!lineBounds.ContainsKey(lineNum))
                    lineBounds[lineNum] = (left, top, left + width, top + height, new List<string>());

                var lb = lineBounds[lineNum];
                lb.x1 = Math.Min(lb.x1, left);
                lb.y1 = Math.Min(lb.y1, top);
                lb.x2 = Math.Max(lb.x2, left + width);
                lb.y2 = Math.Max(lb.y2, top + height);
                lb.texts.Add(text);
                lineBounds[lineNum] = lb;
            }

            // Build line-level entries
            foreach (var kvp in lineBounds)
            {
                var lb = kvp.Value;
                lines.Add(new OcrLineInfo
                {
                    Text = string.Join(" ", lb.texts),
                    X = lb.x1,
                    Y = lb.y1,
                    W = lb.x2 - lb.x1,
                    H = lb.y2 - lb.y1
                });
            }

            return (lines, words);
        }

        private struct OcrLineInfo
        {
            public string Text;
            public int X, Y, W, H;
        }

        private struct OcrWordInfo
        {
            public string Text;
            public int X, Y, W, H;
            public double Confidence;
        }

        private static string JsonEscape(string s)
        {
            if (s == null) return "\"\"";
            return "\"" + s.Replace("\\", "\\\\").Replace("\"", "\\\"").Replace("\n", "\\n").Replace("\r", "\\r").Replace("\t", "\\t") + "\"";
        }

        private static string JsonError(string message)
        {
            return $"{{\"success\":false,\"error\":\"{EscapeJson(message)}\",\"lines\":[],\"words\":[],\"line_count\":0,\"word_count\":0,\"language\":null}}";
        }

        private static string EscapeJson(string s)
        {
            return s.Replace("\\", "\\\\").Replace("\"", "\\\"").Replace("\n", "\\n").Replace("\r", "\\r");
        }

        private static string FindExePath(string exeName)
        {
            try
            {
                string paths = Environment.GetEnvironmentVariable("PATH");
                foreach (string path in paths.Split(Path.PathSeparator))
                {
                    string full = Path.Combine(path.Trim(), exeName);
                    if (File.Exists(full))
                        return full;
                }
            }
            catch { }
            return null;
        }

        /// <summary>
        /// Quick check: is Tesseract available?
        /// </summary>
        public static bool IsOcrAvailable()
        {
            try
            {
                string exe = FindExePath("tesseract.exe");
                if (exe == null)
                {
                    string[] scoopRoots = {
                        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), "scoop"),
                        "F:\\Scoop"
                    };
                    bool found = false;
                    foreach (string root in scoopRoots)
                    {
                        string scoopPath = Path.Combine(root, "apps", "tesseract", "current", "tesseract.exe");
                        if (File.Exists(scoopPath)) { found = true; break; }
                    }
                    if (!found) return false;
                }

                // Quick version check
                var psi = new ProcessStartInfo
                {
                    FileName = TesseractPath,
                    Arguments = "--version",
                    UseShellExecute = false,
                    CreateNoWindow = true,
                    RedirectStandardOutput = true,
                };

                string tessdata = TessdataDir;
                if (tessdata != null)
                    psi.EnvironmentVariables["TESSDATA_PREFIX"] = tessdata;

                using (var proc = Process.Start(psi))
                {
                    if (proc == null) return false;
                    proc.WaitForExit(5000);
                    return proc.ExitCode == 0;
                }
            }
            catch
            {
                return false;
            }
        }
    }
}
